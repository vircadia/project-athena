//
//  TypedArrays.h
//
//
//  Created by Clement on 7/9/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TypedArrays_h
#define hifi_TypedArrays_h

#include "ArrayBufferViewClass.h"

static const QString BYTES_PER_ELEMENT_PROPERTY_NAME = "BYTES_PER_ELEMENT";
static const QString LENGTH_PROPERTY_NAME = "length";

static const QString INT_8_ARRAY_CLASS_NAME = "Int8Array";
static const QString UINT_8_ARRAY_CLASS_NAME = "Uint8Array";
static const QString UINT_8_CLAMPED_ARRAY_CLASS_NAME = "Uint8ClampedArray";
static const QString INT_16_ARRAY_CLASS_NAME = "Int16Array";
static const QString UINT_16_ARRAY_CLASS_NAME = "Uint16Array";
static const QString INT_32_ARRAY_CLASS_NAME = "Int32Array";
static const QString UINT_32_ARRAY_CLASS_NAME = "Uint32Array";
static const QString FLOAT_32_ARRAY_CLASS_NAME = "Float32Array";
static const QString FLOAT_64_ARRAY_CLASS_NAME = "Float64Array";

class TypedArray : public ArrayBufferViewClass {
    Q_OBJECT
public:
    TypedArray(ScriptEngineQtScript* scriptEngine, QString name);
    virtual QScriptValue newInstance(quint32 length);
    virtual QScriptValue newInstance(QScriptValue array);
    virtual QScriptValue newInstance(QScriptValue buffer, quint32 byteOffset, quint32 length);

    virtual QueryFlags queryProperty(const QScriptValue& object,
                                     const QScriptString& name,
                                     QueryFlags flags, uint* id) override;
    virtual QScriptValue property(const QScriptValue& object,
                                  const QScriptString& name, uint id) override;
    virtual void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override = 0;
    virtual QScriptValue::PropertyFlags propertyFlags(const QScriptValue& object,
                                                      const QScriptString& name, uint id) override;

    QString name() const override;
    QScriptValue prototype() const override;

protected:
    static QScriptValue construct(QScriptContext* context, QScriptEngine* engine);

    void setBytesPerElement(quint32 bytesPerElement);

    QScriptValue _proto;
    QScriptValue _ctor;

    QScriptString _name;
    QScriptString _bytesPerElementName;
    QScriptString _lengthName;

    quint32 _bytesPerElement;

    friend class TypedArrayPrototype;
};

class Int8ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Int8ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Uint8ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Uint8ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Uint8ClampedArrayClass : public TypedArray {
    Q_OBJECT
public:
    Uint8ClampedArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Int16ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Int16ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Uint16ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Uint16ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Int32ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Int32ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Uint32ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Uint32ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Float32ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Float32ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

class Float64ArrayClass : public TypedArray {
    Q_OBJECT
public:
    Float64ArrayClass(ScriptEngineQtScript* scriptEngine);

    QScriptValue property(const QScriptValue& object, const QScriptString& name, uint id) override;
    void setProperty(QScriptValue& object, const QScriptString& name, uint id, const QScriptValue& value) override;
};

#endif // hifi_TypedArrays_h
