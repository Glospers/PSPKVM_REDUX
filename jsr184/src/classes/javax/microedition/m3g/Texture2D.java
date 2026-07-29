/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Texture2D extends Transformable {

    public static final int FILTER_BASE_LEVEL = 208;
    public static final int FILTER_LINEAR     = 209;
    public static final int FILTER_NEAREST    = 210;
    public static final int FUNC_ADD          = 224;
    public static final int FUNC_BLEND        = 225;
    public static final int FUNC_DECAL        = 226;
    public static final int FUNC_MODULATE     = 227;
    public static final int FUNC_REPLACE      = 228;
    public static final int WRAP_CLAMP        = 240;
    public static final int WRAP_REPEAT       = 241;

    private Image2D image;
    private int blendColor;
    private int blending = FUNC_MODULATE;
    private int wrappingS = WRAP_REPEAT;
    private int wrappingT = WRAP_REPEAT;
    private int levelFilter = FILTER_BASE_LEVEL;
    private int imageFilter = FILTER_NEAREST;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    Texture2D() {
    }

    public Texture2D(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.image = image;
        construct();
    }

    void createDeferred() {
        handle = nCreate(image.handle);
        register();
    }

    void applyDeferred() {
        super.applyDeferred();
        nSetFiltering(handle, levelFilter, imageFilter);
        nSetWrapping(handle, wrappingS, wrappingT);
        nSetBlending(handle, blending);
        nSetBlendColor(handle, blendColor);
    }

    public void setImage(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.image = image;
        if (handle != 0) {
            nSetImage(handle, image.handle);
        }
    }

    public Image2D getImage() {
        if (handle != 0) {
            return (Image2D) Object3D.wrap(nGetImage(handle));
        }
        return image;
    }

    public void setFiltering(int levelFilter, int imageFilter) {
        this.levelFilter = levelFilter;
        this.imageFilter = imageFilter;
        if (handle != 0) {
            nSetFiltering(handle, levelFilter, imageFilter);
        }
    }

    public int getLevelFilter() { return levelFilter; }
    public int getImageFilter() { return imageFilter; }

    public void setWrapping(int wrapS, int wrapT) {
        this.wrappingS = wrapS;
        this.wrappingT = wrapT;
        if (handle != 0) {
            nSetWrapping(handle, wrapS, wrapT);
        }
    }

    public int getWrappingS() { return wrappingS; }
    public int getWrappingT() { return wrappingT; }

    public void setBlending(int func) {
        this.blending = func;
        if (handle != 0) {
            nSetBlending(handle, func);
        }
    }

    public int getBlending() {
        return blending;
    }

    public void setBlendColor(int RGB) {
        this.blendColor = RGB & 0x00FFFFFF;
        if (handle != 0) {
            nSetBlendColor(handle, this.blendColor);
        }
    }

    public int getBlendColor() {
        return blendColor;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int image);
    private static native void nSetImage(int handle, int image);
    private static native void nSetFiltering(int handle, int levelFilter,
                                             int imageFilter);
    private static native void nSetWrapping(int handle, int wrapS, int wrapT);
    private static native void nSetBlending(int handle, int func);
    private static native void nSetBlendColor(int handle, int RGB);
    private static native int nGetImage(int handle);
}
