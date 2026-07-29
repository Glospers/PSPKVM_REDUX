/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class CompositingMode extends Object3D {

    public static final int ALPHA       = 64;
    public static final int ALPHA_ADD   = 65;
    public static final int MODULATE    = 66;
    public static final int MODULATE_X2 = 67;
    public static final int REPLACE     = 68;

    private int blending = REPLACE;
    private float alphaThreshold;
    private float depthOffsetFactor, depthOffsetUnits;
    private boolean depthTestEnabled = true;
    private boolean depthWriteEnabled = true;
    private boolean colorWriteEnabled = true;
    private boolean alphaWriteEnabled = true;

    public CompositingMode() {
        handle = nCreate();
        register();
    }

    public void setBlending(int mode) {
        if (mode < ALPHA || mode > REPLACE) {
            throw new IllegalArgumentException("invalid blending mode");
        }
        this.blending = mode;
        if (handle != 0) {
            nSetBlending(handle, mode);
        }
    }

    public int getBlending() {
        return blending;
    }

    public void setAlphaThreshold(float threshold) {
        if (threshold < 0.0f || threshold > 1.0f) {
            throw new IllegalArgumentException("threshold must be in [0,1]");
        }
        this.alphaThreshold = threshold;
        if (handle != 0) {
            nSetAlphaThreshold(handle, threshold);
        }
    }

    public float getAlphaThreshold() {
        return alphaThreshold;
    }

    public void setDepthTestEnable(boolean enable) {
        depthTestEnabled = enable;
        if (handle != 0) {
            nEnableDepthTest(handle, enable ? 1 : 0);
        }
    }

    public boolean isDepthTestEnabled() {
        return depthTestEnabled;
    }

    public void setDepthWriteEnable(boolean enable) {
        depthWriteEnabled = enable;
        if (handle != 0) {
            nEnableDepthWrite(handle, enable ? 1 : 0);
        }
    }

    public boolean isDepthWriteEnabled() {
        return depthWriteEnabled;
    }

    public void setColorWriteEnable(boolean enable) {
        colorWriteEnabled = enable;
        if (handle != 0) {
            nEnableColorWrite(handle, enable ? 1 : 0);
        }
    }

    public boolean isColorWriteEnabled() {
        return colorWriteEnabled;
    }

    public void setAlphaWriteEnable(boolean enable) {
        alphaWriteEnabled = enable;
        if (handle != 0) {
            nEnableAlphaWrite(handle, enable ? 1 : 0);
        }
    }

    public boolean isAlphaWriteEnabled() {
        return alphaWriteEnabled;
    }

    public void setDepthOffset(float factor, float units) {
        depthOffsetFactor = factor;
        depthOffsetUnits = units;
        if (handle != 0) {
            nSetDepthOffset(handle, factor, units);
        }
    }

    public float getDepthOffsetFactor() { return depthOffsetFactor; }
    public float getDepthOffsetUnits()  { return depthOffsetUnits; }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetBlending(int handle, int mode);
    private static native void nSetAlphaThreshold(int handle, float threshold);
    private static native void nSetDepthOffset(int handle, float factor,
                                               float units);
    private static native void nEnableDepthTest(int handle, int enable);
    private static native void nEnableDepthWrite(int handle, int enable);
    private static native void nEnableColorWrite(int handle, int enable);
    private static native void nEnableAlphaWrite(int handle, int enable);
}
