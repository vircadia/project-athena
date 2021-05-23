//
//  ScriptEngineCast.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/9/2021.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptEngineCast_h
#define hifi_ScriptEngineCast_h

// Object conversion helpers (copied from QScriptEngine)

#include "ScriptEngine.h"
#include "ScriptValue.h"

template <typename T>
inline ScriptValuePointer scriptValueFromValue(ScriptEngine* engine, const T& t) {
    if (!engine) {
        return ScriptValuePointer();
    }

    return engine->create(qMetaTypeId<T>(), &t);
}

template <>
inline ScriptValuePointer scriptValueFromValue<QVariant>(ScriptEngine* engine, const QVariant& v) {
    if (!engine) {
        return ScriptValuePointer();
    }

    return engine->create(v.userType(), v.data());
}

template <typename T>
T scriptvalue_cast(const ScriptValuePointer& value) {
    T t;
    const int id = qMetaTypeId<T>();

    auto engine = value->engine();
    if (engine && engine->convert(value, id, &t)) {
        return t;
    } else if (value->isVariant()) {
        return qvariant_cast<T>(value->toVariant());
    }

    return T();
}

template <>
inline QVariant scriptvalue_cast<QVariant>(const ScriptValuePointer& value) {
    return value->toVariant();
}

template <typename T>
int scriptRegisterMetaType(ScriptEngine* eng,
                           ScriptValuePointer (*toScriptValue)(ScriptEngine*, const T& t),
                           void (*fromScriptValue)(const ScriptValuePointer&, T& t),
                           const ScriptValuePointer& prototype = ScriptValuePointer(),
                           T* = 0) {
    const int id = qRegisterMetaType<T>();  // make sure it's registered
    eng->registerCustomType(id, reinterpret_cast<ScriptEngine::MarshalFunction>(toScriptValue),
                            reinterpret_cast<ScriptEngine::DemarshalFunction>(fromScriptValue), prototype);
    return id;
}

template <class Container>
ScriptValuePointer scriptValueFromSequence(ScriptEngine* eng, const Container& cont) {
    ScriptValuePointer a = eng->newArray();
    typename Container::const_iterator begin = cont.begin();
    typename Container::const_iterator end = cont.end();
    typename Container::const_iterator it;
    quint32 i;
    for (it = begin, i = 0; it != end; ++it, ++i) {
        a->setProperty(i, eng->toScriptValue(*it));
    }
    return a;
}

template <class Container>
void scriptValueToSequence(const ScriptValuePointer& value, Container& cont) {
    quint32 len = value->property(QLatin1String("length"))->toUInt32();
    for (quint32 i = 0; i < len; ++i) {
        ScriptValuePointer item = value->property(i);
        cont.push_back(scriptvalue_cast<typename Container::value_type>(item));
    }
}

template <typename T>
int scriptRegisterSequenceMetaType(ScriptEngine* engine,
                                   const ScriptValuePointer& prototype = ScriptValuePointer(),
                                   T* = 0) {
    return scriptRegisterMetaType<T>(engine, scriptValueFromSequence, scriptValueToSequence, prototype);
}

#endif // hifi_ScriptEngineCast_h
