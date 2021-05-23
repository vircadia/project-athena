//
//  RegisteredMetaTypes.h
//  libraries/shared/src
//
//  Created by Stephen Birarda on 10/3/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RegisteredMetaTypes_h
#define hifi_RegisteredMetaTypes_h

#include <QtCore/QUuid>
#include <QtCore/QUrl>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AACube.h"
#include "ShapeInfo.h"
#include "SharedUtil.h"
#include "shared/Bilateral.h"
#include "Transform.h"
#include "PhysicsCollisionGroups.h"
#include "StencilMaskMode.h"

class QColor;
class QUrl;

Q_DECLARE_METATYPE(glm::vec2)
Q_DECLARE_METATYPE(glm::u8vec3)
Q_DECLARE_METATYPE(glm::vec3)
Q_DECLARE_METATYPE(glm::vec4)
Q_DECLARE_METATYPE(glm::quat)
Q_DECLARE_METATYPE(glm::mat4)
Q_DECLARE_METATYPE(QVector<float>)
Q_DECLARE_METATYPE(unsigned int)
Q_DECLARE_METATYPE(QVector<unsigned int>)
Q_DECLARE_METATYPE(AACube)
Q_DECLARE_METATYPE(std::function<void()>);
Q_DECLARE_METATYPE(std::function<QVariant()>);

// Mat4
/*@jsdoc
 * A 4 x 4 matrix, typically containing a scale, rotation, and translation transform. See also the {@link Mat4(0)|Mat4} object.
 *
 * @typedef {object} Mat4
 * @property {number} r0c0 - Row 0, column 0 value.
 * @property {number} r1c0 - Row 1, column 0 value.
 * @property {number} r2c0 - Row 2, column 0 value.
 * @property {number} r3c0 - Row 3, column 0 value.
 * @property {number} r0c1 - Row 0, column 1 value.
 * @property {number} r1c1 - Row 1, column 1 value.
 * @property {number} r2c1 - Row 2, column 1 value.
 * @property {number} r3c1 - Row 3, column 1 value.
 * @property {number} r0c2 - Row 0, column 2 value.
 * @property {number} r1c2 - Row 1, column 2 value.
 * @property {number} r2c2 - Row 2, column 2 value.
 * @property {number} r3c2 - Row 3, column 2 value.
 * @property {number} r0c3 - Row 0, column 3 value.
 * @property {number} r1c3 - Row 1, column 3 value.
 * @property {number} r2c3 - Row 2, column 3 value.
 * @property {number} r3c3 - Row 3, column 3 value.
 */
QVariant mat4ToVariant(const glm::mat4& mat4);
glm::mat4 mat4FromVariant(const QVariant& object, bool& valid);
glm::mat4 mat4FromVariant(const QVariant& object);

/*@jsdoc
* A 2-dimensional vector.
*
* @typedef {object} Vec2
* @property {number} x - X-coordinate of the vector. Synonyms: <code>u</code>.
* @property {number} y - Y-coordinate of the vector. Synonyms: <code>v</code>.
* @example <caption>Vec2s can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
* Entities.editEntity(<id>, { materialMappingPos: { x: 0.1, y: 0.2 }});          // { x: 0.1, y: 0.2 }
* Entities.editEntity(<id>, { materialMappingPos: { u: 0.3, v: 0.4 }});          // { x: 0.3, y: 0.4 }
* Entities.editEntity(<id>, { materialMappingPos: [0.5, 0.6] });                 // { x: 0.5, y: 0.6 }
* Entities.editEntity(<id>, { materialMappingPos: 0.7 });                        // { x: 0.7, y: 0.7 }
* var color = Entities.getEntityProperties(<id>).materialMappingPos;             // { x: 0.7, y: 0.7 }
* color.v = 0.8;                                                                 // { x: 0.7, y: 0.8 }
*/
QVariant vec2ToVariant(const glm::vec2& vec2);
glm::vec2 vec2FromVariant(const QVariant& object, bool& valid);
glm::vec2 vec2FromVariant(const QVariant& object);

