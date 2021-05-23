//
//  TypedArrayPrototype.h
//
//
//  Created by Clement on 7/14/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TypedArrayPrototype_h
#define hifi_TypedArrayPrototype_h

#include <QtCore/QObject>
#include <QtScript/QScriptable>
#include <QtScript/QScriptValue>

class TypedArrayPrototype : public QObject, public QScriptable {
    Q_OBJECT
public:
    TypedArrayPrototype(QObject* parent = NULL);
    
public slots:
    void set(QScriptValue array, qint32 offset = 0);
    QScriptValue subarray(qint32 begin);
    QScriptValue subarray(qint32 begin, qint32 end);
    
    QScriptValue get(quint32 index);
    void set(quint32 index, QScriptValue& value);
private:
    QByteArray* thisArrayBuffer() const;
};

#endif // hifi_TypedArrayPrototype_h