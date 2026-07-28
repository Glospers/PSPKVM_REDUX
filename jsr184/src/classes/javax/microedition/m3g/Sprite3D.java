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

    public Sprite3D(boolean scaled, Image2D image, Appearance appearance) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.scaled = scaled;
        this.image = image;
        this.appearance = appearance;
        this.cropWidth = image.getWidth();
        this.cropHeight = image.getHeight();
    }

    public boolean isScaled() {
        return scaled;
    }

    public void setImage(Image2D image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.image = image;
    }

    public Image2D getImage() {
        return image;
    }

    public void setAppearance(Appearance appearance) {
        this.appearance = appearance;
    }

    public Appearance getAppearance() {
        return appearance;
    }

    public void setCrop(int cropX, int cropY, int width, int height) {
        this.cropX = cropX;
        this.cropY = cropY;
        this.cropWidth = width;
        this.cropHeight = height;
    }

    public int getCropX()      { return cropX; }
    public int getCropY()      { return cropY; }
    public int getCropWidth()  { return cropWidth; }
    public int getCropHeight() { return cropHeight; }
}
