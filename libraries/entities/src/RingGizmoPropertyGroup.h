//
//  Created by Sam Gondelman on 1/22/19
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RingGizmoPropertyGroup_h
#define hifi_RingGizmoPropertyGroup_h

#include <stdint.h>

#include <QtCore/QSharedPointer>

#include "PropertyGroup.h"
#include "EntityItemPropertiesMacros.h"
#include "EntityItemPropertiesDefaults.h"

class EntityItemProperties;
class EncodeBitstreamParams;
class OctreePacketData;
class ReadBitstreamToTreeParams;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

using u8vec3Color = glm::u8vec3;

/*@jsdoc
 * A {@link Entities.EntityProperties-Gizmo|ring Gizmo} entity is defined by the following properties:
 * @typedef {object} Entities.RingGizmo
 *
 * @property {number} startAngle=0 - The angle at which the ring starts, in degrees.
 * @property {number} endAngle=360 - The angle at which the ring ends, in degrees.
 * @property {number} innerRadius=0 - The inner radius of the ring as a fraction of the total radius, range <code>0.0</code> 
 *     &mdash; <code>1.0</code>.

 * @property {Color} innerStartColor=255,255,255 - The color at the inner start point of the ring.
 * @property {Color} innerEndColor=255,255,255 - The color at the inner end point of the ring.
 * @property {Color} outerStartColor=255,255,255 - The color at the outer start point of the ring.
 * @property {Color} outerEndColor=255,255,255 - The color at the outer end point of the ring.
 * @property {number} innerStartAlpha=1 - The opacity at the inner start point of the ring.
 * @property {number} innerEndAlpha=1 - The opacity at the inner end point of the ring.
 * @property {number} outerStartAlpha=1 - The opacity at the outer start point of the ring.
 * @property {number} outerEndAlpha=1 - The opacity at the outer end point of the ring.

 * @property {boolean} hasTickMarks=false - <code>true</code> to render tick marks, otherwise <code>false</code>.
 * @property {number} majorTickMarksAngle=0 - The angle between major tick marks, in degrees.
 * @property {number} minorTickMarksAngle=0 - The angle between minor tick marks, in degrees.
 * @property {number} majorTickMarksLength=0 - The length of the major tick marks as a fraction of the radius. A positive value 
 *     draws tick marks outwards from the inner radius; a negative value draws tick marks inwards from the outer radius.
 * @property {number} minorTickMarksLength=0 - The length of the minor tick marks, as a fraction of the radius. A positive 
 *     value draws tick marks outwards from the inner radius; a negative value draws tick marks inwards from the outer radius.
 * @property {Color} majorTickMarksColor=255,255,255 - The color of the major tick marks.
 * @property {Color} minorTickMarksColor=255,255,255 - The color of the minor tick marks.
 */

class RingGizmoPropertyGroup : public PropertyGroup {
public:
    // EntityItemProperty related helpers
    virtual void copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                   ScriptEngine* engine, bool skipDefaults,
                                   EntityItemProperties& defaultEntityProperties) const override;
    virtual void copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) override;

    void merge(const RingGizmoPropertyGroup& other);

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

    bool operator==(const RingGizmoPropertyGroup& a) const;
    bool operator!=(const RingGizmoPropertyGroup& a) const { return !(*this == a); }

    static const float MIN_ANGLE;
    static const float MAX_ANGLE;
    static const float MIN_ALPHA;
    static const float MAX_ALPHA;
    static const float MIN_RADIUS;
    static const float MAX_RADIUS;

    DEFINE_PROPERTY(PROP_START_ANGLE, StartAngle, startAngle, float, 0.0f);
    DEFINE_PROPERTY(PROP_END_ANGLE, EndAngle, endAngle, float, 360.0f);
    DEFINE_PROPERTY(PROP_INNER_RADIUS, InnerRadius, innerRadius, float, 0.0f);

    DEFINE_PROPERTY_REF(PROP_INNER_START_COLOR, InnerStartColor, innerStartColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
    DEFINE_PROPERTY_REF(PROP_INNER_END_COLOR, InnerEndColor, innerEndColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
    DEFINE_PROPERTY_REF(PROP_OUTER_START_COLOR, OuterStartColor, outerStartColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
    DEFINE_PROPERTY_REF(PROP_OUTER_END_COLOR, OuterEndColor, outerEndColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);

    DEFINE_PROPERTY(PROP_INNER_START_ALPHA, InnerStartAlpha, innerStartAlpha, float, ENTITY_ITEM_DEFAULT_ALPHA);
    DEFINE_PROPERTY(PROP_INNER_END_ALPHA, InnerEndAlpha, innerEndAlpha, float, ENTITY_ITEM_DEFAULT_ALPHA);
    DEFINE_PROPERTY(PROP_OUTER_START_ALPHA, OuterStartAlpha, outerStartAlpha, float, ENTITY_ITEM_DEFAULT_ALPHA);
    DEFINE_PROPERTY(PROP_OUTER_END_ALPHA, OuterEndAlpha, outerEndAlpha, float, ENTITY_ITEM_DEFAULT_ALPHA);

    DEFINE_PROPERTY(PROP_HAS_TICK_MARKS, HasTickMarks, hasTickMarks, bool, false);
    DEFINE_PROPERTY(PROP_MAJOR_TICK_MARKS_ANGLE, MajorTickMarksAngle, majorTickMarksAngle, float, 0.0f);
    DEFINE_PROPERTY(PROP_MINOR_TICK_MARKS_ANGLE, MinorTickMarksAngle, minorTickMarksAngle, float, 0.0f);
    DEFINE_PROPERTY(PROP_MAJOR_TICK_MARKS_LENGTH, MajorTickMarksLength, majorTickMarksLength, float, 0.0f);
    DEFINE_PROPERTY(PROP_MINOR_TICK_MARKS_LENGTH, MinorTickMarksLength, minorTickMarksLength, float, 0.0f);
    DEFINE_PROPERTY_REF(PROP_MAJOR_TICK_MARKS_COLOR, MajorTickMarksColor, majorTickMarksColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
    DEFINE_PROPERTY_REF(PROP_MINOR_TICK_MARKS_COLOR, MinorTickMarksColor, minorTickMarksColor, u8vec3Color, ENTITY_ITEM_DEFAULT_COLOR);
};

#endif // hifi_RingGizmoPropertyGroup_h
