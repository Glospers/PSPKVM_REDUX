/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Group extends Node {

    private java.util.Vector children = new java.util.Vector();

    public Group() {
        handle = nCreate();
        register();
    }

    /**
     * For subclasses that create an engine object of their own.
     *
     * World is a Group in Java but an M3GWorld in the engine, so it must not
     * inherit this class's m3gCreateGroup call and then leak the result.
     */
    Group(boolean createEngineObject) {
        if (createEngineObject) {
            handle = nCreate();
            register();
        }
    }

    public void addChild(Node child) {
        if (child == null) {
            throw new NullPointerException();
        }
        if (child == this) {
            throw new IllegalArgumentException("cannot add a node to itself");
        }
        children.addElement(child);
        child.parent = this;
        if (handle != 0 && child.handle != 0) {
            nAddChild(handle, child.handle);
        }
    }

    public void removeChild(Node child) {
        if (child == null) {
            return;
        }
        if (children.removeElement(child)) {
            child.parent = null;
        }
        if (handle != 0 && child.handle != 0) {
            nRemoveChild(handle, child.handle);
        }
    }

    /*
     * Read through to the engine rather than from the Vector above: the
     * children of a Group that came out of Loader exist only inside m3gcore,
     * and walking a loaded scene is the normal way a MIDlet uses one.
     */

    public int getChildCount() {
        return (handle != 0) ? nGetChildCount(handle) : children.size();
    }

    public Node getChild(int index) {
        if (handle == 0) {
            return (Node) children.elementAt(index);
        }
        if (index < 0 || index >= nGetChildCount(handle)) {
            throw new IndexOutOfBoundsException();
        }
        return (Node) Object3D.wrap(nGetChild(handle, index));
    }

    public boolean pick(int scope, float x, float y, Camera camera,
                        RayIntersection ri) {
        return false;
    }

    public boolean pick(int scope, float ox, float oy, float oz,
                        float dx, float dy, float dz, RayIntersection ri) {
        return false;
    }

    public Object3D find(int userID) {
        Object3D found = super.find(userID);
        if (found != null) {
            return found;
        }
        for (int i = 0; i < children.size(); i++) {
            found = ((Node) children.elementAt(i)).find(userID);
            if (found != null) {
                return found;
            }
        }
        return null;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nAddChild(int group, int child);
    private static native void nRemoveChild(int group, int child);
    private static native int nGetChildCount(int group);
    private static native int nGetChild(int group, int index);
}
