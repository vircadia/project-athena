//
//  RegisteredMetaTypes.cpp
//  libraries/shared/src
//
//  Created by Stephen Birarda on 10/3/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RegisteredMetaTypes.h"

#include <glm/gtc/quaternion.hpp>

#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtCore/QRect>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QQuaternion>
#include <QtNetwork/QAbstractSocket>
#include <QJsonDocument>

int uint32MetaTypeId = qRegisterMetaType<glm::uint32>("uint32");
int glmUint32MetaTypeId = qRegisterMetaType<glm::uint32>("glm::uint32");
int vec2MetaTypeId = qRegisterMetaType<glm::vec2>();
int u8vec3MetaTypeId = qRegisterMetaType<u8vec3>();
int vec3MetaTypeId = qRegisterMetaType<glm::vec3>();
int vec4MetaTypeId = qRegisterMetaType<glm::vec4>();
int qVectorVec3MetaTypeId = qRegisterMetaType<QVector<glm::vec3>>();
int qVectorQuatMetaTypeId = qRegisterMetaType<QVector<glm::quat>>();
int qVectorBoolMetaTypeId = qRegisterMetaType<QVector<bool>>();
int qVectorGLMUint32MetaTypeId = qRegisterMetaType<QVector<unsigned int>>("QVector<glm::uint32>");
int qVectorQUuidMetaTypeId = qRegisterMetaType<QVector<QUuid>>();
int quatMetaTypeId = qRegisterMetaType<glm::quat>();
int pickRayMetaTypeId = qRegisterMetaType<PickRay>();
int collisionMetaTypeId = qRegisterMetaType<Collision>();
int qMapURLStringMetaTypeId = qRegisterMetaType<QMap<QUrl,QString>>();
int socketErrorMetaTypeId = qRegisterMetaType<QAbstractSocket::SocketError>();
int voidLambdaType = qRegisterMetaType<std::function<void()>>();
int variantLambdaType = qRegisterMetaType<std::function<QVariant()>>();
int stencilModeMetaTypeId = qRegisterMetaType<StencilMaskMode>();

QVariant vec2ToVariant(const glm::vec2 &vec2) {
    if (vec2.x != vec2.x || vec2.y != vec2.y) {
        // if vec2 contains a NaN don't try to convert it
        return QVariant();
    }
    QVariantMap result;
    result["x"] = vec2.x;
    result["y"] = vec2.y;
    return result;
}

glm::vec2 vec2FromVariant(const QVariant &object, bool& isValid) {
    isValid = false;
    glm::vec2 result;
    if (object.canConvert<float>()) {
        result = glm::vec2(object.toFloat());
        isValid = true;
    } else if (object.canConvert<QVector2D>()) {
        auto qvec2 = qvariant_cast<QVector2D>(object);
        result.x = qvec2.x();
        result.y = qvec2.y();
        isValid = true;
    } else {
        auto map = object.toMap();
        auto x = map["x"];
        if (!x.isValid()) {
            x = map["u"];
        }

        auto y = map["y"];
        if (!y.isValid()) {
            y = map["v"];
        }

        if (x.isValid() && y.isValid()) {
            result.x = x.toFloat(&isValid);
            if (isValid) {
                result.y = y.toFloat(&isValid);
            }
        }
    }
    return result;
}

glm::vec2 vec2FromVariant(const QVariant &object) {
    bool valid;
    return vec2FromVariant(object, valid);
}

QVariant vec3toVariant(const glm::vec3& vec3) {
    if (vec3.x != vec3.x || vec3.y != vec3.y || vec3.z != vec3.z) {
        // if vec3 contains a NaN don't try to convert it
        return QVariant();
    }
    QVariantMap result;
    result["x"] = vec3.x;
    result["y"] = vec3.y;
    result["z"] = vec3.z;
    return result;
}

