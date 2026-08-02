/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class KeyframeSequence extends Object3D {

    public static final int CONSTANT = 192;
    public static final int LINEAR   = 176;
    public static final int LOOP     = 193;
    public static final int SLERP    = 177;
    public static final int SPLINE   = 178;
    public static final int SQUAD    = 179;
    public static final int STEP     = 180;

    private int numKeyframes, numComponents, interpolation;
    private int duration, validRangeFirst, validRangeLast;
    private int repeatMode = CONSTANT;
    private int[] keyTimes;
    private float[][] keyValues;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    KeyframeSequence() {
    }

    public KeyframeSequence(int numKeyframes, int numComponents,
                            int interpolation) {
        if (numKeyframes < 1 || numComponents < 1) {
            throw new IllegalArgumentException("sequence must be non-empty");
        }
        this.numKeyframes = numKeyframes;
        this.numComponents = numComponents;
        this.interpolation = interpolation;
        this.keyTimes = new int[numKeyframes];
        this.keyValues = new float[numKeyframes][numComponents];
        this.validRangeLast = numKeyframes - 1;
        construct();
    }

    void createDeferred() {
        handle = nCreate(numKeyframes, numComponents, interpolation);
        register();
    }

    void applyDeferred() {
        for (int i = 0; i < numKeyframes; i++) {
            nSetKeyframe(handle, i, keyTimes[i], numComponents, keyValues[i]);
        }
        nSetValidRange(handle, validRangeFirst, validRangeLast);
        if (duration > 0) {
            nSetDuration(handle, duration);
        }
        nSetRepeatMode(handle, repeatMode);
    }

    public int getComponentCount()   { return numComponents; }
    public int getKeyframeCount() {
        return (handle != 0) ? nGetKeyframeCount(handle) : numKeyframes;
    }
    public int getInterpolationType() { return interpolation; }

    public void setKeyframe(int index, int time, float[] value) {
        if (value == null) {
            throw new NullPointerException();
        }
        if (time < 0) {
            throw new IllegalArgumentException("time must be >= 0");
        }
        if (value.length < numComponents) {
            throw new IllegalArgumentException("too few components");
        }
        /* Loaded wrappers have no Java mirror (see Object3D); the engine is
         * the only store for them. */
        if (keyTimes != null) {
            keyTimes[index] = time;
            System.arraycopy(value, 0, keyValues[index], 0, numComponents);
        }
        if (handle != 0) {
            nSetKeyframe(handle, index, time,
                         (numComponents > 0) ? numComponents : value.length,
                         value);
        }
    }

    public int getKeyframe(int index, float[] value) {
        if (value != null) {
            System.arraycopy(keyValues[index], 0, value, 0, numComponents);
        }
        return keyTimes[index];
    }

    public void setValidRange(int first, int last) {
        validRangeFirst = first;
        validRangeLast = last;
        if (handle != 0) {
            nSetValidRange(handle, first, last);
        }
    }

    public int getValidRangeFirst() { return validRangeFirst; }
    public int getValidRangeLast()  { return validRangeLast; }

    public void setDuration(int duration) {
        if (duration <= 0) {
            throw new IllegalArgumentException("duration must be > 0");
        }
        this.duration = duration;
        if (handle != 0) {
            nSetDuration(handle, duration);
        }
    }

    public int getDuration() {
        return (handle != 0) ? nGetDuration(handle) : duration;
    }

    public void setRepeatMode(int mode) {
        if (mode != CONSTANT && mode != LOOP) {
            throw new IllegalArgumentException("invalid repeat mode");
        }
        repeatMode = mode;
        if (handle != 0) {
            nSetRepeatMode(handle, mode);
        }
    }

    public int getRepeatMode() {
        return repeatMode;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int numKeyframes, int numComponents,
                                      int interpolation);
    private static native void nSetKeyframe(int handle, int index, int time,
                                            int numComponents, float[] value);
    private static native void nSetValidRange(int handle, int first, int last);
    private static native void nSetDuration(int handle, int duration);
    private static native void nSetRepeatMode(int handle, int mode);
    private static native int nGetDuration(int handle);
    private static native int nGetKeyframeCount(int handle);
}
