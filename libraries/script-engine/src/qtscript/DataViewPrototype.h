//
//  DataViewPrototype.h
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DataViewPrototype_h
#define hifi_DataViewPrototype_h

#include <QtCore/QObject>
#include <QtScript/QScriptable>

class DataViewPrototype : public QObject, public QScriptable {
    Q_OBJECT
public:
    DataViewPrototype(QObject* parent = NULL);
    
public slots:
    // Gets the value of the given type at the specified byte offset
    // from the start of the view. There is no alignment constraint;
    // multi-byte values may be fetched from any offset.
    //
    // For multi-byte values, the optional littleEndian argument
    // indicates whether a big-endian or little-endian value should be
    // read. If false or undefined, a big-endian value is read.
    //
    // These methods raise an exception if they would read
    // beyond the end of the view.
    qint32 getInt8(qint32 byteOffset);
    quint32 getUint8(qint32 byteOffset);
    qint32 getInt16(qint32 byteOffset, bool littleEndian = false);
    quint32 getUint16(qint32 byteOffset, bool littleEndian = false);
    qint32 getInt32(qint32 byteOffset, bool littleEndian = false);
    quint32 getUint32(qint32 byteOffset, bool littleEndian = false);
    QScriptValue getFloat32(qint32 byteOffset, bool littleEndian = false);
    QScriptValue getFloat64(qint32 byteOffset, bool littleEndian = false);
    
    // Stores a value of the given type at the specified byte offset
    // from the start of the view. There is no alignment constraint;
    // multi-byte values may be stored at any offset.
    //
    // For multi-byte values, the optional littleEndian argument
    // indicates whether the value should be stored in big-endian or
    // little-endian byte order. If false or undefined, the value is
    // stored in big-endian byte order.
    //
    // These methods raise an exception if they would write
    // beyond the end of the view.
    void setInt8(qint32 byteOffset, qint32 value);
    void setUint8(qint32 byteOffset, quint32 value);
    void setInt16(qint32 byteOffset, qint32 value, bool littleEndian = false);
    void setUint16(qint32 byteOffset, quint32 value, bool littleEndian = false);
    void setInt32(qint32 byteOffset, qint32 value, bool littleEndian = false);
    void setUint32(qint32 byteOffset, quint32 value, bool littleEndian = false);
    void setFloat32(qint32 byteOffset, float value, bool littleEndian = false);
    void setFloat64(qint32 byteOffset, double value, bool littleEndian = false);
    
private:
    QByteArray* thisArrayBuffer() const;
    bool realOffset(qint32& offset, size_t size) const;
};

#endif // hifi_DataViewPrototype_h