/*@jsdoc
* A 3-dimensional vector. See also the {@link Vec3(0)|Vec3} object.
*
* @typedef {object} Vec3
* @property {number} x - X-coordinate of the vector. Synonyms: <code>r</code>, <code>red</code>.
* @property {number} y - Y-coordinate of the vector. Synonyms: <code>g</code>, <code>green</code>.
* @property {number} z - Z-coordinate of the vector. Synonyms: <code>b</code>, <code>blue</code>.
* @example <caption>Vec3 values can be set in multiple ways and modified with their aliases, but still stringify in the same 
*     way.</caption>
* Entities.editEntity(<id>, { position: { x: 1, y: 2, z: 3 }});                 // { x: 1, y: 2, z: 3 }
* Entities.editEntity(<id>, { position: { r: 4, g: 5, b: 6 }});                 // { x: 4, y: 5, z: 6 }
* Entities.editEntity(<id>, { position: { red: 7, green: 8, blue: 9 }});        // { x: 7, y: 8, z: 9 }
* Entities.editEntity(<id>, { position: [10, 11, 12] });                        // { x: 10, y: 11, z: 12 }
* Entities.editEntity(<id>, { position: 13 });                                  // { x: 13, y: 13, z: 13 }
* var position = Entities.getEntityProperties(<id>).position;                   // { x: 13, y: 13, z: 13 }
* position.g = 14;                                                              // { x: 13, y: 14, z: 13 }
* position.blue = 15;                                                           // { x: 13, y: 14, z: 15 }
* Entities.editEntity(<id>, { position: "red"});                                // { x: 255, y: 0, z: 0 }
* Entities.editEntity(<id>, { position: "#00FF00"});                            // { x: 0, y: 255, z: 0 }
*/
QVariant vec3toVariant(const glm::vec3& vec3);
glm::vec3 vec3FromVariant(const QVariant &object, bool& valid);
glm::vec3 vec3FromVariant(const QVariant &object);

/*@jsdoc
 * A color vector. See also the {@link Vec3(0)|Vec3} object.
 *
 * @typedef {object} Color
 * @property {number} red - Red component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>r</code>, <code>x</code>.
 * @property {number} green - Green component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>g</code>, <code>y</code>.
 * @property {number} blue - Blue component value. Integer in the range <code>0</code> - <code>255</code>.  Synonyms: <code>b</code>, <code>z</code>.
 * @example <caption>Colors can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
 * Entities.editEntity(<id>, { color: { x: 1, y: 2, z: 3 }});                 // { red: 1, green: 2, blue: 3 }
 * Entities.editEntity(<id>, { color: { r: 4, g: 5, b: 6 }});                 // { red: 4, green: 5, blue: 6 }
 * Entities.editEntity(<id>, { color: { red: 7, green: 8, blue: 9 }});        // { red: 7, green: 8, blue: 9 }
 * Entities.editEntity(<id>, { color: [10, 11, 12] });                        // { red: 10, green: 11, blue: 12 }
 * Entities.editEntity(<id>, { color: 13 });                                  // { red: 13, green: 13, blue: 13 }
 * var color = Entities.getEntityProperties(<id>).color;                      // { red: 13, green: 13, blue: 13 }
 * color.g = 14;                                                              // { red: 13, green: 14, blue: 13 }
 * color.blue = 15;                                                           // { red: 13, green: 14, blue: 15 }
 * Entities.editEntity(<id>, { color: "red"});                                // { red: 255, green: 0, blue: 0 }
 * Entities.editEntity(<id>, { color: "#00FF00"});                            // { red: 0, green: 255, blue: 0 }
 */
