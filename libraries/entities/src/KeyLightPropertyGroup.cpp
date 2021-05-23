//
//  KeyLightPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Sam Gateau on 2015/10/23.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "KeyLightPropertyGroup.h"

#include <QJsonDocument>
#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

const glm::u8vec3 KeyLightPropertyGroup::DEFAULT_KEYLIGHT_COLOR = { 255, 255, 255 };
const float KeyLightPropertyGroup::DEFAULT_KEYLIGHT_INTENSITY = 1.0f;
const float KeyLightPropertyGroup::DEFAULT_KEYLIGHT_AMBIENT_INTENSITY = 0.5f;
const glm::vec3 KeyLightPropertyGroup::DEFAULT_KEYLIGHT_DIRECTION = { 0.0f, -1.0f, 0.0f };
const bool KeyLightPropertyGroup::DEFAULT_KEYLIGHT_CAST_SHADOWS { false };
const float KeyLightPropertyGroup::DEFAULT_KEYLIGHT_SHADOW_BIAS { 0.5f };
const float KeyLightPropertyGroup::DEFAULT_KEYLIGHT_SHADOW_MAX_DISTANCE { 40.0f };

void KeyLightPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, 
    ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
        
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_KEYLIGHT_COLOR, KeyLight, keyLight, Color, color, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_KEYLIGHT_INTENSITY, KeyLight, keyLight, Intensity, intensity);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_KEYLIGHT_DIRECTION, KeyLight, keyLight, Direction, direction);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_KEYLIGHT_CAST_SHADOW, KeyLight, keyLight, CastShadows, castShadows);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_KEYLIGHT_SHADOW_BIAS, KeyLight, keyLight, ShadowBias, shadowBias);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, KeyLight, keyLight, ShadowMaxDistance, shadowMaxDistance);
}

void KeyLightPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, color, u8vec3Color, setColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, intensity, float, setIntensity);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, direction, vec3, setDirection);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, castShadows, bool, setCastShadows);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, shadowBias, float, setShadowBias);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(keyLight, shadowMaxDistance, float, setShadowMaxDistance);

    // legacy property support
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(keyLightColor, u8vec3Color, setColor, getColor);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(keyLightIntensity, float, setIntensity, getIntensity);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(keyLightDirection, vec3, setDirection, getDirection);
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(keyLightCastShadows, bool, setCastShadows, getCastShadows);
}

void KeyLightPropertyGroup::merge(const KeyLightPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(color);
    COPY_PROPERTY_IF_CHANGED(intensity);
    COPY_PROPERTY_IF_CHANGED(direction);
    COPY_PROPERTY_IF_CHANGED(castShadows);
    COPY_PROPERTY_IF_CHANGED(shadowBias);
    COPY_PROPERTY_IF_CHANGED(shadowMaxDistance);
}

void KeyLightPropertyGroup::debugDump() const {
    qCDebug(entities) << "   KeyLightPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "                   color:" << getColor();
    qCDebug(entities) << "               intensity:" << getIntensity();
    qCDebug(entities) << "               direction:" << getDirection();
    qCDebug(entities) << "             castShadows:" << getCastShadows();
    qCDebug(entities) << "              shadowBias:" << getShadowBias();
    qCDebug(entities) << "       shadowMaxDistance:" << getShadowMaxDistance();
}

void KeyLightPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (colorChanged()) {
        out << "keyLight-color";
    }
    if (intensityChanged()) {
        out << "keyLight-intensity";
    }
    if (directionChanged()) {
        out << "keyLight-direction";
    }
    if (castShadowsChanged()) {
        out << "keyLight-castShadows";
    }
    if (shadowBiasChanged()) {
        out << "keyLight-shadowBias";
    }
    if (shadowMaxDistanceChanged()) {
        out << "keyLight-shadowMaxDistance";
    }
}

bool KeyLightPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
    EntityPropertyFlags& requestedProperties,
    EntityPropertyFlags& propertyFlags,
    EntityPropertyFlags& propertiesDidntFit,
    int& propertyCount, 
    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;
    
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_COLOR, getColor());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_INTENSITY, getIntensity());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_DIRECTION, getDirection());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_CAST_SHADOW, getCastShadows());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_BIAS, getShadowBias());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, getShadowMaxDistance());

    return true;
}

bool KeyLightPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt, 
                                                 int& processedBytes) {
        
    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;
    
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_COLOR, u8vec3Color, setColor);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_INTENSITY, float, setIntensity);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_DIRECTION, glm::vec3, setDirection);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_CAST_SHADOW, bool, setCastShadows);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_BIAS, float, setShadowBias);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, float, setShadowMaxDistance);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_COLOR, Color);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_INTENSITY, Intensity);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_DIRECTION, Direction);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_CAST_SHADOW, CastShadows);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_SHADOW_BIAS, ShadowBias);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, ShadowMaxDistance);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void KeyLightPropertyGroup::markAllChanged() {
    _colorChanged = true;
    _intensityChanged = true;
    _directionChanged = true;
    _castShadowsChanged = true;
    _shadowBiasChanged = true;
    _shadowMaxDistanceChanged = true;
}

EntityPropertyFlags KeyLightPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;
 
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_COLOR, color);
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_INTENSITY, intensity);
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_DIRECTION, direction);
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_CAST_SHADOW, castShadows);
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_SHADOW_BIAS, shadowBias);
    CHECK_PROPERTY_CHANGE(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, shadowMaxDistance);

    return changedProperties;
}

void KeyLightPropertyGroup::getProperties(EntityItemProperties& properties) const {  
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, Color, getColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, Intensity, getIntensity);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, Direction, getDirection);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, CastShadows, getCastShadows);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, ShadowBias, getShadowBias);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(KeyLight, ShadowMaxDistance, getShadowMaxDistance);
}

bool KeyLightPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, Color, color, setColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, Intensity, intensity, setIntensity);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, Direction, direction, setDirection);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, CastShadows, castShadows, setCastShadows);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, ShadowBias, shadowBias, setShadowBias);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(KeyLight, ShadowMaxDistance, shadowMaxDistance, setShadowMaxDistance);

    return somethingChanged;
}

EntityPropertyFlags KeyLightPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_KEYLIGHT_COLOR;
    requestedProperties += PROP_KEYLIGHT_INTENSITY;
    requestedProperties += PROP_KEYLIGHT_DIRECTION;
    requestedProperties += PROP_KEYLIGHT_CAST_SHADOW;
    requestedProperties += PROP_KEYLIGHT_SHADOW_BIAS;
    requestedProperties += PROP_KEYLIGHT_SHADOW_MAX_DISTANCE;

    return requestedProperties;
}
    
void KeyLightPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params, 
                                EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                EntityPropertyFlags& requestedProperties,
                                EntityPropertyFlags& propertyFlags,
                                EntityPropertyFlags& propertiesDidntFit,
                                int& propertyCount, 
                                OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_COLOR, getColor());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_INTENSITY, getIntensity());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_DIRECTION, getDirection());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_CAST_SHADOW, getCastShadows());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_BIAS, getShadowBias());
    APPEND_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, getShadowMaxDistance());
}

int KeyLightPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
                                            ReadBitstreamToTreeParams& args,
                                            EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                            bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;
    
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_COLOR, u8vec3Color, setColor);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_INTENSITY, float, setIntensity);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_DIRECTION, glm::vec3, setDirection);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_CAST_SHADOW, bool, setCastShadows);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_BIAS, float, setShadowBias);
    READ_ENTITY_PROPERTY(PROP_KEYLIGHT_SHADOW_MAX_DISTANCE, float, setShadowMaxDistance);

    return bytesRead;
}
