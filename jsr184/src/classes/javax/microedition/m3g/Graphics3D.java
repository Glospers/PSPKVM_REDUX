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
 * Drawing goes through m3gcore, which on this platform renders with pspgl into
 * an offscreen buffer and reads the result back into MIDP's 16-bit screen
 * buffer -- the same pixels the lcdui Graphics draws into, so 3D composes
 * underneath any 2D the MIDlet paints afterwards, and PSPKVM's existing blit
 * puts the result on screen unchanged. The details are in
 * jsr184/src/native/m3g_graphics3d_kni.c and m3g/src/m3g_psp_render.c.
 *
 * The Java state below is still tracked in full, for two reasons: the getters
 * are part of the API and have to answer, and if the native side cannot bind a
 * target (no screen buffer, out of memory) the class degrades to the state
 * machine it used to be rather than throwing at a MIDlet that has no way to
 * recover.
 */
public final class Graphics3D {

    public static final int ANTIALIAS  = 2;
    public static final int DITHER     = 4;
    public static final int TRUE_COLOR = 8;
    public static final int OVERWRITE  = 16;

    /** Every hint the JSR-184 API defines; anything else is rejected. */
    private static final int VALID_HINTS = ANTIALIAS | DITHER | TRUE_COLOR | OVERWRITE;

    private static Graphics3D instance;

    private Object target;
    private boolean depthBufferEnabled = true;
    private int hints;

    /** True while the engine holds a rendering target, i.e. drawing works. */
    private boolean nativeBound;

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
        if ((hints & ~VALID_HINTS) != 0) {
            throw new IllegalArgumentException("unknown rendering hint");
        }

        this.target = target;
        this.depthBufferEnabled = depthBuffer;
        this.hints = hints;

        // The engine always draws into MIDP's screen buffer, whatever kind of
        // target the MIDlet named; the size it reports back is the size of
        // that buffer, which is also the default viewport.
        int size = nBind(target, hints, depthBuffer ? 1 : 0);
        if (size > 0) {
            nativeBound = true;
            // Binding is one of the two things that brings the renderer up, so
            // anything the MIDlet built before now has an engine object waiting
            // to be made. Do it before the camera and lights are pushed, since
            // those are exactly the objects in question.
            Object3D.flushDeferred();
            viewportWidth  = size >>> 16;
            viewportHeight = size & 0xFFFF;
            nSetViewport(0, 0, viewportWidth, viewportHeight);
            nSetDepthRange(0.0f, 1.0f);
            // The camera and the lights are Graphics3D state, not target
            // state: the specification lets a MIDlet set them once and bind a
            // target per frame, and several do. The engine, though, keeps them
            // on the rendering context, and m3gRenderNode refuses to draw
            // anything at all while that context has no camera
            // (m3gcore/src/m3g_rendercontext.c:1812). So push whatever was set
            // while unbound through now.
            applyCamera();
            applyLights();
        } else {
            nativeBound = false;
            viewportWidth  = 1;
            viewportHeight = 1;
        }
        viewportX = 0;
        viewportY = 0;
        depthRangeNear = 0.0f;
        depthRangeFar  = 1.0f;

