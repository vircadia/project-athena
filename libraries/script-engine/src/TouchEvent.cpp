//
//  TouchEvent.cpp
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TouchEvent.h"

#include <NumericalConstants.h>

#include "RegisteredMetaTypes.h"
#include "ScriptEngine.h"
#include "ScriptValue.h"
#include "ScriptValueUtils.h"

TouchEvent::TouchEvent() :
    x(0.0f),
    y(0.0f),
    isPressed(false),
    isMoved(false),
    isStationary(false),
    isReleased(false),
    isShifted(false),
    isControl(false),
    isMeta(false),
    isAlt(false),
    touchPoints(0),
    points(),
    radius(0.0f),
    isPinching(false),
    isPinchOpening(false),
    angles(),
    angle(0.0f),
    deltaAngle(0.0f),
    isRotating(false),
    rotating("none")
{

}

TouchEvent::TouchEvent(const QTouchEvent& event) :
    // these values are not set by initWithQTouchEvent() because they only apply to comparing to other events
    isPinching(false),
    isPinchOpening(false),
    deltaAngle(0.0f),
    isRotating(false),
    rotating("none")
{
    initWithQTouchEvent(event);
}

TouchEvent::TouchEvent(const QTouchEvent& event, const TouchEvent& other) {
    initWithQTouchEvent(event);
    calculateMetaAttributes(other);
}

// returns the angle (in degrees) between two points (note: 0 degrees is 'east')
float angleBetweenPoints(const glm::vec2& a, const glm::vec2& b ) {
    glm::vec2 length = b - a;
    float angle = DEGREES_PER_RADIAN * std::atan2(length.y, length.x);
    if (angle < 0) {
        angle += 360.0f;
    };
    return angle;
}

void TouchEvent::initWithQTouchEvent(const QTouchEvent& event) {
    // convert the touch points into an average
    const QList<QTouchEvent::TouchPoint>& tPoints = event.touchPoints();
    float touchAvgX = 0.0f;
    float touchAvgY = 0.0f;
    touchPoints = tPoints.count();
    if (touchPoints > 1) {
        for (int i = 0; i < touchPoints; ++i) {
            touchAvgX += (float)tPoints[i].pos().x();
            touchAvgY += (float)tPoints[i].pos().y();

            // add it to our points vector
            glm::vec2 thisPoint(tPoints[i].pos().x(), tPoints[i].pos().y());
            points << thisPoint;
        }
        touchAvgX /= (float)(touchPoints);
        touchAvgY /= (float)(touchPoints);
    } else {
        // I'm not sure this should ever happen, why would Qt send us a touch event for only one point?
        // maybe this happens in the case of a multi-touch where all but the last finger is released?
        touchAvgX = tPoints[0].pos().x();
        touchAvgY = tPoints[0].pos().y();
    }
    x = touchAvgX;
    y = touchAvgY;

    // after calculating the center point (average touch point), determine the maximum radius
    // also calculate the rotation angle for each point
    float maxRadius = 0.0f;
    glm::vec2 center(x,y);
    for (int i = 0; i < touchPoints; ++i) {
        glm::vec2 touchPoint(tPoints[i].pos().x(), tPoints[i].pos().y());
        float thisRadius = glm::distance(center,touchPoint);
        if (thisRadius > maxRadius) {
            maxRadius = thisRadius;
        }

        // calculate the angle for this point
        float thisAngle = angleBetweenPoints(center,touchPoint);
        angles << thisAngle;
    }
    radius = maxRadius;

    // after calculating the angles for each touch point, determine the average angle
    float totalAngle = 0.0f;
    for (int i = 0; i < touchPoints; ++i) {
        totalAngle += angles[i];
    }
    angle = totalAngle/(float)touchPoints;

    isPressed = event.touchPointStates().testFlag(Qt::TouchPointPressed);
    isMoved = event.touchPointStates().testFlag(Qt::TouchPointMoved);
    isStationary = event.touchPointStates().testFlag(Qt::TouchPointStationary);
    isReleased = event.touchPointStates().testFlag(Qt::TouchPointReleased);

    // keyboard modifiers
    isShifted = event.modifiers().testFlag(Qt::ShiftModifier);
    isMeta = event.modifiers().testFlag(Qt::MetaModifier);
    isControl = event.modifiers().testFlag(Qt::ControlModifier);
    isAlt = event.modifiers().testFlag(Qt::AltModifier);
}

void TouchEvent::calculateMetaAttributes(const TouchEvent& other) {
    // calculate comparative event attributes...
    if (other.radius > radius) {
        isPinching = true;
        isPinchOpening = false;
    } else if (other.radius < radius) {
        isPinchOpening = true;
        isPinching = false;
    } else {
        isPinching = other.isPinching;
        isPinchOpening = other.isPinchOpening;
    }

    // determine if the points are rotating...
    // note: if the number of touch points change between events, then we don't consider ourselves to be rotating
    if (touchPoints == other.touchPoints) {
        deltaAngle = angle - other.angle;
        if (other.angle < angle) {
            isRotating = true;
            rotating = "clockwise";
        } else if (other.angle > angle) {
            isRotating = true;
            rotating = "counterClockwise";
        } else {
            isRotating = false;
            rotating = "none";
        }
    } else {
        deltaAngle = 0.0f;
        isRotating = false;
        rotating = "none";
    }
}

