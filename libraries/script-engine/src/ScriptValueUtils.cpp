//
//  ScriptValueUtils.cpp
//  libraries/shared/src
//
//  Created by Anthony Thibault on 4/15/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Utilities for working with QtScriptValues
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptValueUtils.h"

#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtCore/QRect>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <AACube.h>
#include <shared/MiniPromises.h>
#include <RegisteredMetaTypes.h>

#include "ScriptEngine.h"
#include "ScriptEngineCast.h"
#include "ScriptValueIterator.h"

bool isListOfStrings(const ScriptValuePointer& arg) {
    if (!arg->isArray()) {
        return false;
    }

    auto lengthProperty = arg->property("length");
    if (!lengthProperty->isNumber()) {
        return false;
    }

    int length = lengthProperty->toInt32();
    for (int i = 0; i < length; i++) {
        if (!arg->property(i)->isString()) {
            return false;
        }
    }

    return true;
}

void registerMetaTypes(ScriptEngine* engine) {
    scriptRegisterMetaType(engine, vec2ToScriptValue, vec2FromScriptValue);
    scriptRegisterMetaType(engine, vec3ToScriptValue, vec3FromScriptValue);
    scriptRegisterMetaType(engine, u8vec3ToScriptValue, u8vec3FromScriptValue);
    scriptRegisterMetaType(engine, vec4toScriptValue, vec4FromScriptValue);
    scriptRegisterMetaType(engine, quatToScriptValue, quatFromScriptValue);
    scriptRegisterMetaType(engine, mat4toScriptValue, mat4FromScriptValue);

    scriptRegisterMetaType(engine, qVectorVec3ToScriptValue, qVectorVec3FromScriptValue);
    scriptRegisterMetaType(engine, qVectorQuatToScriptValue, qVectorQuatFromScriptValue);
    scriptRegisterMetaType(engine, qVectorBoolToScriptValue, qVectorBoolFromScriptValue);
    scriptRegisterMetaType(engine, qVectorFloatToScriptValue, qVectorFloatFromScriptValue);
    scriptRegisterMetaType(engine, qVectorIntToScriptValue, qVectorIntFromScriptValue);
    scriptRegisterMetaType(engine, qVectorQUuidToScriptValue, qVectorQUuidFromScriptValue);

    scriptRegisterMetaType(engine, qSizeFToScriptValue, qSizeFFromScriptValue);
    scriptRegisterMetaType(engine, qRectToScriptValue, qRectFromScriptValue);
    scriptRegisterMetaType(engine, qURLToScriptValue, qURLFromScriptValue);
    scriptRegisterMetaType(engine, qColorToScriptValue, qColorFromScriptValue);

    scriptRegisterMetaType(engine, pickRayToScriptValue, pickRayFromScriptValue);
    scriptRegisterMetaType(engine, collisionToScriptValue, collisionFromScriptValue);
    scriptRegisterMetaType(engine, quuidToScriptValue, quuidFromScriptValue);
    scriptRegisterMetaType(engine, aaCubeToScriptValue, aaCubeFromScriptValue);

    scriptRegisterMetaType(engine, stencilMaskModeToScriptValue, stencilMaskModeFromScriptValue);

    scriptRegisterMetaType(engine, promiseToScriptValue, promiseFromScriptValue);

    scriptRegisterSequenceMetaType<QVector<unsigned int>>(engine);
}

ScriptValuePointer vec2ToScriptValue(ScriptEngine* engine, const glm::vec2& vec2) {
    auto prototype = engine->globalObject()->property("__hifi_vec2__");
    if (!prototype->property("defined")->toBool()) {
        prototype = engine->evaluate(
            "__hifi_vec2__ = Object.defineProperties({}, { "
            "defined: { value: true },"
            "0: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "1: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "u: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "v: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } }"
            "})");
    }
    ScriptValuePointer value = engine->newObject();
    value->setProperty("x", vec2.x);
    value->setProperty("y", vec2.y);
    value->setPrototype(prototype);
    return value;
}

