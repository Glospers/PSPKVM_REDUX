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
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
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
            if (compositingMode != null && compositingMode.handle == 0) {
                final CompositingMode fValue = compositingMode;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fValue.handle != 0) {
                            nSetCompositingMode(handle, fValue.handle);
                        }
                    }
                });
            }
            else {
                nSetCompositingMode(handle, (compositingMode != null) ? compositingMode.handle : 0);
            }
        }
    }

    public CompositingMode getCompositingMode() {
        if (handle != 0) {
            return (CompositingMode) Object3D.wrap(nGetCompositingMode(handle));
        }
        return compositingMode;
    }

    public void setFog(Fog fog) {
        this.fog = fog;
        if (handle != 0) {
            if (fog != null && fog.handle == 0) {
                final Fog fValue = fog;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fValue.handle != 0) {
                            nSetFog(handle, fValue.handle);
                        }
                    }
                });
            }
            else {
                nSetFog(handle, (fog != null) ? fog.handle : 0);
            }
        }
    }

    public Fog getFog() {
        if (handle != 0) {
            return (Fog) Object3D.wrap(nGetFog(handle));
        }
        return fog;
    }

    public void setPolygonMode(PolygonMode polygonMode) {
        this.polygonMode = polygonMode;
        if (handle != 0) {
            if (polygonMode != null && polygonMode.handle == 0) {
                final PolygonMode fValue = polygonMode;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fValue.handle != 0) {
                            nSetPolygonMode(handle, fValue.handle);
                        }
                    }
                });
            }
            else {
                nSetPolygonMode(handle,
                                (polygonMode != null) ? polygonMode.handle : 0);
            }
        }
    }

    public PolygonMode getPolygonMode() {
        if (handle != 0) {
            return (PolygonMode) Object3D.wrap(nGetPolygonMode(handle));
        }
        return polygonMode;
    }

    public void setMaterial(Material material) {
        this.material = material;
        if (handle != 0) {
            if (material != null && material.handle == 0) {
                final Material fValue = material;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fValue.handle != 0) {
                            nSetMaterial(handle, fValue.handle);
                        }
                    }
                });
            }
            else {
                nSetMaterial(handle, (material != null) ? material.handle : 0);
            }
        }
    }

    public Material getMaterial() {
        if (handle != 0) {
            return (Material) Object3D.wrap(nGetMaterial(handle));
        }
        return material;
    }

    public void setTexture(int index, Texture2D texture) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        textures[index] = texture;
        if (handle != 0) {
            if (texture != null && texture.handle == 0) {
                /* The value exists only in Java so far; forwarding now would
                 * hand the engine a zero handle, which strips whatever the
                 * file put here. Replay once the renderer has built it. */
                final int fIndex = index;
                final Texture2D fTexture = texture;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fTexture.handle != 0) {
                            nSetTexture(handle, fIndex, fTexture.handle);
                        }
                    }
                });
            }
            else {
                nSetTexture(handle, index,
                            (texture != null) ? texture.handle : 0);
            }
        }
    }

    public Texture2D getTexture(int index) {
        if (index < 0 || index >= textures.length) {
            throw new IndexOutOfBoundsException();
        }
        if (handle != 0) {
            return (Texture2D) Object3D.wrap(nGetTexture(handle, index));
        }
        return textures[index];
    }

    void applyDeferred() {
        nSetLayer(handle, layer);
        if (material != null)        { nSetMaterial(handle, material.handle); }
        if (polygonMode != null)     { nSetPolygonMode(handle, polygonMode.handle); }
        if (compositingMode != null) { nSetCompositingMode(handle, compositingMode.handle); }
        if (fog != null)             { nSetFog(handle, fog.handle); }
        for (int i = 0; i < textures.length; i++) {
            if (textures[i] != null && textures[i].handle != 0) {
                nSetTexture(handle, i, textures[i].handle);
            }
        }
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetLayer(int handle, int layer);
    private static native void nSetMaterial(int handle, int material);
    private static native void nSetPolygonMode(int handle, int polygonMode);
    private static native void nSetCompositingMode(int handle, int mode);
    private static native void nSetFog(int handle, int fog);
    private static native void nSetTexture(int handle, int unit, int texture);
    private static native int nGetMaterial(int handle);
    private static native int nGetPolygonMode(int handle);
    private static native int nGetCompositingMode(int handle);
    private static native int nGetFog(int handle);
    private static native int nGetTexture(int handle, int unit);
    private static native int nGetLayer(int handle);
}