/*@jsdoc
 * A display or device touch event.
 * @typedef {object} TouchEvent
 * @property {number} x - Integer x-coordinate of the average position of the touch events.
 * @property {number} y - Integer y-coordinate of the average position of the touch events.
 * @property {boolean} isPressed - <code>true</code> if the touch point has just been pressed, otherwise <code>false</code>.
 * @property {boolean} isMoved - <code>true</code> if the touch point has moved, otherwise <code>false</code>.
 * @property {boolean} isStationary - <code>true</code> if the touch point has not moved, otherwise <code>false</code>.
 * @property {boolean} isReleased - <code>true</code> if the  touch point has just been released, otherwise <code>false</code>.
 * @property {boolean} isShifted - <code>true</code> if the Shift key was pressed when the event was generated, otherwise
 *     <code>false</code>.
 * @property {boolean} isMeta - <code>true</code> if the "meta" key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "meta" key is the Windows key; on OSX it is the Control (Splat) key.
 * @property {boolean} isControl - <code>true</code> if the "control" key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "control" key is the Ctrl key; on OSX it is the Command key.
 * @property {boolean} isAlt - <code>true</code> if the Alt key was pressed when the event was generated, otherwise
 *     <code>false</code>.
 * @property {number} touchPoints - Integer number of touch points.
 * @property {Vec2[]} points - The coordinates of the touch points.
 * @property {number} radius - The radius of a circle centered on their average position that encompasses the touch points.
 * @property {boolean} isPinching - <code>true</code> if the <code>radius</code> has reduced since the most recent touch event 
 *     with a different <code>radius</code> value, otherwise <code>false</code>.
 * @property {boolean} isPinchOpening - <code>true</code> if the <code>radius</code> has increased since the most recent touch 
 *     event with a different <code>radius</code> value, otherwise <code>false</code>.
 * @property {number} angle - An angle calculated from the touch points, in degrees.
 * @property {number} deltaAngle - The change in the <code>angle</code> value since the previous touch event, in degrees, if 
 *     the number of touch points is the same, otherwise <code>0.0</code>.
 * @property {number[]} angles - The angles of each touch point about the center of all the touch points, in degrees.
 * @property {boolean} isRotating - <code>true</code> if the <code>angle</code> of the touch event has changed since the 
 *     previous touch event and the number of touch points is the same, otherwise <code>false</code>.
 * @property {string} rotating - <code>"clockwise"</code> or <code>"counterClockwise"</code> if the <code>angle</code> of the 
 *     touch event has changed since the previous touch event and the number of touch points is the same, otherwise 
 *     <code>"none"</code>.
 *
 * @example <caption>Report the TouchEvent details when a touch event starts.</caption>
 * Controller.touchBeginEvent.connect(function (event) {
 *     print(JSON.stringify(event));
 * });
 */
ScriptValuePointer TouchEvent::toScriptValue(ScriptEngine* engine, const TouchEvent& event) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("x", event.x);
    obj->setProperty("y", event.y);
    obj->setProperty("isPressed", event.isPressed);
    obj->setProperty("isMoved", event.isMoved);
    obj->setProperty("isStationary", event.isStationary);
    obj->setProperty("isReleased", event.isReleased);
    obj->setProperty("isShifted", event.isShifted);
    obj->setProperty("isMeta", event.isMeta);
    obj->setProperty("isControl", event.isControl);
    obj->setProperty("isAlt", event.isAlt);
    obj->setProperty("touchPoints", event.touchPoints);

    ScriptValuePointer pointsObj = engine->newArray();
    int index = 0;
    foreach (glm::vec2 point, event.points) {
        ScriptValuePointer thisPoint = vec2ToScriptValue(engine, point);
        pointsObj->setProperty(index, thisPoint);
        index++;
    }
    obj->setProperty("points", pointsObj);
    obj->setProperty("radius", event.radius);
    obj->setProperty("isPinching", event.isPinching);
    obj->setProperty("isPinchOpening", event.isPinchOpening);

    obj->setProperty("angle", event.angle);
    obj->setProperty("deltaAngle", event.deltaAngle);
    ScriptValuePointer anglesObj = engine->newArray();
    index = 0;
    foreach (float angle, event.angles) {
        anglesObj->setProperty(index, angle);
        index++;
    }
    obj->setProperty("angles", anglesObj);

    obj->setProperty("isRotating", event.isRotating);
    obj->setProperty("rotating", event.rotating);
    return obj;
}

void TouchEvent::fromScriptValue(const ScriptValuePointer& object, TouchEvent& event) {
    // nothing for now...
}
