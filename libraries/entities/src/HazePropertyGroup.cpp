//
//  HazePropertyGroup.h
//  libraries/entities/src
//
//  Created by Nissim hadar on 9/21/17.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "HazePropertyGroup.h"

#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

void HazePropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_RANGE, Haze, haze, HazeRange, hazeRange);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_HAZE_COLOR, Haze, haze, HazeColor, hazeColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_HAZE_GLARE_COLOR, Haze, haze, HazeGlareColor, hazeGlareColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_ENABLE_GLARE, Haze, haze, HazeEnableGlare, hazeEnableGlare);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_GLARE_ANGLE, Haze, haze, HazeGlareAngle, hazeGlareAngle);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_ALTITUDE_EFFECT, Haze, haze, HazeAltitudeEffect, hazeAltitudeEffect);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_CEILING, Haze, haze, HazeCeiling, hazeCeiling);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_BASE_REF, Haze, haze, HazeBaseRef, hazeBaseRef);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_BACKGROUND_BLEND, Haze, haze, HazeBackgroundBlend, hazeBackgroundBlend);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_ATTENUATE_KEYLIGHT, Haze, haze, HazeAttenuateKeyLight, hazeAttenuateKeyLight);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_KEYLIGHT_RANGE, Haze, haze, HazeKeyLightRange, hazeKeyLightRange);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAZE_KEYLIGHT_ALTITUDE, Haze, haze, HazeKeyLightAltitude, hazeKeyLightAltitude);
}

void HazePropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeRange, float, setHazeRange);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeColor, u8vec3Color, setHazeColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeGlareColor, u8vec3Color, setHazeGlareColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeEnableGlare, bool, setHazeEnableGlare);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeGlareAngle, float, setHazeGlareAngle);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeAltitudeEffect, bool, setHazeAltitudeEffect);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeCeiling, float, setHazeCeiling);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeBaseRef, float, setHazeBaseRef);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeBackgroundBlend, float, setHazeBackgroundBlend);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeAttenuateKeyLight, bool, setHazeAttenuateKeyLight);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeKeyLightRange, float, setHazeKeyLightRange);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(haze, hazeKeyLightAltitude, float, setHazeKeyLightAltitude);
}

void HazePropertyGroup::merge(const HazePropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(hazeRange);
    COPY_PROPERTY_IF_CHANGED(hazeColor);
    COPY_PROPERTY_IF_CHANGED(hazeGlareColor);
    COPY_PROPERTY_IF_CHANGED(hazeEnableGlare);
    COPY_PROPERTY_IF_CHANGED(hazeGlareAngle);

    COPY_PROPERTY_IF_CHANGED(hazeAltitudeEffect);
    COPY_PROPERTY_IF_CHANGED(hazeCeiling);
    COPY_PROPERTY_IF_CHANGED(hazeBaseRef);

    COPY_PROPERTY_IF_CHANGED(hazeBackgroundBlend);

    COPY_PROPERTY_IF_CHANGED(hazeAttenuateKeyLight);
    COPY_PROPERTY_IF_CHANGED(hazeKeyLightRange);
    COPY_PROPERTY_IF_CHANGED(hazeKeyLightAltitude);
}

void HazePropertyGroup::debugDump() const {
    qCDebug(entities) << "   HazePropertyGroup: ---------------------------------------------";

    qCDebug(entities) << "            _hazeRange:" << _hazeRange;
    qCDebug(entities) << "            _hazeColor:" << _hazeColor;
    qCDebug(entities) << "            _hazeGlareColor:" << _hazeGlareColor;
    qCDebug(entities) << "            _hazeEnableGlare:" << _hazeEnableGlare;
    qCDebug(entities) << "            _hazeGlareAngle:" << _hazeGlareAngle;

    qCDebug(entities) << "            _hazeAltitudeEffect:" << _hazeAltitudeEffect;
    qCDebug(entities) << "            _hazeCeiling:" << _hazeCeiling;
    qCDebug(entities) << "            _hazeBaseRef:" << _hazeBaseRef;

    qCDebug(entities) << "            _hazeBackgroundBlend:" << _hazeBackgroundBlend;

    qCDebug(entities) << "            _hazeAttenuateKeyLight:" << _hazeAttenuateKeyLight;
    qCDebug(entities) << "            _hazeKeyLightRange:" << _hazeKeyLightRange;
    qCDebug(entities) << "            _hazeKeyLightAltitude:" << _hazeKeyLightAltitude;
}

