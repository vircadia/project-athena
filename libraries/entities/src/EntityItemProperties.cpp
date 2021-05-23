//
//  EntityItemProperties.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityItemProperties.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ecdsa.h>

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QtCore/QJsonDocument>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <NetworkAccessManager.h>
#include <ByteCountCoding.h>
#include <GLMHelpers.h>
#include <RegisteredMetaTypes.h>
#include <Extents.h>
#include <VariantMapToScriptValue.h>
#include <ScriptValue.h>

#include "EntitiesLogging.h"
#include "EntityItem.h"
#include "ModelEntityItem.h"
#include "PolyLineEntityItem.h"

AnimationPropertyGroup EntityItemProperties::_staticAnimation;
SkyboxPropertyGroup EntityItemProperties::_staticSkybox;
HazePropertyGroup EntityItemProperties::_staticHaze;
BloomPropertyGroup EntityItemProperties::_staticBloom;
KeyLightPropertyGroup EntityItemProperties::_staticKeyLight;
AmbientLightPropertyGroup EntityItemProperties::_staticAmbientLight;
GrabPropertyGroup EntityItemProperties::_staticGrab;
PulsePropertyGroup EntityItemProperties::_staticPulse;
RingGizmoPropertyGroup EntityItemProperties::_staticRing;

EntityPropertyList PROP_LAST_ITEM = (EntityPropertyList)(PROP_AFTER_LAST_ITEM - 1);

EntityItemProperties::EntityItemProperties(EntityPropertyFlags desiredProperties) :
    _id(UNKNOWN_ENTITY_ID),
    _idSet(false),
    _lastEdited(0),
    _type(EntityTypes::Unknown),

    _defaultSettings(true),
    _naturalDimensions(1.0f, 1.0f, 1.0f),
    _naturalPosition(0.0f, 0.0f, 0.0f),
    _desiredProperties(desiredProperties)
{

}

void EntityItemProperties::calculateNaturalPosition(const vec3& min, const vec3& max) {
    vec3 halfDimension = (max - min) / 2.0f;
    _naturalPosition = max - halfDimension;
}

void EntityItemProperties::debugDump() const {
    qCDebug(entities) << "EntityItemProperties...";
    qCDebug(entities) << "    _type=" << EntityTypes::getEntityTypeName(_type);
    qCDebug(entities) << "   _id=" << _id;
    qCDebug(entities) << "   _idSet=" << _idSet;
    qCDebug(entities) << "   _position=" << _position.x << "," << _position.y << "," << _position.z;
    qCDebug(entities) << "   _dimensions=" << getDimensions();
    qCDebug(entities) << "   _modelURL=" << _modelURL;
    qCDebug(entities) << "   _compoundShapeURL=" << _compoundShapeURL;

    getAnimation().debugDump();
    getSkybox().debugDump();
    getHaze().debugDump();
    getKeyLight().debugDump();
    getAmbientLight().debugDump();
    getBloom().debugDump();
    getGrab().debugDump();

    qCDebug(entities) << "   changed properties...";
    EntityPropertyFlags props = getChangedProperties();
    props.debugDumpBits();
}

void EntityItemProperties::setLastEdited(quint64 usecTime) {
    _lastEdited = usecTime > _created ? usecTime : _created;
}

bool EntityItemProperties::constructFromBuffer(const unsigned char* data, int dataLength) {
    ReadBitstreamToTreeParams args;
    EntityItemPointer tempEntity = EntityTypes::constructEntityItem(data, dataLength);
    if (!tempEntity) {
        return false;
    }
    tempEntity->readEntityDataFromBuffer(data, dataLength, args);
    (*this) = tempEntity->getProperties();
    return true;
}

inline void addShapeType(QHash<QString, ShapeType>& lookup, ShapeType type) { lookup[ShapeInfo::getNameForShapeType(type)] = type; }
QHash<QString, ShapeType> stringToShapeTypeLookup = [] {
    QHash<QString, ShapeType> toReturn;
    addShapeType(toReturn, SHAPE_TYPE_NONE);
    addShapeType(toReturn, SHAPE_TYPE_BOX);
    addShapeType(toReturn, SHAPE_TYPE_SPHERE);
    addShapeType(toReturn, SHAPE_TYPE_CAPSULE_X);
    addShapeType(toReturn, SHAPE_TYPE_CAPSULE_Y);
    addShapeType(toReturn, SHAPE_TYPE_CAPSULE_Z);
    addShapeType(toReturn, SHAPE_TYPE_CYLINDER_X);
    addShapeType(toReturn, SHAPE_TYPE_CYLINDER_Y);
    addShapeType(toReturn, SHAPE_TYPE_CYLINDER_Z);
    addShapeType(toReturn, SHAPE_TYPE_HULL);
    addShapeType(toReturn, SHAPE_TYPE_PLANE);
    addShapeType(toReturn, SHAPE_TYPE_COMPOUND);
    addShapeType(toReturn, SHAPE_TYPE_SIMPLE_HULL);
    addShapeType(toReturn, SHAPE_TYPE_SIMPLE_COMPOUND);
    addShapeType(toReturn, SHAPE_TYPE_STATIC_MESH);
    addShapeType(toReturn, SHAPE_TYPE_ELLIPSOID);
    addShapeType(toReturn, SHAPE_TYPE_CIRCLE);
    return toReturn;
}();
QString EntityItemProperties::getShapeTypeAsString() const { return ShapeInfo::getNameForShapeType(_shapeType); }
void EntityItemProperties::setShapeTypeFromString(const QString& shapeName) {
    auto shapeTypeItr = stringToShapeTypeLookup.find(shapeName.toLower());
    if (shapeTypeItr != stringToShapeTypeLookup.end()) {
        _shapeType = shapeTypeItr.value();
        _shapeTypeChanged = true;
    }
}

inline void addMaterialMappingMode(QHash<QString, MaterialMappingMode>& lookup, MaterialMappingMode mode) { lookup[MaterialMappingModeHelpers::getNameForMaterialMappingMode(mode)] = mode; }
const QHash<QString, MaterialMappingMode> stringToMaterialMappingModeLookup = [] {
    QHash<QString, MaterialMappingMode> toReturn;
    addMaterialMappingMode(toReturn, UV);
    addMaterialMappingMode(toReturn, PROJECTED);
    return toReturn;
}();
QString EntityItemProperties::getMaterialMappingModeAsString() const { return MaterialMappingModeHelpers::getNameForMaterialMappingMode(_materialMappingMode); }
void EntityItemProperties::setMaterialMappingModeFromString(const QString& materialMappingMode) {
    auto materialMappingModeItr = stringToMaterialMappingModeLookup.find(materialMappingMode.toLower());
    if (materialMappingModeItr != stringToMaterialMappingModeLookup.end()) {
        _materialMappingMode = materialMappingModeItr.value();
        _materialMappingModeChanged = true;
    }
}

inline void addBillboardMode(QHash<QString, BillboardMode>& lookup, BillboardMode mode) { lookup[BillboardModeHelpers::getNameForBillboardMode(mode)] = mode; }
const QHash<QString, BillboardMode> stringToBillboardModeLookup = [] {
    QHash<QString, BillboardMode> toReturn;
    addBillboardMode(toReturn, BillboardMode::NONE);
    addBillboardMode(toReturn, BillboardMode::YAW);
    addBillboardMode(toReturn, BillboardMode::FULL);
    return toReturn;
}();
QString EntityItemProperties::getBillboardModeAsString() const { return BillboardModeHelpers::getNameForBillboardMode(_billboardMode); }
void EntityItemProperties::setBillboardModeFromString(const QString& billboardMode) {
    auto billboardModeItr = stringToBillboardModeLookup.find(billboardMode.toLower());
    if (billboardModeItr != stringToBillboardModeLookup.end()) {
        _billboardMode = billboardModeItr.value();
        _billboardModeChanged = true;
    }
}

inline void addRenderLayer(QHash<QString, RenderLayer>& lookup, RenderLayer mode) { lookup[RenderLayerHelpers::getNameForRenderLayer(mode)] = mode; }
const QHash<QString, RenderLayer> stringToRenderLayerLookup = [] {
    QHash<QString, RenderLayer> toReturn;
    addRenderLayer(toReturn, RenderLayer::WORLD);
    addRenderLayer(toReturn, RenderLayer::FRONT);
    addRenderLayer(toReturn, RenderLayer::HUD);
    return toReturn;
}();
QString EntityItemProperties::getRenderLayerAsString() const { return RenderLayerHelpers::getNameForRenderLayer(_renderLayer); }
void EntityItemProperties::setRenderLayerFromString(const QString& renderLayer) {
    auto renderLayerItr = stringToRenderLayerLookup.find(renderLayer.toLower());
    if (renderLayerItr != stringToRenderLayerLookup.end()) {
        _renderLayer = renderLayerItr.value();
        _renderLayerChanged = true;
    }
}

inline void addPrimitiveMode(QHash<QString, PrimitiveMode>& lookup, PrimitiveMode mode) { lookup[PrimitiveModeHelpers::getNameForPrimitiveMode(mode)] = mode; }
const QHash<QString, PrimitiveMode> stringToPrimitiveModeLookup = [] {
    QHash<QString, PrimitiveMode> toReturn;
    addPrimitiveMode(toReturn, PrimitiveMode::SOLID);
    addPrimitiveMode(toReturn, PrimitiveMode::LINES);
    return toReturn;
}();
QString EntityItemProperties::getPrimitiveModeAsString() const { return PrimitiveModeHelpers::getNameForPrimitiveMode(_primitiveMode); }
void EntityItemProperties::setPrimitiveModeFromString(const QString& primitiveMode) {
    auto primitiveModeItr = stringToPrimitiveModeLookup.find(primitiveMode.toLower());
    if (primitiveModeItr != stringToPrimitiveModeLookup.end()) {
        _primitiveMode = primitiveModeItr.value();
        _primitiveModeChanged = true;
    }
}

inline void addWebInputMode(QHash<QString, WebInputMode>& lookup, WebInputMode mode) { lookup[WebInputModeHelpers::getNameForWebInputMode(mode)] = mode; }
const QHash<QString, WebInputMode> stringToWebInputModeLookup = [] {
    QHash<QString, WebInputMode> toReturn;
    addWebInputMode(toReturn, WebInputMode::TOUCH);
    addWebInputMode(toReturn, WebInputMode::MOUSE);
    return toReturn;
}();
QString EntityItemProperties::getInputModeAsString() const { return WebInputModeHelpers::getNameForWebInputMode(_inputMode); }
void EntityItemProperties::setInputModeFromString(const QString& webInputMode) {
    auto webInputModeItr = stringToWebInputModeLookup.find(webInputMode.toLower());
    if (webInputModeItr != stringToWebInputModeLookup.end()) {
        _inputMode = webInputModeItr.value();
        _inputModeChanged = true;
    }
}

inline void addGizmoType(QHash<QString, GizmoType>& lookup, GizmoType mode) { lookup[GizmoTypeHelpers::getNameForGizmoType(mode)] = mode; }
const QHash<QString, GizmoType> stringToGizmoTypeLookup = [] {
    QHash<QString, GizmoType> toReturn;
    addGizmoType(toReturn, GizmoType::RING);
    return toReturn;
}();
QString EntityItemProperties::getGizmoTypeAsString() const { return GizmoTypeHelpers::getNameForGizmoType(_gizmoType); }
void EntityItemProperties::setGizmoTypeFromString(const QString& gizmoType) {
    auto gizmoTypeItr = stringToGizmoTypeLookup.find(gizmoType.toLower());
    if (gizmoTypeItr != stringToGizmoTypeLookup.end()) {
        _gizmoType = gizmoTypeItr.value();
        _gizmoTypeChanged = true;
    }
}

inline void addComponentMode(QHash<QString, ComponentMode>& lookup, ComponentMode mode) { lookup[ComponentModeHelpers::getNameForComponentMode(mode)] = mode; }
const QHash<QString, ComponentMode> stringToComponentMode = [] {
    QHash<QString, ComponentMode> toReturn;
    addComponentMode(toReturn, ComponentMode::COMPONENT_MODE_INHERIT);
    addComponentMode(toReturn, ComponentMode::COMPONENT_MODE_DISABLED);
    addComponentMode(toReturn, ComponentMode::COMPONENT_MODE_ENABLED);
    return toReturn;
}();
QString EntityItemProperties::getComponentModeAsString(uint32_t mode) { return ComponentModeHelpers::getNameForComponentMode((ComponentMode)mode); }
QString EntityItemProperties::getSkyboxModeAsString() const { return getComponentModeAsString(_skyboxMode); }
QString EntityItemProperties::getKeyLightModeAsString() const { return getComponentModeAsString(_keyLightMode); }
QString EntityItemProperties::getAmbientLightModeAsString() const { return getComponentModeAsString(_ambientLightMode); }
QString EntityItemProperties::getHazeModeAsString() const { return getComponentModeAsString(_hazeMode); }
QString EntityItemProperties::getBloomModeAsString() const { return getComponentModeAsString(_bloomMode); }
void EntityItemProperties::setSkyboxModeFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _skyboxMode = modeItr.value();
        _skyboxModeChanged = true;
    }
}
void EntityItemProperties::setKeyLightModeFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _keyLightMode = modeItr.value();
        _keyLightModeChanged = true;
    }
}
void EntityItemProperties::setAmbientLightModeFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _ambientLightMode = modeItr.value();
        _ambientLightModeChanged = true;
    }
}
void EntityItemProperties::setHazeModeFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _hazeMode = modeItr.value();
        _hazeModeChanged = true;
    }
}
void EntityItemProperties::setBloomModeFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _bloomMode = modeItr.value();
        _bloomModeChanged = true;
    }
}

inline void addAvatarPriorityMode(QHash<QString, AvatarPriorityMode>& lookup, AvatarPriorityMode mode) { lookup[AvatarPriorityModeHelpers::getNameForAvatarPriorityMode(mode)] = mode; }
const QHash<QString, AvatarPriorityMode> stringToAvatarPriority = [] {
    QHash<QString, AvatarPriorityMode> toReturn;
    addAvatarPriorityMode(toReturn, AvatarPriorityMode::AVATAR_PRIORITY_INHERIT);
    addAvatarPriorityMode(toReturn, AvatarPriorityMode::AVATAR_PRIORITY_CROWD);
    addAvatarPriorityMode(toReturn, AvatarPriorityMode::AVATAR_PRIORITY_HERO);
    return toReturn;
}();
QString EntityItemProperties::getAvatarPriorityAsString() const { return AvatarPriorityModeHelpers::getNameForAvatarPriorityMode((AvatarPriorityMode)_avatarPriority); }
void EntityItemProperties::setAvatarPriorityFromString(const QString& mode) {
    auto modeItr = stringToAvatarPriority.find(mode.toLower());
    if (modeItr != stringToAvatarPriority.end()) {
        _avatarPriority = modeItr.value();
        _avatarPriorityChanged = true;
    }
}

QString EntityItemProperties::getScreenshareAsString() const { return getComponentModeAsString(_screenshare); }
void EntityItemProperties::setScreenshareFromString(const QString& mode) {
    auto modeItr = stringToComponentMode.find(mode.toLower());
    if (modeItr != stringToComponentMode.end()) {
        _screenshare = modeItr.value();
        _screenshareChanged = true;
    }
}

inline void addTextEffect(QHash<QString, TextEffect>& lookup, TextEffect effect) { lookup[TextEffectHelpers::getNameForTextEffect(effect)] = effect; }
const QHash<QString, TextEffect> stringToTextEffectLookup = [] {
    QHash<QString, TextEffect> toReturn;
    addTextEffect(toReturn, TextEffect::NO_EFFECT);
    addTextEffect(toReturn, TextEffect::OUTLINE_EFFECT);
    addTextEffect(toReturn, TextEffect::OUTLINE_WITH_FILL_EFFECT);
    addTextEffect(toReturn, TextEffect::SHADOW_EFFECT);
    return toReturn;
}();
QString EntityItemProperties::getTextEffectAsString() const { return TextEffectHelpers::getNameForTextEffect(_textEffect); }
void EntityItemProperties::setTextEffectFromString(const QString& effect) {
    auto textEffectItr = stringToTextEffectLookup.find(effect.toLower());
    if (textEffectItr != stringToTextEffectLookup.end()) {
        _textEffect = textEffectItr.value();
        _textEffectChanged = true;
    }
}

inline void addTextAlignment(QHash<QString, TextAlignment>& lookup, TextAlignment alignment) { lookup[TextAlignmentHelpers::getNameForTextAlignment(alignment)] = alignment; }
const QHash<QString, TextAlignment> stringToTextAlignmentLookup = [] {
    QHash<QString, TextAlignment> toReturn;
    addTextAlignment(toReturn, TextAlignment::LEFT);
    addTextAlignment(toReturn, TextAlignment::CENTER);
    addTextAlignment(toReturn, TextAlignment::RIGHT);
    return toReturn;
}();
QString EntityItemProperties::getAlignmentAsString() const { return TextAlignmentHelpers::getNameForTextAlignment(_alignment); }
void EntityItemProperties::setAlignmentFromString(const QString& alignment) {
    auto textAlignmentItr = stringToTextAlignmentLookup.find(alignment.toLower());
    if (textAlignmentItr != stringToTextAlignmentLookup.end()) {
        _alignment = textAlignmentItr.value();
        _alignmentChanged = true;
    }
}

QString getCollisionGroupAsString(uint16_t group) {
    switch (group) {
        case USER_COLLISION_GROUP_DYNAMIC:
            return "dynamic";
        case USER_COLLISION_GROUP_STATIC:
            return "static";
        case USER_COLLISION_GROUP_KINEMATIC:
            return "kinematic";
        case USER_COLLISION_GROUP_MY_AVATAR:
            return "myAvatar";
        case USER_COLLISION_GROUP_OTHER_AVATAR:
            return "otherAvatar";
    };
    return "";
}

uint16_t getCollisionGroupAsBitMask(const QStringRef& name) {
    if (0 == name.compare(QString("dynamic"))) {
        return USER_COLLISION_GROUP_DYNAMIC;
    } else if (0 == name.compare(QString("static"))) {
        return USER_COLLISION_GROUP_STATIC;
    } else if (0 == name.compare(QString("kinematic"))) {
        return USER_COLLISION_GROUP_KINEMATIC;
    } else if (0 == name.compare(QString("myAvatar"))) {
        return USER_COLLISION_GROUP_MY_AVATAR;
    } else if (0 == name.compare(QString("otherAvatar"))) {
        return USER_COLLISION_GROUP_OTHER_AVATAR;
    }
    return 0;
}

QString EntityItemProperties::getCollisionMaskAsString() const {
    QString maskString("");
    for (int i = 0; i < NUM_USER_COLLISION_GROUPS; ++i) {
        uint16_t group = 0x0001 << i;
        if (group & _collisionMask) {
            maskString.append(getCollisionGroupAsString(group));
            maskString.append(',');
        }
    }
    return maskString;
}

void EntityItemProperties::setCollisionMaskFromString(const QString& maskString) {
    QVector<QStringRef> groups = maskString.splitRef(',');
    uint16_t mask = 0x0000;
    for (auto groupName : groups) {
        mask |= getCollisionGroupAsBitMask(groupName);
    }
    _collisionMask = mask;
    _collisionMaskChanged = true;
}

QString EntityItemProperties::getEntityHostTypeAsString() const {
    switch (_entityHostType) {
        case entity::HostType::DOMAIN:
            return "domain";
        case entity::HostType::AVATAR:
            return "avatar";
        case entity::HostType::LOCAL:
            return "local";
        default:
            return "";
    }
}

void EntityItemProperties::setEntityHostTypeFromString(const QString& entityHostType) {
    if (entityHostType == "domain") {
        _entityHostType = entity::HostType::DOMAIN;
    } else if (entityHostType == "avatar") {
        _entityHostType = entity::HostType::AVATAR;
    } else if (entityHostType == "local") {
        _entityHostType = entity::HostType::LOCAL;
    }
}

EntityPropertyFlags EntityItemProperties::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    // Core
    CHECK_PROPERTY_CHANGE(PROP_SIMULATION_OWNER, simulationOwner);
    CHECK_PROPERTY_CHANGE(PROP_PARENT_ID, parentID);
    CHECK_PROPERTY_CHANGE(PROP_PARENT_JOINT_INDEX, parentJointIndex);
    CHECK_PROPERTY_CHANGE(PROP_VISIBLE, visible);
    CHECK_PROPERTY_CHANGE(PROP_NAME, name);
    CHECK_PROPERTY_CHANGE(PROP_LOCKED, locked);
    CHECK_PROPERTY_CHANGE(PROP_USER_DATA, userData);
    CHECK_PROPERTY_CHANGE(PROP_PRIVATE_USER_DATA, privateUserData);
    CHECK_PROPERTY_CHANGE(PROP_HREF, href);
    CHECK_PROPERTY_CHANGE(PROP_DESCRIPTION, description);
    CHECK_PROPERTY_CHANGE(PROP_POSITION, position);
    CHECK_PROPERTY_CHANGE(PROP_DIMENSIONS, dimensions);
    CHECK_PROPERTY_CHANGE(PROP_ROTATION, rotation);
    CHECK_PROPERTY_CHANGE(PROP_REGISTRATION_POINT, registrationPoint);
    CHECK_PROPERTY_CHANGE(PROP_CREATED, created);
    CHECK_PROPERTY_CHANGE(PROP_LAST_EDITED_BY, lastEditedBy);
    CHECK_PROPERTY_CHANGE(PROP_ENTITY_HOST_TYPE, entityHostType);
    CHECK_PROPERTY_CHANGE(PROP_OWNING_AVATAR_ID, owningAvatarID);
    CHECK_PROPERTY_CHANGE(PROP_QUERY_AA_CUBE, queryAACube);
    CHECK_PROPERTY_CHANGE(PROP_CAN_CAST_SHADOW, canCastShadow);
    CHECK_PROPERTY_CHANGE(PROP_VISIBLE_IN_SECONDARY_CAMERA, isVisibleInSecondaryCamera);
    CHECK_PROPERTY_CHANGE(PROP_RENDER_LAYER, renderLayer);
    CHECK_PROPERTY_CHANGE(PROP_PRIMITIVE_MODE, primitiveMode);
    CHECK_PROPERTY_CHANGE(PROP_IGNORE_PICK_INTERSECTION, ignorePickIntersection);
    CHECK_PROPERTY_CHANGE(PROP_RENDER_WITH_ZONES, renderWithZones);
    CHECK_PROPERTY_CHANGE(PROP_BILLBOARD_MODE, billboardMode);
    changedProperties += _grab.getChangedProperties();

    // Physics
    CHECK_PROPERTY_CHANGE(PROP_DENSITY, density);
    CHECK_PROPERTY_CHANGE(PROP_VELOCITY, velocity);
    CHECK_PROPERTY_CHANGE(PROP_ANGULAR_VELOCITY, angularVelocity);
    CHECK_PROPERTY_CHANGE(PROP_GRAVITY, gravity);
    CHECK_PROPERTY_CHANGE(PROP_ACCELERATION, acceleration);
    CHECK_PROPERTY_CHANGE(PROP_DAMPING, damping);
    CHECK_PROPERTY_CHANGE(PROP_ANGULAR_DAMPING, angularDamping);
    CHECK_PROPERTY_CHANGE(PROP_RESTITUTION, restitution);
    CHECK_PROPERTY_CHANGE(PROP_FRICTION, friction);
    CHECK_PROPERTY_CHANGE(PROP_LIFETIME, lifetime);
    CHECK_PROPERTY_CHANGE(PROP_COLLISIONLESS, collisionless);
    CHECK_PROPERTY_CHANGE(PROP_COLLISION_MASK, collisionMask);
    CHECK_PROPERTY_CHANGE(PROP_DYNAMIC, dynamic);
    CHECK_PROPERTY_CHANGE(PROP_COLLISION_SOUND_URL, collisionSoundURL);
    CHECK_PROPERTY_CHANGE(PROP_ACTION_DATA, actionData);

    // Cloning
    CHECK_PROPERTY_CHANGE(PROP_CLONEABLE, cloneable);
    CHECK_PROPERTY_CHANGE(PROP_CLONE_LIFETIME, cloneLifetime);
    CHECK_PROPERTY_CHANGE(PROP_CLONE_LIMIT, cloneLimit);
    CHECK_PROPERTY_CHANGE(PROP_CLONE_DYNAMIC, cloneDynamic);
    CHECK_PROPERTY_CHANGE(PROP_CLONE_AVATAR_ENTITY, cloneAvatarEntity);
    CHECK_PROPERTY_CHANGE(PROP_CLONE_ORIGIN_ID, cloneOriginID);

    // Scripts
    CHECK_PROPERTY_CHANGE(PROP_SCRIPT, script);
    CHECK_PROPERTY_CHANGE(PROP_SCRIPT_TIMESTAMP, scriptTimestamp);
    CHECK_PROPERTY_CHANGE(PROP_SERVER_SCRIPTS, serverScripts);

    // Certifiable Properties
    CHECK_PROPERTY_CHANGE(PROP_ITEM_NAME, itemName);
    CHECK_PROPERTY_CHANGE(PROP_ITEM_DESCRIPTION, itemDescription);
    CHECK_PROPERTY_CHANGE(PROP_ITEM_CATEGORIES, itemCategories);
    CHECK_PROPERTY_CHANGE(PROP_ITEM_ARTIST, itemArtist);
    CHECK_PROPERTY_CHANGE(PROP_ITEM_LICENSE, itemLicense);
    CHECK_PROPERTY_CHANGE(PROP_LIMITED_RUN, limitedRun);
    CHECK_PROPERTY_CHANGE(PROP_MARKETPLACE_ID, marketplaceID);
    CHECK_PROPERTY_CHANGE(PROP_EDITION_NUMBER, editionNumber);
    CHECK_PROPERTY_CHANGE(PROP_ENTITY_INSTANCE_NUMBER, entityInstanceNumber);
    CHECK_PROPERTY_CHANGE(PROP_CERTIFICATE_ID, certificateID);
    CHECK_PROPERTY_CHANGE(PROP_CERTIFICATE_TYPE, certificateType);
    CHECK_PROPERTY_CHANGE(PROP_STATIC_CERTIFICATE_VERSION, staticCertificateVersion);

    // Location data for scripts
    CHECK_PROPERTY_CHANGE(PROP_LOCAL_POSITION, localPosition);
    CHECK_PROPERTY_CHANGE(PROP_LOCAL_ROTATION, localRotation);
    CHECK_PROPERTY_CHANGE(PROP_LOCAL_VELOCITY, localVelocity);
    CHECK_PROPERTY_CHANGE(PROP_LOCAL_ANGULAR_VELOCITY, localAngularVelocity);
    CHECK_PROPERTY_CHANGE(PROP_LOCAL_DIMENSIONS, localDimensions);

    // Common
    CHECK_PROPERTY_CHANGE(PROP_SHAPE_TYPE, shapeType);
    CHECK_PROPERTY_CHANGE(PROP_COMPOUND_SHAPE_URL, compoundShapeURL);
    CHECK_PROPERTY_CHANGE(PROP_COLOR, color);
    CHECK_PROPERTY_CHANGE(PROP_ALPHA, alpha);
    changedProperties += _pulse.getChangedProperties();
    CHECK_PROPERTY_CHANGE(PROP_TEXTURES, textures);

    // Particles
    CHECK_PROPERTY_CHANGE(PROP_MAX_PARTICLES, maxParticles);
    CHECK_PROPERTY_CHANGE(PROP_LIFESPAN, lifespan);
    CHECK_PROPERTY_CHANGE(PROP_EMITTING_PARTICLES, isEmitting);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_RATE, emitRate);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_SPEED, emitSpeed);
    CHECK_PROPERTY_CHANGE(PROP_SPEED_SPREAD, speedSpread);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_ORIENTATION, emitOrientation);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_DIMENSIONS, emitDimensions);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_RADIUS_START, emitRadiusStart);
    CHECK_PROPERTY_CHANGE(PROP_POLAR_START, polarStart);
    CHECK_PROPERTY_CHANGE(PROP_POLAR_FINISH, polarFinish);
    CHECK_PROPERTY_CHANGE(PROP_AZIMUTH_START, azimuthStart);
    CHECK_PROPERTY_CHANGE(PROP_AZIMUTH_FINISH, azimuthFinish);
    CHECK_PROPERTY_CHANGE(PROP_EMIT_ACCELERATION, emitAcceleration);
    CHECK_PROPERTY_CHANGE(PROP_ACCELERATION_SPREAD, accelerationSpread);
    CHECK_PROPERTY_CHANGE(PROP_PARTICLE_RADIUS, particleRadius);
    CHECK_PROPERTY_CHANGE(PROP_RADIUS_SPREAD, radiusSpread);
    CHECK_PROPERTY_CHANGE(PROP_RADIUS_START, radiusStart);
    CHECK_PROPERTY_CHANGE(PROP_RADIUS_FINISH, radiusFinish);
    CHECK_PROPERTY_CHANGE(PROP_COLOR_SPREAD, colorSpread);
    CHECK_PROPERTY_CHANGE(PROP_COLOR_START, colorStart);
    CHECK_PROPERTY_CHANGE(PROP_COLOR_FINISH, colorFinish);
    CHECK_PROPERTY_CHANGE(PROP_ALPHA_SPREAD, alphaSpread);
    CHECK_PROPERTY_CHANGE(PROP_ALPHA_START, alphaStart);
    CHECK_PROPERTY_CHANGE(PROP_ALPHA_FINISH, alphaFinish);
    CHECK_PROPERTY_CHANGE(PROP_EMITTER_SHOULD_TRAIL, emitterShouldTrail);
    CHECK_PROPERTY_CHANGE(PROP_PARTICLE_SPIN, particleSpin);
    CHECK_PROPERTY_CHANGE(PROP_SPIN_SPREAD, spinSpread);
    CHECK_PROPERTY_CHANGE(PROP_SPIN_START, spinStart);
    CHECK_PROPERTY_CHANGE(PROP_SPIN_FINISH, spinFinish);
    CHECK_PROPERTY_CHANGE(PROP_PARTICLE_ROTATE_WITH_ENTITY, rotateWithEntity);

    // Model
    CHECK_PROPERTY_CHANGE(PROP_MODEL_URL, modelURL);
    CHECK_PROPERTY_CHANGE(PROP_MODEL_SCALE, modelScale);
    CHECK_PROPERTY_CHANGE(PROP_JOINT_ROTATIONS_SET, jointRotationsSet);
    CHECK_PROPERTY_CHANGE(PROP_JOINT_ROTATIONS, jointRotations);
    CHECK_PROPERTY_CHANGE(PROP_JOINT_TRANSLATIONS_SET, jointTranslationsSet);
    CHECK_PROPERTY_CHANGE(PROP_JOINT_TRANSLATIONS, jointTranslations);
    CHECK_PROPERTY_CHANGE(PROP_RELAY_PARENT_JOINTS, relayParentJoints);
    CHECK_PROPERTY_CHANGE(PROP_GROUP_CULLED, groupCulled);
    CHECK_PROPERTY_CHANGE(PROP_BLENDSHAPE_COEFFICIENTS, blendshapeCoefficients);
    CHECK_PROPERTY_CHANGE(PROP_USE_ORIGINAL_PIVOT, useOriginalPivot);
    changedProperties += _animation.getChangedProperties();

    // Light
    CHECK_PROPERTY_CHANGE(PROP_IS_SPOTLIGHT, isSpotlight);
    CHECK_PROPERTY_CHANGE(PROP_INTENSITY, intensity);
    CHECK_PROPERTY_CHANGE(PROP_EXPONENT, exponent);
    CHECK_PROPERTY_CHANGE(PROP_CUTOFF, cutoff);
    CHECK_PROPERTY_CHANGE(PROP_FALLOFF_RADIUS, falloffRadius);

    // Text
    CHECK_PROPERTY_CHANGE(PROP_TEXT, text);
    CHECK_PROPERTY_CHANGE(PROP_LINE_HEIGHT, lineHeight);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_COLOR, textColor);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_ALPHA, textAlpha);
    CHECK_PROPERTY_CHANGE(PROP_BACKGROUND_COLOR, backgroundColor);
    CHECK_PROPERTY_CHANGE(PROP_BACKGROUND_ALPHA, backgroundAlpha);
    CHECK_PROPERTY_CHANGE(PROP_LEFT_MARGIN, leftMargin);
    CHECK_PROPERTY_CHANGE(PROP_RIGHT_MARGIN, rightMargin);
    CHECK_PROPERTY_CHANGE(PROP_TOP_MARGIN, topMargin);
    CHECK_PROPERTY_CHANGE(PROP_BOTTOM_MARGIN, bottomMargin);
    CHECK_PROPERTY_CHANGE(PROP_UNLIT, unlit);
    CHECK_PROPERTY_CHANGE(PROP_FONT, font);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_EFFECT, textEffect);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_EFFECT_COLOR, textEffectColor);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_EFFECT_THICKNESS, textEffectThickness);
    CHECK_PROPERTY_CHANGE(PROP_TEXT_ALIGNMENT, alignment);

    // Zone
    changedProperties += _keyLight.getChangedProperties();
    changedProperties += _ambientLight.getChangedProperties();
    changedProperties += _skybox.getChangedProperties();
    changedProperties += _haze.getChangedProperties();
    changedProperties += _bloom.getChangedProperties();
    CHECK_PROPERTY_CHANGE(PROP_FLYING_ALLOWED, flyingAllowed);
    CHECK_PROPERTY_CHANGE(PROP_GHOSTING_ALLOWED, ghostingAllowed);
    CHECK_PROPERTY_CHANGE(PROP_FILTER_URL, filterURL);
    CHECK_PROPERTY_CHANGE(PROP_KEY_LIGHT_MODE, keyLightMode);
    CHECK_PROPERTY_CHANGE(PROP_AMBIENT_LIGHT_MODE, ambientLightMode);
    CHECK_PROPERTY_CHANGE(PROP_SKYBOX_MODE, skyboxMode);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_MODE, hazeMode);
    CHECK_PROPERTY_CHANGE(PROP_BLOOM_MODE, bloomMode);
    CHECK_PROPERTY_CHANGE(PROP_AVATAR_PRIORITY, avatarPriority);
    CHECK_PROPERTY_CHANGE(PROP_SCREENSHARE, screenshare);

    // Polyvox
    CHECK_PROPERTY_CHANGE(PROP_VOXEL_VOLUME_SIZE, voxelVolumeSize);
    CHECK_PROPERTY_CHANGE(PROP_VOXEL_DATA, voxelData);
    CHECK_PROPERTY_CHANGE(PROP_VOXEL_SURFACE_STYLE, voxelSurfaceStyle);
    CHECK_PROPERTY_CHANGE(PROP_X_TEXTURE_URL, xTextureURL);
    CHECK_PROPERTY_CHANGE(PROP_Y_TEXTURE_URL, yTextureURL);
    CHECK_PROPERTY_CHANGE(PROP_Z_TEXTURE_URL, zTextureURL);
    CHECK_PROPERTY_CHANGE(PROP_X_N_NEIGHBOR_ID, xNNeighborID);
    CHECK_PROPERTY_CHANGE(PROP_Y_N_NEIGHBOR_ID, yNNeighborID);
    CHECK_PROPERTY_CHANGE(PROP_Z_N_NEIGHBOR_ID, zNNeighborID);
    CHECK_PROPERTY_CHANGE(PROP_X_P_NEIGHBOR_ID, xPNeighborID);
    CHECK_PROPERTY_CHANGE(PROP_Y_P_NEIGHBOR_ID, yPNeighborID);
    CHECK_PROPERTY_CHANGE(PROP_Z_P_NEIGHBOR_ID, zPNeighborID);

    // Web
    CHECK_PROPERTY_CHANGE(PROP_SOURCE_URL, sourceUrl);
    CHECK_PROPERTY_CHANGE(PROP_DPI, dpi);
    CHECK_PROPERTY_CHANGE(PROP_SCRIPT_URL, scriptURL);
    CHECK_PROPERTY_CHANGE(PROP_MAX_FPS, maxFPS);
    CHECK_PROPERTY_CHANGE(PROP_INPUT_MODE, inputMode);
    CHECK_PROPERTY_CHANGE(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, showKeyboardFocusHighlight);
    CHECK_PROPERTY_CHANGE(PROP_WEB_USE_BACKGROUND, useBackground);
    CHECK_PROPERTY_CHANGE(PROP_USER_AGENT, userAgent);

    // Polyline
    CHECK_PROPERTY_CHANGE(PROP_LINE_POINTS, linePoints);
    CHECK_PROPERTY_CHANGE(PROP_STROKE_WIDTHS, strokeWidths);
    CHECK_PROPERTY_CHANGE(PROP_STROKE_NORMALS, normals);
    CHECK_PROPERTY_CHANGE(PROP_STROKE_COLORS, strokeColors);
    CHECK_PROPERTY_CHANGE(PROP_IS_UV_MODE_STRETCH, isUVModeStretch);
    CHECK_PROPERTY_CHANGE(PROP_LINE_GLOW, glow);
    CHECK_PROPERTY_CHANGE(PROP_LINE_FACE_CAMERA, faceCamera);

    // Shape
    CHECK_PROPERTY_CHANGE(PROP_SHAPE, shape);

    // Material
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_URL, materialURL);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_MAPPING_MODE, materialMappingMode);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_PRIORITY, priority);
    CHECK_PROPERTY_CHANGE(PROP_PARENT_MATERIAL_NAME, parentMaterialName);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_MAPPING_POS, materialMappingPos);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_MAPPING_SCALE, materialMappingScale);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_MAPPING_ROT, materialMappingRot);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_DATA, materialData);
    CHECK_PROPERTY_CHANGE(PROP_MATERIAL_REPEAT, materialRepeat);

    // Image
    CHECK_PROPERTY_CHANGE(PROP_IMAGE_URL, imageURL);
    CHECK_PROPERTY_CHANGE(PROP_EMISSIVE, emissive);
    CHECK_PROPERTY_CHANGE(PROP_KEEP_ASPECT_RATIO, keepAspectRatio);
    CHECK_PROPERTY_CHANGE(PROP_SUB_IMAGE, subImage);

    // Grid
    CHECK_PROPERTY_CHANGE(PROP_GRID_FOLLOW_CAMERA, followCamera);
    CHECK_PROPERTY_CHANGE(PROP_MAJOR_GRID_EVERY, majorGridEvery);
    CHECK_PROPERTY_CHANGE(PROP_MINOR_GRID_EVERY, minorGridEvery);

    // Gizmo
    CHECK_PROPERTY_CHANGE(PROP_GIZMO_TYPE, gizmoType);
    changedProperties += _ring.getChangedProperties();

    return changedProperties;
}

