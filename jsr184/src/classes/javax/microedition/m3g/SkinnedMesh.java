/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class SkinnedMesh extends Mesh {

    private Group skeleton;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    SkinnedMesh() {
    }

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer submesh,
                       Appearance appearance, Group skeleton) {
        this(vertices,
             new IndexBuffer[] { submesh },
             new Appearance[] { appearance },
             skeleton);
    }

    public SkinnedMesh(VertexBuffer vertices, IndexBuffer[] submeshes,
                       Appearance[] appearances, Group skeleton) {
        /* false: a SkinnedMesh is not an M3GMesh, so the base class must not
         * create one -- see Mesh(VertexBuffer, IndexBuffer[], Appearance[],
         * boolean). */
        super(vertices, submeshes, appearances, false);
        initSkeleton(skeleton);
        handle = nCreate(vertices.handle,
                         handles(submeshes),
                         handles(getAppearances()),
                         skeleton.handle);
        register();
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
        if (handle != 0 && bone.handle != 0) {
            nAddTransform(handle, bone.handle, weight, firstVertex, numVertices);
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

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int vertices, int[] submeshes,
                                      int[] appearances, int skeleton);
    private static native void nAddTransform(int handle, int bone, int weight,
                                             int firstVertex, int numVertices);
}