void vec2FromScriptValue(const ScriptValuePointer& object, glm::vec2& vec2) {
    if (object->isNumber()) {
        vec2 = glm::vec2(object->toVariant().toFloat());
    } else if (object->isArray()) {
        QVariantList list = object->toVariant().toList();
        if (list.length() == 2) {
            vec2.x = list[0].toFloat();
            vec2.y = list[1].toFloat();
        }
    } else {
        ScriptValuePointer x = object->property("x");
        if (!x->isValid()) {
            x = object->property("u");
        }

        ScriptValuePointer y = object->property("y");
        if (!y->isValid()) {
            y = object->property("v");
        }

        vec2.x = x->toVariant().toFloat();
        vec2.y = y->toVariant().toFloat();
    }
}

ScriptValuePointer vec3ToScriptValue(ScriptEngine* engine, const glm::vec3& vec3) {
    auto prototype = engine->globalObject()->property("__hifi_vec3__");
    if (!prototype->property("defined")->toBool()) {
        prototype = engine->evaluate(
            "__hifi_vec3__ = Object.defineProperties({}, { "
            "defined: { value: true },"
            "0: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "1: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "2: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } },"
            "r: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "g: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "b: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } },"
            "red: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "green: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "blue: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } }"
            "})");
    }
    ScriptValuePointer value = engine->newObject();
    value->setProperty("x", vec3.x);
    value->setProperty("y", vec3.y);
    value->setProperty("z", vec3.z);
    value->setPrototype(prototype);
    return value;
}

ScriptValuePointer vec3ColorToScriptValue(ScriptEngine* engine, const glm::vec3& vec3) {
    auto prototype = engine->globalObject()->property("__hifi_vec3_color__");
    if (!prototype->property("defined")->toBool()) {
        prototype = engine->evaluate(
            "__hifi_vec3_color__ = Object.defineProperties({}, { "
            "defined: { value: true },"
            "0: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "1: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "2: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } },"
            "r: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "g: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "b: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } },"
            "x: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "y: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "z: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } }"
            "})");
    }
    ScriptValuePointer value = engine->newObject();
    value->setProperty("red", vec3.x);
    value->setProperty("green", vec3.y);
    value->setProperty("blue", vec3.z);
    value->setPrototype(prototype);
    return value;
}

void vec3FromScriptValue(const ScriptValuePointer& object, glm::vec3& vec3) {
    if (object->isNumber()) {
        vec3 = glm::vec3(object->toVariant().toFloat());
    } else if (object->isString()) {
        QColor qColor(object->toString());
        if (qColor.isValid()) {
            vec3.x = qColor.red();
            vec3.y = qColor.green();
            vec3.z = qColor.blue();
        }
    } else if (object->isArray()) {
        QVariantList list = object->toVariant().toList();
        if (list.length() == 3) {
            vec3.x = list[0].toFloat();
            vec3.y = list[1].toFloat();
            vec3.z = list[2].toFloat();
        }
    } else {
        ScriptValuePointer x = object->property("x");
        if (!x->isValid()) {
            x = object->property("r");
        }
        if (!x->isValid()) {
            x = object->property("red");
        }

        ScriptValuePointer y = object->property("y");
        if (!y->isValid()) {
            y = object->property("g");
        }
        if (!y->isValid()) {
            y = object->property("green");
        }

        ScriptValuePointer z = object->property("z");
        if (!z->isValid()) {
            z = object->property("b");
        }
        if (!z->isValid()) {
            z = object->property("blue");
        }

        vec3.x = x->toVariant().toFloat();
        vec3.y = y->toVariant().toFloat();
        vec3.z = z->toVariant().toFloat();
    }
}