/*@jsdoc
 * Different entity types have different properties: some common to all entities (listed in the table) and some specific to
 * each {@link Entities.EntityType|EntityType} (linked to below).
 *
 * @typedef {object} Entities.EntityProperties
 * @property {Uuid} id - The ID of the entity. <em>Read-only.</em>
 * @property {string} name="" - A name for the entity. Need not be unique.
 * @property {Entities.EntityType} type - The entity's type. You cannot change the type of an entity after it's created.
 *     However, its value may switch among <code>"Box"</code>, <code>"Shape"</code>, and <code>"Sphere"</code> depending on
 *     changes to the <code>shape</code> property set for entities of these types. <em>Read-only.</em>
 *
 * @property {Entities.EntityHostType} entityHostType="domain" - How the entity is hosted and sent to others for display.
 *     The value can only be set at entity creation by one of the {@link Entities.addEntity} methods. <em>Read-only.</em>
 * @property {boolean} avatarEntity=false - <code>true</code> if the entity is an {@link Entities.EntityHostType|avatar entity},
 *     <code>false</code> if it isn't. The value is per the <code>entityHostType</code> property value, set at entity creation
 *     by one of the {@link Entities.addEntity} methods. <em>Read-only.</em>
 * @property {boolean} clientOnly=false - A synonym for <code>avatarEntity</code>. <em>Read-only.</em>
 * @property {boolean} localEntity=false - <code>true</code> if the entity is a {@link Entities.EntityHostType|local entity},
 *     <code>false</code> if it isn't. The value is per the <code>entityHostType</code> property value, set at entity creation
 *     by one of the {@link Entities.addEntity} methods. <em>Read-only.</em>
 *
 * @property {Uuid} owningAvatarID=Uuid.NULL - The session ID of the owning avatar if <code>avatarEntity</code> is
 *     <code>true</code>, otherwise {@link Uuid(0)|Uuid.NULL}. <em>Read-only.</em>
 *
 * @property {string} created - The UTC date and time that the entity was created, in ISO 8601 format as
 *     <code>yyyy-MM-ddTHH:mm:ssZ</code>. <em>Read-only.</em>
 * @property {number} age - The age of the entity in seconds since it was created. <em>Read-only.</em>
 * @property {string} ageAsText - The age of the entity since it was created, formatted as <code>h hours m minutes s
 *     seconds</code>.
 * @property {number} lifetime=-1 - How long an entity lives for, in seconds, before being automatically deleted. A value of
 *     <code>-1</code> means that the entity lives for ever.
 * @property {number} lastEdited - When the entity was last edited, expressed as the number of microseconds since
 *     1970-01-01T00:00:00 UTC. <em>Read-only.</em>
 * @property {Uuid} lastEditedBy - The session ID of the avatar or agent that most recently created or edited the entity.
 *     <em>Read-only.</em>
 *
 * @property {boolean} locked=false - <code>true</code> if properties other than <code>locked</code> cannot be changed and the
 *     entity cannot be deleted, <code>false</code> if all properties can be changed and the entity can be deleted.
 * @property {boolean} visible=true - <code>true</code> if the entity is rendered, <code>false</code> if it isn't.
 * @property {boolean} canCastShadow=true - <code>true</code> if the entity can cast a shadow, <code>false</code> if it can't.
 *     Currently applicable only to {@link Entities.EntityProperties-Model|Model} and
 *     {@link Entities.EntityProperties-Shape|Shape} entities. Shadows are cast if inside a
 *     {@link Entities.EntityProperties-Zone|Zone} entity with <code>castShadows</code> enabled in its <code>keyLight</code>
 *     property.
 * @property {boolean} isVisibleInSecondaryCamera=true - <code>true</code> if the entity is rendered in the secondary camera,
 *     <code>false</code> if it isn't.
 * @property {Entities.RenderLayer} renderLayer="world" - The layer that the entity renders in.
 * @property {Entities.PrimitiveMode} primitiveMode="solid" - How the entity's geometry is rendered.
 * @property {boolean} ignorePickIntersection=false - <code>true</code> if {@link Picks} and {@link RayPick} ignore the entity,
 *     <code>false</code> if they don't.
 *
 * @property {Vec3} position=0,0,0 - The position of the entity in world coordinates.
 * @property {Quat} rotation=0,0,0,1 - The orientation of the entity in world coordinates.
 * @property {Vec3} registrationPoint=0.5,0.5,0.5 - The point in the entity that is set to the entity's position and is rotated
 *      about, range {@link Vec3(0)|Vec3.ZERO} &ndash; {@link Vec3(0)|Vec3.ONE}. A value of {@link Vec3(0)|Vec3.ZERO} is the
 *      entity's minimum x, y, z corner; a value of {@link Vec3(0)|Vec3.ONE} is the entity's maximum x, y, z corner.
 *
 * @property {Vec3} naturalPosition=0,0,0 - The center of the entity's unscaled mesh model if it has one, otherwise
 *     {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
 * @property {Vec3} naturalDimensions - The dimensions of the entity's unscaled mesh model or image if it has one, otherwise
 *     {@link Vec3(0)|Vec3.ONE}. <em>Read-only.</em>
 *
 * @property {Vec3} velocity=0,0,0 - The linear velocity of the entity in m/s with respect to world coordinates.
 * @property {number} damping=0.39347 - How much the linear velocity of an entity slows down over time, range
 *     <code>0.0</code> &ndash; <code>1.0</code>. A higher damping value slows down the entity more quickly. The default value
 *     is for an exponential decay timescale of 2.0s, where it takes 2.0s for the movement to slow to <code>1/e = 0.368</code>
 *     of its initial value.
 * @property {Vec3} angularVelocity=0,0,0 - The angular velocity of the entity in rad/s with respect to its axes, about its
 *     registration point.
 * @property {number} angularDamping=0.39347 - How much the angular velocity of an entity slows down over time, range
 *     <code>0.0</code> &ndash; <code>1.0</code>. A higher damping value slows down the entity more quickly. The default value
 *     is for an exponential decay timescale of 2.0s, where it takes 2.0s for the movement to slow to <code>1/e = 0.368</code>
 *     of its initial value.
 *
 * @property {Vec3} gravity=0,0,0 - The acceleration due to gravity in m/s<sup>2</sup> that the entity should move with, in
 *     world coordinates. Use a value of <code>{ x: 0, y: -9.8, z: 0 }</code> to simulate Earth's gravity. Gravity is applied
 *     to an entity's motion only if its <code>dynamic</code> property is <code>true</code>.
 *     <p>If changing an entity's <code>gravity</code> from {@link Vec3(0)|Vec3.ZERO}, you need to give it a small
 *     <code>velocity</code> in order to kick off physics simulation.</p>
 * @property {Vec3} acceleration - The current, measured acceleration of the entity, in m/s<sup>2</sup>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {number} restitution=0.5 - The "bounciness" of an entity when it collides, range <code>0.0</code> &ndash;
 *     <code>0.99</code>. The higher the value, the more bouncy.
 * @property {number} friction=0.5 - How much an entity slows down when it's moving against another, range <code>0.0</code>
 *     &ndash; <code>10.0</code>. The higher the value, the more quickly it slows down. Examples: <code>0.1</code> for ice,
 *     <code>0.9</code> for sandpaper.
 * @property {number} density=1000 - The density of the entity in kg/m<sup>3</sup>, range <code>100</code> &ndash;
 *     <code>10000</code>. Examples: <code>100</code> for balsa wood, <code>10000</code> for silver. The density is used in
 *     conjunction with the entity's bounding box volume to work out its mass in the application of physics.
 *
 * @property {boolean} collisionless=false - <code>true</code> if the entity shouldn't collide, <code>false</code> if it
 *     collides with items per its <code>collisionMask</code> property.
 * @property {boolean} ignoreForCollisions - Synonym for <code>collisionless</code>.
 * @property {CollisionMask} collisionMask=31 - What types of items the entity should collide with.
 * @property {string} collidesWith="static,dynamic,kinematic,myAvatar,otherAvatar," - Synonym for <code>collisionMask</code>,
 *     in text format.
 * @property {string} collisionSoundURL="" - The sound that's played when the entity experiences a collision. Valid file
 *     formats are per {@link SoundObject}.
 * @property {boolean} dynamic=false - <code>true</code> if the entity's movement is affected by collisions, <code>false</code>
 *     if it isn't.
 * @property {boolean} collisionsWillMove - A synonym for <code>dynamic</code>.
 *
 * @property {string} href="" - A "hifi://" metaverse address that a user is teleported to when they click on the entity.
 * @property {string} description="" - A description of the <code>href</code> property value.
 *
 * @property {string} userData="" - Used to store extra data about the entity in JSON format.
 *     <p><strong>Warning:</strong> Other apps may also use this property, so make sure you handle data stored by other apps:
 *     edit only your bit and leave the rest of the data intact. You can use <code>JSON.parse()</code> to parse the string into
 *     a JavaScript object which you can manipulate the properties of, and use <code>JSON.stringify()</code> to convert the
 *     object into a string to put back in the property.</p>
 *
 * @property {string} privateUserData="" - Like <code>userData</code>, but only accessible by server entity scripts, assignment
 *     client scripts, and users who have "Can Get and Set Private User Data" permissions in the domain.
 *
 * @property {string} script="" - The URL of the client entity script, if any, that is attached to the entity.
 * @property {number} scriptTimestamp=0 - Used to indicate when the client entity script was loaded. Should be
 *     an integer number of milliseconds since midnight GMT on January 1, 1970 (e.g., as supplied by <code>Date.now()</code>.
 *     If you update the property's value, the <code>script</code> is re-downloaded and reloaded. This is how the "reload"
 *     button beside the "script URL" field in properties tab of the Create app works.
 * @property {string} serverScripts="" - The URL of the server entity script, if any, that is attached to the entity.
 *
 * @property {Uuid} parentID=Uuid.NULL - The ID of the entity or avatar that the entity is parented to. A value of
 *     {@link Uuid(0)|Uuid.NULL} is used if the entity is not parented.
 * @property {number} parentJointIndex=65535 - The joint of the entity or avatar that the entity is parented to. Use
 *     <code>65535</code> or <code>-1</code> to parent to the entity or avatar's position and orientation rather than a joint.
 * @property {Vec3} localPosition=0,0,0 - The position of the entity relative to its parent if the entity is parented,
 *     otherwise the same value as <code>position</code>. If the entity is parented to an avatar and is an avatar entity
 *     so that it scales with the avatar, this value remains the original local position value while the avatar scale changes.
 * @property {Quat} localRotation=0,0,0,1 - The rotation of the entity relative to its parent if the entity is parented,
 *     otherwise the same value as <code>rotation</code>.
 * @property {Vec3} localVelocity=0,0,0 - The velocity of the entity relative to its parent if the entity is parented,
 *     otherwise the same value as <code>velocity</code>.
 * @property {Vec3} localAngularVelocity=0,0,0 - The angular velocity of the entity relative to its parent if the entity is
 *     parented, otherwise the same value as <code>angularVelocity</code>.
 * @property {Vec3} localDimensions - The dimensions of the entity. If the entity is parented to an avatar and is an
 *     avatar entity so that it scales with the avatar, this value remains the original dimensions value while the
 *     avatar scale changes.
 *
 * @property {Entities.BoundingBox} boundingBox - The axis-aligned bounding box that tightly encloses the entity.
 *     <em>Read-only.</em>
 * @property {AACube} queryAACube - The axis-aligned cube that determines where the entity lives in the entity server's octree.
 *     The cube may be considerably larger than the entity in some situations, e.g., when the entity is grabbed by an avatar:
 *     the position of the entity is determined through avatar mixer updates and so the AA cube is expanded in order to reduce
 *     unnecessary entity server updates. Scripts should not change this property's value.
 *
 * @property {string} actionData="" - Base-64 encoded compressed dump of the actions associated with the entity. This property
 *     is typically not used in scripts directly; rather, functions that manipulate an entity's actions update it, e.g., 
 *     {@link Entities.addAction}. The size of this property increases with the number of actions. Because this property value 
 *     has to fit within a Vircadia datagram packet, there is a limit to the number of actions that an entity can have;
 *     edits which would result in overflow are rejected. <em>Read-only.</em>
 * @property {Entities.RenderInfo} renderInfo - Information on the cost of rendering the entity. Currently information is only
 *     provided for <code>Model</code> entities. <em>Read-only.</em>
 *
 * @property {boolean} cloneable=false - <code>true</code> if the domain or avatar entity can be cloned via
 *     {@link Entities.cloneEntity}, <code>false</code> if it can't be.
 * @property {number} cloneLifetime=300 - The entity lifetime for clones created from this entity.
 * @property {number} cloneLimit=0 - The total number of clones of this entity that can exist in the domain at any given time.
 * @property {boolean} cloneDynamic=false - <code>true</code> if clones created from this entity will have their
 *     <code>dynamic</code> property set to <code>true</code>, <code>false</code> if they won't.
 * @property {boolean} cloneAvatarEntity=false - <code>true</code> if clones created from this entity will be created as
 *     avatar entities, <code>false</code> if they won't be.
 * @property {Uuid} cloneOriginID - The ID of the entity that this entity was cloned from.
 *
 * @property {Uuid[]} renderWithZones=[]] - A list of entity IDs representing with which zones this entity should render.
 *     If it is empty, this entity will render normally.  Otherwise, this entity will only render if your avatar is within
 *     one of the zones in this list.
 * @property {BillboardMode} billboardMode="none" - Whether the entity is billboarded to face the camera.  Use the rotation
 *     property to control which axis is facing you.
 *
 * @property {Entities.Grab} grab - The entity's grab-related properties.
 *
 * @property {string} itemName="" - Certifiable name of the Marketplace item.
 * @property {string} itemDescription="" - Certifiable description of the Marketplace item.
 * @property {string} itemCategories="" - Certifiable category of the Marketplace item.
 * @property {string} itemArtist="" - Certifiable artist that created the Marketplace item.
 * @property {string} itemLicense="" - Certifiable license URL for the Marketplace item.
 * @property {number} limitedRun=4294967295 - Certifiable maximum integer number of editions (copies) of the Marketplace item
 *     allowed to be sold.
 * @property {number} editionNumber=0 - Certifiable integer edition (copy) number or the Marketplace item. Each copy sold in
 *     the Marketplace is numbered sequentially, starting at 1.
 * @property {number} entityInstanceNumber=0 - Certifiable integer instance number for identical entities in a Marketplace
 *     item. A Marketplace item may have multiple, identical parts. If so, then each is numbered sequentially with an instance
 *     number.
 * @property {string} marketplaceID="" - Certifiable UUID for the Marketplace item, as used in the URL of the item's download
 *     and its Marketplace Web page.
 * @property {string} certificateID="" - Hash of the entity's static certificate JSON, signed by the artist's private key.
 * @property {number} staticCertificateVersion=0 - The version of the method used to generate the <code>certificateID</code>.
 *
 * @comment The different entity types have additional properties as follows:
 * @see {@link Entities.EntityProperties-Box|EntityProperties-Box}
 * @see {@link Entities.EntityProperties-Gizmo|EntityProperties-Gizmo}
 * @see {@link Entities.EntityProperties-Grid|EntityProperties-Grid}
 * @see {@link Entities.EntityProperties-Image|EntityProperties-Image}
 * @see {@link Entities.EntityProperties-Light|EntityProperties-Light}
 * @see {@link Entities.EntityProperties-Line|EntityProperties-Line}
 * @see {@link Entities.EntityProperties-Material|EntityProperties-Material}
 * @see {@link Entities.EntityProperties-Model|EntityProperties-Model}
 * @see {@link Entities.EntityProperties-ParticleEffect|EntityProperties-ParticleEffect}
 * @see {@link Entities.EntityProperties-PolyLine|EntityProperties-PolyLine}
 * @see {@link Entities.EntityProperties-PolyVox|EntityProperties-PolyVox}
 * @see {@link Entities.EntityProperties-Shape|EntityProperties-Shape}
 * @see {@link Entities.EntityProperties-Sphere|EntityProperties-Sphere}
 * @see {@link Entities.EntityProperties-Text|EntityProperties-Text}
 * @see {@link Entities.EntityProperties-Web|EntityProperties-Web}
 * @see {@link Entities.EntityProperties-Zone|EntityProperties-Zone}
 */

/*@jsdoc
 * The <code>"Box"</code> {@link Entities.EntityType|EntityType} is the same as the <code>"Shape"</code>
 * {@link Entities.EntityType|EntityType} except that its <code>shape</code> value is always set to <code>"Cube"</code>
 * when the entity is created. If its <code>shape</code> property value is subsequently changed then the entity's
 * <code>type</code> will be reported as <code>"Sphere"</code> if the <code>shape</code> is set to <code>"Sphere"</code>,
 * otherwise it will be reported as <code>"Shape"</code>.
 *
 * @typedef {object} Entities.EntityProperties-Box
 * @see {@link Entities.EntityProperties-Shape|EntityProperties-Shape}
 */

/*@jsdoc
 * The <code>"Light"</code> {@link Entities.EntityType|EntityType} adds local lighting effects. It has properties in addition
 * to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Light
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity. Surfaces outside these dimensions are not lit
 *     by the light.
 * @property {Color} color=255,255,255 - The color of the light emitted.
 * @property {number} intensity=1 - The brightness of the light.
 * @property {number} falloffRadius=0.1 - The distance from the light's center at which intensity is reduced by 25%.
 * @property {boolean} isSpotlight=false - <code>true</code> if the light is directional, emitting along the entity's
 *     local negative z-axis; <code>false</code> if the light is a point light which emanates in all directions.
 * @property {number} exponent=0 - Affects the softness of the spotlight beam: the higher the value the softer the beam.
 * @property {number} cutoff=1.57 - Affects the size of the spotlight beam: the higher the value the larger the beam.
 * @example <caption>Create a spotlight pointing at the ground.</caption>
 * Entities.addEntity({
 *     type: "Light",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -4 })),
 *     rotation: Quat.fromPitchYawRollDegrees(-75, 0, 0),
 *     dimensions: { x: 5, y: 5, z: 5 },
 *     intensity: 100,
 *     falloffRadius: 0.3,
 *     isSpotlight: true,
 *     exponent: 20,
 *     cutoff: 30,
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Line"</code> {@link Entities.EntityType|EntityType} draws thin, straight lines between a sequence of two or more
 * points. It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 * <p class=important>Deprecated: Use {@link Entities.EntityProperties-PolyLine|PolyLine} entities instead.</p>
 *
 * @typedef {object} Entities.EntityProperties-Line
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity. Must be sufficient to contain all the
 *     <code>linePoints</code>.
 * @property {Vec3[]} linePoints=[]] - The sequence of points to draw lines between. The values are relative to the entity's
 *     position. A maximum of 70 points can be specified. The property's value is set only if all the <code>linePoints</code>
 *     lie within the entity's <code>dimensions</code>.
 * @property {Color} color=255,255,255 - The color of the line.
 * @example <caption>Draw lines in a "V".</caption>
 * var entity = Entities.addEntity({
 *     type: "Line",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -5 })),
 *     rotation: MyAvatar.orientation,
 *     dimensions: { x: 2, y: 2, z: 1 },
 *     linePoints: [
 *         { x: -1, y: 1, z: 0 },
 *         { x: 0, y: -1, z: 0 },
 *         { x: 1, y: 1, z: 0 },
 *     ],
 *     color: { red: 255, green: 0, blue: 0 },
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Material"</code> {@link Entities.EntityType|EntityType} modifies existing materials on entities and avatars. It
 * has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 * <p>To apply a material to an entity, set the material entity's <code>parentID</code> property to the entity ID.
 * To apply a material to an avatar, set the material entity's <code>parentID</code> property to the avatar's session UUID.
 * To apply a material to your avatar such that it persists across domains and log-ins, create the material as an avatar entity
 * by setting the <code>entityHostType</code> parameter in {@link Entities.addEntity} to <code>"avatar"</code> and set the
 * entity's <code>parentID</code> property to <code>MyAvatar.SELF_ID</code>.
 * Material entities render as non-scalable spheres if they don't have their parent set.</p>
 *
 * @typedef {object} Entities.EntityProperties-Material
 * @property {Vec3} dimensions=0.1,0.1,0.1 - Used when <code>materialMappingMode == "projected"</code>.
 * @property {string} materialURL="" - URL to a {@link Entities.MaterialResource|MaterialResource}. If you append
 *     <code>"#name"</code> to the URL, the  material with that name in the {@link Entities.MaterialResource|MaterialResource}
 *     will be applied to the entity. Alternatively, set the property value to <code>"materialData"</code> to use the
 *     <code>materialData</code> property for the {@link Entities.MaterialResource|MaterialResource} values.
 * @property {string} materialData="" - Used to store {@link Entities.MaterialResource|MaterialResource} data as a JSON string.
 *     You can use <code>JSON.parse()</code> to parse the string into a JavaScript object which you can manipulate the
 *     properties of, and use <code>JSON.stringify()</code> to convert the object into a string to put in the property.
 * @property {number} priority=0 - The priority for applying the material to its parent. Only the highest priority material is
 *     applied, with materials of the same priority randomly assigned. Materials that come with the model have a priority of
 *     <code>0</code>.
 * @property {string} parentMaterialName="0" - Selects the mesh part or parts within the parent to which to apply the material.
 *     If in the format <code>"mat::string"</code>, all mesh parts with material name <code>"string"</code> are replaced.
 *     If <code>"all"</code>, then all mesh parts are replaced.
 *     Otherwise the property value is parsed as an unsigned integer, specifying the mesh part index to modify.
 *     <p>If the string represents an array (starts with <code>"["</code> and ends with <code>"]"</code>), the string is split
 *     at each <code>","</code> and each element parsed as either a number or a string if it starts with <code>"mat::"</code>.
 *     For example, <code>"[0,1,mat::string,mat::string2]"</code> will replace mesh parts 0 and 1, and any mesh parts with
 *     material <code>"string"</code> or <code>"string2"</code>. Do not put spaces around the commas. Invalid values are parsed
 *     to <code>0</code>.</p>
 * @property {string} materialMappingMode="uv" - How the material is mapped to the entity. Either <code>"uv"</code> or
 *     <code>"projected"</code>. In <code>"uv"</code> mode, the material is evaluated within the UV space of the mesh it is
 *     applied to. In <code>"projected"</code> mode, the 3D transform (position, rotation, and dimensions) of the Material
 *     entity is used to evaluate the texture coordinates for the material.
 * @property {Vec2} materialMappingPos=0,0 - Offset position in UV-space of the top left of the material, range
 *     <code>{ x: 0, y: 0 }</code> &ndash; <code>{ x: 1, y: 1 }</code>.
 * @property {Vec2} materialMappingScale=1,1 - How much to scale the material within the parent's UV-space.
 * @property {number} materialMappingRot=0 - How much to rotate the material within the parent's UV-space, in degrees.
 * @property {boolean} materialRepeat=true - <code>true</code> if the material repeats, <code>false</code> if it doesn't. If
 *     <code>false</code>, fragments outside of texCoord 0 &ndash; 1 will be discarded. Works in both <code>"uv"</code> and
 *     <code>"projected"</code> modes.
 * @example <caption>Color a sphere using a Material entity.</caption>
 * var entityID = Entities.addEntity({
 *     type: "Sphere",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
 *     dimensions: { x: 1, y: 1, z: 1 },
 *     color: { red: 128, green: 128, blue: 128 },
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 *
 * var materialID = Entities.addEntity({
 *     type: "Material",
 *     parentID: entityID,
 *     materialURL: "materialData",
 *     priority: 1,
 *     materialData: JSON.stringify({
 *         materialVersion: 1,
 *         materials: {
 *             // Value overrides entity's "color" property.
 *             albedo: [1.0, 1.0, 0]  // Yellow
 *         }
 *     })
 * });
 */

