/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public abstract class Transformable extends Object3D {

    private Transform transform = new Transform();
    private float tx, ty, tz;
    private float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    private float angle, ax, ay, az = 1.0f;

    Transformable() {
    }

    public void setTransform(Transform transform) {
        this.transform = (transform != null)
            ? new Transform(transform) : new Transform();
    }

    public void getTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        transform.set(this.transform);
    }

    public void getCompositeTransform(Transform transform) {
        if (transform == null) {
            throw new NullPointerException();
        }
        transform.set(this.transform);
    }

    public void setTranslation(float tx, float ty, float tz) {
        this.tx = tx; this.ty = ty; this.tz = tz;
    }

    public void translate(float tx, float ty, float tz) {
        this.tx += tx; this.ty += ty; this.tz += tz;
    }

    public void getTranslation(float[] translation) {
        if (translation == null) {
            throw new NullPointerException();
        }
        if (translation.length < 3) {
            throw new IllegalArgumentException("need 3 elements");
        }
        translation[0] = tx; translation[1] = ty; translation[2] = tz;
    }

    public void setScale(float sx, float sy, float sz) {
        this.sx = sx; this.sy = sy; this.sz = sz;
    }

    public void scale(float sx, float sy, float sz) {
        this.sx *= sx; this.sy *= sy; this.sz *= sz;
    }

    public void getScale(float[] scale) {
        if (scale == null) {
            throw new NullPointerException();
        }
        if (scale.length < 3) {
            throw new IllegalArgumentException("need 3 elements");
        }
        scale[0] = sx; scale[1] = sy; scale[2] = sz;
    }

    public void setOrientation(float angle, float ax, float ay, float az) {
        this.angle = angle; this.ax = ax; this.ay = ay; this.az = az;
    }

    public void preRotate(float angle, float ax, float ay, float az) {
    }

    public void postRotate(float angle, float ax, float ay, float az) {
    }

    public void getOrientation(float[] angleAxis) {
        if (angleAxis == null) {
            throw new NullPointerException();
        }
        if (angleAxis.length < 4) {
            throw new IllegalArgumentException("need 4 elements");
        }
        angleAxis[0] = angle; angleAxis[1] = ax;
        angleAxis[2] = ay;    angleAxis[3] = az;
    }
}