ScriptValuePointer u8vec3ToScriptValue(ScriptEngine* engine, const glm::u8vec3& vec3) {
    auto prototype = engine->globalObject()->property("__hifi_u8vec3__");
    if (!prototype->property("defined")->toBool()) {
        prototype = engine->evaluate(
            "__hifi_u8vec3__ = Object.defineProperties({}, { "
            "defined: { value: true },"
            "0: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "1: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "2: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } },"
            "r: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "g: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "b: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } },"
            "red: { set: function(nv) { return this.x = nv; }, get: function() { return this.x; } },"
            "green: { set: function(nv) { return this.y = nv; }, get: function() { return this.y; } },"
            "blue: { set: function(nv) { return this.z = nv; }, get: function() { return this.z; } }"
            "})");
    }
    ScriptValuePointer value = engine->newObject();
    value->setProperty("x", vec3.x);
    value->setProperty("y", vec3.y);
    value->setProperty("z", vec3.z);
    value->setPrototype(prototype);
    return value;
}

ScriptValuePointer u8vec3ColorToScriptValue(ScriptEngine* engine, const glm::u8vec3& vec3) {
    auto prototype = engine->globalObject()->property("__hifi_u8vec3_color__");
    if (!prototype->property("defined")->toBool()) {
        prototype = engine->evaluate(
            "__hifi_u8vec3_color__ = Object.defineProperties({}, { "
            "defined: { value: true },"
            "0: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "1: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "2: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } },"
            "r: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "g: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "b: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } },"
            "x: { set: function(nv) { return this.red = nv; }, get: function() { return this.red; } },"
            "y: { set: function(nv) { return this.green = nv; }, get: function() { return this.green; } },"
            "z: { set: function(nv) { return this.blue = nv; }, get: function() { return this.blue; } }"
            "})");
    }
    ScriptValuePointer value = engine->newObject();
    value->setProperty("red", vec3.x);
    value->setProperty("green", vec3.y);
    value->setProperty("blue", vec3.z);
    value->setPrototype(prototype);
    return value;
}

void u8vec3FromScriptValue(const ScriptValuePointer& object, glm::u8vec3& vec3) {
    if (object->isNumber()) {
        vec3 = glm::vec3(object->toVariant().toUInt());
    } else if (object->isString()) {
        QColor qColor(object->toString());
        if (qColor.isValid()) {
            vec3.x = (uint8_t)qColor.red();
            vec3.y = (uint8_t)qColor.green();
            vec3.z = (uint8_t)qColor.blue();
        }
    } else if (object->isArray()) {
        QVariantList list = object->toVariant().toList();
        if (list.length() == 3) {
            vec3.x = list[0].toUInt();
            vec3.y = list[1].toUInt();
            vec3.z = list[2].toUInt();
        }
    } else {
        ScriptValuePointer x = object->property("x");
        if (!x->isValid()) {
            x = object->property("r");
        }
        if (!x->isValid()) {
            x = object->property("red");
        }

        ScriptValuePointer y = object->property("y");
        if (!y->isValid()) {
            y = object->property("g");
        }
        if (!y->isValid()) {
            y = object->property("green");
        }

        ScriptValuePointer z = object->property("z");
        if (!z->isValid()) {
            z = object->property("b");
        }
        if (!z->isValid()) {
            z = object->property("blue");
        }

        vec3.x = x->toVariant().toUInt();
        vec3.y = y->toVariant().toUInt();
        vec3.z = z->toVariant().toUInt();
    }
}

ScriptValuePointer vec4toScriptValue(ScriptEngine* engine, const glm::vec4& vec4) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("x", vec4.x);
    obj->setProperty("y", vec4.y);
    obj->setProperty("z", vec4.z);
    obj->setProperty("w", vec4.w);
    return obj;
}

void vec4FromScriptValue(const ScriptValuePointer& object, glm::vec4& vec4) {
    vec4.x = object->property("x")->toVariant().toFloat();
    vec4.y = object->property("y")->toVariant().toFloat();
    vec4.z = object->property("z")->toVariant().toFloat();
    vec4.w = object->property("w")->toVariant().toFloat();
}