/*@jsdoc
 * The <code>"Model"</code> {@link Entities.EntityType|EntityType} displays a glTF, FBX, or OBJ model. When adding an entity,
 * if no <code>dimensions</code> value is specified then the model is automatically sized to its
 * <code>{@link Entities.EntityProperties|naturalDimensions}</code>. It has properties in addition to the common
 * {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Model
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity. When adding an entity, if no <code>dimensions</code>
 *     value is specified then the model is automatically sized to its
 *     <code>{@link Entities.EntityProperties|naturalDimensions}</code>.
 * @property {string} modelURL="" - The URL of the glTF, FBX, or OBJ model. glTF models may be in JSON or binary format
 *     (".gltf" or ".glb" URLs respectively). Baked models' URLs have ".baked" before the file type. Model files may also be
 *     compressed in GZ format, in which case the URL ends in ".gz".
 * @property {Vec3} modelScale - The scale factor applied to the model's dimensions.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {string} blendshapeCoefficients - A JSON string of a map of blendshape names to values.  Only stores set values.
 *     When editing this property, only coefficients that you are editing will change; it will not explicitly reset other
 *     coefficients.
 * @property {boolean} useOriginalPivot=false - If <code>false</code>, the model will be centered based on its content,
 *     ignoring any offset in the model itself. If <code>true</code>, the model will respect its original offset.  Currently,
 *     only pivots relative to <code>{x: 0, y: 0, z: 0}</code> are supported.
 * @property {string} textures="" - A JSON string of texture name, URL pairs used when rendering the model in place of the
 *     model's original textures. Use a texture name from the <code>originalTextures</code> property to override that texture.
 *     Only the texture names and URLs to be overridden need be specified; original textures are used where there are no
 *     overrides. You can use <code>JSON.stringify()</code> to convert a JavaScript object of name, URL pairs into a JSON
 *     string.
 * @property {string} originalTextures="{}" - A JSON string of texture name, URL pairs used in the model. The property value is
 *     filled in after the entity has finished rezzing (i.e., textures have loaded). You can use <code>JSON.parse()</code> to
 *     parse the JSON string into a JavaScript object of name, URL pairs. <em>Read-only.</em>
 * @property {Color} color=255,255,255 - <em>Currently not used.</em>
 *
 * @property {ShapeType} shapeType="none" - The shape of the collision hull used if collisions are enabled.
 * @property {string} compoundShapeURL="" - The model file to use for the compound shape if <code>shapeType</code> is
 *     <code>"compound"</code>.
 *
 * @property {Entities.AnimationProperties} animation - An animation to play on the model.
 *
 * @property {Quat[]} jointRotations=[]] - Joint rotations applied to the model; <code>[]</code> if none are applied or the
 *     model hasn't loaded. The array indexes are per {@link Entities.getJointIndex|getJointIndex}. Rotations are relative to
 *     each joint's parent.
 *     <p>Joint rotations can be set by {@link Entities.setLocalJointRotation|setLocalJointRotation} and similar functions, or
 *     by setting the value of this property. If you set a joint rotation using this property, you also need to set the
 *     corresponding <code>jointRotationsSet</code> value to <code>true</code>.</p>
 * @property {boolean[]} jointRotationsSet=[]] - <code>true</code> values for joints that have had rotations applied,
 *     <code>false</code> otherwise; <code>[]</code> if none are applied or the model hasn't loaded. The array indexes are per
 *     {@link Entities.getJointIndex|getJointIndex}.
 * @property {Vec3[]} jointTranslations=[]] - Joint translations applied to the model; <code>[]</code> if none are applied or
 *     the model hasn't loaded. The array indexes are per {@link Entities.getJointIndex|getJointIndex}. Translations are
 *     relative to each joint's parent.
 *     <p>Joint translations can be set by {@link Entities.setLocalJointTranslation|setLocalJointTranslation} and similar
 *     functions, or by setting the value of this property. If you set a joint translation using this property you also need to
 *     set the corresponding <code>jointTranslationsSet</code> value to <code>true</code>.</p>
 * @property {boolean[]} jointTranslationsSet=[]] - <code>true</code> values for joints that have had translations applied,
 *     <code>false</code> otherwise; <code>[]</code> if none are applied or the model hasn't loaded. The array indexes are per
 *     {@link Entities.getJointIndex|getJointIndex}.
 * @property {boolean} relayParentJoints=false - <code>true</code> if when the entity is parented to an avatar, the avatar's
 *     joint rotations are applied to the entity's joints; <code>false</code> if a parent avatar's joint rotations are not
 *     applied to the entity's joints.
 * @property {boolean} groupCulled=false - <code>true</code> if the mesh parts of the model are LOD culled as a group,
 *     <code>false</code> if separate mesh parts are LOD culled individually.
 *
 * @example <caption>Rez a cowboy hat.</caption>
 * var entity = Entities.addEntity({
 *     type: "Model",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -2 })),
 *     rotation: MyAvatar.orientation,
 *     modelURL: "https://apidocs.vircadia.dev/models/cowboy-hat.fbx",
 *     dimensions: { x: 0.8569, y: 0.3960, z: 1.0744 },
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"ParticleEffect"</code> {@link Entities.EntityType|EntityType} displays a particle system that can be used to
 * simulate things such as fire, smoke, snow, magic spells, etc. The particles emanate from an ellipsoid or part thereof.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-ParticleEffect
 * @property {boolean} isEmitting=true - <code>true</code> if particles are being emitted, <code>false</code> if they aren't.
 * @property {number} maxParticles=1000 - The maximum number of particles to render at one time. Older particles are deleted if
 *     necessary when new ones are created.
 * @property {number} lifespan=3s - How long, in seconds, each particle lives.
 * @property {number} emitRate=15 - The number of particles per second to emit.
 * @property {number} emitSpeed=5 - The speed, in m/s, that each particle is emitted at.
 * @property {number} speedSpread=1 - The spread in speeds at which particles are emitted at. For example, if
 *     <code>emitSpeed == 5</code> and <code>speedSpread == 1</code>, particles will be emitted with speeds in the range
 *     <code>4</code> &ndash; <code>6</code>m/s.
 * @property {Vec3} emitAcceleration=0,-9.8,0 - The acceleration that is applied to each particle during its lifetime. The
 *     default is Earth's gravity value.
 * @property {Vec3} accelerationSpread=0,0,0 - The spread in accelerations that each particle is given. For example, if
 *     <code>emitAccelerations == {x: 0, y: -9.8, z: 0}</code> and <code>accelerationSpread ==
 *     {x: 0, y: 1, z: 0}</code>, each particle will have an acceleration in the range <code>{x: 0, y: -10.8, z: 0}</code>
 *     &ndash; <code>{x: 0, y: -8.8, z: 0}</code>.
 * @property {Vec3} dimensions - The dimensions of the particle effect, i.e., a bounding box containing all the particles
 *     during their lifetimes, assuming that <code>emitterShouldTrail == false</code>. <em>Read-only.</em>
 * @property {boolean} emitterShouldTrail=false - <code>true</code> if particles are "left behind" as the emitter moves,
 *     <code>false</code> if they stay within the entity's dimensions.
 *
 * @property {Quat} emitOrientation=-0.707,0,0,0.707 - The orientation of particle emission relative to the entity's axes. By
 *     default, particles emit along the entity's local z-axis, and <code>azimuthStart</code> and <code>azimuthFinish</code>
 *     are relative to the entity's local x-axis. The default value is a rotation of -90 degrees about the local x-axis, i.e.,
 *     the particles emit vertically.
 *
 * @property {ShapeType} shapeType="ellipsoid" - The shape from which particles are emitted.
 * @property {string} compoundShapeURL="" - The model file to use for the compound shape if <code>shapeType ==
 *     "compound"</code>.
 * @property {Vec3} emitDimensions=0,0,0 - The dimensions of the shape from which particles are emitted.
 * @property {number} emitRadiusStart=1 - The starting radius within the shape at which particles start being emitted;
 *     range <code>0.0</code> &ndash; <code>1.0</code> for the center to the surface, respectively.
 *     Particles are emitted from the portion of the shape that lies between <code>emitRadiusStart</code> and the
 *     shape's surface.
 * @property {number} polarStart=0 - The angle in radians from the entity's local z-axis at which particles start being emitted
 *     within the shape; range <code>0</code> &ndash; <code>Math.PI</code>. Particles are emitted from the portion of the
 *     shape that lies between <code>polarStart</code> and <code>polarFinish</code>. Only used if <code>shapeType</code> is
 *     <code>"ellipsoid"</code> or <code>"sphere"</code>.
 * @property {number} polarFinish=0 - The angle in radians from the entity's local z-axis at which particles stop being emitted
 *     within the shape; range <code>0</code> &ndash; <code>Math.PI</code>. Particles are emitted from the portion of the
 *     shape that lies between <code>polarStart</code> and <code>polarFinish</code>. Only used if <code>shapeType</code> is
 *     <code>"ellipsoid"</code> or <code>"sphere"</code>.
 * @property {number} azimuthStart=-Math.PI - The angle in radians from the entity's local x-axis about the entity's local
 *     z-axis at which particles start being emitted; range <code>-Math.PI</code> &ndash; <code>Math.PI</code>. Particles are
 *     emitted from the portion of the shape that lies between <code>azimuthStart</code> and <code>azimuthFinish</code>.
 *     Only used if <code>shapeType</code> is <code>"ellipsoid"</code>, <code>"sphere"</code>, or <code>"circle"</code>.
 * @property {number} azimuthFinish=Math.PI - The angle in radians from the entity's local x-axis about the entity's local
 *     z-axis at which particles stop being emitted; range <code>-Math.PI</code> &ndash; <code>Math.PI</code>. Particles are
 *     emitted from the portion of the shape that lies between <code>azimuthStart</code> and <code>azimuthFinish</code>.
 *     Only used if <code>shapeType</code> is <code>"ellipsoid"</code>, <code>"sphere"</code>, or <code>"circle"</code>.
 *
 * @property {string} textures="" - The URL of a JPG or PNG image file to display for each particle. If you want transparency,
 *     use PNG format.
 * @property {number} particleRadius=0.025 - The radius of each particle at the middle of its life.
 * @property {number} radiusStart=null - The radius of each particle at the start of its life. If <code>null</code>, the
 *     <code>particleRadius</code> value is used.
 * @property {number} radiusFinish=null - The radius of each particle at the end of its life. If <code>null</code>, the
 *     <code>particleRadius</code> value is used.
 * @property {number} radiusSpread=0 - The spread in radius that each particle is given. For example, if
 *     <code>particleRadius == 0.5</code> and <code>radiusSpread == 0.25</code>, each particle will have a radius in the range
 *     <code>0.25</code> &ndash; <code>0.75</code>.
 * @property {Color} color=255,255,255 - The color of each particle at the middle of its life.
 * @property {ColorFloat} colorStart=null,null,null - The color of each particle at the start of its life. If any of the
 *     component values are undefined, the <code>color</code> value is used.
 * @property {ColorFloat} colorFinish=null,null,null - The color of each particle at the end of its life. If any of the
 *     component values are undefined, the <code>color</code> value is used.
 * @property {Color} colorSpread=0,0,0 - The spread in color that each particle is given. For example, if
 *     <code>color == {red: 100, green: 100, blue: 100}</code> and <code>colorSpread ==
 *     {red: 10, green: 25, blue: 50}</code>, each particle will have a color in the range
 *     <code>{red: 90, green: 75, blue: 50}</code> &ndash; <code>{red: 110, green: 125, blue: 150}</code>.
 * @property {number} alpha=1 - The opacity of each particle at the middle of its life.
 * @property {number} alphaStart=null - The opacity of each particle at the start of its life. If <code>null</code>, the
 *     <code>alpha</code> value is used.
 * @property {number} alphaFinish=null - The opacity of each particle at the end of its life. If <code>null</code>, the
 *     <code>alpha</code> value is used.
 * @property {number} alphaSpread=0 - The spread in alpha that each particle is given. For example, if
 *     <code>alpha == 0.5</code> and <code>alphaSpread == 0.25</code>, each particle will have an alpha in the range
 *     <code>0.25</code> &ndash; <code>0.75</code>.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {number} particleSpin=0 - The rotation of each particle at the middle of its life, range <code>-2 * Math.PI</code>
 *     &ndash; <code>2 * Math.PI</code> radians.
 * @property {number} spinStart=null - The rotation of each particle at the start of its life, range <code>-2 * Math.PI</code>
 *     &ndash; <code>2 * Math.PI</code> radians. If <code>null</code>, the <code>particleSpin</code> value is used.
 * @property {number} spinFinish=null - The rotation of each particle at the end of its life, range <code>-2 * Math.PI</code>
 *     &ndash; <code>2 * Math.PI</code> radians. If <code>null</code>, the <code>particleSpin</code> value is used.
 * @property {number} spinSpread=0 - The spread in spin that each particle is given, range <code>0</code> &ndash;
 *     <code>2 * Math.PI</code> radians. For example, if <code>particleSpin == Math.PI</code> and
 *     <code>spinSpread == Math.PI / 2</code>, each particle will have a rotation in the range <code>Math.PI / 2</code> &ndash;
 *     <code>3 * Math.PI / 2</code>.
 * @property {boolean} rotateWithEntity=false - <code>true</code> if the particles' rotations are relative to the entity's
 *     instantaneous rotation, <code>false</code> if they're relative to world coordinates. If <code>true</code> with
 *     <code>particleSpin == 0</code>, the particles keep oriented per the entity's orientation.
 *
 * @example <caption>Create a ball of green smoke.</caption>
 * particles = Entities.addEntity({
 *     type: "ParticleEffect",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -4 })),
 *     lifespan: 5,
 *     emitRate: 10,
 *     emitSpeed: 0.02,
 *     speedSpread: 0.01,
 *     emitAcceleration: { x: 0, y: 0.02, z: 0 },
 *     polarFinish: Math.PI,
 *     textures: "https://content.vircadia.com/eu-c-1/vircadia-assets/interface/default/default_particle.png",
 *     particleRadius: 0.1,
 *     color: { red: 0, green: 255, blue: 0 },
 *     alphaFinish: 0,
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"PolyLine"</code> {@link Entities.EntityType|EntityType} draws textured, straight lines between a sequence of
 * points. It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-PolyLine
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity, i.e., the size of the bounding box that contains the
 *     lines drawn. <em>Read-only.</em>
 * @property {Vec3[]} linePoints=[]] - The sequence of points to draw lines between. The values are relative to the entity's
 *     position. A maximum of 70 points can be specified.
 * @property {Vec3[]} normals=[]] - The normal vectors for the line's surface at the <code>linePoints</code>. The values are
 *     relative to the entity's orientation. Must be specified in order for the entity to render.
 * @property {number[]} strokeWidths=[]] - The widths, in m, of the line at the <code>linePoints</code>. Must be specified in
 *     order for the entity to render.
 * @property {Vec3[]} strokeColors=[]] - The base colors of each point, with values in the range <code>0.0,0.0,0.0</code>
 *     &ndash; <code>1.0,1.0,1.0</code>. These colors are multiplied with the color of the texture. If there are more line
 *     points than stroke colors, the <code>color</code> property value is used for the remaining points.
 *     <p><strong>Warning:</strong> The ordinate values are in the range <code>0.0</code> &ndash; <code>1.0</code>.</p>
 * @property {Color} color=255,255,255 - Used as the color for each point if <code>strokeColors</code> doesn't have a value for
 *     the point.
 * @property {string} textures="" - The URL of a JPG or PNG texture to use for the lines. If you want transparency, use PNG
 *     format.
 * @property {boolean} isUVModeStretch=true - <code>true</code> if the texture is stretched to fill the whole line,
 *     <code>false</code> if the texture repeats along the line.
 * @property {boolean} glow=false - <code>true</code> if the opacity of the strokes drops off away from the line center,
 *     <code>false</code> if it doesn't.
 * @property {boolean} faceCamera=false - <code>true</code> if each line segment rotates to face the camera, <code>false</code>
 *     if they don't.
 * @example <caption>Draw a textured "V".</caption>
 * var entity = Entities.addEntity({
 *     type: "PolyLine",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -5 })),
 *     rotation: MyAvatar.orientation,
 *     linePoints: [
 *         { x: -1, y: 0.5, z: 0 },
 *         { x: 0, y: 0, z: 0 },
 *         { x: 1, y: 0.5, z: 0 }
 *     ],
 *     normals: [
 *         { x: 0, y: 0, z: 1 },
 *         { x: 0, y: 0, z: 1 },
 *         { x: 0, y: 0, z: 1 }
 *     ],
 *     strokeWidths: [ 0.1, 0.1, 0.1 ],
 *     color: { red: 255, green: 0, blue: 0 },  // Use just the red channel from the image.
 *     textures: "https://cdn-1.vircadia.com/us-e-1/DomainContent/Toybox/flowArts/trails.png",
 *     isUVModeStretch: true,
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"PolyVox"</code> {@link Entities.EntityType|EntityType} displays a set of textured voxels.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 * If you have two or more neighboring PolyVox entities of the same size abutting each other, you can display them as joined by
 * configuring their <code>voxelSurfaceStyle</code> and various neighbor ID properties.
 * <p>PolyVox entities uses a library from <a href="http://www.volumesoffun.com/">Volumes of Fun</a>. Their
 * <a href="http://www.volumesoffun.com/polyvox-documentation/">library documentation</a> may be useful to read.</p>
 *
 * @typedef {object} Entities.EntityProperties-PolyVox
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity.
 * @property {Vec3} voxelVolumeSize=32,32,32 - Integer number of voxels along each axis of the entity, in the range
 *     <code>1,1,1</code> to <code>128,128,128</code>. The dimensions of each voxel is
 *     <code>dimensions / voxelVolumesize</code>.
 * @property {string} voxelData="ABAAEAAQAAAAHgAAEAB42u3BAQ0AAADCoPdPbQ8HFAAAAPBuEAAAAQ==" - Base-64 encoded compressed dump of
 *     the PolyVox data. This property is typically not used in scripts directly; rather, functions that manipulate a PolyVox
 *     entity update it.
 *     <p>The size of this property increases with the size and complexity of the PolyVox entity, with the size depending on how 
 *     the particular entity's voxels compress. Because this property value has to fit within a Vircadia datagram packet, 
 *     there is a limit to the size and complexity of a PolyVox entity; edits which would result in an overflow are rejected.</p>
 * @property {Entities.PolyVoxSurfaceStyle} voxelSurfaceStyle=2 - The style of rendering the voxels' surface and how
 *     neighboring PolyVox entities are joined.
 * @property {string} xTextureURL="" - The URL of the texture to map to surfaces perpendicular to the entity's local x-axis.
 *     JPG or PNG format. If no texture is specified the surfaces display white.
 * @property {string} yTextureURL="" - The URL of the texture to map to surfaces perpendicular to the entity's local y-axis.
 *     JPG or PNG format. If no texture is specified the surfaces display white.
 * @property {string} zTextureURL="" - The URL of the texture to map to surfaces perpendicular to the entity's local z-axis.
 *     JPG or PNG format. If no texture is specified the surfaces display white.
 * @property {Uuid} xNNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's -ve local x-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @property {Uuid} yNNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's -ve local y-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @property {Uuid} zNNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's -ve local z-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @property {Uuid} xPNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's +ve local x-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @property {Uuid} yPNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's +ve local y-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @property {Uuid} zPNeighborID=Uuid.NULL - The ID of the neighboring PolyVox entity in the entity's +ve local z-axis
 *     direction, if you want them joined. Set to {@link Uuid(0)|Uuid.NULL} if there is none or you don't want to join them.
 * @example <caption>Create a textured PolyVox sphere.</caption>
 * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 }));
 * var texture = "http://public.highfidelity.com/cozza13/tuscany/Concrete2.jpg";
 * var polyVox = Entities.addEntity({
 *     type: "PolyVox",
 *     position: position,
 *     dimensions: { x: 2, y: 2, z: 2 },
 *     xTextureURL: texture,
 *     yTextureURL: texture,
 *     zTextureURL: texture,
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 * Entities.setVoxelSphere(polyVox, position, 0.8, 255);
 */

/*@jsdoc
 * The <code>"Shape"</code> {@link Entities.EntityType|EntityType} displays an entity of a specified <code>shape</code>.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Shape
 * @property {Entities.Shape} shape="Sphere" - The shape of the entity.
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the entity.
 * @property {Color} color=255,255,255 - The color of the entity.
 * @property {number} alpha=1 - The opacity of the entity, range <code>0.0</code> &ndash; <code>1.0</code>.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @example <caption>Create a cylinder.</caption>
 * var shape = Entities.addEntity({
 *     type: "Shape",
 *     shape: "Cylinder",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
 *     dimensions: { x: 0.4, y: 0.6, z: 0.4 },
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Sphere"</code> {@link Entities.EntityType|EntityType} is the same as the <code>"Shape"</code>
 * {@link Entities.EntityType|EntityType} except that its <code>shape</code> value is always set to <code>"Sphere"</code>
 * when the entity is created. If its <code>shape</code> property value is subsequently changed then the entity's
 * <code>type</code> will be reported as <code>"Box"</code> if the <code>shape</code> is set to <code>"Cube"</code>,
 * otherwise it will be reported as <code>"Shape"</code>.
 *
 * @typedef {object} Entities.EntityProperties-Sphere
 * @see {@link Entities.EntityProperties-Shape|EntityProperties-Shape}
 */

/*@jsdoc
 * The <code>"Text"</code> {@link Entities.EntityType|EntityType} displays a 2D rectangle of text in the domain.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Text
 * @property {Vec3} dimensions=0.1,0.1,0.01 - The dimensions of the entity.
 * @property {string} text="" - The text to display on the face of the entity. Text wraps if necessary to fit. New lines can be
 *     created using <code>\n</code>. Overflowing lines are not displayed.
 * @property {number} lineHeight=0.1 - The height of each line of text (thus determining the font size).
 * @property {Color} textColor=255,255,255 - The color of the text.
 * @property {number} textAlpha=1.0 - The opacity of the text.
 * @property {Color} backgroundColor=0,0,0 - The color of the background rectangle.
 * @property {number} backgroundAlpha=1.0 - The opacity of the background.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {number} leftMargin=0.0 - The left margin, in meters.
 * @property {number} rightMargin=0.0 - The right margin, in meters.
 * @property {number} topMargin=0.0 - The top margin, in meters.
 * @property {number} bottomMargin=0.0 - The bottom margin, in meters.
 * @property {boolean} unlit=false - <code>true</code> if the entity is unaffected by lighting, <code>false</code> if it is lit
 *     by the key light and local lights.
 * @property {string} font="" - The font to render the text with. It can be one of the following: <code>"Courier"</code>,
 *     <code>"Inconsolata"</code>, <code>"Roboto"</code>, <code>"Timeless"</code>, or a path to a .sdff file.
 * @property {Entities.TextEffect} textEffect="none" - The effect that is applied to the text.
 * @property {Color} textEffectColor=255,255,255 - The color of the effect.
 * @property {number} textEffectThickness=0.2 - The magnitude of the text effect, range <code>0.0</code> &ndash; <code>0.5</code>.
 * @property {Entities.TextAlignment} alignment="left" - How the text is aligned against its background.
 * @property {boolean} faceCamera - <code>true</code> if <code>billboardMode</code> is <code>"yaw"</code>, <code>false</code>
 *     if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} isFacingAvatar - <code>true</code> if <code>billboardMode</code> is <code>"full"</code>,
 *     <code>false</code> if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to
 *     <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @example <caption>Create a text entity.</caption>
 * var text = Entities.addEntity({
 *     type: "Text",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
 *     dimensions: { x: 0.6, y: 0.3, z: 0.01 },
 *     lineHeight: 0.12,
 *     text: "Hello\nthere!",
 *     billboardMode: "yaw",
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Web"</code> {@link Entities.EntityType|EntityType} displays a browsable web page. Each user views their own copy
 * of the web page: if one user navigates to another page on the entity, other users do not see the change; if a video is being
 * played, users don't see it in sync. It has properties in addition to the common
 * {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Web
 * @property {Vec3} dimensions=0.1,0.1,0.01 - The dimensions of the entity.
 * @property {string} sourceUrl="" - The URL of the web page to display. This value does not change as you or others navigate
 *     on the Web entity.
 * @property {Color} color=255,255,255 - The color of the web surface. This color tints the web page displayed: the pixel
 *     colors on the web page are multiplied by the property color. For example, a value of
 *     <code>{ red: 255, green: 0, blue: 0 }</code> lets only the red channel of pixels' colors through.
 * @property {number} alpha=1 - The opacity of the web surface.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} faceCamera - <code>true</code> if <code>billboardMode</code> is <code>"yaw"</code>, <code>false</code>
 *     if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} isFacingAvatar - <code>true</code> if <code>billboardMode</code> is <code>"full"</code>,
 *     <code>false</code> if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to
 *     <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {number} dpi=30 - The resolution to display the page at, in dots per inch. If you convert this to dots per meter
 *     (multiply by 1 / 0.0254 = 39.3701) then multiply <code>dimensions.x</code> and <code>dimensions.y</code> by that value
 *     you get the resolution in pixels.
 * @property {string} scriptURL="" - The URL of a JavaScript file to inject into the web page.
 * @property {number} maxFPS=10 - The maximum update rate for the web content, in frames/second.
 * @property {WebInputMode} inputMode="touch" - The user input mode to use.
 * @property {boolean} showKeyboardFocusHighlight=true - <code>true</code> if the entity is highlighted when it has keyboard
 *     focus, <code>false</code> if it isn't.
 * @property {boolean} useBackground=true - <code>true</code> if the web entity should have a background,
 *     <code>false</code> if the web entity's background should be transparent. The webpage must have CSS properties for transparency set
 *     on the <code>background-color</code> for this property to have an effect.
 * @property {string} userAgent - The user agent for the web entity to use when visiting web pages.
 *     Default value: <code>Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) 
 *     Chrome/69.0.3497.113 Mobile Safari/537.36</code>
 * @example <caption>Create a Web entity displaying at 1920 x 1080 resolution.</caption>
 * var METERS_TO_INCHES = 39.3701;
 * var entity = Entities.addEntity({
 *     type: "Web",
 *     sourceUrl: "https://vircadia.com/",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -4 })),
 *     rotation: MyAvatar.orientation,
 *     dimensions: {
 *         x: 3,
 *         y: 3 * 1080 / 1920,
 *         z: 0.01
 *     },
 *     dpi: 1920 / (3 * METERS_TO_INCHES),
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Zone"</code> {@link Entities.EntityType|EntityType} is a volume of lighting effects and avatar permissions.
 * Avatar interaction events such as {@link Entities.enterEntity} are also often used with a Zone entity. It has properties in
 * addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Zone
 * @property {Vec3} dimensions=0.1,0.1,0.1 - The dimensions of the volume in which the zone's lighting effects and avatar
 *     permissions have effect.
 *
 * @property {ShapeType} shapeType="box" - The shape of the volume in which the zone's lighting effects and avatar
 *     permissions have effect. Reverts to the default value if set to <code>"none"</code>, or set to <code>"compound"</code>
 *     and <code>compoundShapeURL</code> is <code>""</code>.
  * @property {string} compoundShapeURL="" - The model file to use for the compound shape if <code>shapeType</code> is
 *     <code>"compound"</code>.
 *
 * @property {Entities.ComponentMode} keyLightMode="inherit" - Configures the key light in the zone.
 * @property {Entities.KeyLight} keyLight - The key light properties of the zone.
 *
 * @property {Entities.ComponentMode} ambientLightMode="inherit" - Configures the ambient light in the zone.
 * @property {Entities.AmbientLight} ambientLight - The ambient light properties of the zone.
 *
 * @property {Entities.ComponentMode} skyboxMode="inherit" - Configures the skybox displayed in the zone.
 * @property {Entities.Skybox} skybox - The skybox properties of the zone.
 *
 * @property {Entities.ComponentMode} hazeMode="inherit" - Configures the haze in the zone.
 * @property {Entities.Haze} haze - The haze properties of the zone.
 *
 * @property {Entities.ComponentMode} bloomMode="inherit" - Configures the bloom in the zone.
 * @property {Entities.Bloom} bloom - The bloom properties of the zone.
 *
 * @property {boolean} flyingAllowed=true - <code>true</code> if visitors can fly in the zone; <code>false</code> if they
 *     cannot. Only works for domain entities.
 * @property {boolean} ghostingAllowed=true - <code>true</code> if visitors with avatar collisions turned off will not
 *     collide with content in the zone; <code>false</code> if visitors will always collide with content in the zone. Only
 *     works for domain entities.
 *
 * @property {string} filterURL="" - The URL of a JavaScript file that filters changes to properties of entities within the
 *     zone. It is periodically executed for each entity in the zone. It can, for example, be used to not allow changes to
 *     certain properties:
 * <pre>
 * function filter(properties) {
 *     // Check and edit properties object values,
 *     // e.g., properties.modelURL, as required.
 *     return properties;
 * }
 * </pre>
 *
 * @property {Entities.AvatarPriorityMode} avatarPriority="inherit" - Configures the priority of updates from avatars in the
 *     zone to other clients.
 *
 * @property {Entities.ScreenshareMode} screenshare="inherit" - Configures a zone for screen-sharing.
 *
 * @example <caption>Create a zone that casts a red key light along the x-axis.</caption>
 * var zone = Entities.addEntity({
 *     type: "Zone",
 *     position: MyAvatar.position,
 *     dimensions: { x: 100, y: 100, z: 100 },
 *     keyLightMode: "enabled",
 *     keyLight: {
 *         "color": { "red": 255, "green": 0, "blue": 0 },
 *         "direction": { "x": 1, "y": 0, "z": 0 }
 *     },
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Image"</code> {@link Entities.EntityType|EntityType} displays an image on a 2D rectangle in the domain.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Image
 * @property {Vec3} dimensions=0.1,0.1,0.01 - The dimensions of the entity.
 * @property {string} imageURL="" - The URL of the image to use.
 * @property {boolean} emissive=false - <code>true</code> if the image should be emissive (unlit), <code>false</code> if it
 *     shouldn't.
 * @property {boolean} keepAspectRatio=true - <code>true</code> if the image should maintain its aspect ratio,
 *     <code>false</code> if it shouldn't.
 * @property {Rect} subImage=0,0,0,0 - The portion of the image to display. If width or height are <code>0</code>, it defaults
 *     to the full image in that dimension.
 * @property {Color} color=255,255,255 - The color of the image.
 * @property {number} alpha=1 - The opacity of the image.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} faceCamera - <code>true</code> if <code>billboardMode</code> is <code>"yaw"</code>, <code>false</code>
 *     if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} isFacingAvatar - <code>true</code> if <code>billboardMode</code> is <code>"full"</code>,
 *     <code>false</code> if it isn't. Setting this property to <code>false</code> sets the <code>billboardMode</code> to
 *     <code>"none"</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @example <caption>Create an image entity.</caption>
 * var image = Entities.addEntity({
 *     type: "Image",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
 *     dimensions: { x: 0.6, y: 0.3, z: 0.01 },
 *     imageURL: "https://images.pexels.com/photos/1020315/pexels-photo-1020315.jpeg",
 *     billboardMode: "yaw",
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Grid"</code> {@link Entities.EntityType|EntityType} displays a grid on a 2D plane.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Grid
 * @property {Vec3} dimensions - 0.1,0.1,0.01 - The dimensions of the entity.
 * @property {Color} color=255,255,255 - The color of the grid.
 * @property {number} alpha=1 - The opacity of the grid.
 * @property {Entities.Pulse} pulse - Color and alpha pulse.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} followCamera=true - <code>true</code> if the grid is always visible even as the camera moves to another
 *     position, <code>false</code> if it doesn't follow the camrmea.
 * @property {number} majorGridEvery=5 - Integer number of <code>minorGridEvery</code> intervals at which to draw a thick grid
 *     line. Minimum value = <code>1</code>.
 * @property {number} minorGridEvery=1 - Real number of meters at which to draw thin grid lines. Minimum value =
 *     <code>0.001</code>.
 * @example <caption>Create a grid entity.</caption>
 * var grid = Entities.addEntity({
 *     type: "Grid",
 *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
 *     dimensions: { x: 100.0, y: 100.0, z: 0.01 },
 *     followCamera: false,
 *     majorGridEvery: 4,
 *     minorGridEvery: 0.5,
 *     lifetime: 300  // Delete after 5 minutes.
 * });
 */

/*@jsdoc
 * The <code>"Gizmo"</code> {@link Entities.EntityType|EntityType} displays an entity that could be used as UI.
 * It has properties in addition to the common {@link Entities.EntityProperties|EntityProperties}.
 *
 * @typedef {object} Entities.EntityProperties-Gizmo
 * @property {Vec3} dimensions=0.1,0.001,0.1 - The dimensions of the entity.
 * @property {Entities.GizmoType} gizmoType="ring" - The gizmo type of the entity.
 * @property {Entities.RingGizmo} ring - The ring gizmo properties.
 */

