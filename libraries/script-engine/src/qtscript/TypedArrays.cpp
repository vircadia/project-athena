//
//  TypedArrays.cpp
//
//
//  Created by Clement on 7/9/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TypedArrays.h"

#include <glm/glm.hpp>

#include <QtCore/QDataStream>

#include <SharedUtil.h>

#include "ArrayBufferClass.h"
#include "ScriptEngineQtScript.h"
#include "TypedArrayPrototype.h"

Q_DECLARE_METATYPE(QByteArray*)

TypedArray::TypedArray(ScriptEngineQtScript* scriptEngine, QString name) : ArrayBufferViewClass(scriptEngine) {
    _bytesPerElementName = engine()->toStringHandle(BYTES_PER_ELEMENT_PROPERTY_NAME.toLatin1());
    _lengthName = engine()->toStringHandle(LENGTH_PROPERTY_NAME.toLatin1());
    _name = engine()->toStringHandle(name.toLatin1());
    
    QScriptValue global = engine()->globalObject();
    
    // build prototype
    _proto = engine()->newQObject(new TypedArrayPrototype(this),
                                  QScriptEngine::QtOwnership,
                                  QScriptEngine::SkipMethodsInEnumeration |
                                  QScriptEngine::ExcludeSuperClassMethods |
                                  QScriptEngine::ExcludeSuperClassProperties);
    _proto.setPrototype(global.property("Object").property("prototype"));
    
    // Register constructor
    _ctor = engine()->newFunction(construct, _proto);
    _ctor.setData(engine()->toScriptValue(this));
    engine()->globalObject().setProperty(_name, _ctor);
}

QScriptValue TypedArray::newInstance(quint32 length) {
    ArrayBufferClass* array = getScriptEngine()->getArrayBufferClass();
    QScriptValue buffer = array->newInstance(length * _bytesPerElement);
    return newInstance(buffer, 0, length);
}

QScriptValue TypedArray::newInstance(QScriptValue array) {
    const QString ARRAY_LENGTH_HANDLE = "length";
    if (array.property(ARRAY_LENGTH_HANDLE).isValid()) {
        quint32 length = array.property(ARRAY_LENGTH_HANDLE).toInt32();
        QScriptValue newArray = newInstance(length);
        for (quint32 i = 0; i < length; ++i) {
            QScriptValue value = array.property(QString::number(i));
            setProperty(newArray, engine()->toStringHandle(QString::number(i)),
                        i * _bytesPerElement, (value.isNumber()) ? value : QScriptValue(0));
        }
        return newArray;
    }
    engine()->evaluate("throw \"ArgumentError: not an array\"");
    return QScriptValue();
}

QScriptValue TypedArray::newInstance(QScriptValue buffer, quint32 byteOffset, quint32 length) {
    QScriptValue data = engine()->newObject();
    data.setProperty(_bufferName, buffer);
    data.setProperty(_byteOffsetName, byteOffset);
    data.setProperty(_byteLengthName, length * _bytesPerElement);
    data.setProperty(_lengthName, length);
    
    return engine()->newObject(this, data);
}

