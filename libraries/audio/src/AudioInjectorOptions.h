//
//  AudioInjectorOptions.h
//  libraries/audio/src
//
//  Created by Stephen Birarda on 1/2/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AudioInjectorOptions_h
#define hifi_AudioInjectorOptions_h

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class AudioInjectorOptions {
public:
    AudioInjectorOptions();
    glm::vec3 position;
    bool positionSet;
    float volume;
    bool loop;
    glm::quat orientation;
    bool stereo;
    bool ambisonic;
    bool ignorePenumbra;
    bool localOnly;
    float secondOffset;
    float pitch;    // multiplier, where 2.0f shifts up one octave
};

Q_DECLARE_METATYPE(AudioInjectorOptions);

ScriptValuePointer injectorOptionsToScriptValue(ScriptEngine* engine, const AudioInjectorOptions& injectorOptions);
void injectorOptionsFromScriptValue(const ScriptValuePointer& object, AudioInjectorOptions& injectorOptions);

#endif // hifi_AudioInjectorOptions_h
