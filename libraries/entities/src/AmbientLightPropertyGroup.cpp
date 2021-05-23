//
//  AmbientLightPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Nissim Hadar on 2017/12/24.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AmbientLightPropertyGroup.h"

#include <QJsonDocument>
#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

const float AmbientLightPropertyGroup::DEFAULT_AMBIENT_LIGHT_INTENSITY = 0.5f;

void AmbientLightPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
    ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_AMBIENT_LIGHT_INTENSITY, AmbientLight, ambientLight, AmbientIntensity, ambientIntensity);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_AMBIENT_LIGHT_URL, AmbientLight, ambientLight, AmbientURL, ambientURL);
}

void AmbientLightPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ambientLight, ambientIntensity, float, setAmbientIntensity);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ambientLight, ambientURL, QString, setAmbientURL);
    
    // legacy property support
    COPY_PROPERTY_FROM_QSCRIPTVALUE_GETTER(ambientLightAmbientIntensity, float, setAmbientIntensity, getAmbientIntensity);
}

void AmbientLightPropertyGroup::merge(const AmbientLightPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(ambientIntensity);
    COPY_PROPERTY_IF_CHANGED(ambientURL);
}

void AmbientLightPropertyGroup::debugDump() const {
    qCDebug(entities) << "   AmbientLightPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "        ambientIntensity:" << getAmbientIntensity();
}

void AmbientLightPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (ambientIntensityChanged()) {
        out << "ambientLight-ambientIntensity";
    }
    if (ambientURLChanged()) {
        out << "ambientLight-ambientURL";
    }
}

bool AmbientLightPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;
    
    APPEND_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_INTENSITY, getAmbientIntensity());
    APPEND_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_URL, getAmbientURL());
    
    return true;
}

bool AmbientLightPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt,
    int& processedBytes) {
        
    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;
    
    READ_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_INTENSITY, float, setAmbientIntensity);
    READ_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_URL, QString, setAmbientURL);
    
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_AMBIENT_LIGHT_INTENSITY, AmbientIntensity);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_AMBIENT_LIGHT_URL, AmbientURL);
    
    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void AmbientLightPropertyGroup::markAllChanged() {
    _ambientIntensityChanged = true;
    _ambientURLChanged = true;
}

EntityPropertyFlags AmbientLightPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;
 
    CHECK_PROPERTY_CHANGE(PROP_AMBIENT_LIGHT_INTENSITY, ambientIntensity);
    CHECK_PROPERTY_CHANGE(PROP_AMBIENT_LIGHT_URL, ambientURL);
    
    return changedProperties;
}

void AmbientLightPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(AmbientLight, AmbientIntensity, getAmbientIntensity);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(AmbientLight, AmbientURL, getAmbientURL);
}

bool AmbientLightPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(AmbientLight, AmbientIntensity, ambientIntensity, setAmbientIntensity);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(AmbientLight, AmbientURL, ambientURL, setAmbientURL);

    return somethingChanged;
}

EntityPropertyFlags AmbientLightPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_AMBIENT_LIGHT_INTENSITY;
    requestedProperties += PROP_AMBIENT_LIGHT_URL;

    return requestedProperties;
}
    
void AmbientLightPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params, 
    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
    EntityPropertyFlags& requestedProperties,
    EntityPropertyFlags& propertyFlags,
    EntityPropertyFlags& propertiesDidntFit,
    int& propertyCount, 
    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_INTENSITY, getAmbientIntensity());
    APPEND_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_URL, getAmbientURL());
}

int AmbientLightPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
    ReadBitstreamToTreeParams& args,
    EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
    bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;
    
    READ_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_INTENSITY, float, setAmbientIntensity);
    READ_ENTITY_PROPERTY(PROP_AMBIENT_LIGHT_URL, QString, setAmbientURL);

    return bytesRead;
}
