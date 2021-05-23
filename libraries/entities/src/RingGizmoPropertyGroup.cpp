//
//  Created by Sam Gondelman on 1/22/19
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RingGizmoPropertyGroup.h"

#include <OctreePacketData.h>

#include "EntityItemProperties.h"
#include "EntityItemPropertiesMacros.h"

const float RingGizmoPropertyGroup::MIN_ANGLE = 0.0f;
const float RingGizmoPropertyGroup::MAX_ANGLE = 360.0f;
const float RingGizmoPropertyGroup::MIN_ALPHA = 0.0f;
const float RingGizmoPropertyGroup::MAX_ALPHA = 1.0f;
const float RingGizmoPropertyGroup::MIN_RADIUS = 0.0f;
const float RingGizmoPropertyGroup::MAX_RADIUS = 0.5f;

void RingGizmoPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, ScriptValuePointer& properties,
                                          ScriptEngine* engine, bool skipDefaults,
                                          EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_START_ANGLE, Ring, ring, StartAngle, startAngle);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_END_ANGLE, Ring, ring, EndAngle, endAngle);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_INNER_RADIUS, Ring, ring, InnerRadius, innerRadius);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_INNER_START_COLOR, Ring, ring, InnerStartColor, innerStartColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_INNER_END_COLOR, Ring, ring, InnerEndColor, innerEndColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_OUTER_START_COLOR, Ring, ring, OuterStartColor, outerStartColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_OUTER_END_COLOR, Ring, ring, OuterEndColor, outerEndColor, u8vec3Color);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_INNER_START_ALPHA, Ring, ring, InnerStartAlpha, innerStartAlpha);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_INNER_END_ALPHA, Ring, ring, InnerEndAlpha, innerEndAlpha);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_OUTER_START_ALPHA, Ring, ring, OuterStartAlpha, outerStartAlpha);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_OUTER_END_ALPHA, Ring, ring, OuterEndAlpha, outerEndAlpha);

    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_HAS_TICK_MARKS, Ring, ring, HasTickMarks, hasTickMarks);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_MAJOR_TICK_MARKS_ANGLE, Ring, ring, MajorTickMarksAngle, majorTickMarksAngle);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_MINOR_TICK_MARKS_ANGLE, Ring, ring, MinorTickMarksAngle, minorTickMarksAngle);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_MAJOR_TICK_MARKS_LENGTH, Ring, ring, MajorTickMarksLength, majorTickMarksLength);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_MINOR_TICK_MARKS_LENGTH, Ring, ring, MinorTickMarksLength, minorTickMarksLength);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_MAJOR_TICK_MARKS_COLOR, Ring, ring, MajorTickMarksColor, majorTickMarksColor, u8vec3Color);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE_TYPED(PROP_MINOR_TICK_MARKS_COLOR, Ring, ring, MinorTickMarksColor, minorTickMarksColor, u8vec3Color);
}

void RingGizmoPropertyGroup::copyFromScriptValue(const ScriptValuePointer& object, bool& _defaultSettings) {
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, startAngle, float, setStartAngle);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, endAngle, float, setEndAngle);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, innerRadius, float, setInnerRadius);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, innerStartColor, u8vec3Color, setInnerStartColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, innerEndColor, u8vec3Color, setInnerEndColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, outerStartColor, u8vec3Color, setOuterStartColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, outerEndColor, u8vec3Color, setOuterEndColor);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, innerStartAlpha, float, setInnerStartAlpha);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, innerEndAlpha, float, setInnerEndAlpha);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, outerStartAlpha, float, setOuterStartAlpha);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, outerEndAlpha, float, setOuterEndAlpha);

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, hasTickMarks, bool, setHasTickMarks);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, majorTickMarksAngle, float, setMajorTickMarksAngle);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, minorTickMarksAngle, float, setMinorTickMarksAngle);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, majorTickMarksLength, float, setMajorTickMarksLength);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, minorTickMarksLength, float, setMinorTickMarksLength);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, majorTickMarksColor, u8vec3Color, setMajorTickMarksColor);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(ring, minorTickMarksColor, u8vec3Color, setMinorTickMarksColor);
}

