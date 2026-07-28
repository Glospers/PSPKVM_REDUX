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
        super(base, submesh, appearance);
        initTargets(targets);
    }

    public MorphingMesh(VertexBuffer base, VertexBuffer[] targets,
                        IndexBuffer[] submeshes, Appearance[] appearances) {
        super(base, submeshes, appearances);
        initTargets(targets);
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
    }

    public void getWeights(float[] weights) {
        if (weights == null) {
            throw new NullPointerException();
        }
        System.arraycopy(this.weights, 0, weights, 0, this.weights.length);
    }
}
