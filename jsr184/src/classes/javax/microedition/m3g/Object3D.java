/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;

/**
 * Base class of every object that can appear in a 3D scene.
 *
 * Every instance stands for an object inside m3gcore, identified by
 * {@link #handle}. Objects that came out of {@link Loader} were built by the
 * engine's own file parser; the rest are built by the public constructors,
 * which call the matching <code>m3gCreate*</code> through the natives in
 * jsr184/src/native/m3g_object_kni.c. The engine is the source of truth for
 * anything rendering depends on; the Java fields that remain exist only so the
 * getters can answer.
 *
 * A zero handle means the engine refused the creation -- almost always the M3G
 * arena running out (m3g/src/m3g_psp_arena.c). The wrappers stay usable in
 * that case and simply do not draw, rather than throwing at a MIDlet that has
 * no way to recover; the native side writes one diagnostic line per failure to
 * ms0:/pspkvm_vm.log.
 */
public abstract class Object3D {

    /*
     * m3gcore class identifiers, i.e. the M3GClass enumeration of
     * m3g/inc/M3G/m3g_core.h:272-297. They arrive from the native side as
     * plain ints and are the only thing that says which Java class a loaded
     * object belongs to.
     */
    static final int CLASS_ANIMATION_CONTROLLER =  1;
    static final int CLASS_ANIMATION_TRACK      =  2;
    static final int CLASS_APPEARANCE           =  3;
    static final int CLASS_BACKGROUND           =  4;
    static final int CLASS_CAMERA               =  5;
    static final int CLASS_COMPOSITING_MODE     =  6;
    static final int CLASS_FOG                  =  7;
    static final int CLASS_GROUP                =  8;
    static final int CLASS_IMAGE                =  9;
    static final int CLASS_INDEX_BUFFER         = 10;
    static final int CLASS_KEYFRAME_SEQUENCE    = 11;
    static final int CLASS_LIGHT                = 12;
    static final int CLASS_LOADER               = 13;
    static final int CLASS_MATERIAL             = 14;
    static final int CLASS_MESH                 = 15;
    static final int CLASS_MORPHING_MESH        = 16;
    static final int CLASS_POLYGON_MODE         = 17;
    static final int CLASS_RENDER_CONTEXT       = 18;
    static final int CLASS_SKINNED_MESH         = 19;
    static final int CLASS_SPRITE               = 20;
    static final int CLASS_TEXTURE              = 21;
    static final int CLASS_VERTEX_ARRAY         = 22;
    static final int CLASS_VERTEX_BUFFER        = 23;
    static final int CLASS_WORLD                = 24;

    /**
     * Handle of the m3gcore object this instance wraps, or 0 if there is
     * none. Stored as an int because m3gcore itself keeps object pointers in
     * M3Guint fields and asserts sizeof(M3Guint) >= sizeof(void*)
     * (m3gcore/inc/m3g_defs.h:609).
     */
    int handle;

    private Object userObject;

    /**
     * Every wrapper that currently stands for an engine object, keyed by its
     * handle.
     *
     * The scene-graph accessors hand back raw handles, and the same handle has
     * to keep producing the same Java object or identity breaks: a MIDlet that
     * writes <code>group.removeChild(group.getChild(0))</code>, or compares a
     * node it kept against one it just read back, depends on it. This table is
     * what makes that hold.
     *
     * Nothing is ever removed. CLDC has no finalization, so there is no moment
     * at which a wrapper is known to be dead, and the engine objects leak
     * already; this adds a reference per object to a leak that is accepted for
     * now.
     */
    private static java.util.Hashtable wrappers = new java.util.Hashtable();

    /**
     * Objects constructed before the renderer existed, in construction order.
     *
     * m3gCreate* needs an M3GInterface, and creating that interface is what
     * starts EGL and pspgl -- which must not happen while PSPKVM is still
     * painting its own 2D (see the note above m3gIface in
     * jsr184/src/native/m3g_object_kni.c). A MIDlet that builds geometry
     * during startup therefore gets no engine object at the time, so the
     * object is queued here and built for real the moment the renderer comes
     * up, which is the first Loader.load or Graphics3D.bindTarget.
     */
    private static java.util.Vector deferred = new java.util.Vector();