void RingGizmoPropertyGroup::merge(const RingGizmoPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(startAngle);
    COPY_PROPERTY_IF_CHANGED(endAngle);
    COPY_PROPERTY_IF_CHANGED(innerRadius);

    COPY_PROPERTY_IF_CHANGED(innerStartColor);
    COPY_PROPERTY_IF_CHANGED(innerEndColor);
    COPY_PROPERTY_IF_CHANGED(outerStartColor);
    COPY_PROPERTY_IF_CHANGED(outerEndColor);

    COPY_PROPERTY_IF_CHANGED(innerStartAlpha);
    COPY_PROPERTY_IF_CHANGED(innerEndAlpha);
    COPY_PROPERTY_IF_CHANGED(outerStartAlpha);
    COPY_PROPERTY_IF_CHANGED(outerEndAlpha);

    COPY_PROPERTY_IF_CHANGED(hasTickMarks);
    COPY_PROPERTY_IF_CHANGED(majorTickMarksAngle);
    COPY_PROPERTY_IF_CHANGED(minorTickMarksAngle);
    COPY_PROPERTY_IF_CHANGED(majorTickMarksLength);
    COPY_PROPERTY_IF_CHANGED(minorTickMarksLength);
    COPY_PROPERTY_IF_CHANGED(majorTickMarksColor);
    COPY_PROPERTY_IF_CHANGED(minorTickMarksColor);
}

void RingGizmoPropertyGroup::debugDump() const {
    qCDebug(entities) << "   RingGizmoPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "            _startAngle:" << _startAngle;
    qCDebug(entities) << "              _endAngle:" << _endAngle;
    qCDebug(entities) << "           _innerRadius:" << _innerRadius;
    qCDebug(entities) << "       _innerStartColor:" << _innerStartColor;
    qCDebug(entities) << "         _innerEndColor:" << _innerEndColor;
    qCDebug(entities) << "       _outerStartColor:" << _outerStartColor;
    qCDebug(entities) << "         _outerEndColor:" << _outerEndColor;
    qCDebug(entities) << "       _innerStartAlpha:" << _innerStartAlpha;
    qCDebug(entities) << "         _innerEndAlpha:" << _innerEndAlpha;
    qCDebug(entities) << "       _outerStartAlpha:" << _outerStartAlpha;
    qCDebug(entities) << "         _outerEndAlpha:" << _outerEndAlpha;
    qCDebug(entities) << "          _hasTickMarks:" << _hasTickMarks;
    qCDebug(entities) << "   _majorTickMarksAngle:" << _majorTickMarksAngle;
    qCDebug(entities) << "   _minorTickMarksAngle:" << _minorTickMarksAngle;
    qCDebug(entities) << "  _majorTickMarksLength:" << _majorTickMarksLength;
    qCDebug(entities) << "  _minorTickMarksLength:" << _minorTickMarksLength;
    qCDebug(entities) << "   _majorTickMarksColor:" << _majorTickMarksColor;
    qCDebug(entities) << "   _minorTickMarksColor:" << _minorTickMarksColor;
}

void RingGizmoPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (startAngleChanged()) {
        out << "ring-startAngle";
    }
    if (endAngleChanged()) {
        out << "ring-endAngle";
    }
    if (innerRadiusChanged()) {
        out << "ring-innerRadius";
    }

    if (innerStartColorChanged()) {
        out << "ring-innerStartColor";
    }
    if (innerEndColorChanged()) {
        out << "ring-innerEndColor";
    }
    if (outerStartColorChanged()) {
        out << "ring-outerStartColor";
    }
    if (outerEndColorChanged()) {
        out << "ring-outerEndColor";
    }

    if (innerStartAlphaChanged()) {
        out << "ring-innerStartAlpha";
    }
    if (innerEndAlphaChanged()) {
        out << "ring-innerEndAlpha";
    }
    if (outerStartAlphaChanged()) {
        out << "ring-outerStartAlpha";
    }
    if (outerEndAlphaChanged()) {
        out << "ring-outerEndAlpha";
    }

    if (hasTickMarksChanged()) {
        out << "ring-hasTickMarks";
    }
    if (majorTickMarksAngleChanged()) {
        out << "ring-majorTickMarksAngle";
    }
    if (minorTickMarksAngleChanged()) {
        out << "ring-minorTickMarksAngle";
    }
    if (majorTickMarksLengthChanged()) {
        out << "ring-majorTickMarksLength";
    }
    if (minorTickMarksLengthChanged()) {
        out << "ring-minorTickMarksLength";
    }
    if (majorTickMarksColorChanged()) {
        out << "ring-majorTickMarksColor";
    }
    if (minorTickMarksColorChanged()) {
        out << "ring-minorTickMarksColor";
    }
}

