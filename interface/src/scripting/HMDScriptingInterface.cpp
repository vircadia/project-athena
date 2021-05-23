//
//  HMDScriptingInterface.cpp
//  interface/src/scripting
//
//  Created by Thijs Wenker on 1/12/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "HMDScriptingInterface.h"

#include <shared/QtHelpers.h>
#include <avatar/AvatarManager.h>
#include <display-plugins/DisplayPlugin.h>
#include <display-plugins/CompositorHelper.h>
#include <OffscreenUi.h>
#include <plugins/PluginUtils.h>
#include <ScriptContext.h>
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <QUuid>

#include "Application.h"

HMDScriptingInterface::HMDScriptingInterface() {
    connect(qApp, &Application::activeDisplayPluginChanged, [this]{
        emit displayModeChanged(isHMDMode());
    });
    connect(qApp, &Application::miniTabletEnabledChanged, [this](bool enabled) {
        emit miniTabletEnabledChanged(enabled);
    });
    connect(qApp, &Application::awayStateWhenFocusLostInVRChanged, [this](bool enabled) {
        emit awayStateWhenFocusLostInVRChanged(enabled);
    });
}

glm::vec3 HMDScriptingInterface::calculateRayUICollisionPoint(const glm::vec3& position, const glm::vec3& direction) const {
    glm::vec3 result;
    qApp->getApplicationCompositor().calculateRayUICollisionPoint(position, direction, result);
    return result;
}

glm::vec3 HMDScriptingInterface::calculateParabolaUICollisionPoint(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& acceleration, float& parabolicDistance) const {
    glm::vec3 result;
    qApp->getApplicationCompositor().calculateParabolaUICollisionPoint(position, velocity, acceleration, result, parabolicDistance);
    return result;
}

glm::vec2 HMDScriptingInterface::overlayFromWorldPoint(const glm::vec3& position) const {
    return qApp->getApplicationCompositor().overlayFromSphereSurface(position);
}

glm::vec3 HMDScriptingInterface::worldPointFromOverlay(const glm::vec2& overlay) const {
    return qApp->getApplicationCompositor().sphereSurfaceFromOverlay(overlay);
}

glm::vec2 HMDScriptingInterface::sphericalToOverlay(const glm::vec2 & position) const {
    return qApp->getApplicationCompositor().sphericalToOverlay(position);
}

glm::vec2 HMDScriptingInterface::overlayToSpherical(const glm::vec2 & position) const {
    return qApp->getApplicationCompositor().overlayToSpherical(position);
}

bool HMDScriptingInterface::isHMDAvailable(const QString& name) {
    return PluginUtils::isHMDAvailable(name);
}

bool HMDScriptingInterface::isHeadControllerAvailable(const QString& name) {
    return PluginUtils::isHeadControllerAvailable(name);
}

bool HMDScriptingInterface::isHandControllerAvailable(const QString& name) {
    return PluginUtils::isHandControllerAvailable(name);
}


bool HMDScriptingInterface::isSubdeviceContainingNameAvailable(const QString& name) {
    return PluginUtils::isSubdeviceContainingNameAvailable(name);
}

void HMDScriptingInterface::requestShowHandControllers() {
    _showHandControllersCount++;
    emit shouldShowHandControllersChanged();
}

void HMDScriptingInterface::requestHideHandControllers() {
    _showHandControllersCount--;
    emit shouldShowHandControllersChanged();
}

bool HMDScriptingInterface::shouldShowHandControllers() const {
    return _showHandControllersCount > 0;
}

void HMDScriptingInterface::activateHMDHandMouse() {
    QWriteLocker lock(&_hmdHandMouseLock);
    if (auto offscreenUI = DependencyManager::get<OffscreenUi>()) {
        offscreenUI->getDesktop()->setProperty("hmdHandMouseActive", true);
    }
    _hmdHandMouseCount++;
}

