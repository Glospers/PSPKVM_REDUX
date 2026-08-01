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
        construct();
    }

    void createDeferred() {
        handle = nCreate(rawVertexBuffer().handle,
                         handles(targets),
                         handles(rawSubmeshes()),
                         handles(getAppearances()));
        register();
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

    /*
     * As in Mesh: a MorphingMesh that came out of Loader has no Java-side
     * targets or weights -- the wrapper constructor takes none -- so all of
     * these read through to the engine, which has them, rather than through
     * arrays that are only there for a mesh a MIDlet built.
     */

    public int getMorphTargetCount() {
        if (handle != 0) {
            return nGetMorphTargetCount(handle);
        }
        return targets.length;
    }

    public VertexBuffer getMorphTarget(int index) {
        if (handle != 0) {
            if (index < 0 || index >= nGetMorphTargetCount(handle)) {
                throw new IndexOutOfBoundsException();
            }
            return (VertexBuffer) Object3D.wrap(nGetMorphTarget(handle, index));
        }
        return targets[index];
    }

    public void setWeights(float[] weights) {
        if (weights == null) {
            throw new NullPointerException();
        }
        if (weights.length < getMorphTargetCount()) {
            throw new IllegalArgumentException("too few weights");
        }
        if (this.weights != null) {
            System.arraycopy(weights, 0, this.weights, 0, this.weights.length);
        }
        if (handle != 0) {
            nSetWeights(handle, weights, getMorphTargetCount());
        }
    }

    public void getWeights(float[] weights) {
        if (weights == null) {
            throw new NullPointerException();
        }
        if (handle != 0) {
            nGetWeights(handle, weights, getMorphTargetCount());
            return;
        }
        System.arraycopy(this.weights, 0, weights, 0, this.weights.length);
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int vertices, int[] targets,
                                      int[] submeshes, int[] appearances);
    private static native int nGetMorphTargetCount(int handle);
    private static native int nGetMorphTarget(int handle, int index);
    private static native void nSetWeights(int handle, float[] weights,
                                           int count);
    private static native void nGetWeights(int handle, float[] weights,
                                           int count);
}
