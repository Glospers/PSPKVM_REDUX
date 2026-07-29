/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public abstract class Node extends Transformable {

    public static final int NONE   = 144;
    public static final int ORIGIN = 145;
    public static final int X_AXIS = 146;
    public static final int Y_AXIS = 147;
    public static final int Z_AXIS = 148;

    /* m3gEnable / m3gIsEnabled selectors, m3g/inc/M3G/m3g_core.h:664-665. */
    private static final int ENABLE_RENDERING = 0;
    private static final int ENABLE_PICKING   = 1;

    private boolean renderingEnabled = true;
    private boolean pickingEnabled   = true;
    private float alphaFactor = 1.0f;
    private int scope = -1;
    Node parent;

    Node() {
    }

    public Node getParent() {
        if (handle != 0) {
            return (Node) Object3D.wrap(nGetParent(handle));
        }
        return parent;
    }

    public void setRenderingEnable(boolean enable) {
        renderingEnabled = enable;
        if (handle != 0) {
            nEnable(handle, ENABLE_RENDERING, enable ? 1 : 0);
        }
    }

    public boolean isRenderingEnabled() {
        return renderingEnabled;
    }

    public void setPickingEnable(boolean enable) {
        pickingEnabled = enable;
        if (handle != 0) {
            nEnable(handle, ENABLE_PICKING, enable ? 1 : 0);
        }
    }

    public boolean isPickingEnabled() {
        return pickingEnabled;
    }

    public void setAlphaFactor(float alphaFactor) {
        if (alphaFactor < 0.0f || alphaFactor > 1.0f) {
            throw new IllegalArgumentException("alphaFactor must be in [0,1]");
        }
        this.alphaFactor = alphaFactor;
        if (handle != 0) {
            nSetAlphaFactor(handle, alphaFactor);
        }
    }

    public float getAlphaFactor() {
        return alphaFactor;
    }

    public void setScope(int scope) {
        this.scope = scope;
        if (handle != 0) {
            nSetScope(handle, scope);
        }
    }

    public int getScope() {
        return scope;
    }

    public void align(Node reference) {
        if (handle != 0) {
            nAlign(handle, (reference != null) ? reference.handle : 0);
        }
    }

    public void setAlignment(Node zRef, int zTarget, Node yRef, int yTarget) {
        if (handle != 0) {
            nSetAlignment(handle,
                          (zRef != null) ? zRef.handle : 0, zTarget,
                          (yRef != null) ? yRef.handle : 0, yTarget);
        }
    }

    public Node getAlignmentReference(int axis) {
        return null;
    }

    public int getAlignmentTarget(int axis) {
        return NONE;
    }

    public boolean getTransformTo(Node target, Transform transform) {
        if (target == null || transform == null) {
            throw new NullPointerException();
        }
        if (handle != 0 && target.handle != 0) {
            return nGetTransformTo(handle, target.handle, transform.rows());
        }
        transform.setIdentity();
        return true;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native void nSetAlphaFactor(int handle, float alphaFactor);
    private static native void nEnable(int handle, int which, int enable);
    private static native void nSetScope(int handle, int scope);
    private static native boolean nGetTransformTo(int handle, int target,
                                                  float[] matrix);
    private static native void nSetAlignment(int handle, int zRef, int zTarget,
                                             int yRef, int yTarget);
    private static native void nAlign(int handle, int reference);
    private static native int nGetParent(int handle);
}
