/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class MorphingMesh extends Mesh {

    private VertexBuffer[] targets;
    private float[] weights;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    MorphingMesh() {
    }

    public MorphingMesh(VertexBuffer base, VertexBuffer[] targets,
                        IndexBuffer submesh, Appearance appearance) {
        this(base, targets,
             new IndexBuffer[] { submesh },
             new Appearance[] { appearance });
    }

    public MorphingMesh(VertexBuffer base, VertexBuffer[] targets,
                        IndexBuffer[] submeshes, Appearance[] appearances) {
        /* false: a MorphingMesh is not an M3GMesh -- see
         * Mesh(VertexBuffer, IndexBuffer[], Appearance[], boolean). */
        super(base, submeshes, appearances, false);
        initTargets(targets);
        handle = nCreate(base.handle,
                         handles(targets),
                         handles(submeshes),
                         handles(getAppearances()));
    }

    private void initTargets(VertexBuffer[] targets) {
        if (targets == null) {
            throw new NullPointerException();
        }
        if (targets.length == 0) {
            throw new IllegalArgumentException("no morph targets");
        }
        this.targets = targets;
        this.weights = new float[targets.length];
    }

    public int getMorphTargetCount() {
        return targets.length;
    }

    public VertexBuffer getMorphTarget(int index) {
        return targets[index];
    }

    public void setWeights(float[] weights) {
        if (weights == null) {
            throw new NullPointerException();
        }
        if (weights.length < targets.length) {
            throw new IllegalArgumentException("too few weights");
        }
        System.arraycopy(weights, 0, this.weights, 0, targets.length);
        if (handle != 0) {
            nSetWeights(handle, this.weights);
        }
    }

    public void getWeights(float[] weights) {
        if (weights == null) {
            throw new NullPointerException();
        }
        System.arraycopy(this.weights, 0, weights, 0, this.weights.length);
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int vertices, int[] targets,
                                      int[] submeshes, int[] appearances);
    private static native void nSetWeights(int handle, float[] weights);
}
