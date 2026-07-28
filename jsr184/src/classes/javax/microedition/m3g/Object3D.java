/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;

/**
 * Base class of every object that can appear in a 3D scene.
 *
 * The class hierarchy and public API are in place so that MIDlets link and
 * run. Objects that came out of {@link Loader} additionally carry the handle
 * of the m3gcore object they stand for; the rest are Java-only so far.
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

        object.handle = handle;
        return object;
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
}