QScriptValue TypedArray::construct(QScriptContext* context, QScriptEngine* engine) {
    TypedArray* cls = qscriptvalue_cast<TypedArray*>(context->callee().data());
    if (!cls) {
        return QScriptValue();
    }
    if (context->argumentCount() == 0) {
        return cls->newInstance(0);
    }
    
    QScriptValue newObject;
    QScriptValue bufferArg = context->argument(0);
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(bufferArg.data());
    
    // parse arguments
    if (arrayBuffer) {
        if (context->argumentCount() == 1) {
            // Case for entire ArrayBuffer
            if (arrayBuffer->size() % cls->_bytesPerElement != 0) {
                engine->evaluate("throw \"RangeError: byteLength is not a multiple of BYTES_PER_ELEMENT\"");
            }
            quint32 length = arrayBuffer->size() / cls->_bytesPerElement;
            newObject = cls->newInstance(bufferArg, 0, length);
        } else {
            QScriptValue byteOffsetArg = context->argument(1);
            if (!byteOffsetArg.isNumber()) {
                engine->evaluate("throw \"ArgumentError: 2nd arg is not a number\"");
                return QScriptValue();
            }
            if (byteOffsetArg.toInt32() < 0 || byteOffsetArg.toInt32() > arrayBuffer->size()) {
                engine->evaluate("throw \"RangeError: byteOffset out of range\"");
                return QScriptValue();
            }
            if (byteOffsetArg.toInt32() % cls->_bytesPerElement != 0) {
                engine->evaluate("throw \"RangeError: byteOffset not a multiple of BYTES_PER_ELEMENT\"");
            }
            quint32 byteOffset = byteOffsetArg.toInt32();
            
            if (context->argumentCount() == 2) {
                // case for end of ArrayBuffer
                if ((arrayBuffer->size() - byteOffset) % cls->_bytesPerElement != 0) {
                    engine->evaluate("throw \"RangeError: byteLength - byteOffset not a multiple of BYTES_PER_ELEMENT\"");
                }
                quint32 length = (arrayBuffer->size() - byteOffset) / cls->_bytesPerElement;
                newObject = cls->newInstance(bufferArg, byteOffset, length);
            } else {
                
                QScriptValue lengthArg = (context->argumentCount() > 2) ? context->argument(2) : QScriptValue();
                if (!lengthArg.isNumber()) {
                    engine->evaluate("throw \"ArgumentError: 3nd arg is not a number\"");
                    return QScriptValue();
                }
                if (lengthArg.toInt32() < 0 ||
                    byteOffsetArg.toInt32() + lengthArg.toInt32() * (qint32)(cls->_bytesPerElement) > arrayBuffer->size()) {
                    engine->evaluate("throw \"RangeError: byteLength out of range\"");
                    return QScriptValue();
                }
                quint32 length = lengthArg.toInt32();
                
                // case for well-defined range
                newObject = cls->newInstance(bufferArg, byteOffset, length);
            }
        }
    } else if (context->argument(0).isNumber()) {
        // case for new ArrayBuffer
        newObject = cls->newInstance(context->argument(0).toInt32());
    } else {
        newObject = cls->newInstance(bufferArg);
    }
    
    if (context->isCalledAsConstructor()) {
        // if called with the new keyword, replace this object
        context->setThisObject(newObject);
        return engine->undefinedValue();
    }
    
    return newObject;
}

QScriptClass::QueryFlags TypedArray::queryProperty(const QScriptValue& object,
                                                   const QScriptString& name,
                                                   QueryFlags flags, uint* id) {
    if (name == _bytesPerElementName || name == _lengthName) {
        return flags &= HandlesReadAccess; // Only keep read access flags
    }
    
    quint32 byteOffset = object.data().property(_byteOffsetName).toInt32();
    quint32 length = object.data().property(_lengthName).toInt32();
    bool ok = false;
    quint32 pos = name.toArrayIndex(&ok);
    
    // Check that name is a valid index and arrayBuffer exists
    if (ok && pos < length) {
        *id = byteOffset + pos * _bytesPerElement; // save pos to avoid recomputation
        return HandlesReadAccess | HandlesWriteAccess; // Read/Write access
    }
    
    return ArrayBufferViewClass::queryProperty(object, name, flags, id);
}

QScriptValue TypedArray::property(const QScriptValue& object,
                                      const QScriptString& name, uint id) {
    if (name == _bytesPerElementName) {
        return QScriptValue(_bytesPerElement);
    }
    if (name == _lengthName) {
        return object.data().property(_lengthName);
    }
    return ArrayBufferViewClass::property(object, name, id);
}

QScriptValue::PropertyFlags TypedArray::propertyFlags(const QScriptValue& object,
                                                      const QScriptString& name, uint id) {
    return QScriptValue::Undeletable;
}

QString TypedArray::name() const {
    return _name.toString();
}

QScriptValue TypedArray::prototype() const {
    return _proto;
}

void TypedArray::setBytesPerElement(quint32 bytesPerElement) {
    _bytesPerElement = bytesPerElement;
    _ctor.setProperty(_bytesPerElementName, _bytesPerElement);
}

// templated helper functions
// don't work for floats as they require single precision settings
template<class T>
QScriptValue propertyHelper(const QByteArray* arrayBuffer, const QScriptString& name, uint id) {
    bool ok = false;
    name.toArrayIndex(&ok);
    
    if (ok && arrayBuffer) {
        QDataStream stream(*arrayBuffer);
        stream.skipRawData(id);
        
        stream.setByteOrder(QDataStream::LittleEndian);
        T result;
        stream >> result;
        return result;
    }
    return QScriptValue();
}

