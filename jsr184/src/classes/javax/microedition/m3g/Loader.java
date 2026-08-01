/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;

import java.io.IOException;
import java.io.InputStream;

/**
 * Loads .m3g scene files.
 *
 * Both overloads funnel into one native entry point: the resource variant
 * reads the bytes here, in Java, through the same mechanism the rest of MIDP
 * uses for images (Class.getResourceAsStream, which resolves against the
 * MIDlet suite JAR), and then hands the array over. The native side therefore
 * only ever has to deal with a block of memory.
 *
 * The parse itself is m3gcore's; see m3g/src/m3g_psp_loader.c. What comes back
 * from it is the set of root objects -- the ones nothing else in the file
 * refers to -- each wrapped in the Java class its m3gcore class id names.
 *
 * Two limits worth knowing about at this stage:
 *
 *   - PNG images are not handled. The JSR-184 Loader is also allowed to load a
 *     PNG into an Image2D, but m3gcore's loader parks itself in
 *     LOADSTATE_NOT_SUPPORTED when it sees a PNG identifier
 *     (m3gcore/src/m3g_loader.c:500), so those come back as an IOException.
 *   - The scene graph below each root exists in the engine but is not mirrored
 *     into Java yet, so e.g. World.getActiveCamera() still returns null. That
 *     needs a per-class set of accessor natives and is the next step.
 */
public final class Loader {

    /*
     * Failure codes returned by nLoadData in place of an object count. They
     * mirror the M3G_PSP_ERR_* constants of m3g/inc/M3G/m3g_psp.h.
     */
    private static final int ERR_INVALID       = -1;
    private static final int ERR_NO_INTERFACE  = -2;
    private static final int ERR_OUT_OF_MEMORY = -3;
    private static final int ERR_IO            = -4;
    private static final int ERR_UNSUPPORTED   = -5;

    /*
     * The native side parks the result of a load in a single static slot
     * between nLoadData and nResultCommit/nResultAbort. A KNI native is not
     * preempted, but the Java code between two of them can be, so the whole
     * sequence is held under one monitor.
     */
    private static final Object LOCK = new Object();

    private Loader() {
    }

    public static Object3D[] load(String name) throws IOException {
        if (name == null) {
            throw new NullPointerException();
        }

        InputStream in = Loader.class.getResourceAsStream(name);
        if (in == null) {
            throw new IOException("resource not found: " + name);
        }

        byte[] data;
        try {
            data = readFully(in);
        } finally {
            try {
                in.close();
            } catch (IOException ignored) {
                /* the bytes are already read; a failed close is not the
                 * caller's problem */
            }
        }

        return decode(data, 0, data.length);
    }

    public static Object3D[] load(byte[] data, int offset) throws IOException {
        if (data == null) {
            throw new NullPointerException();
        }
        if (offset < 0 || offset >= data.length) {
            throw new IndexOutOfBoundsException();
        }
        return decode(data, offset, data.length - offset);
    }

    private static Object3D[] decode(byte[] data, int offset, int length)
            throws IOException {
        synchronized (LOCK) {
            int count = nLoadData(data, offset, length);

            if (count == ERR_OUT_OF_MEMORY) {
                throw new OutOfMemoryError("not enough memory to load M3G data");
            }
            if (count < 0) {
                throw new IOException(describe(count));
            }

            /*
             * NOT the place to build deferred objects, even though the
             * renderer is up by the time we get here.
             *
             * Materialising an Image2D commits it, and committing it uploads
             * the texture there and then -- glGenTextures, glBindTexture,
             * glTexImage2D (m3gcore/src/m3g_image.inl:146, :157, :190). That is
             * real GE traffic, and doing it here means doing it while PSPKVM's
             * own display thread is blitting the MIDP screen with sceGu. The
             * two drive the same hardware and do not coordinate.
             *
             * So the flush belongs in Graphics3D.bindTarget, inside the window
             * the MIDlet has asked to draw in, which is the only period this
             * port was ever designed to have pspgl active.
             */

            boolean loaded = false;
            try {
                Object3D[] roots = new Object3D[count];
                for (int i = 0; i < count; i++) {
                    Object3D object =
                        Object3D.createWrapper(nResultClass(i), nResultHandle(i));
                    if (object == null) {
                        throw new IOException(
                            "M3G file contains an object of an unknown class");
                    }
                    roots[i] = object;
                }
                attachUserObjects();
                loaded = true;
                return roots;
            } finally {
                /*
                 * On success the references the native side took on the roots
                 * belong to the wrappers above and only the handle array is
                 * freed; on any failure they are dropped again, which takes
                 * the rest of the scene graph with them.
                 */
                if (loaded) {
                    nResultCommit();
                } else {
                    nResultAbort();
                }
            }
        }
    }

