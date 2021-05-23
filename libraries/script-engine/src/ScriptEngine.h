//
//  ScriptEngine.h
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 12/14/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptEngine_h
#define hifi_ScriptEngine_h

#include <QtCore/QFlags>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

class QByteArray;
class QLatin1String;
class QString;
class QThread;
class QVariant;
class ScriptContext;
class ScriptEngine;
class ScriptManager;
class ScriptProgram;
class ScriptValue;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptProgramPointer = QSharedPointer<ScriptProgram>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

Q_DECLARE_METATYPE(ScriptEnginePointer)

class ScriptEngine {
public:
    typedef ScriptValuePointer (*FunctionSignature)(ScriptContext*, ScriptEngine*);
    typedef ScriptValuePointer (*MarshalFunction)(ScriptEngine*, const void*);
    typedef void (*DemarshalFunction)(const ScriptValuePointer&, void*);

    enum ValueOwnership {
        QtOwnership = 0,
        ScriptOwnership = 1,
        AutoOwnership = 2,
    };

    enum QObjectWrapOption {
        ExcludeChildObjects	= 0x0001,	// The script object will not expose child objects as properties.
        ExcludeSuperClassMethods = 0x0002,	// The script object will not expose signals and slots inherited from the superclass.
        ExcludeSuperClassProperties	= 0x0004,	// The script object will not expose properties inherited from the superclass.
        ExcludeSuperClassContents = 0x0006,	// Shorthand form for ExcludeSuperClassMethods | ExcludeSuperClassProperties
        ExcludeDeleteLater = 0x0010,	// The script object will not expose the QObject::deleteLater() slot.
        ExcludeSlots = 0x0020,	// The script object will not expose the QObject's slots.
        AutoCreateDynamicProperties = 0x0100,	// Properties that don't already exist in the QObject will be created as dynamic properties of that object, rather than as properties of the script object.
        PreferExistingWrapperObject = 0x0200,	// If a wrapper object with the requested configuration already exists, return that object.
        SkipMethodsInEnumeration = 0x0008,	// Don't include methods (signals and slots) when enumerating the object's properties.
    };
    Q_DECLARE_FLAGS(QObjectWrapOptions, QObjectWrapOption);

public:
    virtual void abortEvaluation() = 0;
    virtual void clearExceptions() = 0;
    virtual ScriptValuePointer cloneUncaughtException(const QString& detail = QString()) = 0;
    virtual ScriptContext* currentContext() const = 0;
    virtual ScriptValuePointer evaluate(const QString& program, const QString& fileName = QString()) = 0;
    virtual ScriptValuePointer evaluate(const ScriptProgramPointer &program) = 0;
    virtual ScriptValuePointer evaluateInClosure(const ScriptValuePointer& locals, const ScriptProgramPointer& program) = 0;
    virtual ScriptValuePointer globalObject() const = 0;
    virtual bool hasUncaughtException() const = 0;
    virtual bool isEvaluating() const = 0;
    virtual ScriptValuePointer lintScript(const QString& sourceCode, const QString& fileName, const int lineNumber = 1) = 0;
    virtual ScriptValuePointer makeError(const ScriptValuePointer& other = ScriptValuePointer(), const QString& type = "Error") = 0;
    virtual ScriptManager* manager() const = 0;
    virtual bool maybeEmitUncaughtException(const QString& debugHint = QString()) = 0;
    virtual ScriptValuePointer newArray(uint length = 0) = 0;
    virtual ScriptValuePointer newArrayBuffer(const QByteArray& message) = 0;
    virtual ScriptValuePointer newFunction(FunctionSignature fun, int length = 0) = 0;
    virtual ScriptValuePointer newObject() = 0;
    virtual ScriptProgramPointer newProgram(const QString& sourceCode, const QString& fileName) = 0;
    virtual ScriptValuePointer newQObject(QObject *object, ValueOwnership ownership = QtOwnership, const QObjectWrapOptions &options = QObjectWrapOptions()) = 0;
    virtual ScriptValuePointer newValue(bool value) = 0;
    virtual ScriptValuePointer newValue(int value) = 0;
    virtual ScriptValuePointer newValue(uint value) = 0;
    virtual ScriptValuePointer newValue(double value) = 0;
    virtual ScriptValuePointer newValue(const QString& value) = 0;
    virtual ScriptValuePointer newValue(const QLatin1String& value) = 0;
    virtual ScriptValuePointer newValue(const char* value) = 0;
    virtual ScriptValuePointer newVariant(const QVariant& value) = 0;
    virtual ScriptValuePointer nullValue() = 0;
    virtual bool raiseException(const ScriptValuePointer& exception) = 0;
    virtual void registerEnum(const QString& enumName, QMetaEnum newEnum) = 0;
    virtual void registerFunction(const QString& name, FunctionSignature fun, int numArguments = -1) = 0;
    virtual void registerFunction(const QString& parent, const QString& name, FunctionSignature fun, int numArguments = -1) = 0;
    virtual void registerGetterSetter(const QString& name, FunctionSignature getter, FunctionSignature setter, const QString& parent = QString("")) = 0;
    virtual void registerGlobalObject(const QString& name, QObject* object) = 0;
    virtual void setDefaultPrototype(int metaTypeId, const ScriptValuePointer& prototype) = 0;
    virtual void setObjectName(const QString& name) = 0;
    virtual bool setProperty(const char* name, const QVariant& value) = 0;
    virtual void setProcessEventsInterval(int interval) = 0;
    virtual QThread* thread() const = 0;
    virtual ScriptValuePointer undefinedValue() = 0;
    virtual ScriptValuePointer uncaughtException() const = 0;
    virtual QStringList uncaughtExceptionBacktrace() const = 0;
    virtual int uncaughtExceptionLineNumber() const = 0;
    virtual void updateMemoryCost(const qint64& deltaSize) = 0;

public:
    // helper to detect and log warnings when other code invokes QScriptEngine/BaseScriptEngine in thread-unsafe ways
    bool IS_THREADSAFE_INVOCATION(const QString& method);

public:
    template <typename T>
    inline T fromScriptValue(const ScriptValuePointer& value) {
        return scriptvalue_cast<T>(value);
    }