ScriptValuePointer mat4toScriptValue(ScriptEngine* engine, const glm::mat4& mat4) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("r0c0", mat4[0][0]);
    obj->setProperty("r1c0", mat4[0][1]);
    obj->setProperty("r2c0", mat4[0][2]);
    obj->setProperty("r3c0", mat4[0][3]);
    obj->setProperty("r0c1", mat4[1][0]);
    obj->setProperty("r1c1", mat4[1][1]);
    obj->setProperty("r2c1", mat4[1][2]);
    obj->setProperty("r3c1", mat4[1][3]);
    obj->setProperty("r0c2", mat4[2][0]);
    obj->setProperty("r1c2", mat4[2][1]);
    obj->setProperty("r2c2", mat4[2][2]);
    obj->setProperty("r3c2", mat4[2][3]);
    obj->setProperty("r0c3", mat4[3][0]);
    obj->setProperty("r1c3", mat4[3][1]);
    obj->setProperty("r2c3", mat4[3][2]);
    obj->setProperty("r3c3", mat4[3][3]);
    return obj;
}

void mat4FromScriptValue(const ScriptValuePointer& object, glm::mat4& mat4) {
    mat4[0][0] = object->property("r0c0")->toVariant().toFloat();
    mat4[0][1] = object->property("r1c0")->toVariant().toFloat();
    mat4[0][2] = object->property("r2c0")->toVariant().toFloat();
    mat4[0][3] = object->property("r3c0")->toVariant().toFloat();
    mat4[1][0] = object->property("r0c1")->toVariant().toFloat();
    mat4[1][1] = object->property("r1c1")->toVariant().toFloat();
    mat4[1][2] = object->property("r2c1")->toVariant().toFloat();
    mat4[1][3] = object->property("r3c1")->toVariant().toFloat();
    mat4[2][0] = object->property("r0c2")->toVariant().toFloat();
    mat4[2][1] = object->property("r1c2")->toVariant().toFloat();
    mat4[2][2] = object->property("r2c2")->toVariant().toFloat();
    mat4[2][3] = object->property("r3c2")->toVariant().toFloat();
    mat4[3][0] = object->property("r0c3")->toVariant().toFloat();
    mat4[3][1] = object->property("r1c3")->toVariant().toFloat();
    mat4[3][2] = object->property("r2c3")->toVariant().toFloat();
    mat4[3][3] = object->property("r3c3")->toVariant().toFloat();
}

ScriptValuePointer qVectorVec3ColorToScriptValue(ScriptEngine* engine, const QVector<glm::vec3>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, vec3ColorToScriptValue(engine, vector.at(i)));
    }
    return array;
}

ScriptValuePointer qVectorVec3ToScriptValue(ScriptEngine* engine, const QVector<glm::vec3>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, vec3ToScriptValue(engine, vector.at(i)));
    }
    return array;
}

QVector<glm::vec3> qVectorVec3FromScriptValue(const ScriptValuePointer& array) {
    QVector<glm::vec3> newVector;
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        glm::vec3 newVec3 = glm::vec3();
        vec3FromScriptValue(array->property(i), newVec3);
        newVector << newVec3;
    }
    return newVector;
}

void qVectorVec3FromScriptValue(const ScriptValuePointer& array, QVector<glm::vec3>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        glm::vec3 newVec3 = glm::vec3();
        vec3FromScriptValue(array->property(i), newVec3);
        vector << newVec3;
    }
}

ScriptValuePointer quatToScriptValue(ScriptEngine* engine, const glm::quat& quat) {
    ScriptValuePointer obj = engine->newObject();
    if (quat.x != quat.x || quat.y != quat.y || quat.z != quat.z || quat.w != quat.w) {
        // if quat contains a NaN don't try to convert it
        return obj;
    }
    obj->setProperty("x", quat.x);
    obj->setProperty("y", quat.y);
    obj->setProperty("z", quat.z);
    obj->setProperty("w", quat.w);
    return obj;
}

void quatFromScriptValue(const ScriptValuePointer& object, glm::quat& quat) {
    quat.x = object->property("x")->toVariant().toFloat();
    quat.y = object->property("y")->toVariant().toFloat();
    quat.z = object->property("z")->toVariant().toFloat();
    quat.w = object->property("w")->toVariant().toFloat();

    // enforce normalized quaternion
    float length = glm::length(quat);
    if (length > FLT_EPSILON) {
        quat /= length;
    } else {
        quat = glm::quat();
    }
}

