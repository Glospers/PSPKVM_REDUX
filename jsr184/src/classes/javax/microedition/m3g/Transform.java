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

    /**
     * General 4x4 inverse, by cofactor expansion.
     *
     * A transpose is not good enough here even for a rigid-body transform,
     * because JSR-184 matrices routinely carry a translation and a scale, and
     * a MIDlet that inverts its camera matrix and gets the transpose back gets
     * a scene that is silently in the wrong place.
     */
    public void invert() {
        float[] a = m;
        float[] r = new float[16];

        r[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15]
               + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        r[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15]
               - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        r[8]  =  a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15]
               + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        r[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14]
               - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];

        r[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15]
               - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        r[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15]
               + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        r[9]  = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15]
               - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        r[13] =  a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14]
               + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];

        r[2]  =  a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15]
               + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
        r[6]  = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15]
               - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
        r[10] =  a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15]
               + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
        r[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14]
               - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];

        r[3]  = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11]
               - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
        r[7]  =  a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11]
               + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
        r[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11]
               - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
        r[15] =  a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10]
               + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];

        float det = a[0]*r[0] + a[1]*r[4] + a[2]*r[8] + a[3]*r[12];
        if (det == 0.0f) {
            throw new ArithmeticException("matrix is singular");
        }
        float inv = 1.0f / det;
        for (int i = 0; i < 16; i++) {
            r[i] *= inv;
        }
        m = r;
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

    /**
     * Post-multiplies by a rotation of <code>angle</code> degrees about the
     * given axis.
     *
     * This one matters more than it looks: MIDlets build their camera and
     * object transforms out of postTranslate/postRotate pairs, so a
     * postRotate that does nothing puts an entire scene in the wrong
     * orientation without any other symptom.
     */
    public void postRotate(float angle, float ax, float ay, float az) {
        float len = (float) Math.sqrt(ax * ax + ay * ay + az * az);
        if (len == 0.0f) {
            throw new IllegalArgumentException("rotation axis is zero");
        }
        float x = ax / len, y = ay / len, z = az / len;

        double radians = angle * (Math.PI / 180.0);
        float c = (float) Math.cos(radians);
        float s = (float) Math.sin(radians);
        float t = 1.0f - c;

        Transform r = new Transform();
        r.m[0]  = t*x*x + c;    r.m[1]  = t*x*y - s*z; r.m[2]  = t*x*z + s*y;
        r.m[4]  = t*x*y + s*z;  r.m[5]  = t*y*y + c;   r.m[6]  = t*y*z - s*x;
        r.m[8]  = t*x*z - s*y;  r.m[9]  = t*y*z + s*x; r.m[10] = t*z*z + c;
        postMultiply(r);
    }

    public void postRotateQuat(float qx, float qy, float qz, float qw) {
        float len = (float) Math.sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
        if (len == 0.0f) {
            throw new IllegalArgumentException("quaternion is zero");
        }
        float x = qx / len, y = qy / len, z = qz / len, w = qw / len;

        Transform r = new Transform();
        r.m[0]  = 1.0f - 2.0f*(y*y + z*z);
        r.m[1]  =        2.0f*(x*y - z*w);
        r.m[2]  =        2.0f*(x*z + y*w);
        r.m[4]  =        2.0f*(x*y + z*w);
        r.m[5]  = 1.0f - 2.0f*(x*x + z*z);
        r.m[6]  =        2.0f*(y*z - x*w);
        r.m[8]  =        2.0f*(x*z - y*w);
        r.m[9]  =        2.0f*(y*z + x*w);
        r.m[10] = 1.0f - 2.0f*(x*x + y*y);
        postMultiply(r);
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
