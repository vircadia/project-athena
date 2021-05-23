//
//  PropertyGroup.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PropertyGroup_h
#define hifi_PropertyGroup_h

#include <QtCore/QSharedPointer>

#include <OctreeElement.h>

#include "EntityPropertyFlags.h"


class EntityItemProperties;
class EncodeBitstreamParams;
class OctreePacketData;
class EntityTreeElementExtraEncodeData;
class ReadBitstreamToTreeParams;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;
using EntityTreeElementExtraEncodeDataPointer = std::shared_ptr<EntityTreeElementExtraEncodeData>;


class PropertyGroup {
public:
    virtual ~PropertyGroup() = default;

    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties, ScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const = 0;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) = 0;
    virtual void debugDump() const { }
    virtual void listChangedProperties(QList<QString>& out) { }

    virtual bool appendToEditPacket(OctreePacketData* packetData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const = 0;

    virtual bool decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) = 0;
    virtual void markAllChanged() = 0;
    virtual EntityPropertyFlags getChangedProperties() const = 0;

    // EntityItem related helpers
    // methods for getting/setting all properties of an entity
    virtual void getProperties(EntityItemProperties& propertiesOut) const = 0;
    
    /// returns true if something changed
    virtual bool setProperties(const EntityItemProperties& properties) = 0;

    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const = 0;
        
    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params, 
                                    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const = 0;

    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) = 0;
};

#endif // hifi_PropertyGroup_h