ScriptValuePointer qVectorQuatToScriptValue(ScriptEngine* engine, const QVector<glm::quat>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, quatToScriptValue(engine, vector.at(i)));
    }
    return array;
}

ScriptValuePointer qVectorBoolToScriptValue(ScriptEngine* engine, const QVector<bool>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, vector.at(i));
    }
    return array;
}

QVector<float> qVectorFloatFromScriptValue(const ScriptValuePointer& array) {
    if (!array->isArray()) {
        return QVector<float>();
    }
    QVector<float> newVector;
    int length = array->property("length")->toInteger();
    newVector.reserve(length);
    for (int i = 0; i < length; i++) {
        if (array->property(i)->isNumber()) {
            newVector << array->property(i)->toNumber();
        }
    }

    return newVector;
}

ScriptValuePointer qVectorQUuidToScriptValue(ScriptEngine* engine, const QVector<QUuid>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, quuidToScriptValue(engine, vector.at(i)));
    }
    return array;
}

void qVectorQUuidFromScriptValue(const ScriptValuePointer& array, QVector<QUuid>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        vector << array->property(i)->toVariant().toUuid();
    }
}

QVector<QUuid> qVectorQUuidFromScriptValue(const ScriptValuePointer& array) {
    if (!array->isArray()) {
        return QVector<QUuid>();
    }
    QVector<QUuid> newVector;
    int length = array->property("length")->toInteger();
    newVector.reserve(length);
    for (int i = 0; i < length; i++) {
        QString uuidAsString = array->property(i)->toString();
        QUuid fromString(uuidAsString);
        newVector << fromString;
    }
    return newVector;
}

ScriptValuePointer qVectorFloatToScriptValue(ScriptEngine* engine, const QVector<float>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        float num = vector.at(i);
        array->setProperty(i, engine->newValue(num));
    }
    return array;
}

ScriptValuePointer qVectorIntToScriptValue(ScriptEngine* engine, const QVector<uint32_t>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        int num = vector.at(i);
        array->setProperty(i, engine->newValue(num));
    }
    return array;
}

void qVectorFloatFromScriptValue(const ScriptValuePointer& array, QVector<float>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        vector << array->property(i)->toVariant().toFloat();
    }
}

void qVectorIntFromScriptValue(const ScriptValuePointer& array, QVector<uint32_t>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        vector << array->property(i)->toVariant().toInt();
    }
}

QVector<glm::quat> qVectorQuatFromScriptValue(const ScriptValuePointer& array) {
    QVector<glm::quat> newVector;
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        glm::quat newQuat = glm::quat();
        quatFromScriptValue(array->property(i), newQuat);
        newVector << newQuat;
    }
    return newVector;
}

void qVectorQuatFromScriptValue(const ScriptValuePointer& array, QVector<glm::quat>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        glm::quat newQuat = glm::quat();
        quatFromScriptValue(array->property(i), newQuat);
        vector << newQuat;
    }
}

QVector<bool> qVectorBoolFromScriptValue(const ScriptValuePointer& array) {
    QVector<bool> newVector;
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        newVector << array->property(i)->toBool();
    }
    return newVector;
}

void qVectorBoolFromScriptValue(const ScriptValuePointer& array, QVector<bool>& vector) {
    int length = array->property("length")->toInteger();

    for (int i = 0; i < length; i++) {
        vector << array->property(i)->toBool();
    }
}

ScriptValuePointer qRectToScriptValue(ScriptEngine* engine, const QRect& rect) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("x", rect.x());
    obj->setProperty("y", rect.y());
    obj->setProperty("width", rect.width());
    obj->setProperty("height", rect.height());
    return obj;
}

void qRectFromScriptValue(const ScriptValuePointer& object, QRect& rect) {
    rect.setX(object->property("x")->toVariant().toInt());
    rect.setY(object->property("y")->toVariant().toInt());
    rect.setWidth(object->property("width")->toVariant().toInt());
    rect.setHeight(object->property("height")->toVariant().toInt());
}

ScriptValuePointer qRectFToScriptValue(ScriptEngine* engine, const QRectF& rect) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("x", rect.x());
    obj->setProperty("y", rect.y());
    obj->setProperty("width", rect.width());
    obj->setProperty("height", rect.height());
    return obj;
}

