//
//  PulsePropertyGroup.cpp
//
//  Created by Sam Gondelman on 1/15/19
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PulsePropertyGroup.h"

#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

QHash<QString, PulseMode> stringToPulseModeLookup;

void addPulseMode(PulseMode mode) {
    stringToPulseModeLookup[PulseModeHelpers::getNameForPulseMode(mode)] = mode;
}

void buildStringToPulseModeLookup() {
    addPulseMode(PulseMode::NONE);
    addPulseMode(PulseMode::IN_PHASE);
    addPulseMode(PulseMode::OUT_PHASE);
}

QString PulsePropertyGroup::getColorModeAsString() const {
    return PulseModeHelpers::getNameForPulseMode(_colorMode);
}

void PulsePropertyGroup::setColorModeFromString(const QString& pulseMode) {
    if (stringToPulseModeLookup.empty()) {
        buildStringToPulseModeLookup();
    }
    auto pulseModeItr = stringToPulseModeLookup.find(pulseMode.toLower());
    if (pulseModeItr != stringToPulseModeLookup.end()) {
        _colorMode = pulseModeItr.value();
        _colorModeChanged = true;
    }
}

QString PulsePropertyGroup::getAlphaModeAsString() const {
    return PulseModeHelpers::getNameForPulseMode(_alphaMode);
}

void PulsePropertyGroup::setAlphaModeFromString(const QString& pulseMode) {
    if (stringToPulseModeLookup.empty()) {
        buildStringToPulseModeLookup();
    }
    auto pulseModeItr = stringToPulseModeLookup.find(pulseMode.toLower());
    if (pulseModeItr != stringToPulseModeLookup.end()) {
        _alphaMode = pulseModeItr.value();
        _alphaModeChanged = true;
    }
}

void PulsePropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                          ScriptEngine* engine, bool skipDefaults,
                                          EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_PULSE_MIN, Pulse, pulse, Min, min);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_PULSE_MAX, Pulse, pulse, Max, max);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_PULSE_PERIOD, Pulse, pulse, Period, period);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_PULSE_COLOR_MODE, Pulse, pulse, ColorMode, colorMode, getColorModeAsString);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_GETTER(PROP_PULSE_ALPHA_MODE, Pulse, pulse, AlphaMode, alphaMode, getAlphaModeAsString);
}

void PulsePropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(pulse, min, float, setMin);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(pulse, max, float, setMax);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(pulse, period, float, setPeriod);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE_ENUM(pulse, colorMode, ColorMode);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE_ENUM(pulse, alphaMode, AlphaMode);
}

void PulsePropertyGroup::merge(const PulsePropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(min);
    COPY_PROPERTY_IF_CHANGED(max);
    COPY_PROPERTY_IF_CHANGED(period);
    COPY_PROPERTY_IF_CHANGED(colorMode);
    COPY_PROPERTY_IF_CHANGED(alphaMode);
}

void PulsePropertyGroup::debugDump() const {
    qCDebug(entities) << "   PulsePropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "            _min:" << _min;
    qCDebug(entities) << "            _max:" << _max;
    qCDebug(entities) << "         _period:" << _period;
    qCDebug(entities) << "      _colorMode:" << getColorModeAsString();
    qCDebug(entities) << "      _alphaMode:" << getAlphaModeAsString();
}

void PulsePropertyGroup::listChangedProperties(QList<QString>& out) {
    if (minChanged()) {
        out << "pulse-min";
    }
    if (maxChanged()) {
        out << "pulse-max";
    }
    if (periodChanged()) {
        out << "pulse-period";
    }
    if (colorModeChanged()) {
        out << "pulse-colorMode";
    }
    if (alphaModeChanged()) {
        out << "pulse-alphaMode";
    }
}

bool PulsePropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                           EntityPropertyFlags& requestedProperties,
                                           EntityPropertyFlags& propertyFlags,
                                           EntityPropertyFlags& propertiesDidntFit,
                                           int& propertyCount,
                                           OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_PULSE_MIN, getMin());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_MAX, getMax());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_PERIOD, getPeriod());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_COLOR_MODE, (uint32_t)getColorMode());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_ALPHA_MODE, (uint32_t)getAlphaMode());

    return true;
}