ScriptValuePointer EntityItemProperties::copyToScriptValue(ScriptEngine* engine, bool skipDefaults, bool allowUnknownCreateTime,
    bool strictSemantics, EntityPsuedoPropertyFlags psueudoPropertyFlags) const {

    // If strictSemantics is true and skipDefaults is false, then all and only those properties are copied for which the property flag
    // is included in _desiredProperties, or is one of the specially enumerated ALWAYS properties below.
    // (There may be exceptions, but if so, they are bugs.)
    // In all other cases, you are welcome to inspect the code and try to figure out what was intended. I wish you luck. -HRS 1/18/17
    ScriptValuePointer properties = engine->newObject();
    EntityItemProperties defaultEntityProperties;

    const bool psuedoPropertyFlagsActive = psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::FlagsActive);
    // Fix to skip the default return all mechanism, when psuedoPropertyFlagsActive
    const bool psuedoPropertyFlagsButDesiredEmpty = psuedoPropertyFlagsActive && _desiredProperties.isEmpty();

    if (_created == UNKNOWN_CREATED_TIME && !allowUnknownCreateTime) {
        // No entity properties can have been set so return without setting any default, zero property values.
        return properties;
    }

    if (_idSet && (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::ID))) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_ALWAYS(id, _id.toString());
    }
    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::Type)) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_ALWAYS(type, EntityTypes::getEntityTypeName(_type));
    }
    if ((!skipDefaults || _lifetime != defaultEntityProperties._lifetime) && !strictSemantics) {
        if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::Age)) {
            COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_NO_SKIP(age, getAge()); // gettable, but not settable
        }
        if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::AgeAsText)) {
            COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_NO_SKIP(ageAsText, formatSecondsElapsed(getAge())); // gettable, but not settable
        }
    }
    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::LastEdited)) {
        properties->setProperty("lastEdited", convertScriptValue(engine, _lastEdited));
    }
    if (!skipDefaults) {
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DIMENSIONS, naturalDimensions); // gettable, but not settable
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_POSITION, naturalPosition);
    }

    // Core properties
    //COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SIMULATION_OWNER, simulationOwner); // not exposed yet
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARENT_ID, parentID);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARENT_JOINT_INDEX, parentJointIndex);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VISIBLE, visible);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_NAME, name);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCKED, locked);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_USER_DATA, userData);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PRIVATE_USER_DATA, privateUserData);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_HREF, href);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DESCRIPTION, description);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_POSITION, position);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DIMENSIONS, dimensions);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ROTATION, rotation);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_REGISTRATION_POINT, registrationPoint);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CREATED, created);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LAST_EDITED_BY, lastEditedBy);
    COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_ENTITY_HOST_TYPE, entityHostType, getEntityHostTypeAsString());
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_OWNING_AVATAR_ID, owningAvatarID);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_QUERY_AA_CUBE, queryAACube);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CAN_CAST_SHADOW, canCastShadow);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VISIBLE_IN_SECONDARY_CAMERA, isVisibleInSecondaryCamera);
    COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_RENDER_LAYER, renderLayer, getRenderLayerAsString());
    COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_PRIMITIVE_MODE, primitiveMode, getPrimitiveModeAsString());
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_IGNORE_PICK_INTERSECTION, ignorePickIntersection);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RENDER_WITH_ZONES, renderWithZones);
    COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_BILLBOARD_MODE, billboardMode, getBillboardModeAsString());
    _grab.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);

    // Physics
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DENSITY, density);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VELOCITY, velocity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ANGULAR_VELOCITY, angularVelocity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_GRAVITY, gravity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ACCELERATION, acceleration);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DAMPING, damping);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ANGULAR_DAMPING, angularDamping);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RESTITUTION, restitution);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_FRICTION, friction);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LIFETIME, lifetime);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COLLISIONLESS, collisionless);
    COPY_PROXY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_COLLISIONLESS, collisionless, ignoreForCollisions, getCollisionless()); // legacy support
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COLLISION_MASK, collisionMask);
    COPY_PROXY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_COLLISION_MASK, collisionMask, collidesWith, getCollisionMaskAsString());
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DYNAMIC, dynamic);
    COPY_PROXY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_DYNAMIC, dynamic, collisionsWillMove, getDynamic()); // legacy support
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COLLISION_SOUND_URL, collisionSoundURL);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ACTION_DATA, actionData);

    // Cloning
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONEABLE, cloneable);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONE_LIFETIME, cloneLifetime);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONE_LIMIT, cloneLimit);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONE_DYNAMIC, cloneDynamic);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONE_AVATAR_ENTITY, cloneAvatarEntity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CLONE_ORIGIN_ID, cloneOriginID);

    // Scripts
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SCRIPT, script);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SCRIPT_TIMESTAMP, scriptTimestamp);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SERVER_SCRIPTS, serverScripts);

    // Certifiable Properties
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ITEM_NAME, itemName);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ITEM_DESCRIPTION, itemDescription);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ITEM_CATEGORIES, itemCategories);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ITEM_ARTIST, itemArtist);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ITEM_LICENSE, itemLicense);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LIMITED_RUN, limitedRun);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MARKETPLACE_ID, marketplaceID);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EDITION_NUMBER, editionNumber);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ENTITY_INSTANCE_NUMBER, entityInstanceNumber);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CERTIFICATE_ID, certificateID);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CERTIFICATE_TYPE, certificateType);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_STATIC_CERTIFICATE_VERSION, staticCertificateVersion);

    // Local props for scripts
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCAL_POSITION, localPosition);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCAL_ROTATION, localRotation);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCAL_VELOCITY, localVelocity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCAL_ANGULAR_VELOCITY, localAngularVelocity);
    COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LOCAL_DIMENSIONS, localDimensions);

    // Particles only
    if (_type == EntityTypes::ParticleEffect) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SHAPE_TYPE, shapeType, getShapeTypeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COMPOUND_SHAPE_URL, compoundShapeURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA, alpha);
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXTURES, textures);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MAX_PARTICLES, maxParticles);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LIFESPAN, lifespan);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMITTING_PARTICLES, isEmitting);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_RATE, emitRate);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_SPEED, emitSpeed);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SPEED_SPREAD, speedSpread);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_ORIENTATION, emitOrientation);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_DIMENSIONS, emitDimensions);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_RADIUS_START, emitRadiusStart);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_POLAR_START, polarStart);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_POLAR_FINISH, polarFinish);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_AZIMUTH_START, azimuthStart);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_AZIMUTH_FINISH, azimuthFinish);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMIT_ACCELERATION, emitAcceleration);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ACCELERATION_SPREAD, accelerationSpread);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARTICLE_RADIUS, particleRadius);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RADIUS_SPREAD, radiusSpread);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RADIUS_START, radiusStart);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RADIUS_FINISH, radiusFinish);

        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR_SPREAD, colorSpread, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR_START, colorStart, vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR_FINISH, colorFinish, vec3Color);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA_SPREAD, alphaSpread);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA_START, alphaStart);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA_FINISH, alphaFinish);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMITTER_SHOULD_TRAIL, emitterShouldTrail);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARTICLE_SPIN, particleSpin);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SPIN_SPREAD, spinSpread);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SPIN_START, spinStart);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SPIN_FINISH, spinFinish);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARTICLE_ROTATE_WITH_ENTITY, rotateWithEntity);
    }

    // Models only
    if (_type == EntityTypes::Model) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SHAPE_TYPE, shapeType, getShapeTypeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COMPOUND_SHAPE_URL, compoundShapeURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXTURES, textures);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MODEL_URL, modelURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MODEL_SCALE, modelScale);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_JOINT_ROTATIONS_SET, jointRotationsSet);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_JOINT_ROTATIONS, jointRotations);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_JOINT_TRANSLATIONS_SET, jointTranslationsSet);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_JOINT_TRANSLATIONS, jointTranslations);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RELAY_PARENT_JOINTS, relayParentJoints);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_GROUP_CULLED, groupCulled);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_BLENDSHAPE_COEFFICIENTS, blendshapeCoefficients);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_USE_ORIGINAL_PIVOT, useOriginalPivot);
        if (!psuedoPropertyFlagsButDesiredEmpty) {
            _animation.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
        }
    }

    // FIXME: Shouldn't provide a shapeType property for Box and Sphere entities.
    if (_type == EntityTypes::Box) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SHAPE_TYPE, shapeType, QString("Box"));
    }
    if (_type == EntityTypes::Sphere) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SHAPE_TYPE, shapeType, QString("Sphere"));
    }

    if (_type == EntityTypes::Box || _type == EntityTypes::Sphere || _type == EntityTypes::Shape) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA, alpha);
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SHAPE, shape);
    }

    // Lights only
    if (_type == EntityTypes::Light) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_IS_SPOTLIGHT, isSpotlight);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_INTENSITY, intensity);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EXPONENT, exponent);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_CUTOFF, cutoff);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_FALLOFF_RADIUS, falloffRadius);
    }

    // Text only
    if (_type == EntityTypes::Text) {
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXT, text);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LINE_HEIGHT, lineHeight);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_TEXT_COLOR, textColor, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXT_ALPHA, textAlpha);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_BACKGROUND_COLOR, backgroundColor, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_BACKGROUND_ALPHA, backgroundAlpha);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LEFT_MARGIN, leftMargin);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_RIGHT_MARGIN, rightMargin);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TOP_MARGIN, topMargin);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_BOTTOM_MARGIN, bottomMargin);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_UNLIT, unlit);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_FONT, font);
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_TEXT_EFFECT, textEffect, getTextEffectAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_TEXT_EFFECT_COLOR, textEffectColor, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXT_EFFECT_THICKNESS, textEffectThickness);
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_TEXT_ALIGNMENT, alignment, getAlignmentAsString());
    }

    // Zones only
    if (_type == EntityTypes::Zone) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SHAPE_TYPE, shapeType, getShapeTypeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_COMPOUND_SHAPE_URL, compoundShapeURL);

        if (!psuedoPropertyFlagsButDesiredEmpty) {
            _keyLight.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
            _ambientLight.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
            _skybox.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
            _haze.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
            _bloom.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
        }

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_FLYING_ALLOWED, flyingAllowed);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_GHOSTING_ALLOWED, ghostingAllowed);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_FILTER_URL, filterURL);

        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_KEY_LIGHT_MODE, keyLightMode, getKeyLightModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_AMBIENT_LIGHT_MODE, ambientLightMode, getAmbientLightModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SKYBOX_MODE, skyboxMode, getSkyboxModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_HAZE_MODE, hazeMode, getHazeModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_BLOOM_MODE, bloomMode, getBloomModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_AVATAR_PRIORITY, avatarPriority, getAvatarPriorityAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_SCREENSHARE, screenshare, getScreenshareAsString());
    }

    // Web only
    if (_type == EntityTypes::Web) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA, alpha);
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SOURCE_URL, sourceUrl);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_DPI, dpi);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SCRIPT_URL, scriptURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MAX_FPS, maxFPS);
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_INPUT_MODE, inputMode, getInputModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, showKeyboardFocusHighlight);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_WEB_USE_BACKGROUND, useBackground);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_USER_AGENT, userAgent);
    }

    // PolyVoxel only
    if (_type == EntityTypes::PolyVox) {
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VOXEL_VOLUME_SIZE, voxelVolumeSize);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VOXEL_DATA, voxelData);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_VOXEL_SURFACE_STYLE, voxelSurfaceStyle);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_X_TEXTURE_URL, xTextureURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Y_TEXTURE_URL, yTextureURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Z_TEXTURE_URL, zTextureURL);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_X_N_NEIGHBOR_ID, xNNeighborID);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Y_N_NEIGHBOR_ID, yNNeighborID);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Z_N_NEIGHBOR_ID, zNNeighborID);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_X_P_NEIGHBOR_ID, xPNeighborID);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Y_P_NEIGHBOR_ID, yPNeighborID);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_Z_P_NEIGHBOR_ID, zPNeighborID);
    }

    // Lines
    if (_type == EntityTypes::Line) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LINE_POINTS, linePoints);
    }

    // Polylines
    if (_type == EntityTypes::PolyLine) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_TEXTURES, textures);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LINE_POINTS, linePoints);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_STROKE_WIDTHS, strokeWidths);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_STROKE_NORMALS, normals);
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_STROKE_COLORS, strokeColors, qVectorVec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_IS_UV_MODE_STRETCH, isUVModeStretch);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LINE_GLOW, glow);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_LINE_FACE_CAMERA, faceCamera);
    }

    // Materials
    if (_type == EntityTypes::Material) {
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_URL, materialURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_MATERIAL_MAPPING_MODE, materialMappingMode, getMaterialMappingModeAsString());
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_PRIORITY, priority);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_PARENT_MATERIAL_NAME, parentMaterialName);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_MAPPING_POS, materialMappingPos);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_MAPPING_SCALE, materialMappingScale);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_MAPPING_ROT, materialMappingRot);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_DATA, materialData);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MATERIAL_REPEAT, materialRepeat);
    }

    // Image only
    if (_type == EntityTypes::Image) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA, alpha);
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_IMAGE_URL, imageURL);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_EMISSIVE, emissive);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_KEEP_ASPECT_RATIO, keepAspectRatio);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_SUB_IMAGE, subImage);

        // Handle conversions to old 'textures' property from "imageURL"
        if (((!psuedoPropertyFlagsButDesiredEmpty && _desiredProperties.isEmpty()) || _desiredProperties.getHasProperty(PROP_IMAGE_URL)) &&
                (!skipDefaults || defaultEntityProperties._imageURL != _imageURL)) {
            ScriptValuePointer textures = engine->newObject();
            textures->setProperty("tex.picture", _imageURL);
            properties->setProperty("textures", textures);
        }
    }

    // Grid only
    if (_type == EntityTypes::Grid) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_COLOR, color, u8vec3Color);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_ALPHA, alpha);
        _pulse.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);

        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_GRID_FOLLOW_CAMERA, followCamera);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MAJOR_GRID_EVERY, majorGridEvery);
        COPY_PROPERTY_TO_QSCRIPTVALUE(PROP_MINOR_GRID_EVERY, minorGridEvery);
    }

    // Gizmo only
    if (_type == EntityTypes::Gizmo) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_GIZMO_TYPE, gizmoType, getGizmoTypeAsString());
        _ring.copyToScriptValue(_desiredProperties, properties, engine, skipDefaults, defaultEntityProperties);
    }

    /*@jsdoc
     * The axis-aligned bounding box of an entity.
     * @typedef {object} Entities.BoundingBox
     * @property {Vec3} brn - The bottom right near (minimum axes values) corner of the AA box.
     * @property {Vec3} tfl - The top far left (maximum axes values) corner of the AA box.
     * @property {Vec3} center - The center of the AA box.
     * @property {Vec3} dimensions - The dimensions of the AA box.
     */
    if (!skipDefaults && !strictSemantics &&
        (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::BoundingBox))) {

        AABox aaBox = getAABox();
        ScriptValuePointer boundingBox = engine->newObject();
        ScriptValuePointer bottomRightNear = vec3ToScriptValue(engine, aaBox.getCorner());
        ScriptValuePointer topFarLeft = vec3ToScriptValue(engine, aaBox.calcTopFarLeft());
        ScriptValuePointer center = vec3ToScriptValue(engine, aaBox.calcCenter());
        ScriptValuePointer boundingBoxDimensions = vec3ToScriptValue(engine, aaBox.getDimensions());
        boundingBox->setProperty("brn", bottomRightNear);
        boundingBox->setProperty("tfl", topFarLeft);
        boundingBox->setProperty("center", center);
        boundingBox->setProperty("dimensions", boundingBoxDimensions);
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_NO_SKIP(boundingBox, boundingBox); // gettable, but not settable
    }

    QString textureNamesStr = QJsonDocument::fromVariant(_textureNames).toJson();
    if (!skipDefaults && !strictSemantics && (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::OriginalTextures))) {
        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_NO_SKIP(originalTextures, textureNamesStr); // gettable, but not settable
    }

    // Rendering info
    if (!skipDefaults && !strictSemantics &&
        (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::RenderInfo))) {

        ScriptValuePointer renderInfo = engine->newObject();

        /*@jsdoc
         * Information on how an entity is rendered. Properties are only filled in for <code>Model</code> entities; other
         * entity types have an empty object, <code>{}</code>.
         * @typedef {object} Entities.RenderInfo
         * @property {number} verticesCount - The number of vertices in the entity.
         * @property {number} texturesCount  - The number of textures in the entity.
         * @property {number} texturesSize - The total size of the textures in the entity, in bytes.
         * @property {boolean} hasTransparent - <code>true</code> if any of the textures has transparency, <code>false</code>
         *     if none of them do.
         * @property {number} drawCalls - The number of draw calls required to render the entity.
         */
        // currently only supported by models
        if (_type == EntityTypes::Model) {
            renderInfo->setProperty("verticesCount", (int)getRenderInfoVertexCount()); // FIXME - theoretically the number of vertex could be > max int
            renderInfo->setProperty("texturesSize", (int)getRenderInfoTextureSize()); // FIXME - theoretically the size of textures could be > max int
            renderInfo->setProperty("hasTransparent", getRenderInfoHasTransparent());
            renderInfo->setProperty("drawCalls", getRenderInfoDrawCalls());
            renderInfo->setProperty("texturesCount", getRenderInfoTextureCount());
        }

        COPY_PROPERTY_TO_QSCRIPTVALUE_GETTER_NO_SKIP(renderInfo, renderInfo);  // Gettable but not settable
    }

    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::ClientOnly)) {
        properties->setProperty("clientOnly", convertScriptValue(engine, getEntityHostType() == entity::HostType::AVATAR));
    }
    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::AvatarEntity)) {
        properties->setProperty("avatarEntity", convertScriptValue(engine, getEntityHostType() == entity::HostType::AVATAR));
    }
    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::LocalEntity)) {
        properties->setProperty("localEntity", convertScriptValue(engine, getEntityHostType() == entity::HostType::LOCAL));
    }

    if (_type != EntityTypes::PolyLine && (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::FaceCamera))) {
        properties->setProperty("faceCamera", convertScriptValue(engine, getBillboardMode() == BillboardMode::YAW));
    }
    if (!psuedoPropertyFlagsActive || psueudoPropertyFlags.test(EntityPsuedoPropertyFlag::IsFacingAvatar)) {
        properties->setProperty("isFacingAvatar", convertScriptValue(engine, getBillboardMode() == BillboardMode::FULL));
    }

    return properties;
}