void qRectFFromScriptValue(const ScriptValuePointer& object, QRectF& rect) {
    rect.setX(object->property("x")->toVariant().toFloat());
    rect.setY(object->property("y")->toVariant().toFloat());
    rect.setWidth(object->property("width")->toVariant().toFloat());
    rect.setHeight(object->property("height")->toVariant().toFloat());
}

ScriptValuePointer qColorToScriptValue(ScriptEngine* engine, const QColor& color) {
    ScriptValuePointer object = engine->newObject();
    object->setProperty("red", color.red());
    object->setProperty("green", color.green());
    object->setProperty("blue", color.blue());
    object->setProperty("alpha", color.alpha());
    return object;
}

/*@jsdoc
 * An axis-aligned cube, defined as the bottom right near (minimum axes values) corner of the cube plus the dimension of its 
 * sides.
 * @typedef {object} AACube
 * @property {number} x - X coordinate of the brn corner of the cube.
 * @property {number} y - Y coordinate of the brn corner of the cube.
 * @property {number} z - Z coordinate of the brn corner of the cube.
 * @property {number} scale - The dimensions of each side of the cube.
 */
ScriptValuePointer aaCubeToScriptValue(ScriptEngine* engine, const AACube& aaCube) {
    ScriptValuePointer obj = engine->newObject();
    const glm::vec3& corner = aaCube.getCorner();
    obj->setProperty("x", corner.x);
    obj->setProperty("y", corner.y);
    obj->setProperty("z", corner.z);
    obj->setProperty("scale", aaCube.getScale());
    return obj;
}

void aaCubeFromScriptValue(const ScriptValuePointer& object, AACube& aaCube) {
    glm::vec3 corner;
    corner.x = object->property("x")->toVariant().toFloat();
    corner.y = object->property("y")->toVariant().toFloat();
    corner.z = object->property("z")->toVariant().toFloat();
    float scale = object->property("scale")->toVariant().toFloat();

    aaCube.setBox(corner, scale);
}

void qColorFromScriptValue(const ScriptValuePointer& object, QColor& color) {
    if (object->isNumber()) {
        color.setRgb(object->toUInt32());

    } else if (object->isString()) {
        color.setNamedColor(object->toString());

    } else {
        ScriptValuePointer alphaValue = object->property("alpha");
        color.setRgb(object->property("red")->toInt32(), object->property("green")->toInt32(), object->property("blue")->toInt32(),
                     alphaValue->isNumber() ? alphaValue->toInt32() : 255);
    }
}

ScriptValuePointer qURLToScriptValue(ScriptEngine* engine, const QUrl& url) {
    return engine->newValue(url.toString());
}

void qURLFromScriptValue(const ScriptValuePointer& object, QUrl& url) {
    url = object->toString();
}

ScriptValuePointer pickRayToScriptValue(ScriptEngine* engine, const PickRay& pickRay) {
    ScriptValuePointer obj = engine->newObject();
    ScriptValuePointer origin = vec3ToScriptValue(engine, pickRay.origin);
    obj->setProperty("origin", origin);
    ScriptValuePointer direction = vec3ToScriptValue(engine, pickRay.direction);
    obj->setProperty("direction", direction);
    return obj;
}

void pickRayFromScriptValue(const ScriptValuePointer& object, PickRay& pickRay) {
    ScriptValuePointer originValue = object->property("origin");
    if (originValue->isValid()) {
        auto x = originValue->property("x");
        auto y = originValue->property("y");
        auto z = originValue->property("z");
        if (x->isValid() && y->isValid() && z->isValid()) {
            pickRay.origin.x = x->toVariant().toFloat();
            pickRay.origin.y = y->toVariant().toFloat();
            pickRay.origin.z = z->toVariant().toFloat();
        }
    }
    ScriptValuePointer directionValue = object->property("direction");
    if (directionValue->isValid()) {
        auto x = directionValue->property("x");
        auto y = directionValue->property("y");
        auto z = directionValue->property("z");
        if (x->isValid() && y->isValid() && z->isValid()) {
            pickRay.direction.x = x->toVariant().toFloat();
            pickRay.direction.y = y->toVariant().toFloat();
            pickRay.direction.z = z->toVariant().toFloat();
        }
    }
}

