//
//  WheelEvent.h
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WheelEvent_h
#define hifi_WheelEvent_h

#include <QString>
#include <QWheelEvent>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class WheelEvent {
public:
    WheelEvent();
    WheelEvent(const QWheelEvent& event);
    
    static ScriptValuePointer toScriptValue(ScriptEngine* engine, const WheelEvent& event);
    static void fromScriptValue(const ScriptValuePointer& object, WheelEvent& event);
    
    int x;
    int y;
    int delta;
    QString orientation;
    bool isLeftButton;
    bool isRightButton;
    bool isMiddleButton;
    bool isShifted;
    bool isControl;
    bool isMeta;
    bool isAlt;
};

Q_DECLARE_METATYPE(WheelEvent)

#endif // hifi_WheelEvent_h
