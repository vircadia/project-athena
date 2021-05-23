//
//  DataViewPrototype.cpp
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DataViewPrototype.h"

#include <QDebug>
#include <QtCore/QDataStream>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

#include <glm/glm.hpp>

#include <SharedUtil.h>

#include "ArrayBufferViewClass.h"

Q_DECLARE_METATYPE(QByteArray*)

DataViewPrototype::DataViewPrototype(QObject* parent) : QObject(parent) {
}

QByteArray* DataViewPrototype::thisArrayBuffer() const {
    QScriptValue bufferObject = thisObject().data().property(BUFFER_PROPERTY_NAME);
    return qscriptvalue_cast<QByteArray*>(bufferObject.data());
}

bool DataViewPrototype::realOffset(qint32& offset, size_t size) const {
    if (offset < 0) {
        return false;
    }
    quint32 viewOffset = thisObject().data().property(BYTE_OFFSET_PROPERTY_NAME).toInt32();
    quint32 viewLength = thisObject().data().property(BYTE_LENGTH_PROPERTY_NAME).toInt32();
    offset += viewOffset;
    return (offset + size) <= viewOffset + viewLength;
}

qint32 DataViewPrototype::getInt8(qint32 byteOffset) {
    if (realOffset(byteOffset, sizeof(qint8))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        
        qint8 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

quint32 DataViewPrototype::getUint8(qint32 byteOffset) {
    if (realOffset(byteOffset, sizeof(quint8))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        
        quint8 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

qint32 DataViewPrototype::getInt16(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(qint16))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        qint16 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

quint32 DataViewPrototype::getUint16(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(quint16))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        quint16 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

qint32 DataViewPrototype::getInt32(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(qint32))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        qint32 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

quint32 DataViewPrototype::getUint32(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(quint32))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        quint32 result;
        stream >> result;
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return 0;
}

QScriptValue DataViewPrototype::getFloat32(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(float))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        
        float result;
        stream >> result;
        if (isNaN(result)) {
            return QScriptValue();
        }
        
        return QScriptValue(result);
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return QScriptValue();
}

QScriptValue DataViewPrototype::getFloat64(qint32 byteOffset, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(double))) {
        QDataStream stream(*thisArrayBuffer());
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        
        double result;
        stream >> result;
        if (isNaN(result)) {
            return QScriptValue();
        }
        
        return result;
    }
    thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    return QScriptValue();
}

void DataViewPrototype::setInt8(qint32 byteOffset, qint32 value) {
    if (realOffset(byteOffset, sizeof(qint8))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        
        stream << (qint8)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setUint8(qint32 byteOffset, quint32 value) {
    if (realOffset(byteOffset, sizeof(quint8))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        
        stream << (quint8)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setInt16(qint32 byteOffset, qint32 value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(qint16))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        stream << (qint16)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setUint16(qint32 byteOffset, quint32 value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(quint16))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        stream << (quint16)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setInt32(qint32 byteOffset, qint32 value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(qint32))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        stream << (qint32)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setUint32(qint32 byteOffset, quint32 value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(quint32))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        
        stream << (quint32)value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setFloat32(qint32 byteOffset, float value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(float))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        
        stream << value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}

void DataViewPrototype::setFloat64(qint32 byteOffset, double value, bool littleEndian) {
    if (realOffset(byteOffset, sizeof(double))) {
        QDataStream stream(thisArrayBuffer(), QIODevice::ReadWrite);
        stream.skipRawData(byteOffset);
        stream.setByteOrder((littleEndian) ? QDataStream::LittleEndian : QDataStream::BigEndian);
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        
        stream << value;
    } else {
        thisObject().engine()->evaluate("throw \"RangeError: byteOffset out of range\"");
    }
}