/*@jsdoc
 * Details of a collision between avatars and entities.
 * @typedef {object} Collision
 * @property {ContactEventType} type - The contact type of the collision event.
 * @property {Uuid} idA - The ID of one of the avatars or entities in the collision.
 * @property {Uuid} idB - The ID of the other of the avatars or entities in the collision.
 * @property {Vec3} penetration - The amount of penetration between the two items.
 * @property {Vec3} contactPoint - The point of contact.
 * @property {Vec3} velocityChange - The change in relative velocity of the two items, in m/s.
 */
ScriptValuePointer collisionToScriptValue(ScriptEngine* engine, const Collision& collision) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("type", collision.type);
    obj->setProperty("idA", quuidToScriptValue(engine, collision.idA));
    obj->setProperty("idB", quuidToScriptValue(engine, collision.idB));
    obj->setProperty("penetration", vec3ToScriptValue(engine, collision.penetration));
    obj->setProperty("contactPoint", vec3ToScriptValue(engine, collision.contactPoint));
    obj->setProperty("velocityChange", vec3ToScriptValue(engine, collision.velocityChange));
    return obj;
}

void collisionFromScriptValue(const ScriptValuePointer& object, Collision& collision) {
    // TODO: implement this when we know what it means to accept collision events from JS
}

ScriptValuePointer quuidToScriptValue(ScriptEngine* engine, const QUuid& uuid) {
    if (uuid.isNull()) {
        return engine->nullValue();
    }
    ScriptValuePointer obj(engine->newValue(uuid.toString()));
    return obj;
}

void quuidFromScriptValue(const ScriptValuePointer& object, QUuid& uuid) {
    if (object.isNull()) {
        uuid = QUuid();
        return;
    }
    QString uuidAsString = object->toVariant().toString();
    QUuid fromString(uuidAsString);
    uuid = fromString;
}

/*@jsdoc
 * A 2D size value.
 * @typedef {object} Size
 * @property {number} height - The height value.
 * @property {number} width - The width value.
 */
ScriptValuePointer qSizeFToScriptValue(ScriptEngine* engine, const QSizeF& qSizeF) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("width", qSizeF.width());
    obj->setProperty("height", qSizeF.height());
    return obj;
}

void qSizeFFromScriptValue(const ScriptValuePointer& object, QSizeF& qSizeF) {
    qSizeF.setWidth(object->property("width")->toVariant().toFloat());
    qSizeF.setHeight(object->property("height")->toVariant().toFloat());
}

/*@jsdoc
 * The details of an animation that is playing.
 * @typedef {object} Avatar.AnimationDetails
 * @property {string} role - <em>Not used.</em>
 * @property {string} url - The URL to the animation file. Animation files need to be in glTF or FBX format but only need to 
 *     contain the avatar skeleton and animation data. glTF models may be in JSON or binary format (".gltf" or ".glb" URLs 
 *     respectively).
 *     <p><strong>Warning:</strong> glTF animations currently do not always animate correctly.</p>
 * @property {number} fps - The frames per second(FPS) rate for the animation playback. 30 FPS is normal speed.
 * @property {number} priority - <em>Not used.</em>
 * @property {boolean} loop - <code>true</code> if the animation should loop, <code>false</code> if it shouldn't.
 * @property {boolean} hold - <em>Not used.</em>
 * @property {number} firstFrame - The frame the animation should start at.
 * @property {number} lastFrame - The frame the animation should stop at.
 * @property {boolean} running - <em>Not used.</em>
 * @property {number} currentFrame - The current frame being played.
 * @property {boolean} startAutomatically - <em>Not used.</em>
 * @property {boolean} allowTranslation - <em>Not used.</em>
 */
