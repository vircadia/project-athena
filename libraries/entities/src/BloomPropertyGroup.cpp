//
//  BloomPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Sam Gondelman on 8/7/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "BloomPropertyGroup.h"

#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

void BloomPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_BLOOM_INTENSITY, Bloom, bloom, BloomIntensity, bloomIntensity);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_BLOOM_THRESHOLD, Bloom, bloom, BloomThreshold, bloomThreshold);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_BLOOM_SIZE, Bloom, bloom, BloomSize, bloomSize);
}

void BloomPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(bloom, bloomIntensity, float, setBloomIntensity);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(bloom, bloomThreshold, float, setBloomThreshold);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(bloom, bloomSize, float, setBloomSize);
}

void BloomPropertyGroup::merge(const BloomPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(bloomIntensity);
    COPY_PROPERTY_IF_CHANGED(bloomThreshold);
    COPY_PROPERTY_IF_CHANGED(bloomSize);
}

void BloomPropertyGroup::debugDump() const {
    qCDebug(entities) << "   BloomPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "      _bloomIntensity:" << _bloomIntensity;
    qCDebug(entities) << "      _bloomThreshold:" << _bloomThreshold;
    qCDebug(entities) << "           _bloomSize:" << _bloomSize;
}

void BloomPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (bloomIntensityChanged()) {
        out << "bloom-bloomIntensity";
    }
    if (bloomThresholdChanged()) {
        out << "bloom-bloomThreshold";
    }
    if (bloomSizeChanged()) {
        out << "bloom-bloomSize";
    }
}

bool BloomPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                            EntityPropertyFlags& requestedProperties,
                                            EntityPropertyFlags& propertyFlags,
                                            EntityPropertyFlags& propertiesDidntFit,
                                            int& propertyCount, 
                                            OctreeElement::AppendState& appendState) const {
    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_BLOOM_INTENSITY, getBloomIntensity());
    APPEND_ENTITY_PROPERTY(PROP_BLOOM_THRESHOLD, getBloomThreshold());
    APPEND_ENTITY_PROPERTY(PROP_BLOOM_SIZE, getBloomSize());

    return true;
}

bool BloomPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) {
    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_BLOOM_INTENSITY, float, setBloomIntensity);
    READ_ENTITY_PROPERTY(PROP_BLOOM_THRESHOLD, float, setBloomThreshold);
    READ_ENTITY_PROPERTY(PROP_BLOOM_SIZE, float, setBloomSize);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_BLOOM_INTENSITY, BloomIntensity);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_BLOOM_THRESHOLD, BloomThreshold);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_BLOOM_SIZE, BloomSize);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void BloomPropertyGroup::markAllChanged() {
    _bloomIntensityChanged = true;
    _bloomThresholdChanged = true;
    _bloomSizeChanged = true;
}

EntityPropertyFlags BloomPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_BLOOM_INTENSITY, bloomIntensity);
    CHECK_PROPERTY_CHANGE(PROP_BLOOM_THRESHOLD, bloomThreshold);
    CHECK_PROPERTY_CHANGE(PROP_BLOOM_SIZE, bloomSize);

    return changedProperties;
}

void BloomPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Bloom, BloomIntensity, getBloomIntensity);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Bloom, BloomThreshold, getBloomThreshold);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Bloom, BloomSize, getBloomSize);
}

bool BloomPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Bloom, BloomIntensity, bloomIntensity, setBloomIntensity);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Bloom, BloomThreshold, bloomThreshold, setBloomThreshold);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Bloom, BloomSize, bloomSize, setBloomSize);

    return somethingChanged;
}

EntityPropertyFlags BloomPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_BLOOM_INTENSITY;
    requestedProperties += PROP_BLOOM_THRESHOLD;
    requestedProperties += PROP_BLOOM_SIZE;

    return requestedProperties;
}
    
void BloomPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                            EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                            EntityPropertyFlags& requestedProperties,
                                            EntityPropertyFlags& propertyFlags,
                                            EntityPropertyFlags& propertiesDidntFit,
                                            int& propertyCount, 
                                            OctreeElement::AppendState& appendState) const {
    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_BLOOM_INTENSITY, getBloomIntensity());
    APPEND_ENTITY_PROPERTY(PROP_BLOOM_THRESHOLD, getBloomThreshold());
    APPEND_ENTITY_PROPERTY(PROP_BLOOM_SIZE, getBloomSize());
}

int BloomPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                         ReadBitstreamToTreeParams& args,
                                                         EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                         bool& somethingChanged) {
    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_BLOOM_INTENSITY, float, setBloomIntensity);
    READ_ENTITY_PROPERTY(PROP_BLOOM_THRESHOLD, float, setBloomThreshold);
    READ_ENTITY_PROPERTY(PROP_BLOOM_SIZE, float, setBloomSize);

    return bytesRead;
}
