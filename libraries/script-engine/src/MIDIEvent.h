//
//  MIDIEvent.h
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2014-06-30.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MIDIEvent_h
#define hifi_MIDIEvent_h

#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class MIDIEvent {
public:
    double deltaTime;
    unsigned int type;
    unsigned int data1;
    unsigned int data2;
};

Q_DECLARE_METATYPE(MIDIEvent)

void registerMIDIMetaTypes(ScriptEngine* engine);

ScriptValuePointer midiEventToScriptValue(ScriptEngine* engine, const MIDIEvent& event);
void midiEventFromScriptValue(const ScriptValuePointer &object, MIDIEvent& event);

#endif // hifi_MIDIEvent_h