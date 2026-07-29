/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class TriangleStripArray extends IndexBuffer {

    /* The constructor arguments, kept so that a strip built before the
     * renderer existed can be rebuilt once it does. */
    private int[] stripLengths;
    private int firstIndex;
    private boolean implicit;

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
        /* Kept so the buffer can be rebuilt if the renderer was not up yet;
         * the engine wants the strip lengths, which the flattened index array
         * above cannot be taken back apart into. */
        this.stripLengths = stripLengths;
        this.firstIndex = firstIndex;
        this.implicit = true;
        construct();
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
        this.stripLengths = stripLengths;
        construct();
    }

    void createDeferred() {
        handle = implicit ? nCreateImplicit(firstIndex, stripLengths)
                          : nCreateExplicit(indices, stripLengths);
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