    /**
     * Set while createWrapper is building an instance for a handle that
     * already exists.
     *
     * Without it the public constructors would each build an engine object
     * that adopt() then immediately destroys -- and because the arena reuses
     * the freed block, the next unrelated object lands on the same address.
     */
    private static boolean wrapping;

    Object3D() {
    }

    /**
     * The engine half of a public constructor.
     *
     * Every constructor that owns an engine object ends with this rather than
     * calling nCreate directly, so that the two cases a raw nCreate gets wrong
     * are handled in one place: building a wrapper for something that already
     * exists, and building anything at all before the renderer is up.
     */
    void construct() {
        if (wrapping) {
            /* createWrapper is about to install the real handle. */
            return;
        }
        createDeferred();
        if (handle == 0) {
            deferred.addElement(this);
        }
    }

    /**
     * Builds this object's engine object, if the renderer is up.
     *
     * Overridden by every class that has one; the override is the same
     * nCreate call its constructor would have made, so it can be run either at
     * construction time or later, when the renderer appears.
     */
    void createDeferred() {
    }

    /**
     * Pushes the Java-side state of a just-built engine object into it.
     *
     * Runs in a second pass, after every deferred object has a handle, so that
     * links pointing forward -- a Group whose child was constructed after it --
     * resolve.
     */
    void applyDeferred() {
    }

    /**
     * Links recorded while a live engine object was handed a deferred one.
     *
     * A MIDlet may set a not-yet-created object into one that came out of
     * Loader -- appearance.setTexture(0, new Texture2D(...)) during startup
     * is the canonical case.  The receiving object is live, so the setter
     * forwards immediately -- but the value's handle is still 0, and the
     * engine reads a zero handle as "remove": the file's own texture was
     * being stripped off every re-dressed appearance and never replaced.
     * These runnables replay exactly those forwards after the deferred pass
     * has given every object its real handle.
     */
    private static java.util.Vector pendingLinks = new java.util.Vector();

    static void linkLater(Runnable link) {
        pendingLinks.addElement(link);
    }

    /**
     * Builds everything queued while the renderer was down.
     *
     * Called from the only two places that can bring the renderer up:
     * Loader.decode and Graphics3D.bindTarget.
     */
    static void flushDeferred() {
        if (deferred.size() != 0) {
            java.util.Vector pending = deferred;
            deferred = new java.util.Vector();

            int n = pending.size();
            for (int i = 0; i < n; i++) {
                ((Object3D) pending.elementAt(i)).createDeferred();
            }
            for (int i = 0; i < n; i++) {
                Object3D o = (Object3D) pending.elementAt(i);
                if (o.handle != 0) {
                    o.applyDeferred();
                }
            }
        }

        if (pendingLinks.size() != 0) {
            java.util.Vector links = pendingLinks;
            pendingLinks = new java.util.Vector();
            for (int i = 0; i < links.size(); i++) {
                ((Runnable) links.elementAt(i)).run();
            }
        }
    }

    /**
     * The wrapper for an engine object, made if it does not exist yet.
     *
     * @return null for a zero handle, or if the handle names a class that
     *         cannot be wrapped
     */
    static Object3D wrap(int handle) {
        if (handle == 0) {
            return null;
        }
        Integer key = new Integer(handle);
        Object existing = wrappers.get(key);
        if (existing != null) {
            return (Object3D) existing;
        }
        return createWrapper(nClassID(handle), handle);
    }

    /** Records this instance as the wrapper for its handle, and takes a
     *  real engine reference so the engine can never free an object Java
     *  can still name.  See nAddRef in m3g_object_kni.c: without this the
     *  engine's refcounting frees scene nodes out from under wrappers and
     *  every later call through them runs on stale memory. */
    void register() {
        if (handle != 0) {
            wrappers.put(new Integer(handle), this);
            nAddRef(handle);
        }
    }

