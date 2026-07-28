/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;

/**
 * Base class of every object that can appear in a 3D scene.
 *
 * Phase 1: the class hierarchy and public API are in place so that MIDlets
 * link and run; the rendering core is not wired up yet.
 */
public abstract class Object3D {

    private int userID;
    private Object userObject;
    private java.util.Vector animationTracks = new java.util.Vector();

    Object3D() {
    }

    public final Object3D duplicate() {
        return duplicateImpl();
    }

    /** Overridden by subclasses that carry state worth copying. */
    Object3D duplicateImpl() {
        return this;
    }

    public Object3D find(int userID) {
        return (this.userID == userID) ? this : null;
    }

    public int getReferences(Object3D[] references) {
        return 0;
    }

    public int getUserID() {
        return userID;
    }

    public void setUserID(int userID) {
        this.userID = userID;
    }

    public Object getUserObject() {
        return userObject;
    }

    public void setUserObject(Object userObject) {
        this.userObject = userObject;
    }

    public void addAnimationTrack(AnimationTrack animationTrack) {
        if (animationTrack == null) {
            throw new NullPointerException();
        }
        animationTracks.addElement(animationTrack);
    }

    public AnimationTrack getAnimationTrack(int index) {
        return (AnimationTrack) animationTracks.elementAt(index);
    }

    public int getAnimationTrackCount() {
        return animationTracks.size();
    }

    public void removeAnimationTrack(AnimationTrack animationTrack) {
        animationTracks.removeElement(animationTrack);
    }

    public int animate(int time) {
        return 0;
    }
}
