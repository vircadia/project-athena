//
//  LaserPointerScriptingInterface.cpp
//  interface/src/raypick
//
//  Created by Sam Gondelman 7/11/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LaserPointerScriptingInterface.h"

#include "PointerScriptingInterface.h"
#include <ScriptValueUtils.h>

void LaserPointerScriptingInterface::setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreItems) const {
    DependencyManager::get<PointerManager>()->setIgnoreItems(uid, qVectorQUuidFromScriptValue(ignoreItems));
}

void LaserPointerScriptingInterface::setIncludeItems(unsigned int uid, const ScriptValuePointer& includeItems) const {
    DependencyManager::get<PointerManager>()->setIncludeItems(uid, qVectorQUuidFromScriptValue(includeItems));
}

unsigned int LaserPointerScriptingInterface::createLaserPointer(const QVariant& properties) const {
    return DependencyManager::get<PointerScriptingInterface>()->createPointer(PickQuery::PickType::Ray, properties);
}

void LaserPointerScriptingInterface::editRenderState(unsigned int uid, const QString& renderState, const QVariant& properties) const {
    DependencyManager::get<PointerScriptingInterface>()->editRenderState(uid, renderState, properties);
}

QVariantMap LaserPointerScriptingInterface::getPrevRayPickResult(unsigned int uid) const {
    return DependencyManager::get<PointerScriptingInterface>()->getPrevPickResult(uid);
}
