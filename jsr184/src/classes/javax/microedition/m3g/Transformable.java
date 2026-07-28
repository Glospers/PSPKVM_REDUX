/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


/**
 * The node transformation: translation, orientation, scale and a free-form
 * matrix, composed in that order by the engine.
 *
 * This is the class that decides where anything appears, so every mutator here
 * forwards into m3gcore and the engine holds the authoritative value. The
 * getters read it back rather than answering from a Java mirror -- a MIDlet
 * that calls postRotate() twice and then getOrientation() has to see the
 * composed result, and only the engine knows it.
 */
public abstract class Transformable extends Object3D {

    Transformable() {
    }

    public void setTransform(Transform transform) {
        if (handle != 0) {
            nSetTransform(handle, (transform != null) ? transform.rows() : null);
        }
    }

    public void getTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        if (handle != 0) {
            nGetTransform(handle, transform.rows());
        } else {
            transform.setIdentity();
        }
    }

    public void getCompositeTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        if (handle != 0) {
            nGetCompositeTransform(handle, transform.rows());
        } else {
            transform.setIdentity();
        }
    }

    public void setTranslation(float tx, float ty, float tz) {
        if (handle != 0) {
            nSetTranslation(handle, tx, ty, tz);
        }
    }

    public void translate(float tx, float ty, float tz) {
        if (handle != 0) {
            nTranslate(handle, tx, ty, tz);
        }
    }

    public void getTranslation(float[] translation) {
        if (translation == null) {
            throw new NullPointerException();
        }
        if (translation.length < 3) {
            throw new IllegalArgumentException("need 3 elements");
        }
        if (handle != 0) {
            nGetTranslation(handle, translation);
        } else {
            translation[0] = 0.0f; translation[1] = 0.0f; translation[2] = 0.0f;
        }
    }

    public void setScale(float sx, float sy, float sz) {
        if (handle != 0) {
            nSetScale(handle, sx, sy, sz);
        }
    }

    public void scale(float sx, float sy, float sz) {
        if (handle != 0) {
            nScale(handle, sx, sy, sz);
        }
    }

    public void getScale(float[] scale) {
        if (scale == null) {
            throw new NullPointerException();
        }
        if (scale.length < 3) {
            throw new IllegalArgumentException("need 3 elements");
        }
        if (handle != 0) {
            nGetScale(handle, scale);
        } else {
            scale[0] = 1.0f; scale[1] = 1.0f; scale[2] = 1.0f;
        }
    }

    public void setOrientation(float angle, float ax, float ay, float az) {
        if (handle != 0) {
            nSetOrientation(handle, angle, ax, ay, az);
        }
    }

    public void preRotate(float angle, float ax, float ay, float az) {
        if (handle != 0) {
            nPreRotate(handle, angle, ax, ay, az);
        }
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        if (handle != 0) {
            nPostRotate(handle, angle, ax, ay, az);
        }
    }

    public void getOrientation(float[] angleAxis) {
        if (angleAxis == null) {
            throw new NullPointerException();
        }
        if (angleAxis.length < 4) {
            throw new IllegalArgumentException("need 4 elements");
        }
        if (handle != 0) {
            nGetOrientation(handle, angleAxis);
        } else {
            angleAxis[0] = 0.0f; angleAxis[1] = 0.0f;
            angleAxis[2] = 0.0f; angleAxis[3] = 1.0f;
        }
    }

    /*
     * Natives; see jsr184/src/native/m3g_object_kni.c. The matrix arrays are
     * sixteen floats in row-major order, the order Transform uses.
     */

    private static native void nSetTransform(int handle, float[] matrix);
    private static native void nGetTransform(int handle, float[] matrix);
    private static native void nGetCompositeTransform(int handle, float[] matrix);
    private static native void nSetTranslation(int handle, float tx, float ty, float tz);
    private static native void nTranslate(int handle, float tx, float ty, float tz);
    private static native void nGetTranslation(int handle, float[] translation);
    private static native void nSetScale(int handle, float sx, float sy, float sz);
    private static native void nScale(int handle, float sx, float sy, float sz);
    private static native void nGetScale(int handle, float[] scale);
    private static native void nSetOrientation(int handle, float angle,
                                               float ax, float ay, float az);
    private static native void nPreRotate(int handle, float angle,
                                          float ax, float ay, float az);
    private static native void nPostRotate(int handle, float angle,
                                           float ax, float ay, float az);
    private static native void nGetOrientation(int handle, float[] angleAxis);
}