void EntityItemProperties::copyFromScriptValue(const ScriptValuePointer& object, bool honorReadOnly) {
    ScriptValuePointer typeScriptValue = object->property("type");
    if (typeScriptValue->isValid()) {
        setType(typeScriptValue->toVariant().toString());
    }

    // Core
    if (!honorReadOnly) {
        // not handled yet
        // COPY_PROPERTY_FROM_QSCRIPTVALUE(simulationOwner, SimulationOwner, setSimulationOwner);
    }
    COPY_PROPERTY_FROM_QSCRIPTVALUE(parentID, QUuid, setParentID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(parentJointIndex, quint16, setParentJointIndex);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(visible, bool, setVisible);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(name, QString, setName);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(locked, bool, setLocked);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(userData, QString, setUserData);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(privateUserData, QString, setPrivateUserData);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(href, QString, setHref);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(description, QString, setDescription);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(position, vec3, setPosition);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(dimensions, vec3, setDimensions);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(rotation, quat, setRotation);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(registrationPoint, vec3, setRegistrationPoint);
    if (!honorReadOnly) {
        COPY_PROPERTY_FROM_QSCRIPTVALUE(created, quint64, setCreated);
        COPY_PROPERTY_FROM_QSCRIPTVALUE(lastEditedBy, QUuid, setLastEditedBy);
        COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(entityHostType, EntityHostType);
        COPY_PROPERTY_FROM_QSCRIPTVALUE(owningAvatarID, QUuid, setOwningAvatarID);
    }
    COPY_PROPERTY_FROM_QSCRIPTVALUE(queryAACube, AACube, setQueryAACube); // TODO: should scripts be able to set this?
    COPY_PROPERTY_FROM_QSCRIPTVALUE(canCastShadow, bool, setCanCastShadow);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(isVisibleInSecondaryCamera, bool, setIsVisibleInSecondaryCamera);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(renderLayer, RenderLayer);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(primitiveMode, PrimitiveMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(ignorePickIntersection, bool, setIgnorePickIntersection);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(renderWithZones, qVectorQUuid, setRenderWithZones);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(billboardMode, BillboardMode);
    _grab.copyFromScriptValue(object, _defaultSettings);

    // Physics
    COPY_PROPERTY_FROM_QSCRIPTVALUE(density, float, setDensity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(velocity, vec3, setVelocity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(angularVelocity, vec3, setAngularVelocity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(gravity, vec3, setGravity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(acceleration, vec3, setAcceleration);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(damping, float, setDamping);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(angularDamping, float, setAngularDamping);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(restitution, float, setRestitution);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(friction, float, setFriction);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(lifetime, float, setLifetime);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(collisionless, bool, setCollisionless);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(ignoreForCollisions, bool, setCollisionless, getCollisionless); // legacy support
    COPY_PROPERTY_FROM_QSCRIPTVALUE(collisionMask, uint16_t, setCollisionMask);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(collidesWith, CollisionMask);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(collisionsWillMove, bool, setDynamic, getDynamic); // legacy support
    COPY_PROPERTY_FROM_QSCRIPTVALUE(dynamic, bool, setDynamic);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(collisionSoundURL, QString, setCollisionSoundURL);
    if (!honorReadOnly) {
        COPY_PROPERTY_FROM_QSCRIPTVALUE(actionData, QByteArray, setActionData);
    }

    // Cloning
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneable, bool, setCloneable);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneLifetime, float, setCloneLifetime);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneLimit, float, setCloneLimit);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneDynamic, bool, setCloneDynamic);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneAvatarEntity, bool, setCloneAvatarEntity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cloneOriginID, QUuid, setCloneOriginID);

    // Scripts
    COPY_PROPERTY_FROM_QSCRIPTVALUE(script, QString, setScript);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(scriptTimestamp, quint64, setScriptTimestamp);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(serverScripts, QString, setServerScripts);

    // Certifiable Properties
    COPY_PROPERTY_FROM_QSCRIPTVALUE(itemName, QString, setItemName);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(itemDescription, QString, setItemDescription);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(itemCategories, QString, setItemCategories);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(itemArtist, QString, setItemArtist);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(itemLicense, QString, setItemLicense);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(limitedRun, quint32, setLimitedRun);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(marketplaceID, QString, setMarketplaceID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(editionNumber, quint32, setEditionNumber);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(entityInstanceNumber, quint32, setEntityInstanceNumber);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(certificateID, QString, setCertificateID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(certificateType, QString, setCertificateType);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(staticCertificateVersion, quint32, setStaticCertificateVersion);

    // Script location data
    COPY_PROPERTY_FROM_QSCRIPTVALUE(localPosition, vec3, setLocalPosition);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(localRotation, quat, setLocalRotation);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(localVelocity, vec3, setLocalVelocity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(localAngularVelocity, vec3, setLocalAngularVelocity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(localDimensions, vec3, setLocalDimensions);

    // Common
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(shapeType, ShapeType);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(compoundShapeURL, QString, setCompoundShapeURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(color, u8vec3Color, setColor);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(alpha, float, setAlpha);
    _pulse.copyFromScriptValue(object, _defaultSettings);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(textures, QString, setTextures);

    // Particles
    COPY_PROPERTY_FROM_QSCRIPTVALUE(maxParticles, quint32, setMaxParticles);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(lifespan, float, setLifespan);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(isEmitting, bool, setIsEmitting);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitRate, float, setEmitRate);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitSpeed, float, setEmitSpeed);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(speedSpread, float, setSpeedSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitOrientation, quat, setEmitOrientation);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitDimensions, vec3, setEmitDimensions);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitRadiusStart, float, setEmitRadiusStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(polarStart, float, setPolarStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(polarFinish, float, setPolarFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(azimuthStart, float, setAzimuthStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(azimuthFinish, float, setAzimuthFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitAcceleration, vec3, setEmitAcceleration);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(accelerationSpread, vec3, setAccelerationSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(particleRadius, float, setParticleRadius);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(radiusSpread, float, setRadiusSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(radiusStart, float, setRadiusStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(radiusFinish, float, setRadiusFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(colorSpread, u8vec3Color, setColorSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(colorStart, vec3Color, setColorStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(colorFinish, vec3Color, setColorFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(alphaSpread, float, setAlphaSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(alphaStart, float, setAlphaStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(alphaFinish, float, setAlphaFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emitterShouldTrail, bool, setEmitterShouldTrail);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(particleSpin, float, setParticleSpin);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(spinSpread, float, setSpinSpread);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(spinStart, float, setSpinStart);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(spinFinish, float, setSpinFinish);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(rotateWithEntity, bool, setRotateWithEntity);

    // Model
    COPY_PROPERTY_FROM_QSCRIPTVALUE(modelURL, QString, setModelURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(modelScale, vec3, setModelScale);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(jointRotationsSet, qVectorBool, setJointRotationsSet);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(jointRotations, qVectorQuat, setJointRotations);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(jointTranslationsSet, qVectorBool, setJointTranslationsSet);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(jointTranslations, qVectorVec3, setJointTranslations);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(relayParentJoints, bool, setRelayParentJoints);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(groupCulled, bool, setGroupCulled);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(blendshapeCoefficients, QString, setBlendshapeCoefficients);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(useOriginalPivot, bool, setUseOriginalPivot);
    _animation.copyFromScriptValue(object, _defaultSettings);

    // Light
    COPY_PROPERTY_FROM_QSCRIPTVALUE(isSpotlight, bool, setIsSpotlight);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(intensity, float, setIntensity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(exponent, float, setExponent);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(cutoff, float, setCutoff);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(falloffRadius, float, setFalloffRadius);

    // Text
    COPY_PROPERTY_FROM_QSCRIPTVALUE(text, QString, setText);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(lineHeight, float, setLineHeight);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(textColor, u8vec3Color, setTextColor);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(textAlpha, float, setTextAlpha);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(backgroundColor, u8vec3Color, setBackgroundColor);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(backgroundAlpha, float, setBackgroundAlpha);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(leftMargin, float, setLeftMargin);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(rightMargin, float, setRightMargin);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(topMargin, float, setTopMargin);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(bottomMargin, float, setBottomMargin);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(unlit, bool, setUnlit);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(font, QString, setFont);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(textEffect, TextEffect);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(textEffectColor, u8vec3Color, setTextEffectColor);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(textEffectThickness, float, setTextEffectThickness);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(alignment, Alignment);

    // Zone
    _keyLight.copyFromScriptValue(object, _defaultSettings);
    _ambientLight.copyFromScriptValue(object, _defaultSettings);
    _skybox.copyFromScriptValue(object, _defaultSettings);
    _haze.copyFromScriptValue(object, _defaultSettings);
    _bloom.copyFromScriptValue(object, _defaultSettings);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(flyingAllowed, bool, setFlyingAllowed);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(ghostingAllowed, bool, setGhostingAllowed);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(filterURL, QString, setFilterURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(keyLightMode, KeyLightMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(ambientLightMode, AmbientLightMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(skyboxMode, SkyboxMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(hazeMode, HazeMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(bloomMode, BloomMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(avatarPriority, AvatarPriority);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(screenshare, Screenshare);

    // Polyvox
    COPY_PROPERTY_FROM_QSCRIPTVALUE(voxelVolumeSize, vec3, setVoxelVolumeSize);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(voxelData, QByteArray, setVoxelData);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(voxelSurfaceStyle, uint16_t, setVoxelSurfaceStyle);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(xTextureURL, QString, setXTextureURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(yTextureURL, QString, setYTextureURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(zTextureURL, QString, setZTextureURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(xNNeighborID, EntityItemID, setXNNeighborID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(yNNeighborID, EntityItemID, setYNNeighborID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(zNNeighborID, EntityItemID, setZNNeighborID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(xPNeighborID, EntityItemID, setXPNeighborID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(yPNeighborID, EntityItemID, setYPNeighborID);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(zPNeighborID, EntityItemID, setZPNeighborID);

    // Web
    COPY_PROPERTY_FROM_QSCRIPTVALUE(sourceUrl, QString, setSourceUrl);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(dpi, uint16_t, setDPI);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(scriptURL, QString, setScriptURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(maxFPS, uint8_t, setMaxFPS);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(inputMode, InputMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(showKeyboardFocusHighlight, bool, setShowKeyboardFocusHighlight);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(useBackground, bool, setUseBackground);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(userAgent, QString, setUserAgent);

    // Polyline
    COPY_PROPERTY_FROM_QSCRIPTVALUE(linePoints, qVectorVec3, setLinePoints);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(strokeWidths, qVectorFloat, setStrokeWidths);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(normals, qVectorVec3, setNormals);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(strokeColors, qVectorVec3, setStrokeColors);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(isUVModeStretch, bool, setIsUVModeStretch);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(glow, bool, setGlow);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(faceCamera, bool, setFaceCamera);

    // Shape
    COPY_PROPERTY_FROM_QSCRIPTVALUE(shape, QString, setShape);

    // Material
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialURL, QString, setMaterialURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(materialMappingMode, MaterialMappingMode);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(priority, quint16, setPriority);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(parentMaterialName, QString, setParentMaterialName);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialMappingPos, vec2, setMaterialMappingPos);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialMappingScale, vec2, setMaterialMappingScale);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialMappingRot, float, setMaterialMappingRot);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialData, QString, setMaterialData);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(materialRepeat, bool, setMaterialRepeat);

    // Image
    COPY_PROPERTY_FROM_QSCRIPTVALUE(imageURL, QString, setImageURL);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(emissive, bool, setEmissive);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(keepAspectRatio, bool, setKeepAspectRatio);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(subImage, QRect, setSubImage);

    // Grid
    COPY_PROPERTY_FROM_QSCRIPTVALUE(followCamera, bool, setFollowCamera);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(majorGridEvery, uint32_t, setMajorGridEvery);
    COPY_PROPERTY_FROM_QSCRIPTVALUE(minorGridEvery, float, setMinorGridEvery);

    // Gizmo
    COPY_PROPERTY_FROM_QSCRIPTVALUE_ENUM(gizmoType, GizmoType);
    _ring.copyFromScriptValue(object, _defaultSettings);

    // Handle conversions from old 'textures' property to "imageURL"
    {
        ScriptValuePointer V = object->property("textures");
        if (_type == EntityTypes::Image && V->isValid() && !object->property("imageURL")->isValid()) {
            bool isValid = false;
            QString textures = QString_convertFromScriptValue(V, isValid);
            if (isValid) {
                QVariantMap texturesMap = parseTexturesToMap(textures, QVariantMap());
                auto texPicture = texturesMap.find("tex.picture");
                if (texPicture != texturesMap.end()) {
                    auto imageURL = texPicture.value().toString();
                    if (_defaultSettings || imageURL != _imageURL) {
                        setImageURL(imageURL);
                    }
                }
            }
        }
    }

    // Handle old "faceCamera" and "isFacingAvatar" props
    if (_type != EntityTypes::PolyLine) {
        ScriptValuePointer P = object->property("faceCamera");
        if (P->isValid() && !object->property("billboardMode")->isValid()) {
            bool newValue = P->toVariant().toBool();
            bool oldValue = getBillboardMode() == BillboardMode::YAW;
            if (_defaultSettings || newValue != oldValue) {
                setBillboardMode(newValue ? BillboardMode::YAW : BillboardMode::NONE);
            }
        }
    }
    {
        ScriptValuePointer P = object->property("isFacingAvatar");
        if (P->isValid() && !object->property("billboardMode")->isValid() && !object->property("faceCamera")->isValid()) {
            bool newValue = P->toVariant().toBool();
            bool oldValue = getBillboardMode() == BillboardMode::FULL;
            if (_defaultSettings || newValue != oldValue) {
                setBillboardMode(newValue ? BillboardMode::FULL : BillboardMode::NONE);
            }
        }
    }

    _lastEdited = usecTimestampNow();
}

void EntityItemProperties::copyFromJSONString(ScriptEngine& scriptEngine, const QString& jsonString) {
    // DANGER: this method is expensive
    QJsonDocument propertiesDoc = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject propertiesObj = propertiesDoc.object();
    QVariant propertiesVariant(propertiesObj);
    QVariantMap propertiesMap = propertiesVariant.toMap();
    ScriptValuePointer propertiesScriptValue = variantMapToScriptValue(propertiesMap, scriptEngine);
    bool honorReadOnly = true;
    copyFromScriptValue(propertiesScriptValue, honorReadOnly);
}


void EntityItemProperties::merge(const EntityItemProperties& other) {
    // Core
    COPY_PROPERTY_IF_CHANGED(simulationOwner);
    COPY_PROPERTY_IF_CHANGED(parentID);
    COPY_PROPERTY_IF_CHANGED(parentJointIndex);
    COPY_PROPERTY_IF_CHANGED(visible);
    COPY_PROPERTY_IF_CHANGED(name);
    COPY_PROPERTY_IF_CHANGED(locked);
    COPY_PROPERTY_IF_CHANGED(userData);
    COPY_PROPERTY_IF_CHANGED(privateUserData);
    COPY_PROPERTY_IF_CHANGED(href);
    COPY_PROPERTY_IF_CHANGED(description);
    COPY_PROPERTY_IF_CHANGED(position);
    COPY_PROPERTY_IF_CHANGED(dimensions);
    COPY_PROPERTY_IF_CHANGED(rotation);
    COPY_PROPERTY_IF_CHANGED(registrationPoint);
    COPY_PROPERTY_IF_CHANGED(created);
    COPY_PROPERTY_IF_CHANGED(lastEditedBy);
    COPY_PROPERTY_IF_CHANGED(entityHostType);
    COPY_PROPERTY_IF_CHANGED(owningAvatarID);
    COPY_PROPERTY_IF_CHANGED(queryAACube);
    COPY_PROPERTY_IF_CHANGED(canCastShadow);
    COPY_PROPERTY_IF_CHANGED(isVisibleInSecondaryCamera);
    COPY_PROPERTY_IF_CHANGED(renderLayer);
    COPY_PROPERTY_IF_CHANGED(primitiveMode);
    COPY_PROPERTY_IF_CHANGED(ignorePickIntersection);
    COPY_PROPERTY_IF_CHANGED(renderWithZones);
    COPY_PROPERTY_IF_CHANGED(billboardMode);
    _grab.merge(other._grab);

    // Physics
    COPY_PROPERTY_IF_CHANGED(density);
    COPY_PROPERTY_IF_CHANGED(velocity);
    COPY_PROPERTY_IF_CHANGED(angularVelocity);
    COPY_PROPERTY_IF_CHANGED(gravity);
    COPY_PROPERTY_IF_CHANGED(acceleration);
    COPY_PROPERTY_IF_CHANGED(damping);
    COPY_PROPERTY_IF_CHANGED(angularDamping);
    COPY_PROPERTY_IF_CHANGED(restitution);
    COPY_PROPERTY_IF_CHANGED(friction);
    COPY_PROPERTY_IF_CHANGED(lifetime);
    COPY_PROPERTY_IF_CHANGED(collisionless);
    COPY_PROPERTY_IF_CHANGED(collisionMask);
    COPY_PROPERTY_IF_CHANGED(dynamic);
    COPY_PROPERTY_IF_CHANGED(collisionSoundURL);
    COPY_PROPERTY_IF_CHANGED(actionData);

    // Cloning
    COPY_PROPERTY_IF_CHANGED(cloneable);
    COPY_PROPERTY_IF_CHANGED(cloneLifetime);
    COPY_PROPERTY_IF_CHANGED(cloneLimit);
    COPY_PROPERTY_IF_CHANGED(cloneDynamic);
    COPY_PROPERTY_IF_CHANGED(cloneAvatarEntity);
    COPY_PROPERTY_IF_CHANGED(cloneOriginID);

    // Scripts
    COPY_PROPERTY_IF_CHANGED(script);
    COPY_PROPERTY_IF_CHANGED(scriptTimestamp);
    COPY_PROPERTY_IF_CHANGED(serverScripts);

    // Certifiable Properties
    COPY_PROPERTY_IF_CHANGED(itemName);
    COPY_PROPERTY_IF_CHANGED(itemDescription);
    COPY_PROPERTY_IF_CHANGED(itemCategories);
    COPY_PROPERTY_IF_CHANGED(itemArtist);
    COPY_PROPERTY_IF_CHANGED(itemLicense);
    COPY_PROPERTY_IF_CHANGED(limitedRun);
    COPY_PROPERTY_IF_CHANGED(marketplaceID);
    COPY_PROPERTY_IF_CHANGED(editionNumber);
    COPY_PROPERTY_IF_CHANGED(entityInstanceNumber);
    COPY_PROPERTY_IF_CHANGED(certificateID);
    COPY_PROPERTY_IF_CHANGED(certificateType);
    COPY_PROPERTY_IF_CHANGED(staticCertificateVersion);

    // Local props for scripts
    COPY_PROPERTY_IF_CHANGED(localPosition);
    COPY_PROPERTY_IF_CHANGED(localRotation);
    COPY_PROPERTY_IF_CHANGED(localVelocity);
    COPY_PROPERTY_IF_CHANGED(localAngularVelocity);
    COPY_PROPERTY_IF_CHANGED(localDimensions);

    // Common
    COPY_PROPERTY_IF_CHANGED(shapeType);
    COPY_PROPERTY_IF_CHANGED(compoundShapeURL);
    COPY_PROPERTY_IF_CHANGED(color);
    COPY_PROPERTY_IF_CHANGED(alpha);
    _pulse.merge(other._pulse);
    COPY_PROPERTY_IF_CHANGED(textures);

    // Particles
    COPY_PROPERTY_IF_CHANGED(maxParticles);
    COPY_PROPERTY_IF_CHANGED(lifespan);
    COPY_PROPERTY_IF_CHANGED(isEmitting);
    COPY_PROPERTY_IF_CHANGED(emitRate);
    COPY_PROPERTY_IF_CHANGED(emitSpeed);
    COPY_PROPERTY_IF_CHANGED(speedSpread);
    COPY_PROPERTY_IF_CHANGED(emitOrientation);
    COPY_PROPERTY_IF_CHANGED(emitDimensions);
    COPY_PROPERTY_IF_CHANGED(emitRadiusStart);
    COPY_PROPERTY_IF_CHANGED(polarStart);
    COPY_PROPERTY_IF_CHANGED(polarFinish);
    COPY_PROPERTY_IF_CHANGED(azimuthStart);
    COPY_PROPERTY_IF_CHANGED(azimuthFinish);
    COPY_PROPERTY_IF_CHANGED(emitAcceleration);
    COPY_PROPERTY_IF_CHANGED(accelerationSpread);
    COPY_PROPERTY_IF_CHANGED(particleRadius);
    COPY_PROPERTY_IF_CHANGED(radiusSpread);
    COPY_PROPERTY_IF_CHANGED(radiusStart);
    COPY_PROPERTY_IF_CHANGED(radiusFinish);
    COPY_PROPERTY_IF_CHANGED(colorSpread);
    COPY_PROPERTY_IF_CHANGED(colorStart);
    COPY_PROPERTY_IF_CHANGED(colorFinish);
    COPY_PROPERTY_IF_CHANGED(alphaSpread);
    COPY_PROPERTY_IF_CHANGED(alphaStart);
    COPY_PROPERTY_IF_CHANGED(alphaFinish);
    COPY_PROPERTY_IF_CHANGED(emitterShouldTrail);
    COPY_PROPERTY_IF_CHANGED(particleSpin);
    COPY_PROPERTY_IF_CHANGED(spinSpread);
    COPY_PROPERTY_IF_CHANGED(spinStart);
    COPY_PROPERTY_IF_CHANGED(spinFinish);
    COPY_PROPERTY_IF_CHANGED(rotateWithEntity);

    // Model
    COPY_PROPERTY_IF_CHANGED(modelURL);
    COPY_PROPERTY_IF_CHANGED(modelScale);
    COPY_PROPERTY_IF_CHANGED(jointRotationsSet);
    COPY_PROPERTY_IF_CHANGED(jointRotations);
    COPY_PROPERTY_IF_CHANGED(jointTranslationsSet);
    COPY_PROPERTY_IF_CHANGED(jointTranslations);
    COPY_PROPERTY_IF_CHANGED(relayParentJoints);
    COPY_PROPERTY_IF_CHANGED(groupCulled);
    COPY_PROPERTY_IF_CHANGED(blendshapeCoefficients);
    COPY_PROPERTY_IF_CHANGED(useOriginalPivot);
    _animation.merge(other._animation);

    // Light
    COPY_PROPERTY_IF_CHANGED(isSpotlight);
    COPY_PROPERTY_IF_CHANGED(intensity);
    COPY_PROPERTY_IF_CHANGED(exponent);
    COPY_PROPERTY_IF_CHANGED(cutoff);
    COPY_PROPERTY_IF_CHANGED(falloffRadius);

    // Text
    COPY_PROPERTY_IF_CHANGED(text);
    COPY_PROPERTY_IF_CHANGED(lineHeight);
    COPY_PROPERTY_IF_CHANGED(textColor);
    COPY_PROPERTY_IF_CHANGED(textAlpha);
    COPY_PROPERTY_IF_CHANGED(backgroundColor);
    COPY_PROPERTY_IF_CHANGED(backgroundAlpha);
    COPY_PROPERTY_IF_CHANGED(leftMargin);
    COPY_PROPERTY_IF_CHANGED(rightMargin);
    COPY_PROPERTY_IF_CHANGED(topMargin);
    COPY_PROPERTY_IF_CHANGED(bottomMargin);
    COPY_PROPERTY_IF_CHANGED(unlit);
    COPY_PROPERTY_IF_CHANGED(font);
    COPY_PROPERTY_IF_CHANGED(textEffect);
    COPY_PROPERTY_IF_CHANGED(textEffectColor);
    COPY_PROPERTY_IF_CHANGED(textEffectThickness);
    COPY_PROPERTY_IF_CHANGED(alignment);

    // Zone
    _keyLight.merge(other._keyLight);
    _ambientLight.merge(other._ambientLight);
    _skybox.merge(other._skybox);
    _haze.merge(other._haze);
    _bloom.merge(other._bloom);
    COPY_PROPERTY_IF_CHANGED(flyingAllowed);
    COPY_PROPERTY_IF_CHANGED(ghostingAllowed);
    COPY_PROPERTY_IF_CHANGED(filterURL);
    COPY_PROPERTY_IF_CHANGED(keyLightMode);
    COPY_PROPERTY_IF_CHANGED(ambientLightMode);
    COPY_PROPERTY_IF_CHANGED(skyboxMode);
    COPY_PROPERTY_IF_CHANGED(hazeMode);
    COPY_PROPERTY_IF_CHANGED(bloomMode);
    COPY_PROPERTY_IF_CHANGED(avatarPriority);
    COPY_PROPERTY_IF_CHANGED(screenshare);

    // Polyvox
    COPY_PROPERTY_IF_CHANGED(voxelVolumeSize);
    COPY_PROPERTY_IF_CHANGED(voxelData);
    COPY_PROPERTY_IF_CHANGED(voxelSurfaceStyle);
    COPY_PROPERTY_IF_CHANGED(xTextureURL);
    COPY_PROPERTY_IF_CHANGED(yTextureURL);
    COPY_PROPERTY_IF_CHANGED(zTextureURL);
    COPY_PROPERTY_IF_CHANGED(xNNeighborID);
    COPY_PROPERTY_IF_CHANGED(yNNeighborID);
    COPY_PROPERTY_IF_CHANGED(zNNeighborID);
    COPY_PROPERTY_IF_CHANGED(xPNeighborID);
    COPY_PROPERTY_IF_CHANGED(yPNeighborID);
    COPY_PROPERTY_IF_CHANGED(zPNeighborID);

    // Web
    COPY_PROPERTY_IF_CHANGED(sourceUrl);
    COPY_PROPERTY_IF_CHANGED(dpi);
    COPY_PROPERTY_IF_CHANGED(scriptURL);
    COPY_PROPERTY_IF_CHANGED(maxFPS);
    COPY_PROPERTY_IF_CHANGED(inputMode);
    COPY_PROPERTY_IF_CHANGED(showKeyboardFocusHighlight);
    COPY_PROPERTY_IF_CHANGED(useBackground);
    COPY_PROPERTY_IF_CHANGED(userAgent);

    // Polyline
    COPY_PROPERTY_IF_CHANGED(linePoints);
    COPY_PROPERTY_IF_CHANGED(strokeWidths);
    COPY_PROPERTY_IF_CHANGED(normals);
    COPY_PROPERTY_IF_CHANGED(strokeColors);
    COPY_PROPERTY_IF_CHANGED(isUVModeStretch);
    COPY_PROPERTY_IF_CHANGED(glow);
    COPY_PROPERTY_IF_CHANGED(faceCamera);

    // Shape
    COPY_PROPERTY_IF_CHANGED(shape);

    // Material
    COPY_PROPERTY_IF_CHANGED(materialURL);
    COPY_PROPERTY_IF_CHANGED(materialMappingMode);
    COPY_PROPERTY_IF_CHANGED(priority);
    COPY_PROPERTY_IF_CHANGED(parentMaterialName);
    COPY_PROPERTY_IF_CHANGED(materialMappingPos);
    COPY_PROPERTY_IF_CHANGED(materialMappingScale);
    COPY_PROPERTY_IF_CHANGED(materialMappingRot);
    COPY_PROPERTY_IF_CHANGED(materialData);
    COPY_PROPERTY_IF_CHANGED(materialRepeat);

    // Image
    COPY_PROPERTY_IF_CHANGED(imageURL);
    COPY_PROPERTY_IF_CHANGED(emissive);
    COPY_PROPERTY_IF_CHANGED(keepAspectRatio);
    COPY_PROPERTY_IF_CHANGED(subImage);

    // Grid
    COPY_PROPERTY_IF_CHANGED(followCamera);
    COPY_PROPERTY_IF_CHANGED(majorGridEvery);
    COPY_PROPERTY_IF_CHANGED(minorGridEvery);

    // Gizmo
    COPY_PROPERTY_IF_CHANGED(gizmoType);
    _ring.merge(other._ring);

    _lastEdited = usecTimestampNow();
}

ScriptValuePointer EntityItemPropertiesToScriptValue(ScriptEngine* engine, const EntityItemProperties& properties) {
    return properties.copyToScriptValue(engine, false);
}

ScriptValuePointer EntityItemNonDefaultPropertiesToScriptValue(ScriptEngine* engine, const EntityItemProperties& properties) {
    return properties.copyToScriptValue(engine, true);
}

void EntityItemPropertiesFromScriptValueIgnoreReadOnly(const ScriptValuePointer &object, EntityItemProperties& properties) {
    properties.copyFromScriptValue(object, false);
}

void EntityItemPropertiesFromScriptValueHonorReadOnly(const ScriptValuePointer &object, EntityItemProperties& properties) {
    properties.copyFromScriptValue(object, true);
}

ScriptValuePointer EntityPropertyFlagsToScriptValue(ScriptEngine* engine, const EntityPropertyFlags& flags) {
    return EntityItemProperties::entityPropertyFlagsToScriptValue(engine, flags);
}

void EntityPropertyFlagsFromScriptValue(const ScriptValuePointer& object, EntityPropertyFlags& flags) {
    EntityItemProperties::entityPropertyFlagsFromScriptValue(object, flags);
}


ScriptValuePointer EntityItemProperties::entityPropertyFlagsToScriptValue(ScriptEngine* engine, const EntityPropertyFlags& flags) {
    ScriptValuePointer result = engine->newObject();
    return result;
}

void EntityItemProperties::entityPropertyFlagsFromScriptValue(const ScriptValuePointer& object, EntityPropertyFlags& flags) {
    if (object->isString()) {
        EntityPropertyInfo propertyInfo;
        if (getPropertyInfo(object->toString(), propertyInfo)) {
            flags << propertyInfo.propertyEnum;
        }
    }
    else if (object->isArray()) {
        quint32 length = object->property("length")->toInt32();
        for (quint32 i = 0; i < length; i++) {
            QString propertyName = object->property(i)->toString();
            EntityPropertyInfo propertyInfo;
            if (getPropertyInfo(propertyName, propertyInfo)) {
                flags << propertyInfo.propertyEnum;
            }
        }
    }
}

static QHash<QString, EntityPropertyInfo> _propertyInfos;
static QHash<EntityPropertyList, QString> _enumsToPropertyStrings;

bool EntityItemProperties::getPropertyInfo(const QString& propertyName, EntityPropertyInfo& propertyInfo) {

    static std::once_flag initMap;

    std::call_once(initMap, []() {
        // Core
        ADD_PROPERTY_TO_MAP(PROP_SIMULATION_OWNER, SimulationOwner, simulationOwner, SimulationOwner);
        ADD_PROPERTY_TO_MAP(PROP_PARENT_ID, ParentID, parentID, QUuid);
        ADD_PROPERTY_TO_MAP(PROP_PARENT_JOINT_INDEX, ParentJointIndex, parentJointIndex, uint16_t);
        ADD_PROPERTY_TO_MAP(PROP_VISIBLE, Visible, visible, bool);
        ADD_PROPERTY_TO_MAP(PROP_NAME, Name, name, QString);
        ADD_PROPERTY_TO_MAP(PROP_LOCKED, Locked, locked, bool);
        ADD_PROPERTY_TO_MAP(PROP_USER_DATA, UserData, userData, QString);
        ADD_PROPERTY_TO_MAP(PROP_PRIVATE_USER_DATA, PrivateUserData, privateUserData, QString);
        ADD_PROPERTY_TO_MAP(PROP_HREF, Href, href, QString);
        ADD_PROPERTY_TO_MAP(PROP_DESCRIPTION, Description, description, QString);
        ADD_PROPERTY_TO_MAP(PROP_POSITION, Position, position, vec3);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_DIMENSIONS, Dimensions, dimensions, vec3, ENTITY_ITEM_MIN_DIMENSION, FLT_MAX);
        ADD_PROPERTY_TO_MAP(PROP_ROTATION, Rotation, rotation, quat);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_REGISTRATION_POINT, RegistrationPoint, registrationPoint, vec3,
                                       ENTITY_ITEM_MIN_REGISTRATION_POINT, ENTITY_ITEM_MAX_REGISTRATION_POINT);
        ADD_PROPERTY_TO_MAP(PROP_CREATED, Created, created, quint64);
        ADD_PROPERTY_TO_MAP(PROP_LAST_EDITED_BY, LastEditedBy, lastEditedBy, QUuid);
        ADD_PROPERTY_TO_MAP(PROP_ENTITY_HOST_TYPE, EntityHostType, entityHostType, entity::HostType);
        ADD_PROPERTY_TO_MAP(PROP_OWNING_AVATAR_ID, OwningAvatarID, owningAvatarID, QUuid);
        ADD_PROPERTY_TO_MAP(PROP_QUERY_AA_CUBE, QueryAACube, queryAACube, AACube);
        ADD_PROPERTY_TO_MAP(PROP_CAN_CAST_SHADOW, CanCastShadow, canCastShadow, bool);
        ADD_PROPERTY_TO_MAP(PROP_VISIBLE_IN_SECONDARY_CAMERA, IsVisibleInSecondaryCamera, isVisibleInSecondaryCamera, bool);
        ADD_PROPERTY_TO_MAP(PROP_RENDER_LAYER, RenderLayer, renderLayer, RenderLayer);
        ADD_PROPERTY_TO_MAP(PROP_PRIMITIVE_MODE, PrimitiveMode, primitiveMode, PrimitiveMode);
        ADD_PROPERTY_TO_MAP(PROP_IGNORE_PICK_INTERSECTION, IgnorePickIntersection, ignorePickIntersection, bool);
        ADD_PROPERTY_TO_MAP(PROP_RENDER_WITH_ZONES, RenderWithZones, renderWithZones, QVector<QUuid>);
        ADD_PROPERTY_TO_MAP(PROP_BILLBOARD_MODE, BillboardMode, billboardMode, BillboardMode);
        { // Grab
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_GRABBABLE, Grab, grab, Grabbable, grabbable);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_KINEMATIC, Grab, grab, GrabKinematic, grabKinematic);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_FOLLOWS_CONTROLLER, Grab, grab, GrabFollowsController, grabFollowsController);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_TRIGGERABLE, Grab, grab, Triggerable, triggerable);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_EQUIPPABLE, Grab, grab, Equippable, equippable);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_DELEGATE_TO_PARENT, Grab, grab, GrabDelegateToParent, grabDelegateToParent);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_LEFT_EQUIPPABLE_POSITION_OFFSET, Grab, grab,
                                      EquippableLeftPosition, equippableLeftPosition);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_LEFT_EQUIPPABLE_ROTATION_OFFSET, Grab, grab,
                                      EquippableLeftRotation, equippableLeftRotation);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_RIGHT_EQUIPPABLE_POSITION_OFFSET, Grab, grab,
                                      EquippableRightPosition, equippableRightPosition);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_RIGHT_EQUIPPABLE_ROTATION_OFFSET, Grab, grab,
                                      EquippableRightRotation, equippableRightRotation);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_EQUIPPABLE_INDICATOR_URL, Grab, grab,
                                      EquippableIndicatorURL, equippableIndicatorURL);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_EQUIPPABLE_INDICATOR_SCALE, Grab, grab,
                                      EquippableIndicatorScale, equippableIndicatorScale);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_GRAB_EQUIPPABLE_INDICATOR_OFFSET, Grab, grab,
                                      EquippableIndicatorOffset, equippableIndicatorOffset);
        }

        // Physics
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_DENSITY, Density, density, float,
                                       ENTITY_ITEM_MIN_DENSITY, ENTITY_ITEM_MAX_DENSITY);
        ADD_PROPERTY_TO_MAP(PROP_VELOCITY, Velocity, velocity, vec3);
        ADD_PROPERTY_TO_MAP(PROP_ANGULAR_VELOCITY, AngularVelocity, angularVelocity, vec3);
        ADD_PROPERTY_TO_MAP(PROP_GRAVITY, Gravity, gravity, vec3);
        ADD_PROPERTY_TO_MAP(PROP_ACCELERATION, Acceleration, acceleration, vec3);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_DAMPING, Damping, damping, float,
                                       ENTITY_ITEM_MIN_DAMPING, ENTITY_ITEM_MAX_DAMPING);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ANGULAR_DAMPING, AngularDamping, angularDamping, float,
                                       ENTITY_ITEM_MIN_DAMPING, ENTITY_ITEM_MAX_DAMPING);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_RESTITUTION, Restitution, restitution, float,
                                       ENTITY_ITEM_MIN_RESTITUTION, ENTITY_ITEM_MAX_RESTITUTION);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_FRICTION, Friction, friction, float,
                                       ENTITY_ITEM_MIN_FRICTION, ENTITY_ITEM_MAX_FRICTION);
        ADD_PROPERTY_TO_MAP(PROP_LIFETIME, Lifetime, lifetime, float);
        ADD_PROPERTY_TO_MAP(PROP_COLLISIONLESS, Collisionless, collisionless, bool);
        ADD_PROPERTY_TO_MAP(PROP_COLLISIONLESS, unused, ignoreForCollisions, bool); // legacy support
        ADD_PROPERTY_TO_MAP(PROP_COLLISION_MASK, unused, collisionMask, uint16_t);
        ADD_PROPERTY_TO_MAP(PROP_COLLISION_MASK, unused, collidesWith, uint16_t);
        ADD_PROPERTY_TO_MAP(PROP_DYNAMIC, unused, collisionsWillMove, bool); // legacy support
        ADD_PROPERTY_TO_MAP(PROP_DYNAMIC, unused, dynamic, bool);
        ADD_PROPERTY_TO_MAP(PROP_COLLISION_SOUND_URL, CollisionSoundURL, collisionSoundURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_ACTION_DATA, ActionData, actionData, QByteArray);

        // Cloning
        ADD_PROPERTY_TO_MAP(PROP_CLONEABLE, Cloneable, cloneable, bool);
        ADD_PROPERTY_TO_MAP(PROP_CLONE_LIFETIME, CloneLifetime, cloneLifetime, float);
        ADD_PROPERTY_TO_MAP(PROP_CLONE_LIMIT, CloneLimit, cloneLimit, float);
        ADD_PROPERTY_TO_MAP(PROP_CLONE_DYNAMIC, CloneDynamic, cloneDynamic, bool);
        ADD_PROPERTY_TO_MAP(PROP_CLONE_AVATAR_ENTITY, CloneAvatarEntity, cloneAvatarEntity, bool);
        ADD_PROPERTY_TO_MAP(PROP_CLONE_ORIGIN_ID, CloneOriginID, cloneOriginID, QUuid);

        // Scripts
        ADD_PROPERTY_TO_MAP(PROP_SCRIPT, Script, script, QString);
        ADD_PROPERTY_TO_MAP(PROP_SCRIPT_TIMESTAMP, ScriptTimestamp, scriptTimestamp, quint64);
        ADD_PROPERTY_TO_MAP(PROP_SERVER_SCRIPTS, ServerScripts, serverScripts, QString);

        // Certifiable Properties
        ADD_PROPERTY_TO_MAP(PROP_ITEM_NAME, ItemName, itemName, QString);
        ADD_PROPERTY_TO_MAP(PROP_ITEM_DESCRIPTION, ItemDescription, itemDescription, QString);
        ADD_PROPERTY_TO_MAP(PROP_ITEM_CATEGORIES, ItemCategories, itemCategories, QString);
        ADD_PROPERTY_TO_MAP(PROP_ITEM_ARTIST, ItemArtist, itemArtist, QString);
        ADD_PROPERTY_TO_MAP(PROP_ITEM_LICENSE, ItemLicense, itemLicense, QString);
        ADD_PROPERTY_TO_MAP(PROP_LIMITED_RUN, LimitedRun, limitedRun, quint32);
        ADD_PROPERTY_TO_MAP(PROP_MARKETPLACE_ID, MarketplaceID, marketplaceID, QString);
        ADD_PROPERTY_TO_MAP(PROP_EDITION_NUMBER, EditionNumber, editionNumber, quint32);
        ADD_PROPERTY_TO_MAP(PROP_ENTITY_INSTANCE_NUMBER, EntityInstanceNumber, entityInstanceNumber, quint32);
        ADD_PROPERTY_TO_MAP(PROP_CERTIFICATE_ID, CertificateID, certificateID, QString);
        ADD_PROPERTY_TO_MAP(PROP_CERTIFICATE_TYPE, CertificateType, certificateType, QString);
        ADD_PROPERTY_TO_MAP(PROP_STATIC_CERTIFICATE_VERSION, StaticCertificateVersion, staticCertificateVersion, quint32);

        // Local script props
        ADD_PROPERTY_TO_MAP(PROP_LOCAL_POSITION, LocalPosition, localPosition, vec3);
        ADD_PROPERTY_TO_MAP(PROP_LOCAL_ROTATION, LocalRotation, localRotation, quat);
        ADD_PROPERTY_TO_MAP(PROP_LOCAL_VELOCITY, LocalVelocity, localVelocity, vec3);
        ADD_PROPERTY_TO_MAP(PROP_LOCAL_ANGULAR_VELOCITY, LocalAngularVelocity, localAngularVelocity, vec3);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_LOCAL_DIMENSIONS, LocalDimensions, localDimensions, vec3,
                                       ENTITY_ITEM_MIN_DIMENSION, FLT_MAX);

        // Common
        ADD_PROPERTY_TO_MAP(PROP_SHAPE_TYPE, ShapeType, shapeType, ShapeType);
        ADD_PROPERTY_TO_MAP(PROP_COMPOUND_SHAPE_URL, CompoundShapeURL, compoundShapeURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_COLOR, Color, color, u8vec3Color);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ALPHA, Alpha, alpha, float, particle::MINIMUM_ALPHA, particle::MAXIMUM_ALPHA);
        { // Pulse
            ADD_GROUP_PROPERTY_TO_MAP(PROP_PULSE_MIN, Pulse, pulse, Min, min);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_PULSE_MAX, Pulse, pulse, Max, max);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_PULSE_PERIOD, Pulse, pulse, Period, period);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_PULSE_COLOR_MODE, Pulse, pulse, ColorMode, colorMode);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_PULSE_ALPHA_MODE, Pulse, pulse, AlphaMode, alphaMode);
        }
        ADD_PROPERTY_TO_MAP(PROP_TEXTURES, Textures, textures, QString);

        // Particles
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_MAX_PARTICLES, MaxParticles, maxParticles, quint32,
                                       particle::MINIMUM_MAX_PARTICLES, particle::MAXIMUM_MAX_PARTICLES);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_LIFESPAN, Lifespan, lifespan, float,
                                       particle::MINIMUM_LIFESPAN, particle::MAXIMUM_LIFESPAN);
        ADD_PROPERTY_TO_MAP(PROP_EMITTING_PARTICLES, IsEmitting, isEmitting, bool);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_EMIT_RATE, EmitRate, emitRate, float,
                                       particle::MINIMUM_EMIT_RATE, particle::MAXIMUM_EMIT_RATE);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_EMIT_SPEED, EmitSpeed, emitSpeed, vec3,
                                       particle::MINIMUM_EMIT_SPEED, particle::MAXIMUM_EMIT_SPEED);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_SPEED_SPREAD, SpeedSpread, speedSpread, vec3,
                                       particle::MINIMUM_EMIT_SPEED, particle::MAXIMUM_EMIT_SPEED);
        ADD_PROPERTY_TO_MAP(PROP_EMIT_ORIENTATION, EmitOrientation, emitOrientation, quat);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_EMIT_DIMENSIONS, EmitDimensions, emitDimensions, vec3,
                                       particle::MINIMUM_EMIT_DIMENSION, particle::MAXIMUM_EMIT_DIMENSION);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_EMIT_RADIUS_START, EmitRadiusStart, emitRadiusStart, float,
                                       particle::MINIMUM_EMIT_RADIUS_START, particle::MAXIMUM_EMIT_RADIUS_START);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_POLAR_START, EmitPolarStart, polarStart, float,
                                       particle::MINIMUM_POLAR, particle::MAXIMUM_POLAR);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_POLAR_FINISH, EmitPolarFinish, polarFinish, float,
                                       particle::MINIMUM_POLAR, particle::MAXIMUM_POLAR);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_AZIMUTH_START, EmitAzimuthStart, azimuthStart, float,
                                       particle::MINIMUM_AZIMUTH, particle::MAXIMUM_AZIMUTH);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_AZIMUTH_FINISH, EmitAzimuthFinish, azimuthFinish, float,
                                       particle::MINIMUM_AZIMUTH, particle::MAXIMUM_AZIMUTH);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_EMIT_ACCELERATION, EmitAcceleration, emitAcceleration, vec3,
                                       particle::MINIMUM_EMIT_ACCELERATION, particle::MAXIMUM_EMIT_ACCELERATION);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ACCELERATION_SPREAD, AccelerationSpread, accelerationSpread, vec3,
                                       particle::MINIMUM_ACCELERATION_SPREAD, particle::MAXIMUM_ACCELERATION_SPREAD);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_PARTICLE_RADIUS, ParticleRadius, particleRadius, float,
                                       particle::MINIMUM_PARTICLE_RADIUS, particle::MAXIMUM_PARTICLE_RADIUS);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_RADIUS_SPREAD, RadiusSpread, radiusSpread, float,
                                       particle::MINIMUM_PARTICLE_RADIUS, particle::MAXIMUM_PARTICLE_RADIUS);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_RADIUS_START, RadiusStart, radiusStart, float,
                                       particle::MINIMUM_PARTICLE_RADIUS, particle::MAXIMUM_PARTICLE_RADIUS);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_RADIUS_FINISH, RadiusFinish, radiusFinish, float,
                                       particle::MINIMUM_PARTICLE_RADIUS, particle::MAXIMUM_PARTICLE_RADIUS);
        ADD_PROPERTY_TO_MAP(PROP_COLOR_SPREAD, ColorSpread, colorSpread, u8vec3Color);
        ADD_PROPERTY_TO_MAP(PROP_COLOR_START, ColorStart, colorStart, vec3Color);
        ADD_PROPERTY_TO_MAP(PROP_COLOR_FINISH, ColorFinish, colorFinish, vec3Color);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ALPHA_SPREAD, AlphaSpread, alphaSpread, float,
                                       particle::MINIMUM_ALPHA, particle::MAXIMUM_ALPHA);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ALPHA_START, AlphaStart, alphaStart, float,
                                       particle::MINIMUM_ALPHA, particle::MAXIMUM_ALPHA);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_ALPHA_FINISH, AlphaFinish, alphaFinish, float,
                                       particle::MINIMUM_ALPHA, particle::MAXIMUM_ALPHA);
        ADD_PROPERTY_TO_MAP(PROP_EMITTER_SHOULD_TRAIL, EmitterShouldTrail, emitterShouldTrail, bool);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_PARTICLE_SPIN, ParticleSpin, particleSpin, float,
                                       particle::MINIMUM_PARTICLE_SPIN, particle::MAXIMUM_PARTICLE_SPIN);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_SPIN_SPREAD, SpinSpread, spinSpread, float,
                                       particle::MINIMUM_PARTICLE_SPIN, particle::MAXIMUM_PARTICLE_SPIN);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_SPIN_START, SpinStart, spinStart, float,
                                       particle::MINIMUM_PARTICLE_SPIN, particle::MAXIMUM_PARTICLE_SPIN);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_SPIN_FINISH, SpinFinish, spinFinish, float,
                                       particle::MINIMUM_PARTICLE_SPIN, particle::MAXIMUM_PARTICLE_SPIN);
        ADD_PROPERTY_TO_MAP(PROP_PARTICLE_ROTATE_WITH_ENTITY, RotateWithEntity, rotateWithEntity, float);

        // Model
        ADD_PROPERTY_TO_MAP(PROP_MODEL_URL, ModelURL, modelURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_MODEL_SCALE, ModelScale, modelScale, vec3);
        ADD_PROPERTY_TO_MAP(PROP_JOINT_ROTATIONS_SET, JointRotationsSet, jointRotationsSet, QVector<bool>);
        ADD_PROPERTY_TO_MAP(PROP_JOINT_ROTATIONS, JointRotations, jointRotations, QVector<quat>);
        ADD_PROPERTY_TO_MAP(PROP_JOINT_TRANSLATIONS_SET, JointTranslationsSet, jointTranslationsSet, QVector<bool>);
        ADD_PROPERTY_TO_MAP(PROP_JOINT_TRANSLATIONS, JointTranslations, jointTranslations, QVector<vec3>);
        ADD_PROPERTY_TO_MAP(PROP_RELAY_PARENT_JOINTS, RelayParentJoints, relayParentJoints, bool);
        ADD_PROPERTY_TO_MAP(PROP_GROUP_CULLED, GroupCulled, groupCulled, bool);
        ADD_PROPERTY_TO_MAP(PROP_BLENDSHAPE_COEFFICIENTS, BlendshapeCoefficients, blendshapeCoefficients, QString);
        ADD_PROPERTY_TO_MAP(PROP_USE_ORIGINAL_PIVOT, UseOriginalPivot, useOriginalPivot, bool);
        { // Animation
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_URL, Animation, animation, URL, url);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_ALLOW_TRANSLATION, Animation, animation, AllowTranslation, allowTranslation);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_FPS, Animation, animation, FPS, fps);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_FRAME_INDEX, Animation, animation, CurrentFrame, currentFrame);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_PLAYING, Animation, animation, Running, running);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_LOOP, Animation, animation, Loop, loop);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_FIRST_FRAME, Animation, animation, FirstFrame, firstFrame);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_LAST_FRAME, Animation, animation, LastFrame, lastFrame);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_ANIMATION_HOLD, Animation, animation, Hold, hold);
        }

        // Light
        ADD_PROPERTY_TO_MAP(PROP_IS_SPOTLIGHT, IsSpotlight, isSpotlight, bool);
        ADD_PROPERTY_TO_MAP(PROP_INTENSITY, Intensity, intensity, float);
        ADD_PROPERTY_TO_MAP(PROP_EXPONENT, Exponent, exponent, float);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_CUTOFF, Cutoff, cutoff, float,
                                       LightEntityItem::MIN_CUTOFF, LightEntityItem::MAX_CUTOFF);
        ADD_PROPERTY_TO_MAP(PROP_FALLOFF_RADIUS, FalloffRadius, falloffRadius, float);

        // Text
        ADD_PROPERTY_TO_MAP(PROP_TEXT, Text, text, QString);
        ADD_PROPERTY_TO_MAP(PROP_LINE_HEIGHT, LineHeight, lineHeight, float);
        ADD_PROPERTY_TO_MAP(PROP_TEXT_COLOR, TextColor, textColor, u8vec3Color);
        ADD_PROPERTY_TO_MAP(PROP_TEXT_ALPHA, TextAlpha, textAlpha, float);
        ADD_PROPERTY_TO_MAP(PROP_BACKGROUND_COLOR, BackgroundColor, backgroundColor, u8vec3Color);
        ADD_PROPERTY_TO_MAP(PROP_BACKGROUND_ALPHA, BackgroundAlpha, backgroundAlpha, float);
        ADD_PROPERTY_TO_MAP(PROP_LEFT_MARGIN, LeftMargin, leftMargin, float);
        ADD_PROPERTY_TO_MAP(PROP_RIGHT_MARGIN, RightMargin, rightMargin, float);
        ADD_PROPERTY_TO_MAP(PROP_TOP_MARGIN, TopMargin, topMargin, float);
        ADD_PROPERTY_TO_MAP(PROP_BOTTOM_MARGIN, BottomMargin, bottomMargin, float);
        ADD_PROPERTY_TO_MAP(PROP_UNLIT, Unlit, unlit, bool);
        ADD_PROPERTY_TO_MAP(PROP_FONT, Font, font, QString);
        ADD_PROPERTY_TO_MAP(PROP_TEXT_EFFECT, TextEffect, textEffect, TextEffect);
        ADD_PROPERTY_TO_MAP(PROP_TEXT_EFFECT_COLOR, TextEffectColor, textEffectColor, u8vec3Color);
        ADD_PROPERTY_TO_MAP_WITH_RANGE(PROP_TEXT_EFFECT_THICKNESS, TextEffectThickness, textEffectThickness, float, 0.0, 0.5);
        ADD_PROPERTY_TO_MAP(PROP_TEXT_ALIGNMENT, Alignment, alignment, TextAlignment);

        // Zone
        { // Keylight
            ADD_GROUP_PROPERTY_TO_MAP(PROP_KEYLIGHT_COLOR, KeyLight, keyLight, Color, color);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_KEYLIGHT_INTENSITY, KeyLight, keyLight, Intensity, intensity);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_KEYLIGHT_DIRECTION, KeyLight, keylight, Direction, direction);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_KEYLIGHT_CAST_SHADOW, KeyLight, keyLight, CastShadows, castShadows);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_KEYLIGHT_SHADOW_BIAS, KeyLight, keyLight, ShadowBias, shadowBias, 0.0f, 1.0f);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, KeyLight, keyLight, ShadowMaxDistance, shadowMaxDistance, 1.0f, 250.0f);
        }
        { // Ambient light
            ADD_GROUP_PROPERTY_TO_MAP(PROP_AMBIENT_LIGHT_INTENSITY, AmbientLight, ambientLight, Intensity, intensity);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_AMBIENT_LIGHT_URL, AmbientLight, ambientLight, URL, url);
        }
        { // Skybox
            ADD_GROUP_PROPERTY_TO_MAP(PROP_SKYBOX_COLOR, Skybox, skybox, Color, color);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_SKYBOX_URL, Skybox, skybox, URL, url);
        }
        { // Haze
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_RANGE, Haze, haze, HazeRange, hazeRange);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_COLOR, Haze, haze, HazeColor, hazeColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_GLARE_COLOR, Haze, haze, HazeGlareColor, hazeGlareColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_ENABLE_GLARE, Haze, haze, HazeEnableGlare, hazeEnableGlare);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_GLARE_ANGLE, Haze, haze, HazeGlareAngle, hazeGlareAngle);

            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_ALTITUDE_EFFECT, Haze, haze, HazeAltitudeEffect, hazeAltitudeEfect);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_CEILING, Haze, haze, HazeCeiling, hazeCeiling);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_BASE_REF, Haze, haze, HazeBaseRef, hazeBaseRef);

            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_BACKGROUND_BLEND, Haze, haze, HazeBackgroundBlend, hazeBackgroundBlend);

            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_ATTENUATE_KEYLIGHT, Haze, haze, HazeAttenuateKeyLight, hazeAttenuateKeyLight);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_KEYLIGHT_RANGE, Haze, haze, HazeKeyLightRange, hazeKeyLightRange);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAZE_KEYLIGHT_ALTITUDE, Haze, haze, HazeKeyLightAltitude, hazeKeyLightAltitude);
        }
        { // Bloom
            ADD_GROUP_PROPERTY_TO_MAP(PROP_BLOOM_INTENSITY, Bloom, bloom, BloomIntensity, bloomIntensity);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_BLOOM_THRESHOLD, Bloom, bloom, BloomThreshold, bloomThreshold);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_BLOOM_SIZE, Bloom, bloom, BloomSize, bloomSize);
        }
        ADD_PROPERTY_TO_MAP(PROP_FLYING_ALLOWED, FlyingAllowed, flyingAllowed, bool);
        ADD_PROPERTY_TO_MAP(PROP_GHOSTING_ALLOWED, GhostingAllowed, ghostingAllowed, bool);
        ADD_PROPERTY_TO_MAP(PROP_FILTER_URL, FilterURL, filterURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_KEY_LIGHT_MODE, KeyLightMode, keyLightMode, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_AMBIENT_LIGHT_MODE, AmbientLightMode, ambientLightMode, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_SKYBOX_MODE, SkyboxMode, skyboxMode, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_HAZE_MODE, HazeMode, hazeMode, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_BLOOM_MODE, BloomMode, bloomMode, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_AVATAR_PRIORITY, AvatarPriority, avatarPriority, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_SCREENSHARE, Screenshare, screenshare, uint32_t);

        // Polyvox
        ADD_PROPERTY_TO_MAP(PROP_VOXEL_VOLUME_SIZE, VoxelVolumeSize, voxelVolumeSize, vec3);
        ADD_PROPERTY_TO_MAP(PROP_VOXEL_DATA, VoxelData, voxelData, QByteArray);
        ADD_PROPERTY_TO_MAP(PROP_VOXEL_SURFACE_STYLE, VoxelSurfaceStyle, voxelSurfaceStyle, uint16_t);
        ADD_PROPERTY_TO_MAP(PROP_X_TEXTURE_URL, XTextureURL, xTextureURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_Y_TEXTURE_URL, YTextureURL, yTextureURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_Z_TEXTURE_URL, ZTextureURL, zTextureURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_X_N_NEIGHBOR_ID, XNNeighborID, xNNeighborID, EntityItemID);
        ADD_PROPERTY_TO_MAP(PROP_Y_N_NEIGHBOR_ID, YNNeighborID, yNNeighborID, EntityItemID);
        ADD_PROPERTY_TO_MAP(PROP_Z_N_NEIGHBOR_ID, ZNNeighborID, zNNeighborID, EntityItemID);
        ADD_PROPERTY_TO_MAP(PROP_X_P_NEIGHBOR_ID, XPNeighborID, xPNeighborID, EntityItemID);
        ADD_PROPERTY_TO_MAP(PROP_Y_P_NEIGHBOR_ID, YPNeighborID, yPNeighborID, EntityItemID);
        ADD_PROPERTY_TO_MAP(PROP_Z_P_NEIGHBOR_ID, ZPNeighborID, zPNeighborID, EntityItemID);

        // Web
        ADD_PROPERTY_TO_MAP(PROP_SOURCE_URL, SourceUrl, sourceUrl, QString);
        ADD_PROPERTY_TO_MAP(PROP_DPI, DPI, dpi, uint16_t);
        ADD_PROPERTY_TO_MAP(PROP_SCRIPT_URL, ScriptURL, scriptURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_MAX_FPS, MaxFPS, maxFPS, uint8_t);
        ADD_PROPERTY_TO_MAP(PROP_INPUT_MODE, InputMode, inputMode, WebInputMode);
        ADD_PROPERTY_TO_MAP(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, ShowKeyboardFocusHighlight, showKeyboardFocusHighlight, bool);
        ADD_PROPERTY_TO_MAP(PROP_WEB_USE_BACKGROUND, useBackground, useBackground, bool);
        ADD_PROPERTY_TO_MAP(PROP_USER_AGENT, UserAgent, userAgent, QString);

        // Polyline
        ADD_PROPERTY_TO_MAP(PROP_LINE_POINTS, LinePoints, linePoints, QVector<vec3>);
        ADD_PROPERTY_TO_MAP(PROP_STROKE_WIDTHS, StrokeWidths, strokeWidths, QVector<float>);
        ADD_PROPERTY_TO_MAP(PROP_STROKE_NORMALS, Normals, normals, QVector<vec3>);
        ADD_PROPERTY_TO_MAP(PROP_STROKE_COLORS, StrokeColors, strokeColors, QVector<vec3>);
        ADD_PROPERTY_TO_MAP(PROP_IS_UV_MODE_STRETCH, IsUVModeStretch, isUVModeStretch, QVector<float>);
        ADD_PROPERTY_TO_MAP(PROP_LINE_GLOW, Glow, glow, bool);
        ADD_PROPERTY_TO_MAP(PROP_LINE_FACE_CAMERA, FaceCamera, faceCamera, bool);

        // Shape
        ADD_PROPERTY_TO_MAP(PROP_SHAPE, Shape, shape, QString);

        // Material
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_URL, MaterialURL, materialURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_MAPPING_MODE, MaterialMappingMode, materialMappingMode, MaterialMappingMode);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_PRIORITY, Priority, priority, quint16);
        ADD_PROPERTY_TO_MAP(PROP_PARENT_MATERIAL_NAME, ParentMaterialName, parentMaterialName, QString);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_MAPPING_POS, MaterialMappingPos, materialMappingPos, vec2);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_MAPPING_SCALE, MaterialMappingScale, materialMappingScale, vec2);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_MAPPING_ROT, MaterialMappingRot, materialMappingRot, float);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_DATA, MaterialData, materialData, QString);
        ADD_PROPERTY_TO_MAP(PROP_MATERIAL_REPEAT, MaterialRepeat, materialRepeat, bool);

        // Image
        ADD_PROPERTY_TO_MAP(PROP_IMAGE_URL, ImageURL, imageURL, QString);
        ADD_PROPERTY_TO_MAP(PROP_EMISSIVE, Emissive, emissive, bool);
        ADD_PROPERTY_TO_MAP(PROP_KEEP_ASPECT_RATIO, KeepAspectRatio, keepAspectRatio, bool);
        ADD_PROPERTY_TO_MAP(PROP_SUB_IMAGE, SubImage, subImage, QRect);

        // Grid
        ADD_PROPERTY_TO_MAP(PROP_GRID_FOLLOW_CAMERA, FollowCamera, followCamera, bool);
        ADD_PROPERTY_TO_MAP(PROP_MAJOR_GRID_EVERY, MajorGridEvery, majorGridEvery, uint32_t);
        ADD_PROPERTY_TO_MAP(PROP_MINOR_GRID_EVERY, MinorGridEvery, minorGridEvery, float);

        // Gizmo
        ADD_PROPERTY_TO_MAP(PROP_GIZMO_TYPE, GizmoType, gizmoType, GizmoType);
        { // RingGizmo
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_START_ANGLE, Ring, ring, StartAngle, startAngle, RingGizmoPropertyGroup::MIN_ANGLE, RingGizmoPropertyGroup::MAX_ANGLE);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_END_ANGLE, Ring, ring, EndAngle, endAngle, RingGizmoPropertyGroup::MIN_ANGLE, RingGizmoPropertyGroup::MAX_ANGLE);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_INNER_RADIUS, Ring, ring, InnerRadius, innerRadius, RingGizmoPropertyGroup::MIN_RADIUS, RingGizmoPropertyGroup::MAX_RADIUS);

            ADD_GROUP_PROPERTY_TO_MAP(PROP_INNER_START_COLOR, Ring, ring, InnerStartColor, innerStartColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_INNER_END_COLOR, Ring, ring, InnerEndColor, innerEndColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_OUTER_START_COLOR, Ring, ring, OuterStartColor, outerStartColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_OUTER_END_COLOR, Ring, ring, OuterEndColor, outerEndColor);

            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_INNER_START_ALPHA, Ring, ring, InnerStartAlpha, innerStartAlpha, RingGizmoPropertyGroup::MIN_ALPHA, RingGizmoPropertyGroup::MAX_ALPHA);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_INNER_END_ALPHA, Ring, ring, InnerEndAlpha, innerEndAlpha, RingGizmoPropertyGroup::MIN_ALPHA, RingGizmoPropertyGroup::MAX_ALPHA);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_OUTER_START_ALPHA, Ring, ring, OuterStartAlpha, outerStartAlpha, RingGizmoPropertyGroup::MIN_ALPHA, RingGizmoPropertyGroup::MAX_ALPHA);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_OUTER_END_ALPHA, Ring, ring, OuterEndAlpha, outerEndAlpha, RingGizmoPropertyGroup::MIN_ALPHA, RingGizmoPropertyGroup::MAX_ALPHA);

            ADD_GROUP_PROPERTY_TO_MAP(PROP_HAS_TICK_MARKS, Ring, ring, HasTickMarks, hasTickMarks);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_MAJOR_TICK_MARKS_ANGLE, Ring, ring, MajorTickMarksAngle, majorTickMarksAngle, RingGizmoPropertyGroup::MIN_ANGLE, RingGizmoPropertyGroup::MAX_ANGLE);
            ADD_GROUP_PROPERTY_TO_MAP_WITH_RANGE(PROP_MINOR_TICK_MARKS_ANGLE, Ring, ring, MinorTickMarksAngle, minorTickMarksAngle, RingGizmoPropertyGroup::MIN_ANGLE, RingGizmoPropertyGroup::MAX_ANGLE);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_MAJOR_TICK_MARKS_LENGTH, Ring, ring, MajorTickMarksLength, majorTickMarksLength);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_MINOR_TICK_MARKS_LENGTH, Ring, ring, MinorTickMarksLength, minorTickMarksLength);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_MAJOR_TICK_MARKS_COLOR, Ring, ring, MajorTickMarksColor, majorTickMarksColor);
            ADD_GROUP_PROPERTY_TO_MAP(PROP_MINOR_TICK_MARKS_COLOR, Ring, ring, MinorTickMarksColor, minorTickMarksColor);
        }
    });

    auto iter = _propertyInfos.find(propertyName);
    if (iter != _propertyInfos.end()) {
        propertyInfo = *iter;
        return true;
    }

    return false;
}

