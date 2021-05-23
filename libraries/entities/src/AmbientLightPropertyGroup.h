//
//  AmbientLightPropertyGroup.h
//  libraries/entities/src
//
//  Created by Nissim Hadar on 2017/12/24.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#ifndef hifi_AmbientLightPropertyGroup_h
#define hifi_AmbientLightPropertyGroup_h

#include <stdint.h>

#include <glm/glm.hpp>

#include <QtCore/QSharedPointer>
#include "EntityItemPropertiesMacros.h"
#include "PropertyGroup.h"

class EntityItemProperties;
class EncodeBitstreamParams;
class OctreePacketData;
class EntityTreeElementExtraEncodeData;
class ReadBitstreamToTreeParams;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * Ambient light is defined by the following properties:
 * @typedef {object} Entities.AmbientLight
 * @property {number} ambientIntensity=0.5 - The intensity of the light.
 * @property {string} ambientURL="" - A cube map image that defines the color of the light coming from each direction. If 
 *     <code>""</code> then the entity's {@link Entities.Skybox|Skybox} <code>url</code> property value is used, unless that also is <code>""</code> in which 
 *     case the entity's <code>ambientLightMode</code> property is set to <code>"inherit"</code>.
 */
class AmbientLightPropertyGroup : public PropertyGroup {
public:
    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                   ScriptEngine* engine, bool skipDefaults,
                                   EntityItemProperties& defaultEntityProperties) const override;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) override;

    void merge(const AmbientLightPropertyGroup& other);

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

    static const float DEFAULT_AMBIENT_LIGHT_INTENSITY;

    DEFINE_PROPERTY(PROP_AMBIENT_LIGHT_INTENSITY, AmbientIntensity, ambientIntensity, float, DEFAULT_AMBIENT_LIGHT_INTENSITY);
    DEFINE_PROPERTY_REF(PROP_AMBIENT_LIGHT_URL, AmbientURL, ambientURL, QString, "");
};

#endif // hifi_AmbientLightPropertyGroup_h
