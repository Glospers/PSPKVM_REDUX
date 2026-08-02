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
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
    }

    public int getVertexCount() {
        if (handle != 0) {
            return nGetVertexCount(handle);
        }
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
        if (handle != 0) {
            if (positions != null && positions.handle == 0) {
                /* The last unguarded setter family.  A title that builds a
                 * fresh array during loading and hands it to a LIVE loaded
                 * buffer forwarded a zero handle here -- for this title
                 * that was the sky dome's dynamic colour array: the warm
                 * gradient existed, was updated every frame, and was never
                 * attached, so the dome drew colourless.  See
                 * Object3D.linkLater. */
                final VertexArray fArray = positions;
                final float fScale = scale;
                final float[] fBias = (bias != null)
                    ? new float[] { bias[0], bias[1], bias[2] } : null;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fArray.handle != 0
                                && VertexBuffer.this.positions == fArray) {
                            nSetPositions(handle, fArray.handle,
                                          fScale, fBias);
                        }
                    }
                });
            }
            else {
                nSetPositions(handle,
                              (positions != null) ? positions.handle : 0,
                              scale, bias);
            }
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
        if (handle != 0) {
            if (normals != null && normals.handle == 0) {
                final VertexArray fArray = normals;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fArray.handle != 0
                                && VertexBuffer.this.normals == fArray) {
                            nSetNormals(handle, fArray.handle);
                        }
                    }
                });
            }
            else {
                nSetNormals(handle, (normals != null) ? normals.handle : 0);
            }
        }
    }

    public VertexArray getNormals() {
        return normals;
    }

    public void setColors(VertexArray colors) {
        this.colors = colors;
        if (handle != 0) {
            if (colors != null && colors.handle == 0) {
                /* The sky dome's warm gradient came through here and was
                 * dropped; see setPositions above. */
                final VertexArray fArray = colors;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fArray.handle != 0
                                && VertexBuffer.this.colors == fArray) {
                            nSetColors(handle, fArray.handle);
                        }
                    }
                });
            }
            else {
                nSetColors(handle, (colors != null) ? colors.handle : 0);
            }
        }
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
        if (handle != 0) {
            if (texCoords != null && texCoords.handle == 0) {
                final VertexArray fArray = texCoords;
                final int fIndex = index;
                final float fScale = scale;
                /* No Object.clone() on CLDC; copy by hand. */
                float[] biasCopy = null;
                if (bias != null) {
                    biasCopy = new float[bias.length];
                    System.arraycopy(bias, 0, biasCopy, 0, bias.length);
                }
                final float[] fBias = biasCopy;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fArray.handle != 0
                                && VertexBuffer.this.texCoords[fIndex] == fArray) {
                            nSetTexCoords(handle, fIndex, fArray.handle,
                                          fScale, fBias);
                        }
                    }
                });
            }
            else {
                nSetTexCoords(handle, index,
                              (texCoords != null) ? texCoords.handle : 0,
                              scale, bias);
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

    /** TEMPORARY -- diagnostic budget for the tint trace below. */
    private static int colorDiagLeft = 10;

    public void setDefaultColor(int ARGB) {
        this.defaultColor = ARGB;
        if ((ARGB & 0x00FFFFFF) != 0x00FFFFFF && colorDiagLeft > 0) {
            /* Only non-white tints: the first trace burned its budget on
             * load-time white and would have missed the depth tinting
             * that happens during play. */
            colorDiagLeft--;
            Object3D.nDiag(-12, ARGB);
            Object3D.nDiag(-13, (handle != 0) ? 1 : 0);
        }
        if (handle != 0) {
            nSetDefaultColor(handle, ARGB);
        }
    }

    public int getDefaultColor() {
        return defaultColor;
    }

    void applyDeferred() {
        if (positions != null && positions.handle != 0) {
            nSetPositions(handle, positions.handle, positionScaleBias[0],
                          new float[] { positionScaleBias[1],
                                        positionScaleBias[2],
                                        positionScaleBias[3] });
        }
        if (normals != null && normals.handle != 0) {
            nSetNormals(handle, normals.handle);
        }
        if (colors != null && colors.handle != 0) {
            nSetColors(handle, colors.handle);
        }
        for (int i = 0; i < texCoords.length; i++) {
            if (texCoords[i] != null && texCoords[i].handle != 0) {
                nSetTexCoords(handle, i, texCoords[i].handle, texScaleBias[i][0],
                              new float[] { texScaleBias[i][1],
                                            texScaleBias[i][2],
                                            texScaleBias[i][3] });
            }
        }
        nSetDefaultColor(handle, defaultColor);
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetPositions(int handle, int array,
                                             float scale, float[] bias);
    private static native void nSetNormals(int handle, int array);
    private static native void nSetColors(int handle, int array);
    private static native void nSetTexCoords(int handle, int unit, int array,
                                             float scale, float[] bias);
    private static native void nSetDefaultColor(int handle, int ARGB);
    private static native int nGetVertexCount(int handle);
}
