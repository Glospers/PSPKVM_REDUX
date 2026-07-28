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
    }

    public void setLayer(int layer) {
        if (layer < -63 || layer > 63) {
            throw new IndexOutOfBoundsException("layer must be in [-63,63]");
        }
        this.layer = layer;
    }

    public int getLayer() {
        return layer;
    }

    public void setCompositingMode(CompositingMode compositingMode) {
        this.compositingMode = compositingMode;
    }

    public CompositingMode getCompositingMode() {
        return compositingMode;
    }

    public void setFog(Fog fog) {
        this.fog = fog;
    }

    public Fog getFog() {
        return fog;
    }

    public void setPolygonMode(PolygonMode polygonMode) {
        this.polygonMode = polygonMode;
    }

    public PolygonMode getPolygonMode() {
        return polygonMode;
    }

    public void setMaterial(Material material) {
        this.material = material;
    }

    public Material getMaterial() {
        return material;
    }

    public void setTexture(int index, Texture2D texture) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        textures[index] = texture;
    }

    public Texture2D getTexture(int index) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        return textures[index];
    }
}
