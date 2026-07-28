/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Mesh extends Node {

    private VertexBuffer vertexBuffer;
    private IndexBuffer[] submeshes;
    private Appearance[] appearances;

    public Mesh(VertexBuffer vertices, IndexBuffer submesh,
                Appearance appearance) {
        this(vertices,
             new IndexBuffer[] { submesh },
             new Appearance[] { appearance },
             true);
    }

    public Mesh(VertexBuffer vertices, IndexBuffer[] submeshes,
                Appearance[] appearances) {
        this(vertices, submeshes, appearances, true);
    }

    Mesh() {
    }

    /**
     * The shared part of the two public constructors.
     *
     * MorphingMesh and SkinnedMesh are Meshes in Java but not in the engine --
     * each has its own m3gCreate* -- so they run the Java-side setup with
     * <code>createEngineObject</code> false and then build their own.
     */
    Mesh(VertexBuffer vertices, IndexBuffer[] submeshes,
         Appearance[] appearances, boolean createEngineObject) {
        if (vertices == null || submeshes == null) {
            throw new NullPointerException();
        }
        if (submeshes.length == 0
                || (appearances != null && appearances.length < submeshes.length)) {
            throw new IllegalArgumentException("submesh/appearance mismatch");
        }
        for (int i = 0; i < submeshes.length; i++) {
            if (submeshes[i] == null) {
                throw new NullPointerException();
            }
        }
        this.vertexBuffer = vertices;
        this.submeshes = submeshes;
        this.appearances = (appearances != null)
            ? appearances : new Appearance[submeshes.length];

        if (createEngineObject) {
            handle = nCreate(vertices.handle,
                             handles(submeshes),
                             handles(this.appearances));
        }
    }

    /** The appearance array as normalised by the constructor above. */
    Appearance[] getAppearances() {
        return appearances;
    }

    public VertexBuffer getVertexBuffer() {
        return vertexBuffer;
    }

    public int getSubmeshCount() {
        return (submeshes != null) ? submeshes.length : 0;
    }

    public IndexBuffer getIndexBuffer(int index) {
        return submeshes[index];
    }

    public void setAppearance(int index, Appearance appearance) {
        appearances[index] = appearance;
        if (handle != 0) {
            nSetAppearance(handle, index,
                           (appearance != null) ? appearance.handle : 0);
        }
    }

    public Appearance getAppearance(int index) {
        return appearances[index];
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int vertices, int[] submeshes,
                                      int[] appearances);
    private static native void nSetAppearance(int handle, int index,
                                              int appearance);
}
