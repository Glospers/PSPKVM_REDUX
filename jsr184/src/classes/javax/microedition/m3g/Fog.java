/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Fog extends Object3D {

    public static final int EXPONENTIAL = 80;
    public static final int LINEAR      = 81;

    private int mode = LINEAR;
    private int color;
    private float density = 1.0f;
    private float near, far = 1.0f;

    public Fog() {
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
    }

    public void setMode(int mode) {
        if (mode != EXPONENTIAL && mode != LINEAR) {
            throw new IllegalArgumentException("invalid fog mode");
        }
        this.mode = mode;
        if (handle != 0) {
            nSetMode(handle, mode);
        }
    }

    public int getMode() {
        return mode;
    }

    public void setColor(int RGB) {
        this.color = RGB & 0x00FFFFFF;
        if (handle != 0) {
            nSetColor(handle, this.color);
        }
    }

    public int getColor() {
        return color;
    }

    public void setDensity(float density) {
        if (density < 0.0f) {
            throw new IllegalArgumentException("density must be >= 0");
        }
        this.density = density;
        if (handle != 0) {
            nSetDensity(handle, density);
        }
    }

    public float getDensity() {
        return density;
    }

    public void setLinear(float near, float far) {
        this.near = near;
        this.far = far;
        if (handle != 0) {
            nSetLinear(handle, near, far);
        }
    }

    public float getNearDistance() { return near; }
    public float getFarDistance()  { return far; }

    void applyDeferred() {
        nSetMode(handle, mode);
        nSetColor(handle, color);
        nSetDensity(handle, density);
        nSetLinear(handle, near, far);
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetMode(int handle, int mode);
    private static native void nSetColor(int handle, int RGB);
    private static native void nSetDensity(int handle, float density);
    private static native void nSetLinear(int handle, float near, float far);
}