ScriptValuePointer animationDetailsToScriptValue(ScriptEngine* engine, const AnimationDetails& details) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("role", details.role);
    obj->setProperty("url", details.url.toString());
    obj->setProperty("fps", details.fps);
    obj->setProperty("priority", details.priority);
    obj->setProperty("loop", details.loop);
    obj->setProperty("hold", details.hold);
    obj->setProperty("startAutomatically", details.startAutomatically);
    obj->setProperty("firstFrame", details.firstFrame);
    obj->setProperty("lastFrame", details.lastFrame);
    obj->setProperty("running", details.running);
    obj->setProperty("currentFrame", details.currentFrame);
    obj->setProperty("allowTranslation", details.allowTranslation);
    return obj;
}

void animationDetailsFromScriptValue(const ScriptValuePointer& object, AnimationDetails& details) {
    // nothing for now...
}

ScriptValuePointer meshToScriptValue(ScriptEngine* engine, MeshProxy* const& in) {
    return engine->newQObject(in, ScriptEngine::QtOwnership,
                              ScriptEngine::ExcludeDeleteLater | ScriptEngine::ExcludeChildObjects);
}

void meshFromScriptValue(const ScriptValuePointer& value, MeshProxy*& out) {
    out = qobject_cast<MeshProxy*>(value->toQObject());
}

ScriptValuePointer meshesToScriptValue(ScriptEngine* engine, const MeshProxyList& in) {
    // ScriptValueList result;
    ScriptValuePointer result = engine->newArray();
    int i = 0;
    foreach (MeshProxy* const meshProxy, in) { result->setProperty(i++, meshToScriptValue(engine, meshProxy)); }
    return result;
}

void meshesFromScriptValue(const ScriptValuePointer& value, MeshProxyList& out) {
    ScriptValueIteratorPointer itr(value->newIterator());

    qDebug() << "in meshesFromScriptValue, value.length =" << value->property("length")->toInt32();

    while (itr->hasNext()) {
        itr->next();
        MeshProxy* meshProxy = scriptvalue_cast<MeshProxyList::value_type>(itr->value());
        if (meshProxy) {
            out.append(meshProxy);
        } else {
            qDebug() << "null meshProxy";
        }
    }
}

/*@jsdoc
 * A triangle in a mesh.
 * @typedef {object} MeshFace
 * @property {number[]} vertices - The indexes of the three vertices that make up the face.
 */
ScriptValuePointer meshFaceToScriptValue(ScriptEngine* engine, const MeshFace& meshFace) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("vertices", qVectorIntToScriptValue(engine, meshFace.vertexIndices));
    return obj;
}

void meshFaceFromScriptValue(const ScriptValuePointer& object, MeshFace& meshFaceResult) {
    qVectorIntFromScriptValue(object->property("vertices"), meshFaceResult.vertexIndices);
}

ScriptValuePointer qVectorMeshFaceToScriptValue(ScriptEngine* engine, const QVector<MeshFace>& vector) {
    ScriptValuePointer array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array->setProperty(i, meshFaceToScriptValue(engine, vector.at(i)));
    }
    return array;
}

void qVectorMeshFaceFromScriptValue(const ScriptValuePointer& array, QVector<MeshFace>& result) {
    int length = array->property("length")->toInteger();
    result.clear();

    for (int i = 0; i < length; i++) {
        MeshFace meshFace = MeshFace();
        meshFaceFromScriptValue(array->property(i), meshFace);
        result << meshFace;
    }
}

ScriptValuePointer stencilMaskModeToScriptValue(ScriptEngine* engine, const StencilMaskMode& stencilMode) {
    return engine->newValue((int)stencilMode);
}

void stencilMaskModeFromScriptValue(const ScriptValuePointer& object, StencilMaskMode& stencilMode) {
    stencilMode = StencilMaskMode(object->toVariant().toInt());
}

void promiseFromScriptValue(const ScriptValuePointer& object, std::shared_ptr<MiniPromise>& promise) {
    Q_ASSERT(false);
}
ScriptValuePointer promiseToScriptValue(ScriptEngine* engine, const std::shared_ptr<MiniPromise>& promise) {
    return engine->newQObject(promise.get());
}
