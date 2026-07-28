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
    }

    public void setMode(int mode) {
        if (mode != EXPONENTIAL && mode != LINEAR) {
            throw new IllegalArgumentException("invalid fog mode");
        }
        this.mode = mode;
    }

    public int getMode() {
        return mode;
    }

    public void setColor(int RGB) {
        this.color = RGB & 0x00FFFFFF;
    }

    public int getColor() {
        return color;
    }

    public void setDensity(float density) {
        if (density < 0.0f) {
            throw new IllegalArgumentException("density must be >= 0");
        }
        this.density = density;
    }

    public float getDensity() {
        return density;
    }

    public void setLinear(float near, float far) {
        this.near = near;
        this.far = far;
    }

    public float getNearDistance() { return near; }
    public float getFarDistance()  { return far; }
}
