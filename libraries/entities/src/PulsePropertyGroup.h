//
//  PulsePropertyGroup.h
//
//  Created by Sam Gondelman on 1/15/19
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PulsePropertyGroup_h
#define hifi_PulsePropertyGroup_h

#include <stdint.h>

#include <QtCore/QSharedPointer>

#include <PulseMode.h>

#include "PropertyGroup.h"
#include "EntityItemPropertiesMacros.h"

class EntityItemProperties;
class EncodeBitstreamParams;
class OctreePacketData;
class ReadBitstreamToTreeParams;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * A color and alpha pulse that an entity may have.
 * @typedef {object} Entities.Pulse
 * @property {number} min=0 - The minimum value of the pulse multiplier.
 * @property {number} max=1 - The maximum value of the pulse multiplier.
 * @property {number} period=1 - The duration of the color and alpha pulse, in seconds. A pulse multiplier value goes from
 *     <code>min</code> to <code>max</code>, then <code>max</code> to <code>min</code> in one period.
 * @property {Entities.PulseMode} colorMode="none" - If "in", the color is pulsed in phase with the pulse period; if "out"
 *     the color is pulsed out of phase with the pulse period.
 * @property {Entities.PulseMode} alphaMode="none" - If "in", the alpha is pulsed in phase with the pulse period; if "out"
 *     the alpha is pulsed out of phase with the pulse period.
 */
class PulsePropertyGroup : public PropertyGroup {
public:
    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                   ScriptEngine* engine, bool skipDefaults,
                                   EntityItemProperties& defaultEntityProperties) const override;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) override;

    void merge(const PulsePropertyGroup& other);

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

    // returns true if something changed
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

    bool operator==(const PulsePropertyGroup& a) const;
    bool operator!=(const PulsePropertyGroup& a) const { return !(*this == a); }

    DEFINE_PROPERTY(PROP_PULSE_MIN, Min, min, float, 0.0f);
    DEFINE_PROPERTY(PROP_PULSE_MAX, Max, max, float, 1.0f);
    DEFINE_PROPERTY(PROP_PULSE_PERIOD, Period, period, float, 1.0f);
    DEFINE_PROPERTY_REF_ENUM(PROP_PULSE_COLOR_MODE, ColorMode, colorMode, PulseMode, PulseMode::NONE);
    DEFINE_PROPERTY_REF_ENUM(PROP_PULSE_ALPHA_MODE, AlphaMode, alphaMode, PulseMode, PulseMode::NONE);
};

#endif // hifi_PulsePropertyGroup_h
