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
            construct();
        }
    }

    void createDeferred() {
        handle = nCreate(vertexBuffer.handle,
                         handles(submeshes),
                         handles(appearances));
        register();
    }

    /** The constructor arguments, for the subclasses that rebuild themselves. */
    VertexBuffer rawVertexBuffer() { return vertexBuffer; }
    IndexBuffer[] rawSubmeshes()   { return submeshes; }

    /** The appearance array as normalised by the constructor above. */
    Appearance[] getAppearances() {
        return appearances;
    }

    /*
     * As in Group: a Mesh that came out of Loader has no Java-side geometry,
     * so these read through to the engine whenever there is one to read.
     */

    public VertexBuffer getVertexBuffer() {
        if (handle != 0) {
            return (VertexBuffer) Object3D.wrap(nGetVertexBuffer(handle));
        }
        return vertexBuffer;
    }

    public int getSubmeshCount() {
        if (handle != 0) {
            return nGetSubmeshCount(handle);
        }
        return (submeshes != null) ? submeshes.length : 0;
    }

    public IndexBuffer getIndexBuffer(int index) {
        if (handle != 0) {
            if (index < 0 || index >= nGetSubmeshCount(handle)) {
                throw new IndexOutOfBoundsException();
            }
            return (IndexBuffer) Object3D.wrap(nGetIndexBuffer(handle, index));
        }
        return submeshes[index];
    }

    /**
     * Sets the appearance of one submesh.
     *
     * The array below only exists for a Mesh a MIDlet built: one that came out
     * of Loader was made by Object3D.createWrapper, whose constructor takes no
     * geometry and leaves every field here null, because the engine holds all
     * of it. Assigning into the array unconditionally therefore threw a
     * NullPointerException on the first loaded mesh anyone re-dressed -- which
     * is what titles do to a scene the moment they have loaded it, and it
     * killed the thread that was doing the loading.
     *
     * So the engine is asked first and is the only thing that has to answer;
     * the array is updated when there is one, since it is what applyDeferred
     * re-pushes and what getAppearance reads while the object has no handle.
     */
    public void setAppearance(int index, Appearance appearance) {
        if (handle != 0) {
            if (index < 0 || index >= nGetSubmeshCount(handle)) {
                throw new IndexOutOfBoundsException();
            }
            if (appearance != null && appearance.handle == 0) {
                /* Deferred appearance into a live mesh: forwarding now would
                 * strip the file's appearance for nothing. See
                 * Object3D.linkLater. */
                final int fIndex = index;
                final Appearance fValue = appearance;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fValue.handle != 0) {
                            nSetAppearance(handle, fIndex, fValue.handle);
                        }
                    }
                });
            }
            else {
                nSetAppearance(handle, index,
                               (appearance != null) ? appearance.handle : 0);
            }
        }
        if (appearances != null) {
            if (index < 0 || index >= appearances.length) {
                throw new IndexOutOfBoundsException();
            }
            appearances[index] = appearance;
        }
    }

    public Appearance getAppearance(int index) {
        if (handle != 0) {
            if (index < 0 || index >= nGetSubmeshCount(handle)) {
                throw new IndexOutOfBoundsException();
            }
            return (Appearance) Object3D.wrap(nGetAppearance(handle, index));
        }
        return appearances[index];
    }

    /** Re-pushes appearances, which may have been set after construction. */
    void applyDeferred() {
        super.applyDeferred();
        if (appearances == null) {
            return;   /* built by the loader; the engine already has them */
        }
        for (int i = 0; i < appearances.length; i++) {
            if (appearances[i] != null && appearances[i].handle != 0) {
                nSetAppearance(handle, i, appearances[i].handle);
            }
        }
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int vertices, int[] submeshes,
                                      int[] appearances);
    private static native void nSetAppearance(int handle, int index,
                                              int appearance);
    private static native int nGetSubmeshCount(int handle);
    private static native int nGetVertexBuffer(int handle);
    private static native int nGetIndexBuffer(int handle, int index);
    private static native int nGetAppearance(int handle, int index);
}
