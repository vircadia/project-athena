//
//  ScriptValue.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 4/25/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptValue_h
#define hifi_ScriptValue_h

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

class QObject;
class ScriptEngine;
class ScriptValue;
class ScriptValueIterator;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;
using ScriptValueList = QList<ScriptValuePointer>;
using ScriptValueIteratorPointer = QSharedPointer<ScriptValueIterator>;

class ScriptValue {
public:
    enum ResolveFlag
    {
        ResolveLocal = 0,
        ResolvePrototype = 1,
    };
    using ResolveFlags = QFlags<ResolveFlag>;

    enum PropertyFlag
    {
        ReadOnly = 0x00000001,
        Undeletable = 0x00000002,
        SkipInEnumeration = 0x00000004,
        PropertyGetter = 0x00000008,
        PropertySetter = 0x00000010,
        KeepExistingFlags = 0x00000800,
    };
    Q_DECLARE_FLAGS(PropertyFlags, PropertyFlag);

public:
    virtual ScriptValuePointer call(const ScriptValuePointer& thisObject = ScriptValuePointer(),
                                    const ScriptValueList& args = ScriptValueList()) = 0;
    virtual ScriptValuePointer call(const ScriptValuePointer& thisObject, const ScriptValuePointer& arguments) = 0;
    virtual ScriptValuePointer construct(const ScriptValueList& args = ScriptValueList()) = 0;
    virtual ScriptValuePointer construct(const ScriptValuePointer& arguments) = 0;
    virtual ScriptValuePointer data() const = 0;
    virtual ScriptEnginePointer engine() const = 0;
    inline bool equals(const ScriptValuePointer& other) const;
    inline bool isArray() const;
    inline bool isBool() const;
    inline bool isError() const;
    inline bool isFunction() const;
    inline bool isNumber() const;
    inline bool isNull() const;
    inline bool isObject() const;
    inline bool isString() const;
    inline bool isUndefined() const;
    inline bool isValid() const;
    inline bool isVariant() const;
    virtual ScriptValueIteratorPointer newIterator() = 0;
    virtual ScriptValuePointer property(const QString& name, const ResolveFlags& mode = ResolvePrototype) const = 0;
    virtual ScriptValuePointer property(quint32 arrayIndex, const ResolveFlags& mode = ResolvePrototype) const = 0;
    virtual void setData(const ScriptValuePointer& val) = 0;
    virtual void setProperty(const QString& name,
                             const ScriptValuePointer& value,
                             const PropertyFlags& flags = KeepExistingFlags) = 0;
    virtual void setProperty(quint32 arrayIndex,
                             const ScriptValuePointer& value,
                             const PropertyFlags& flags = KeepExistingFlags) = 0;
    template<typename TYP> void setProperty(const QString& name, const TYP& value,
                             const PropertyFlags& flags = KeepExistingFlags);
    template<typename TYP> void setProperty(quint32 arrayIndex, const TYP& value,
                             const PropertyFlags& flags = KeepExistingFlags);
    virtual void setPrototype(const ScriptValuePointer& prototype) = 0;
    virtual bool strictlyEquals(const ScriptValuePointer& other) const = 0;

    virtual bool toBool() const = 0;
    virtual qint32 toInt32() const = 0;
    virtual double toInteger() const = 0;
    virtual double toNumber() const = 0;
    virtual QString toString() const = 0;
    virtual quint16 toUInt16() const = 0;
    virtual quint32 toUInt32() const = 0;
    virtual QVariant toVariant() const = 0;
    virtual QObject* toQObject() const = 0;

protected:
    virtual bool equalsInternal(const ScriptValuePointer& other) const = 0;
    virtual bool isArrayInternal() const = 0;
    virtual bool isBoolInternal() const = 0;
    virtual bool isErrorInternal() const = 0;
    virtual bool isFunctionInternal() const = 0;
    virtual bool isNumberInternal() const = 0;
    virtual bool isNullInternal() const = 0;
    virtual bool isObjectInternal() const = 0;
    virtual bool isStringInternal() const = 0;
    virtual bool isUndefinedInternal() const = 0;
    virtual bool isValidInternal() const = 0;
    virtual bool isVariantInternal() const = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ScriptValue::PropertyFlags)

bool ScriptValue::equals(const ScriptValuePointer& other) const {
    if (this == NULL || !other) {
        return (this == NULL) == !other;
    }
    return equalsInternal(other);
}

bool ScriptValue::isArray() const {
    assert(this != NULL);
    if (this == NULL) return false;
    return isArrayInternal();
}

bool ScriptValue::isBool() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isBoolInternal();
}

bool ScriptValue::isError() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isErrorInternal();
}

bool ScriptValue::isFunction() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isFunctionInternal();
}

bool ScriptValue::isNumber() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isNumberInternal();
}

bool ScriptValue::isNull() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isNullInternal();
}

bool ScriptValue::isObject() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isObjectInternal();
}

bool ScriptValue::isString() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isStringInternal();
}

bool ScriptValue::isUndefined() const {
    assert(this != NULL);
    if (this == NULL)
        return true;
    return isUndefinedInternal();
}

bool ScriptValue::isValid() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isValidInternal();
}

bool ScriptValue::isVariant() const {
    assert(this != NULL);
    if (this == NULL)
        return false;
    return isVariantInternal();
}

template <typename TYP>
void ScriptValue::setProperty(const QString& name, const TYP& value, const PropertyFlags& flags) {
    setProperty(name, engine()->newValue(value), flags);
}

template <typename TYP>
void ScriptValue::setProperty(quint32 arrayIndex, const TYP& value, const PropertyFlags& flags) {
    setProperty(arrayIndex, engine()->newValue(value), flags);
}

template <typename T>
T scriptvalue_cast(const ScriptValuePointer& value);

#endif // hifi_ScriptValue_h