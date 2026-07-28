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
    }

    public void setMode(int mode) {
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

    public void setIntensity(float intensity) {
        this.intensity = intensity;
    }

    public float getIntensity() {
        return intensity;
    }

    public void setSpotAngle(float angle) {
        if (angle < 0.0f || angle > 90.0f) {
            throw new IllegalArgumentException("spot angle must be in [0,90]");
        }
        this.spotAngle = angle;
    }

    public float getSpotAngle() {
        return spotAngle;
    }

    public void setSpotExponent(float exponent) {
        this.spotExponent = exponent;
    }

    public float getSpotExponent() {
        return spotExponent;
    }

    public void setAttenuation(float constant, float linear, float quadratic) {
        this.constantAttenuation = constant;
        this.linearAttenuation = linear;
        this.quadraticAttenuation = quadratic;
    }

    public float getConstantAttenuation()  { return constantAttenuation; }
    public float getLinearAttenuation()    { return linearAttenuation; }
    public float getQuadraticAttenuation() { return quadraticAttenuation; }
}
