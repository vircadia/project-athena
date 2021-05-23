//
//  MouseEvent.h
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MouseEvent_h
#define hifi_MouseEvent_h

#include <QMouseEvent>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class MouseEvent {
public:
    MouseEvent();
    MouseEvent(const QMouseEvent& event);
    
    static ScriptValuePointer toScriptValue(ScriptEngine* engine, const MouseEvent& event);
    static void fromScriptValue(const ScriptValuePointer& object, MouseEvent& event);

    ScriptValuePointer toScriptValue(ScriptEngine* engine) const { return MouseEvent::toScriptValue(engine, *this); }
    
    int x;
    int y;
    QString button;
    bool isLeftButton;
    bool isRightButton;
    bool isMiddleButton;
    bool isShifted;
    bool isControl;
    bool isMeta;
    bool isAlt;
};

Q_DECLARE_METATYPE(MouseEvent)

#endif // hifi_MouseEvent_h
