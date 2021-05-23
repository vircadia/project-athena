//
//  Scriptable.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/1/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Scriptable_h
#define hifi_Scriptable_h

#include <QtCore/QSharedPointer>

#include "ScriptContext.h"

class ScriptEngine;
class ScriptValue;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class Scriptable {
public:
    static inline ScriptEnginePointer engine();
    static ScriptContext* context();
    static inline ScriptValuePointer thisObject();
    static inline int argumentCount();
    static inline ScriptValuePointer argument(int index);

    static void setContext(ScriptContext* context);
};

ScriptEnginePointer Scriptable::engine() {
    ScriptContext* scriptContext = context();
    if (!scriptContext) return nullptr;
    return scriptContext->engine();
}

ScriptValuePointer Scriptable::thisObject() {
    ScriptContext* scriptContext = context();
    if (!scriptContext) return nullptr;
    return scriptContext->thisObject();
}

int Scriptable::argumentCount() {
    ScriptContext* scriptContext = context();
    if (!scriptContext) return 0;
    return scriptContext->argumentCount();
}

ScriptValuePointer Scriptable::argument(int index) {
    ScriptContext* scriptContext = context();
    if (!scriptContext) return ScriptValuePointer();
    return scriptContext->argument(index);
}

#endif // hifi_Scriptable_h