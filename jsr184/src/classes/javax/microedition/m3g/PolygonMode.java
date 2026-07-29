/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class PolygonMode extends Object3D {

    public static final int CULL_BACK   = 160;
    public static final int CULL_FRONT  = 161;
    public static final int CULL_NONE   = 162;
    public static final int SHADE_FLAT  = 164;
    public static final int SHADE_SMOOTH = 165;
    public static final int WINDING_CCW = 168;
    public static final int WINDING_CW  = 169;

    private int culling = CULL_BACK;
    private int shading = SHADE_SMOOTH;
    private int winding = WINDING_CCW;
    private boolean twoSidedLightingEnabled;
    private boolean localCameraLightingEnabled;
    private boolean perspectiveCorrectionEnabled;

    public PolygonMode() {
        handle = nCreate();
        register();
    }

    public void setCulling(int mode) {
        if (mode < CULL_BACK || mode > CULL_NONE) {
            throw new IllegalArgumentException("invalid culling mode");
        }
        culling = mode;
        if (handle != 0) {
            nSetCulling(handle, mode);
        }
    }

    public int getCulling() {
        return culling;
    }

    public void setShading(int mode) {
        if (mode != SHADE_FLAT && mode != SHADE_SMOOTH) {
            throw new IllegalArgumentException("invalid shading mode");
        }
        shading = mode;
        if (handle != 0) {
            nSetShading(handle, mode);
        }
    }

    public int getShading() {
        return shading;
    }

    public void setWinding(int mode) {
        if (mode != WINDING_CCW && mode != WINDING_CW) {
            throw new IllegalArgumentException("invalid winding mode");
        }
        winding = mode;
        if (handle != 0) {
            nSetWinding(handle, mode);
        }
    }

    public int getWinding() {
        return winding;
    }

    public void setTwoSidedLightingEnable(boolean enable) {
        twoSidedLightingEnabled = enable;
        if (handle != 0) {
            nSetTwoSidedLighting(handle, enable ? 1 : 0);
        }
    }

    public boolean isTwoSidedLightingEnabled() {
        return twoSidedLightingEnabled;
    }

    public void setLocalCameraLightingEnable(boolean enable) {
        localCameraLightingEnabled = enable;
        if (handle != 0) {
            nSetLocalCameraLighting(handle, enable ? 1 : 0);
        }
    }

    public boolean isLocalCameraLightingEnabled() {
        return localCameraLightingEnabled;
    }

    public void setPerspectiveCorrectionEnable(boolean enable) {
        perspectiveCorrectionEnabled = enable;
        if (handle != 0) {
            nSetPerspectiveCorrection(handle, enable ? 1 : 0);
        }
    }

    public boolean isPerspectiveCorrectionEnabled() {
        return perspectiveCorrectionEnabled;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetCulling(int handle, int mode);
    private static native void nSetShading(int handle, int mode);
    private static native void nSetWinding(int handle, int mode);
    private static native void nSetTwoSidedLighting(int handle, int enable);
    private static native void nSetLocalCameraLighting(int handle, int enable);
    private static native void nSetPerspectiveCorrection(int handle, int enable);
}