/*@jsdoc
 * A color vector with real values. Values may also be <code>null</code>. See also the {@link Vec3(0)|Vec3} object.
 *
 * @typedef {object} ColorFloat
 * @property {number} red - Red component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>r</code>, <code>x</code>.
 * @property {number} green - Green component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>g</code>, <code>y</code>.
 * @property {number} blue - Blue component value. Real in the range <code>0</code> - <code>255</code>.  Synonyms: <code>b</code>, <code>z</code>.
 * @example <caption>ColorFloats can be set in multiple ways and modified with their aliases, but still stringify in the same way</caption>
 * Entities.editEntity(<id>, { color: { x: 1, y: 2, z: 3 }});                 // { red: 1, green: 2, blue: 3 }
 * Entities.editEntity(<id>, { color: { r: 4, g: 5, b: 6 }});                 // { red: 4, green: 5, blue: 6 }
 * Entities.editEntity(<id>, { color: { red: 7, green: 8, blue: 9 }});        // { red: 7, green: 8, blue: 9 }
 * Entities.editEntity(<id>, { color: [10, 11, 12] });                        // { red: 10, green: 11, blue: 12 }
 * Entities.editEntity(<id>, { color: 13 });                                  // { red: 13, green: 13, blue: 13 }
 * var color = Entities.getEntityProperties(<id>).color;                      // { red: 13, green: 13, blue: 13 }
 * color.g = 14;                                                              // { red: 13, green: 14, blue: 13 }
 * color.blue = 15;                                                           // { red: 13, green: 14, blue: 15 }
 * Entities.editEntity(<id>, { color: "red"});                                // { red: 255, green: 0, blue: 0 }
 * Entities.editEntity(<id>, { color: "#00FF00"});                            // { red: 0, green: 255, blue: 0 }
 */
QVariant u8vec3toVariant(const glm::u8vec3& vec3);
QVariant u8vec3ColortoVariant(const glm::u8vec3& vec3);
glm::u8vec3 u8vec3FromVariant(const QVariant &object, bool& valid);
glm::u8vec3 u8vec3FromVariant(const QVariant &object);

/*@jsdoc
 * A 4-dimensional vector.
 *
 * @typedef {object} Vec4
 * @property {number} x - X-coordinate of the vector.
 * @property {number} y - Y-coordinate of the vector.
 * @property {number} z - Z-coordinate of the vector.
 * @property {number} w - W-coordinate of the vector.
 */
QVariant vec4toVariant(const glm::vec4& vec4);
glm::vec4 vec4FromVariant(const QVariant &object, bool& valid);
glm::vec4 vec4FromVariant(const QVariant &object);

// Quaternions
QVariant quatToVariant(const glm::quat& quat);
glm::quat quatFromVariant(const QVariant &object, bool& isValid);
glm::quat quatFromVariant(const QVariant &object);

/*@jsdoc
 * Defines a rectangular portion of an image or screen, or similar.
 * @typedef {object} Rect
 * @property {number} x - Left, x-coordinate value.
 * @property {number} y - Top, y-coordinate value.
 * @property {number} width - Width of the rectangle.
 * @property {number} height - Height of the rectangle.
 */
QRect qRectFromVariant(const QVariant& object, bool& isValid);
QRect qRectFromVariant(const QVariant& object);
QVariant qRectToVariant(const QRect& rect);

QRectF qRectFFromVariant(const QVariant& object, bool& isValid);
QRectF qRectFFromVariant(const QVariant& object);
QVariant qRectFToVariant(const QRectF& rect);

// MathPicks also have to overide operator== for their type
class MathPick {
public:
    virtual ~MathPick() {}
    virtual operator bool() const = 0;
    virtual QVariantMap toVariantMap() const = 0;
};

/*@jsdoc
 * A vector with a starting point. It is used, for example, when finding entities or avatars that lie under a mouse click or 
 * intersect a laser beam.
 *
 * @typedef {object} PickRay
 * @property {Vec3} origin - The starting position of the ray.
 * @property {Vec3} direction - The direction that the ray travels.
 */
class PickRay : public MathPick {
public:
    PickRay() : origin(NAN), direction(NAN)  { }
    PickRay(const QVariantMap& pickVariant) : origin(vec3FromVariant(pickVariant["origin"])), direction(vec3FromVariant(pickVariant["direction"])) {}
    PickRay(const glm::vec3& origin, const glm::vec3 direction) : origin(origin), direction(direction) {}
    glm::vec3 origin;
    glm::vec3 direction;

    operator bool() const override {
        return !(glm::any(glm::isnan(origin)) || glm::any(glm::isnan(direction)));
    }
    bool operator==(const PickRay& other) const {
        return (origin == other.origin && direction == other.direction);
    }
    QVariantMap toVariantMap() const override {
        QVariantMap pickRay;
        pickRay["origin"] = vec3toVariant(origin);
        pickRay["direction"] = vec3toVariant(direction);
        return pickRay;
    }
};
Q_DECLARE_METATYPE(PickRay)