glm::vec3 vec3FromVariant(const QVariant& object, bool& valid) {
    glm::vec3 v;
    valid = false;
    if (!object.isValid() || object.isNull()) {
        return v;
    } else if (object.canConvert<float>()) {
        v = glm::vec3(object.toFloat());
        valid = true;
    } else if (object.canConvert<QVector3D>()) {
        auto qvec3 = qvariant_cast<QVector3D>(object);
        v.x = qvec3.x();
        v.y = qvec3.y();
        v.z = qvec3.z();
        valid = true;
    } else if (object.canConvert<QString>()) {
        QColor qColor(object.toString());
        if (qColor.isValid()) {
            v.x = (uint8_t)qColor.red();
            v.y = (uint8_t)qColor.green();
            v.z = (uint8_t)qColor.blue();
            valid = true;
        }
    } else if (object.canConvert<QColor>()) {
        QColor qColor = qvariant_cast<QColor>(object);
        if (qColor.isValid()) {
            v.x = (uint8_t)qColor.red();
            v.y = (uint8_t)qColor.green();
            v.z = (uint8_t)qColor.blue();
            valid = true;
        }
    } else {
        auto map = object.toMap();
        auto x = map["x"];
        if (!x.isValid()) {
            x = map["r"];
        }
        if (!x.isValid()) {
            x = map["red"];
        }

        auto y = map["y"];
        if (!y.isValid()) {
            y = map["g"];
        }
        if (!y.isValid()) {
            y = map["green"];
        }

        auto z = map["z"];
        if (!z.isValid()) {
            z = map["b"];
        }
        if (!z.isValid()) {
            z = map["blue"];
        }

        if (x.canConvert<float>() && y.canConvert<float>() && z.canConvert<float>()) {
            v.x = x.toFloat();
            v.y = y.toFloat();
            v.z = z.toFloat();
            valid = true;
        }
    }
    return v;
}

glm::vec3 vec3FromVariant(const QVariant& object) {
    bool valid = false;
    return vec3FromVariant(object, valid);
}

QVariant u8vec3toVariant(const glm::u8vec3& vec3) {
    QVariantMap result;
    result["x"] = vec3.x;
    result["y"] = vec3.y;
    result["z"] = vec3.z;
    return result;
}

QVariant u8vec3ColortoVariant(const glm::u8vec3& vec3) {
    QVariantMap result;
    result["red"] = vec3.x;
    result["green"] = vec3.y;
    result["blue"] = vec3.z;
    return result;
}

glm::u8vec3 u8vec3FromVariant(const QVariant& object, bool& valid) {
    glm::u8vec3 v;
    valid = false;
    if (!object.isValid() || object.isNull()) {
        return v;
    } else if (object.canConvert<uint>()) {
        v = glm::vec3(object.toUInt());
        valid = true;
    } else if (object.canConvert<QVector3D>()) {
        auto qvec3 = qvariant_cast<QVector3D>(object);
        v.x = (uint8_t)qvec3.x();
        v.y = (uint8_t)qvec3.y();
        v.z = (uint8_t)qvec3.z();
        valid = true;
    } else if (object.canConvert<QString>()) {
        QColor qColor(object.toString());
        if (qColor.isValid()) {
            v.x = (uint8_t)qColor.red();
            v.y = (uint8_t)qColor.green();
            v.z = (uint8_t)qColor.blue();
            valid = true;
        }
    } else if (object.canConvert<QColor>()) {
        QColor qColor = qvariant_cast<QColor>(object);
        if (qColor.isValid()) {
            v.x = (uint8_t)qColor.red();
            v.y = (uint8_t)qColor.green();
            v.z = (uint8_t)qColor.blue();
            valid = true;
        }
    } else {
        auto map = object.toMap();
        auto x = map["x"];
        if (!x.isValid()) {
            x = map["r"];
        }
        if (!x.isValid()) {
            x = map["red"];
        }

        auto y = map["y"];
        if (!y.isValid()) {
            y = map["g"];
        }
        if (!y.isValid()) {
            y = map["green"];
        }

        auto z = map["z"];
        if (!z.isValid()) {
            z = map["b"];
        }
        if (!z.isValid()) {
            z = map["blue"];
        }

        if (x.canConvert<uint>() && y.canConvert<uint>() && z.canConvert<uint>()) {
            v.x = x.toUInt();
            v.y = y.toUInt();
            v.z = z.toUInt();
            valid = true;
        }
    }
    return v;
}

glm::u8vec3 u8vec3FromVariant(const QVariant& object) {
    bool valid = false;
    return u8vec3FromVariant(object, valid);
}

QVariant vec4toVariant(const glm::vec4& vec4) {
    if (isNaN(vec4.x) || isNaN(vec4.y) || isNaN(vec4.z) || isNaN(vec4.w)) {
        // if vec4 contains a NaN don't try to convert it
        return QVariant();
    }
    QVariantMap result;
    result["x"] = vec4.x;
    result["y"] = vec4.y;
    result["z"] = vec4.z;
    result["w"] = vec4.w;
    return result;
}

