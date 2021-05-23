//
//  ArrayBufferViewClass.cpp
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ArrayBufferViewClass.h"
#include "ScriptEngineQtScript.h"

Q_DECLARE_METATYPE(QByteArray*)

ArrayBufferViewClass::ArrayBufferViewClass(ScriptEngineQtScript* scriptEngine) :
    QObject(scriptEngine),
QScriptClass(scriptEngine),
_scriptEngine(scriptEngine) {
    // Save string handles for quick lookup
    _bufferName = engine()->toStringHandle(BUFFER_PROPERTY_NAME.toLatin1());
    _byteOffsetName = engine()->toStringHandle(BYTE_OFFSET_PROPERTY_NAME.toLatin1());
    _byteLengthName = engine()->toStringHandle(BYTE_LENGTH_PROPERTY_NAME.toLatin1());
}

QScriptClass::QueryFlags ArrayBufferViewClass::queryProperty(const QScriptValue& object,
                                                             const QScriptString& name,
                                                             QueryFlags flags, uint* id) {
    if (name == _bufferName || name == _byteOffsetName || name == _byteLengthName) {
        return flags &= HandlesReadAccess; // Only keep read access flags
    }
    return 0; // No access
}

QScriptValue ArrayBufferViewClass::property(const QScriptValue& object,
                                            const QScriptString& name, uint id) {
    if (name == _bufferName) {
        return object.data().property(_bufferName);
    }
    if (name == _byteOffsetName) {
        return object.data().property(_byteOffsetName);
    }
    if (name == _byteLengthName) {
        return object.data().property(_byteLengthName);
    }
    return QScriptValue();
}

QScriptValue::PropertyFlags ArrayBufferViewClass::propertyFlags(const QScriptValue& object,
                                                                const QScriptString& name, uint id) {
    return QScriptValue::Undeletable;
}
