//
//  DataViewClass.h
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DataViewClass_h
#define hifi_DataViewClass_h

#include "ArrayBufferViewClass.h"

class DataViewClass : public ArrayBufferViewClass {
    Q_OBJECT
public:
    DataViewClass(ScriptEngineQtScript* scriptEngine);
    QScriptValue newInstance(QScriptValue buffer, quint32 byteOffset, quint32 byteLength);

    QString name() const override;
    QScriptValue prototype() const override;

private:
    static QScriptValue construct(QScriptContext* context, QScriptEngine* engine);

    QScriptValue _proto;
    QScriptValue _ctor;

    QScriptString _name;
};


#endif // hifi_DataViewClass_h