/*@jsdoc
 * The tip of a stylus.
 *
 * @typedef {object} StylusTip
 * @property {number} side - The hand that the stylus is attached to: <code>0</code> for left hand, <code>1</code> for the 
 *     right hand, <code>-1</code> for invalid.
 * @property {Vec3} tipOffset - The position of the stylus tip relative to the body of the stylus.
 * @property {Vec3} position - The position of the stylus tip.
 * @property {Quat} orientation - The orientation of the stylus.
 * @property {Vec3} velocity - The velocity of the stylus tip.
 */
class StylusTip : public MathPick {
public:
    StylusTip() : position(NAN), velocity(NAN) {}
    StylusTip(const bilateral::Side& side, const glm::vec3& tipOffset = Vectors::ZERO ,const glm::vec3& position = Vectors::ZERO,
              const glm::quat& orientation = Quaternions::IDENTITY, const glm::vec3& velocity = Vectors::ZERO) :
        side(side), tipOffset(tipOffset), position(position), orientation(orientation), velocity(velocity) {}
    StylusTip(const QVariantMap& pickVariant) : side(bilateral::Side(pickVariant["side"].toInt())), tipOffset(vec3FromVariant(pickVariant["tipOffset"])),
        position(vec3FromVariant(pickVariant["position"])), orientation(quatFromVariant(pickVariant["orientation"])), velocity(vec3FromVariant(pickVariant["velocity"])) {}

    bilateral::Side side { bilateral::Side::Invalid };
    glm::vec3 tipOffset;
    glm::vec3 position;
    glm::quat orientation;
    glm::vec3 velocity;

    operator bool() const override { return side != bilateral::Side::Invalid; }

    bool operator==(const StylusTip& other) const {
        return (side == other.side && tipOffset == other.tipOffset && position == other.position && orientation == other.orientation && velocity == other.velocity);
    }

    QVariantMap toVariantMap() const override {
        QVariantMap stylusTip;
        stylusTip["side"] = (int)side;
        stylusTip["tipOffset"] = vec3toVariant(tipOffset);
        stylusTip["position"] = vec3toVariant(position);
        stylusTip["orientation"] = quatToVariant(orientation);
        stylusTip["velocity"] = vec3toVariant(velocity);
        return stylusTip;
    }
};

/*@jsdoc
* A parabola defined by a starting point, initial velocity, and acceleration. It is used, for example, when finding entities or
* avatars that intersect a parabolic beam.
*
* @typedef {object} PickParabola
* @property {Vec3} origin - The starting position of the parabola, i.e., the initial position of a virtual projectile whose 
*     trajectory defines the parabola.
* @property {Vec3} velocity - The starting velocity of the parabola in m/s, i.e., the initial speed of a virtual projectile 
*     whose trajectory defines the parabola.
* @property {Vec3} acceleration - The acceleration that the parabola experiences in m/s<sup>2</sup>, i.e., the acceleration of 
*     a virtual projectile whose trajectory defines the parabola, both magnitude and direction.
*/
class PickParabola : public MathPick {
public:
    PickParabola() : origin(NAN), velocity(NAN), acceleration(NAN) { }
    PickParabola(const QVariantMap& pickVariant) : origin(vec3FromVariant(pickVariant["origin"])), velocity(vec3FromVariant(pickVariant["velocity"])), acceleration(vec3FromVariant(pickVariant["acceleration"])) {}
    PickParabola(const glm::vec3& origin, const glm::vec3 velocity, const glm::vec3 acceleration) : origin(origin), velocity(velocity), acceleration(acceleration) {}
    glm::vec3 origin;
    glm::vec3 velocity;
    glm::vec3 acceleration;

