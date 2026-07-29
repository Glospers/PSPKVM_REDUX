/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Background extends Object3D {

    public static final int BORDER = 32;
    public static final int REPEAT = 33;

    /* m3gSetBgEnable selectors, m3g/inc/M3G/m3g_core.h:660-661. */
    private static final int ENABLE_COLOR_CLEAR = 0;
    private static final int ENABLE_DEPTH_CLEAR = 1;

    private int color;
    private Image2D image;
    private int imageModeX = BORDER, imageModeY = BORDER;
    private int cropX, cropY, cropWidth, cropHeight;
    private boolean colorClearEnabled = true;
    private boolean depthClearEnabled = true;

    public Background() {
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
    }

    public void setColor(int ARGB) {
        this.color = ARGB;
        if (handle != 0) {
            nSetColor(handle, ARGB);
        }
    }

    public int getColor() {
        return color;
    }

    public void setImage(Image2D image) {
        this.image = image;
        if (image != null) {
            cropWidth = image.getWidth();
            cropHeight = image.getHeight();
        }
        if (handle != 0) {
            nSetImage(handle, (image != null) ? image.handle : 0);
        }
    }

    public Image2D getImage() {
        return image;
    }

    public void setImageMode(int modeX, int modeY) {
        if ((modeX != BORDER && modeX != REPEAT)
                || (modeY != BORDER && modeY != REPEAT)) {
            throw new IllegalArgumentException("invalid image mode");
        }
        imageModeX = modeX;
        imageModeY = modeY;
        if (handle != 0) {
            nSetImageMode(handle, modeX, modeY);
        }
    }

    public int getImageModeX() { return imageModeX; }
    public int getImageModeY() { return imageModeY; }

    public void setCrop(int cropX, int cropY, int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("crop must be non-negative");
        }
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

    public void setColorClearEnable(boolean enable) {
        colorClearEnabled = enable;
        if (handle != 0) {
            nSetEnable(handle, ENABLE_COLOR_CLEAR, enable ? 1 : 0);
        }
    }

    public boolean isColorClearEnabled() { return colorClearEnabled; }

    public void setDepthClearEnable(boolean enable) {
        depthClearEnabled = enable;
        if (handle != 0) {
            nSetEnable(handle, ENABLE_DEPTH_CLEAR, enable ? 1 : 0);
        }
    }

    public boolean isDepthClearEnabled() { return depthClearEnabled; }

    void applyDeferred() {
        nSetColor(handle, color);
        if (image != null && image.handle != 0) {
            nSetImage(handle, image.handle);
        }
        nSetImageMode(handle, imageModeX, imageModeY);
        nSetCrop(handle, cropX, cropY, cropWidth, cropHeight);
        nSetEnable(handle, ENABLE_COLOR_CLEAR, colorClearEnabled ? 1 : 0);
        nSetEnable(handle, ENABLE_DEPTH_CLEAR, depthClearEnabled ? 1 : 0);
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetColor(int handle, int ARGB);
    private static native void nSetImage(int handle, int image);
    private static native void nSetImageMode(int handle, int modeX, int modeY);
    private static native void nSetCrop(int handle, int cropX, int cropY,
                                        int width, int height);
    private static native void nSetEnable(int handle, int which, int enable);
}