void HMDScriptingInterface::deactivateHMDHandMouse() {
    QWriteLocker lock(&_hmdHandMouseLock);
    _hmdHandMouseCount = std::max(_hmdHandMouseCount - 1, 0);
    if (_hmdHandMouseCount == 0) {
        if (auto offscreenUI = DependencyManager::get<OffscreenUi>()) {
            offscreenUI->getDesktop()->setProperty("hmdHandMouseActive", false);
        }
    }
}

void  HMDScriptingInterface::closeTablet() {
    _showTablet = false;
    _tabletContextualMode = false;
}

void HMDScriptingInterface::openTablet(bool contextualMode) {
    _showTablet = true;
    _tabletContextualMode = contextualMode;
}

void HMDScriptingInterface::toggleShouldShowTablet() {
    setShouldShowTablet(!getShouldShowTablet());
}

void HMDScriptingInterface::setShouldShowTablet(bool value) {
    if (_showTablet != value) {
        _showTablet = value;
        _tabletContextualMode = false;
        emit showTabletChanged(value);
    }
}

void HMDScriptingInterface::setMiniTabletEnabled(bool enabled) {
    qApp->setMiniTabletEnabled(enabled);
}

bool HMDScriptingInterface::getMiniTabletEnabled() {
    return qApp->getMiniTabletEnabled();
}

void HMDScriptingInterface::setAwayStateWhenFocusLostInVREnabled(bool enabled) {
    qApp->setAwayStateWhenFocusLostInVREnabled(enabled);
}

bool HMDScriptingInterface::getAwayStateWhenFocusLostInVREnabled() {
    return qApp->getAwayStateWhenFocusLostInVREnabled();
}


ScriptValuePointer HMDScriptingInterface::getHUDLookAtPosition2D(ScriptContext* context, ScriptEngine* engine) {
    glm::vec3 hudIntersection;
    auto instance = DependencyManager::get<HMDScriptingInterface>();
    if (instance->getHUDLookAtPosition3D(hudIntersection)) {
        glm::vec2 overlayPos = qApp->getApplicationCompositor().overlayFromSphereSurface(hudIntersection);
        return scriptValueFromValue<glm::vec2>(engine, overlayPos);
    }
    return engine->nullValue();
}

ScriptValuePointer HMDScriptingInterface::getHUDLookAtPosition3D(ScriptContext* context, ScriptEngine* engine) {
    glm::vec3 result;
    auto instance = DependencyManager::get<HMDScriptingInterface>();
    if (instance->getHUDLookAtPosition3D(result)) {
        return scriptValueFromValue<glm::vec3>(engine, result);
    }
    return engine->nullValue();
}

bool HMDScriptingInterface::getHUDLookAtPosition3D(glm::vec3& result) const {
    const Camera& camera = qApp->getCamera();
    glm::vec3 position = camera.getPosition();
    glm::quat orientation = camera.getOrientation();

    glm::vec3 direction = orientation * glm::vec3(0.0f, 0.0f, -1.0f);

    const auto& compositor = qApp->getApplicationCompositor();

    return compositor.calculateRayUICollisionPoint(position, direction, result);
}

glm::mat4 HMDScriptingInterface::getWorldHMDMatrix() const {
    auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();
    return myAvatar->getSensorToWorldMatrix() * myAvatar->getHMDSensorMatrix();
}

glm::vec3 HMDScriptingInterface::getPosition() const {
    if (qApp->getActiveDisplayPlugin()->isHmd()) {
        return extractTranslation(getWorldHMDMatrix());
    }
    return glm::vec3();
}

glm::quat HMDScriptingInterface::getOrientation() const {
    if (qApp->getActiveDisplayPlugin()->isHmd()) {
        return glmExtractRotation(getWorldHMDMatrix());
    }
    return glm::quat();
}

bool HMDScriptingInterface::isMounted() const {
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    return (displayPlugin->isHmd() && displayPlugin->isDisplayVisible());
}

QString HMDScriptingInterface::preferredAudioInput() const {
    return qApp->getActiveDisplayPlugin()->getPreferredAudioInDevice();
}