    operator bool() const override {
        return !(glm::any(glm::isnan(origin)) || glm::any(glm::isnan(velocity)) || glm::any(glm::isnan(acceleration)));
    }
    bool operator==(const PickParabola& other) const {
        return (origin == other.origin && velocity == other.velocity && acceleration == other.acceleration);
    }
    QVariantMap toVariantMap() const override {
        QVariantMap pickParabola;
        pickParabola["origin"] = vec3toVariant(origin);
        pickParabola["velocity"] = vec3toVariant(velocity);
        pickParabola["acceleration"] = vec3toVariant(acceleration);
        return pickParabola;
    }
};

class CollisionRegion : public MathPick {
public:
    CollisionRegion() { }

    CollisionRegion(const CollisionRegion& collisionRegion) :
        loaded(collisionRegion.loaded),
        modelURL(collisionRegion.modelURL),
        shapeInfo(std::make_shared<ShapeInfo>()),
        transform(collisionRegion.transform),
        threshold(collisionRegion.threshold),
        collisionGroup(collisionRegion.collisionGroup)
    {
        shapeInfo->setParams(collisionRegion.shapeInfo->getType(), collisionRegion.shapeInfo->getHalfExtents(), collisionRegion.modelURL.toString());
    }

    CollisionRegion(const QVariantMap& pickVariant) {
        // "loaded" is not deserialized here because there is no way to know if the shape is actually loaded
        if (pickVariant["shape"].isValid()) {
            auto shape = pickVariant["shape"].toMap();
            if (!shape.empty()) {
                ShapeType shapeType = SHAPE_TYPE_NONE;
                if (shape["shapeType"].isValid()) {
                    shapeType = ShapeInfo::getShapeTypeForName(shape["shapeType"].toString());
                }
                if (shapeType >= SHAPE_TYPE_COMPOUND && shapeType <= SHAPE_TYPE_STATIC_MESH && shape["modelURL"].isValid()) {
                    QString newURL = shape["modelURL"].toString();
                    modelURL.setUrl(newURL);
                } else {
                    modelURL.setUrl("");
                }

                if (shape["dimensions"].isValid()) {
                    transform.setScale(vec3FromVariant(shape["dimensions"]));
                }

                shapeInfo->setParams(shapeType, transform.getScale() / 2.0f, modelURL.toString());
            }
        }

        if (pickVariant["threshold"].isValid()) {
            threshold = glm::max(0.0f, pickVariant["threshold"].toFloat());
        }

        if (pickVariant["position"].isValid()) {
            transform.setTranslation(vec3FromVariant(pickVariant["position"]));
        }
        if (pickVariant["orientation"].isValid()) {
            transform.setRotation(quatFromVariant(pickVariant["orientation"]));
        }
        if (pickVariant["collisionGroup"].isValid()) {
            collisionGroup = pickVariant["collisionGroup"].toUInt();
        }
    }

    /*@jsdoc
     * A volume for checking collisions in the physics simulation.
     * @typedef {object} CollisionRegion
     * @property {Shape} shape - The collision region's shape and size. Dimensions are in world coordinates, but scale with the 
     *     parent if defined.
     * @property {boolean} loaded - <code>true</code> if the <code>shape</code> has no model, or has a model and it is loaded, 
     *     <code>false</code> if otherwise.
     * @property {Vec3} position - The position of the collision region, relative to the parent if defined.
     * @property {Quat} orientation - The orientation of the collision region, relative to the parent if defined.
     * @property {number} threshold - The approximate minimum penetration depth for a test object to be considered in contact with
     *     the collision region. The depth is in world coordinates but scales with the parent if defined.
     * @property {CollisionMask} [collisionGroup=8] - The type of objects the collision region collides as. Objects whose collision
     *     masks overlap with the region's collision group are considered to be colliding with the region.
     */

    /*@jsdoc
     * A physical volume.
     * @typedef {object} Shape
     * @property {ShapeType} shapeType="none" - The type of shape.
     * @property {string} [modelUrl=""] - The model to load to for the shape if <code>shapeType</code> is one of
     *     <code>"compound"</code>, <code>"simple-hull"</code>, <code>"simple-compound"</code>, or <code>"static-mesh"</code>.
     * @property {Vec3} dimensions - The dimensions of the shape.
     */