    /**
     * Hands each loaded object the user parameters the file attached to it.
     *
     * JSR-184 delivers these as the object's user object: a Hashtable keyed by
     * the parameter id, holding the raw bytes. Titles use it to carry data the
     * format has no field for -- what a node is for, where something spawns --
     * so an object that had parameters and answers null to getUserObject() is
     * a MIDlet reading the wrong thing, not an empty one.
     *
     * Must run before nResultCommit: the parameters belong to the loader, and
     * that is what releases it.
     */
    private static void attachUserObjects() {
        int objects = nUserObjectCount();

        for (int i = 0; i < objects; i++) {
            Object3D object = Object3D.wrap(nUserObjectHandle(i));
            int count = nUserParamCount(i);

            if (object == null || count <= 0) {
                continue;
            }

            java.util.Hashtable parameters = new java.util.Hashtable();
            for (int j = 0; j < count; j++) {
                byte[] value = new byte[nUserParamLength(i, j)];
                parameters.put(new Integer(nUserParam(i, j, value)), value);
            }
            object.setUserObject(parameters);
        }
    }

    private static String describe(int code) {
        switch (code) {
        case ERR_INVALID:
            return "invalid M3G data";
        case ERR_NO_INTERFACE:
            return "the M3G engine could not be initialised";
        case ERR_UNSUPPORTED:
            return "unsupported file format (M3G files only)";
        case ERR_IO:
        default:
            return "malformed or truncated M3G data";
        }
    }

    /**
     * Reads a resource stream to the end.
     *
     * available() is used only as a starting hint -- ResourceInputStream does
     * report the full remaining size, but nothing in the contract promises it,
     * so the buffer grows if it turns out to be short.
     */
    private static byte[] readFully(InputStream in) throws IOException {
        int size = in.available();
        if (size <= 0) {
            size = 4096;
        }

        byte[] buffer = new byte[size];
        int used = 0;

        for (;;) {
            if (used == buffer.length) {
                byte[] bigger = new byte[buffer.length * 2];
                System.arraycopy(buffer, 0, bigger, 0, used);
                buffer = bigger;
            }
            int read = in.read(buffer, used, buffer.length - used);
            if (read < 0) {
                break;
            }
            used += read;
        }

        if (used != buffer.length) {
            byte[] exact = new byte[used];
            System.arraycopy(buffer, 0, exact, 0, used);
            buffer = exact;
        }
        return buffer;
    }

    /**
     * Parses one .m3g image.
     *
     * @return the number of root objects, or one of the ERR_* codes above
     */
    private static native int nLoadData(byte[] data, int offset, int length);

    /** Handle of root <code>index</code> of the pending result. */
    private static native int nResultHandle(int index);

    /** m3gcore class id of root <code>index</code> of the pending result. */
    private static native int nResultClass(int index);

    /** How many loaded objects carry user parameters. */
    private static native int nUserObjectCount();

    /** Handle of the object owning user-parameter set <code>object</code>. */
    private static native int nUserObjectHandle(int object);

    /** How many parameters that set holds. */
    private static native int nUserParamCount(int object);

    /** Length in bytes of one parameter. */
    private static native int nUserParamLength(int object, int index);

    /** Copies one parameter into <code>value</code>; returns its id. */
    private static native int nUserParam(int object, int index, byte[] value);

    /** Releases the pending result, keeping the objects alive. */
    private static native void nResultCommit();

    /** Releases the pending result and the objects with it. */
    private static native void nResultAbort();
}