    /**
     * Wraps a freshly loaded engine object in the Java class that matches its
     * m3gcore class id.
     *
     * The state already lives in the engine, so nothing here builds one. The
     * `wrapping` flag is what enforces that: several of the classes below have
     * a parameterless *public* constructor, and without the flag each of them
     * would create an engine object that adopt() then destroys one line later.
     * That is not merely wasteful -- the arena reuses the freed block, so the
     * next unrelated object is handed the same address, and a log full of
     * distinct objects sharing one handle is the result.
     *
     * @return the wrapper, or null if the id names a class that cannot appear
     *         in a file (Loader, RenderContext) or is not recognised at all
     */
    static Object3D createWrapper(int classID, int handle) {
        Object3D object;

        boolean outer = wrapping;
        wrapping = true;
        try {
            object = instantiate(classID);
        } finally {
            wrapping = outer;
        }
        if (object == null) {
            /*
             * TEMPORARY -- name the class id we cannot wrap.
             *
             * A null from here is how the title gets hurt: it loads a scene,
             * asks the engine for a node by user id, and this hands back null
             * because the id names a class with no case in instantiate(). The
             * title dereferences it, throws a NullPointerException, catches it,
             * and abandons its loading thread without notifying whoever waits --
             * which leaves every Java thread blocked and the VM polling forever.
             * One such exception per loaded scene is exactly what the log shows.
             *
             * Printed with a "javax." prefix deliberately: the diagnostic on the
             * print path only forwards lines that look like Java exception text,
             * and this has to pass that filter to reach the log at all.
             */
            nDiag(classID, handle);
            return null;
        }
        object.adopt(handle);
        return object;
    }

    private static Object3D instantiate(int classID) {
        Object3D object;

        switch (classID) {
        case CLASS_ANIMATION_CONTROLLER: object = new AnimationController(); break;
        case CLASS_ANIMATION_TRACK:      object = new AnimationTrack();      break;
        case CLASS_APPEARANCE:           object = new Appearance();          break;
        case CLASS_BACKGROUND:           object = new Background();          break;
        case CLASS_CAMERA:               object = new Camera();              break;
        case CLASS_COMPOSITING_MODE:     object = new CompositingMode();     break;
        case CLASS_FOG:                  object = new Fog();                 break;
        case CLASS_GROUP:                object = new Group();               break;
        case CLASS_IMAGE:                object = new Image2D();             break;
        /* IndexBuffer is abstract; the file format only ever stores the
         * triangle strip flavour (m3gcore/src/m3g_loader.c, class id 10). */
        case CLASS_INDEX_BUFFER:         object = new TriangleStripArray();  break;
        case CLASS_KEYFRAME_SEQUENCE:    object = new KeyframeSequence();    break;
        case CLASS_LIGHT:                object = new Light();               break;
        case CLASS_MATERIAL:             object = new Material();            break;
        case CLASS_MESH:                 object = new Mesh();                break;
        case CLASS_MORPHING_MESH:        object = new MorphingMesh();        break;
        case CLASS_POLYGON_MODE:         object = new PolygonMode();         break;
        case CLASS_SKINNED_MESH:         object = new SkinnedMesh();         break;
        case CLASS_SPRITE:               object = new Sprite3D();            break;
        case CLASS_TEXTURE:              object = new Texture2D();           break;
        case CLASS_VERTEX_ARRAY:         object = new VertexArray();         break;
        case CLASS_VERTEX_BUFFER:        object = new VertexBuffer();        break;
        case CLASS_WORLD:                object = new World();               break;
        default:
            return null;
        }
        return object;
    }

    /**
     * Installs the handle of an engine object somebody else built.
     *
     * The wrapper constructors above are the public ones for every class whose
     * public constructor is parameterless, so several of them have already
     * created an engine object of their own by the time this runs. That one is
     * released here: it was never referenced by anything else, so dropping the
     * single reference the constructor took destroys it.
     */
    void adopt(int engineHandle) {
        if (handle != 0 && handle != engineHandle) {
            /*
             * Forget the throwaway before destroying it. Handles are engine
             * addresses and the arena reuses them, so an entry left behind for
             * a freed object would eventually be handed back by wrap() for a
             * completely unrelated one.
             */
            wrappers.remove(new Integer(handle));
            nDeleteRef(handle);
        }
        handle = engineHandle;
        register();
    }

