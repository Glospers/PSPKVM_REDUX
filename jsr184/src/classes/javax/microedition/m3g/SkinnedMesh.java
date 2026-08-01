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
        construct();
    }

    void createDeferred() {
        handle = nCreate(rawVertexBuffer().handle,
                         handles(rawSubmeshes()),
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

    /* As in Mesh: a SkinnedMesh from Loader has no Java-side skeleton, so read
     * the one the engine holds. */
    public Group getSkeleton() {
        if (handle != 0) {
            return (Group) Object3D.wrap(nGetSkeleton(handle));
        }
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
        else {
            /* Either side may not exist yet, and there is no Java-side
             * record for applyDeferred to replay -- without this the call
             * vanished entirely.  linkLater runs after every deferred
             * object has been created, so both handles exist by then. */
            final Node fBone = bone;
            final int fWeight = weight;
            final int fFirst = firstVertex;
            final int fCount = numVertices;
            Object3D.linkLater(new Runnable() {
                public void run() {
                    if (handle != 0 && fBone.handle != 0) {
                        nAddTransform(handle, fBone.handle,
                                      fWeight, fFirst, fCount);
                    }
                }
            });
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
    private static native int nGetSkeleton(int handle);
    private static native void nAddTransform(int handle, int bone, int weight,
                                             int firstVertex, int numVertices);
}