/*@jsdoc
 * Information about an entity property.
 * @typedef {object} Entities.EntityPropertyInfo
 * @property {number} propertyEnum - The internal number of the property.
 * @property {string} minimum - The minimum numerical value the property may have, if available, otherwise <code>""</code>.
 * @property {string} maximum - The maximum numerical value the property may have, if available, otherwise <code>""</code>.
 */
ScriptValuePointer EntityPropertyInfoToScriptValue(ScriptEngine* engine, const EntityPropertyInfo& propertyInfo) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("propertyEnum", propertyInfo.propertyEnum);
    obj->setProperty("minimum", propertyInfo.minimum.toString());
    obj->setProperty("maximum", propertyInfo.maximum.toString());
    return obj;
}

void EntityPropertyInfoFromScriptValue(const ScriptValuePointer& object, EntityPropertyInfo& propertyInfo) {
    propertyInfo.propertyEnum = (EntityPropertyList)object->property("propertyEnum")->toVariant().toUInt();
    propertyInfo.minimum = object->property("minimum")->toVariant();
    propertyInfo.maximum = object->property("maximum")->toVariant();
}

// TODO: Implement support for edit packets that can span an MTU sized buffer. We need to implement a mechanism for the
//       encodeEntityEditPacket() method to communicate the the caller which properties couldn't fit in the buffer. Similar
//       to how we handle this in the Octree streaming case.
//
// TODO: Right now, all possible properties for all subclasses are handled here. Ideally we'd prefer
//       to handle this in a more generic way. Allowing subclasses of EntityItem to register their properties
//
// TODO: There's a lot of repeated patterns in the code below to handle each property. It would be nice if the property
//       registration mechanism allowed us to collapse these repeated sections of code into a single implementation that
//       utilized the registration table to shorten up and simplify this code.
//
// TODO: Implement support for paged properties, spanning MTU, and custom properties
//
// TODO: Implement support for script and visible properties.
//
OctreeElement::AppendState EntityItemProperties::encodeEntityEditPacket(PacketType command, EntityItemID id, const EntityItemProperties& properties,
                QByteArray& buffer, EntityPropertyFlags requestedProperties, EntityPropertyFlags& didntFitProperties) {

    OctreePacketData ourDataPacket(false, buffer.size()); // create a packetData object to add out packet details too.
    OctreePacketData* packetData = &ourDataPacket; // we want a pointer to this so we can use our APPEND_ENTITY_PROPERTY macro

    bool success = true; // assume the best
    OctreeElement::AppendState appendState = OctreeElement::COMPLETED; // assume the best

    // TODO: We need to review how jurisdictions should be handled for entities. (The old Models and Particles code
    // didn't do anything special for jurisdictions, so we're keeping that same behavior here.)
    //
    // Always include the root octcode. This is only because the OctreeEditPacketSender will check these octcodes
    // to determine which server to send the changes to in the case of multiple jurisdictions. The root will be sent
    // to all servers.
    vec3 rootPosition(0);
    float rootScale = 0.5f;
    unsigned char* octcode = pointToOctalCode(rootPosition.x, rootPosition.y, rootPosition.z, rootScale);

    success = packetData->startSubTree(octcode);
    delete[] octcode;

    // assuming we have room to fit our octalCode, proceed...
    if (success) {

        // Now add our edit content details...

        // id
        // encode our ID as a byte count coded byte stream
        QByteArray encodedID = id.toRfc4122(); // NUM_BYTES_RFC4122_UUID

        // encode our ID as a byte count coded byte stream
        ByteCountCoded<quint32> tokenCoder;
        QByteArray encodedToken;

        // encode our type as a byte count coded byte stream
        ByteCountCoded<quint32> typeCoder = (quint32)properties.getType();
        QByteArray encodedType = typeCoder;

        quint64 updateDelta = 0; // this is an edit so by definition, it's update is in sync
        ByteCountCoded<quint64> updateDeltaCoder = updateDelta;
        QByteArray encodedUpdateDelta = updateDeltaCoder;

        EntityPropertyFlags propertyFlags(PROP_LAST_ITEM);
        EntityPropertyFlags propertiesDidntFit = requestedProperties;

        LevelDetails entityLevel = packetData->startLevel();

        // Last Edited quint64 always first, before any other details, which allows us easy access to adjusting this
        // timestamp for clock skew
        quint64 lastEdited = properties.getLastEdited();
        bool successLastEditedFits = packetData->appendValue(lastEdited);

        bool successIDFits = packetData->appendRawData(encodedID);
        if (successIDFits) {
            successIDFits = packetData->appendRawData(encodedToken);
        }
        bool successTypeFits = packetData->appendRawData(encodedType);

        // NOTE: We intentionally do not send "created" times in edit messages. This is because:
        //   1) if the edit is to an existing entity, the created time can not be changed
        //   2) if the edit is to a new entity, the created time is the last edited time

        // TODO: Should we get rid of this in this in edit packets, since this has to always be 0?
        bool successLastUpdatedFits = packetData->appendRawData(encodedUpdateDelta);

        int propertyFlagsOffset = packetData->getUncompressedByteOffset();
        QByteArray encodedPropertyFlags = propertyFlags;
        int oldPropertyFlagsLength = encodedPropertyFlags.length();
        bool successPropertyFlagsFits = packetData->appendRawData(encodedPropertyFlags);
        int propertyCount = 0;

        bool headerFits = successIDFits && successTypeFits && successLastEditedFits &&
            successLastUpdatedFits && successPropertyFlagsFits;

        int startOfEntityItemData = packetData->getUncompressedByteOffset();

        if (headerFits) {
            bool successPropertyFits;
            propertyFlags -= PROP_LAST_ITEM; // clear the last item for now, we may or may not set it as the actual item

            // These items would go here once supported....
            //      PROP_PAGED_PROPERTY,
            //      PROP_CUSTOM_PROPERTIES_INCLUDED,


            APPEND_ENTITY_PROPERTY(PROP_SIMULATION_OWNER, properties._simulationOwner.toByteArray());
            APPEND_ENTITY_PROPERTY(PROP_PARENT_ID, properties.getParentID());
            APPEND_ENTITY_PROPERTY(PROP_PARENT_JOINT_INDEX, properties.getParentJointIndex());
            APPEND_ENTITY_PROPERTY(PROP_VISIBLE, properties.getVisible());
            APPEND_ENTITY_PROPERTY(PROP_NAME, properties.getName());
            APPEND_ENTITY_PROPERTY(PROP_LOCKED, properties.getLocked());
            APPEND_ENTITY_PROPERTY(PROP_USER_DATA, properties.getUserData());
            APPEND_ENTITY_PROPERTY(PROP_PRIVATE_USER_DATA, properties.getPrivateUserData());
            APPEND_ENTITY_PROPERTY(PROP_HREF, properties.getHref());
            APPEND_ENTITY_PROPERTY(PROP_DESCRIPTION, properties.getDescription());
            APPEND_ENTITY_PROPERTY(PROP_POSITION, properties.getPosition());
            APPEND_ENTITY_PROPERTY(PROP_DIMENSIONS, properties.getDimensions());
            APPEND_ENTITY_PROPERTY(PROP_ROTATION, properties.getRotation());
            APPEND_ENTITY_PROPERTY(PROP_REGISTRATION_POINT, properties.getRegistrationPoint());
            APPEND_ENTITY_PROPERTY(PROP_CREATED, properties.getCreated());
            APPEND_ENTITY_PROPERTY(PROP_LAST_EDITED_BY, properties.getLastEditedBy());
            // APPEND_ENTITY_PROPERTY(PROP_ENTITY_HOST_TYPE, (uint32_t)properties.getEntityHostType());              // not sent over the wire
            // APPEND_ENTITY_PROPERTY(PROP_OWNING_AVATAR_ID, properties.getOwningAvatarID());                        // not sent over the wire
            APPEND_ENTITY_PROPERTY(PROP_QUERY_AA_CUBE, properties.getQueryAACube());
            APPEND_ENTITY_PROPERTY(PROP_CAN_CAST_SHADOW, properties.getCanCastShadow());
            // APPEND_ENTITY_PROPERTY(PROP_VISIBLE_IN_SECONDARY_CAMERA, properties.getIsVisibleInSecondaryCamera()); // not sent over the wire
            APPEND_ENTITY_PROPERTY(PROP_RENDER_LAYER, (uint32_t)properties.getRenderLayer());
            APPEND_ENTITY_PROPERTY(PROP_PRIMITIVE_MODE, (uint32_t)properties.getPrimitiveMode());
            APPEND_ENTITY_PROPERTY(PROP_IGNORE_PICK_INTERSECTION, properties.getIgnorePickIntersection());
            APPEND_ENTITY_PROPERTY(PROP_RENDER_WITH_ZONES, properties.getRenderWithZones());
            APPEND_ENTITY_PROPERTY(PROP_BILLBOARD_MODE, (uint32_t)properties.getBillboardMode());
            _staticGrab.setProperties(properties);
            _staticGrab.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                                           propertiesDidntFit, propertyCount, appendState);

            // Physics
            APPEND_ENTITY_PROPERTY(PROP_DENSITY, properties.getDensity());
            APPEND_ENTITY_PROPERTY(PROP_VELOCITY, properties.getVelocity());
            APPEND_ENTITY_PROPERTY(PROP_ANGULAR_VELOCITY, properties.getAngularVelocity());
            APPEND_ENTITY_PROPERTY(PROP_GRAVITY, properties.getGravity());
            APPEND_ENTITY_PROPERTY(PROP_ACCELERATION, properties.getAcceleration());
            APPEND_ENTITY_PROPERTY(PROP_DAMPING, properties.getDamping());
            APPEND_ENTITY_PROPERTY(PROP_ANGULAR_DAMPING, properties.getAngularDamping());
            APPEND_ENTITY_PROPERTY(PROP_RESTITUTION, properties.getRestitution());
            APPEND_ENTITY_PROPERTY(PROP_FRICTION, properties.getFriction());
            APPEND_ENTITY_PROPERTY(PROP_LIFETIME, properties.getLifetime());
            APPEND_ENTITY_PROPERTY(PROP_COLLISIONLESS, properties.getCollisionless());
            APPEND_ENTITY_PROPERTY(PROP_COLLISION_MASK, properties.getCollisionMask());
            APPEND_ENTITY_PROPERTY(PROP_DYNAMIC, properties.getDynamic());
            APPEND_ENTITY_PROPERTY(PROP_COLLISION_SOUND_URL, properties.getCollisionSoundURL());
            APPEND_ENTITY_PROPERTY(PROP_ACTION_DATA, properties.getActionData());

            // Cloning
            APPEND_ENTITY_PROPERTY(PROP_CLONEABLE, properties.getCloneable());
            APPEND_ENTITY_PROPERTY(PROP_CLONE_LIFETIME, properties.getCloneLifetime());
            APPEND_ENTITY_PROPERTY(PROP_CLONE_LIMIT, properties.getCloneLimit());
            APPEND_ENTITY_PROPERTY(PROP_CLONE_DYNAMIC, properties.getCloneDynamic());
            APPEND_ENTITY_PROPERTY(PROP_CLONE_AVATAR_ENTITY, properties.getCloneAvatarEntity());
            APPEND_ENTITY_PROPERTY(PROP_CLONE_ORIGIN_ID, properties.getCloneOriginID());

            // Scripts
            APPEND_ENTITY_PROPERTY(PROP_SCRIPT, properties.getScript());
            APPEND_ENTITY_PROPERTY(PROP_SCRIPT_TIMESTAMP, properties.getScriptTimestamp());
            APPEND_ENTITY_PROPERTY(PROP_SERVER_SCRIPTS, properties.getServerScripts());

            // Certifiable Properties
            APPEND_ENTITY_PROPERTY(PROP_ITEM_NAME, properties.getItemName());
            APPEND_ENTITY_PROPERTY(PROP_ITEM_DESCRIPTION, properties.getItemDescription());
            APPEND_ENTITY_PROPERTY(PROP_ITEM_CATEGORIES, properties.getItemCategories());
            APPEND_ENTITY_PROPERTY(PROP_ITEM_ARTIST, properties.getItemArtist());
            APPEND_ENTITY_PROPERTY(PROP_ITEM_LICENSE, properties.getItemLicense());
            APPEND_ENTITY_PROPERTY(PROP_LIMITED_RUN, properties.getLimitedRun());
            APPEND_ENTITY_PROPERTY(PROP_MARKETPLACE_ID, properties.getMarketplaceID());
            APPEND_ENTITY_PROPERTY(PROP_EDITION_NUMBER, properties.getEditionNumber());
            APPEND_ENTITY_PROPERTY(PROP_ENTITY_INSTANCE_NUMBER, properties.getEntityInstanceNumber());
            APPEND_ENTITY_PROPERTY(PROP_CERTIFICATE_ID, properties.getCertificateID());
            APPEND_ENTITY_PROPERTY(PROP_CERTIFICATE_TYPE, properties.getCertificateType());
            APPEND_ENTITY_PROPERTY(PROP_STATIC_CERTIFICATE_VERSION, properties.getStaticCertificateVersion());

            if (properties.getType() == EntityTypes::ParticleEffect) {
                APPEND_ENTITY_PROPERTY(PROP_SHAPE_TYPE, (uint32_t)(properties.getShapeType()));
                APPEND_ENTITY_PROPERTY(PROP_COMPOUND_SHAPE_URL, properties.getCompoundShapeURL());
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA, properties.getAlpha());
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);
                APPEND_ENTITY_PROPERTY(PROP_TEXTURES, properties.getTextures());

                APPEND_ENTITY_PROPERTY(PROP_MAX_PARTICLES, properties.getMaxParticles());
                APPEND_ENTITY_PROPERTY(PROP_LIFESPAN, properties.getLifespan());

                APPEND_ENTITY_PROPERTY(PROP_EMITTING_PARTICLES, properties.getIsEmitting());
                APPEND_ENTITY_PROPERTY(PROP_EMIT_RATE, properties.getEmitRate());
                APPEND_ENTITY_PROPERTY(PROP_EMIT_SPEED, properties.getEmitSpeed());
                APPEND_ENTITY_PROPERTY(PROP_SPEED_SPREAD, properties.getSpeedSpread());
                APPEND_ENTITY_PROPERTY(PROP_EMIT_ORIENTATION, properties.getEmitOrientation());
                APPEND_ENTITY_PROPERTY(PROP_EMIT_DIMENSIONS, properties.getEmitDimensions());
                APPEND_ENTITY_PROPERTY(PROP_EMIT_RADIUS_START, properties.getEmitRadiusStart());

                APPEND_ENTITY_PROPERTY(PROP_POLAR_START, properties.getPolarStart());
                APPEND_ENTITY_PROPERTY(PROP_POLAR_FINISH, properties.getPolarFinish());
                APPEND_ENTITY_PROPERTY(PROP_AZIMUTH_START, properties.getAzimuthStart());
                APPEND_ENTITY_PROPERTY(PROP_AZIMUTH_FINISH, properties.getAzimuthFinish());

                APPEND_ENTITY_PROPERTY(PROP_EMIT_ACCELERATION, properties.getEmitAcceleration());
                APPEND_ENTITY_PROPERTY(PROP_ACCELERATION_SPREAD, properties.getAccelerationSpread());

                APPEND_ENTITY_PROPERTY(PROP_PARTICLE_RADIUS, properties.getParticleRadius());
                APPEND_ENTITY_PROPERTY(PROP_RADIUS_SPREAD, properties.getRadiusSpread());
                APPEND_ENTITY_PROPERTY(PROP_RADIUS_START, properties.getRadiusStart());
                APPEND_ENTITY_PROPERTY(PROP_RADIUS_FINISH, properties.getRadiusFinish());

                APPEND_ENTITY_PROPERTY(PROP_COLOR_SPREAD, properties.getColorSpread());
                APPEND_ENTITY_PROPERTY(PROP_COLOR_START, properties.getColorStart());
                APPEND_ENTITY_PROPERTY(PROP_COLOR_FINISH, properties.getColorFinish());

                APPEND_ENTITY_PROPERTY(PROP_ALPHA_SPREAD, properties.getAlphaSpread());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA_START, properties.getAlphaStart());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA_FINISH, properties.getAlphaFinish());

                APPEND_ENTITY_PROPERTY(PROP_EMITTER_SHOULD_TRAIL, properties.getEmitterShouldTrail());

                APPEND_ENTITY_PROPERTY(PROP_PARTICLE_SPIN, properties.getParticleSpin());
                APPEND_ENTITY_PROPERTY(PROP_SPIN_SPREAD, properties.getSpinSpread());
                APPEND_ENTITY_PROPERTY(PROP_SPIN_START, properties.getSpinStart());
                APPEND_ENTITY_PROPERTY(PROP_SPIN_FINISH, properties.getSpinFinish());
                APPEND_ENTITY_PROPERTY(PROP_PARTICLE_ROTATE_WITH_ENTITY, properties.getRotateWithEntity())
            }

            if (properties.getType() == EntityTypes::Model) {
                APPEND_ENTITY_PROPERTY(PROP_SHAPE_TYPE, (uint32_t)(properties.getShapeType()));
                APPEND_ENTITY_PROPERTY(PROP_COMPOUND_SHAPE_URL, properties.getCompoundShapeURL());
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_TEXTURES, properties.getTextures());

                APPEND_ENTITY_PROPERTY(PROP_MODEL_URL, properties.getModelURL());
                APPEND_ENTITY_PROPERTY(PROP_MODEL_SCALE, properties.getModelScale());
                APPEND_ENTITY_PROPERTY(PROP_JOINT_ROTATIONS_SET, properties.getJointRotationsSet());
                APPEND_ENTITY_PROPERTY(PROP_JOINT_ROTATIONS, properties.getJointRotations());
                APPEND_ENTITY_PROPERTY(PROP_JOINT_TRANSLATIONS_SET, properties.getJointTranslationsSet());
                APPEND_ENTITY_PROPERTY(PROP_JOINT_TRANSLATIONS, properties.getJointTranslations());
                APPEND_ENTITY_PROPERTY(PROP_RELAY_PARENT_JOINTS, properties.getRelayParentJoints());
                APPEND_ENTITY_PROPERTY(PROP_GROUP_CULLED, properties.getGroupCulled());
                APPEND_ENTITY_PROPERTY(PROP_BLENDSHAPE_COEFFICIENTS, properties.getBlendshapeCoefficients());
                APPEND_ENTITY_PROPERTY(PROP_USE_ORIGINAL_PIVOT, properties.getUseOriginalPivot());

                _staticAnimation.setProperties(properties);
                _staticAnimation.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);
            }

            if (properties.getType() == EntityTypes::Light) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_IS_SPOTLIGHT, properties.getIsSpotlight());
                APPEND_ENTITY_PROPERTY(PROP_INTENSITY, properties.getIntensity());
                APPEND_ENTITY_PROPERTY(PROP_EXPONENT, properties.getExponent());
                APPEND_ENTITY_PROPERTY(PROP_CUTOFF, properties.getCutoff());
                APPEND_ENTITY_PROPERTY(PROP_FALLOFF_RADIUS, properties.getFalloffRadius());
            }

            if (properties.getType() == EntityTypes::Text) {
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);

                APPEND_ENTITY_PROPERTY(PROP_TEXT, properties.getText());
                APPEND_ENTITY_PROPERTY(PROP_LINE_HEIGHT, properties.getLineHeight());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_COLOR, properties.getTextColor());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_ALPHA, properties.getTextAlpha());
                APPEND_ENTITY_PROPERTY(PROP_BACKGROUND_COLOR, properties.getBackgroundColor());
                APPEND_ENTITY_PROPERTY(PROP_BACKGROUND_ALPHA, properties.getBackgroundAlpha());
                APPEND_ENTITY_PROPERTY(PROP_LEFT_MARGIN, properties.getLeftMargin());
                APPEND_ENTITY_PROPERTY(PROP_RIGHT_MARGIN, properties.getRightMargin());
                APPEND_ENTITY_PROPERTY(PROP_TOP_MARGIN, properties.getTopMargin());
                APPEND_ENTITY_PROPERTY(PROP_BOTTOM_MARGIN, properties.getBottomMargin());
                APPEND_ENTITY_PROPERTY(PROP_UNLIT, properties.getUnlit());
                APPEND_ENTITY_PROPERTY(PROP_FONT, properties.getFont());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_EFFECT, (uint32_t)properties.getTextEffect());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_EFFECT_COLOR, properties.getTextEffectColor());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_EFFECT_THICKNESS, properties.getTextEffectThickness());
                APPEND_ENTITY_PROPERTY(PROP_TEXT_ALIGNMENT, (uint32_t)properties.getAlignment());
            }

            if (properties.getType() == EntityTypes::Zone) {
                APPEND_ENTITY_PROPERTY(PROP_SHAPE_TYPE, (uint32_t)properties.getShapeType());
                APPEND_ENTITY_PROPERTY(PROP_COMPOUND_SHAPE_URL, properties.getCompoundShapeURL());

                _staticKeyLight.setProperties(properties);
                _staticKeyLight.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);

                _staticAmbientLight.setProperties(properties);
                _staticAmbientLight.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);

                _staticSkybox.setProperties(properties);
                _staticSkybox.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);

                _staticHaze.setProperties(properties);
                _staticHaze.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);

                _staticBloom.setProperties(properties);
                _staticBloom.appendToEditPacket(packetData, requestedProperties, propertyFlags, propertiesDidntFit, propertyCount, appendState);

                APPEND_ENTITY_PROPERTY(PROP_FLYING_ALLOWED, properties.getFlyingAllowed());
                APPEND_ENTITY_PROPERTY(PROP_GHOSTING_ALLOWED, properties.getGhostingAllowed());
                APPEND_ENTITY_PROPERTY(PROP_FILTER_URL, properties.getFilterURL());

                APPEND_ENTITY_PROPERTY(PROP_KEY_LIGHT_MODE, (uint32_t)properties.getKeyLightMode());
                APPEND_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_MODE, (uint32_t)properties.getAmbientLightMode());
                APPEND_ENTITY_PROPERTY(PROP_SKYBOX_MODE, (uint32_t)properties.getSkyboxMode());
                APPEND_ENTITY_PROPERTY(PROP_HAZE_MODE, (uint32_t)properties.getHazeMode());
                APPEND_ENTITY_PROPERTY(PROP_BLOOM_MODE, (uint32_t)properties.getBloomMode());
                APPEND_ENTITY_PROPERTY(PROP_AVATAR_PRIORITY, (uint32_t)properties.getAvatarPriority());
                APPEND_ENTITY_PROPERTY(PROP_SCREENSHARE, (uint32_t)properties.getScreenshare());
            }

            if (properties.getType() == EntityTypes::PolyVox) {
                APPEND_ENTITY_PROPERTY(PROP_VOXEL_VOLUME_SIZE, properties.getVoxelVolumeSize());
                APPEND_ENTITY_PROPERTY(PROP_VOXEL_DATA, properties.getVoxelData());
                APPEND_ENTITY_PROPERTY(PROP_VOXEL_SURFACE_STYLE, properties.getVoxelSurfaceStyle());
                APPEND_ENTITY_PROPERTY(PROP_X_TEXTURE_URL, properties.getXTextureURL());
                APPEND_ENTITY_PROPERTY(PROP_Y_TEXTURE_URL, properties.getYTextureURL());
                APPEND_ENTITY_PROPERTY(PROP_Z_TEXTURE_URL, properties.getZTextureURL());
                APPEND_ENTITY_PROPERTY(PROP_X_N_NEIGHBOR_ID, properties.getXNNeighborID());
                APPEND_ENTITY_PROPERTY(PROP_Y_N_NEIGHBOR_ID, properties.getYNNeighborID());
                APPEND_ENTITY_PROPERTY(PROP_Z_N_NEIGHBOR_ID, properties.getZNNeighborID());
                APPEND_ENTITY_PROPERTY(PROP_X_P_NEIGHBOR_ID, properties.getXPNeighborID());
                APPEND_ENTITY_PROPERTY(PROP_Y_P_NEIGHBOR_ID, properties.getYPNeighborID());
                APPEND_ENTITY_PROPERTY(PROP_Z_P_NEIGHBOR_ID, properties.getZPNeighborID());
            }

            if (properties.getType() == EntityTypes::Web) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA, properties.getAlpha());
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);

                APPEND_ENTITY_PROPERTY(PROP_SOURCE_URL, properties.getSourceUrl());
                APPEND_ENTITY_PROPERTY(PROP_DPI, properties.getDPI());
                APPEND_ENTITY_PROPERTY(PROP_SCRIPT_URL, properties.getScriptURL());
                APPEND_ENTITY_PROPERTY(PROP_MAX_FPS, properties.getMaxFPS());
                APPEND_ENTITY_PROPERTY(PROP_INPUT_MODE, (uint32_t)properties.getInputMode());
                APPEND_ENTITY_PROPERTY(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, properties.getShowKeyboardFocusHighlight());
                APPEND_ENTITY_PROPERTY(PROP_WEB_USE_BACKGROUND, properties.getUseBackground());
                APPEND_ENTITY_PROPERTY(PROP_USER_AGENT, properties.getUserAgent());
            }

            if (properties.getType() == EntityTypes::Line) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());

                APPEND_ENTITY_PROPERTY(PROP_LINE_POINTS, properties.getLinePoints());
            }

            if (properties.getType() == EntityTypes::PolyLine) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_TEXTURES, properties.getTextures());

                APPEND_ENTITY_PROPERTY(PROP_LINE_POINTS, properties.getLinePoints());
                APPEND_ENTITY_PROPERTY(PROP_STROKE_WIDTHS, properties.getStrokeWidths());
                APPEND_ENTITY_PROPERTY(PROP_STROKE_NORMALS, properties.getPackedNormals());
                APPEND_ENTITY_PROPERTY(PROP_STROKE_COLORS, properties.getPackedStrokeColors());
                APPEND_ENTITY_PROPERTY(PROP_IS_UV_MODE_STRETCH, properties.getIsUVModeStretch());
                APPEND_ENTITY_PROPERTY(PROP_LINE_GLOW, properties.getGlow());
                APPEND_ENTITY_PROPERTY(PROP_LINE_FACE_CAMERA, properties.getFaceCamera());
            }

            // NOTE: Spheres and Boxes are just special cases of Shape, and they need to include their PROP_SHAPE
            // when encoding/decoding edits because otherwise they can't polymorph to other shape types
            if (properties.getType() == EntityTypes::Shape ||
                properties.getType() == EntityTypes::Box ||
                properties.getType() == EntityTypes::Sphere) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA, properties.getAlpha());
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);
                APPEND_ENTITY_PROPERTY(PROP_SHAPE, properties.getShape());
            }

            // Materials
            if (properties.getType() == EntityTypes::Material) {
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_URL, properties.getMaterialURL());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_MODE, (uint32_t)properties.getMaterialMappingMode());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_PRIORITY, properties.getPriority());
                APPEND_ENTITY_PROPERTY(PROP_PARENT_MATERIAL_NAME, properties.getParentMaterialName());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_POS, properties.getMaterialMappingPos());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_SCALE, properties.getMaterialMappingScale());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_ROT, properties.getMaterialMappingRot());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_DATA, properties.getMaterialData());
                APPEND_ENTITY_PROPERTY(PROP_MATERIAL_REPEAT, properties.getMaterialRepeat());
            }

            // Image
            if (properties.getType() == EntityTypes::Image) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA, properties.getAlpha());
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);

                APPEND_ENTITY_PROPERTY(PROP_IMAGE_URL, properties.getImageURL());
                APPEND_ENTITY_PROPERTY(PROP_EMISSIVE, properties.getEmissive());
                APPEND_ENTITY_PROPERTY(PROP_KEEP_ASPECT_RATIO, properties.getKeepAspectRatio());
                APPEND_ENTITY_PROPERTY(PROP_SUB_IMAGE, properties.getSubImage());
            }

            // Grid
            if (properties.getType() == EntityTypes::Grid) {
                APPEND_ENTITY_PROPERTY(PROP_COLOR, properties.getColor());
                APPEND_ENTITY_PROPERTY(PROP_ALPHA, properties.getAlpha());
                _staticPulse.setProperties(properties);
                _staticPulse.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);

                APPEND_ENTITY_PROPERTY(PROP_GRID_FOLLOW_CAMERA, properties.getFollowCamera());
                APPEND_ENTITY_PROPERTY(PROP_MAJOR_GRID_EVERY, properties.getMajorGridEvery());
                APPEND_ENTITY_PROPERTY(PROP_MINOR_GRID_EVERY, properties.getMinorGridEvery());
            }

            if (properties.getType() == EntityTypes::Gizmo) {
                APPEND_ENTITY_PROPERTY(PROP_GIZMO_TYPE, (uint32_t)properties.getGizmoType());
                _staticRing.setProperties(properties);
                _staticRing.appendToEditPacket(packetData, requestedProperties, propertyFlags,
                    propertiesDidntFit, propertyCount, appendState);
            }
        }

        if (propertyCount > 0) {
            int endOfEntityItemData = packetData->getUncompressedByteOffset();

            encodedPropertyFlags = propertyFlags;
            int newPropertyFlagsLength = encodedPropertyFlags.length();
            packetData->updatePriorBytes(propertyFlagsOffset, (const unsigned char*)encodedPropertyFlags.constData(),
                                         encodedPropertyFlags.length());

            // if the size of the PropertyFlags shrunk, we need to shift everything down to front of packet.
            if (newPropertyFlagsLength < oldPropertyFlagsLength) {
                int oldSize = packetData->getUncompressedSize();

                const unsigned char* modelItemData = packetData->getUncompressedData(propertyFlagsOffset + oldPropertyFlagsLength);
                int modelItemDataLength = endOfEntityItemData - startOfEntityItemData;
                int newEntityItemDataStart = propertyFlagsOffset + newPropertyFlagsLength;
                packetData->updatePriorBytes(newEntityItemDataStart, modelItemData, modelItemDataLength);

                int newSize = oldSize - (oldPropertyFlagsLength - newPropertyFlagsLength);
                packetData->setUncompressedSize(newSize);

            } else {
                assert(newPropertyFlagsLength == oldPropertyFlagsLength); // should not have grown
            }

            packetData->endLevel(entityLevel);
        } else {
            packetData->discardLevel(entityLevel);
            appendState = OctreeElement::NONE; // if we got here, then we didn't include the item
        }

        // If any part of the model items didn't fit, then the element is considered partial
        if (appendState != OctreeElement::COMPLETED) {
            didntFitProperties = propertiesDidntFit;
        }

        packetData->endSubTree();

        const char* finalizedData = reinterpret_cast<const char*>(packetData->getFinalizedData());
        int finalizedSize = packetData->getFinalizedSize();

        if (finalizedSize <= buffer.size()) {
            buffer.replace(0, finalizedSize, finalizedData, finalizedSize);
            buffer.resize(finalizedSize);
        } else {
            qCDebug(entities) << "ERROR - encoded edit message doesn't fit in output buffer.";
            appendState = OctreeElement::NONE; // if we got here, then we didn't include the item
            // maybe we should assert!!!
        }
    } else {
        packetData->discardSubTree();
    }

    return appendState;
}

