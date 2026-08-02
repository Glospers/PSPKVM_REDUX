/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
package javax.microedition.m3g;


public class AnimationController extends Object3D {

    private float speed = 1.0f;
    private float weight = 1.0f;
    private int activeIntervalStart, activeIntervalEnd;
    private float referenceSequenceTime;
    private int referenceWorldTime;

    public AnimationController() {
        construct();
    }

    void createDeferred() {
        handle = nCreate();
        register();
    }

    void applyDeferred() {
        /* Speed first, position last: the engine re-anchors its position
         * reference on every speed change, so pushing the position
         * afterwards leaves exactly the mirrored state. */
        nSetSpeed(handle, speed, referenceWorldTime);
        nSetPosition(handle, referenceSequenceTime, referenceWorldTime);
        nSetActiveInterval(handle, activeIntervalStart, activeIntervalEnd);
        nSetWeight(handle, weight);
    }

    public void setActiveInterval(int start, int end) {
        if (start > end) {
            throw new IllegalArgumentException("start must be <= end");
        }
        activeIntervalStart = start;
        activeIntervalEnd = end;
        if (handle != 0) {
            nSetActiveInterval(handle, start, end);
        }
    }

    public int getActiveIntervalStart() { return activeIntervalStart; }
    public int getActiveIntervalEnd()   { return activeIntervalEnd; }

    public void setSpeed(float speed, int worldTime) {
        this.speed = speed;
        this.referenceWorldTime = worldTime;
        if (handle != 0) {
            nSetSpeed(handle, speed, worldTime);
        }
    }

    public float getSpeed() {
        return speed;
    }

    public void setPosition(float sequenceTime, int worldTime) {
        this.referenceSequenceTime = sequenceTime;
        this.referenceWorldTime = worldTime;
        if (handle != 0) {
            nSetPosition(handle, sequenceTime, worldTime);
        }
    }

    public float getPosition(int worldTime) {
        return referenceSequenceTime
             + speed * (worldTime - referenceWorldTime);
    }

    public void setWeight(float weight) {
        if (weight < 0.0f) {
            throw new IllegalArgumentException("weight must be >= 0");
        }
        this.weight = weight;
        if (handle != 0) {
            nSetWeight(handle, weight);
        }
    }

    public float getWeight() {
        return weight;
    }

    public int getRefWorldTime() {
        return referenceWorldTime;
    }

    /* Natives; see jsr184/src/native/m3g_object_kni.c. */

    private static native int nCreate();
    private static native void nSetActiveInterval(int handle, int start,
                                                  int end);
    private static native void nSetSpeed(int handle, float speed,
                                         int worldTime);
    private static native void nSetPosition(int handle, float sequenceTime,
                                            int worldTime);
    private static native void nSetWeight(int handle, float weight);
}
