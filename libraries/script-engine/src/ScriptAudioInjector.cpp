//
//  ScriptAudioInjector.cpp
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2015-02-11.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptAudioInjector.h"

#include "ScriptEngine.h"
#include "ScriptEngineLogging.h"
#include "ScriptValue.h"

ScriptValuePointer injectorToScriptValue(ScriptEngine* engine, ScriptAudioInjector* const& in) {
    // The AudioScriptingInterface::playSound method can return null, so we need to account for that.
    if (!in) {
        return engine->nullValue();
    }

    return engine->newQObject(in, ScriptEngine::ScriptOwnership);
}

void injectorFromScriptValue(const ScriptValuePointer& object, ScriptAudioInjector*& out) {
    out = qobject_cast<ScriptAudioInjector*>(object->toQObject());
}

ScriptAudioInjector::ScriptAudioInjector(const AudioInjectorPointer& injector) :
    _injector(injector)
{
    QObject::connect(injector.data(), &AudioInjector::finished, this, &ScriptAudioInjector::finished);
    connect(injector.data(), &QObject::destroyed, this, &QObject::deleteLater);
}

ScriptAudioInjector::~ScriptAudioInjector() {
    const auto audioInjectorManager = DependencyManager::get<AudioInjectorManager>();
    // AudioInjectorManager may have been destroyed on application shutdown.
    if (audioInjectorManager) {
        audioInjectorManager->stop(_injector);
    }
}