QByteArray EntityItemProperties::getPackedNormals() const {
    return packNormals(getNormals());
}

QByteArray EntityItemProperties::packNormals(const QVector<vec3>& normals) const {
    int normalsSize = normals.size();
    QByteArray packedNormals = QByteArray(normalsSize * 6 + 1, '0');
    // add size of the array
    packedNormals[0] = ((uint8_t)normalsSize);

    int index = 1;
    for (int i = 0; i < normalsSize; i++) {
        int numBytes = packFloatVec3ToSignedTwoByteFixed((unsigned char*)packedNormals.data() + index, normals[i], 15);
        index += numBytes;
    }
    return packedNormals;
}

QByteArray EntityItemProperties::getPackedStrokeColors() const {
    return packStrokeColors(getStrokeColors());
}
QByteArray EntityItemProperties::packStrokeColors(const QVector<vec3>& strokeColors) const {
    int strokeColorsSize = strokeColors.size();
    QByteArray packedStrokeColors = QByteArray(strokeColorsSize * 3 + 1, '0');

    // add size of the array
    packedStrokeColors[0] = ((uint8_t)strokeColorsSize);


    for (int i = 0; i < strokeColorsSize; i++) {
        // add the color to the QByteArray
        packedStrokeColors[i * 3 + 1] = strokeColors[i].x * 255;
        packedStrokeColors[i * 3 + 2] = strokeColors[i].y * 255;
        packedStrokeColors[i * 3 + 3] = strokeColors[i].z * 255;
    }
    return packedStrokeColors;
}

// TODO:
//   how to handle lastEdited?
//   how to handle lastUpdated?
//   consider handling case where no properties are included... we should just ignore this packet...
//
// TODO: Right now, all possible properties for all subclasses are handled here. Ideally we'd prefer
//       to handle this in a more generic way. Allowing subclasses of EntityItem to register their properties
//
// TODO: There's a lot of repeated patterns in the code below to handle each property. It would be nice if the property
//       registration mechanism allowed us to collapse these repeated sections of code into a single implementation that
//       utilized the registration table to shorten up and simplify this code.
//
// TODO: Implement support for paged properties, spanning MTU, and custom properties
//
// TODO: Implement support for script and visible properties.
//
bool EntityItemProperties::decodeEntityEditPacket(const unsigned char* data, int bytesToRead, int& processedBytes,
                                                  EntityItemID& entityID, EntityItemProperties& properties) {
    bool valid = false;

    const unsigned char* dataAt = data;
    processedBytes = 0;

    // the first part of the data is an octcode, this is a required element of the edit packet format, but we don't
    // actually use it, we do need to skip it and read to the actual data we care about.
    int octets = numberOfThreeBitSectionsInCode(data);
    int bytesToReadOfOctcode = (int)bytesRequiredForCodeLength(octets);

    // we don't actually do anything with this octcode...
    dataAt += bytesToReadOfOctcode;
    processedBytes += bytesToReadOfOctcode;

    // Edit packets have a last edited time stamp immediately following the octcode.
    // NOTE: the edit times have been set by the editor to match out clock, so we don't need to adjust
    // these times for clock skew at this point.
    quint64 lastEdited;
    memcpy(&lastEdited, dataAt, sizeof(lastEdited));
    dataAt += sizeof(lastEdited);
    processedBytes += sizeof(lastEdited);
    properties.setLastEdited(lastEdited);

    // encoded id
    QUuid editID = QUuid::fromRfc4122(QByteArray::fromRawData(reinterpret_cast<const char*>(dataAt), NUM_BYTES_RFC4122_UUID));
    dataAt += NUM_BYTES_RFC4122_UUID;
    processedBytes += NUM_BYTES_RFC4122_UUID;

    entityID = editID;
    valid = true;

    // Entity Type...
    QByteArray encodedType((const char*)dataAt, (bytesToRead - processedBytes));
    ByteCountCoded<quint32> typeCoder = encodedType;
    quint32 entityTypeCode = typeCoder;
    properties.setType((EntityTypes::EntityType)entityTypeCode);
    encodedType = typeCoder; // determine true bytesToRead
    dataAt += encodedType.size();
    processedBytes += encodedType.size();

    // Update Delta - when was this item updated relative to last edit... this really should be 0
    // TODO: Should we get rid of this in this in edit packets, since this has to always be 0?
    // TODO: do properties need to handle lastupdated???

    // last updated is stored as ByteCountCoded delta from lastEdited
    QByteArray encodedUpdateDelta((const char*)dataAt, (bytesToRead - processedBytes));
    ByteCountCoded<quint64> updateDeltaCoder = encodedUpdateDelta;
    encodedUpdateDelta = updateDeltaCoder; // determine true bytesToRead
    dataAt += encodedUpdateDelta.size();
    processedBytes += encodedUpdateDelta.size();

    // TODO: Do we need this lastUpdated?? We don't seem to use it.
    //quint64 updateDelta = updateDeltaCoder;
    //quint64 lastUpdated = lastEdited + updateDelta; // don't adjust for clock skew since we already did that for lastEdited

    // Property Flags...
    QByteArray encodedPropertyFlags((const char*)dataAt, (bytesToRead - processedBytes));
    EntityPropertyFlags propertyFlags = encodedPropertyFlags;
    dataAt += propertyFlags.getEncodedLength();
    processedBytes += propertyFlags.getEncodedLength();

    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SIMULATION_OWNER, QByteArray, setSimulationOwner);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARENT_ID, QUuid, setParentID);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARENT_JOINT_INDEX, quint16, setParentJointIndex);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VISIBLE, bool, setVisible);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_NAME, QString, setName);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LOCKED, bool, setLocked);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_USER_DATA, QString, setUserData);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PRIVATE_USER_DATA, QString, setPrivateUserData);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_HREF, QString, setHref);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DESCRIPTION, QString, setDescription);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_POSITION, vec3, setPosition);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DIMENSIONS, vec3, setDimensions);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ROTATION, quat, setRotation);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_REGISTRATION_POINT, vec3, setRegistrationPoint);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CREATED, quint64, setCreated);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LAST_EDITED_BY, QUuid, setLastEditedBy);
    // READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ENTITY_HOST_TYPE, entity::HostType, setEntityHostType);            // not sent over the wire
    // READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_OWNING_AVATAR_ID, QUuid, setOwningAvatarID);                       // not sent over the wire
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_QUERY_AA_CUBE, AACube, setQueryAACube);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CAN_CAST_SHADOW, bool, setCanCastShadow);
    // READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VISIBLE_IN_SECONDARY_CAMERA, bool, setIsVisibleInSecondaryCamera); // not sent over the wire
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RENDER_LAYER, RenderLayer, setRenderLayer);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PRIMITIVE_MODE, PrimitiveMode, setPrimitiveMode);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_IGNORE_PICK_INTERSECTION, bool, setIgnorePickIntersection);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RENDER_WITH_ZONES, QVector<QUuid>, setRenderWithZones);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BILLBOARD_MODE, BillboardMode, setBillboardMode);
    properties.getGrab().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

    // Physics
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DENSITY, float, setDensity);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VELOCITY, vec3, setVelocity);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ANGULAR_VELOCITY, vec3, setAngularVelocity);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_GRAVITY, vec3, setGravity);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ACCELERATION, vec3, setAcceleration);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DAMPING, float, setDamping);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ANGULAR_DAMPING, float, setAngularDamping);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RESTITUTION, float, setRestitution);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_FRICTION, float, setFriction);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LIFETIME, float, setLifetime);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLLISIONLESS, bool, setCollisionless);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLLISION_MASK, uint16_t, setCollisionMask);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DYNAMIC, bool, setDynamic);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLLISION_SOUND_URL, QString, setCollisionSoundURL);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ACTION_DATA, QByteArray, setActionData);

    // Cloning
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONEABLE, bool, setCloneable);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONE_LIFETIME, float, setCloneLifetime);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONE_LIMIT, float, setCloneLimit);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONE_DYNAMIC, bool, setCloneDynamic);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONE_AVATAR_ENTITY, bool, setCloneAvatarEntity);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CLONE_ORIGIN_ID, QUuid, setCloneOriginID);

    // Scripts
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SCRIPT, QString, setScript);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SCRIPT_TIMESTAMP, quint64, setScriptTimestamp);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SERVER_SCRIPTS, QString, setServerScripts);

    // Certifiable Properties
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ITEM_NAME, QString, setItemName);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ITEM_DESCRIPTION, QString, setItemDescription);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ITEM_CATEGORIES, QString, setItemCategories);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ITEM_ARTIST, QString, setItemArtist);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ITEM_LICENSE, QString, setItemLicense);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LIMITED_RUN, quint32, setLimitedRun);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MARKETPLACE_ID, QString, setMarketplaceID);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EDITION_NUMBER, quint32, setEditionNumber);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ENTITY_INSTANCE_NUMBER, quint32, setEntityInstanceNumber);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CERTIFICATE_ID, QString, setCertificateID);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CERTIFICATE_TYPE, QString, setCertificateType);
    READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_STATIC_CERTIFICATE_VERSION, quint32, setStaticCertificateVersion);

    if (properties.getType() == EntityTypes::ParticleEffect) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SHAPE_TYPE, ShapeType, setShapeType);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COMPOUND_SHAPE_URL, QString, setCompoundShapeURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA, float, setAlpha);
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXTURES, QString, setTextures);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MAX_PARTICLES, quint32, setMaxParticles);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LIFESPAN, float, setLifespan);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMITTING_PARTICLES, bool, setIsEmitting);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_RATE, float, setEmitRate);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_SPEED, float, setEmitSpeed);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SPEED_SPREAD, float, setSpeedSpread);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_ORIENTATION, quat, setEmitOrientation);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_DIMENSIONS, vec3, setEmitDimensions);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_RADIUS_START, float, setEmitRadiusStart);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_POLAR_START, float, setPolarStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_POLAR_FINISH, float, setPolarFinish);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_AZIMUTH_START, float, setAzimuthStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_AZIMUTH_FINISH, float, setAzimuthFinish);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMIT_ACCELERATION, vec3, setEmitAcceleration);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ACCELERATION_SPREAD, vec3, setAccelerationSpread);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARTICLE_RADIUS, float, setParticleRadius);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RADIUS_SPREAD, float, setRadiusSpread);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RADIUS_START, float, setRadiusStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RADIUS_FINISH, float, setRadiusFinish);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR_SPREAD, u8vec3Color, setColorSpread);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR_START, vec3Color, setColorStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR_FINISH, vec3Color, setColorFinish);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA_SPREAD, float, setAlphaSpread);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA_START, float, setAlphaStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA_FINISH, float, setAlphaFinish);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMITTER_SHOULD_TRAIL, bool, setEmitterShouldTrail);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARTICLE_SPIN, float, setParticleSpin);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SPIN_SPREAD, float, setSpinSpread);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SPIN_START, float, setSpinStart);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SPIN_FINISH, float, setSpinFinish);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARTICLE_ROTATE_WITH_ENTITY, bool, setRotateWithEntity);
    }

    if (properties.getType() == EntityTypes::Model) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SHAPE_TYPE, ShapeType, setShapeType);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COMPOUND_SHAPE_URL, QString, setCompoundShapeURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXTURES, QString, setTextures);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MODEL_URL, QString, setModelURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MODEL_SCALE, vec3, setModelScale);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_JOINT_ROTATIONS_SET, QVector<bool>, setJointRotationsSet);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_JOINT_ROTATIONS, QVector<quat>, setJointRotations);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_JOINT_TRANSLATIONS_SET, QVector<bool>, setJointTranslationsSet);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_JOINT_TRANSLATIONS, QVector<vec3>, setJointTranslations);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RELAY_PARENT_JOINTS, bool, setRelayParentJoints);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_GROUP_CULLED, bool, setGroupCulled);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BLENDSHAPE_COEFFICIENTS, QString, setBlendshapeCoefficients);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_USE_ORIGINAL_PIVOT, bool, setUseOriginalPivot);

        properties.getAnimation().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
    }

    if (properties.getType() == EntityTypes::Light) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_IS_SPOTLIGHT, bool, setIsSpotlight);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_INTENSITY, float, setIntensity);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EXPONENT, float, setExponent);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_CUTOFF, float, setCutoff);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_FALLOFF_RADIUS, float, setFalloffRadius);
    }

    if (properties.getType() == EntityTypes::Text) {
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT, QString, setText);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LINE_HEIGHT, float, setLineHeight);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_COLOR, u8vec3Color, setTextColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_ALPHA, float, setTextAlpha);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BACKGROUND_COLOR, u8vec3Color, setBackgroundColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BACKGROUND_ALPHA, float, setBackgroundAlpha);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LEFT_MARGIN, float, setLeftMargin);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_RIGHT_MARGIN, float, setRightMargin);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TOP_MARGIN, float, setTopMargin);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BOTTOM_MARGIN, float, setBottomMargin);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_UNLIT, bool, setUnlit);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_FONT, QString, setFont);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_EFFECT, TextEffect, setTextEffect);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_EFFECT_COLOR, u8vec3Color, setTextEffectColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_EFFECT_THICKNESS, float, setTextEffectThickness);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXT_ALIGNMENT, TextAlignment, setAlignment);
    }

    if (properties.getType() == EntityTypes::Zone) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SHAPE_TYPE, ShapeType, setShapeType);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COMPOUND_SHAPE_URL, QString, setCompoundShapeURL);

        properties.getKeyLight().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
        properties.getAmbientLight().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
        properties.getSkybox().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
        properties.getHaze().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
        properties.getBloom().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_FLYING_ALLOWED, bool, setFlyingAllowed);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_GHOSTING_ALLOWED, bool, setGhostingAllowed);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_FILTER_URL, QString, setFilterURL);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_KEY_LIGHT_MODE, uint32_t, setKeyLightMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_AMBIENT_LIGHT_MODE, uint32_t, setAmbientLightMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SKYBOX_MODE, uint32_t, setSkyboxMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_HAZE_MODE, uint32_t, setHazeMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_BLOOM_MODE, uint32_t, setBloomMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_AVATAR_PRIORITY, uint32_t, setAvatarPriority);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SCREENSHARE, uint32_t, setScreenshare);
    }

    if (properties.getType() == EntityTypes::PolyVox) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VOXEL_VOLUME_SIZE, vec3, setVoxelVolumeSize);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VOXEL_DATA, QByteArray, setVoxelData);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_VOXEL_SURFACE_STYLE, uint16_t, setVoxelSurfaceStyle);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_X_TEXTURE_URL, QString, setXTextureURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Y_TEXTURE_URL, QString, setYTextureURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Z_TEXTURE_URL, QString, setZTextureURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_X_N_NEIGHBOR_ID, EntityItemID, setXNNeighborID);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Y_N_NEIGHBOR_ID, EntityItemID, setYNNeighborID);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Z_N_NEIGHBOR_ID, EntityItemID, setZNNeighborID);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_X_P_NEIGHBOR_ID, EntityItemID, setXPNeighborID);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Y_P_NEIGHBOR_ID, EntityItemID, setYPNeighborID);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_Z_P_NEIGHBOR_ID, EntityItemID, setZPNeighborID);
    }

    if (properties.getType() == EntityTypes::Web) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA, float, setAlpha);
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SOURCE_URL, QString, setSourceUrl);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_DPI, uint16_t, setDPI);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SCRIPT_URL, QString, setScriptURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MAX_FPS, uint8_t, setMaxFPS);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_INPUT_MODE, WebInputMode, setInputMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SHOW_KEYBOARD_FOCUS_HIGHLIGHT, bool, setShowKeyboardFocusHighlight);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_WEB_USE_BACKGROUND, bool, setUseBackground);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_USER_AGENT, QString, setUserAgent);
    }

    if (properties.getType() == EntityTypes::Line) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LINE_POINTS, QVector<vec3>, setLinePoints);
    }

    if (properties.getType() == EntityTypes::PolyLine) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_TEXTURES, QString, setTextures);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LINE_POINTS, QVector<vec3>, setLinePoints);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_STROKE_WIDTHS, QVector<float>, setStrokeWidths);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_STROKE_NORMALS, QByteArray, setPackedNormals);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_STROKE_COLORS, QByteArray, setPackedStrokeColors);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_IS_UV_MODE_STRETCH, bool, setIsUVModeStretch);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LINE_GLOW, bool, setGlow);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_LINE_FACE_CAMERA, bool, setFaceCamera);
    }

    // NOTE: Spheres and Boxes are just special cases of Shape, and they need to include their PROP_SHAPE
    // when encoding/decoding edits because otherwise they can't polymorph to other shape types
    if (properties.getType() == EntityTypes::Shape ||
        properties.getType() == EntityTypes::Box ||
        properties.getType() == EntityTypes::Sphere) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA, float, setAlpha);
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SHAPE, QString, setShape);
    }

    // Materials
    if (properties.getType() == EntityTypes::Material) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_URL, QString, setMaterialURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_MAPPING_MODE, MaterialMappingMode, setMaterialMappingMode);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_PRIORITY, quint16, setPriority);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_PARENT_MATERIAL_NAME, QString, setParentMaterialName);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_MAPPING_POS, vec2, setMaterialMappingPos);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_MAPPING_SCALE, vec2, setMaterialMappingScale);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_MAPPING_ROT, float, setMaterialMappingRot);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_DATA, QString, setMaterialData);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MATERIAL_REPEAT, bool, setMaterialRepeat);
    }

    // Image
    if (properties.getType() == EntityTypes::Image) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA, float, setAlpha);
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_IMAGE_URL, QString, setImageURL);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_EMISSIVE, bool, setEmissive);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_KEEP_ASPECT_RATIO, bool, setKeepAspectRatio);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_SUB_IMAGE, QRect, setSubImage);
    }

    // Grid
    if (properties.getType() == EntityTypes::Grid) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_COLOR, u8vec3Color, setColor);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_ALPHA, float, setAlpha);
        properties.getPulse().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);

        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_GRID_FOLLOW_CAMERA, bool, setFollowCamera);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MAJOR_GRID_EVERY, uint32_t, setMajorGridEvery);
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_MINOR_GRID_EVERY, float, setMinorGridEvery);
    }

    if (properties.getType() == EntityTypes::Gizmo) {
        READ_ENTITY_PROPERTY_TO_PROPERTIES(PROP_GIZMO_TYPE, GizmoType, setGizmoType);
        properties.getRing().decodeFromEditPacket(propertyFlags, dataAt, processedBytes);
    }

    return valid;
}

void EntityItemProperties::setPackedNormals(const QByteArray& value) {
    setNormals(unpackNormals(value));
}

QVector<vec3> EntityItemProperties::unpackNormals(const QByteArray& normals) {
    // the size of the vector is packed first
    QVector<vec3> unpackedNormals = QVector<vec3>((int)normals[0]);

    if ((int)normals[0] == normals.size() / 6) {
        int j = 0;
        for (int i = 1; i < normals.size();) {
            vec3 aux = vec3();
            i += unpackFloatVec3FromSignedTwoByteFixed((unsigned char*)normals.data() + i, aux, 15);
            unpackedNormals[j] = aux;
            j++;
        }
    } else {
        qCDebug(entities) << "WARNING - Expected received size for normals does not match. Expected: " << (int)normals[0]
                          << " Received: " << (normals.size() / 6);
    }
    return unpackedNormals;
}

void EntityItemProperties::setPackedStrokeColors(const QByteArray& value) {
    setStrokeColors(unpackStrokeColors(value));
}

QVector<vec3> EntityItemProperties::unpackStrokeColors(const QByteArray& strokeColors) {
    // the size of the vector is packed first
    QVector<vec3> unpackedStrokeColors = QVector<vec3>((int)strokeColors[0]);

    if ((int)strokeColors[0] == strokeColors.size() / 3) {
        int j = 0;
        for (int i = 1; i < strokeColors.size();) {

            float r = (uint8_t)strokeColors[i++] / 255.0f;
            float g = (uint8_t)strokeColors[i++] / 255.0f;
            float b = (uint8_t)strokeColors[i++] / 255.0f;
            unpackedStrokeColors[j++] = vec3(r, g, b);
        }
    } else {
        qCDebug(entities) << "WARNING - Expected received size for stroke colors does not match. Expected: "
            << (int)strokeColors[0] << " Received: " << (strokeColors.size() / 3);
    }

    return unpackedStrokeColors;
}

// NOTE: This version will only encode the portion of the edit message immediately following the
// header it does not include the send times and sequence number because that is handled by the
// edit packet sender...
bool EntityItemProperties::encodeEraseEntityMessage(const EntityItemID& entityItemID, QByteArray& buffer) {

    char* copyAt = buffer.data();
    uint16_t numberOfIds = 1; // only one entity ID in this message

    int outputLength = 0;

    if (buffer.size() < (int)(sizeof(numberOfIds) + NUM_BYTES_RFC4122_UUID)) {
        qCDebug(entities) << "ERROR - encodeEraseEntityMessage() called with buffer that is too small!";
        return false;
    }

    memcpy(copyAt, &numberOfIds, sizeof(numberOfIds));
    copyAt += sizeof(numberOfIds);
    outputLength = sizeof(numberOfIds);

    memcpy(copyAt, entityItemID.toRfc4122().constData(), NUM_BYTES_RFC4122_UUID);
    outputLength += NUM_BYTES_RFC4122_UUID;

    buffer.resize(outputLength);

    return true;
}

bool EntityItemProperties::encodeCloneEntityMessage(const EntityItemID& entityIDToClone, const EntityItemID& newEntityID, QByteArray& buffer) {
    char* copyAt = buffer.data();
    int outputLength = 0;

    if (buffer.size() < (int)(NUM_BYTES_RFC4122_UUID * 2)) {
        qCDebug(entities) << "ERROR - encodeCloneEntityMessage() called with buffer that is too small!";
        return false;
    }

    memcpy(copyAt, entityIDToClone.toRfc4122().constData(), NUM_BYTES_RFC4122_UUID);
    copyAt += NUM_BYTES_RFC4122_UUID;
    outputLength += NUM_BYTES_RFC4122_UUID;

    memcpy(copyAt, newEntityID.toRfc4122().constData(), NUM_BYTES_RFC4122_UUID);
    outputLength += NUM_BYTES_RFC4122_UUID;

    buffer.resize(outputLength);

    return true;
}

bool EntityItemProperties::decodeCloneEntityMessage(const QByteArray& buffer, int& processedBytes, EntityItemID& entityIDToClone, EntityItemID& newEntityID) {
    const unsigned char* packetData = (const unsigned char*)buffer.constData();
    const unsigned char* dataAt = packetData;
    size_t packetLength = buffer.size();
    processedBytes = 0;

    if (NUM_BYTES_RFC4122_UUID * 2 > packetLength) {
        qCDebug(entities) << "EntityItemProperties::decodeCloneEntityMessage().... bailing because not enough bytes in buffer";
        return false; // bail to prevent buffer overflow
    }

    QByteArray encodedID = buffer.mid((int)processedBytes, NUM_BYTES_RFC4122_UUID);
    entityIDToClone = QUuid::fromRfc4122(encodedID);
    dataAt += encodedID.size();
    processedBytes += encodedID.size();

    encodedID = buffer.mid((int)processedBytes, NUM_BYTES_RFC4122_UUID);
    newEntityID = QUuid::fromRfc4122(encodedID);
    dataAt += encodedID.size();
    processedBytes += encodedID.size();

    return true;
}

void EntityItemProperties::markAllChanged() {
    // Core
    _simulationOwnerChanged = true;
    _parentIDChanged = true;
    _parentJointIndexChanged = true;
    _visibleChanged = true;
    _nameChanged = true;
    _lockedChanged = true;
    _userDataChanged = true;
    _privateUserDataChanged = true;
    _hrefChanged = true;
    _descriptionChanged = true;
    _positionChanged = true;
    _dimensionsChanged = true;
    _rotationChanged = true;
    _registrationPointChanged = true;
    _createdChanged = true;
    _lastEditedByChanged = true;
    _entityHostTypeChanged = true;
    _owningAvatarIDChanged = true;
    _queryAACubeChanged = true;
    _canCastShadowChanged = true;
    _isVisibleInSecondaryCameraChanged = true;
    _renderLayerChanged = true;
    _primitiveModeChanged = true;
    _ignorePickIntersectionChanged = true;
    _renderWithZonesChanged = true;
    _billboardModeChanged = true;
    _grab.markAllChanged();

    // Physics
    _densityChanged = true;
    _velocityChanged = true;
    _angularVelocityChanged = true;
    _gravityChanged = true;
    _accelerationChanged = true;
    _dampingChanged = true;
    _angularDampingChanged = true;
    _restitutionChanged = true;
    _frictionChanged = true;
    _lifetimeChanged = true;
    _collisionlessChanged = true;
    _collisionMaskChanged = true;
    _dynamicChanged = true;
    _collisionSoundURLChanged = true;
    _actionDataChanged = true;

    // Cloning
    _cloneableChanged = true;
    _cloneLifetimeChanged = true;
    _cloneLimitChanged = true;
    _cloneDynamicChanged = true;
    _cloneAvatarEntityChanged = true;
    _cloneOriginIDChanged = true;

    // Scripts
    _scriptChanged = true;
    _scriptTimestampChanged = true;
    _serverScriptsChanged = true;

    // Certifiable Properties
    _itemNameChanged = true;
    _itemDescriptionChanged = true;
    _itemCategoriesChanged = true;
    _itemArtistChanged = true;
    _itemLicenseChanged = true;
    _limitedRunChanged = true;
    _marketplaceIDChanged = true;
    _editionNumberChanged = true;
    _entityInstanceNumberChanged = true;
    _certificateIDChanged = true;
    _certificateTypeChanged = true;
    _staticCertificateVersionChanged = true;

    // Common
    _shapeTypeChanged = true;
    _compoundShapeURLChanged = true;
    _colorChanged = true;
    _alphaChanged = true;
    _pulse.markAllChanged();
    _texturesChanged = true;

    // Particles
    _maxParticlesChanged = true;
    _lifespanChanged = true;
    _isEmittingChanged = true;
    _emitRateChanged = true;
    _emitSpeedChanged = true;
    _speedSpreadChanged = true;
    _emitOrientationChanged = true;
    _emitDimensionsChanged = true;
    _emitRadiusStartChanged = true;
    _polarStartChanged = true;
    _polarFinishChanged = true;
    _azimuthStartChanged = true;
    _azimuthFinishChanged = true;
    _emitAccelerationChanged = true;
    _accelerationSpreadChanged = true;
    _particleRadiusChanged = true;
    _radiusSpreadChanged = true;
    _radiusStartChanged = true;
    _radiusFinishChanged = true;
    _colorSpreadChanged = true;
    _colorStartChanged = true;
    _colorFinishChanged = true;
    _alphaSpreadChanged = true;
    _alphaStartChanged = true;
    _alphaFinishChanged = true;
    _emitterShouldTrailChanged = true;
    _particleSpinChanged = true;
    _spinStartChanged = true;
    _spinFinishChanged = true;
    _spinSpreadChanged = true;
    _rotateWithEntityChanged = true;

    // Model
    _modelURLChanged = true;
    _modelScaleChanged = true;
    _jointRotationsSetChanged = true;
    _jointRotationsChanged = true;
    _jointTranslationsSetChanged = true;
    _jointTranslationsChanged = true;
    _relayParentJointsChanged = true;
    _groupCulledChanged = true;
    _blendshapeCoefficientsChanged = true;
    _useOriginalPivotChanged = true;
    _animation.markAllChanged();

    // Light
    _isSpotlightChanged = true;
    _intensityChanged = true;
    _exponentChanged = true;
    _cutoffChanged = true;
    _falloffRadiusChanged = true;

    // Text
    _textChanged = true;
    _lineHeightChanged = true;
    _textColorChanged = true;
    _textAlphaChanged = true;
    _backgroundColorChanged = true;
    _backgroundAlphaChanged = true;
    _leftMarginChanged = true;
    _rightMarginChanged = true;
    _topMarginChanged = true;
    _bottomMarginChanged = true;
    _unlitChanged = true;
    _fontChanged = true;
    _textEffectChanged = true;
    _textEffectColorChanged = true;
    _textEffectThicknessChanged = true;
    _alignmentChanged = true;

    // Zone
    _keyLight.markAllChanged();
    _ambientLight.markAllChanged();
    _skybox.markAllChanged();
    _haze.markAllChanged();
    _bloom.markAllChanged();
    _flyingAllowedChanged = true;
    _ghostingAllowedChanged = true;
    _filterURLChanged = true;
    _keyLightModeChanged = true;
    _ambientLightModeChanged = true;
    _skyboxModeChanged = true;
    _hazeModeChanged = true;
    _bloomModeChanged = true;
    _avatarPriorityChanged = true;
    _screenshareChanged = true;

    // Polyvox
    _voxelVolumeSizeChanged = true;
    _voxelDataChanged = true;
    _voxelSurfaceStyleChanged = true;
    _xTextureURLChanged = true;
    _yTextureURLChanged = true;
    _zTextureURLChanged = true;
    _xNNeighborIDChanged = true;
    _yNNeighborIDChanged = true;
    _zNNeighborIDChanged = true;
    _xPNeighborIDChanged = true;
    _yPNeighborIDChanged = true;
    _zPNeighborIDChanged = true;

    // Web
    _sourceUrlChanged = true;
    _dpiChanged = true;
    _scriptURLChanged = true;
    _maxFPSChanged = true;
    _inputModeChanged = true;
    _showKeyboardFocusHighlightChanged = true;
    _useBackgroundChanged = true;
    _userAgentChanged = true;

    // Polyline
    _linePointsChanged = true;
    _strokeWidthsChanged = true;
    _normalsChanged = true;
    _strokeColorsChanged = true;
    _isUVModeStretchChanged = true;
    _glowChanged = true;
    _faceCameraChanged = true;

    // Shape
    _shapeChanged = true;

    // Material
    _materialURLChanged = true;
    _materialMappingModeChanged = true;
    _priorityChanged = true;
    _parentMaterialNameChanged = true;
    _materialMappingPosChanged = true;
    _materialMappingScaleChanged = true;
    _materialMappingRotChanged = true;
    _materialDataChanged = true;
    _materialRepeatChanged = true;

    // Image
    _imageURLChanged = true;
    _emissiveChanged = true;
    _keepAspectRatioChanged = true;
    _subImageChanged = true;

    // Grid
    _followCameraChanged = true;
    _majorGridEveryChanged = true;
    _minorGridEveryChanged = true;

    // Gizmo
    _gizmoTypeChanged = true;
    _ring.markAllChanged();
}

