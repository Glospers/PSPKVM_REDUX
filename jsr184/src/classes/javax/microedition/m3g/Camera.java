/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Camera extends Node {

    public static final int GENERIC     = 48;
    public static final int PARALLEL    = 49;
    public static final int PERSPECTIVE = 50;

    private int projectionType = GENERIC;
    private float fovy = 60.0f, aspectRatio = 1.0f;
    private float near = 0.1f, far = 1000.0f;
    private Transform projection = new Transform();

    public Camera() {
        handle = nCreate();
    }

    public void setParallel(float fovy, float aspectRatio,
                            float near, float far) {
        this.projectionType = PARALLEL;
        this.fovy = fovy;
        this.aspectRatio = aspectRatio;
        this.near = near;
        this.far = far;
        if (handle != 0) {
            nSetParallel(handle, fovy, aspectRatio, near, far);
        }
    }

    public void setPerspective(float fovy, float aspectRatio,
                               float near, float far) {
        this.projectionType = PERSPECTIVE;
        this.fovy = fovy;
        this.aspectRatio = aspectRatio;
        this.near = near;
        this.far = far;
        if (handle != 0) {
            nSetPerspective(handle, fovy, aspectRatio, near, far);
        }
    }

    public void setGeneric(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        this.projectionType = GENERIC;
        this.projection = new Transform(transform);
        if (handle != 0) {
            nSetGeneric(handle, this.projection.rows());
        }
    }

    public int getProjection(Transform transform) {
        if (transform != null) {
            transform.set(projection);
        }
        return projectionType;
    }

    public int getProjection(float[] params) {
        if (params != null) {
            if (params.length < 4) {
                throw new IllegalArgumentException("need 4 elements");
            }
            params[0] = fovy;
            params[1] = aspectRatio;
            params[2] = near;
            params[3] = far;
        }
        return projectionType;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetPerspective(int handle, float fovy,
                                               float aspectRatio,
                                               float near, float far);
    private static native void nSetParallel(int handle, float height,
                                            float aspectRatio,
                                            float near, float far);
    private static native void nSetGeneric(int handle, float[] matrix);
}
