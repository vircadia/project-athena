//
//  TouchEvent.h
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TouchEvent_h
#define hifi_TouchEvent_h

#include <glm/glm.hpp>

#include <QVector>
#include <QTouchEvent>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class TouchEvent {
public:
    TouchEvent();
    TouchEvent(const QTouchEvent& event);
    TouchEvent(const QTouchEvent& event, const TouchEvent& other);
    
    static ScriptValuePointer toScriptValue(ScriptEngine* engine, const TouchEvent& event);
    static void fromScriptValue(const ScriptValuePointer& object, TouchEvent& event);
    
    float x;
    float y;
    bool isPressed;
    bool isMoved;
    bool isStationary;
    bool isReleased;
    bool isShifted;
    bool isControl;
    bool isMeta;
    bool isAlt;
    int touchPoints;
    QVector<glm::vec2> points;
    float radius;
    bool isPinching;
    bool isPinchOpening;
    
    // angles are in degrees
    QVector<float> angles; // angle from center to each point
    float angle; // the average of the angles
    float deltaAngle; // the change in average angle from last event
    bool isRotating;
    QString rotating;
    
private:
    void initWithQTouchEvent(const QTouchEvent& event);
    void calculateMetaAttributes(const TouchEvent& other);
};

Q_DECLARE_METATYPE(TouchEvent)

#endif // hifi_TouchEvent_h