void HazePropertyGroup::listChangedProperties(QList<QString>& out) {
    if (hazeRangeChanged()) {
        out << "haze-hazeRange";
    }
    if (hazeColorChanged()) {
        out << "haze-hazeColor";
    }
    if (hazeGlareColorChanged()) {
        out << "haze-hazeGlareColor";
    }
    if (hazeEnableGlareChanged()) {
        out << "haze-hazeEnableGlare";
    }
    if (hazeGlareAngleChanged()) {
        out << "haze-hazeGlareAngle";
    }

    if (hazeAltitudeEffectChanged()) {
        out << "haze-hazeAltitudeEffect";
    }
    if (hazeCeilingChanged()) {
        out << "haze-hazeCeiling";
    }
    if (hazeBaseRefChanged()) {
        out << "haze-hazeBaseRef";
    }

    if (hazeBackgroundBlendChanged()) {
        out << "haze-hazeBackgroundBlend";
    }

    if (hazeAttenuateKeyLightChanged()) {
        out << "haze-hazeAttenuateKeyLight";
    }
    if (hazeKeyLightRangeChanged()) {
        out << "haze-hazeKeyLightRange";
    }
    if (hazeKeyLightAltitudeChanged()) {
        out << "haze-hazeKeyLightAltitude";
    }
}

bool HazePropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_HAZE_RANGE, getHazeRange());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_COLOR, getHazeColor());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_GLARE_COLOR, getHazeGlareColor());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_ENABLE_GLARE, getHazeEnableGlare());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_GLARE_ANGLE, getHazeGlareAngle());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_ALTITUDE_EFFECT, getHazeAltitudeEffect());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_CEILING, getHazeCeiling());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_BASE_REF, getHazeBaseRef());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_BACKGROUND_BLEND, getHazeBackgroundBlend());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_ATTENUATE_KEYLIGHT, getHazeAttenuateKeyLight());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_RANGE, getHazeKeyLightRange());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_ALTITUDE, getHazeKeyLightAltitude());

    return true;
}

bool HazePropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) {

    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_HAZE_RANGE, float, setHazeRange);
    READ_ENTITY_PROPERTY(PROP_HAZE_COLOR, u8vec3Color, setHazeColor);
    READ_ENTITY_PROPERTY(PROP_HAZE_GLARE_COLOR, u8vec3Color, setHazeGlareColor);
    READ_ENTITY_PROPERTY(PROP_HAZE_ENABLE_GLARE, bool, setHazeEnableGlare);
    READ_ENTITY_PROPERTY(PROP_HAZE_GLARE_ANGLE, float, setHazeGlareAngle);

    READ_ENTITY_PROPERTY(PROP_HAZE_ALTITUDE_EFFECT, bool, setHazeAltitudeEffect);
    READ_ENTITY_PROPERTY(PROP_HAZE_CEILING, float, setHazeCeiling);
    READ_ENTITY_PROPERTY(PROP_HAZE_BASE_REF, float, setHazeBaseRef);

    READ_ENTITY_PROPERTY(PROP_HAZE_BACKGROUND_BLEND, float, setHazeBackgroundBlend);

    READ_ENTITY_PROPERTY(PROP_HAZE_ATTENUATE_KEYLIGHT, bool, setHazeAttenuateKeyLight);
    READ_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_RANGE, float, setHazeKeyLightRange);
    READ_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_ALTITUDE, float, setHazeKeyLightAltitude);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_RANGE, HazeRange);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_COLOR, HazeColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_GLARE_COLOR, HazeGlareColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_ENABLE_GLARE, HazeEnableGlare);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_GLARE_ANGLE, HazeGlareAngle);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_ALTITUDE_EFFECT, HazeAltitudeEffect);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_CEILING, HazeCeiling);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_BASE_REF, HazeBaseRef);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_BACKGROUND_BLEND, HazeBackgroundBlend);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_ATTENUATE_KEYLIGHT, HazeAttenuateKeyLight);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_KEYLIGHT_RANGE, HazeKeyLightRange);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAZE_KEYLIGHT_ALTITUDE, HazeKeyLightAltitude);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void HazePropertyGroup::markAllChanged() {
    _hazeRangeChanged = true;
    _hazeColorChanged = true;
    _hazeGlareColorChanged = true;
    _hazeEnableGlareChanged = true;
    _hazeGlareAngleChanged = true;

    _hazeAltitudeEffectChanged = true;
    _hazeCeilingChanged = true;
    _hazeBaseRefChanged = true;

    _hazeBackgroundBlendChanged = true;

    _hazeAttenuateKeyLightChanged = true;
    _hazeKeyLightRangeChanged = true;
    _hazeKeyLightAltitudeChanged = true;
}

EntityPropertyFlags HazePropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_HAZE_RANGE, hazeRange);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_COLOR, hazeColor);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_GLARE_COLOR, hazeGlareColor);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_ENABLE_GLARE, hazeEnableGlare);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_GLARE_ANGLE, hazeGlareAngle);

    CHECK_PROPERTY_CHANGE(PROP_HAZE_ALTITUDE_EFFECT, hazeAltitudeEffect);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_CEILING, hazeCeiling);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_BASE_REF, hazeBaseRef);

    CHECK_PROPERTY_CHANGE(PROP_HAZE_BACKGROUND_BLEND, hazeBackgroundBlend);

    CHECK_PROPERTY_CHANGE(PROP_HAZE_ATTENUATE_KEYLIGHT, hazeAttenuateKeyLight);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_KEYLIGHT_RANGE, hazeKeyLightRange);
    CHECK_PROPERTY_CHANGE(PROP_HAZE_KEYLIGHT_ALTITUDE, hazeKeyLightAltitude);

    return changedProperties;
}

void HazePropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeRange, getHazeRange);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeColor, getHazeColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeGlareColor, getHazeGlareColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeEnableGlare, getHazeEnableGlare);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeGlareAngle, getHazeGlareAngle);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeAltitudeEffect, getHazeAltitudeEffect);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeCeiling, getHazeCeiling);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeBaseRef, getHazeBaseRef);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeBackgroundBlend, getHazeBackgroundBlend);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeAttenuateKeyLight, getHazeAttenuateKeyLight);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeKeyLightRange, getHazeKeyLightRange);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Haze, HazeKeyLightAltitude, getHazeKeyLightAltitude);
}

bool HazePropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeRange, hazeRange, setHazeRange);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeColor, hazeColor, setHazeColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeGlareColor, hazeGlareColor, setHazeGlareColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeEnableGlare, hazeEnableGlare, setHazeEnableGlare);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeGlareAngle, hazeGlareAngle, setHazeGlareAngle);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeAltitudeEffect, hazeAltitudeEffect, setHazeAltitudeEffect);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeCeiling, hazeCeiling, setHazeCeiling);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeBaseRef, hazeBaseRef, setHazeBaseRef);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeBackgroundBlend, hazeBackgroundBlend, setHazeBackgroundBlend);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeAttenuateKeyLight, hazeAttenuateKeyLight, setHazeAttenuateKeyLight);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeKeyLightRange, hazeKeyLightRange, setHazeKeyLightRange);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Haze, HazeKeyLightAltitude, hazeKeyLightAltitude, setHazeKeyLightAltitude);

    return somethingChanged;
}

EntityPropertyFlags HazePropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_HAZE_RANGE;
    requestedProperties += PROP_HAZE_COLOR;
    requestedProperties += PROP_HAZE_GLARE_COLOR;
    requestedProperties += PROP_HAZE_ENABLE_GLARE;
    requestedProperties += PROP_HAZE_GLARE_ANGLE;

    requestedProperties += PROP_HAZE_ALTITUDE_EFFECT;
    requestedProperties += PROP_HAZE_CEILING;
    requestedProperties += PROP_HAZE_BASE_REF;

    requestedProperties += PROP_HAZE_BACKGROUND_BLEND;

    requestedProperties += PROP_HAZE_ATTENUATE_KEYLIGHT;
    requestedProperties += PROP_HAZE_KEYLIGHT_RANGE;
    requestedProperties += PROP_HAZE_KEYLIGHT_ALTITUDE;

    return requestedProperties;
}
    
void HazePropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                EntityPropertyFlags& requestedProperties,
                                EntityPropertyFlags& propertyFlags,
                                EntityPropertyFlags& propertiesDidntFit,
                                int& propertyCount, 
                                OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_HAZE_RANGE, getHazeRange());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_COLOR, getHazeColor());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_GLARE_COLOR, getHazeGlareColor());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_ENABLE_GLARE, getHazeEnableGlare());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_GLARE_ANGLE, getHazeGlareAngle());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_ALTITUDE_EFFECT, getHazeAltitudeEffect());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_CEILING, getHazeCeiling());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_BASE_REF, getHazeBaseRef());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_BACKGROUND_BLEND, getHazeBackgroundBlend());

    APPEND_ENTITY_PROPERTY(PROP_HAZE_ATTENUATE_KEYLIGHT, getHazeAttenuateKeyLight());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_RANGE, getHazeKeyLightRange());
    APPEND_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_ALTITUDE, getHazeKeyLightAltitude());
}

int HazePropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                            ReadBitstreamToTreeParams& args,
                                            EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                            bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_HAZE_RANGE, float, setHazeRange);
    READ_ENTITY_PROPERTY(PROP_HAZE_COLOR, u8vec3Color, setHazeColor);
    READ_ENTITY_PROPERTY(PROP_HAZE_GLARE_COLOR, u8vec3Color, setHazeGlareColor);
    READ_ENTITY_PROPERTY(PROP_HAZE_ENABLE_GLARE, bool, setHazeEnableGlare);
    READ_ENTITY_PROPERTY(PROP_HAZE_GLARE_ANGLE, float, setHazeGlareAngle);

    READ_ENTITY_PROPERTY(PROP_HAZE_ALTITUDE_EFFECT, bool, setHazeAltitudeEffect);
    READ_ENTITY_PROPERTY(PROP_HAZE_CEILING, float, setHazeCeiling);
    READ_ENTITY_PROPERTY(PROP_HAZE_BASE_REF, float, setHazeBaseRef);

    READ_ENTITY_PROPERTY(PROP_HAZE_BACKGROUND_BLEND, float, setHazeBackgroundBlend);

    READ_ENTITY_PROPERTY(PROP_HAZE_ATTENUATE_KEYLIGHT, bool, setHazeAttenuateKeyLight);
    READ_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_RANGE, float, setHazeKeyLightRange);
    READ_ENTITY_PROPERTY(PROP_HAZE_KEYLIGHT_ALTITUDE, float, setHazeKeyLightAltitude);

    return bytesRead;
}
