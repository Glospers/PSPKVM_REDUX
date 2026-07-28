/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public abstract class IndexBuffer extends Object3D {

    int[] indices = new int[0];

    IndexBuffer() {
    }

    public int getIndexCount() {
        return indices.length;
    }

    public void getIndices(int[] indices) {
        if (indices == null) {
            throw new NullPointerException();
        }
        if (indices.length < this.indices.length) {
            throw new IllegalArgumentException("target array too small");
        }
        System.arraycopy(this.indices, 0, indices, 0, this.indices.length);
    }
}