    /**
     * The handles of an array of objects, in the layout the engine's
     * <code>M3GObject *</code> parameters expect.
     *
     * A Java <code>int[]</code> and an array of engine handles are the same
     * bytes, so the natives pass one straight through as the other rather than
     * copying (see the note on raw array pointers in
     * jsr184/src/native/m3g_object_kni.c).
     */
    static int[] handles(Object3D[] objects) {
        if (objects == null) {
            return null;
        }
        int[] result = new int[objects.length];
        for (int i = 0; i < objects.length; i++) {
            result[i] = (objects[i] != null) ? objects[i].handle : 0;
        }
        return result;
    }

    /**
     * A deep copy, made by the engine.
     *
     * m3gDuplicate copies the whole subtree the specification asks for, which
     * a Java-side copy could not do -- the children of a loaded Group exist
     * only inside the engine.
     */
    public final Object3D duplicate() {
        if (handle == 0) {
            return this;
        }
        Object3D copy = wrap(nDuplicate(handle));
        return (copy != null) ? copy : this;
    }

    public Object3D find(int userID) {
        if (handle != 0) {
            Object3D found = wrap(nFind(handle, userID));
            if (found == null) {
                nDiag(-1, userID);   /* engine knows no such id, or wrap refused */
            }
            return found;
        }
        nDiag(-3, userID);           /* asked of an object with no engine behind it */
        return null;
    }

    public int getReferences(Object3D[] references) {
        return 0;
    }

    public int getUserID() {
        return (handle != 0) ? nGetUserID(handle) : 0;
    }

    public void setUserID(int userID) {
        if (handle != 0) {
            nSetUserID(handle, userID);
        }
    }

    public Object getUserObject() {
        return userObject;
    }

    public void setUserObject(Object userObject) {
        this.userObject = userObject;
    }

    public void addAnimationTrack(AnimationTrack animationTrack) {
        if (animationTrack == null) {
            throw new NullPointerException();
        }
        if (handle != 0 && animationTrack.handle != 0) {
            nAddAnimationTrack(handle, animationTrack.handle);
        }
        else {
            /* One side (or both) has no engine object yet -- the canonical
             * case is a game building its animation rig during startup,
             * before the renderer is up.  Replay after the deferred pass has
             * created everything; flushDeferred runs these links after both
             * creation passes, so construction order does not matter. */
            final AnimationTrack fTrack = animationTrack;
            linkLater(new Runnable() {
                public void run() {
                    if (handle != 0 && fTrack.handle != 0) {
                        nAddAnimationTrack(handle, fTrack.handle);
                    }
                }
            });
        }
    }

    public AnimationTrack getAnimationTrack(int index) {
        if (handle == 0) {
            throw new IndexOutOfBoundsException();
        }
        return (AnimationTrack) wrap(nGetAnimationTrack(handle, index));
    }

    public int getAnimationTrackCount() {
        return (handle != 0) ? nGetAnimationTrackCount(handle) : 0;
    }

    public void removeAnimationTrack(AnimationTrack animationTrack) {
        if (animationTrack != null && handle != 0 && animationTrack.handle != 0) {
            nRemoveAnimationTrack(handle, animationTrack.handle);
        }
    }

    public int animate(int time) {
        return (handle != 0) ? nAnimate(handle, time) : 0;
    }

    /*
     * Natives. Statically bound by name; see
     * jsr184/src/native/m3g_object_kni.c.
     */

    /** Drops one reference, destroying the object if it was the last. */
    /* TEMPORARY -- logging path that reaches ms0:/pspkvm_vm.log. System.out does
     * not; only javacall_print is mirrored there. See m3g_object_kni.c. */
    static native void nDiag(int a, int b);

    private static native void nDeleteRef(int handle);

    private static native void nSetUserID(int handle, int userID);
    private static native int nGetUserID(int handle);

    /** The m3gcore class id, i.e. which Java class a handle belongs in. */
    private static native int nClassID(int handle);
    private static native void nAddRef(int handle);

    private static native int nAnimate(int handle, int time);
    private static native int nDuplicate(int handle);
    private static native int nFind(int handle, int userID);
    private static native int nGetAnimationTrackCount(int handle);
    private static native int nGetAnimationTrack(int handle, int index);
    private static native void nAddAnimationTrack(int handle, int track);
    private static native void nRemoveAnimationTrack(int handle, int track);
}
