//
//  SpatialEvent.h
//  script-engine/src
//
//  Created by Stephen Birarda on 2014-10-27.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SpatialEvent_h
#define hifi_SpatialEvent_h

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <QtCore/QSharedPointer>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

class SpatialEvent {
public:
    SpatialEvent();
    SpatialEvent(const SpatialEvent& other);
    
    static ScriptValuePointer toScriptValue(ScriptEngine* engine, const SpatialEvent& event);
    static void fromScriptValue(const ScriptValuePointer& object, SpatialEvent& event);
    
    glm::vec3 locTranslation;
    glm::quat locRotation;
    glm::vec3 absTranslation;
    glm::quat absRotation;
};

Q_DECLARE_METATYPE(SpatialEvent)

#endif // hifi_SpatialEvent_h