        // Honour the Graphics clip so that 3D drawn into a clipped context
        // stays inside it. MIDP clip coordinates are relative to the current
        // translation; the target buffer is not.
        if (nativeBound && target instanceof javax.microedition.lcdui.Graphics) {
            javax.microedition.lcdui.Graphics g =
                (javax.microedition.lcdui.Graphics) target;
            int cw = g.getClipWidth();
            int ch = g.getClipHeight();
            if (cw > 0 && ch > 0) {
                nSetClipRect(g.getTranslateX() + g.getClipX(),
                             g.getTranslateY() + g.getClipY(), cw, ch);
            }
        }
    }

    public void releaseTarget() {
        if (nativeBound) {
            // The target goes back to the native side because that is where
            // the finished frame has to be written: the engine renders into a
            // staging buffer at a fixed address, not into the MIDlet's own
            // pixels, which may have moved while it was drawing. See nBind in
            // jsr184/src/native/m3g_graphics3d_kni.c.
            nRelease(target);
            nativeBound = false;
        }
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
        if (nativeBound) {
            nSetViewport(x, y, width, height);
        }
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
        if (nativeBound) {
            nSetDepthRange(near, far);
        }
    }

    public float getDepthRangeNear() { return depthRangeNear; }
    public float getDepthRangeFar()  { return depthRangeFar; }

    /** TEMPORARY -- budget for the clear-state diagnostic below. */
    private static int clearDiagLeft = 6;

    public void clear(Background background) {
        checkBound();
        if (nativeBound) {
            if (background != null) {
                background.reassert();
            }
            if (clearDiagLeft > 0) {
                clearDiagLeft--;
                /* -10: 0 none, 1 no image, 2 image deferred, 3 image live.
                 * -11: colour-clear flag in bit 0, colour's RGB above it. */
                Object3D.nDiag(-10, (background == null) ? 0
                        : ((background.getImage() == null) ? 1
                        : ((background.getImage().handle == 0) ? 2 : 3)));
                if (background != null) {
                    Object3D.nDiag(-11,
                            (background.isColorClearEnabled() ? 1 : 0)
                            | (background.getColor() & 0xFFFFFF00));
                }
            }
            nClear(background != null ? background.handle : 0);
        }
    }

    public void render(World world) {
        if (world == null) {
            throw new NullPointerException();
        }
        checkBound();
        if (nativeBound) {
            // The handle is enough: the engine holds the scene graph, the
            // active camera, the lights and the background internally, so
            // nothing has to be walked on the Java side.
            nRenderWorld(world.handle);
        }
    }

    public void render(Node node, Transform transform) {
        if (node == null) {
            throw new NullPointerException();
        }
        checkBound();
        if (nativeBound) {
            nRenderNode(node.handle,
                        transform != null ? transform.rows() : null);
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
        checkBound();
        if (nativeBound) {
            // Works for geometry that came out of Loader; geometry a MIDlet
            // builds with the public constructors has no engine object behind
            // it yet, and the native side traces that rather than dropping it
            // silently.
            nRenderImmediate(vertices.handle, triangles.handle,
                             appearance.handle,
                             transform != null ? transform.rows() : null,
                             scope);
        }
    }

    public void setCamera(Camera camera, Transform transform) {
        this.camera = camera;
        this.cameraTransform = (transform != null)
            ? new Transform(transform) : new Transform();
        applyCamera();
    }

    private void applyCamera() {
        if (nativeBound) {
            nSetCamera(camera != null ? camera.handle : 0,
                       camera != null ? cameraTransform.rows() : null);
        }
    }

    private void applyLights() {
        if (!nativeBound) {
            return;
        }
        nClearLights();
        for (int i = 0; i < lightCount; i++) {
            if (lights[i] != null) {
                nAddLight(lights[i].handle,
                          lightTransforms[i] != null
                              ? lightTransforms[i].rows() : null);
            }
        }
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
        if (nativeBound) {
            nAddLight(light.handle, lightTransforms[index].rows());
        }
        return index;
    }

    public void setLight(int index, Light light, Transform transform) {
        if (index < 0 || index >= lightCount) {
            throw new IndexOutOfBoundsException();
        }
        lights[index] = light;
        lightTransforms[index] = (transform != null)
            ? new Transform(transform) : new Transform();
        applyLights();
    }

    public void resetLights() {
        for (int i = 0; i < lightCount; i++) {
            lights[i] = null;
            lightTransforms[i] = null;
        }
        lightCount = 0;
        if (nativeBound) {
            nClearLights();
        }
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
        p.put("supportPerspectiveCorrection", Boolean.TRUE);
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

    private void checkBound() {
        if (target == null) {
            throw new IllegalStateException("no target bound");
        }
    }

    /*
     * Natives. Statically bound by name -- the romizer writes the mangled
     * symbol straight into ROMImage.cpp, so the C side in
     * jsr184/src/native/m3g_graphics3d_kni.c only has to spell these the same
     * way. They are static so that KNI parameter index 1 is the first
     * argument, matching the Loader natives.
     */

    /** @return (width &lt;&lt; 16) | height, or a negative failure code. */
    private static native int nBind(Object target, int hints, int depthBuffer);
    private static native int nRelease(Object target);
    private static native void nSetViewport(int x, int y, int width, int height);
    private static native void nSetClipRect(int x, int y, int width, int height);
    private static native void nSetDepthRange(float near, float far);
    private static native int nClear(int backgroundHandle);
    private static native int nRenderWorld(int worldHandle);
    private static native int nRenderNode(int nodeHandle, float[] transform);
    private static native int nRenderImmediate(int vertices, int triangles,
                                               int appearance,
                                               float[] transform, int scope);
    private static native int nSetCamera(int cameraHandle, float[] transform);
    private static native int nAddLight(int lightHandle, float[] transform);
    private static native void nClearLights();
}
