//
//  RayPickScriptingInterface.cpp
//  interface/src/raypick
//
//  Created by Sam Gondelman 8/15/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RayPickScriptingInterface.h"

#include <QVariant>
#include "GLMHelpers.h"

#include <PickManager.h>
#include <ScriptValueUtils.h>

unsigned int RayPickScriptingInterface::createRayPick(const QVariant& properties) {
    return DependencyManager::get<PickScriptingInterface>()->createPick(PickQuery::PickType::Ray, properties);
}

void RayPickScriptingInterface::enableRayPick(unsigned int uid) {
    DependencyManager::get<PickManager>()->enablePick(uid);
}

void RayPickScriptingInterface::disableRayPick(unsigned int uid) {
    DependencyManager::get<PickManager>()->disablePick(uid);
}

void RayPickScriptingInterface::removeRayPick(unsigned int uid) {
    DependencyManager::get<PickManager>()->removePick(uid);
}

QVariantMap RayPickScriptingInterface::getPrevRayPickResult(unsigned int uid) {
    QVariantMap result;
    auto pickResult = DependencyManager::get<PickManager>()->getPrevPickResult(uid);
    if (pickResult) {
        result = pickResult->toVariantMap();
    }
    return result;
}

void RayPickScriptingInterface::setPrecisionPicking(unsigned int uid, bool precisionPicking) {
    DependencyManager::get<PickManager>()->setPrecisionPicking(uid, precisionPicking);
}

void RayPickScriptingInterface::setIgnoreItems(unsigned int uid, const ScriptValuePointer& ignoreItems) {
    DependencyManager::get<PickManager>()->setIgnoreItems(uid, qVectorQUuidFromScriptValue(ignoreItems));
}

void RayPickScriptingInterface::setIncludeItems(unsigned int uid, const ScriptValuePointer& includeItems) {
    DependencyManager::get<PickManager>()->setIncludeItems(uid, qVectorQUuidFromScriptValue(includeItems));
}

bool RayPickScriptingInterface::isLeftHand(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isLeftHand(uid);
}

bool RayPickScriptingInterface::isRightHand(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isRightHand(uid);
}

bool RayPickScriptingInterface::isMouse(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isMouse(uid);
}