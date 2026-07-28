/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


/**
 * Loads .m3g scene files and PNG images.
 *
 * Phase 1 reports an unsupported format rather than pretending to decode; the
 * m3gcore loader is attached in the next phase.
 */
public final class Loader {

    private Loader() {
    }

    public static Object3D[] load(String name) throws java.io.IOException {
        if (name == null) {
            throw new NullPointerException();
        }
        throw new java.io.IOException("m3g loader not available yet");
    }

    public static Object3D[] load(byte[] data, int offset)
            throws java.io.IOException {
        if (data == null) {
            throw new NullPointerException();
        }
        if (offset < 0 || offset >= data.length) {
            throw new IndexOutOfBoundsException();
        }
        throw new java.io.IOException("m3g loader not available yet");
    }
}