glm::vec4 vec4FromVariant(const QVariant& object, bool& valid) {
    glm::vec4 v;
    valid = false;
    if (!object.isValid() || object.isNull()) {
        return v;
    } else if (object.canConvert<float>()) {
        v = glm::vec4(object.toFloat());
        valid = true;
    } else if (object.canConvert<QVector4D>()) {
        auto qvec4 = qvariant_cast<QVector4D>(object);
        v.x = qvec4.x();
        v.y = qvec4.y();
        v.z = qvec4.z();
        v.w = qvec4.w();
        valid = true;
    } else {
        auto map = object.toMap();
        auto x = map["x"];
        auto y = map["y"];
        auto z = map["z"];
        auto w = map["w"];
        if (x.canConvert<float>() && y.canConvert<float>() && z.canConvert<float>() && w.canConvert<float>()) {
            v.x = x.toFloat();
            v.y = y.toFloat();
            v.z = z.toFloat();
            v.w = w.toFloat();
            valid = true;
        }
    }
    return v;
}

glm::vec4 vec4FromVariant(const QVariant& object) {
    bool valid = false;
    return vec4FromVariant(object, valid);
}

QVariant mat4ToVariant(const glm::mat4& mat4) {
    if (mat4 != mat4) {
        // NaN
        return QVariant();
    }
    QVariantMap object;

    object["r0c0"] = mat4[0][0];
    object["r1c0"] = mat4[0][1];
    object["r2c0"] = mat4[0][2];
    object["r3c0"] = mat4[0][3];
    object["r0c1"] = mat4[1][0];
    object["r1c1"] = mat4[1][1];
    object["r2c1"] = mat4[1][2];
    object["r3c1"] = mat4[1][3];
    object["r0c2"] = mat4[2][0];
    object["r1c2"] = mat4[2][1];
    object["r2c2"] = mat4[2][2];
    object["r3c2"] = mat4[2][3];
    object["r0c3"] = mat4[3][0];
    object["r1c3"] = mat4[3][1];
    object["r2c3"] = mat4[3][2];
    object["r3c3"] = mat4[3][3];

    return object;
}

glm::mat4 mat4FromVariant(const QVariant& object, bool& valid) {
    glm::mat4 mat4;
    valid = false;
    if (!object.isValid() || object.isNull()) {
        return mat4;
    } else {
        const static auto getElement = [](const QVariantMap& map, const char * key, float& value, bool& everyConversionValid) {
            auto variantValue = map[key];
            if (variantValue.canConvert<float>()) {
                value = variantValue.toFloat();
            } else {
                everyConversionValid = false;
            }
        };

        auto map = object.toMap();
        bool everyConversionValid = true;

        getElement(map, "r0c0", mat4[0][0], everyConversionValid);
        getElement(map, "r1c0", mat4[0][1], everyConversionValid);
        getElement(map, "r2c0", mat4[0][2], everyConversionValid);
        getElement(map, "r3c0", mat4[0][3], everyConversionValid);
        getElement(map, "r0c1", mat4[1][0], everyConversionValid);
        getElement(map, "r1c1", mat4[1][1], everyConversionValid);
        getElement(map, "r2c1", mat4[1][2], everyConversionValid);
        getElement(map, "r3c1", mat4[1][3], everyConversionValid);
        getElement(map, "r0c2", mat4[2][0], everyConversionValid);
        getElement(map, "r1c2", mat4[2][1], everyConversionValid);
        getElement(map, "r2c2", mat4[2][2], everyConversionValid);
        getElement(map, "r3c2", mat4[2][3], everyConversionValid);
        getElement(map, "r0c3", mat4[3][0], everyConversionValid);
        getElement(map, "r1c3", mat4[3][1], everyConversionValid);
        getElement(map, "r2c3", mat4[3][2], everyConversionValid);
        getElement(map, "r3c3", mat4[3][3], everyConversionValid);

        if (everyConversionValid) {
            valid = true;
        }

        return mat4;
    }
}

glm::mat4 mat4FromVariant(const QVariant& object) {
    bool valid = false;
    return mat4FromVariant(object, valid);
}

glm::quat quatFromVariant(const QVariant &object, bool& isValid) {
    glm::quat q;
    if (object.canConvert<QQuaternion>()) {
        auto qquat = qvariant_cast<QQuaternion>(object);
        q.x = qquat.x();
        q.y = qquat.y();
        q.z = qquat.z();
        q.w = qquat.scalar();

        // enforce normalized quaternion
        float length = glm::length(q);
        if (length > FLT_EPSILON) {
            q /= length;
        } else {
            q = glm::quat();
        }
        isValid = true;
    } else {
        auto map = object.toMap();
        q.x = map["x"].toFloat(&isValid);
        if (!isValid) {
            return glm::quat();
        }
        q.y = map["y"].toFloat(&isValid);
        if (!isValid) {
            return glm::quat();
        }
        q.z = map["z"].toFloat(&isValid);
        if (!isValid) {
            return glm::quat();
        }
        q.w = map["w"].toFloat(&isValid);
        if (!isValid) {
            return glm::quat();
        }
    }
    return q;
}

