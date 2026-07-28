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

    private boolean renderingEnabled = true;
    private boolean pickingEnabled   = true;
    private float alphaFactor = 1.0f;
    private int scope = -1;
    Node parent;

    Node() {
    }

    public Node getParent() {
        return parent;
    }

    public void setRenderingEnable(boolean enable) {
        renderingEnabled = enable;
    }

    public boolean isRenderingEnabled() {
        return renderingEnabled;
    }

    public void setPickingEnable(boolean enable) {
        pickingEnabled = enable;
    }

    public boolean isPickingEnabled() {
        return pickingEnabled;
    }

    public void setAlphaFactor(float alphaFactor) {
        if (alphaFactor < 0.0f || alphaFactor > 1.0f) {
            throw new IllegalArgumentException("alphaFactor must be in [0,1]");
        }
        this.alphaFactor = alphaFactor;
    }

    public float getAlphaFactor() {
        return alphaFactor;
    }

    public void setScope(int scope) {
        this.scope = scope;
    }

    public int getScope() {
        return scope;
    }

    public void align(Node reference) {
    }

    public void setAlignment(Node zRef, int zTarget, Node yRef, int yTarget) {
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
        transform.setIdentity();
        return true;
    }
}
