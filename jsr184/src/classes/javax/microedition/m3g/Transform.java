/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


/** A 4x4 row-major transformation matrix. */
public class Transform {

    private float[] m = new float[16];

    public Transform() {
        setIdentity();
    }

    public Transform(Transform transform) {
        set(transform);
    }

    public void setIdentity() {
        for (int i = 0; i < 16; i++) {
            m[i] = 0.0f;
        }
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    public void set(float[] matrix) {
        if (matrix == null) {
            throw new NullPointerException();
        }
        if (matrix.length < 16) {
            throw new IllegalArgumentException("matrix must hold 16 elements");
        }
        System.arraycopy(matrix, 0, m, 0, 16);
    }

    public void set(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        System.arraycopy(transform.m, 0, m, 0, 16);
    }

    /**
     * The backing array, row-major, without a copy.
     *
     * Package private and read-only by convention: it exists so that
     * {@link Graphics3D} can hand a matrix to the native side without
     * allocating a temporary on every frame. The KNI layer copies out of it
     * immediately (jsr184/src/native/m3g_graphics3d_kni.c).
     */
    float[] rows() {
        return m;
    }

    public void get(float[] matrix) {
        if (matrix == null) {
            throw new NullPointerException();
        }
        if (matrix.length < 16) {
            throw new IllegalArgumentException("matrix must hold 16 elements");
        }
        System.arraycopy(m, 0, matrix, 0, 16);
    }

    public void transpose() {
        for (int r = 0; r < 4; r++) {
            for (int c = r + 1; c < 4; c++) {
                float t = m[r * 4 + c];
                m[r * 4 + c] = m[c * 4 + r];
                m[c * 4 + r] = t;
            }
        }
    }

    public void invert() {
        // Phase 1 keeps the matrix well-formed; a full inverse arrives with
        // the math backend.
        transpose();
    }

    public void postMultiply(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        float[] r = new float[16];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float s = 0.0f;
                for (int k = 0; k < 4; k++) {
                    s += m[i * 4 + k] * transform.m[k * 4 + j];
                }
                r[i * 4 + j] = s;
            }
        }
        m = r;
    }

    public void postScale(float sx, float sy, float sz) {
        Transform t = new Transform();
        t.m[0] = sx; t.m[5] = sy; t.m[10] = sz;
        postMultiply(t);
    }

    public void postTranslate(float tx, float ty, float tz) {
        Transform t = new Transform();
        t.m[3] = tx; t.m[7] = ty; t.m[11] = tz;
        postMultiply(t);
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        postMultiply(new Transform());
    }

    public void postRotateQuat(float qx, float qy, float qz, float qw) {
        postMultiply(new Transform());
    }

    public void transform(float[] vectors) {
        if (vectors == null) {
            throw new NullPointerException();
        }
        if (vectors.length % 4 != 0) {
            throw new IllegalArgumentException("length must be a multiple of 4");
        }
        for (int base = 0; base < vectors.length; base += 4) {
            float x = vectors[base], y = vectors[base + 1];
            float z = vectors[base + 2], w = vectors[base + 3];
            for (int i = 0; i < 4; i++) {
                vectors[base + i] = m[i * 4] * x + m[i * 4 + 1] * y
                                  + m[i * 4 + 2] * z + m[i * 4 + 3] * w;
            }
        }
    }

    public void transform(VertexArray in, float[] out, boolean W) {
        if (in == null || out == null) {
            throw new NullPointerException();
        }
    }
}