bool RingGizmoPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                           EntityPropertyFlags& requestedProperties,
                                           EntityPropertyFlags& propertyFlags,
                                           EntityPropertyFlags& propertiesDidntFit,
                                           int& propertyCount,
                                           OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_START_ANGLE, getStartAngle());
    APPEND_ENTITY_PROPERTY(PROP_END_ANGLE, getEndAngle());
    APPEND_ENTITY_PROPERTY(PROP_INNER_RADIUS, getInnerRadius());

    APPEND_ENTITY_PROPERTY(PROP_INNER_START_COLOR, getInnerStartColor());
    APPEND_ENTITY_PROPERTY(PROP_INNER_END_COLOR, getInnerEndColor());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_START_COLOR, getOuterStartColor());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_END_COLOR, getOuterEndColor());

    APPEND_ENTITY_PROPERTY(PROP_INNER_START_ALPHA, getInnerStartAlpha());
    APPEND_ENTITY_PROPERTY(PROP_INNER_END_ALPHA, getInnerEndAlpha());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_START_ALPHA, getOuterStartAlpha());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_END_ALPHA, getOuterEndAlpha());

    APPEND_ENTITY_PROPERTY(PROP_HAS_TICK_MARKS, getHasTickMarks());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_ANGLE, getMajorTickMarksAngle());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_ANGLE, getMinorTickMarksAngle());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_LENGTH, getMajorTickMarksLength());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_LENGTH, getMinorTickMarksLength());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_COLOR, getMajorTickMarksColor());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_COLOR, getMinorTickMarksColor());

    return true;
}

bool RingGizmoPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags,
                                             const unsigned char*& dataAt , int& processedBytes) {

    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_START_ANGLE, float, setStartAngle);
    READ_ENTITY_PROPERTY(PROP_END_ANGLE, float, setEndAngle);
    READ_ENTITY_PROPERTY(PROP_INNER_RADIUS, float, setInnerRadius);

    READ_ENTITY_PROPERTY(PROP_INNER_START_COLOR, u8vec3Color, setInnerStartColor);
    READ_ENTITY_PROPERTY(PROP_INNER_END_COLOR, u8vec3Color, setInnerEndColor);
    READ_ENTITY_PROPERTY(PROP_OUTER_START_COLOR, u8vec3Color, setOuterStartColor);
    READ_ENTITY_PROPERTY(PROP_OUTER_END_COLOR, u8vec3Color, setOuterEndColor);

    READ_ENTITY_PROPERTY(PROP_INNER_START_ALPHA, float, setInnerStartAlpha);
    READ_ENTITY_PROPERTY(PROP_INNER_END_ALPHA, float, setInnerEndAlpha);
    READ_ENTITY_PROPERTY(PROP_OUTER_START_ALPHA, float, setOuterStartAlpha);
    READ_ENTITY_PROPERTY(PROP_OUTER_END_ALPHA, float, setOuterEndAlpha);

    READ_ENTITY_PROPERTY(PROP_HAS_TICK_MARKS, bool, setHasTickMarks);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_ANGLE, float, setMajorTickMarksAngle);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_ANGLE, float, setMinorTickMarksAngle);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_LENGTH, float, setMajorTickMarksLength);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_LENGTH, float, setMinorTickMarksLength);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_COLOR, u8vec3Color, setMajorTickMarksColor);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_COLOR, u8vec3Color, setMinorTickMarksColor);


    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_START_ANGLE, StartAngle);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_END_ANGLE, EndAngle);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_INNER_RADIUS, InnerRadius);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_INNER_START_COLOR, InnerStartColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_INNER_END_COLOR, InnerEndColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_OUTER_START_COLOR, OuterStartColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_OUTER_END_COLOR, OuterEndColor);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_INNER_START_ALPHA, InnerStartAlpha);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_INNER_END_ALPHA, InnerEndAlpha);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_OUTER_START_ALPHA, OuterStartAlpha);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_OUTER_END_ALPHA, OuterEndAlpha);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_HAS_TICK_MARKS, HasTickMarks);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MAJOR_TICK_MARKS_ANGLE, MajorTickMarksAngle);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MINOR_TICK_MARKS_ANGLE, MinorTickMarksAngle);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MAJOR_TICK_MARKS_LENGTH, MajorTickMarksLength);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MINOR_TICK_MARKS_LENGTH, MinorTickMarksLength);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MAJOR_TICK_MARKS_COLOR, MajorTickMarksColor);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_MINOR_TICK_MARKS_COLOR, MinorTickMarksColor);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void RingGizmoPropertyGroup::markAllChanged() {
    _startAngleChanged = true;
    _endAngleChanged = true;
    _innerRadiusChanged = true;

    _innerStartColorChanged = true;
    _innerEndColorChanged = true;
    _outerStartColorChanged = true;
    _outerEndColorChanged = true;

    _innerStartAlphaChanged = true;
    _innerEndAlphaChanged = true;
    _outerStartAlphaChanged = true;
    _outerEndAlphaChanged = true;

    _hasTickMarksChanged = true;
    _majorTickMarksAngleChanged = true;
    _minorTickMarksAngleChanged = true;
    _majorTickMarksLengthChanged = true;
    _minorTickMarksLengthChanged = true;
    _majorTickMarksColorChanged = true;
    _minorTickMarksColorChanged = true;
}

