//
//  EntityItemProperties.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityItemProperties_h
#define hifi_EntityItemProperties_h

#include <stdint.h>

#include <limits>
#include <type_traits>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include <QtCore/QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QtCore/QSharedPointer>

#include <AACube.h>
#include <NumericalConstants.h>
#include <PropertyFlags.h>
#include <OctreeConstants.h>
#include <ShapeInfo.h>
#include <ColorUtils.h>
#include "FontFamilies.h"

#include "EntityItemID.h"
#include "EntityItemPropertiesDefaults.h"
#include "EntityItemPropertiesMacros.h"
#include "EntityTypes.h"
#include "EntityPropertyFlags.h"
#include "EntityPsuedoPropertyFlags.h"
#include "SimulationOwner.h"

#include "TextEntityItem.h"
#include "WebEntityItem.h"
#include "ParticleEffectEntityItem.h"
#include "LineEntityItem.h"
#include "PolyVoxEntityItem.h"
#include "GridEntityItem.h"
#include "GizmoEntityItem.h"
#include "LightEntityItem.h"
#include "ZoneEntityItem.h"

#include "AnimationPropertyGroup.h"
#include "SkyboxPropertyGroup.h"
#include "HazePropertyGroup.h"
#include "BloomPropertyGroup.h"
#include "PulsePropertyGroup.h"
#include "RingGizmoPropertyGroup.h"

#include "MaterialMappingMode.h"
#include "BillboardMode.h"
#include "RenderLayer.h"
#include "PrimitiveMode.h"
#include "WebInputMode.h"
#include "GizmoType.h"
#include "TextEffect.h"
#include "TextAlignment.h"

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

const quint64 UNKNOWN_CREATED_TIME = 0;

using vec3Color = glm::vec3;
using u8vec3Color = glm::u8vec3;

struct EntityPropertyInfo {
    EntityPropertyInfo(EntityPropertyList propEnum) :
        propertyEnum(propEnum) {}
    EntityPropertyInfo(EntityPropertyList propEnum, QVariant min, QVariant max) :
        propertyEnum(propEnum), minimum(min), maximum(max) {}
    EntityPropertyInfo() = default;
    EntityPropertyList propertyEnum;
    QVariant minimum;
    QVariant maximum;
};

template <typename T>
EntityPropertyInfo makePropertyInfo(EntityPropertyList p, typename std::enable_if<!std::is_integral<T>::value>::type* = 0) {
    return EntityPropertyInfo(p);
}

