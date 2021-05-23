//
//  WheelEvent.cpp
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WheelEvent.h"

#include "ScriptEngine.h"
#include "ScriptValue.h"

WheelEvent::WheelEvent() :
    x(0.0f),
    y(0.0f),
    delta(0.0f),
    orientation("UNKNOwN"),
    isLeftButton(false),
    isRightButton(false),
    isMiddleButton(false),
    isShifted(false),
    isControl(false),
    isMeta(false),
    isAlt(false)
{
    
}

WheelEvent::WheelEvent(const QWheelEvent& event) {
    x = event.x();
    y = event.y();
    
    delta = event.delta();
    if (event.orientation() == Qt::Horizontal) {
        orientation = "HORIZONTAL";
    } else {
        orientation = "VERTICAL";
    }
    
    // button pressed state
    isLeftButton = (event.buttons().testFlag(Qt::LeftButton));
    isRightButton = (event.buttons().testFlag(Qt::RightButton));
    isMiddleButton = (event.buttons().testFlag(Qt::MiddleButton));
    
    // keyboard modifiers
    isShifted = event.modifiers().testFlag(Qt::ShiftModifier);
    isMeta = event.modifiers().testFlag(Qt::MetaModifier);
    isControl = event.modifiers().testFlag(Qt::ControlModifier);
    isAlt = event.modifiers().testFlag(Qt::AltModifier);
}

/*@jsdoc
 * A mouse wheel event.
 * @typedef {object} WheelEvent
 * @property {number} x - Integer x-coordinate of the event on the Interface window or HMD HUD.
 * @property {number} y - Integer y-coordinate of the event on the Interface window or HMD HUD.
 * @property {number} delta - Integer number indicating the direction and speed to scroll: positive numbers to scroll up, and 
 *     negative numers to scroll down.
 * @property {string} orientation - The orientation of the wheel: <code>"VERTICAL"</code> for a typical mouse; 
 *     <code>"HORIZONTAL"</code> for a "horizontal" wheel.
 * @property {boolean} isLeftButton - <code>true</code> if the left button was pressed when the event was generated, otherwise 
 *     <code>false</code>.
 * @property {boolean} isMiddleButton - <code>true</code> if the middle button was pressed when the event was generated, 
 *     otherwise <code>false</code>.
 * @property {boolean} isRightButton - <code>true</code> if the right button was pressed when the event was generated, 
 *     otherwise <code>false</code>.
 * @property {boolean} isShifted - <code>true</code> if the Shift key was pressed when the event was generated, otherwise
 *     <code>false</code>.
 * @property {boolean} isMeta - <code>true</code> if the "meta" key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "meta" key is the Windows key; on OSX it is the Control (Splat) key.
 * @property {boolean} isControl - <code>true</code> if the "control" key was pressed when the event was generated, otherwise
 *     <code>false</code>. On Windows the "control" key is the Ctrl key; on OSX it is the Command key.
 * @property {boolean} isAlt - <code>true</code> if the Alt key was pressed when the event was generated, otherwise
 *     <code>false</code>.
 * @example <caption>Report the WheelEvent details for each wheel rotation.</caption>
 * Controller.wheelEvent.connect(function (event) {
 *     print(JSON.stringify(event));
 * });
 */
ScriptValuePointer WheelEvent::toScriptValue(ScriptEngine* engine, const WheelEvent& event) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("x", event.x);
    obj->setProperty("y", event.y);
    obj->setProperty("delta", event.delta);
    obj->setProperty("orientation", event.orientation);
    obj->setProperty("isLeftButton", event.isLeftButton);
    obj->setProperty("isRightButton", event.isRightButton);
    obj->setProperty("isMiddleButton", event.isMiddleButton);
    obj->setProperty("isShifted", event.isShifted);
    obj->setProperty("isMeta", event.isMeta);
    obj->setProperty("isControl", event.isControl);
    obj->setProperty("isAlt", event.isAlt);
    return obj;
}

void WheelEvent::fromScriptValue(const ScriptValuePointer& object, WheelEvent& event) {
    // nothing for now...
}