template<class T>
void setPropertyHelper(QByteArray* arrayBuffer, const QScriptString& name, uint id, const QScriptValue& value) {
    if (arrayBuffer && value.isNumber()) {
        QDataStream stream(arrayBuffer, QIODevice::ReadWrite);
        stream.skipRawData(id);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        stream << (T)value.toNumber();
    }
}

Int8ArrayClass::Int8ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, INT_8_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(qint8));
}

QScriptValue Int8ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<qint8>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Int8ArrayClass::setProperty(QScriptValue &object, const QScriptString &name,
                                 uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<qint8>(ba, name, id, value);
}

Uint8ArrayClass::Uint8ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, UINT_8_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(quint8));
}

QScriptValue Uint8ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<quint8>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Uint8ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<quint8>(ba, name, id, value);
}

Uint8ClampedArrayClass::Uint8ClampedArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, UINT_8_CLAMPED_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(quint8));
}

QScriptValue Uint8ClampedArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<quint8>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Uint8ClampedArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    if (ba && value.isNumber()) {
        QDataStream stream(ba, QIODevice::ReadWrite);
        stream.skipRawData(id);
        if (value.toNumber() > 255) {
            stream << (quint8)255;
        } else if (value.toNumber() < 0) {
            stream << (quint8)0;
        } else {
            stream << (quint8)glm::clamp(qRound(value.toNumber()), 0, 255);
        }
    }
}

Int16ArrayClass::Int16ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, INT_16_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(qint16));
}

QScriptValue Int16ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<qint16>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Int16ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<qint16>(ba, name, id, value);
}

Uint16ArrayClass::Uint16ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, UINT_16_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(quint16));
}

QScriptValue Uint16ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<quint16>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Uint16ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<quint16>(ba, name, id, value);
}

Int32ArrayClass::Int32ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, INT_32_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(qint32));
}

QScriptValue Int32ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<qint32>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Int32ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<qint32>(ba, name, id, value);
}

Uint32ArrayClass::Uint32ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, UINT_32_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(quint32));
}

QScriptValue Uint32ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    QScriptValue result = propertyHelper<quint32>(arrayBuffer, name, id);
    return (result.isValid()) ? result : TypedArray::property(object, name, id);
}

void Uint32ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    setPropertyHelper<quint32>(ba, name, id, value);
}

Float32ArrayClass::Float32ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, FLOAT_32_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(float));
}

QScriptValue Float32ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());bool ok = false;
    name.toArrayIndex(&ok);
    
    if (ok && arrayBuffer) {
        QDataStream stream(*arrayBuffer);
        stream.skipRawData(id);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        
        float result;
        stream >> result;
        if (isNaN(result)) {
            return QScriptValue();
        }
        return result;
    }
    return TypedArray::property(object, name, id);
}

void Float32ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    if (ba && value.isNumber()) {
        QDataStream stream(ba, QIODevice::ReadWrite);
        stream.skipRawData(id);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        
        stream << (float)value.toNumber();
    }
}

Float64ArrayClass::Float64ArrayClass(ScriptEngineQtScript* scriptEngine) : TypedArray(scriptEngine, FLOAT_64_ARRAY_CLASS_NAME) {
    setBytesPerElement(sizeof(double));
}

QScriptValue Float64ArrayClass::property(const QScriptValue& object, const QScriptString& name, uint id) {
    QByteArray* arrayBuffer = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());bool ok = false;
    name.toArrayIndex(&ok);
    
    if (ok && arrayBuffer) {
        QDataStream stream(*arrayBuffer);
        stream.skipRawData(id);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        
        double result;
        stream >> result;
        if (isNaN(result)) {
            return QScriptValue();
        }
        return result;
    }
    return TypedArray::property(object, name, id);
}

void Float64ArrayClass::setProperty(QScriptValue& object, const QScriptString& name,
                                  uint id, const QScriptValue& value) {
    QByteArray* ba = qscriptvalue_cast<QByteArray*>(object.data().property(_bufferName).data());
    if (ba && value.isNumber()) {
        QDataStream stream(ba, QIODevice::ReadWrite);
        stream.skipRawData(id);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        
        stream << (double)value.toNumber();
    }
}

