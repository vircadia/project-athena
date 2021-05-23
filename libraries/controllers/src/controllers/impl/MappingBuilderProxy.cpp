//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MappingBuilderProxy.h"

#include <QtCore/QHash>
#include <QtCore/QDebug>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "RouteBuilderProxy.h"
#include "../ScriptingInterface.h"
#include "../Logging.h"
#include <ScriptValue.h>

using namespace controller;

QObject* MappingBuilderProxy::fromQml(const QJSValue& source) {
    qCDebug(controllers) << "Creating new Route builder proxy from " << source.toString();
    auto sourceEndpoint = _parent.endpointFor(source);
    return from(sourceEndpoint);
}

QObject* MappingBuilderProxy::from(const ScriptValuePointer& source) {
    qCDebug(controllers) << "Creating new Route builder proxy from " << source->toString();
    auto sourceEndpoint = _parent.endpointFor(source);
    return from(sourceEndpoint);
}

QObject* MappingBuilderProxy::from(const Endpoint::Pointer& source) {
    if (source) {
        auto route = Route::Pointer(new Route());
        route->source = source;
        return new RouteBuilderProxy(_parent, _mapping, route);
    } else {
        qCDebug(controllers) << "MappingBuilderProxy::from : source is null so no route created";
        return nullptr;
    }
}

QObject* MappingBuilderProxy::makeAxisQml(const QJSValue& source1, const QJSValue& source2) {
    auto source1Endpoint = _parent.endpointFor(source1);
    auto source2Endpoint = _parent.endpointFor(source2);
    return from(_parent.compositeEndpointFor(source1Endpoint, source2Endpoint));
}

QObject* MappingBuilderProxy::makeAxis(const ScriptValuePointer& source1, const ScriptValuePointer& source2) {
    auto source1Endpoint = _parent.endpointFor(source1);
    auto source2Endpoint = _parent.endpointFor(source2);
    return from(_parent.compositeEndpointFor(source1Endpoint, source2Endpoint));
}

QObject* MappingBuilderProxy::enable(bool enable) {
    _parent.enableMapping(_mapping->name, enable);
    return this;
}


