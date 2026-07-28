/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class SkinnedMesh extends Mesh {

    private Group skeleton;

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer submesh,
                       Appearance appearance, Group skeleton) {
        super(vertices, submesh, appearance);
        initSkeleton(skeleton);
    }

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer[] submeshes,
                       Appearance[] appearances, Group skeleton) {
        super(vertices, submeshes, appearances);
        initSkeleton(skeleton);
    }

    private void initSkeleton(Group skeleton) {
        if (skeleton == null) {
            throw new NullPointerException();
        }
        if (skeleton.getParent() != null) {
            throw new IllegalArgumentException("skeleton already has a parent");
        }
        this.skeleton = skeleton;
    }

    public Group getSkeleton() {
        return skeleton;
    }

    public void addTransform(Node bone, int weight, int firstVertex,
                             int numVertices) {
        if (bone == null) {
            throw new NullPointerException();
        }
        if (weight <= 0 || numVertices <= 0) {
            throw new IllegalArgumentException("weight and count must be > 0");
        }
    }

    public void getBoneTransform(Node bone, Transform transform) {
        if (bone == null || transform == null) {
            throw new NullPointerException();
        }
        transform.setIdentity();
    }

    public int getBoneVertices(Node bone, int[] indices, float[] weights) {
        return 0;
    }
}
