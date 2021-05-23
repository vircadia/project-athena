//
//  DataViewClass.cpp
//
//
//  Created by Clement on 7/8/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DataViewClass.h"

#include "DataViewPrototype.h"

Q_DECLARE_METATYPE(QByteArray*)

static const QString DATA_VIEW_NAME = "DataView";

DataViewClass::DataViewClass(ScriptEngineQtScript* scriptEngine) : ArrayBufferViewClass(scriptEngine) {
    QScriptValue global = engine()->globalObject();
    
    // Save string handles for quick lookup
    _name = engine()->toStringHandle(DATA_VIEW_NAME.toLatin1());
    
    // build prototype
    _proto = engine()->newQObject(new DataViewPrototype(this),
                                QScriptEngine::QtOwnership,
                                QScriptEngine::SkipMethodsInEnumeration |
                                QScriptEngine::ExcludeSuperClassMethods |
                                QScriptEngine::ExcludeSuperClassProperties);
    _proto.setPrototype(global.property("Object").property("prototype"));
    
    // Register constructor
    _ctor = engine()->newFunction(construct, _proto);
    _ctor.setData(engine()->toScriptValue(this));
    engine()->globalObject().setProperty(name(), _ctor);
}

QScriptValue DataViewClass::newInstance(QScriptValue buffer, quint32 byteOffset, quint32 byteLentgh) {
    QScriptValue data = engine()->newObject();
    data.setProperty(_bufferName, buffer);
    data.setProperty(_byteOffsetName, byteOffset);
    data.setProperty(_byteLengthName, byteLentgh);
    
    return engine()->newObject(this, data);
}

QScriptValue DataViewClass::construct(QScriptContext* context, QScriptEngine* engine) {
    DataViewClass* cls = qscriptvalue_cast<DataViewClass*>(context->callee().data());
    if (!cls || context->argumentCount() < 1) {
        return QScriptValue();
    }
    
    QScriptValue bufferArg = context->argument(0);
    QScriptValue byteOffsetArg = (context->argumentCount() >= 2) ? context->argument(1) : QScriptValue();
    QScriptValue byteLengthArg = (context->argumentCount() >= 3) ? context->argument(2) : QScriptValue();
    
    QByteArray* arrayBuffer = (bufferArg.isValid()) ? qscriptvalue_cast<QByteArray*>(bufferArg.data()) :NULL;
    if (!arrayBuffer) {
        engine->evaluate("throw \"TypeError: 1st argument not a ArrayBuffer\"");
        return QScriptValue();
    }
    if (byteOffsetArg.isNumber() &&
        (byteOffsetArg.toInt32() < 0 ||
         byteOffsetArg.toInt32() > arrayBuffer->size())) {
            engine->evaluate("throw \"RangeError: byteOffset out of range\"");
            return QScriptValue();
        }
    if (byteLengthArg.isNumber() &&
        (byteLengthArg.toInt32() < 0 ||
         byteOffsetArg.toInt32() + byteLengthArg.toInt32() > arrayBuffer->size())) {
            engine->evaluate("throw \"RangeError: byteLength out of range\"");
            return QScriptValue();
        }
    quint32 byteOffset = (byteOffsetArg.isNumber()) ? byteOffsetArg.toInt32() : 0;
    quint32 byteLength = (byteLengthArg.isNumber()) ? byteLengthArg.toInt32() : arrayBuffer->size() - byteOffset;
    QScriptValue newObject = cls->newInstance(bufferArg, byteOffset, byteLength);
    
    if (context->isCalledAsConstructor()) {
        context->setThisObject(newObject);
        return engine->undefinedValue();
    }
    
    return newObject;
}

QString DataViewClass::name() const {
    return _name.toString();
}

QScriptValue DataViewClass::prototype() const {
    return _proto;
}
