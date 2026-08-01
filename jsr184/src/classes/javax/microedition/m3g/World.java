/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class World extends Group {

    private Camera activeCamera;
    private Background background;

    public World() {
        /* Not a Group in the engine -- see Group(boolean). */
        super(false);
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
    }

    public void setActiveCamera(Camera camera) {
        if (camera == null) {
            throw new NullPointerException();
        }
        this.activeCamera = camera;
        if (handle != 0) {
            if (camera.handle == 0) {
                /* Deferred camera into a live world: forwarding zero would
                 * silently leave the file's camera active.  See
                 * Object3D.linkLater; the field check keeps only the last
                 * camera set before the replay. */
                final Camera fCamera = camera;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fCamera.handle != 0 && activeCamera == fCamera) {
                            nSetActiveCamera(handle, fCamera.handle);
                        }
                    }
                });
            }
            else {
                nSetActiveCamera(handle, camera.handle);
            }
        }
    }

    public Camera getActiveCamera() {
        if (handle != 0) {
            return (Camera) Object3D.wrap(nGetActiveCamera(handle));
        }
        return activeCamera;
    }

    public void setBackground(Background background) {
        this.background = background;
        if (handle != 0) {
            if (background != null && background.handle == 0) {
                final Background fBackground = background;
                Object3D.linkLater(new Runnable() {
                    public void run() {
                        if (fBackground.handle != 0
                                && World.this.background == fBackground) {
                            nSetBackground(handle, fBackground.handle);
                        }
                    }
                });
            }
            else {
                nSetBackground(handle,
                               (background != null) ? background.handle : 0);
            }
        }
    }

    public Background getBackground() {
        if (handle != 0) {
            return (Background) Object3D.wrap(nGetBackground(handle));
        }
        return background;
    }

    void applyDeferred() {
        super.applyDeferred();
        if (activeCamera != null && activeCamera.handle != 0) {
            nSetActiveCamera(handle, activeCamera.handle);
        }
        if (background != null && background.handle != 0) {
            nSetBackground(handle, background.handle);
        }
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetActiveCamera(int world, int camera);
    private static native void nSetBackground(int world, int background);
    private static native int nGetActiveCamera(int world);
    private static native int nGetBackground(int world);
}
