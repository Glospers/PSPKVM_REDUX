/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class VertexBuffer extends Object3D {

    private VertexArray positions, normals, colors;
    private VertexArray[] texCoords = new VertexArray[2];
    private float[] positionScaleBias = new float[] { 1.0f, 0.0f, 0.0f, 0.0f };
    private float[][] texScaleBias = new float[2][4];
    private int defaultColor = 0xFFFFFFFF;

    public VertexBuffer() {
        for (int i = 0; i < 2; i++) {
            texScaleBias[i][0] = 1.0f;
        }
    }

    public int getVertexCount() {
        return (positions != null) ? positions.getVertexCount() : 0;
    }

    public void setPositions(VertexArray positions, float scale, float[] bias) {
        this.positions = positions;
        positionScaleBias[0] = scale;
        if (bias != null) {
            positionScaleBias[1] = bias[0];
            positionScaleBias[2] = bias[1];
            positionScaleBias[3] = bias[2];
        }
    }

    public VertexArray getPositions(float[] scaleBias) {
        if (scaleBias != null && scaleBias.length >= 4) {
            System.arraycopy(positionScaleBias, 0, scaleBias, 0, 4);
        }
        return positions;
    }

    public void setNormals(VertexArray normals) {
        this.normals = normals;
    }

    public VertexArray getNormals() {
        return normals;
    }

    public void setColors(VertexArray colors) {
        this.colors = colors;
    }

    public VertexArray getColors() {
        return colors;
    }

    public void setTexCoords(int index, VertexArray texCoords,
                             float scale, float[] bias) {
        if (index < 0 || index >= this.texCoords.length) {
            throw new IndexOutOfBoundsException();
        }
        this.texCoords[index] = texCoords;
        texScaleBias[index][0] = scale;
        if (bias != null) {
            for (int i = 0; i < bias.length && i < 3; i++) {
                texScaleBias[index][i + 1] = bias[i];
            }
        }
    }

    public VertexArray getTexCoords(int index, float[] scaleBias) {
        if (index < 0 || index >= texCoords.length) {
            throw new IndexOutOfBoundsException();
        }
        if (scaleBias != null && scaleBias.length >= 4) {
            System.arraycopy(texScaleBias[index], 0, scaleBias, 0, 4);
        }
        return texCoords[index];
    }

    public void setDefaultColor(int ARGB) {
        this.defaultColor = ARGB;
    }

    public int getDefaultColor() {
        return defaultColor;
    }
}
