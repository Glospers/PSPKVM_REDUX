/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Background extends Object3D {

    public static final int BORDER = 32;
    public static final int REPEAT = 33;

    private int color;
    private Image2D image;
    private int imageModeX = BORDER, imageModeY = BORDER;
    private int cropX, cropY, cropWidth, cropHeight;
    private boolean colorClearEnabled = true;
    private boolean depthClearEnabled = true;

    public Background() {
    }

    public void setColor(int ARGB) {
        this.color = ARGB;
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
    }

    public int getCropX()      { return cropX; }
    public int getCropY()      { return cropY; }
    public int getCropWidth()  { return cropWidth; }
    public int getCropHeight() { return cropHeight; }

    public void setColorClearEnable(boolean enable) { colorClearEnabled = enable; }
    public boolean isColorClearEnabled()            { return colorClearEnabled; }
    public void setDepthClearEnable(boolean enable) { depthClearEnabled = enable; }
    public boolean isDepthClearEnabled()            { return depthClearEnabled; }
}
