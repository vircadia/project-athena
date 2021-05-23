//
//  Created by Bradley Austin Davis on 2015/10/18
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Pose.h"

#include <ScriptEngine.h>
#include <ScriptValue.h>

#include <RegisteredMetaTypes.h>
#include <ScriptValueUtils.h>

namespace controller {

    Pose::Pose(const vec3& translation, const quat& rotation,
            const vec3& velocity, const vec3& angularVelocity) :
            translation(translation), rotation(rotation), velocity(velocity), angularVelocity(angularVelocity), valid (true) { }

    bool Pose::operator==(const Pose& right) const {
        // invalid poses return false for comparison, even against identical invalid poses, like NaN
        if (!valid || !right.valid) {
            return false;
        }

        // FIXME add margin of error?  Or add an additional withinEpsilon function?
        return translation == right.getTranslation() && rotation == right.getRotation() &&
            velocity == right.getVelocity() && angularVelocity == right.getAngularVelocity();
    }

    /*@jsdoc
     * The pose of a joint or other item relative to the world or a parent.
     * @typedef {object} Pose
     * @property {Vec3} translation - Translation.
     * @property {Quat} rotation - Rotation.
     * @property {Vec3} velocity - Velocity in m/s.
     * @property {Vec3} angularVelocity - Angular velocity in rad/s.
     * @property {boolean} valid - <code>true</code> if the pose is valid, otherwise <code>false</code>.
     */
    ScriptValuePointer Pose::toScriptValue(ScriptEngine* engine, const Pose& pose) {
        ScriptValuePointer obj = engine->newObject();
        obj->setProperty("translation", vec3ToScriptValue(engine, pose.translation));
        obj->setProperty("rotation", quatToScriptValue(engine, pose.rotation));
        obj->setProperty("velocity", vec3ToScriptValue(engine, pose.velocity));
        obj->setProperty("angularVelocity", vec3ToScriptValue(engine, pose.angularVelocity));
        obj->setProperty("valid", pose.valid);
        return obj;
    }

    void Pose::fromScriptValue(const ScriptValuePointer& object, Pose& pose) {
        auto translation = object->property("translation");
        auto rotation = object->property("rotation");
        auto velocity = object->property("velocity");
        auto angularVelocity = object->property("angularVelocity");
        if (translation->isValid() &&
                rotation->isValid() &&
                velocity->isValid() &&
                angularVelocity->isValid()) {
            vec3FromScriptValue(translation, pose.translation);
            quatFromScriptValue(rotation, pose.rotation);
            vec3FromScriptValue(velocity, pose.velocity);
            vec3FromScriptValue(angularVelocity, pose.angularVelocity);
            pose.valid = true;
        } else {
            pose.valid = false;
        }
    }

    Pose Pose::transform(const glm::mat4& mat) const {
        auto rot = glmExtractRotation(mat);
        Pose pose(transformPoint(mat, translation),
                  rot * rotation,
                  transformVectorFast(mat, velocity),
                  rot * angularVelocity);
        pose.valid = valid;
        return pose;
    }

    Pose Pose::postTransform(const glm::mat4& mat) const {
        glm::mat4 original = ::createMatFromQuatAndPos(rotation, translation);
        glm::mat4 result = original * mat;
        auto translationOut = ::extractTranslation(result);
        auto rotationOut = ::glmExtractRotation(result);
        auto velocityOut = velocity + glm::cross(angularVelocity, translationOut - translation); // warning: this may be completely wrong
        auto angularVelocityOut = angularVelocity;

        Pose pose(translationOut,
            rotationOut,
            velocityOut,
            angularVelocityOut);

        pose.valid = valid;
        return pose;
    }

}

