//
//  BloomPropertyGroup.h
//  libraries/entities/src
//
//  Created by Sam Gondelman on 8/7/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_BloomPropertyGroup_h
#define hifi_BloomPropertyGroup_h

#include <stdint.h>
#include <glm/glm.hpp>

#include <QtCore/QSharedPointer>

#include "PropertyGroup.h"
#include "EntityItemPropertiesMacros.h"

class EntityItemProperties;
class EncodeBitstreamParams;
class OctreePacketData;
class EntityTreeElementExtraEncodeData;
class ReadBitstreamToTreeParams;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

static const float INITIAL_BLOOM_INTENSITY { 0.25f };
static const float INITIAL_BLOOM_THRESHOLD { 0.7f };
static const float INITIAL_BLOOM_SIZE { 0.9f };

/*@jsdoc
 * Bloom is defined by the following properties:
 * @typedef {object} Entities.Bloom
 * @property {number} bloomIntensity=0.25 - The intensity of the bloom effect.
 * @property {number} bloomThreshold=0.7 - The threshold for the bloom effect.
 * @property {number} bloomSize=0.9 - The size of the bloom effect.
 */
class BloomPropertyGroup : public PropertyGroup {
public:
    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                   ScriptEngine* engine, bool skipDefaults,
                                   EntityItemProperties& defaultEntityProperties) const override;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) override;

    void merge(const BloomPropertyGroup& other);

    virtual void debugDump() const override;
    virtual void listChangedProperties(QList<QString>& out) override;

    virtual bool appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const override;

    virtual bool decodeFromEditPacket(EntityPropertyFlags& propertyFlags,
                                      const unsigned char*& dataAt, int& processedBytes) override;
    virtual void markAllChanged() override;
    virtual EntityPropertyFlags getChangedProperties() const override;

    // EntityItem related helpers
    // methods for getting/setting all properties of an entity
    virtual void getProperties(EntityItemProperties& propertiesOut) const override;

    /// returns true if something changed
    virtual bool setProperties(const EntityItemProperties& properties) override;

    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const override;

    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const override;

    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) override;

    DEFINE_PROPERTY(PROP_BLOOM_INTENSITY, BloomIntensity, bloomIntensity, float, INITIAL_BLOOM_INTENSITY);
    DEFINE_PROPERTY(PROP_BLOOM_THRESHOLD, BloomThreshold, bloomThreshold, float, INITIAL_BLOOM_THRESHOLD);
    DEFINE_PROPERTY(PROP_BLOOM_SIZE, BloomSize, bloomSize, float, INITIAL_BLOOM_SIZE);

};

#endif // hifi_BloomPropertyGroup_h
