/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class VertexArray extends Object3D {

    private int numVertices, numComponents, componentSize;
    private byte[] byteValues;
    private short[] shortValues;

    public VertexArray(int numVertices, int numComponents, int componentSize) {
        if (numVertices < 1 || numVertices > 65535
                || numComponents < 2 || numComponents > 4
                || (componentSize != 1 && componentSize != 2)) {
            throw new IllegalArgumentException("invalid vertex array shape");
        }
        this.numVertices = numVertices;
        this.numComponents = numComponents;
        this.componentSize = componentSize;
        if (componentSize == 1) {
            byteValues = new byte[numVertices * numComponents];
        } else {
            shortValues = new short[numVertices * numComponents];
        }
    }

    public int getVertexCount()   { return numVertices; }
    public int getComponentCount() { return numComponents; }
    public int getComponentType()  { return componentSize; }

    public void set(int firstVertex, int numVertices, byte[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (byteValues == null) {
            throw new IllegalStateException("array is not of byte type");
        }
        System.arraycopy(values, 0, byteValues,
                         firstVertex * numComponents,
                         numVertices * numComponents);
    }

    public void set(int firstVertex, int numVertices, short[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (shortValues == null) {
            throw new IllegalStateException("array is not of short type");
        }
        System.arraycopy(values, 0, shortValues,
                         firstVertex * numComponents,
                         numVertices * numComponents);
    }

    public void get(int firstVertex, int numVertices, byte[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (byteValues == null) {
            throw new IllegalStateException("array is not of byte type");
        }
        System.arraycopy(byteValues, firstVertex * numComponents,
                         values, 0, numVertices * numComponents);
    }

    public void get(int firstVertex, int numVertices, short[] values) {
        if (values == null) {
            throw new NullPointerException();
        }
        if (shortValues == null) {
            throw new IllegalStateException("array is not of short type");
        }
        System.arraycopy(shortValues, firstVertex * numComponents,
                         values, 0, numVertices * numComponents);
    }
}
