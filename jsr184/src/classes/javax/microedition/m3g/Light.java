/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Light extends Node {

    public static final int AMBIENT     = 128;
    public static final int DIRECTIONAL = 129;
    public static final int OMNI        = 130;
    public static final int SPOT        = 131;

    private int mode = DIRECTIONAL;
    private int color = 0x00FFFFFF;
    private float intensity = 1.0f;
    private float spotAngle = 45.0f;
    private float spotExponent = 0.0f;
    private float constantAttenuation = 1.0f;
    private float linearAttenuation = 0.0f;
    private float quadraticAttenuation = 0.0f;

    public Light() {
        handle = nCreate();
    }

    public void setMode(int mode) {
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

    public void setIntensity(float intensity) {
        this.intensity = intensity;
        if (handle != 0) {
            nSetIntensity(handle, intensity);
        }
    }

    public float getIntensity() {
        return intensity;
    }

    public void setSpotAngle(float angle) {
        if (angle < 0.0f || angle > 90.0f) {
            throw new IllegalArgumentException("spot angle must be in [0,90]");
        }
        this.spotAngle = angle;
        if (handle != 0) {
            nSetSpotAngle(handle, angle);
        }
    }

    public float getSpotAngle() {
        return spotAngle;
    }

    public void setSpotExponent(float exponent) {
        this.spotExponent = exponent;
        if (handle != 0) {
            nSetSpotExponent(handle, exponent);
        }
    }

    public float getSpotExponent() {
        return spotExponent;
    }

    public void setAttenuation(float constant, float linear, float quadratic) {
        this.constantAttenuation = constant;
        this.linearAttenuation = linear;
        this.quadraticAttenuation = quadratic;
        if (handle != 0) {
            nSetAttenuation(handle, constant, linear, quadratic);
        }
    }

    public float getConstantAttenuation()  { return constantAttenuation; }
    public float getLinearAttenuation()    { return linearAttenuation; }
    public float getQuadraticAttenuation() { return quadraticAttenuation; }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetMode(int handle, int mode);
    private static native void nSetColor(int handle, int RGB);
    private static native void nSetIntensity(int handle, float intensity);
    private static native void nSetSpotAngle(int handle, float angle);
    private static native void nSetSpotExponent(int handle, float exponent);
    private static native void nSetAttenuation(int handle, float constant,
                                               float linear, float quadratic);
}