// The minimum bounding box for the entity.
AABox EntityItemProperties::getAABox() const {

    // _position represents the position of the registration point.
    vec3 registrationRemainder = vec3(1.0f) - _registrationPoint;

    vec3 unrotatedMinRelativeToEntity = -(_dimensions * _registrationPoint);
    vec3 unrotatedMaxRelativeToEntity = _dimensions * registrationRemainder;
    Extents unrotatedExtentsRelativeToRegistrationPoint = { unrotatedMinRelativeToEntity, unrotatedMaxRelativeToEntity };
    Extents rotatedExtentsRelativeToRegistrationPoint = unrotatedExtentsRelativeToRegistrationPoint.getRotated(_rotation);

    // shift the extents to be relative to the position/registration point
    rotatedExtentsRelativeToRegistrationPoint.shiftBy(_position);

    return AABox(rotatedExtentsRelativeToRegistrationPoint);
}

bool EntityItemProperties::hasTransformOrVelocityChanges() const {
    return _positionChanged || _localPositionChanged
        || _rotationChanged || _localRotationChanged
        || _velocityChanged || _localVelocityChanged
        || _angularVelocityChanged || _localAngularVelocityChanged
        || _accelerationChanged;
}

void EntityItemProperties::clearTransformOrVelocityChanges() {
    _positionChanged = false;
    _localPositionChanged = false;
    _rotationChanged = false;
    _localRotationChanged = false;
    _velocityChanged = false;
    _localVelocityChanged = false;
    _angularVelocityChanged = false;
    _localAngularVelocityChanged = false;
    _accelerationChanged = false;
}

bool EntityItemProperties::hasMiscPhysicsChanges() const {
    return _gravityChanged || _dimensionsChanged || _densityChanged || _frictionChanged
        || _restitutionChanged || _dampingChanged || _angularDampingChanged || _registrationPointChanged ||
        _compoundShapeURLChanged || _dynamicChanged || _collisionlessChanged || _collisionMaskChanged;
}

bool EntityItemProperties::hasSimulationRestrictedChanges() const {
    return _positionChanged || _localPositionChanged
        || _rotationChanged || _localRotationChanged
        || _velocityChanged || _localVelocityChanged
        || _localDimensionsChanged || _dimensionsChanged
        || _angularVelocityChanged || _localAngularVelocityChanged
        || _accelerationChanged
        || _parentIDChanged || _parentJointIndexChanged;
}

void EntityItemProperties::copySimulationRestrictedProperties(const EntityItemPointer& entity) {
    if (!_parentIDChanged) {
        setParentID(entity->getParentID());
    }
    if (!_parentJointIndexChanged) {
        setParentJointIndex(entity->getParentJointIndex());
    }
    if (!_localPositionChanged && !_positionChanged) {
        setPosition(entity->getWorldPosition());
    }
    if (!_localRotationChanged && !_rotationChanged) {
        setRotation(entity->getWorldOrientation());
    }
    if (!_localVelocityChanged && !_velocityChanged) {
        setVelocity(entity->getWorldVelocity());
    }
    if (!_localAngularVelocityChanged && !_angularVelocityChanged) {
        setAngularVelocity(entity->getWorldAngularVelocity());
    }
    if (!_accelerationChanged) {
        setAcceleration(entity->getAcceleration());
    }
    if (!_localDimensionsChanged && !_dimensionsChanged) {
        setLocalDimensions(entity->getScaledDimensions());
    }
}

void EntityItemProperties::clearSimulationRestrictedProperties() {
    _positionChanged = false;
    _localPositionChanged = false;
    _rotationChanged = false;
    _localRotationChanged = false;
    _velocityChanged = false;
    _localVelocityChanged = false;
    _angularVelocityChanged = false;
    _localAngularVelocityChanged = false;
    _accelerationChanged = false;
    _parentIDChanged = false;
    _parentJointIndexChanged = false;
}

void EntityItemProperties::clearSimulationOwner() {
    _simulationOwner.clear();
    _simulationOwnerChanged = true;
}

void EntityItemProperties::setSimulationOwner(const QUuid& id, uint8_t priority) {
    if (!_simulationOwner.matchesValidID(id) || _simulationOwner.getPriority() != priority) {
        _simulationOwner.set(id, priority);
        _simulationOwnerChanged = true;
    }
}

void EntityItemProperties::setSimulationOwner(const QByteArray& data) {
    if (_simulationOwner.fromByteArray(data)) {
        _simulationOwnerChanged = true;
    }
}

uint8_t EntityItemProperties::computeSimulationBidPriority() const {
    uint8_t priority = 0;
    if (_parentIDChanged || _parentJointIndexChanged) {
        // we need higher simulation ownership priority to chang parenting info
        priority = SCRIPT_GRAB_SIMULATION_PRIORITY;
    } else if (_positionChanged || _localPositionChanged
            || _rotationChanged || _localRotationChanged
            || _velocityChanged || _localVelocityChanged
            || _angularVelocityChanged || _localAngularVelocityChanged) {
        priority = SCRIPT_POKE_SIMULATION_PRIORITY;
    }
    return priority;
}

QList<QString> EntityItemProperties::listChangedProperties() {
    QList<QString> out;

    // Core
    if (simulationOwnerChanged()) {
        out += "simulationOwner";
    }
    if (parentIDChanged()) {
        out += "parentID";
    }
    if (parentJointIndexChanged()) {
        out += "parentJointIndex";
    }
    if (visibleChanged()) {
        out += "visible";
    }
    if (nameChanged()) {
        out += "name";
    }
    if (lockedChanged()) {
        out += "locked";
    }
    if (userDataChanged()) {
        out += "userData";
    }
    if (privateUserDataChanged()) {
        out += "privateUserData";
    }
    if (hrefChanged()) {
        out += "href";
    }
    if (descriptionChanged()) {
        out += "description";
    }
    if (containsPositionChange()) {
        out += "position";
    }
    if (dimensionsChanged()) {
        out += "dimensions";
    }
    if (rotationChanged()) {
        out += "rotation";
    }
    if (registrationPointChanged()) {
        out += "registrationPoint";
    }
    if (createdChanged()) {
        out += "created";
    }
    if (lastEditedByChanged()) {
        out += "lastEditedBy";
    }
    if (entityHostTypeChanged()) {
        out += "entityHostType";
    }
    if (owningAvatarIDChanged()) {
        out += "owningAvatarID";
    }
    if (queryAACubeChanged()) {
        out += "queryAACube";
    }
    if (canCastShadowChanged()) {
        out += "canCastShadow";
    }
    if (isVisibleInSecondaryCameraChanged()) {
        out += "isVisibleInSecondaryCamera";
    }
    if (renderLayerChanged()) {
        out += "renderLayer";
    }
    if (primitiveModeChanged()) {
        out += "primitiveMode";
    }
    if (ignorePickIntersectionChanged()) {
        out += "ignorePickIntersection";
    }
    if (renderWithZonesChanged()) {
        out += "renderWithZones";
    }
    if (billboardModeChanged()) {
        out += "billboardMode";
    }
    getGrab().listChangedProperties(out);

    // Physics
    if (densityChanged()) {
        out += "density";
    }
    if (velocityChanged()) {
        out += "velocity";
    }
    if (angularVelocityChanged()) {
        out += "angularVelocity";
    }
    if (gravityChanged()) {
        out += "gravity";
    }
    if (accelerationChanged()) {
        out += "acceleration";
    }
    if (dampingChanged()) {
        out += "damping";
    }
    if (angularDampingChanged()) {
        out += "angularDamping";
    }
    if (restitutionChanged()) {
        out += "restitution";
    }
    if (frictionChanged()) {
        out += "friction";
    }
    if (lifetimeChanged()) {
        out += "lifetime";
    }
    if (collisionlessChanged()) {
        out += "collisionless";
    }
    if (collisionMaskChanged()) {
        out += "collisionMask";
    }
    if (dynamicChanged()) {
        out += "dynamic";
    }
    if (collisionSoundURLChanged()) {
        out += "collisionSoundURL";
    }
    if (actionDataChanged()) {
        out += "actionData";
    }

    // Cloning
    if (cloneableChanged()) {
        out += "cloneable";
    }
    if (cloneLifetimeChanged()) {
        out += "cloneLifetime";
    }
    if (cloneLimitChanged()) {
        out += "cloneLimit";
    }
    if (cloneDynamicChanged()) {
        out += "cloneDynamic";
    }
    if (cloneAvatarEntityChanged()) {
        out += "cloneAvatarEntity";
    }
    if (cloneOriginIDChanged()) {
        out += "cloneOriginID";
    }

    // Scripts
    if (scriptChanged()) {
        out += "script";
    }
    if (scriptTimestampChanged()) {
        out += "scriptTimestamp";
    }
    if (serverScriptsChanged()) {
        out += "serverScripts";
    }

    // Certifiable Properties
    if (itemNameChanged()) {
        out += "itemName";
    }
    if (itemDescriptionChanged()) {
        out += "itemDescription";
    }
    if (itemCategoriesChanged()) {
        out += "itemCategories";
    }
    if (itemArtistChanged()) {
        out += "itemArtist";
    }
    if (itemLicenseChanged()) {
        out += "itemLicense";
    }
    if (limitedRunChanged()) {
        out += "limitedRun";
    }
    if (marketplaceIDChanged()) {
        out += "marketplaceID";
    }
    if (editionNumberChanged()) {
        out += "editionNumber";
    }
    if (entityInstanceNumberChanged()) {
        out += "entityInstanceNumber";
    }
    if (certificateIDChanged()) {
        out += "certificateID";
    }
    if (certificateTypeChanged()) {
        out += "certificateType";
    }
    if (staticCertificateVersionChanged()) {
        out += "staticCertificateVersion";
    }

    // Common
    if (shapeTypeChanged()) {
        out += "shapeType";
    }
    if (compoundShapeURLChanged()) {
        out += "compoundShapeURL";
    }
    if (colorChanged()) {
        out += "color";
    }
    if (alphaChanged()) {
        out += "alpha";
    }
    getPulse().listChangedProperties(out);
    if (texturesChanged()) {
        out += "textures";
    }

    // Particles
    if (maxParticlesChanged()) {
        out += "maxParticles";
    }
    if (lifespanChanged()) {
        out += "lifespan";
    }
    if (isEmittingChanged()) {
        out += "isEmitting";
    }
    if (emitRateChanged()) {
        out += "emitRate";
    }
    if (emitSpeedChanged()) {
        out += "emitSpeed";
    }
    if (speedSpreadChanged()) {
        out += "speedSpread";
    }
    if (emitOrientationChanged()) {
        out += "emitOrientation";
    }
    if (emitDimensionsChanged()) {
        out += "emitDimensions";
    }
    if (emitRadiusStartChanged()) {
        out += "emitRadiusStart";
    }
    if (polarStartChanged()) {
        out += "polarStart";
    }
    if (polarFinishChanged()) {
        out += "polarFinish";
    }
    if (azimuthStartChanged()) {
        out += "azimuthStart";
    }
    if (azimuthFinishChanged()) {
        out += "azimuthFinish";
    }
    if (emitAccelerationChanged()) {
        out += "emitAcceleration";
    }
    if (accelerationSpreadChanged()) {
        out += "accelerationSpread";
    }
    if (particleRadiusChanged()) {
        out += "particleRadius";
    }
    if (radiusSpreadChanged()) {
        out += "radiusSpread";
    }
    if (radiusStartChanged()) {
        out += "radiusStart";
    }
    if (radiusFinishChanged()) {
        out += "radiusFinish";
    }
    if (colorSpreadChanged()) {
        out += "colorSpread";
    }
    if (colorStartChanged()) {
        out += "colorStart";
    }
    if (colorFinishChanged()) {
        out += "colorFinish";
    }
    if (alphaSpreadChanged()) {
        out += "alphaSpread";
    }
    if (alphaStartChanged()) {
        out += "alphaStart";
    }
    if (alphaFinishChanged()) {
        out += "alphaFinish";
    }
    if (emitterShouldTrailChanged()) {
        out += "emitterShouldTrail";
    }
    if (particleSpinChanged()) {
        out += "particleSpin";
    }
    if (spinSpreadChanged()) {
        out += "spinSpread";
    }
    if (spinStartChanged()) {
        out += "spinStart";
    }
    if (spinFinishChanged()) {
        out += "spinFinish";
    }
    if (rotateWithEntityChanged()) {
        out += "rotateWithEntity";
    }

    // Model
    if (modelURLChanged()) {
        out += "modelURL";
    }
    if (modelScaleChanged()) {
        out += "scale";
    }
    if (jointRotationsSetChanged()) {
        out += "jointRotationsSet";
    }
    if (jointRotationsChanged()) {
        out += "jointRotations";
    }
    if (jointTranslationsSetChanged()) {
        out += "jointTranslationsSet";
    }
    if (jointTranslationsChanged()) {
        out += "jointTranslations";
    }
    if (relayParentJointsChanged()) {
        out += "relayParentJoints";
    }
    if (groupCulledChanged()) {
        out += "groupCulled";
    }
    if (blendshapeCoefficientsChanged()) {
        out += "blendshapeCoefficients";
    }
    if (useOriginalPivotChanged()) {
        out += "useOriginalPivot";
    }
    getAnimation().listChangedProperties(out);

    // Light
    if (isSpotlightChanged()) {
        out += "isSpotlight";
    }
    if (intensityChanged()) {
        out += "intensity";
    }
    if (exponentChanged()) {
        out += "exponent";
    }
    if (cutoffChanged()) {
        out += "cutoff";
    }
    if (falloffRadiusChanged()) {
        out += "falloffRadius";
    }

    // Text
    if (textChanged()) {
        out += "text";
    }
    if (lineHeightChanged()) {
        out += "lineHeight";
    }
    if (textColorChanged()) {
        out += "textColor";
    }
    if (textAlphaChanged()) {
        out += "textAlpha";
    }
    if (backgroundColorChanged()) {
        out += "backgroundColor";
    }
    if (backgroundAlphaChanged()) {
        out += "backgroundAlpha";
    }
    if (leftMarginChanged()) {
        out += "leftMargin";
    }
    if (rightMarginChanged()) {
        out += "rightMargin";
    }
    if (topMarginChanged()) {
        out += "topMargin";
    }
    if (bottomMarginChanged()) {
        out += "bottomMargin";
    }
    if (unlitChanged()) {
        out += "unlit";
    }
    if (fontChanged()) {
        out += "font";
    }
    if (textEffectChanged()) {
        out += "textEffect";
    }
    if (textEffectColorChanged()) {
        out += "textEffectColor";
    }
    if (textEffectThicknessChanged()) {
        out += "textEffectThickness";
    }
    if (alignmentChanged()) {
        out += "alignment";
    }

    // Zone
    getKeyLight().listChangedProperties(out);
    getAmbientLight().listChangedProperties(out);
    getSkybox().listChangedProperties(out);
    getHaze().listChangedProperties(out);
    getBloom().listChangedProperties(out);
    if (flyingAllowedChanged()) {
        out += "flyingAllowed";
    }
    if (ghostingAllowedChanged()) {
        out += "ghostingAllowed";
    }
    if (filterURLChanged()) {
        out += "filterURL";
    }
    if (keyLightModeChanged()) {
        out += "keyLightMode";
    }
    if (ambientLightModeChanged()) {
        out += "ambientLightMode";
    }
    if (skyboxModeChanged()) {
        out += "skyboxMode";
    }
    if (hazeModeChanged()) {
        out += "hazeMode";
    }
    if (bloomModeChanged()) {
        out += "bloomMode";
    }
    if (avatarPriorityChanged()) {
        out += "avatarPriority";
    }
    if (screenshareChanged()) {
        out += "screenshare";
    }

    // Polyvox
    if (voxelVolumeSizeChanged()) {
        out += "voxelVolumeSize";
    }
    if (voxelDataChanged()) {
        out += "voxelData";
    }
    if (voxelSurfaceStyleChanged()) {
        out += "voxelSurfaceStyle";
    }
    if (xTextureURLChanged()) {
        out += "xTextureURL";
    }
    if (yTextureURLChanged()) {
        out += "yTextureURL";
    }
    if (zTextureURLChanged()) {
        out += "zTextureURL";
    }
    if (xNNeighborIDChanged()) {
        out += "xNNeighborID";
    }
    if (yNNeighborIDChanged()) {
        out += "yNNeighborID";
    }
    if (zNNeighborIDChanged()) {
        out += "zNNeighborID";
    }
    if (xPNeighborIDChanged()) {
        out += "xPNeighborID";
    }
    if (yPNeighborIDChanged()) {
        out += "yPNeighborID";
    }
    if (zPNeighborIDChanged()) {
        out += "zPNeighborID";
    }

    // Web
    if (sourceUrlChanged()) {
        out += "sourceUrl";
    }
    if (dpiChanged()) {
        out += "dpi";
    }
    if (scriptURLChanged()) {
        out += "scriptURL";
    }
    if (maxFPSChanged()) {
        out += "maxFPS";
    }
    if (inputModeChanged()) {
        out += "inputMode";
    }

    // Polyline
    if (linePointsChanged()) {
        out += "linePoints";
    }
    if (strokeWidthsChanged()) {
        out += "strokeWidths";
    }
    if (normalsChanged()) {
        out += "normals";
    }
    if (strokeColorsChanged()) {
        out += "strokeColors";
    }
    if (isUVModeStretchChanged()) {
        out += "isUVModeStretch";
    }
    if (glowChanged()) {
        out += "glow";
    }
    if (faceCameraChanged()) {
        out += "faceCamera";
    }
    if (showKeyboardFocusHighlightChanged()) {
        out += "showKeyboardFocusHighlight";
    }
    if (useBackgroundChanged()) {
        out += "useBackground";
    }
    if (userAgentChanged()) {
        out += "userAgent";
    }

    // Shape
    if (shapeChanged()) {
        out += "shape";
    }

    // Material
    if (materialURLChanged()) {
        out += "materialURL";
    }
    if (materialMappingModeChanged()) {
        out += "materialMappingMode";
    }
    if (priorityChanged()) {
        out += "priority";
    }
    if (parentMaterialNameChanged()) {
        out += "parentMaterialName";
    }
    if (materialMappingPosChanged()) {
        out += "materialMappingPos";
    }
    if (materialMappingScaleChanged()) {
        out += "materialMappingScale";
    }
    if (materialMappingRotChanged()) {
        out += "materialMappingRot";
    }
    if (materialDataChanged()) {
        out += "materialData";
    }
    if (materialRepeatChanged()) {
        out += "materialRepeat";
    }

    // Image
    if (imageURLChanged()) {
        out += "imageURL";
    }
    if (emissiveChanged()) {
        out += "emissive";
    }
    if (keepAspectRatioChanged()) {
        out += "keepAspectRatio";
    }
    if (subImageChanged()) {
        out += "subImage";
    }

    // Grid
    if (followCameraChanged()) {
        out += "followCamera";
    }
    if (majorGridEveryChanged()) {
        out += "majorGridEvery";
    }
    if (minorGridEveryChanged()) {
        out += "minorGridEvery";
    }

    // Gizmo
    if (gizmoTypeChanged()) {
        out += "gizmoType";
    }
    getRing().listChangedProperties(out);

    return out;
}

bool EntityItemProperties::transformChanged() const {
    return positionChanged() || rotationChanged() ||
        localPositionChanged() || localRotationChanged();
}

bool EntityItemProperties::getScalesWithParent() const {
    // keep this logic the same as in EntityItem::getScalesWithParent
    bool scalesWithParent { false };
    if (parentIDChanged()) {
        bool success;
        SpatiallyNestablePointer parent = SpatiallyNestable::findByID(getParentID(), success);
        if (success && parent) {
            bool avatarAncestor = (parent->getNestableType() == NestableType::Avatar ||
                                   parent->hasAncestorOfType(NestableType::Avatar));
            scalesWithParent = getEntityHostType() == entity::HostType::AVATAR && avatarAncestor;
        }
    }
    return scalesWithParent;
}

bool EntityItemProperties::parentRelatedPropertyChanged() const {
    return positionChanged() || rotationChanged() ||
        localPositionChanged() || localRotationChanged() ||
        localDimensionsChanged() ||
        parentIDChanged() || parentJointIndexChanged();
}

bool EntityItemProperties::queryAACubeRelatedPropertyChanged() const {
    return parentRelatedPropertyChanged() || dimensionsChanged();
}

bool EntityItemProperties::grabbingRelatedPropertyChanged() const {
    const GrabPropertyGroup& grabProperties = getGrab();
    return grabProperties.triggerableChanged() || grabProperties.grabbableChanged() ||
        grabProperties.grabFollowsControllerChanged() || grabProperties.grabKinematicChanged() ||
        grabProperties.equippableChanged() || grabProperties.equippableLeftPositionChanged() ||
        grabProperties.equippableRightPositionChanged() || grabProperties.equippableLeftRotationChanged() ||
        grabProperties.equippableRightRotationChanged() || grabProperties.equippableIndicatorURLChanged() ||
        grabProperties.equippableIndicatorScaleChanged() || grabProperties.equippableIndicatorOffsetChanged();
}

// Checking Certifiable Properties
#define ADD_STRING_PROPERTY(n, N) if (!get##N().isEmpty()) json[#n] = get##N()
#define ADD_ENUM_PROPERTY(n, N) json[#n] = get##N##AsString()
#define ADD_INT_PROPERTY(n, N) if (get##N() != 0) json[#n] = (get##N() == (quint32) -1) ? -1.0 : ((double) get##N())
QByteArray EntityItemProperties::getStaticCertificateJSON() const {
    // Produce a compact json of every non-default static certificate property, with the property names in alphabetical order.
    // The static certificate properties include all an only those properties that cannot be changed without altering the identity
    // of the entity as reviewed during the certification submission.

    QJsonObject json;

    quint32 staticCertificateVersion = getStaticCertificateVersion();

    if (!getAnimation().getURL().isEmpty()) {
        json["animationURL"] = getAnimation().getURL();
    }
    if (staticCertificateVersion >= 3) {
        ADD_STRING_PROPERTY(certificateType, CertificateType);
    }
    ADD_STRING_PROPERTY(collisionSoundURL, CollisionSoundURL);
    ADD_STRING_PROPERTY(compoundShapeURL, CompoundShapeURL);
    ADD_INT_PROPERTY(editionNumber, EditionNumber);
    ADD_INT_PROPERTY(entityInstanceNumber, EntityInstanceNumber);
    ADD_STRING_PROPERTY(itemArtist, ItemArtist);
    ADD_STRING_PROPERTY(itemCategories, ItemCategories);
    ADD_STRING_PROPERTY(itemDescription, ItemDescription);
    ADD_STRING_PROPERTY(itemLicenseUrl, ItemLicense);
    ADD_STRING_PROPERTY(itemName, ItemName);
    ADD_INT_PROPERTY(limitedRun, LimitedRun);
    ADD_STRING_PROPERTY(marketplaceID, MarketplaceID);
    ADD_STRING_PROPERTY(modelURL, ModelURL);
    ADD_STRING_PROPERTY(script, Script);
    if (staticCertificateVersion >= 1) {
        ADD_STRING_PROPERTY(serverScripts, ServerScripts);
    }
    ADD_ENUM_PROPERTY(shapeType, ShapeType);
    ADD_INT_PROPERTY(staticCertificateVersion, StaticCertificateVersion);
    json["type"] = EntityTypes::getEntityTypeName(getType());

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}
QByteArray EntityItemProperties::getStaticCertificateHash() const {
    return QCryptographicHash::hash(getStaticCertificateJSON(), QCryptographicHash::Sha256);
}

// FIXME: This is largely copied from EntityItemProperties::verifyStaticCertificateProperties, which should be refactored to use this.
// I also don't like the nested-if style, but for this step I'm deliberately preserving the similarity.
bool EntityItemProperties::verifySignature(const QString& publicKey, const QByteArray& digestByteArray, const QByteArray& signatureByteArray) {

    if (digestByteArray.isEmpty()) {
        return false;
    }

    auto keyByteArray = publicKey.toUtf8();
    auto key = keyByteArray.constData();
    int keyLength = publicKey.length();

    BIO *bio = BIO_new_mem_buf((void*)key, keyLength);
    EVP_PKEY* evp_key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    if (evp_key) {
        EC_KEY* ec = EVP_PKEY_get1_EC_KEY(evp_key);
        if (ec) {
            const unsigned char* digest = reinterpret_cast<const unsigned char*>(digestByteArray.constData());
            int digestLength = digestByteArray.length();

            const unsigned char* signature = reinterpret_cast<const unsigned char*>(signatureByteArray.constData());
            int signatureLength = signatureByteArray.length();

            ERR_clear_error();
            // ECSDA verification prototype: note that type is currently ignored
            // int ECDSA_verify(int type, const unsigned char *dgst, int dgstlen,
            // const unsigned char *sig, int siglen, EC_KEY *eckey);
            int answer = ECDSA_verify(0,
                digest,
                digestLength,
                signature,
                signatureLength,
                ec);
            long error = ERR_get_error();
            if (error != 0 || answer == -1) {
                qCWarning(entities) << "ERROR while verifying signature!"
                    << "\nKey:" << publicKey << "\nutf8 Key Length:" << keyLength
                    << "\nDigest:" << digest << "\nDigest Length:" << digestLength
                    << "\nSignature:" << signature << "\nSignature Length:" << signatureLength;
                while (error != 0) {
                    const char* error_str = ERR_error_string(error, NULL);
                    qCWarning(entities) << "EC error:" << error_str;
                    error = ERR_get_error();
                }
            }
            EC_KEY_free(ec);
            if (bio) {
                BIO_free(bio);
            }
            if (evp_key) {
                EVP_PKEY_free(evp_key);
            }
            return (answer == 1);
        } else {
            if (bio) {
                BIO_free(bio);
            }
            if (evp_key) {
                EVP_PKEY_free(evp_key);
            }
            long error = ERR_get_error();
            const char* error_str = ERR_error_string(error, NULL);
            qCWarning(entities) << "Failed to verify signature! key" << publicKey << " EC key error:" << error_str;
            return false;
        }
    } else {
        if (bio) {
            BIO_free(bio);
        }
        long error = ERR_get_error();
        const char* error_str = ERR_error_string(error, NULL);
        qCWarning(entities) << "Failed to verify signature! key" << publicKey << " EC PEM error:" << error_str;
        return false;
    }
}

bool EntityItemProperties::verifyStaticCertificateProperties() {
    // True IFF a non-empty certificateID matches the static certificate json.
    // I.e., if we can verify that the certificateID was produced by Vircadia signing the static certificate hash.
    return verifySignature(EntityItem::_marketplacePublicKey, getStaticCertificateHash(), QByteArray::fromBase64(getCertificateID().toUtf8()));
}

void EntityItemProperties::convertToCloneProperties(const EntityItemID& entityIDToClone) {
    setName(getName() + "-clone-" + entityIDToClone.toString());
    setLocked(false);
    setParentID(QUuid());
    setParentJointIndex(-1);
    setLifetime(getCloneLifetime());
    setDynamic(getCloneDynamic());
    if (getEntityHostType() != entity::HostType::LOCAL) {
        setEntityHostType(getCloneAvatarEntity() ? entity::HostType::AVATAR : entity::HostType::DOMAIN);
    } else {
        // Local Entities clone as local entities
        setEntityHostType(entity::HostType::LOCAL);
        setCollisionless(true);
    }
    uint64_t now = usecTimestampNow();
    setCreated(now);
    setLastEdited(now);
    setCloneable(ENTITY_ITEM_DEFAULT_CLONEABLE);
    setCloneLifetime(ENTITY_ITEM_DEFAULT_CLONE_LIFETIME);
    setCloneLimit(ENTITY_ITEM_DEFAULT_CLONE_LIMIT);
    setCloneDynamic(ENTITY_ITEM_DEFAULT_CLONE_DYNAMIC);
    setCloneAvatarEntity(ENTITY_ITEM_DEFAULT_CLONE_AVATAR_ENTITY);
}

bool EntityItemProperties::blobToProperties(ScriptEngine& scriptEngine, const QByteArray& blob, EntityItemProperties& properties) {
    // DANGER: this method is NOT efficient.
    // begin recipe for converting unfortunately-formatted-binary-blob to EntityItemProperties
    QJsonDocument jsonProperties = QJsonDocument::fromBinaryData(blob);
    if (jsonProperties.isEmpty() || jsonProperties.isNull() || !jsonProperties.isObject() || jsonProperties.object().isEmpty()) {
        qCDebug(entities) << "bad avatarEntityData json" << QString(blob.toHex());
        return false;
    }
    QVariant variant = jsonProperties.toVariant();
    QVariantMap variantMap = variant.toMap();
    ScriptValuePointer scriptValue = variantMapToScriptValue(variantMap, scriptEngine);
    EntityItemPropertiesFromScriptValueIgnoreReadOnly(scriptValue, properties);
    // end recipe
    return true;
}

void EntityItemProperties::propertiesToBlob(ScriptEngine& scriptEngine, const QUuid& myAvatarID,
            const EntityItemProperties& properties, QByteArray& blob, bool allProperties) {
    // DANGER: this method is NOT efficient.
    // begin recipe for extracting unfortunately-formatted-binary-blob from EntityItem
    ScriptValuePointer scriptValue = allProperties
        ? EntityItemPropertiesToScriptValue(&scriptEngine, properties)
        : EntityItemNonDefaultPropertiesToScriptValue(&scriptEngine, properties);
    QVariant variantProperties = scriptValue->toVariant();
    QJsonDocument jsonProperties = QJsonDocument::fromVariant(variantProperties);
    // the ID of the parent/avatar changes from session to session.  use a special UUID to indicate the avatar
    QJsonObject jsonObject = jsonProperties.object();
    if (jsonObject.contains("parentID")) {
        if (QUuid(jsonObject["parentID"].toString()) == myAvatarID) {
            jsonObject["parentID"] = AVATAR_SELF_ID.toString();
        }
    }
    jsonProperties = QJsonDocument(jsonObject);
    blob = jsonProperties.toBinaryData();
    // end recipe
}

QDebug& operator<<(QDebug& dbg, const EntityPropertyFlags& f) {
    QString result = "[ ";

    for (int i = 0; i < PROP_AFTER_LAST_ITEM; i++) {
        auto prop = EntityPropertyList(i);
        if (f.getHasProperty(prop)) {
            result = result + _enumsToPropertyStrings[prop] + " ";
        }
    }

    result += "]";
    dbg.nospace() << result;
    return dbg;
}