    QVariantMap toVariantMap() const override {
        QVariantMap collisionRegion;

        QVariantMap shape;
        shape["shapeType"] = ShapeInfo::getNameForShapeType(shapeInfo->getType());
        shape["modelURL"] = modelURL.toString();
        shape["dimensions"] = vec3toVariant(transform.getScale());

        collisionRegion["shape"] = shape;
        collisionRegion["loaded"] = loaded;

        collisionRegion["threshold"] = threshold;
        collisionRegion["collisionGroup"] = collisionGroup;

        collisionRegion["position"] = vec3toVariant(transform.getTranslation());
        collisionRegion["orientation"] = quatToVariant(transform.getRotation());

        return collisionRegion;
    }

    operator bool() const override {
        return !std::isnan(threshold) &&
            !(glm::any(glm::isnan(transform.getTranslation())) ||
            glm::any(glm::isnan(transform.getRotation())) ||
            shapeInfo->getType() == SHAPE_TYPE_NONE ||
            collisionGroup == 0);
    }

    bool operator==(const CollisionRegion& other) const {
        return loaded == other.loaded &&
            threshold == other.threshold &&
            collisionGroup == other.collisionGroup &&
            glm::all(glm::equal(transform.getTranslation(), other.transform.getTranslation())) &&
            glm::all(glm::equal(transform.getRotation(), other.transform.getRotation())) &&
            glm::all(glm::equal(transform.getScale(), other.transform.getScale())) &&
            shapeInfo->getType() == other.shapeInfo->getType() &&
            modelURL == other.modelURL;
    }

    bool shouldComputeShapeInfo() const {
        if (!(shapeInfo->getType() == SHAPE_TYPE_HULL ||
            (shapeInfo->getType() >= SHAPE_TYPE_COMPOUND &&
                shapeInfo->getType() <= SHAPE_TYPE_STATIC_MESH)
            )) {
            return false;
        }

        if (collisionGroup == 0) {
            return false;
        }

        return !shapeInfo->getPointCollection().size();
    }

    // We can't load the model here because it would create a circular dependency, so we delegate that responsibility to the owning CollisionPick
    bool loaded { false };
    QUrl modelURL;

    // We can't compute the shapeInfo here without loading the model first, so we delegate that responsibility to the owning CollisionPick
    std::shared_ptr<ShapeInfo> shapeInfo = std::make_shared<ShapeInfo>();
    Transform transform;
    float threshold { 0.0f };
    uint16_t collisionGroup { USER_COLLISION_GROUP_MY_AVATAR };
};

namespace std {
    inline void hash_combine(std::size_t& seed) { }

    template <typename T, typename... Rest>
    inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        hash_combine(seed, rest...);
    }

    template <>
    struct hash<bilateral::Side> {
        size_t operator()(const bilateral::Side& a) const {
            return std::hash<int>()((int)a);
        }
    };

    template <>
    struct hash<glm::vec3> {
        size_t operator()(const glm::vec3& a) const {
            size_t result = 0;
            hash_combine(result, a.x, a.y, a.z);
            return result;
        }
    };

    template <>
    struct hash<glm::quat> {
        size_t operator()(const glm::quat& a) const {
            size_t result = 0;
            hash_combine(result, a.x, a.y, a.z, a.w);
            return result;
        }
    };

    template <>
    struct hash<Transform> {
        size_t operator()(const Transform& a) const {
            size_t result = 0;
            hash_combine(result, a.getTranslation(), a.getRotation(), a.getScale());
            return result;
        }
    };

    template <>
    struct hash<PickRay> {
        size_t operator()(const PickRay& a) const {
            size_t result = 0;
            hash_combine(result, a.origin, a.direction);
            return result;
        }
    };

    template <>
    struct hash<StylusTip> {
        size_t operator()(const StylusTip& a) const {
            size_t result = 0;
            hash_combine(result, a.side, a.position, a.orientation, a.velocity);
            return result;
        }
    };

    template <>
    struct hash<PickParabola> {
        size_t operator()(const PickParabola& a) const {
            size_t result = 0;
            hash_combine(result, a.origin, a.velocity, a.acceleration);
            return result;
        }
    };

    template <>
    struct hash<CollisionRegion> {
        size_t operator()(const CollisionRegion& a) const {
            size_t result = 0;
            hash_combine(result, a.transform, (int)a.shapeInfo->getType(), qHash(a.modelURL));
            return result;
        }
    };

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    template <>
    struct hash<QString> {
        size_t operator()(const QString& a) const {
            return qHash(a);
        }
    };
