//
//  Quat.cpp
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 1/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Quat.h"

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>

#include <QDebug>

#include "ScriptEngineLogging.h"
#include "ScriptEngine.h"
#include "ScriptManager.h"

quat Quat::normalize(const glm::quat& q) {
    return glm::normalize(q);
}

quat Quat::conjugate(const glm::quat& q) {
    return glm::conjugate(q);
}

glm::quat Quat::rotationBetween(const glm::vec3& v1, const glm::vec3& v2) {
    return ::rotationBetween(v1, v2);
}

glm::quat Quat::lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
    return glm::inverse(glm::quat_cast(glm::lookAt(eye, center, up)));  // OpenGL view matrix is inverse of our camera matrix.
}

glm::quat Quat::lookAtSimple(const glm::vec3& eye, const glm::vec3& center) {
    auto dir = glm::normalize(center - eye);
    // if the direction is nearly aligned with the Y axis, then use the X axis for 'up'
    const float MAX_ABS_Y_COMPONENT = 0.9999991f;
    if (fabsf(dir.y) > MAX_ABS_Y_COMPONENT) {
        return lookAt(eye, center, Vectors::UNIT_X);
    }
    return lookAt(eye, center, Vectors::UNIT_Y);
}

glm::quat Quat::multiply(const glm::quat& q1, const glm::quat& q2) {
    return q1 * q2;
}

glm::quat Quat::fromVec3Degrees(const glm::vec3& eulerAngles) {
    return glm::quat(glm::radians(eulerAngles));
}

glm::quat Quat::fromVec3Radians(const glm::vec3& eulerAngles) {
    return glm::quat(eulerAngles);
}

glm::quat Quat::fromPitchYawRollDegrees(float pitch, float yaw, float roll) {
    return glm::quat(glm::radians(glm::vec3(pitch, yaw, roll)));
}

glm::quat Quat::fromPitchYawRollRadians(float pitch, float yaw, float roll) {
    return glm::quat(glm::vec3(pitch, yaw, roll));
}

glm::quat Quat::inverse(const glm::quat& q) {
    return glm::inverse(q);
}

glm::vec3 Quat::getForward(const glm::quat& orientation) {
    return orientation * Vectors::FRONT;
}

glm::vec3 Quat::getRight(const glm::quat& orientation) {
    return orientation * Vectors::RIGHT;
}

glm::vec3 Quat::getUp(const glm::quat& orientation) {
    return orientation * Vectors::UP;
}

glm::vec3 Quat::safeEulerAngles(const glm::quat& orientation) {
    return glm::degrees(::safeEulerAngles(orientation));
}

glm::quat Quat::angleAxis(float angle, const glm::vec3& v) {
    return glm::angleAxis(glm::radians(angle), v);
}

glm::vec3 Quat::axis(const glm::quat& orientation) {
    return glm::axis(orientation);
}

float Quat::angle(const glm::quat& orientation) {
    return glm::angle(orientation);
}

glm::quat Quat::mix(const glm::quat& q1, const glm::quat& q2, float alpha) {
    return safeMix(q1, q2, alpha);
}

/// Spherical Linear Interpolation
glm::quat Quat::slerp(const glm::quat& q1, const glm::quat& q2, float alpha) {
    return glm::slerp(q1, q2, alpha);
}

// Spherical Quadratic Interpolation
glm::quat Quat::squad(const glm::quat& q1, const glm::quat& q2, const glm::quat& s1, const glm::quat& s2, float h) {
    return glm::squad(q1, q2, s1, s2, h);
}

float Quat::dot(const glm::quat& q1, const glm::quat& q2) {
    return glm::dot(q1, q2);
}

void Quat::print(const QString& label, const glm::quat& q, bool asDegrees) {
    QString message = QString("%1 %2").arg(qPrintable(label));
    if (asDegrees) {
        message = message.arg(glm::to_string(glm::dvec3(safeEulerAngles(q))).c_str());
    } else {
        message = message.arg(glm::to_string(glm::dquat(q)).c_str());
    }
    qCDebug(scriptengine) << message;
    if (ScriptManager* scriptManager = engine()->manager()) {
        scriptManager->print(message);
    }
}

bool Quat::equal(const glm::quat& q1, const glm::quat& q2) {
    return q1 == q2;
}

glm::quat Quat::cancelOutRollAndPitch(const glm::quat& q) {
    return ::cancelOutRollAndPitch(q);
}

glm::quat Quat::cancelOutRoll(const glm::quat& q) {
    return ::cancelOutRoll(q);
}
