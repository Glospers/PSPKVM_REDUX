/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class RayIntersection {

    private Node intersected;
    private float distance;
    private int submeshIndex;
    private float[] ray = new float[6];
    private float normalX, normalY, normalZ = 1.0f;

    public RayIntersection() {
    }

    public Node getIntersected() {
        return intersected;
    }

    public void getRay(float[] ray) {
        if (ray == null) {
            throw new NullPointerException();
        }
        if (ray.length < 6) {
            throw new IllegalArgumentException("need 6 elements");
        }
        System.arraycopy(this.ray, 0, ray, 0, 6);
    }

    public float getDistance() {
        return distance;
    }

    public int getSubmeshIndex() {
        return submeshIndex;
    }

    public float getTextureS(int index) {
        return 0.0f;
    }

    public float getTextureT(int index) {
        return 0.0f;
    }

    public float getNormalX() { return normalX; }
    public float getNormalY() { return normalY; }
    public float getNormalZ() { return normalZ; }
}
