/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Image2D extends Object3D {

    public static final int ALPHA           = 96;
    public static final int LUMINANCE       = 97;
    public static final int LUMINANCE_ALPHA = 98;
    public static final int RGB             = 99;
    public static final int RGBA            = 100;

    private int format, width, height;
    private boolean mutable;

    public Image2D(int format, int width, int height) {
        checkShape(format, width, height);
        this.format = format;
        this.width = width;
        this.height = height;
        this.mutable = true;
    }

    public Image2D(int format, int width, int height, byte[] image) {
        checkShape(format, width, height);
        if (image == null) {
            throw new NullPointerException();
        }
        this.format = format;
        this.width = width;
        this.height = height;
    }

    public Image2D(int format, int width, int height, byte[] image,
                   byte[] palette) {
        this(format, width, height, image);
        if (palette == null) {
            throw new NullPointerException();
        }
    }

    public Image2D(int format, Object image) {
        if (image == null) {
            throw new NullPointerException();
        }
        this.format = format;
        if (image instanceof javax.microedition.lcdui.Image) {
            javax.microedition.lcdui.Image img =
                (javax.microedition.lcdui.Image) image;
            this.width = img.getWidth();
            this.height = img.getHeight();
        } else {
            throw new IllegalArgumentException("unsupported image object");
        }
    }

    private static void checkShape(int format, int width, int height) {
        if (format < ALPHA || format > RGBA) {
            throw new IllegalArgumentException("invalid image format");
        }
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("image must be non-empty");
        }
    }

    public int getFormat()    { return format; }
    public int getWidth()     { return width; }
    public int getHeight()    { return height; }
    public boolean isMutable() { return mutable; }

    public void set(int x, int y, int width, int height, byte[] image) {
        if (image == null) {
            throw new NullPointerException();
        }
        if (!mutable) {
            throw new IllegalStateException("image is immutable");
        }
    }
}
