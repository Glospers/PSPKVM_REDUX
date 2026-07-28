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

    /*
     * m3gCreateImage flags, m3g/inc/M3G/m3g_core.h:403-406.
     *
     * An immutable JSR-184 image is NOT created with M3G_STATIC: the engine
     * refuses m3gSetImage and m3gSetImagePalette on a static image
     * (m3gcore/src/m3g_image.c:1558, :1726), so the sequence is create
     * writable, fill, then m3gCommitImage -- which is what sets M3G_STATIC
     * (:1384). That is exactly what m3gcore's own file loader does
     * (m3gcore/src/m3g_loader.c:2135-2175).
     */
    private static final int FLAG_DYNAMIC  = 0x02;
    private static final int FLAG_PALETTED = 0x08;

    private int format, width, height;
    private boolean mutable;

    /** Wrapper for an object that already exists in the engine; see
     *  Object3D.createWrapper. */
    Image2D() {
    }

    public Image2D(int format, int width, int height) {
        checkShape(format, width, height);
        this.format = format;
        this.width = width;
        this.height = height;
        this.mutable = true;
        handle = nCreate(format, width, height, FLAG_DYNAMIC);
    }

    public Image2D(int format, int width, int height, byte[] image) {
        checkShape(format, width, height);
        if (image == null) {
            throw new NullPointerException();
        }
        this.format = format;
        this.width = width;
        this.height = height;
        handle = nCreate(format, width, height, FLAG_DYNAMIC);
        if (handle != 0) {
            nSetImage(handle, image);
            nCommit(handle);
        }
    }

    public Image2D(int format, int width, int height, byte[] image,
                   byte[] palette) {
        checkShape(format, width, height);
        if (image == null || palette == null) {
            throw new NullPointerException();
        }
        this.format = format;
        this.width = width;
        this.height = height;
        handle = nCreate(format, width, height, FLAG_PALETTED);
        if (handle != 0) {
            /* The engine counts palette entries, not bytes
             * (m3gcore/src/m3g_loader.c:2175). */
            nSetPalette(handle, palette.length / bytesPerPixel(format), palette);
            nSetImage(handle, image);
            nCommit(handle);
        }
    }

    /**
     * Builds an immutable image out of an lcdui Image.
     *
     * The conversion is done here rather than in the native layer because
     * lcdui is the only thing that knows how to read its own image data, and
     * it already offers exactly the accessor needed -- getRGB(), which yields
     * 32-bit ARGB regardless of how the image is actually stored.
     */
    public Image2D(int format, Object image) {
        if (image == null) {
            throw new NullPointerException();
        }
        if (!(image instanceof javax.microedition.lcdui.Image)) {
            throw new IllegalArgumentException("unsupported image object");
        }
        javax.microedition.lcdui.Image img =
            (javax.microedition.lcdui.Image) image;

        this.format = format;
        this.width = img.getWidth();
        this.height = img.getHeight();
        checkShape(format, this.width, this.height);

        int[] argb = new int[this.width * this.height];
        img.getRGB(argb, 0, this.width, 0, 0, this.width, this.height);

        handle = nCreate(format, this.width, this.height, FLAG_DYNAMIC);
        if (handle != 0) {
            nSetImage(handle, pack(argb, format));
            nCommit(handle);
        }
    }

    /** Bytes per pixel of the external (source) layout of a format. */
    private static int bytesPerPixel(int format) {
        switch (format) {
        case ALPHA:
        case LUMINANCE:       return 1;
        case LUMINANCE_ALPHA: return 2;
        case RGB:             return 3;
        default:              return 4;
        }
    }

    /** ARGB pixels laid out the way the requested Image2D format wants them. */
    private static byte[] pack(int[] argb, int format) {
        byte[] out = new byte[argb.length * bytesPerPixel(format)];
        for (int i = 0, o = 0; i < argb.length; i++) {
            int p = argb[i];
            int a = (p >>> 24) & 0xFF;
            int r = (p >>> 16) & 0xFF;
            int g = (p >>>  8) & 0xFF;
            int b =  p         & 0xFF;
            switch (format) {
            case ALPHA:
                out[o++] = (byte) a;
                break;
            case LUMINANCE:
                out[o++] = (byte) ((r * 77 + g * 151 + b * 28) >> 8);
                break;
            case LUMINANCE_ALPHA:
                out[o++] = (byte) ((r * 77 + g * 151 + b * 28) >> 8);
                out[o++] = (byte) a;
                break;
            case RGB:
                out[o++] = (byte) r; out[o++] = (byte) g; out[o++] = (byte) b;
                break;
            default:
                out[o++] = (byte) r; out[o++] = (byte) g;
                out[o++] = (byte) b; out[o++] = (byte) a;
                break;
            }
        }
        return out;
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
        if (handle != 0) {
            /* No commit: a mutable image stays mutable, and m3gCommitImage is
             * what would make it static. */
            nSetSubImage(handle, x, y, width, height, image);
        }
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate(int format, int width, int height,
                                      int flags);
    private static native void nSetImage(int handle, byte[] pixels);
    private static native void nSetPalette(int handle, int length,
                                           byte[] palette);
    private static native void nSetSubImage(int handle, int x, int y,
                                            int width, int height,
                                            byte[] pixels);
    private static native void nCommit(int handle);
}
