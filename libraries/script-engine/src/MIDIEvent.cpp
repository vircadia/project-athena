//
//  MIDIEvent.cpp
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2014-06-30.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MIDIEvent.h"

#include <QtCore/QVariant>

#include "ScriptEngine.h"
#include "ScriptEngineCast.h"
#include "ScriptValue.h"

void registerMIDIMetaTypes(ScriptEngine* engine) {
    scriptRegisterMetaType(engine, midiEventToScriptValue, midiEventFromScriptValue);
}

const QString MIDI_DELTA_TIME_PROP_NAME = "deltaTime";
const QString MIDI_EVENT_TYPE_PROP_NAME = "type";
const QString MIDI_DATA_1_PROP_NAME = "data1";
const QString MIDI_DATA_2_PROP_NAME = "data2";

ScriptValuePointer midiEventToScriptValue(ScriptEngine* engine, const MIDIEvent& event) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty(MIDI_DELTA_TIME_PROP_NAME, event.deltaTime);
    obj->setProperty(MIDI_EVENT_TYPE_PROP_NAME, event.type);
    obj->setProperty(MIDI_DATA_1_PROP_NAME, event.data1);
    obj->setProperty(MIDI_DATA_2_PROP_NAME, event.data2);
    return obj;
}

void midiEventFromScriptValue(const ScriptValuePointer &object, MIDIEvent& event) {
    event.deltaTime = object->property(MIDI_DELTA_TIME_PROP_NAME)->toVariant().toDouble();
    event.type = object->property(MIDI_EVENT_TYPE_PROP_NAME)->toVariant().toUInt();
    event.data1 = object->property(MIDI_DATA_1_PROP_NAME)->toVariant().toUInt();
    event.data2 = object->property(MIDI_DATA_2_PROP_NAME)->toVariant().toUInt();
}