    template <typename T>
    inline ScriptValuePointer toScriptValue(const T& value) {
        return scriptValueFromValue(this, value);
    }

public: // not for public use, but I don't like how Qt strings this along with private friend functions
    virtual ScriptValuePointer create(int type, const void* ptr) = 0;
    virtual bool convert(const ScriptValuePointer& value, int type, void* ptr) = 0;
    virtual void registerCustomType(int type, MarshalFunction mf, DemarshalFunction df, const ScriptValuePointer& prototype) = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ScriptEngine::QObjectWrapOptions)

ScriptEnginePointer newScriptEngine(ScriptManager* manager = nullptr);

// Standardized CPS callback helpers (see: http://fredkschott.com/post/2014/03/understanding-error-first-callbacks-in-node-js/)
// These two helpers allow async JS APIs that use a callback parameter to be more friendly to scripters by accepting thisObject
// context and adopting a consistent and intuitable callback signature:
//   function callback(err, result) { if (err) { ... } else { /* do stuff with result */ } }
//
// To use, first pass the user-specified callback args in the same order used with optionally-scoped  Qt signal connections:
//   auto handler = makeScopedHandlerObject(scopeOrCallback, optionalMethodOrName);
// And then invoke the scoped handler later per CPS conventions:
//   auto result = callScopedHandlerObject(handler, err, result);
ScriptValuePointer makeScopedHandlerObject(ScriptValuePointer scopeOrCallback, ScriptValuePointer methodOrName);
ScriptValuePointer callScopedHandlerObject(ScriptValuePointer handler, ScriptValuePointer err, ScriptValuePointer result);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Inline implementations
/*
QThread* ScriptEngine::thread() const {
    QObject* qobject = toQObject();
    if (qobject == nullptr) {
        return nullptr;
    }
    return qobject->thread();
}
*/

#endif  // hifi_ScriptEngine_h
