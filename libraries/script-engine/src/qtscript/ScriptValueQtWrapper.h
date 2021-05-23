//
//  ScriptValueQtWrapper.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/16/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptValueQtWrapper_h
#define hifi_ScriptValueQtWrapper_h

#include <QtCore/QPointer>
#include <QtScript/QScriptValue>

#include <utility>

#include "../ScriptValue.h"
#include "ScriptEngineQtScript.h"

class ScriptValueQtWrapper : public ScriptValue {
public: // construction
    inline ScriptValueQtWrapper(ScriptEngineQtScript* engine, const QScriptValue& value) :
        _engine(engine), _value(value) {}
    inline ScriptValueQtWrapper(ScriptEngineQtScript* engine, QScriptValue&& value) :
        _engine(engine), _value(std::move(value)) {}
    static ScriptValueQtWrapper* unwrap(ScriptValuePointer val);
    inline const QScriptValue& toQtValue() const { return _value; }

public: // ScriptValue implementation
    virtual ScriptValuePointer call(const ScriptValuePointer& thisObject = ScriptValuePointer(),
                                    const ScriptValueList& args = ScriptValueList());
    virtual ScriptValuePointer call(const ScriptValuePointer& thisObject, const ScriptValuePointer& arguments);
    virtual ScriptValuePointer construct(const ScriptValueList& args = ScriptValueList());
    virtual ScriptValuePointer construct(const ScriptValuePointer& arguments);
    virtual ScriptValuePointer data() const;
    virtual ScriptEnginePointer engine() const;
    virtual ScriptValueIteratorPointer newIterator();
    virtual ScriptValuePointer property(const QString& name, const ResolveFlags& mode = ResolvePrototype) const;
    virtual ScriptValuePointer property(quint32 arrayIndex, const ResolveFlags& mode = ResolvePrototype) const;
    virtual void setData(const ScriptValuePointer& val);
    virtual void setProperty(const QString& name,
                             const ScriptValuePointer& value,
                             const PropertyFlags& flags = KeepExistingFlags);
    virtual void setProperty(quint32 arrayIndex,
                             const ScriptValuePointer& value,
                             const PropertyFlags& flags = KeepExistingFlags);
    virtual void setPrototype(const ScriptValuePointer& prototype);
    virtual bool strictlyEquals(const ScriptValuePointer& other) const;

    virtual bool toBool() const;
    virtual qint32 toInt32() const;
    virtual double toInteger() const;
    virtual double toNumber() const;
    virtual QString toString() const;
    virtual quint16 toUInt16() const;
    virtual quint32 toUInt32() const;
    virtual QVariant toVariant() const;
    virtual QObject* toQObject() const;

protected:  // ScriptValue implementation
    virtual bool equalsInternal(const ScriptValuePointer& other) const;
    virtual bool isArrayInternal() const;
    virtual bool isBoolInternal() const;
    virtual bool isErrorInternal() const;
    virtual bool isFunctionInternal() const;
    virtual bool isNumberInternal() const;
    virtual bool isNullInternal() const;
    virtual bool isObjectInternal() const;
    virtual bool isStringInternal() const;
    virtual bool isUndefinedInternal() const;
    virtual bool isValidInternal() const;
    virtual bool isVariantInternal() const;

private: // helper functions
    QScriptValue fullUnwrap(const ScriptValuePointer& value) const;

private: // storage
    QPointer<ScriptEngineQtScript> _engine;
    QScriptValue _value;
};

#endif  // hifi_ScriptValueQtWrapper_h