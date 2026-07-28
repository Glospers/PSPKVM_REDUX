/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;

import java.util.Hashtable;

/**
 * The singleton 3D graphics context.
 *
 * Phase 1: state is tracked faithfully (target, viewport, camera, lights) but
 * nothing is drawn yet, so a MIDlet that renders sees an empty viewport rather
 * than failing to load. The render paths are where the m3gcore backend gets
 * attached in the next phase.
 */
public final class Graphics3D {

    public static final int ANTIALIAS  = 2;
    public static final int DITHER     = 4;
    public static final int TRUE_COLOR = 8;
    public static final int OVERWRITE  = 16;

    private static Graphics3D instance;

    private Object target;
    private boolean depthBufferEnabled = true;
    private int hints;

    private int viewportX, viewportY;
    private int viewportWidth  = 1;
    private int viewportHeight = 1;

    private float depthRangeNear = 0.0f;
    private float depthRangeFar  = 1.0f;

    private Camera camera;
    private Transform cameraTransform = new Transform();

    private static final int MAX_LIGHTS = 8;
    private Light[] lights = new Light[MAX_LIGHTS];
    private Transform[] lightTransforms = new Transform[MAX_LIGHTS];
    private int lightCount;

    private Graphics3D() {
    }

    public static Graphics3D getInstance() {
        if (instance == null) {
            instance = new Graphics3D();
        }
        return instance;
    }

    public void bindTarget(Object target) {
        bindTarget(target, true, 0);
    }

    public void bindTarget(Object target, boolean depthBuffer, int hints) {
        if (target == null) {
            throw new NullPointerException();
        }
        if (this.target != null) {
            throw new IllegalStateException("target already bound");
        }
        this.target = target;
        this.depthBufferEnabled = depthBuffer;
        this.hints = hints;

        if (target instanceof javax.microedition.lcdui.Graphics) {
            javax.microedition.lcdui.Graphics g =
                (javax.microedition.lcdui.Graphics) target;
            viewportX = 0;
            viewportY = 0;
            viewportWidth  = g.getClipWidth();
            viewportHeight = g.getClipHeight();
        }
        if (viewportWidth  <= 0) { viewportWidth  = 1; }
        if (viewportHeight <= 0) { viewportHeight = 1; }
    }

    public void releaseTarget() {
        target = null;
    }

    public Object getTarget() {
        return target;
    }

    public void setViewport(int x, int y, int width, int height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("viewport must be positive");
        }
        viewportX = x;
        viewportY = y;
        viewportWidth = width;
        viewportHeight = height;
    }

    public int getViewportX()      { return viewportX; }
    public int getViewportY()      { return viewportY; }
    public int getViewportWidth()  { return viewportWidth; }
    public int getViewportHeight() { return viewportHeight; }

    public void setDepthRange(float near, float far) {
        if (near < 0.0f || near > 1.0f || far < 0.0f || far > 1.0f) {
            throw new IllegalArgumentException("depth range must be in [0,1]");
        }
        depthRangeNear = near;
        depthRangeFar = far;
    }

    public float getDepthRangeNear() { return depthRangeNear; }
    public float getDepthRangeFar()  { return depthRangeFar; }

    public void clear(Background background) {
        // Nothing is rasterised yet.
    }

    public void render(World world) {
        if (world == null) {
            throw new NullPointerException();
        }
    }

    public void render(Node node, Transform transform) {
        if (node == null) {
            throw new NullPointerException();
        }
    }

    public void render(VertexBuffer vertices, IndexBuffer triangles,
                       Appearance appearance, Transform transform) {
        render(vertices, triangles, appearance, transform, -1);
    }

    public void render(VertexBuffer vertices, IndexBuffer triangles,
                       Appearance appearance, Transform transform, int scope) {
        if (vertices == null || triangles == null || appearance == null) {
            throw new NullPointerException();
        }
    }

    public void setCamera(Camera camera, Transform transform) {
        this.camera = camera;
        this.cameraTransform = (transform != null)
            ? new Transform(transform) : new Transform();
    }

    public Camera getCamera(Transform transform) {
        if (transform != null && camera != null) {
            transform.set(cameraTransform);
        }
        return camera;
    }

    public int addLight(Light light, Transform transform) {
        if (light == null) {
            throw new NullPointerException();
        }
        if (lightCount >= MAX_LIGHTS) {
            throw new IllegalStateException("too many lights");
        }
        int index = lightCount++;
        lights[index] = light;
        lightTransforms[index] = (transform != null)
            ? new Transform(transform) : new Transform();
        return index;
    }

    public void setLight(int index, Light light, Transform transform) {
        if (index < 0 || index >= lightCount) {
            throw new IndexOutOfBoundsException();
        }
        lights[index] = light;
        lightTransforms[index] = (transform != null)
            ? new Transform(transform) : new Transform();
    }

    public void resetLights() {
        for (int i = 0; i < lightCount; i++) {
            lights[i] = null;
            lightTransforms[i] = null;
        }
        lightCount = 0;
    }

    public int getLightCount() {
        return lightCount;
    }

    public Light getLight(int index, Transform transform) {
        if (index < 0 || index >= lightCount) {
            throw new IndexOutOfBoundsException();
        }
        if (transform != null && lightTransforms[index] != null) {
            transform.set(lightTransforms[index]);
        }
        return lights[index];
    }

    public int getHints() {
        return hints;
    }

    public boolean isDepthBufferEnabled() {
        return depthBufferEnabled;
    }

    public static Hashtable getProperties() {
        Hashtable p = new Hashtable();
        p.put("supportAntialiasing",       Boolean.FALSE);
        p.put("supportTrueColor",          Boolean.FALSE);
        p.put("supportDithering",          Boolean.FALSE);
        p.put("supportMipmapping",         Boolean.FALSE);
        p.put("supportPerspectiveCorrection", Boolean.FALSE);
        p.put("supportLocalCameraLighting",   Boolean.FALSE);
        p.put("maxLights",                 new Integer(MAX_LIGHTS));
        p.put("maxViewportWidth",          new Integer(1024));
        p.put("maxViewportHeight",         new Integer(1024));
        p.put("maxViewportDimension",      new Integer(1024));
        p.put("maxTextureDimension",       new Integer(256));
        p.put("maxSpriteCropDimension",    new Integer(256));
        p.put("maxTransformsPerVertex",    new Integer(2));
        p.put("numTextureUnits",           new Integer(1));
        return p;
    }
}
