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
    }

    public void setBlending(int mode) {
        if (mode < ALPHA || mode > REPLACE) {
            throw new IllegalArgumentException("invalid blending mode");
        }
        this.blending = mode;
    }

    public int getBlending() {
        return blending;
    }

    public void setAlphaThreshold(float threshold) {
        if (threshold < 0.0f || threshold > 1.0f) {
            throw new IllegalArgumentException("threshold must be in [0,1]");
        }
        this.alphaThreshold = threshold;
    }

    public float getAlphaThreshold() {
        return alphaThreshold;
    }

    public void setDepthTestEnable(boolean enable)  { depthTestEnabled = enable; }
    public boolean isDepthTestEnabled()             { return depthTestEnabled; }
    public void setDepthWriteEnable(boolean enable) { depthWriteEnabled = enable; }
    public boolean isDepthWriteEnabled()            { return depthWriteEnabled; }
    public void setColorWriteEnable(boolean enable) { colorWriteEnabled = enable; }
    public boolean isColorWriteEnabled()            { return colorWriteEnabled; }
    public void setAlphaWriteEnable(boolean enable) { alphaWriteEnabled = enable; }
    public boolean isAlphaWriteEnabled()            { return alphaWriteEnabled; }

    public void setDepthOffset(float factor, float units) {
        depthOffsetFactor = factor;
        depthOffsetUnits = units;
    }

    public float getDepthOffsetFactor() { return depthOffsetFactor; }
    public float getDepthOffsetUnits()  { return depthOffsetUnits; }
}