EntityPropertyFlags RingGizmoPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_START_ANGLE, startAngle);
    CHECK_PROPERTY_CHANGE(PROP_END_ANGLE, endAngle);
    CHECK_PROPERTY_CHANGE(PROP_INNER_RADIUS, innerRadius);

    CHECK_PROPERTY_CHANGE(PROP_INNER_START_COLOR, innerStartColor);
    CHECK_PROPERTY_CHANGE(PROP_INNER_END_COLOR, innerEndColor);
    CHECK_PROPERTY_CHANGE(PROP_OUTER_START_COLOR, outerStartColor);
    CHECK_PROPERTY_CHANGE(PROP_OUTER_END_COLOR, outerEndColor);

    CHECK_PROPERTY_CHANGE(PROP_INNER_START_ALPHA, innerStartAlpha);
    CHECK_PROPERTY_CHANGE(PROP_INNER_END_ALPHA, innerEndAlpha);
    CHECK_PROPERTY_CHANGE(PROP_OUTER_START_ALPHA, outerStartAlpha);
    CHECK_PROPERTY_CHANGE(PROP_OUTER_END_ALPHA, outerEndAlpha);

    CHECK_PROPERTY_CHANGE(PROP_HAS_TICK_MARKS, hasTickMarks);
    CHECK_PROPERTY_CHANGE(PROP_MAJOR_TICK_MARKS_ANGLE, majorTickMarksAngle);
    CHECK_PROPERTY_CHANGE(PROP_MINOR_TICK_MARKS_ANGLE, minorTickMarksAngle);
    CHECK_PROPERTY_CHANGE(PROP_MAJOR_TICK_MARKS_LENGTH, majorTickMarksLength);
    CHECK_PROPERTY_CHANGE(PROP_MINOR_TICK_MARKS_LENGTH, minorTickMarksLength);
    CHECK_PROPERTY_CHANGE(PROP_MAJOR_TICK_MARKS_COLOR, majorTickMarksColor);
    CHECK_PROPERTY_CHANGE(PROP_MINOR_TICK_MARKS_COLOR, minorTickMarksColor);

    return changedProperties;
}

void RingGizmoPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, StartAngle, getStartAngle);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, EndAngle, getEndAngle);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, InnerRadius, getInnerRadius);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, InnerStartColor, getInnerStartColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, InnerEndColor, getInnerEndColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, OuterStartColor, getOuterStartColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, OuterEndColor, getOuterEndColor);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, InnerStartAlpha, getInnerStartAlpha);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, InnerEndAlpha, getInnerEndAlpha);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, OuterStartAlpha, getOuterStartAlpha);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, OuterEndAlpha, getOuterEndAlpha);

    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, HasTickMarks, getHasTickMarks);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MajorTickMarksAngle, getMajorTickMarksAngle);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MinorTickMarksAngle, getMinorTickMarksAngle);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MajorTickMarksLength, getMajorTickMarksLength);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MinorTickMarksLength, getMinorTickMarksLength);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MajorTickMarksColor, getMajorTickMarksColor);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(Ring, MinorTickMarksColor, getMinorTickMarksColor);
}

bool RingGizmoPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, StartAngle, startAngle, setStartAngle);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, EndAngle, endAngle, setEndAngle);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, InnerRadius, innerRadius, setInnerRadius);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, InnerStartColor, innerStartColor, setInnerStartColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, InnerEndColor, innerEndColor, setInnerEndColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, OuterStartColor, outerStartColor, setOuterStartColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, OuterEndColor, outerEndColor, setOuterEndColor);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, InnerStartAlpha, innerStartAlpha, setInnerStartAlpha);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, InnerEndAlpha, innerEndAlpha, setInnerEndAlpha);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, OuterStartAlpha, outerStartAlpha, setOuterStartAlpha);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, OuterEndAlpha, outerEndAlpha, setOuterEndAlpha);

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, HasTickMarks, hasTickMarks, setHasTickMarks);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MajorTickMarksAngle, majorTickMarksAngle, setMajorTickMarksAngle);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MinorTickMarksAngle, minorTickMarksAngle, setMinorTickMarksAngle);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MajorTickMarksLength, majorTickMarksLength, setMajorTickMarksLength);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MinorTickMarksLength, minorTickMarksLength, setMinorTickMarksLength);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MajorTickMarksColor, majorTickMarksColor, setMajorTickMarksColor);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(Ring, MinorTickMarksColor, minorTickMarksColor, setMinorTickMarksColor);

    return somethingChanged;
}

EntityPropertyFlags RingGizmoPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_START_ANGLE;
    requestedProperties += PROP_END_ANGLE;
    requestedProperties += PROP_INNER_RADIUS;

    requestedProperties += PROP_INNER_START_COLOR;
    requestedProperties += PROP_INNER_END_COLOR;
    requestedProperties += PROP_OUTER_START_COLOR;
    requestedProperties += PROP_OUTER_END_COLOR;

    requestedProperties += PROP_INNER_START_ALPHA;
    requestedProperties += PROP_INNER_END_ALPHA;
    requestedProperties += PROP_OUTER_START_ALPHA;
    requestedProperties += PROP_OUTER_END_ALPHA;

    requestedProperties += PROP_HAS_TICK_MARKS;
    requestedProperties += PROP_MAJOR_TICK_MARKS_ANGLE;
    requestedProperties += PROP_MINOR_TICK_MARKS_ANGLE;
    requestedProperties += PROP_MAJOR_TICK_MARKS_LENGTH;
    requestedProperties += PROP_MINOR_TICK_MARKS_LENGTH;
    requestedProperties += PROP_MAJOR_TICK_MARKS_COLOR;
    requestedProperties += PROP_MINOR_TICK_MARKS_COLOR;

    return requestedProperties;
}

void RingGizmoPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                           EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                           EntityPropertyFlags& requestedProperties,
                                           EntityPropertyFlags& propertyFlags,
                                           EntityPropertyFlags& propertiesDidntFit,
                                           int& propertyCount,
                                           OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_START_ANGLE, getStartAngle());
    APPEND_ENTITY_PROPERTY(PROP_END_ANGLE, getEndAngle());
    APPEND_ENTITY_PROPERTY(PROP_INNER_RADIUS, getInnerRadius());

    APPEND_ENTITY_PROPERTY(PROP_INNER_START_COLOR, getInnerStartColor());
    APPEND_ENTITY_PROPERTY(PROP_INNER_END_COLOR, getInnerEndColor());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_START_COLOR, getOuterStartColor());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_END_COLOR, getOuterEndColor());

    APPEND_ENTITY_PROPERTY(PROP_INNER_START_ALPHA, getInnerStartAlpha());
    APPEND_ENTITY_PROPERTY(PROP_INNER_END_ALPHA, getInnerEndAlpha());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_START_ALPHA, getOuterStartAlpha());
    APPEND_ENTITY_PROPERTY(PROP_OUTER_END_ALPHA, getOuterEndAlpha());

    APPEND_ENTITY_PROPERTY(PROP_HAS_TICK_MARKS, getHasTickMarks());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_ANGLE, getMajorTickMarksAngle());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_ANGLE, getMinorTickMarksAngle());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_LENGTH, getMajorTickMarksLength());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_LENGTH, getMinorTickMarksLength());
    APPEND_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_COLOR, getMajorTickMarksColor());
    APPEND_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_COLOR, getMinorTickMarksColor());
}

int RingGizmoPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                        ReadBitstreamToTreeParams& args,
                                                        EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                        bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_START_ANGLE, float, setStartAngle);
    READ_ENTITY_PROPERTY(PROP_END_ANGLE, float, setEndAngle);
    READ_ENTITY_PROPERTY(PROP_INNER_RADIUS, float, setInnerRadius);

    READ_ENTITY_PROPERTY(PROP_INNER_START_COLOR, u8vec3Color, setInnerStartColor);
    READ_ENTITY_PROPERTY(PROP_INNER_END_COLOR, u8vec3Color, setInnerEndColor);
    READ_ENTITY_PROPERTY(PROP_OUTER_START_COLOR, u8vec3Color, setOuterStartColor);
    READ_ENTITY_PROPERTY(PROP_OUTER_END_COLOR, u8vec3Color, setOuterEndColor);

    READ_ENTITY_PROPERTY(PROP_INNER_START_ALPHA, float, setInnerStartAlpha);
    READ_ENTITY_PROPERTY(PROP_INNER_END_ALPHA, float, setInnerEndAlpha);
    READ_ENTITY_PROPERTY(PROP_OUTER_START_ALPHA, float, setOuterStartAlpha);
    READ_ENTITY_PROPERTY(PROP_OUTER_END_ALPHA, float, setOuterEndAlpha);

    READ_ENTITY_PROPERTY(PROP_HAS_TICK_MARKS, bool, setHasTickMarks);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_ANGLE, float, setMajorTickMarksAngle);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_ANGLE, float, setMinorTickMarksAngle);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_LENGTH, float, setMajorTickMarksLength);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_LENGTH, float, setMinorTickMarksLength);
    READ_ENTITY_PROPERTY(PROP_MAJOR_TICK_MARKS_COLOR, u8vec3Color, setMajorTickMarksColor);
    READ_ENTITY_PROPERTY(PROP_MINOR_TICK_MARKS_COLOR, u8vec3Color, setMinorTickMarksColor);

    return bytesRead;
}

bool RingGizmoPropertyGroup::operator==(const RingGizmoPropertyGroup& a) const {
    return (a._startAngle == _startAngle) &&
           (a._endAngle == _endAngle) &&
           (a._innerRadius == _innerRadius) &&
           (a._innerStartColor == _innerStartColor) &&
           (a._innerEndColor == _innerEndColor) &&
           (a._outerStartColor == _outerStartColor) &&
           (a._outerEndColor == _outerEndColor) &&
           (a._innerStartAlpha == _innerStartAlpha) &&
           (a._innerEndAlpha == _innerEndAlpha) &&
           (a._outerStartAlpha == _outerStartAlpha) &&
           (a._outerEndAlpha == _outerEndAlpha) &&
           (a._hasTickMarks == _hasTickMarks) &&
           (a._majorTickMarksAngle == _majorTickMarksAngle) &&
           (a._minorTickMarksAngle == _minorTickMarksAngle) &&
           (a._majorTickMarksLength == _majorTickMarksLength) &&
           (a._minorTickMarksLength == _minorTickMarksLength) &&
           (a._majorTickMarksColor == _majorTickMarksColor) &&
           (a._minorTickMarksColor == _minorTickMarksColor);
}