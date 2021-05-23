//
//  AnimVariant.h
//
//  Created by Anthony J. Thibault on 9/2/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimVariant_h
#define hifi_AnimVariant_h

#include <cassert>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <map>
#include <set>
#include <StreamUtils.h>
#include <GLMHelpers.h>
#include "AnimationLogging.h"
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class AnimVariant {
public:
    enum class Type {
        Bool = 0,
        Int,
        Float,
        Vec3,
        Quat,
        String,
        NumTypes
    };

    static const AnimVariant False;

    AnimVariant() : _type(Type::Bool) { memset(&_val, 0, sizeof(_val)); }
    explicit AnimVariant(bool value) : _type(Type::Bool) { _val.boolVal = value; }
    explicit AnimVariant(int value) : _type(Type::Int) { _val.intVal = value; }
    explicit AnimVariant(float value) : _type(Type::Float) { _val.floats[0] = value; }
    explicit AnimVariant(const glm::vec3& value) : _type(Type::Vec3) { *reinterpret_cast<glm::vec3*>(&_val) = value; }
    explicit AnimVariant(const glm::quat& value) : _type(Type::Quat) { *reinterpret_cast<glm::quat*>(&_val) = value; }
    explicit AnimVariant(const QString& value) : _type(Type::String) { _stringVal = value; }

    bool isBool() const { return _type == Type::Bool; }
    bool isInt() const { return _type == Type::Int; }
    bool isFloat() const { return _type == Type::Float; }
    bool isVec3() const { return _type == Type::Vec3; }
    bool isQuat() const { return _type == Type::Quat; }
    bool isString() const { return _type == Type::String; }
    Type getType() const { return _type; }

    void setBool(bool value) { assert(_type == Type::Bool); _val.boolVal = value; }
    void setInt(int value) { assert(_type == Type::Int); _val.intVal = value; }
    void setFloat(float value) { assert(_type == Type::Float); _val.floats[0] = value; }
    void setVec3(const glm::vec3& value) { assert(_type == Type::Vec3); *reinterpret_cast<glm::vec3*>(&_val) = value; }
    void setQuat(const glm::quat& value) { assert(_type == Type::Quat); *reinterpret_cast<glm::quat*>(&_val) = value; }
    void setString(const QString& value) { assert(_type == Type::String); _stringVal = value; }

    bool getBool() const {
        if (_type == Type::Bool) {
            return _val.boolVal;
        } else if (_type == Type::Int) {
            return _val.intVal != 0;
        } else {
            return false;
        }
    }
    int getInt() const {
        if (_type == Type::Int) {
            return _val.intVal;
        } else if (_type == Type::Float) {
            return (int)_val.floats[0];
        } else {
            return 0;
        }
    }
    float getFloat() const {
        if (_type == Type::Float) {
            return _val.floats[0];
        } else if (_type == Type::Int) {
            return (float)_val.intVal;
        } else {
            return 0.0f;
        }
    }
    const glm::vec3& getVec3() const {
        if (_type == Type::Vec3) {
            return *reinterpret_cast<const glm::vec3*>(&_val);
        } else {
            return Vectors::ZERO;
        }
    }
    const glm::quat& getQuat() const {
        if (_type == Type::Quat) {
            return *reinterpret_cast<const glm::quat*>(&_val);
        } else {
            return Quaternions::IDENTITY;
        }
    }
    const QString& getString() const {
        return _stringVal;
    }

protected:
    Type _type;
    QString _stringVal;
    union {
        bool boolVal;
        int intVal;
        float floats[4];
    } _val;
};

class AnimVariantMap {
public:

    bool lookup(const QString& key, bool defaultValue) const {
        // check triggers first, then map
        if (key.isEmpty()) {
            return defaultValue;
        } else if (_triggers.find(key) != _triggers.end()) {
            return true;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getBool() : defaultValue;
        }
    }

