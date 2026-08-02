/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class AnimationTrack extends Object3D {

    public static final int ALPHA            = 256;
    public static final int AMBIENT_COLOR    = 257;
    public static final int COLOR            = 258;
    public static final int CROP             = 259;
    public static final int DENSITY          = 260;
    public static final int DIFFUSE_COLOR    = 261;
    public static final int EMISSIVE_COLOR   = 262;
    public static final int FAR_DISTANCE     = 263;
    public static final int FIELD_OF_VIEW    = 264;
    public static final int INTENSITY        = 265;
    public static final int MORPH_WEIGHTS    = 266;
    public static final int NEAR_DISTANCE    = 267;
    public static final int ORIENTATION      = 268;
    public static final int PICKABILITY      = 269;
    public static final int SCALE            = 270;
    public static final int SHININESS        = 271;
    public static final int SPECULAR_COLOR   = 272;
    public static final int SPOT_ANGLE       = 273;
    public static final int SPOT_EXPONENT    = 274;
    public static final int TRANSLATION      = 275;
    public static final int VISIBILITY       = 276;

    private KeyframeSequence sequence;
    private AnimationController controller;
    private int property;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    AnimationTrack() {
    }

    public AnimationTrack(KeyframeSequence sequence, int property) {
        if (sequence == null) {
            throw new NullPointerException();
        }
        if (property < ALPHA || property > VISIBILITY) {
            throw new IllegalArgumentException("invalid animated property");
        }
        this.sequence = sequence;
        this.property = property;
        construct();
    }

    /*
     * The sequence is constructed before the track by necessity (the
     * constructor requires it), so in the deferred queue it is always
     * created first and its handle is available here.  A zero sequence
     * handle means its creation genuinely failed; the track then stays
     * engine-less, like every other object whose creation failed.
     */
    void createDeferred() {
        if (sequence != null && sequence.handle != 0) {
            handle = nCreate(sequence.handle, property);
            register();
        }
    }

    void applyDeferred() {
        if (controller != null && controller.handle != 0) {
            nSetController(handle, controller.handle);
        }
    }

    public KeyframeSequence getKeyframeSequence() {
        if (handle != 0) {
            return (KeyframeSequence) Object3D.wrap(nGetSequence(handle));
        }
        return sequence;
    }

    public void setController(AnimationController controller) {
        this.controller = controller;
        if (handle != 0) {
            if (controller == null) {
                nSetController(handle, 0);
            }
            else if (controller.handle != 0) {
                nSetController(handle, controller.handle);
            }
            else {
                /* Deferred controller into a live track: forward once the
                 * controller exists.  See Object3D.linkLater. */
                final AnimationController fValue = controller;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (AnimationTrack.this.controller == fValue
                                && fValue.handle != 0) {
                            nSetController(handle, fValue.handle);
                        }
                    }
                });
            }
        }
    }

    public AnimationController getController() {
        return controller;
    }

    public int getTargetProperty() {
        if (handle != 0) {
            return nGetTargetProperty(handle);
        }
        return property;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int sequence, int property);
    private static native void nSetController(int handle, int controller);
    private static native int nGetSequence(int handle);
    private static native int nGetTargetProperty(int handle);
}