#endif
}

/*@jsdoc
 * <p>The type of a collision contact event.</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>Start of the collision.</td></tr>
 *     <tr><td><code>1</code></td><td>Continuation of the collision.</td></tr>
 *     <tr><td><code>2</code></td><td>End of the collision.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {number} ContactEventType
 */
enum ContactEventType {
    CONTACT_EVENT_TYPE_START,
    CONTACT_EVENT_TYPE_CONTINUE,
    CONTACT_EVENT_TYPE_END
};

class Collision {
public:
    Collision() : type(CONTACT_EVENT_TYPE_START), idA(), idB(), contactPoint(0.0f), penetration(0.0f), velocityChange(0.0f) { }
    Collision(ContactEventType cType, const QUuid& cIdA, const QUuid& cIdB, const glm::vec3& cPoint,
              const glm::vec3& cPenetration, const glm::vec3& velocityChange)
    :   type(cType), idA(cIdA), idB(cIdB), contactPoint(cPoint), penetration(cPenetration), velocityChange(velocityChange) { }

    void invert(); // swap A and B

    ContactEventType type;
    QUuid idA;
    QUuid idB;
    glm::vec3 contactPoint; // on B in world-frame
    glm::vec3 penetration; // from B towards A in world-frame
    glm::vec3 velocityChange;
};
Q_DECLARE_METATYPE(Collision)

class AnimationDetails {
public:
    AnimationDetails();
    AnimationDetails(QString role, QUrl url, float fps, float priority, bool loop,
        bool hold, bool startAutomatically, float firstFrame, float lastFrame, bool running, float currentFrame, bool allowTranslation);

    QString role;
    QUrl url;
    float fps;
    float priority;
    bool loop;
    bool hold;
    bool startAutomatically;
    float firstFrame;
    float lastFrame;
    bool running;
    float currentFrame;
    bool allowTranslation;
};
Q_DECLARE_METATYPE(AnimationDetails);

namespace graphics {
    class Mesh;
}

using MeshPointer = std::shared_ptr<graphics::Mesh>;

/*@jsdoc
 * A mesh, such as returned by {@link Entities.getMeshes} or {@link Model} API functions.
 *
 * @class MeshProxy
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @deprecated Use the {@link Graphics} API instead.
 */
class MeshProxy : public QObject {
    Q_OBJECT

public:
    virtual MeshPointer getMeshPointer() const = 0;
    
    /*@jsdoc
     * Gets the number of vertices in the mesh.
     * @function MeshProxy#getNumVertices
     * @returns {number} Integer number of vertices in the mesh.
     */
    Q_INVOKABLE virtual int getNumVertices() const = 0;

    /*@jsdoc
     * Gets the position of a vertex in the mesh.
     * @function MeshProxy#getPos
     * @param {number} index - Integer index of the vertex.
     * @returns {Vec3} Local position of the vertex relative to the mesh.
     */
    Q_INVOKABLE virtual glm::vec3 getPos(int index) const = 0;
    Q_INVOKABLE virtual glm::vec3 getPos3(int index) const { return getPos(index); } // deprecated
};

Q_DECLARE_METATYPE(MeshProxy*);

class MeshProxyList : public QList<MeshProxy*> {}; // typedef and using fight with the Qt macros/templates, do this instead
Q_DECLARE_METATYPE(MeshProxyList);


class MeshFace {

public:
    MeshFace() {}
    ~MeshFace() {}

    QVector<uint32_t> vertexIndices;
    // TODO -- material...
};

Q_DECLARE_METATYPE(MeshFace)
Q_DECLARE_METATYPE(QVector<MeshFace>)

QVariantMap parseTexturesToMap(QString textures, const QVariantMap& defaultTextures);

Q_DECLARE_METATYPE(StencilMaskMode)

#endif // hifi_RegisteredMetaTypes_h
