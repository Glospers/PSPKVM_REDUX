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

    private int userID;
    private Object userObject;
    private java.util.Vector animationTracks = new java.util.Vector();

    Object3D() {
    }

    /**
     * Wraps a freshly loaded engine object in the Java class that matches its
     * m3gcore class id.
     *
     * The instances are deliberately built through the package-private no-arg
     * constructors rather than the public ones: the state already lives in the
     * engine, and the public constructors would demand -- and then duplicate
     * on the Java side -- vertex buffers, images and the like.
     *
     * @return the wrapper, or null if the id names a class that cannot appear
     *         in a file (Loader, RenderContext) or is not recognised at all
     */
    static Object3D createWrapper(int classID, int handle) {
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

        object.adopt(handle);
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
            nDeleteRef(handle);
        }
        handle = engineHandle;
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

    public final Object3D duplicate() {
        return duplicateImpl();
    }

    /** Overridden by subclasses that carry state worth copying. */
    Object3D duplicateImpl() {
        return this;
    }

    public Object3D find(int userID) {
        return (this.userID == userID) ? this : null;
    }

    public int getReferences(Object3D[] references) {
        return 0;
    }

    public int getUserID() {
        return userID;
    }

    public void setUserID(int userID) {
        this.userID = userID;
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
        animationTracks.addElement(animationTrack);
    }

    public AnimationTrack getAnimationTrack(int index) {
        return (AnimationTrack) animationTracks.elementAt(index);
    }

    public int getAnimationTrackCount() {
        return animationTracks.size();
    }

    public void removeAnimationTrack(AnimationTrack animationTrack) {
        animationTracks.removeElement(animationTrack);
    }

    public int animate(int time) {
        return 0;
    }

    /*
     * Natives. Statically bound by name; see
     * jsr184/src/native/m3g_object_kni.c.
     */

    /** Drops one reference, destroying the object if it was the last. */
    private static native void nDeleteRef(int handle);

    private static native void nSetUserID(int handle, int userID);
}