QString HMDScriptingInterface::preferredAudioOutput() const {
    return qApp->getActiveDisplayPlugin()->getPreferredAudioOutDevice();
}

bool HMDScriptingInterface::suppressKeyboard() {
    return qApp->getActiveDisplayPlugin()->suppressKeyboard();
}

void HMDScriptingInterface::unsuppressKeyboard() {
    qApp->getActiveDisplayPlugin()->unsuppressKeyboard();
}

bool HMDScriptingInterface::isKeyboardVisible() {
    return qApp->getActiveDisplayPlugin()->isKeyboardVisible();
}

void HMDScriptingInterface::centerUI() {
    QMetaObject::invokeMethod(qApp, "centerUI", Qt::QueuedConnection);
}

QVariant HMDScriptingInterface::getPlayAreaRect() {
    auto rect = qApp->getActiveDisplayPlugin()->getPlayAreaRect();
    return qRectFToVariant(rect);
}

QVector<glm::vec3> HMDScriptingInterface::getSensorPositions() {
    return qApp->getActiveDisplayPlugin()->getSensorPositions();
}

float HMDScriptingInterface::getVisionSqueezeRatioX() const {
    return qApp->getVisionSqueeze().getVisionSqueezeRatioX();
}

float HMDScriptingInterface::getVisionSqueezeRatioY() const {
    return qApp->getVisionSqueeze().getVisionSqueezeRatioY();
}

void HMDScriptingInterface::setVisionSqueezeRatioX(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeRatioX(value);
}

void HMDScriptingInterface::setVisionSqueezeRatioY(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeRatioY(value);
}

float HMDScriptingInterface::getVisionSqueezeUnSqueezeDelay() const {
    return qApp->getVisionSqueeze().getVisionSqueezeUnSqueezeDelay();
}

void HMDScriptingInterface::setVisionSqueezeUnSqueezeDelay(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeUnSqueezeDelay(value);
}

float HMDScriptingInterface::getVisionSqueezeUnSqueezeSpeed() const {
    return qApp->getVisionSqueeze().getVisionSqueezeUnSqueezeSpeed();
}

void HMDScriptingInterface::setVisionSqueezeUnSqueezeSpeed(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeUnSqueezeSpeed(value);
}

float HMDScriptingInterface::getVisionSqueezeTransition() const {
    return qApp->getVisionSqueeze().getVisionSqueezeTransition();
}

void HMDScriptingInterface::setVisionSqueezeTransition(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeTransition(value);
}

int HMDScriptingInterface::getVisionSqueezePerEye() const {
    return qApp->getVisionSqueeze().getVisionSqueezePerEye();
}

void HMDScriptingInterface::setVisionSqueezePerEye(int value) {
    qApp->getVisionSqueeze().setVisionSqueezePerEye(value);
}

float HMDScriptingInterface::getVisionSqueezeGroundPlaneY() const {
    return qApp->getVisionSqueeze().getVisionSqueezeGroundPlaneY();
}

void HMDScriptingInterface::setVisionSqueezeGroundPlaneY(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeGroundPlaneY(value);
}

float HMDScriptingInterface::getVisionSqueezeSpotlightSize() const {
    return qApp->getVisionSqueeze().getVisionSqueezeSpotlightSize();
}

void HMDScriptingInterface::setVisionSqueezeSpotlightSize(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeSpotlightSize(value);
}

float HMDScriptingInterface::getVisionSqueezeTurningXFactor() const {
    return qApp->getVisionSqueeze().getVisionSqueezeTurningXFactor();
}

void HMDScriptingInterface::setVisionSqueezeTurningXFactor(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeTurningXFactor(value);
}

float HMDScriptingInterface::getVisionSqueezeTurningYFactor() const {
    return qApp->getVisionSqueeze().getVisionSqueezeTurningYFactor();
}

void HMDScriptingInterface::setVisionSqueezeTurningYFactor(float value) {
    qApp->getVisionSqueeze().setVisionSqueezeTurningYFactor(value);
}