bool PulsePropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags,
                                             const unsigned char*& dataAt , int& processedBytes) {

    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_PULSE_MIN, float, setMin);
    READ_ENTITY_PROPERTY(PROP_PULSE_MAX, float, setMax);
    READ_ENTITY_PROPERTY(PROP_PULSE_PERIOD, float, setPeriod);
    READ_ENTITY_PROPERTY(PROP_PULSE_COLOR_MODE, PulseMode, setColorMode);
    READ_ENTITY_PROPERTY(PROP_PULSE_ALPHA_MODE, PulseMode, setAlphaMode);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_PULSE_MIN, Min);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_PULSE_MAX, Max);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_PULSE_PERIOD, Period);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_PULSE_COLOR_MODE, ColorMode);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_PULSE_ALPHA_MODE, AlphaMode);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void PulsePropertyGroup::markAllChanged() {
    _minChanged = true;
    _maxChanged = true;
    _periodChanged = true;
    _colorModeChanged = true;
    _alphaModeChanged = true;
}

EntityPropertyFlags PulsePropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_PULSE_MIN, min);
    CHECK_PROPERTY_CHANGE(PROP_PULSE_MAX, max);
    CHECK_PROPERTY_CHANGE(PROP_PULSE_PERIOD, period);
    CHECK_PROPERTY_CHANGE(PROP_PULSE_COLOR_MODE, colorMode);
    CHECK_PROPERTY_CHANGE(PROP_PULSE_ALPHA_MODE, alphaMode);

    return changedProperties;
}

void PulsePropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Pulse, Min, getMin);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Pulse, Max, getMax);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Pulse, Period, getPeriod);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Pulse, ColorMode, getColorMode);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Pulse, AlphaMode, getAlphaMode);
}

bool PulsePropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Pulse, Min, min, setMin);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Pulse, Max, max, setMax);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Pulse, Period, period, setPeriod);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Pulse, ColorMode, colorMode, setColorMode);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Pulse, AlphaMode, alphaMode, setAlphaMode);

    return somethingChanged;
}

EntityPropertyFlags PulsePropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_PULSE_MIN;
    requestedProperties += PROP_PULSE_MAX;
    requestedProperties += PROP_PULSE_PERIOD;
    requestedProperties += PROP_PULSE_COLOR_MODE;
    requestedProperties += PROP_PULSE_ALPHA_MODE;

    return requestedProperties;
}

void PulsePropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                           EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                           EntityPropertyFlags& requestedProperties,
                                           EntityPropertyFlags& propertyFlags,
                                           EntityPropertyFlags& propertiesDidntFit,
                                           int& propertyCount,
                                           OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_PULSE_MIN, getMin());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_MAX, getMax());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_PERIOD, getPeriod());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_COLOR_MODE, (uint32_t)getColorMode());
    APPEND_ENTITY_PROPERTY(PROP_PULSE_ALPHA_MODE, (uint32_t)getAlphaMode());
}

int PulsePropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                        ReadBitstreamToTreeParams& args,
                                                        EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                        bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_PULSE_MIN, float, setMin);
    READ_ENTITY_PROPERTY(PROP_PULSE_MAX, float, setMax);
    READ_ENTITY_PROPERTY(PROP_PULSE_PERIOD, float, setPeriod);
    READ_ENTITY_PROPERTY(PROP_PULSE_COLOR_MODE, PulseMode, setColorMode);
    READ_ENTITY_PROPERTY(PROP_PULSE_ALPHA_MODE, PulseMode, setAlphaMode);

    return bytesRead;
}

bool PulsePropertyGroup::operator==(const PulsePropertyGroup& a) const {
    return (a._min == _min) &&
           (a._max == _max) &&
           (a._period == _period) &&
           (a._colorMode == _colorMode) &&
           (a._alphaMode == _alphaMode);
}