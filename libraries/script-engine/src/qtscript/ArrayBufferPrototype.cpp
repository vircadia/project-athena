//
//  ArrayBufferPrototype.cpp
//
//
//  Created by Clement on 7/3/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ArrayBufferPrototype.h"

#include <glm/glm.hpp>

#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtScript/QScriptEngine>

static const int QCOMPRESS_HEADER_POSITION = 0;
static const int QCOMPRESS_HEADER_SIZE = 4;

Q_DECLARE_METATYPE(QByteArray*)

ArrayBufferPrototype::ArrayBufferPrototype(QObject* parent) : QObject(parent) {
}

QByteArray ArrayBufferPrototype::slice(qint32 begin, qint32 end) const {
    QByteArray* ba = thisArrayBuffer();
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? ba->size() + begin : begin;
    end = (end < 0) ? ba->size() + end : end;
    
    // here we clamp the indices to fit the array
    begin = glm::clamp(begin, 0, (ba->size() - 1));
    end = glm::clamp(end, 0, (ba->size() - 1));
    
    return (end - begin > 0) ? ba->mid(begin, end - begin) : QByteArray();
}

QByteArray ArrayBufferPrototype::slice(qint32 begin) const {
    QByteArray* ba = thisArrayBuffer();
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? ba->size() + begin : begin;
    
    // here we clamp the indices to fit the array
    begin = glm::clamp(begin, 0, (ba->size() - 1));
    
    return ba->mid(begin, -1);
}

QByteArray ArrayBufferPrototype::compress() const {
    // Compresses the ArrayBuffer data in Zlib format.
    QByteArray* ba = thisArrayBuffer();

    QByteArray buffer = qCompress(*ba);
    buffer.remove(QCOMPRESS_HEADER_POSITION, QCOMPRESS_HEADER_SIZE);  // Remove Qt's custom header to make it proper Zlib.

    return buffer;
}

QByteArray ArrayBufferPrototype::recodeImage(const QString& sourceFormat, const QString& targetFormat, qint32 maxSize) const {
    // Recodes image data if sourceFormat and targetFormat are different.
    // Rescales image data if either dimension is greater than the specified maximum.
    QByteArray* ba = thisArrayBuffer();

    bool mustRecode = sourceFormat.toLower() != targetFormat.toLower();

    QImage image = QImage::fromData(*ba);
    if (image.width() > maxSize || image.height() > maxSize) {
        image = image.scaled(maxSize, maxSize, Qt::KeepAspectRatio);
        mustRecode = true;
    }

    if (mustRecode) {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        std::string str = targetFormat.toUpper().toStdString();
        const char* format = str.c_str();
        image.save(&buffer, format);
        return buffer.data();
    }
    
    return *ba;
}

QByteArray* ArrayBufferPrototype::thisArrayBuffer() const {
    return qscriptvalue_cast<QByteArray*>(thisObject().data());
}
