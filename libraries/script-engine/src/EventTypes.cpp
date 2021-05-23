//
//  EventTypes.cpp
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 1/28/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EventTypes.h"

#include "KeyEvent.h"
#include "MouseEvent.h"
#include "PointerEvent.h"
#include "ScriptEngine.h"
#include "ScriptEngineCast.h"
#include "SpatialEvent.h"
#include "TouchEvent.h"
#include "WheelEvent.h"

void registerEventTypes(ScriptEngine* engine) {
    scriptRegisterMetaType(engine, KeyEvent::toScriptValue, KeyEvent::fromScriptValue);
    scriptRegisterMetaType(engine, MouseEvent::toScriptValue, MouseEvent::fromScriptValue);
    scriptRegisterMetaType(engine, PointerEvent::toScriptValue, PointerEvent::fromScriptValue);
    scriptRegisterMetaType(engine, TouchEvent::toScriptValue, TouchEvent::fromScriptValue);
    scriptRegisterMetaType(engine, WheelEvent::toScriptValue, WheelEvent::fromScriptValue);
    scriptRegisterMetaType(engine, SpatialEvent::toScriptValue, SpatialEvent::fromScriptValue);
}
