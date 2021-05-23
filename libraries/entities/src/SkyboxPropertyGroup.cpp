//
//  SkyboxPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SkyboxPropertyGroup.h"

#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

const glm::u8vec3 SkyboxPropertyGroup::DEFAULT_COLOR = { 0, 0, 0 };

void SkyboxPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_SKYBOX_COLOR, Skybox, skybox, Color, color, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_SKYBOX_URL, Skybox, skybox, URL, url);
}

void SkyboxPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(skybox, color, u8vec3Color, setColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(skybox, url, QString, setURL);
}

void SkyboxPropertyGroup::merge(const SkyboxPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(color);
    COPY_PROPERTY_IF_CHANGED(url);
}


void SkyboxPropertyGroup::debugDump() const {
    qCDebug(entities) << "   SkyboxPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "       Color:" << getColor() << " has changed:" << colorChanged();
    qCDebug(entities) << "       URL:" << getURL() << " has changed:" << urlChanged();
}

void SkyboxPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (colorChanged()) {
        out << "skybox-color";
    }
    if (urlChanged()) {
        out << "skybox-url";
    }
}

bool SkyboxPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_SKYBOX_COLOR, getColor());
    APPEND_ENTITY_PROPERTY(PROP_SKYBOX_URL, getURL());

    return true;
}


bool SkyboxPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) {

    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_SKYBOX_COLOR, u8vec3Color, setColor);
    READ_ENTITY_PROPERTY(PROP_SKYBOX_URL, QString, setURL);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_SKYBOX_COLOR, Color);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_SKYBOX_URL, URL);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void SkyboxPropertyGroup::markAllChanged() {
    _colorChanged = true;
    _urlChanged = true;
}

EntityPropertyFlags SkyboxPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;
    
    CHECK_PROPERTY_CHANGE(PROP_SKYBOX_COLOR, color);
    CHECK_PROPERTY_CHANGE(PROP_SKYBOX_URL, url);

    return changedProperties;
}

void SkyboxPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Skybox, Color, getColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Skybox, URL, getURL);
}

bool SkyboxPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Skybox, Color, color, setColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Skybox, URL, url, setURL);

    return somethingChanged;
}

EntityPropertyFlags SkyboxPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_SKYBOX_COLOR;
    requestedProperties += PROP_SKYBOX_URL;
    
    return requestedProperties;
}
    
void SkyboxPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params, 
                                EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                EntityPropertyFlags& requestedProperties,
                                EntityPropertyFlags& propertyFlags,
                                EntityPropertyFlags& propertiesDidntFit,
                                int& propertyCount, 
                                OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_SKYBOX_COLOR, getColor());
    APPEND_ENTITY_PROPERTY(PROP_SKYBOX_URL, getURL());
}

int SkyboxPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
                                            ReadBitstreamToTreeParams& args,
                                            EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                            bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_SKYBOX_COLOR, u8vec3Color, setColor);
    READ_ENTITY_PROPERTY(PROP_SKYBOX_URL, QString, setURL);

    return bytesRead;
}
