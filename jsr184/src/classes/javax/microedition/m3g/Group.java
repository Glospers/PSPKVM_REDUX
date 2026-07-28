/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class Group extends Node {

    private java.util.Vector children = new java.util.Vector();

    public Group() {
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
    }

    public void removeChild(Node child) {
        if (child == null) {
            return;
        }
        if (children.removeElement(child)) {
            child.parent = null;
        }
    }

    public int getChildCount() {
        return children.size();
    }

    public Node getChild(int index) {
        return (Node) children.elementAt(index);
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
}