glm::quat quatFromVariant(const QVariant& object) {
    bool valid = false;
    return quatFromVariant(object, valid);
}

QVariant quatToVariant(const glm::quat& quat) {
    if (quat.x != quat.x || quat.y != quat.y || quat.z != quat.z) {
        // if quat contains a NaN don't try to convert it
        return QVariant();
    }
    QVariantMap result;
    result["x"] = quat.x;
    result["y"] = quat.y;
    result["z"] = quat.z;
    result["w"] = quat.w;
    return result;
}

QVariant qRectToVariant(const QRect& rect) {
    QVariantMap obj;
    obj["x"] = rect.x();
    obj["y"] = rect.y();
    obj["width"] = rect.width();
    obj["height"] = rect.height();
    return obj;
}

QRect qRectFromVariant(const QVariant& objectVar, bool& valid) {
    QVariantMap object = objectVar.toMap();
    QRect rect;
    valid = false;
    rect.setX(object["x"].toInt(&valid));
    if (valid) {
        rect.setY(object["y"].toInt(&valid));
    }
    if (valid) {
        rect.setWidth(object["width"].toInt(&valid));
    }
    if (valid) {
        rect.setHeight(object["height"].toInt(&valid));
    }
    return rect;
}

QRect qRectFromVariant(const QVariant& object) {
    bool valid;
    return qRectFromVariant(object, valid);
}

QVariant qRectFToVariant(const QRectF& rect) {
    QVariantMap obj;
    obj["x"] = rect.x();
    obj["y"] = rect.y();
    obj["width"] = rect.width();
    obj["height"] = rect.height();
    return obj;
}

QRectF qRectFFromVariant(const QVariant& objectVar, bool& valid) {
    QVariantMap object = objectVar.toMap();
    QRectF rect;
    valid = false;
    rect.setX(object["x"].toFloat(&valid));
    if (valid) {
        rect.setY(object["y"].toFloat(&valid));
    }
    if (valid) {
        rect.setWidth(object["width"].toFloat(&valid));
    }
    if (valid) {
        rect.setHeight(object["height"].toFloat(&valid));
    }
    return rect;
}

QRectF qRectFFromVariant(const QVariant& object) {
    bool valid;
    return qRectFFromVariant(object, valid);
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
void Collision::invert() {
    std::swap(idA, idB);
    contactPoint += penetration;
    penetration *= -1.0f;
}

AnimationDetails::AnimationDetails() :
    role(), url(), fps(0.0f), priority(0.0f), loop(false), hold(false),
    startAutomatically(false), firstFrame(0.0f), lastFrame(0.0f), running(false), currentFrame(0.0f) {
}

AnimationDetails::AnimationDetails(QString role, QUrl url, float fps, float priority, bool loop,
    bool hold, bool startAutomatically, float firstFrame, float lastFrame, bool running, float currentFrame, bool allowTranslation) :
    role(role), url(url), fps(fps), priority(priority), loop(loop), hold(hold),
    startAutomatically(startAutomatically), firstFrame(firstFrame), lastFrame(lastFrame),
    running(running), currentFrame(currentFrame), allowTranslation(allowTranslation) {
}

QVariantMap parseTexturesToMap(QString newTextures, const QVariantMap& defaultTextures) {
    // If textures are unset, revert to original textures
    if (newTextures.isEmpty()) {
        return defaultTextures;
    }

    // Legacy: a ,\n-delimited list of filename:"texturepath"
    if (*newTextures.cbegin() != '{') {
        newTextures = "{\"" + newTextures.replace(":\"", "\":\"").replace(",\n", ",\"") + "}";
    }

    QJsonParseError error;
    QJsonDocument newTexturesJson = QJsonDocument::fromJson(newTextures.toUtf8(), &error);
    // If textures are invalid, revert to original textures
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Could not evaluate textures property value:" << newTextures;
        return defaultTextures;
    }

    QVariantMap newTexturesMap = newTexturesJson.toVariant().toMap();
    QVariantMap toReturn = defaultTextures;
    for (auto& texture : newTexturesMap.keys()) {
        auto newURL = newTexturesMap[texture];
        if (newURL.canConvert<QUrl>()) {
            toReturn[texture] = newURL.toUrl();
        } else if (newURL.canConvert<QString>()) {
            toReturn[texture] = QUrl(newURL.toString());
        } else {
            toReturn[texture] = newURL;
        }
    }

    return toReturn;
}
