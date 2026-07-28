/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Appearance extends Object3D {

    private int layer;
    private CompositingMode compositingMode;
    private Fog fog;
    private PolygonMode polygonMode;
    private Material material;
    private Texture2D[] textures = new Texture2D[2];

    public Appearance() {
        handle = nCreate();
    }

    public void setLayer(int layer) {
        if (layer < -63 || layer > 63) {
            throw new IndexOutOfBoundsException("layer must be in [-63,63]");
        }
        this.layer = layer;
        if (handle != 0) {
            nSetLayer(handle, layer);
        }
    }

    public int getLayer() {
        return layer;
    }

    public void setCompositingMode(CompositingMode compositingMode) {
        this.compositingMode = compositingMode;
        if (handle != 0) {
            nSetCompositingMode(handle,
                (compositingMode != null) ? compositingMode.handle : 0);
        }
    }

    public CompositingMode getCompositingMode() {
        return compositingMode;
    }

    public void setFog(Fog fog) {
        this.fog = fog;
        if (handle != 0) {
            nSetFog(handle, (fog != null) ? fog.handle : 0);
        }
    }

    public Fog getFog() {
        return fog;
    }

    public void setPolygonMode(PolygonMode polygonMode) {
        this.polygonMode = polygonMode;
        if (handle != 0) {
            nSetPolygonMode(handle,
                            (polygonMode != null) ? polygonMode.handle : 0);
        }
    }

    public PolygonMode getPolygonMode() {
        return polygonMode;
    }

    public void setMaterial(Material material) {
        this.material = material;
        if (handle != 0) {
            nSetMaterial(handle, (material != null) ? material.handle : 0);
        }
    }

    public Material getMaterial() {
        return material;
    }

    public void setTexture(int index, Texture2D texture) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        textures[index] = texture;
        if (handle != 0) {
            nSetTexture(handle, index, (texture != null) ? texture.handle : 0);
        }
    }

    public Texture2D getTexture(int index) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        return textures[index];
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetLayer(int handle, int layer);
    private static native void nSetMaterial(int handle, int material);
    private static native void nSetPolygonMode(int handle, int polygonMode);
    private static native void nSetCompositingMode(int handle, int mode);
    private static native void nSetFog(int handle, int fog);
    private static native void nSetTexture(int handle, int unit, int texture);
}