template <typename T>
EntityPropertyInfo makePropertyInfo(EntityPropertyList p, typename std::enable_if<std::is_integral<T>::value>::type* = 0) {
    return EntityPropertyInfo(p, std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

/// A collection of properties of an entity item used in the scripting API. Translates between the actual properties of an
/// entity and a JavaScript style hash/ScriptValuePointer storing a set of properties. Used in scripting to set/get the complete
/// set of entity item properties via JavaScript hashes/QScriptValues
/// all units for SI units (meter, second, radian, etc) 
class EntityItemProperties {
    // TODO: consider removing these friend relationship and use public methods
    friend class EntityItem;
    friend class BoxEntityItem;
    friend class SphereEntityItem;
    friend class ShapeEntityItem;
    friend class ModelEntityItem;
    friend class TextEntityItem;
    friend class ImageEntityItem;
    friend class WebEntityItem;
    friend class ParticleEffectEntityItem;
    friend class LineEntityItem;
    friend class PolyLineEntityItem;
    friend class PolyVoxEntityItem;
    friend class GridEntityItem;
    friend class GizmoEntityItem;
    friend class LightEntityItem;
    friend class ZoneEntityItem;
    friend class MaterialEntityItem;
public:
    static bool blobToProperties(ScriptEngine& scriptEngine, const QByteArray& blob, EntityItemProperties& properties);
    static void propertiesToBlob(ScriptEngine& scriptEngine, const QUuid& myAvatarID, const EntityItemProperties& properties, 
        QByteArray& blob, bool allProperties = false);

    EntityItemProperties(EntityPropertyFlags desiredProperties = EntityPropertyFlags());
    EntityItemProperties(const EntityItemProperties&) = default;

    virtual ~EntityItemProperties() = default;

    void merge(const EntityItemProperties& other);

    EntityTypes::EntityType getType() const { return _type; }
    void setType(EntityTypes::EntityType type) { _type = type; }

    virtual ScriptValuePointer copyToScriptValue(ScriptEngine* engine, bool skipDefaults, bool allowUnknownCreateTime = false,
        bool strictSemantics = false, EntityPsuedoPropertyFlags psueudoPropertyFlags = EntityPsuedoPropertyFlags()) const;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool honorReadOnly);
    void copyFromJSONString(ScriptEngine& scriptEngine, const QString& jsonString);

    static ScriptValuePointer entityPropertyFlagsToScriptValue(ScriptEngine* engine, const EntityPropertyFlags& flags);
    static void entityPropertyFlagsFromScriptValue(const ScriptValuePointer& object, EntityPropertyFlags& flags);

    static bool getPropertyInfo(const QString& propertyName, EntityPropertyInfo& propertyInfo);

    // editing related features supported by all entities
    quint64 getLastEdited() const { return _lastEdited; }
    float getEditedAgo() const /// Elapsed seconds since this entity was last edited
        { return (float)(usecTimestampNow() - getLastEdited()) / (float)USECS_PER_SECOND; }

    EntityPropertyFlags getChangedProperties() const;

    bool transformChanged() const;
    bool getScalesWithParent() const;
    bool parentRelatedPropertyChanged() const;
    bool queryAACubeRelatedPropertyChanged() const;
    bool grabbingRelatedPropertyChanged() const;

    AABox getAABox() const;

    void debugDump() const;
    void setLastEdited(quint64 usecTime);
    EntityPropertyFlags getDesiredProperties() { return _desiredProperties; }
    void setDesiredProperties(EntityPropertyFlags properties) {  _desiredProperties = properties; }

    bool constructFromBuffer(const unsigned char* data, int dataLength);

    // Note:  DEFINE_PROPERTY(PROP_FOO, Foo, foo, type, value) creates the following methods and variables:
    // type getFoo() const;
    // void setFoo(type);
    // bool fooChanged() const;
    // type _foo { value };
    // bool _fooChanged { false };

    // Core Properties
    DEFINE_PROPERTY_REF(PROP_SIMULATION_OWNER, SimulationOwner, simulationOwner, SimulationOwner, SimulationOwner());
    DEFINE_PROPERTY_REF(PROP_PARENT_ID, ParentID, parentID, QUuid, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_PARENT_JOINT_INDEX, ParentJointIndex, parentJointIndex, quint16, -1);
    DEFINE_PROPERTY(PROP_VISIBLE, Visible, visible, bool, ENTITY_ITEM_DEFAULT_VISIBLE);
    DEFINE_PROPERTY_REF(PROP_NAME, Name, name, QString, ENTITY_ITEM_DEFAULT_NAME);
    DEFINE_PROPERTY(PROP_LOCKED, Locked, locked, bool, ENTITY_ITEM_DEFAULT_LOCKED);
    DEFINE_PROPERTY_REF(PROP_USER_DATA, UserData, userData, QString, ENTITY_ITEM_DEFAULT_USER_DATA);
    DEFINE_PROPERTY_REF(PROP_PRIVATE_USER_DATA, PrivateUserData, privateUserData, QString, ENTITY_ITEM_DEFAULT_PRIVATE_USER_DATA);
    DEFINE_PROPERTY_REF(PROP_HREF, Href, href, QString, "");
    DEFINE_PROPERTY_REF(PROP_DESCRIPTION, Description, description, QString, "");
    DEFINE_PROPERTY_REF_WITH_SETTER(PROP_POSITION, Position, position, glm::vec3, ENTITY_ITEM_ZERO_VEC3);
    DEFINE_PROPERTY_REF(PROP_DIMENSIONS, Dimensions, dimensions, glm::vec3, ENTITY_ITEM_DEFAULT_DIMENSIONS);
    DEFINE_PROPERTY_REF(PROP_ROTATION, Rotation, rotation, glm::quat, ENTITY_ITEM_DEFAULT_ROTATION);
    DEFINE_PROPERTY_REF(PROP_REGISTRATION_POINT, RegistrationPoint, registrationPoint, glm::vec3, ENTITY_ITEM_DEFAULT_REGISTRATION_POINT);
    DEFINE_PROPERTY(PROP_CREATED, Created, created, quint64, UNKNOWN_CREATED_TIME);
    DEFINE_PROPERTY_REF(PROP_LAST_EDITED_BY, LastEditedBy, lastEditedBy, QUuid, ENTITY_ITEM_DEFAULT_LAST_EDITED_BY);
    DEFINE_PROPERTY_REF_ENUM(PROP_ENTITY_HOST_TYPE, EntityHostType, entityHostType, entity::HostType, entity::HostType::DOMAIN);
    DEFINE_PROPERTY_REF_WITH_SETTER(PROP_OWNING_AVATAR_ID, OwningAvatarID, owningAvatarID, QUuid, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_QUERY_AA_CUBE, QueryAACube, queryAACube, AACube, AACube());
    DEFINE_PROPERTY(PROP_CAN_CAST_SHADOW, CanCastShadow, canCastShadow, bool, ENTITY_ITEM_DEFAULT_CAN_CAST_SHADOW);
    DEFINE_PROPERTY(PROP_VISIBLE_IN_SECONDARY_CAMERA, IsVisibleInSecondaryCamera, isVisibleInSecondaryCamera, bool, ENTITY_ITEM_DEFAULT_VISIBLE_IN_SECONDARY_CAMERA);
    DEFINE_PROPERTY_REF_ENUM(PROP_RENDER_LAYER, RenderLayer, renderLayer, RenderLayer, RenderLayer::WORLD);
    DEFINE_PROPERTY_REF_ENUM(PROP_PRIMITIVE_MODE, PrimitiveMode, primitiveMode, PrimitiveMode, PrimitiveMode::SOLID);
    DEFINE_PROPERTY(PROP_IGNORE_PICK_INTERSECTION, IgnorePickIntersection, ignorePickIntersection, bool, false);
    DEFINE_PROPERTY_REF(PROP_RENDER_WITH_ZONES, RenderWithZones, renderWithZones, QVector<QUuid>, QVector<QUuid>());
    DEFINE_PROPERTY_REF_ENUM(PROP_BILLBOARD_MODE, BillboardMode, billboardMode, BillboardMode, BillboardMode::NONE);
    DEFINE_PROPERTY_GROUP(Grab, grab, GrabPropertyGroup);

    // Physics
    DEFINE_PROPERTY(PROP_DENSITY, Density, density, float, ENTITY_ITEM_DEFAULT_DENSITY);
    DEFINE_PROPERTY_REF(PROP_VELOCITY, Velocity, velocity, glm::vec3, ENTITY_ITEM_DEFAULT_VELOCITY);
    DEFINE_PROPERTY_REF(PROP_ANGULAR_VELOCITY, AngularVelocity, angularVelocity, glm::vec3, ENTITY_ITEM_DEFAULT_ANGULAR_VELOCITY);
    DEFINE_PROPERTY_REF(PROP_GRAVITY, Gravity, gravity, glm::vec3, ENTITY_ITEM_DEFAULT_GRAVITY);
    DEFINE_PROPERTY_REF(PROP_ACCELERATION, Acceleration, acceleration, glm::vec3, ENTITY_ITEM_DEFAULT_ACCELERATION);
    DEFINE_PROPERTY(PROP_DAMPING, Damping, damping, float, ENTITY_ITEM_DEFAULT_DAMPING);
    DEFINE_PROPERTY(PROP_ANGULAR_DAMPING, AngularDamping, angularDamping, float, ENTITY_ITEM_DEFAULT_ANGULAR_DAMPING);
    DEFINE_PROPERTY(PROP_RESTITUTION, Restitution, restitution, float, ENTITY_ITEM_DEFAULT_RESTITUTION);
    DEFINE_PROPERTY(PROP_FRICTION, Friction, friction, float, ENTITY_ITEM_DEFAULT_FRICTION);
    DEFINE_PROPERTY(PROP_LIFETIME, Lifetime, lifetime, float, ENTITY_ITEM_DEFAULT_LIFETIME);
    DEFINE_PROPERTY(PROP_COLLISIONLESS, Collisionless, collisionless, bool, ENTITY_ITEM_DEFAULT_COLLISIONLESS);
    DEFINE_PROPERTY(PROP_COLLISION_MASK, CollisionMask, collisionMask, uint16_t, ENTITY_COLLISION_MASK_DEFAULT);
    DEFINE_PROPERTY(PROP_DYNAMIC, Dynamic, dynamic, bool, ENTITY_ITEM_DEFAULT_DYNAMIC);
    DEFINE_PROPERTY_REF(PROP_COLLISION_SOUND_URL, CollisionSoundURL, collisionSoundURL, QString, ENTITY_ITEM_DEFAULT_COLLISION_SOUND_URL);
    DEFINE_PROPERTY_REF(PROP_ACTION_DATA, ActionData, actionData, QByteArray, QByteArray());

    // Cloning
    DEFINE_PROPERTY(PROP_CLONEABLE, Cloneable, cloneable, bool, ENTITY_ITEM_DEFAULT_CLONEABLE);
    DEFINE_PROPERTY(PROP_CLONE_LIFETIME, CloneLifetime, cloneLifetime, float, ENTITY_ITEM_DEFAULT_CLONE_LIFETIME);
    DEFINE_PROPERTY(PROP_CLONE_LIMIT, CloneLimit, cloneLimit, float, ENTITY_ITEM_DEFAULT_CLONE_LIMIT);
    DEFINE_PROPERTY(PROP_CLONE_DYNAMIC, CloneDynamic, cloneDynamic, bool, ENTITY_ITEM_DEFAULT_CLONE_DYNAMIC);
    DEFINE_PROPERTY(PROP_CLONE_AVATAR_ENTITY, CloneAvatarEntity, cloneAvatarEntity, bool, ENTITY_ITEM_DEFAULT_CLONE_AVATAR_ENTITY);
    DEFINE_PROPERTY_REF(PROP_CLONE_ORIGIN_ID, CloneOriginID, cloneOriginID, QUuid, ENTITY_ITEM_DEFAULT_CLONE_ORIGIN_ID);

    // Scripts
    DEFINE_PROPERTY_REF(PROP_SCRIPT, Script, script, QString, ENTITY_ITEM_DEFAULT_SCRIPT);
    DEFINE_PROPERTY(PROP_SCRIPT_TIMESTAMP, ScriptTimestamp, scriptTimestamp, quint64, ENTITY_ITEM_DEFAULT_SCRIPT_TIMESTAMP);
    DEFINE_PROPERTY_REF(PROP_SERVER_SCRIPTS, ServerScripts, serverScripts, QString, ENTITY_ITEM_DEFAULT_SERVER_SCRIPTS);

    // Certifiable Properties - related to Proof of Purchase certificates
    DEFINE_PROPERTY_REF(PROP_ITEM_NAME, ItemName, itemName, QString, ENTITY_ITEM_DEFAULT_ITEM_NAME);
    DEFINE_PROPERTY_REF(PROP_ITEM_DESCRIPTION, ItemDescription, itemDescription, QString, ENTITY_ITEM_DEFAULT_ITEM_DESCRIPTION);
    DEFINE_PROPERTY_REF(PROP_ITEM_CATEGORIES, ItemCategories, itemCategories, QString, ENTITY_ITEM_DEFAULT_ITEM_CATEGORIES);
    DEFINE_PROPERTY_REF(PROP_ITEM_ARTIST, ItemArtist, itemArtist, QString, ENTITY_ITEM_DEFAULT_ITEM_ARTIST);
    DEFINE_PROPERTY_REF(PROP_ITEM_LICENSE, ItemLicense, itemLicense, QString, ENTITY_ITEM_DEFAULT_ITEM_LICENSE);
    DEFINE_PROPERTY_REF(PROP_LIMITED_RUN, LimitedRun, limitedRun, quint32, ENTITY_ITEM_DEFAULT_LIMITED_RUN);
    DEFINE_PROPERTY_REF(PROP_MARKETPLACE_ID, MarketplaceID, marketplaceID, QString, ENTITY_ITEM_DEFAULT_MARKETPLACE_ID);
    DEFINE_PROPERTY_REF(PROP_EDITION_NUMBER, EditionNumber, editionNumber, quint32, ENTITY_ITEM_DEFAULT_EDITION_NUMBER);
    DEFINE_PROPERTY_REF(PROP_ENTITY_INSTANCE_NUMBER, EntityInstanceNumber, entityInstanceNumber, quint32, ENTITY_ITEM_DEFAULT_ENTITY_INSTANCE_NUMBER);
    DEFINE_PROPERTY_REF(PROP_CERTIFICATE_ID, CertificateID, certificateID, QString, ENTITY_ITEM_DEFAULT_CERTIFICATE_ID);
    DEFINE_PROPERTY_REF(PROP_CERTIFICATE_TYPE, CertificateType, certificateType, QString, ENTITY_ITEM_DEFAULT_CERTIFICATE_TYPE);
    DEFINE_PROPERTY_REF(PROP_STATIC_CERTIFICATE_VERSION, StaticCertificateVersion, staticCertificateVersion, quint32, ENTITY_ITEM_DEFAULT_STATIC_CERTIFICATE_VERSION);

    // these are used when bouncing location data into and out of scripts
    DEFINE_PROPERTY_REF(PROP_LOCAL_POSITION, LocalPosition, localPosition, glm::vec3, ENTITY_ITEM_ZERO_VEC3);
    DEFINE_PROPERTY_REF(PROP_LOCAL_ROTATION, LocalRotation, localRotation, quat, ENTITY_ITEM_DEFAULT_ROTATION);
    DEFINE_PROPERTY_REF(PROP_LOCAL_VELOCITY, LocalVelocity, localVelocity, glm::vec3, ENTITY_ITEM_ZERO_VEC3);
    DEFINE_PROPERTY_REF(PROP_LOCAL_ANGULAR_VELOCITY, LocalAngularVelocity, localAngularVelocity, glm::vec3, ENTITY_ITEM_ZERO_VEC3);
    DEFINE_PROPERTY_REF(PROP_LOCAL_DIMENSIONS, LocalDimensions, localDimensions, glm::vec3, ENTITY_ITEM_ZERO_VEC3);

    // Common
    DEFINE_PROPERTY_REF_ENUM(PROP_SHAPE_TYPE, ShapeType, shapeType, ShapeType, SHAPE_TYPE_NONE);
    DEFINE_PROPERTY_REF(PROP_COMPOUND_SHAPE_URL, CompoundShapeURL, compoundShapeURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_COLOR, Color, color, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
    DEFINE_PROPERTY(PROP_ALPHA, Alpha, alpha, float, ENTITY_ITEM_DEFAULT_ALPHA);
    DEFINE_PROPERTY_GROUP(Pulse, pulse, PulsePropertyGroup);
    DEFINE_PROPERTY_REF(PROP_TEXTURES, Textures, textures, QString, "");

    // Particles
    DEFINE_PROPERTY(PROP_MAX_PARTICLES, MaxParticles, maxParticles, quint32, particle::DEFAULT_MAX_PARTICLES);
    DEFINE_PROPERTY(PROP_LIFESPAN, Lifespan, lifespan, float, particle::DEFAULT_LIFESPAN);
    DEFINE_PROPERTY(PROP_EMITTING_PARTICLES, IsEmitting, isEmitting, bool, true);
    DEFINE_PROPERTY(PROP_EMIT_RATE, EmitRate, emitRate, float, particle::DEFAULT_EMIT_RATE);
    DEFINE_PROPERTY(PROP_EMIT_SPEED, EmitSpeed, emitSpeed, float, particle::DEFAULT_EMIT_SPEED);
    DEFINE_PROPERTY(PROP_SPEED_SPREAD, SpeedSpread, speedSpread, float, particle::DEFAULT_SPEED_SPREAD);
    DEFINE_PROPERTY_REF(PROP_EMIT_ORIENTATION, EmitOrientation, emitOrientation, glm::quat, particle::DEFAULT_EMIT_ORIENTATION);
    DEFINE_PROPERTY_REF(PROP_EMIT_DIMENSIONS, EmitDimensions, emitDimensions, glm::vec3, particle::DEFAULT_EMIT_DIMENSIONS);
    DEFINE_PROPERTY(PROP_EMIT_RADIUS_START, EmitRadiusStart, emitRadiusStart, float, particle::DEFAULT_EMIT_RADIUS_START);
    DEFINE_PROPERTY(PROP_POLAR_START, PolarStart, polarStart, float, particle::DEFAULT_POLAR_START);
    DEFINE_PROPERTY(PROP_POLAR_FINISH, PolarFinish, polarFinish, float, particle::DEFAULT_POLAR_FINISH);
    DEFINE_PROPERTY(PROP_AZIMUTH_START, AzimuthStart, azimuthStart, float, particle::DEFAULT_AZIMUTH_START);
    DEFINE_PROPERTY(PROP_AZIMUTH_FINISH, AzimuthFinish, azimuthFinish, float, particle::DEFAULT_AZIMUTH_FINISH);
    DEFINE_PROPERTY_REF(PROP_EMIT_ACCELERATION, EmitAcceleration, emitAcceleration, glm::vec3, particle::DEFAULT_EMIT_ACCELERATION);
    DEFINE_PROPERTY_REF(PROP_ACCELERATION_SPREAD, AccelerationSpread, accelerationSpread, glm::vec3, particle::DEFAULT_ACCELERATION_SPREAD);
    DEFINE_PROPERTY(PROP_PARTICLE_RADIUS, ParticleRadius, particleRadius, float, particle::DEFAULT_PARTICLE_RADIUS);
    DEFINE_PROPERTY(PROP_RADIUS_SPREAD, RadiusSpread, radiusSpread, float, particle::DEFAULT_RADIUS_SPREAD);
    DEFINE_PROPERTY(PROP_RADIUS_START, RadiusStart, radiusStart, float, particle::DEFAULT_RADIUS_START);
    DEFINE_PROPERTY(PROP_RADIUS_FINISH, RadiusFinish, radiusFinish, float, particle::DEFAULT_RADIUS_FINISH);
    DEFINE_PROPERTY_REF(PROP_COLOR_SPREAD, ColorSpread, colorSpread, u8vec3Color, particle::DEFAULT_COLOR_SPREAD);
    DEFINE_PROPERTY_REF(PROP_COLOR_START, ColorStart, colorStart, glm::vec3, particle::DEFAULT_COLOR_UNINITIALIZED);
    DEFINE_PROPERTY_REF(PROP_COLOR_FINISH, ColorFinish, colorFinish, glm::vec3, particle::DEFAULT_COLOR_UNINITIALIZED);
    DEFINE_PROPERTY(PROP_ALPHA_SPREAD, AlphaSpread, alphaSpread, float, particle::DEFAULT_ALPHA_SPREAD);
    DEFINE_PROPERTY(PROP_ALPHA_START, AlphaStart, alphaStart, float, particle::DEFAULT_ALPHA_START);
    DEFINE_PROPERTY(PROP_ALPHA_FINISH, AlphaFinish, alphaFinish, float, particle::DEFAULT_ALPHA_FINISH);
    DEFINE_PROPERTY(PROP_EMITTER_SHOULD_TRAIL, EmitterShouldTrail, emitterShouldTrail, bool, particle::DEFAULT_EMITTER_SHOULD_TRAIL);
    DEFINE_PROPERTY(PROP_PARTICLE_SPIN, ParticleSpin, particleSpin, float, particle::DEFAULT_PARTICLE_SPIN);
    DEFINE_PROPERTY(PROP_SPIN_SPREAD, SpinSpread, spinSpread, float, particle::DEFAULT_SPIN_SPREAD);
    DEFINE_PROPERTY(PROP_SPIN_START, SpinStart, spinStart, float, particle::DEFAULT_SPIN_START);
    DEFINE_PROPERTY(PROP_SPIN_FINISH, SpinFinish, spinFinish, float, particle::DEFAULT_SPIN_FINISH);
    DEFINE_PROPERTY(PROP_PARTICLE_ROTATE_WITH_ENTITY, RotateWithEntity, rotateWithEntity, bool, particle::DEFAULT_ROTATE_WITH_ENTITY);

    // Model
    DEFINE_PROPERTY_REF(PROP_MODEL_URL, ModelURL, modelURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_MODEL_SCALE, ModelScale, modelScale, glm::vec3, glm::vec3(1.0f));
    DEFINE_PROPERTY_REF(PROP_JOINT_ROTATIONS_SET, JointRotationsSet, jointRotationsSet, QVector<bool>, QVector<bool>());
    DEFINE_PROPERTY_REF(PROP_JOINT_ROTATIONS, JointRotations, jointRotations, QVector<glm::quat>, QVector<glm::quat>());
    DEFINE_PROPERTY_REF(PROP_JOINT_TRANSLATIONS_SET, JointTranslationsSet, jointTranslationsSet, QVector<bool>, QVector<bool>());
    DEFINE_PROPERTY_REF(PROP_JOINT_TRANSLATIONS, JointTranslations, jointTranslations, QVector<glm::vec3>, ENTITY_ITEM_DEFAULT_EMPTY_VEC3_QVEC);
    DEFINE_PROPERTY(PROP_RELAY_PARENT_JOINTS, RelayParentJoints, relayParentJoints, bool, ENTITY_ITEM_DEFAULT_RELAY_PARENT_JOINTS);
    DEFINE_PROPERTY_REF(PROP_GROUP_CULLED, GroupCulled, groupCulled, bool, false);
    DEFINE_PROPERTY_REF(PROP_BLENDSHAPE_COEFFICIENTS, BlendshapeCoefficients, blendshapeCoefficients, QString, "");
    DEFINE_PROPERTY_REF(PROP_USE_ORIGINAL_PIVOT, UseOriginalPivot, useOriginalPivot, bool, false);
    DEFINE_PROPERTY_GROUP(Animation, animation, AnimationPropertyGroup);

    // Light
    DEFINE_PROPERTY(PROP_IS_SPOTLIGHT, IsSpotlight, isSpotlight, bool, LightEntityItem::DEFAULT_IS_SPOTLIGHT);
    DEFINE_PROPERTY(PROP_INTENSITY, Intensity, intensity, float, LightEntityItem::DEFAULT_INTENSITY);
    DEFINE_PROPERTY(PROP_EXPONENT, Exponent, exponent, float, LightEntityItem::DEFAULT_EXPONENT);
    DEFINE_PROPERTY(PROP_CUTOFF, Cutoff, cutoff, float, LightEntityItem::DEFAULT_CUTOFF);
    DEFINE_PROPERTY(PROP_FALLOFF_RADIUS, FalloffRadius, falloffRadius, float, LightEntityItem::DEFAULT_FALLOFF_RADIUS);

    // Text
    DEFINE_PROPERTY_REF(PROP_TEXT, Text, text, QString, TextEntityItem::DEFAULT_TEXT);
    DEFINE_PROPERTY(PROP_LINE_HEIGHT, LineHeight, lineHeight, float, TextEntityItem::DEFAULT_LINE_HEIGHT);
    DEFINE_PROPERTY_REF(PROP_TEXT_COLOR, TextColor, textColor, u8vec3Color, TextEntityItem::DEFAULT_TEXT_COLOR);
    DEFINE_PROPERTY_REF(PROP_TEXT_ALPHA, TextAlpha, textAlpha, float, TextEntityItem::DEFAULT_TEXT_ALPHA);
    DEFINE_PROPERTY_REF(PROP_BACKGROUND_COLOR, BackgroundColor, backgroundColor, u8vec3Color, TextEntityItem::DEFAULT_BACKGROUND_COLOR);
    DEFINE_PROPERTY_REF(PROP_BACKGROUND_ALPHA, BackgroundAlpha, backgroundAlpha, float, TextEntityItem::DEFAULT_TEXT_ALPHA);
    DEFINE_PROPERTY_REF(PROP_LEFT_MARGIN, LeftMargin, leftMargin, float, TextEntityItem::DEFAULT_MARGIN);
    DEFINE_PROPERTY_REF(PROP_RIGHT_MARGIN, RightMargin, rightMargin, float, TextEntityItem::DEFAULT_MARGIN);
    DEFINE_PROPERTY_REF(PROP_TOP_MARGIN, TopMargin, topMargin, float, TextEntityItem::DEFAULT_MARGIN);
    DEFINE_PROPERTY_REF(PROP_BOTTOM_MARGIN, BottomMargin, bottomMargin, float, TextEntityItem::DEFAULT_MARGIN);
    DEFINE_PROPERTY_REF(PROP_UNLIT, Unlit, unlit, bool, false);
    DEFINE_PROPERTY_REF(PROP_FONT, Font, font, QString, ROBOTO_FONT_FAMILY);
    DEFINE_PROPERTY_REF_ENUM(PROP_TEXT_EFFECT, TextEffect, textEffect, TextEffect, TextEffect::NO_EFFECT);
    DEFINE_PROPERTY_REF(PROP_TEXT_EFFECT_COLOR, TextEffectColor, textEffectColor, u8vec3Color, TextEntityItem::DEFAULT_TEXT_COLOR);
    DEFINE_PROPERTY(PROP_TEXT_EFFECT_THICKNESS, TextEffectThickness, textEffectThickness, float, TextEntityItem::DEFAULT_TEXT_EFFECT_THICKNESS);
    DEFINE_PROPERTY_REF_ENUM(PROP_TEXT_ALIGNMENT, Alignment, alignment, TextAlignment, TextAlignment::LEFT);

    // Zone
    DEFINE_PROPERTY_GROUP(KeyLight, keyLight, KeyLightPropertyGroup);
    DEFINE_PROPERTY_GROUP(AmbientLight, ambientLight, AmbientLightPropertyGroup);
    DEFINE_PROPERTY_GROUP(Skybox, skybox, SkyboxPropertyGroup);
    DEFINE_PROPERTY_GROUP(Haze, haze, HazePropertyGroup);
    DEFINE_PROPERTY_GROUP(Bloom, bloom, BloomPropertyGroup);
    DEFINE_PROPERTY(PROP_FLYING_ALLOWED, FlyingAllowed, flyingAllowed, bool, ZoneEntityItem::DEFAULT_FLYING_ALLOWED);
    DEFINE_PROPERTY(PROP_GHOSTING_ALLOWED, GhostingAllowed, ghostingAllowed, bool, ZoneEntityItem::DEFAULT_GHOSTING_ALLOWED);
    DEFINE_PROPERTY(PROP_FILTER_URL, FilterURL, filterURL, QString, ZoneEntityItem::DEFAULT_FILTER_URL);
    DEFINE_PROPERTY_REF_ENUM(PROP_KEY_LIGHT_MODE, KeyLightMode, keyLightMode, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_SKYBOX_MODE, SkyboxMode, skyboxMode, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_AMBIENT_LIGHT_MODE, AmbientLightMode, ambientLightMode, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_HAZE_MODE, HazeMode, hazeMode, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_BLOOM_MODE, BloomMode, bloomMode, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_AVATAR_PRIORITY, AvatarPriority, avatarPriority, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);
    DEFINE_PROPERTY_REF_ENUM(PROP_SCREENSHARE, Screenshare, screenshare, uint32_t, (uint32_t)COMPONENT_MODE_INHERIT);

    // Polyvox
    DEFINE_PROPERTY_REF(PROP_VOXEL_VOLUME_SIZE, VoxelVolumeSize, voxelVolumeSize, glm::vec3, PolyVoxEntityItem::DEFAULT_VOXEL_VOLUME_SIZE);
    DEFINE_PROPERTY_REF(PROP_VOXEL_DATA, VoxelData, voxelData, QByteArray, PolyVoxEntityItem::DEFAULT_VOXEL_DATA);
    DEFINE_PROPERTY_REF(PROP_VOXEL_SURFACE_STYLE, VoxelSurfaceStyle, voxelSurfaceStyle, uint16_t, PolyVoxEntityItem::DEFAULT_VOXEL_SURFACE_STYLE);
    DEFINE_PROPERTY_REF(PROP_X_TEXTURE_URL, XTextureURL, xTextureURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_Y_TEXTURE_URL, YTextureURL, yTextureURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_Z_TEXTURE_URL, ZTextureURL, zTextureURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_X_N_NEIGHBOR_ID, XNNeighborID, xNNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_Y_N_NEIGHBOR_ID, YNNeighborID, yNNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_Z_N_NEIGHBOR_ID, ZNNeighborID, zNNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_X_P_NEIGHBOR_ID, XPNeighborID, xPNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_Y_P_NEIGHBOR_ID, YPNeighborID, yPNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);
    DEFINE_PROPERTY_REF(PROP_Z_P_NEIGHBOR_ID, ZPNeighborID, zPNeighborID, EntityItemID, UNKNOWN_ENTITY_ID);

    // Web
    DEFINE_PROPERTY_REF(PROP_SOURCE_URL, SourceUrl, sourceUrl, QString, WebEntityItem::DEFAULT_SOURCE_URL);
    DEFINE_PROPERTY_REF(PROP_DPI, DPI, dpi, uint16_t, ENTITY_ITEM_DEFAULT_DPI);
    DEFINE_PROPERTY_REF(PROP_SCRIPT_URL, ScriptURL, scriptURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_MAX_FPS, MaxFPS, maxFPS, uint8_t, WebEntityItem::DEFAULT_MAX_FPS);
    DEFINE_PROPERTY_REF_ENUM(PROP_INPUT_MODE, InputMode, inputMode, WebInputMode, WebInputMode::TOUCH);
    DEFINE_PROPERTY_REF(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, ShowKeyboardFocusHighlight, showKeyboardFocusHighlight, bool, true);
    DEFINE_PROPERTY_REF(PROP_WEB_USE_BACKGROUND, UseBackground, useBackground, bool, true);
    DEFINE_PROPERTY_REF(PROP_USER_AGENT, UserAgent, userAgent, QString, WebEntityItem::DEFAULT_USER_AGENT);

    // Polyline
    DEFINE_PROPERTY_REF(PROP_LINE_POINTS, LinePoints, linePoints, QVector<glm::vec3>, ENTITY_ITEM_DEFAULT_EMPTY_VEC3_QVEC);
    DEFINE_PROPERTY(PROP_STROKE_WIDTHS, StrokeWidths, strokeWidths, QVector<float>, QVector<float>());
    DEFINE_PROPERTY(PROP_STROKE_NORMALS, Normals, normals, QVector<glm::vec3>, ENTITY_ITEM_DEFAULT_EMPTY_VEC3_QVEC);
    DEFINE_PROPERTY(PROP_STROKE_COLORS, StrokeColors, strokeColors, QVector<glm::vec3>, ENTITY_ITEM_DEFAULT_EMPTY_VEC3_QVEC);
    DEFINE_PROPERTY(PROP_IS_UV_MODE_STRETCH, IsUVModeStretch, isUVModeStretch, bool, true);
    DEFINE_PROPERTY(PROP_LINE_GLOW, Glow, glow, bool, false);
    DEFINE_PROPERTY(PROP_LINE_FACE_CAMERA, FaceCamera, faceCamera, bool, false);

    // Shape
    DEFINE_PROPERTY_REF(PROP_SHAPE, Shape, shape, QString, "Sphere");

    // Material
    DEFINE_PROPERTY_REF(PROP_MATERIAL_URL, MaterialURL, materialURL, QString, "");
    DEFINE_PROPERTY_REF_ENUM(PROP_MATERIAL_MAPPING_MODE, MaterialMappingMode, materialMappingMode, MaterialMappingMode, UV);
    DEFINE_PROPERTY_REF(PROP_MATERIAL_PRIORITY, Priority, priority, quint16, 0);
    DEFINE_PROPERTY_REF(PROP_PARENT_MATERIAL_NAME, ParentMaterialName, parentMaterialName, QString, "0");
    DEFINE_PROPERTY_REF(PROP_MATERIAL_MAPPING_POS, MaterialMappingPos, materialMappingPos, glm::vec2, glm::vec2(0.0f));
    DEFINE_PROPERTY_REF(PROP_MATERIAL_MAPPING_SCALE, MaterialMappingScale, materialMappingScale, glm::vec2, glm::vec2(1.0f));
    DEFINE_PROPERTY_REF(PROP_MATERIAL_MAPPING_ROT, MaterialMappingRot, materialMappingRot, float, 0);
    DEFINE_PROPERTY_REF(PROP_MATERIAL_DATA, MaterialData, materialData, QString, "");
    DEFINE_PROPERTY_REF(PROP_MATERIAL_REPEAT, MaterialRepeat, materialRepeat, bool, true);

    // Image
    DEFINE_PROPERTY_REF(PROP_IMAGE_URL, ImageURL, imageURL, QString, "");
    DEFINE_PROPERTY_REF(PROP_EMISSIVE, Emissive, emissive, bool, false);
    DEFINE_PROPERTY_REF(PROP_KEEP_ASPECT_RATIO, KeepAspectRatio, keepAspectRatio, bool, true);
    DEFINE_PROPERTY_REF(PROP_SUB_IMAGE, SubImage, subImage, QRect, QRect());

    // Grid
    DEFINE_PROPERTY_REF(PROP_GRID_FOLLOW_CAMERA, FollowCamera, followCamera, bool, true);
    DEFINE_PROPERTY(PROP_MAJOR_GRID_EVERY, MajorGridEvery, majorGridEvery, uint32_t, GridEntityItem::DEFAULT_MAJOR_GRID_EVERY);
    DEFINE_PROPERTY(PROP_MINOR_GRID_EVERY, MinorGridEvery, minorGridEvery, float, GridEntityItem::DEFAULT_MINOR_GRID_EVERY);

    // Gizmo
    DEFINE_PROPERTY_REF_ENUM(PROP_GIZMO_TYPE, GizmoType, gizmoType, GizmoType, GizmoType::RING);
    DEFINE_PROPERTY_GROUP(Ring, ring, RingGizmoPropertyGroup);

    static QString getComponentModeAsString(uint32_t mode);

public:
    float getMaxDimension() const { return glm::compMax(_dimensions); }

    float getAge() const { return (float)(usecTimestampNow() - _created) / (float)USECS_PER_SECOND; }
    bool hasCreatedTime() const { return (_created != UNKNOWN_CREATED_TIME); }

    bool containsBoundsProperties() const { return (_positionChanged || _dimensionsChanged); }
    bool containsPositionChange() const { return _positionChanged; }
    bool containsDimensionsChange() const { return _dimensionsChanged; }

    static OctreeElement::AppendState encodeEntityEditPacket(PacketType command, EntityItemID id, const EntityItemProperties& properties,
                                       QByteArray& buffer, EntityPropertyFlags requestedProperties, EntityPropertyFlags& didntFitProperties);

    static bool encodeEraseEntityMessage(const EntityItemID& entityItemID, QByteArray& buffer);
    static bool encodeCloneEntityMessage(const EntityItemID& entityIDToClone, const EntityItemID& newEntityID, QByteArray& buffer);
    static bool decodeCloneEntityMessage(const QByteArray& buffer, int& processedBytes, EntityItemID& entityIDToClone, EntityItemID& newEntityID);

    static bool decodeEntityEditPacket(const unsigned char* data, int bytesToRead, int& processedBytes,
                                       EntityItemID& entityID, EntityItemProperties& properties);

    void clearID() { _id = UNKNOWN_ENTITY_ID; _idSet = false; }
    void markAllChanged();

    const glm::vec3& getNaturalDimensions() const { return _naturalDimensions; }
    void setNaturalDimensions(const glm::vec3& value) { _naturalDimensions = value; }
    
    const glm::vec3& getNaturalPosition() const { return _naturalPosition; }
    void calculateNaturalPosition(const glm::vec3& min, const glm::vec3& max);
    
    const QVariantMap& getTextureNames() const { return _textureNames; }
    void setTextureNames(const QVariantMap& value) { _textureNames = value; }

    QString getSimulatorIDAsString() const { return _simulationOwner.getID().toString().mid(1,36).toUpper(); }

    void setVoxelDataDirty() { _voxelDataChanged = true; }

    void setLinePointsDirty() {_linePointsChanged = true; }

    void setQueryAACubeDirty() { _queryAACubeChanged = true; }

    void setLocationDirty() { _positionChanged = true; _rotationChanged = true; }

    bool hasTransformOrVelocityChanges() const;
    void clearTransformOrVelocityChanges();
    bool hasMiscPhysicsChanges() const;

    bool hasSimulationRestrictedChanges() const;
    void copySimulationRestrictedProperties(const EntityItemPointer& entity);
    void clearSimulationRestrictedProperties();

    void clearSimulationOwner();
    void setSimulationOwner(const QUuid& id, uint8_t priority);
    void setSimulationOwner(const QByteArray& data);
    void setSimulationPriority(uint8_t priority) { _simulationOwner.setPriority(priority); }
    uint8_t computeSimulationBidPriority() const;

    void setActionDataDirty() { _actionDataChanged = true; }

    QList<QString> listChangedProperties();

    bool getDimensionsInitialized() const { return _dimensionsInitialized; }
    void setDimensionsInitialized(bool dimensionsInitialized) { _dimensionsInitialized = dimensionsInitialized; }

    void setJointRotationsDirty() { _jointRotationsSetChanged = true; _jointRotationsChanged = true; }
    void setJointTranslationsDirty() { _jointTranslationsSetChanged = true; _jointTranslationsChanged = true; }

    // render info related items
    size_t getRenderInfoVertexCount() const { return _renderInfoVertexCount; }
    void setRenderInfoVertexCount(size_t value) { _renderInfoVertexCount = value; }
    int getRenderInfoTextureCount() const { return _renderInfoTextureCount; }
    void setRenderInfoTextureCount(int value) { _renderInfoTextureCount = value; }
    size_t getRenderInfoTextureSize() const { return _renderInfoTextureSize; }
    void setRenderInfoTextureSize(size_t value) { _renderInfoTextureSize = value; }
    int getRenderInfoDrawCalls() const { return _renderInfoDrawCalls; }
    void setRenderInfoDrawCalls(int value) { _renderInfoDrawCalls = value; }
    bool getRenderInfoHasTransparent() const { return _renderInfoHasTransparent; }
    void setRenderInfoHasTransparent(bool value) { _renderInfoHasTransparent = value; }

    void setPackedNormals(const QByteArray& value);
    QVector<glm::vec3> unpackNormals(const QByteArray& normals);

    void setPackedStrokeColors(const QByteArray& value);
    QVector<glm::vec3> unpackStrokeColors(const QByteArray& strokeColors);

    QByteArray getPackedNormals() const;
    QByteArray packNormals(const QVector<glm::vec3>& normals) const;

    QByteArray getPackedStrokeColors() const;
    QByteArray packStrokeColors(const QVector<glm::vec3>& strokeColors) const;

    QByteArray getStaticCertificateJSON() const;
    QByteArray getStaticCertificateHash() const;
    bool verifyStaticCertificateProperties();
    static bool verifySignature(const QString& key, const QByteArray& text, const QByteArray& signature);

    void convertToCloneProperties(const EntityItemID& entityIDToClone);

protected:
    QString getCollisionMaskAsString() const;
    void setCollisionMaskFromString(const QString& maskString);

private:
    QUuid _id;
    bool _idSet;
    quint64 _lastEdited;
    EntityTypes::EntityType _type;
    void setType(const QString& typeName) { _type = EntityTypes::getEntityTypeFromName(typeName); }

    bool _defaultSettings;
    bool _dimensionsInitialized = true; // Only false if creating an entity locally with no dimensions properties

    // NOTE: The following are pseudo client only properties. They are only used in clients which can access
    // properties of model geometry. But these properties are not serialized like other properties.
    QVariantMap _textureNames;
    glm::vec3 _naturalDimensions;
    glm::vec3 _naturalPosition;

    size_t _renderInfoVertexCount { 0 };
    int _renderInfoTextureCount { 0 };
    size_t _renderInfoTextureSize { 0 };
    int _renderInfoDrawCalls { 0 };
    bool _renderInfoHasTransparent { false };

    EntityPropertyFlags _desiredProperties; // if set will narrow scopes of copy/to/from to just these properties
};

Q_DECLARE_METATYPE(EntityItemProperties);
ScriptValuePointer EntityItemPropertiesToScriptValue(ScriptEngine* engine, const EntityItemProperties& properties);
ScriptValuePointer EntityItemNonDefaultPropertiesToScriptValue(ScriptEngine* engine, const EntityItemProperties& properties);
void EntityItemPropertiesFromScriptValueIgnoreReadOnly(const ScriptValuePointer& object, EntityItemProperties& properties);
void EntityItemPropertiesFromScriptValueHonorReadOnly(const ScriptValuePointer& object, EntityItemProperties& properties);

Q_DECLARE_METATYPE(EntityPropertyFlags);
ScriptValuePointer EntityPropertyFlagsToScriptValue(ScriptEngine* engine, const EntityPropertyFlags& flags);
void EntityPropertyFlagsFromScriptValue(const ScriptValuePointer& object, EntityPropertyFlags& flags);

Q_DECLARE_METATYPE(EntityPropertyInfo);
ScriptValuePointer EntityPropertyInfoToScriptValue(ScriptEngine* engine, const EntityPropertyInfo& propertyInfo);
void EntityPropertyInfoFromScriptValue(const ScriptValuePointer& object, EntityPropertyInfo& propertyInfo);

// define these inline here so the macros work
inline void EntityItemProperties::setPosition(const glm::vec3& value)
                    { _position = glm::clamp(value, (float)-HALF_TREE_SCALE, (float)HALF_TREE_SCALE); _positionChanged = true; }

inline void EntityItemProperties::setOwningAvatarID(const QUuid& id) {
    _owningAvatarID = id;
    if (!_owningAvatarID.isNull()) {
        // for AvatarEntities there's no entity-server to tell us we're the simulation owner,
        // so always set the simulationOwner to the owningAvatarID and a high priority.
        setSimulationOwner(_owningAvatarID, AVATAR_ENTITY_SIMULATION_PRIORITY);
    }
    _owningAvatarIDChanged = true;
}

QDebug& operator<<(QDebug& dbg, const EntityPropertyFlags& f);

inline QDebug operator<<(QDebug debug, const EntityItemProperties& properties) {
    debug << "EntityItemProperties[" << "\n";

    debug << "    _type:" << properties.getType() << "\n";

    // TODO: figure out why position and animationSettings don't seem to like the macro approach
    if (properties.containsPositionChange()) {
        debug << "  position:" << properties.getPosition() << "in meters" << "\n";
    }

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Dimensions, dimensions, "in meters");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Velocity, velocity, "in meters");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Name, name, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Visible, visible, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CanCastShadow, canCastShadow, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Rotation, rotation, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Density, density, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Gravity, gravity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Acceleration, acceleration, "in meters per second");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Damping, damping, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Restitution, restitution, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Friction, friction, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Created, created, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Lifetime, lifetime, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Script, script, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ScriptTimestamp, scriptTimestamp, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CollisionSoundURL, collisionSoundURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Color, color, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ColorSpread, colorSpread, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ColorStart, colorStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ColorFinish, colorFinish, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Alpha, alpha, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AlphaSpread, alphaSpread, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AlphaStart, alphaStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AlphaFinish, alphaFinish, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ModelURL, modelURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CompoundShapeURL, compoundShapeURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, RegistrationPoint, registrationPoint, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AngularVelocity, angularVelocity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AngularDamping, angularDamping, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Collisionless, collisionless, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Dynamic, dynamic, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, IsSpotlight, isSpotlight, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Intensity, intensity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, FalloffRadius, falloffRadius, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Exponent, exponent, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Cutoff, cutoff, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Locked, locked, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Textures, textures, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, UserData, userData, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, PrivateUserData, privateUserData, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, SimulationOwner, simulationOwner, SimulationOwner());
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Text, text, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LineHeight, lineHeight, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, TextColor, textColor, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, BackgroundColor, backgroundColor, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ShapeType, shapeType, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, MaxParticles, maxParticles, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Lifespan, lifespan, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, IsEmitting, isEmitting, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitRate, emitRate, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitSpeed, emitSpeed, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, SpeedSpread, speedSpread, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitOrientation, emitOrientation, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitDimensions, emitDimensions, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitRadiusStart, emitRadiusStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, PolarStart, polarStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, PolarFinish, polarFinish, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AzimuthStart, azimuthStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AzimuthFinish, azimuthFinish, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EmitAcceleration, emitAcceleration, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AccelerationSpread, accelerationSpread, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ParticleRadius, particleRadius, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, RadiusSpread, radiusSpread, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, RadiusStart, radiusStart, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, RadiusFinish, radiusFinish, "");

    // Certifiable Properties
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ItemName, itemName, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ItemDescription, itemDescription, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ItemCategories, itemCategories, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ItemArtist, itemArtist, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ItemLicense, itemLicense, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LimitedRun, limitedRun, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, MarketplaceID, marketplaceID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EditionNumber, editionNumber, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EntityInstanceNumber, entityInstanceNumber, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CertificateID, certificateID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CertificateType, certificateType, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, StaticCertificateVersion, staticCertificateVersion, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LocalPosition, localPosition, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LocalRotation, localRotation, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LocalVelocity, localVelocity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LocalAngularVelocity, localAngularVelocity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LocalDimensions, localDimensions, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, HazeMode, hazeMode, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, KeyLightMode, keyLightMode, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AmbientLightMode, ambientLightMode, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, SkyboxMode, skyboxMode, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, BloomMode, bloomMode, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Cloneable, cloneable, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CloneLifetime, cloneLifetime, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CloneLimit, cloneLimit, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CloneDynamic, cloneDynamic, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CloneAvatarEntity, cloneAvatarEntity, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, CloneOriginID, cloneOriginID, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, VoxelVolumeSize, voxelVolumeSize, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, VoxelData, voxelData, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, VoxelSurfaceStyle, voxelSurfaceStyle, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Href, href, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Description, description, "");
    if (properties.actionDataChanged()) {
        debug << " " << "actionData" << ":" << properties.getActionData().toHex() << "" << "\n";
    }
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, XTextureURL, xTextureURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, YTextureURL, yTextureURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ZTextureURL, zTextureURL, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, XNNeighborID, xNNeighborID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, YNNeighborID, yNNeighborID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ZNNeighborID, zNNeighborID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, XPNeighborID, xPNeighborID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, YPNeighborID, yPNeighborID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ZPNeighborID, zPNeighborID, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ParentID, parentID, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, ParentJointIndex, parentJointIndex, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, QueryAACube, queryAACube, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, JointRotationsSet, jointRotationsSet, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, JointRotations, jointRotations, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, JointTranslationsSet, jointTranslationsSet, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, JointTranslations, jointTranslations, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, FlyingAllowed, flyingAllowed, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, GhostingAllowed, ghostingAllowed, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, FilterURL, filterURL, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, AvatarPriority, avatarPriority, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, Screenshare, screenshare, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, EntityHostTypeAsString, entityHostType, "");
    DEBUG_PROPERTY_IF_CHANGED(debug, properties, OwningAvatarID, owningAvatarID, "");

    DEBUG_PROPERTY_IF_CHANGED(debug, properties, LastEditedBy, lastEditedBy, "");

    debug << "  last edited:" << properties.getLastEdited() << "\n";
    debug << "  edited ago:" << properties.getEditedAgo() << "\n";
    debug << "]";

    return debug;
}

#endif // hifi_EntityItemProperties_h
