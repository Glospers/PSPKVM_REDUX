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
    }

    public void setActiveCamera(Camera camera) {
        if (camera == null) {
            throw new NullPointerException();
        }
        this.activeCamera = camera;
    }

    public Camera getActiveCamera() {
        return activeCamera;
    }

    public void setBackground(Background background) {
        this.background = background;
    }

    public Background getBackground() {
        return background;
    }
}
