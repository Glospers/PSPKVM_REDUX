/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Sprite3D extends Node {

    private boolean scaled;
    private Image2D image;
    private Appearance appearance;
    private int cropX, cropY, cropWidth, cropHeight;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    Sprite3D() {
    }

    public Sprite3D(boolean scaled, Image2D image, Appearance appearance) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.scaled = scaled;
        this.image = image;
        this.appearance = appearance;
        this.cropWidth = image.getWidth();
        this.cropHeight = image.getHeight();
        construct();
    }

    void createDeferred() {
        handle = nCreate(scaled ? 1 : 0, image.handle,
                         (appearance != null) ? appearance.handle : 0);
        register();
    }

    void applyDeferred() {
        super.applyDeferred();
        nSetCrop(handle, cropX, cropY, cropWidth, cropHeight);
    }

    public boolean isScaled() {
        return scaled;
    }

    public void setImage(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.image = image;
        if (handle != 0) {
            if (image.handle == 0) {
                /* Same hole as Background.setImage: a deferred image into a
                 * live sprite forwarded zero and stripped it.  See
                 * Object3D.linkLater. */
                final Image2D fImage = image;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fImage.handle != 0
                                && Sprite3D.this.image == fImage) {
                            nSetImage(handle, fImage.handle);
                        }
                    }
                });
            }
            else {
                nSetImage(handle, image.handle);
            }
        }
    }

    public Image2D getImage() {
        return image;
    }

    public void setAppearance(Appearance appearance) {
        this.appearance = appearance;
        if (handle != 0) {
            nSetAppearance(handle,
                           (appearance != null) ? appearance.handle : 0);
        }
    }

    public Appearance getAppearance() {
        return appearance;
    }

    public void setCrop(int cropX, int cropY, int width, int height) {
        this.cropX = cropX;
        this.cropY = cropY;
        this.cropWidth = width;
        this.cropHeight = height;
        if (handle != 0) {
            nSetCrop(handle, cropX, cropY, width, height);
        }
    }

    public int getCropX()      { return cropX; }
    public int getCropY()      { return cropY; }
    public int getCropWidth()  { return cropWidth; }
    public int getCropHeight() { return cropHeight; }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int scaled, int image, int appearance);
    private static native void nSetImage(int handle, int image);
    private static native void nSetAppearance(int handle, int appearance);
    private static native void nSetCrop(int handle, int cropX, int cropY,
                                        int width, int height);
}
