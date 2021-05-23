//
//  ScriptContext.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/1/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptContext_h
#define hifi_ScriptContext_h

#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

class ScriptContext;
class ScriptEngine;
class ScriptFunctionContext;
class ScriptValue;
using ScriptContextPointer = QSharedPointer<ScriptContext>;
using ScriptFunctionContextPointer = QSharedPointer<ScriptFunctionContext>;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class ScriptFunctionContext {
public:
    enum FunctionType {
        ScriptFunction = 0,
        QtFunction = 1,
        QtPropertyFunction = 2,
        NativeFunction = 3,
    };
    
public:
    virtual QString fileName() const = 0;
    virtual QString functionName() const = 0;
    virtual FunctionType functionType() const = 0;
    virtual int lineNumber() const = 0;
};

class ScriptContext {
public:
    virtual int argumentCount() const = 0;
    virtual ScriptValuePointer argument(int index) const = 0;
    virtual QStringList backtrace() const = 0;
    virtual ScriptValuePointer callee() const = 0;
    virtual ScriptEnginePointer engine() const = 0;
    virtual ScriptFunctionContextPointer functionContext() const = 0;
    virtual ScriptContextPointer parentContext() const = 0;
    virtual ScriptValuePointer thisObject() const = 0;
    virtual ScriptValuePointer throwError(const QString& text) = 0;
    virtual ScriptValuePointer throwValue(const ScriptValuePointer& value) = 0;
};

#endif // hifi_ScriptContext_h