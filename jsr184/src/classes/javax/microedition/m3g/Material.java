/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Material extends Object3D {

    public static final int AMBIENT  = 1024;
    public static final int DIFFUSE  = 2048;
    public static final int EMISSIVE = 4096;
    public static final int SPECULAR = 8192;

    private int ambientColor  = 0x00333333;
    private int diffuseColor  = 0xFFCCCCCC;
    private int emissiveColor = 0;
    private int specularColor = 0;
    private float shininess;
    private boolean vertexColorTrackingEnabled;

    public Material() {
        handle = nCreate();
        register();
    }

    public void setColor(int target, int ARGB) {
        if ((target & AMBIENT)  != 0) { ambientColor  = ARGB & 0x00FFFFFF; }
        if ((target & DIFFUSE)  != 0) { diffuseColor  = ARGB; }
        if ((target & EMISSIVE) != 0) { emissiveColor = ARGB & 0x00FFFFFF; }
        if ((target & SPECULAR) != 0) { specularColor = ARGB & 0x00FFFFFF; }
        if (handle != 0) {
            nSetColor(handle, target, ARGB);
        }
    }

    public int getColor(int target) {
        if (handle != 0) {
            return nGetColor(handle, target);
        }
        switch (target) {
            case AMBIENT:  return ambientColor;
            case DIFFUSE:  return diffuseColor;
            case EMISSIVE: return emissiveColor;
            case SPECULAR: return specularColor;
            default:
                throw new IllegalArgumentException("invalid color target");
        }
    }

    public void setShininess(float shininess) {
        if (shininess < 0.0f || shininess > 128.0f) {
            throw new IllegalArgumentException("shininess must be in [0,128]");
        }
        this.shininess = shininess;
        if (handle != 0) {
            nSetShininess(handle, shininess);
        }
    }

    public float getShininess() {
        return shininess;
    }

    public void setVertexColorTrackingEnable(boolean enable) {
        vertexColorTrackingEnabled = enable;
        if (handle != 0) {
            nSetVertexColorTracking(handle, enable ? 1 : 0);
        }
    }

    public boolean isVertexColorTrackingEnabled() {
        return vertexColorTrackingEnabled;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetColor(int handle, int target, int ARGB);
    private static native void nSetShininess(int handle, float shininess);
    private static native void nSetVertexColorTracking(int handle, int enable);
    private static native int nGetColor(int handle, int target);
}
