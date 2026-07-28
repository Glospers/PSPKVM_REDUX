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
        if (vertices == null || submesh == null) {
            throw new NullPointerException();
        }
        this.vertexBuffer = vertices;
        this.submeshes = new IndexBuffer[] { submesh };
        this.appearances = new Appearance[] { appearance };
    }

    public Mesh(VertexBuffer vertices, IndexBuffer[] submeshes,
                Appearance[] appearances) {
        if (vertices == null || submeshes == null) {
            throw new NullPointerException();
        }
        if (submeshes.length == 0
                || (appearances != null && appearances.length < submeshes.length)) {
            throw new IllegalArgumentException("submesh/appearance mismatch");
        }
        this.vertexBuffer = vertices;
        this.submeshes = submeshes;
        this.appearances = (appearances != null)
            ? appearances : new Appearance[submeshes.length];
    }

    Mesh() {
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
    }

    public Appearance getAppearance(int index) {
        return appearances[index];
    }
}
