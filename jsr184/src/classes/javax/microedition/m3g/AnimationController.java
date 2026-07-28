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
    }

    public void setActiveInterval(int start, int end) {
        if (start > end) {
            throw new IllegalArgumentException("start must be <= end");
        }
        activeIntervalStart = start;
        activeIntervalEnd = end;
    }

    public int getActiveIntervalStart() { return activeIntervalStart; }
    public int getActiveIntervalEnd()   { return activeIntervalEnd; }

    public void setSpeed(float speed, int worldTime) {
        this.speed = speed;
        this.referenceWorldTime = worldTime;
    }

    public float getSpeed() {
        return speed;
    }

    public void setPosition(float sequenceTime, int worldTime) {
        this.referenceSequenceTime = sequenceTime;
        this.referenceWorldTime = worldTime;
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
    }

    public float getWeight() {
        return weight;
    }

    public int getRefWorldTime() {
        return referenceWorldTime;
    }
}
