//
//  ArrayBufferViewClass.h
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ArrayBufferViewClass_h
#define hifi_ArrayBufferViewClass_h

#include <QtScript/QScriptClass>
#include <QtCore/QObject>
#include <QtScript/QScriptClass>
#include <QtScript/QScriptContext>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptString>
#include <QtScript/QScriptValue>

class ScriptEngineQtScript;

static const QString BUFFER_PROPERTY_NAME = "buffer";
static const QString BYTE_OFFSET_PROPERTY_NAME = "byteOffset";
static const QString BYTE_LENGTH_PROPERTY_NAME = "byteLength";

class ArrayBufferViewClass : public QObject, public QScriptClass {
    Q_OBJECT
public:
    ArrayBufferViewClass(ScriptEngineQtScript* scriptEngine);

    ScriptEngineQtScript* getScriptEngine() { return _scriptEngine; }

    virtual QueryFlags queryProperty(const QScriptValue& object,
                                     const QScriptString& name,
                                     QueryFlags flags, uint* id) override;
    virtual QScriptValue property(const QScriptValue& object,
                                  const QScriptString& name, uint id) override;
    virtual QScriptValue::PropertyFlags propertyFlags(const QScriptValue& object,
                                                      const QScriptString& name, uint id) override;
protected:
    // JS Object attributes
    QScriptString _bufferName;
    QScriptString _byteOffsetName;
    QScriptString _byteLengthName;

    ScriptEngineQtScript* _scriptEngine;
};

#endif // hifi_ArrayBufferViewClass_h
