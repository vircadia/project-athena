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

#ifndef hifi_HazePropertyGroup_h
#define hifi_HazePropertyGroup_h

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

static const float INITIAL_HAZE_RANGE{ 1000.0f };
static const glm::u8vec3 initialHazeGlareColor { 255, 229, 179 };
static const glm::u8vec3 initialHazeColor { 128, 154, 179 };
static const float INITIAL_HAZE_GLARE_ANGLE{ 20.0f };

static const float INITIAL_HAZE_BASE_REFERENCE{ 0.0f };
static const float INITIAL_HAZE_HEIGHT{ 200.0f };

static const float INITIAL_HAZE_BACKGROUND_BLEND{ 0.0f };

static const float INITIAL_KEY_LIGHT_RANGE{ 1000.0f };
static const float INITIAL_KEY_LIGHT_ALTITUDE{ 200.0f };

// FIXME: Document hazeAttenuationKeyLight, hazeKeyLightRange, and hazeKeyLightAltitude once they're working and are provided 
// in the Create app's UI.
/*@jsdoc
 * Haze is defined by the following properties:
 * @typedef {object} Entities.Haze
 *
 * @property {number} hazeRange=1000 - The horizontal distance at which visibility is reduced to 95%; i.e., 95% of each pixel's 
 *     color is haze.
 * @property {Color} hazeColor=128,154,179 - The color of the haze when looking away from the key light.
 * @property {boolean} hazeEnableGlare=false - <code>true</code> if the haze is colored with glare from the key light, 
 *     <code>false</code> if it isn't. If <code>true</code>, then <code>hazeGlareColor</code> and <code>hazeGlareAngle</code> 
 *     are used.
 * @property {Color} hazeGlareColor=255,299,179 - The color of the haze when looking towards the key light.
 * @property {number} hazeGlareAngle=20 - The angle in degrees across the circle around the key light that the glare color and 
 *     haze color are blended 50/50.
 *
 * @property {boolean} hazeAltitudeEffect=false - <code>true</code> if haze decreases with altitude as defined by the 
 *     entity's local coordinate system, <code>false</code> if it doesn't. If <code>true</code>, then <code>hazeBaseRef</code> 
 *     and <code>hazeCeiling</code> are used.
 * @property {number} hazeBaseRef=0 - The y-axis value in the entity's local coordinate system at which the haze density starts 
 *     reducing with altitude.
 * @property {number} hazeCeiling=200 - The y-axis value in the entity's local coordinate system at which the haze density has 
 *     reduced to 5%.
 *
 * @property {number} hazeBackgroundBlend=0 - The proportion of the skybox image to show through the haze: <code>0.0</code> 
 *     displays no skybox image; <code>1.0</code> displays no haze.
 *
 * @property {boolean} hazeAttenuateKeyLight=false - <code>true</code> if the haze attenuates the key light, <code>false</code> 
 *     if it doesn't. If <code>true</code>, then <code>hazeKeyLightRange</code> and <code>hazeKeyLightAltitude</code> are used.
 * @property {number} hazeKeyLightRange=1000 -  The distance at which the haze attenuates the key light by 95%.
 * @property {number} hazeKeyLightAltitude=200 - The altitude at which the haze starts attenuating the key light (i.e., the 
 *     altitude at which the distance starts being calculated).
 */
class HazePropertyGroup : public PropertyGroup {
public:
    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                   ScriptEngine* engine, bool skipDefaults,
                                   EntityItemProperties& defaultEntityProperties) const override;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) override;

    void merge(const HazePropertyGroup& other);

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

    // Range only parameters
    DEFINE_PROPERTY(PROP_HAZE_RANGE, HazeRange, hazeRange, float, INITIAL_HAZE_RANGE);
    DEFINE_PROPERTY_REF(PROP_HAZE_COLOR, HazeColor, hazeColor, glm::u8vec3, initialHazeColor);
    DEFINE_PROPERTY_REF(PROP_HAZE_GLARE_COLOR, HazeGlareColor, hazeGlareColor, glm::u8vec3, initialHazeGlareColor);
    DEFINE_PROPERTY(PROP_HAZE_ENABLE_GLARE, HazeEnableGlare, hazeEnableGlare, bool, false);
    DEFINE_PROPERTY_REF(PROP_HAZE_GLARE_ANGLE, HazeGlareAngle, hazeGlareAngle, float, INITIAL_HAZE_GLARE_ANGLE);

    // Altitude parameters
    DEFINE_PROPERTY(PROP_HAZE_ALTITUDE_EFFECT, HazeAltitudeEffect, hazeAltitudeEffect, bool, false);
    DEFINE_PROPERTY_REF(PROP_HAZE_CEILING, HazeCeiling, hazeCeiling, float, INITIAL_HAZE_BASE_REFERENCE + INITIAL_HAZE_HEIGHT);
    DEFINE_PROPERTY_REF(PROP_HAZE_BASE_REF, HazeBaseRef, hazeBaseRef, float, INITIAL_HAZE_BASE_REFERENCE);

    // Background (skybox) blend value
    DEFINE_PROPERTY_REF(PROP_HAZE_BACKGROUND_BLEND, HazeBackgroundBlend, hazeBackgroundBlend, float, INITIAL_HAZE_BACKGROUND_BLEND);

    // hazeDirectional light attenuation
    DEFINE_PROPERTY(PROP_HAZE_ATTENUATE_KEYLIGHT, HazeAttenuateKeyLight, hazeAttenuateKeyLight, bool, false);
    DEFINE_PROPERTY_REF(PROP_HAZE_KEYLIGHT_RANGE, HazeKeyLightRange, hazeKeyLightRange, float, INITIAL_KEY_LIGHT_RANGE);
    DEFINE_PROPERTY_REF(PROP_HAZE_KEYLIGHT_ALTITUDE, HazeKeyLightAltitude, hazeKeyLightAltitude, float, INITIAL_KEY_LIGHT_ALTITUDE);
};

#endif // hifi_HazePropertyGroup_h
