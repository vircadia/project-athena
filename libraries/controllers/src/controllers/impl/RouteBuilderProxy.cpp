//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "RouteBuilderProxy.h"

#include <QtCore/QDebug>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <GLMHelpers.h>

#include "MappingBuilderProxy.h"
#include "../ScriptingInterface.h"
#include "../Logging.h"
#include <ScriptValue.h>

#include "filters/ClampFilter.h"
#include "filters/ConstrainToIntegerFilter.h"
#include "filters/ConstrainToPositiveIntegerFilter.h"
#include "filters/DeadZoneFilter.h"
#include "filters/HysteresisFilter.h"
#include "filters/InvertFilter.h"
#include "filters/NotFilter.h"
#include "filters/PulseFilter.h"
#include "filters/ScaleFilter.h"
#include "filters/TranslateFilter.h"
#include "filters/TransformFilter.h"
#include "filters/PostTransformFilter.h"
#include "filters/RotateFilter.h"
#include "filters/LowVelocityFilter.h"
#include "filters/ExponentialSmoothingFilter.h"
#include "conditionals/AndConditional.h"

using namespace controller;

void RouteBuilderProxy::toQml(const QJSValue& destination) {
    qCDebug(controllers) << "Completing route " << destination.toString();
    auto destinationEndpoint = _parent.endpointFor(destination);
    return to(destinationEndpoint);
}

void RouteBuilderProxy::to(const ScriptValuePointer& destination) {
    qCDebug(controllers) << "Completing route " << destination->toString();
    auto destinationEndpoint = _parent.endpointFor(destination);
    return to(destinationEndpoint);
}

void RouteBuilderProxy::to(const Endpoint::Pointer& destination) {
    _route->destination = destination;
    _mapping->routes.push_back(_route);
    deleteLater();
}

QObject* RouteBuilderProxy::debug(bool enable) {
    _route->debug = enable;
    return this;
}

QObject* RouteBuilderProxy::peek(bool enable) {
    _route->peek = enable;
    return this;
}

QObject* RouteBuilderProxy::when(const ScriptValuePointer& expression) {
    // FIXME: Support "!" conditional in simple expression and array expression.
    // Note that "!" is supported when parsing a JSON file, in UserInputMapper::parseConditional().
    auto newConditional = _parent.conditionalFor(expression);
    if (_route->conditional) {
        _route->conditional = ConditionalPointer(new AndConditional(_route->conditional, newConditional));
    } else {
        _route->conditional = newConditional;
    }
    return this;
}

QObject* RouteBuilderProxy::whenQml(const QJSValue& expression) {
    auto newConditional = _parent.conditionalFor(expression);
    if (_route->conditional) {
        _route->conditional = ConditionalPointer(new AndConditional(_route->conditional, newConditional));
    } else {
        _route->conditional = newConditional;
    }
    return this;
}

QObject* RouteBuilderProxy::clamp(float min, float max) {
    addFilter(std::make_shared<ClampFilter>(min, max));
    return this;
}

QObject* RouteBuilderProxy::scale(float multiplier) {
    addFilter(std::make_shared<ScaleFilter>(multiplier));
    return this;
}

QObject* RouteBuilderProxy::invert() {
    addFilter(std::make_shared<InvertFilter>());
    return this;
}

QObject* RouteBuilderProxy::hysteresis(float min, float max) {
    addFilter(std::make_shared<HysteresisFilter>(min, max));
    return this;
}

QObject* RouteBuilderProxy::deadZone(float min) {
    addFilter(std::make_shared<DeadZoneFilter>(min));
    return this;
}

QObject* RouteBuilderProxy::translate(glm::vec3 translate) {
    addFilter(std::make_shared<TranslateFilter>(translate));
    return this;
}

QObject* RouteBuilderProxy::transform(glm::mat4 transform) {
    addFilter(std::make_shared<TransformFilter>(transform));
    return this;
}

QObject* RouteBuilderProxy::postTransform(glm::mat4 transform) {
    addFilter(std::make_shared<PostTransformFilter>(transform));
    return this;
}

QObject* RouteBuilderProxy::rotate(glm::quat rotation) {
    addFilter(std::make_shared<RotateFilter>(rotation));
    return this;
}

QObject* RouteBuilderProxy::lowVelocity(float rotationConstant, float translationConstant) {
    addFilter(std::make_shared<LowVelocityFilter>(rotationConstant, translationConstant));
    return this;
}

QObject* RouteBuilderProxy::exponentialSmoothing(float rotationConstant, float translationConstant) {
    addFilter(std::make_shared<ExponentialSmoothingFilter>(rotationConstant, translationConstant));
    return this;
}

QObject* RouteBuilderProxy::constrainToInteger() {
    addFilter(std::make_shared<ConstrainToIntegerFilter>());
    return this;
}

QObject* RouteBuilderProxy::constrainToPositiveInteger() {
    addFilter(std::make_shared<ConstrainToPositiveIntegerFilter>());
    return this;
}

QObject* RouteBuilderProxy::pulse(float interval) {
    addFilter(std::make_shared<PulseFilter>(interval));
    return this;
}

QObject* RouteBuilderProxy::logicalNot() {
    addFilter(std::make_shared<NotFilter>());
    return this;
}

void RouteBuilderProxy::addFilter(Filter::Pointer filter) {
    _route->filters.push_back(filter);
}