    int lookup(const QString& key, int defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getInt() : defaultValue;
        }
    }

    float lookup(const QString& key, float defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getFloat() : defaultValue;
        }
    }

    const glm::vec3& lookupRaw(const QString& key, const glm::vec3& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getVec3() : defaultValue;
        }
    }

    glm::vec3 lookupRigToGeometry(const QString& key, const glm::vec3& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? transformPoint(_rigToGeometryMat, iter->second.getVec3()) : defaultValue;
        }
    }

    glm::vec3 lookupRigToGeometryVector(const QString& key, const glm::vec3& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? transformVectorFast(_rigToGeometryMat, iter->second.getVec3()) : defaultValue;
        }
    }

    const glm::quat& lookupRaw(const QString& key, const glm::quat& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getQuat() : defaultValue;
        }
    }

    glm::quat lookupRigToGeometry(const QString& key, const glm::quat& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? _rigToGeometryRot * iter->second.getQuat() : defaultValue;
        }
    }

    const QString& lookup(const QString& key, const QString& defaultValue) const {
        if (key.isEmpty()) {
            return defaultValue;
        } else {
            auto iter = _map.find(key);
            return iter != _map.end() ? iter->second.getString() : defaultValue;
        }
    }

    void set(const QString& key, bool value) { _map[key] = AnimVariant(value); }
    void set(const QString& key, int value) { _map[key] = AnimVariant(value); }
    void set(const QString& key, float value) { _map[key] = AnimVariant(value); }
    void set(const QString& key, const glm::vec3& value) { _map[key] = AnimVariant(value); }
    void set(const QString& key, const glm::quat& value) { _map[key] = AnimVariant(value); }
    void set(const QString& key, const QString& value) { _map[key] = AnimVariant(value); }
    void unset(const QString& key) { _map.erase(key); }

    void setTrigger(const QString& key) { _map[key] = AnimVariant(true); }

    void setRigToGeometryTransform(const glm::mat4& rigToGeometry) {
        _rigToGeometryMat = rigToGeometry;
        _rigToGeometryRot = glmExtractRotation(rigToGeometry);
    }

    void clearMap() { _map.clear(); _triggers.clear(); }
    bool hasKey(const QString& key) const { return _map.find(key) != _map.end(); }

    const AnimVariant& get(const QString& key) const {
        auto iter = _map.find(key);
        if (iter != _map.end()) {
            return iter->second;
        } else {
            return AnimVariant::False;
        }
    }

    // Answer a Plain Old Javascript Object (for the given engine) all of our values set as properties.
    ScriptValuePointer animVariantMapToScriptValue(ScriptEngine* engine, const QStringList& names, bool useNames) const;
    // Side-effect us with the value of object's own properties. (No inherited properties.)
    void animVariantMapFromScriptValue(const ScriptValuePointer& object);
    void copyVariantsFrom(const AnimVariantMap& other);

    // For stat debugging.
    std::map<QString, QString> toDebugMap() const;

#ifndef NDEBUG
    void dump() const {
        qCDebug(animation) << "AnimVariantMap =";
        for (auto& pair : _map) {
            switch (pair.second.getType()) {
            case AnimVariant::Type::Bool:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getBool();
                break;
            case AnimVariant::Type::Int:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getInt();
                break;
            case AnimVariant::Type::Float:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getFloat();
                break;
            case AnimVariant::Type::Vec3:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getVec3();
                break;
            case AnimVariant::Type::Quat:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getQuat();
                break;
            case AnimVariant::Type::String:
                qCDebug(animation) << "    " << pair.first << "=" << pair.second.getString();
                break;
            default:
                assert(false);
            }
        }
    }
#endif

protected:
    std::map<QString, AnimVariant> _map;
    std::set<QString> _triggers;
    glm::mat4 _rigToGeometryMat;
    glm::quat _rigToGeometryRot;
};

typedef std::function<void(ScriptValuePointer)> AnimVariantResultHandler;
Q_DECLARE_METATYPE(AnimVariantResultHandler);
Q_DECLARE_METATYPE(AnimVariantMap)

#endif // hifi_AnimVariant_h
