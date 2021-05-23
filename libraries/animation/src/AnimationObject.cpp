//
//  AnimationObject.cpp
//  libraries/animation/src/
//
//  Created by Andrzej Kapolka on 4/17/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimationObject.h"

#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptValue.h>

#include "AnimationCache.h"

QStringList AnimationObject::getJointNames() const {
    return scriptvalue_cast<AnimationPointer>(thisObject())->getJointNames();
}

QVector<HFMAnimationFrame> AnimationObject::getFrames() const {
    return scriptvalue_cast<AnimationPointer>(thisObject())->getFrames();
}

QVector<glm::quat> AnimationFrameObject::getRotations() const {
    return scriptvalue_cast<HFMAnimationFrame>(thisObject()).rotations;
}

void registerAnimationTypes(ScriptEngine* engine) {
    scriptRegisterSequenceMetaType<QVector<HFMAnimationFrame> >(engine);
    engine->setDefaultPrototype(qMetaTypeId<HFMAnimationFrame>(), engine->newQObject(
        new AnimationFrameObject(), ScriptEngine::ScriptOwnership));
    engine->setDefaultPrototype(qMetaTypeId<AnimationPointer>(), engine->newQObject(
        new AnimationObject(), ScriptEngine::ScriptOwnership));
}

