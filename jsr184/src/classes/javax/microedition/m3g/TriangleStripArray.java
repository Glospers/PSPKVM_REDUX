/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class TriangleStripArray extends IndexBuffer {

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    TriangleStripArray() {
    }

    public TriangleStripArray(int firstIndex, int[] stripLengths) {
        if (stripLengths == null) {
            throw new NullPointerException();
        }
        if (firstIndex < 0) {
            throw new IndexOutOfBoundsException();
        }
        int total = 0;
        for (int i = 0; i < stripLengths.length; i++) {
            if (stripLengths[i] < 3) {
                throw new IllegalArgumentException("strip shorter than 3");
            }
            total += stripLengths[i];
        }
        indices = new int[total];
        for (int i = 0; i < total; i++) {
            indices[i] = firstIndex + i;
        }
        handle = nCreateImplicit(firstIndex, stripLengths);
        register();
    }

    public TriangleStripArray(int[] indices, int[] stripLengths) {
        if (indices == null || stripLengths == null) {
            throw new NullPointerException();
        }
        int total = 0;
        for (int i = 0; i < stripLengths.length; i++) {
            if (stripLengths[i] < 3) {
                throw new IllegalArgumentException("strip shorter than 3");
            }
            total += stripLengths[i];
        }
        if (indices.length < total) {
            throw new IllegalArgumentException("not enough indices");
        }
        this.indices = new int[total];
        System.arraycopy(indices, 0, this.indices, 0, total);
        handle = nCreateExplicit(this.indices, stripLengths);
        register();
    }

    /*
     * Natives; see jsr184/src/native/m3g_object_kni.c. The engine's index
     * arrays are M3G_INT, i.e. exactly a Java int[], so both arrays go through
     * without a copy.
     */

    private static native int nCreateImplicit(int firstIndex,
                                              int[] stripLengths);
    private static native int nCreateExplicit(int[] indices,
                                              int[] stripLengths);
}
