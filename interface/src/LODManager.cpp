//
//  LODManager.cpp
//  interface/src/LODManager.h
//
//  Created by Clement on 1/16/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LODManager.h"

#include <SettingHandle.h>
#include <Util.h>
#include <shared/GlobalAppProperties.h>

#include "Application.h"
#include "ui/DialogsManager.h"
#include "InterfaceLogging.h"

const QString LOD_SETTINGS_PREFIX { "lodManager/" };

Setting::Handle<bool> automaticLODAdjust(LOD_SETTINGS_PREFIX + "automaticLODAdjust", (bool)DEFAULT_LOD_AUTO_ADJUST);
Setting::Handle<float> lodHalfAngle(LOD_SETTINGS_PREFIX + "lodHalfAngle", (float)getHalfAngleFromVisibilityDistance(DEFAULT_VISIBILITY_DISTANCE_FOR_UNIT_ELEMENT));
Setting::Handle<int> desktopWorldDetailQuality(LOD_SETTINGS_PREFIX + "desktopWorldDetailQuality", (int)DEFAULT_WORLD_DETAIL_QUALITY);
Setting::Handle<int> hmdWorldDetailQuality(LOD_SETTINGS_PREFIX + "hmdWorldDetailQuality", (int)DEFAULT_WORLD_DETAIL_QUALITY);

LODManager::LODManager() {
}

// We use a "time-weighted running average" of the maxRenderTime and compare it against min/max thresholds
// to determine if we should adjust the level of detail (LOD).
//
// A time-weighted running average has a timescale which determines how fast the average tracks the measured
// value in real-time.  Given a step-function in the mesured value, and assuming measurements happen
// faster than the runningAverage is computed, the error between the value and its runningAverage will be
// reduced by 1/e every timescale of real-time that passes.
const float LOD_ADJUST_RUNNING_AVG_TIMESCALE = 0.08f; // sec

// batchTIme is always contained in presentTime.
// We favor using batchTime instead of presentTime as a representative value for rendering duration (on present thread) 
// if batchTime + cushionTime < presentTime.
// since we are shooting for fps around 60, 90Hz, the ideal frames are around 10ms
// so we are picking a cushion time of 3ms 
const float LOD_BATCH_TO_PRESENT_CUSHION_TIME = 3.0f; // msec

void LODManager::setRenderTimes(float presentTime, float engineRunTime, float batchTime, float gpuTime) {
    // Make sure the sampled time are positive values
    _presentTime = std::max(0.0f, presentTime);
    _engineRunTime = std::max(0.0f, engineRunTime);
    _batchTime = std::max(0.0f, batchTime);
    _gpuTime = std::max(0.0f, gpuTime);
}

void LODManager::autoAdjustLOD(float realTimeDelta) {
    std::lock_guard<std::mutex> { _automaticLODLock };
 
    // The "render time" is the worse of:
    // - engineRunTime: Time spent in the render thread in the engine producing the gpu::Frame N
    // - batchTime: Time spent in the present thread processing the batches of gpu::Frame N+1
    // - presentTime: Time spent in the present thread between the last 2 swap buffers considered the total time to submit gpu::Frame N+1
    // - gpuTime: Time spent in the GPU executing the gpu::Frame N + 2

    // But Present time is in reality synched with the monitor/display refresh rate, it s always longer than batchTime.
    // So if batchTime is fast enough relative to presentTime we are using it, otherwise we are using presentTime. got it ?
    auto presentTime = (_presentTime > _batchTime + LOD_BATCH_TO_PRESENT_CUSHION_TIME ? _batchTime + LOD_BATCH_TO_PRESENT_CUSHION_TIME : _presentTime);
    float maxRenderTime = glm::max(glm::max(presentTime, _engineRunTime), _gpuTime);

    // maxRenderTime must be a realistic valid duration in order for the regulation to work correctly.
    // We make sure it s a non zero positive value (1.0ms) under 1 sec
    maxRenderTime = std::max(1.0f, std::min(maxRenderTime, (float)MSECS_PER_SECOND));

    // realTimeDelta must be a realistic valid duration in order for the regulation to work correctly.
    // We make sure it a positive value under 1 sec
    // note that if real time delta is very small we will early exit to avoid division by zero
    realTimeDelta = std::max(0.0f, std::min(realTimeDelta, 1.0f));

    // compute time-weighted running average render time (now and smooth)
    // We MUST clamp the blend between 0.0 and 1.0 for stability
    float nowBlend = (realTimeDelta < LOD_ADJUST_RUNNING_AVG_TIMESCALE) ? realTimeDelta / LOD_ADJUST_RUNNING_AVG_TIMESCALE : 1.0f;
    float smoothBlend = (realTimeDelta <  LOD_ADJUST_RUNNING_AVG_TIMESCALE * _smoothScale) ? realTimeDelta / (LOD_ADJUST_RUNNING_AVG_TIMESCALE * _smoothScale) : 1.0f;

    //Evaluate the running averages for the render time
    // We must sanity check for the output average evaluated to be in a valid range to avoid issues 
    _nowRenderTime = (1.0f - nowBlend) * _nowRenderTime + nowBlend * maxRenderTime; // msec
    _nowRenderTime = std::max(0.0f, std::min(_nowRenderTime, (float)MSECS_PER_SECOND));
    _smoothRenderTime = (1.0f - smoothBlend) * _smoothRenderTime + smoothBlend * maxRenderTime; // msec
    _smoothRenderTime = std::max(0.0f, std::min(_smoothRenderTime, (float)MSECS_PER_SECOND));

    // Early exit if not regulating or if the simulation or render times don't matter
    if (!_automaticLODAdjust || realTimeDelta <= 0.0f || _nowRenderTime <= 0.0f || _smoothRenderTime <= 0.0f) {
        return;
    }

    // Previous value for output
    float oldLODAngle = getLODAngleDeg();

    // Target fps is slightly overshooted by 5hz
    float targetFPS = getLODTargetFPS() + LOD_OFFSET_FPS;

    // Current fps based on latest measurments
    float currentNowFPS = (float)MSECS_PER_SECOND / _nowRenderTime;
    float currentSmoothFPS = (float)MSECS_PER_SECOND / _smoothRenderTime;
 
    // Compute the Variance of the FPS signal (FPS - smouthFPS)^2
    // Also scale it by a percentage for fine tuning (default is 100%)
    float currentVarianceFPS = (currentSmoothFPS - currentNowFPS);
    currentVarianceFPS *= currentVarianceFPS;
    currentVarianceFPS *= _pidCoefs.w;

    // evaluate current error between the current smoothFPS and target FPS
    // and the sqaure of the error to compare against the Variance
    auto currentErrorFPS = (targetFPS - currentSmoothFPS);
    auto currentErrorFPSSquare = currentErrorFPS * currentErrorFPS;

    // Define a noiseCoef that is trying to adjust the error to the FPS target value based on its strength
    // relative to the current Variance of the FPS signal.
    // If the error is within the variance, just set to 0.
    // if its within 2x the variance scale the control
    // and full control if error is bigger than 2x variance
    auto noiseCoef = 1.0f;
    if (currentErrorFPSSquare < currentVarianceFPS) {
        noiseCoef = 0.0f;
    } else if (currentErrorFPSSquare < 2.0f * currentVarianceFPS) {
        noiseCoef = (currentErrorFPSSquare - currentVarianceFPS) / currentVarianceFPS;
    }

    // The final normalized error is the the error to the FPS target, weighted by the noiseCoef, then normailzed by the target FPS.
    // it s also clamped in the [-1, 1] range
    auto error = noiseCoef * currentErrorFPS / targetFPS;
    error = glm::clamp(error, -1.0f, 1.0f);

    // Now we are getting into the P.I.D. controler code
    // retreive the history of pid error and integral
    auto previous_error = _pidHistory.x;
    auto previous_integral = _pidHistory.y;

    // The dt used for temporal values of the controller is the current realTimedelta
    // clamped to a reasonable granularity to make sure we are not over reacting
    auto dt = std::min(realTimeDelta, LOD_ADJUST_RUNNING_AVG_TIMESCALE);

    // Compute the current integral and clamp to avoid accumulation
    auto integral = previous_integral + error * dt;
    glm::clamp(integral, -1.0f, 1.0f);

    // Compute derivative
    // dt is never zero because realTimeDelta would have early exit above, but if it ever was let's zero the derivative term
    auto derivative = (dt <= 0.0f ? 0.0f : (error - previous_error) / dt);

    // remember history
    _pidHistory.x = error;
    _pidHistory.y = integral;
    _pidHistory.z = derivative;

    // Compute the output of the PID and record intermediate results for tuning
    _pidOutputs.x = _pidCoefs.x * error;        // Kp * error
    _pidOutputs.y = _pidCoefs.y * integral;     // Ki * integral 
    _pidOutputs.z = _pidCoefs.z * derivative;   // Kd * derivative

    auto output = _pidOutputs.x + _pidOutputs.y + _pidOutputs.z;
    _pidOutputs.w = output;

    // And now add the output of the controller to the LODAngle where we will guarantee it is in the proper range
    setLODAngleDeg(oldLODAngle + output);

    if (oldLODAngle != getLODAngleDeg()) {
        auto lodToolsDialog = DependencyManager::get<DialogsManager>()->getLodToolsDialog();
        if (lodToolsDialog) {
            lodToolsDialog->reloadSliders();
        }
    }
}

float LODManager::getLODHalfAngleTan() const {
    return tan(_lodHalfAngle);
}
float LODManager::getLODAngle() const {
    return 2.0f * _lodHalfAngle;
}
float LODManager::getLODAngleDeg() const {
    return glm::degrees(getLODAngle());
}

float LODManager::getVisibilityDistance() const {
    float systemDistance = getVisibilityDistanceFromHalfAngle(_lodHalfAngle);
    // Maintain behavior with deprecated _boundaryLevelAdjust property
    return systemDistance * powf(2.0f, _boundaryLevelAdjust);
}

void LODManager::setVisibilityDistance(float distance) {
    // Maintain behavior with deprecated _boundaryLevelAdjust property
    float userDistance = distance / powf(2.0f, _boundaryLevelAdjust);
    _lodHalfAngle = getHalfAngleFromVisibilityDistance(userDistance);
    saveSettings();
}

void LODManager::setLODAngleDeg(float lodAngle) {
    auto newLODAngleDeg = std::max(0.001f, std::min(lodAngle, 90.f));
    auto newLODHalfAngle = glm::radians(newLODAngleDeg * 0.5f);
    _lodHalfAngle = newLODHalfAngle;
    saveSettings();
}

void LODManager::setSmoothScale(float t) {
    _smoothScale = glm::max(1.0f, t);
}

float LODManager::getPidKp() const {
    return _pidCoefs.x;
}
float LODManager::getPidKi() const {
    return _pidCoefs.y;
}
float LODManager::getPidKd() const {
    return _pidCoefs.z;
}
float LODManager::getPidKv() const {
    return _pidCoefs.w;
}
void LODManager::setPidKp(float k) {
    _pidCoefs.x = k;
}
void LODManager::setPidKi(float k) {
    _pidCoefs.y = k;
}
void LODManager::setPidKd(float k) {
    _pidCoefs.z = k;
}
void LODManager::setPidKv(float t) {
    _pidCoefs.w = t;
}

float LODManager::getPidOp() const {
    return _pidOutputs.x;
}
float LODManager::getPidOi() const {
    return _pidOutputs.y;
}
float LODManager::getPidOd() const {
    return _pidOutputs.z;
}
float LODManager::getPidO() const {
    return _pidOutputs.w;
}

void LODManager::resetLODAdjust() {
}

void LODManager::setAutomaticLODAdjust(bool value) {
    std::lock_guard<std::mutex> { _automaticLODLock };
    _automaticLODAdjust = value;
    saveSettings();
    emit autoLODChanged();
}

bool LODManager::shouldRender(const RenderArgs* args, const AABox& bounds) {
    // To decide if the bound should be rendered or not at the specified Args->lodAngle,
    // we need to compute the apparent angle of the bound from the frustum origin,
    // and compare it against the lodAngle, if it is greater or equal we should render the content of that bound.
    // we abstract the bound as a sphere centered on the bound center and of radius half diagonal of the bound.

    // Instead of comparing  angles, we are comparing the tangent of the half angle which are more efficient to compute:
    // we are comparing the square of the half tangent apparent angle for the bound against the LODAngle Half tangent square
    // if smaller, the bound is too small and we should NOT render it, return true otherwise.

    // Tangent Adjacent side is eye to bound center vector length
    auto pos = args->getViewFrustum().getPosition() - bounds.calcCenter();
    auto halfTanAdjacentSq = glm::dot(pos, pos);

    // Tangent Opposite side is the half length of the dimensions vector of the bound
    auto dim = bounds.getDimensions();
    auto halfTanOppositeSq = 0.25f * glm::dot(dim, dim);

    // The test is:
    // isVisible = halfTanSq >= lodHalfTanSq = (halfTanOppositeSq / halfTanAdjacentSq) >= lodHalfTanSq
    // which we express as below to avoid division
    // (halfTanOppositeSq) >= lodHalfTanSq * halfTanAdjacentSq
    return (halfTanOppositeSq >= args->_lodAngleHalfTanSq * halfTanAdjacentSq);
};

void LODManager::setOctreeSizeScale(float sizeScale) {
    setVisibilityDistance(sizeScale / TREE_SCALE);
}

float LODManager::getOctreeSizeScale() const {
    return getVisibilityDistance() * TREE_SCALE;
}

void LODManager::setBoundaryLevelAdjust(int boundaryLevelAdjust) {
    _boundaryLevelAdjust = boundaryLevelAdjust;
}

QString LODManager::getLODFeedbackText() {
    // determine granularity feedback
    int boundaryLevelAdjust = getBoundaryLevelAdjust();
    QString granularityFeedback;
    switch (boundaryLevelAdjust) {
    case 0: {
        granularityFeedback = QString(".");
    } break;
    case 1: {
        granularityFeedback = QString(" at half of standard granularity.");
    } break;
    case 2: {
        granularityFeedback = QString(" at a third of standard granularity.");
    } break;
    default: {
        granularityFeedback = QString(" at 1/%1th of standard granularity.").arg(boundaryLevelAdjust + 1);
    } break;
    }
    // distance feedback
    float visibilityDistance = getVisibilityDistance();
    float relativeToDefault = visibilityDistance / DEFAULT_VISIBILITY_DISTANCE_FOR_UNIT_ELEMENT;
    int relativeToTwentyTwenty = 20 / relativeToDefault;

    QString result;
    if (relativeToTwentyTwenty < 1) {
        result = QString("%2 times further than average vision%3").arg(relativeToDefault, 0, 'f', 3).arg(granularityFeedback);
    } else if (relativeToDefault > 1.01f) {
        result = QString("20:%1 or %2 times further than average vision%3").arg(relativeToTwentyTwenty).arg(relativeToDefault, 0, 'f', 2).arg(granularityFeedback);
    } else if (relativeToDefault > 0.99f) {
        result = QString("20:20 or the default distance for average vision%1").arg(granularityFeedback);
    } else if (relativeToDefault > 0.01f) {
        result = QString("20:%1 or %2 of default distance for average vision%3").arg(relativeToTwentyTwenty).arg(relativeToDefault, 0, 'f', 3).arg(granularityFeedback);
    } else {
        result = QString("%2 of default distance for average vision%3").arg(relativeToDefault, 0, 'f', 3).arg(granularityFeedback);
    }
    return result;
}

void LODManager::loadSettings() {
    auto desktopQuality = static_cast<WorldDetailQuality>(desktopWorldDetailQuality.get());
    auto hmdQuality = static_cast<WorldDetailQuality>(hmdWorldDetailQuality.get());

    Setting::Handle<bool> firstRun{ Settings::firstRun, true };
    if (qApp->property(hifi::properties::OCULUS_STORE).toBool() && firstRun.get()) {
        hmdQuality = WORLD_DETAIL_HIGH;
    }
    
    _automaticLODAdjust = automaticLODAdjust.get();
    _lodHalfAngle = lodHalfAngle.get();

    setWorldDetailQuality(desktopQuality, false);
    setWorldDetailQuality(hmdQuality, true);
}

void LODManager::saveSettings() {
    automaticLODAdjust.set((bool)_automaticLODAdjust);
    desktopWorldDetailQuality.set((int)_desktopWorldDetailQuality);
    hmdWorldDetailQuality.set((int)_hmdWorldDetailQuality);
    lodHalfAngle.set((float)_lodHalfAngle);
}

const float MIN_DECREASE_FPS = 0.5f;

void LODManager::setDesktopLODTargetFPS(float fps) {
    if (fps < MIN_DECREASE_FPS) {
        // avoid divide by zero
        fps = MIN_DECREASE_FPS;
    }
    _desktopTargetFPS = fps;
}

float LODManager::getDesktopLODTargetFPS() const {
    return _desktopTargetFPS;
}

void LODManager::setHMDLODTargetFPS(float fps) {
    if (fps < MIN_DECREASE_FPS) {
        // avoid divide by zero
        fps = MIN_DECREASE_FPS;
    }
    _hmdTargetFPS = fps;
}

float LODManager::getHMDLODTargetFPS() const {
    return _hmdTargetFPS;
}

float LODManager::getLODTargetFPS() const {

    // Use the current refresh rate as the recommended rate target used to cap the LOD manager control value.
    // When focused, Use the Focus Inactive as the targget LOD to void abrupt changes from the lod controller.
    auto& refreshRateManager = qApp->getRefreshRateManager();
    auto refreshRateRegime = refreshRateManager.getRefreshRateRegime();
    auto refreshRateProfile = refreshRateManager.getRefreshRateProfile();
    auto refreshRateUXMode = refreshRateManager.getUXMode();
    auto refreshRateFPS = refreshRateManager.getActiveRefreshRate();
    if (refreshRateRegime == RefreshRateManager::RefreshRateRegime::FOCUS_ACTIVE) {
        refreshRateFPS = refreshRateManager.queryRefreshRateTarget(refreshRateProfile, RefreshRateManager::RefreshRateRegime::FOCUS_INACTIVE, refreshRateUXMode);
    }

    auto lodTargetFPS = getDesktopLODTargetFPS();
    if (qApp->isHMDMode()) {
        lodTargetFPS = getHMDLODTargetFPS();
    }
    
    // if RefreshRate is slower than LOD target then it becomes the true LOD target
    if (lodTargetFPS > refreshRateFPS) {
        return refreshRateFPS;
    } else {
        return lodTargetFPS;
    }
}

void LODManager::setWorldDetailQuality(WorldDetailQuality quality, bool isHMDMode) {
    float desiredFPS = isHMDMode ? QUALITY_TO_FPS_HMD[quality] : QUALITY_TO_FPS_DESKTOP[quality];
    if (isHMDMode) {
        _hmdWorldDetailQuality = quality;
        setHMDLODTargetFPS(desiredFPS);
    } else {
        _desktopWorldDetailQuality = quality;
        setDesktopLODTargetFPS(desiredFPS);
    }
}
    
void LODManager::setWorldDetailQuality(WorldDetailQuality quality) {
    setWorldDetailQuality(quality, qApp->isHMDMode());
    saveSettings();
    emit worldDetailQualityChanged();
}

WorldDetailQuality LODManager::getWorldDetailQuality() const {
    return qApp->isHMDMode() ? _hmdWorldDetailQuality : _desktopWorldDetailQuality;
}

ScriptValuePointer worldDetailQualityToScriptValue(ScriptEngine* engine, const WorldDetailQuality& worldDetailQuality) {
    return engine->newValue(worldDetailQuality);
}

void worldDetailQualityFromScriptValue(const ScriptValuePointer& object, WorldDetailQuality& worldDetailQuality) {
    worldDetailQuality = 
        static_cast<WorldDetailQuality>(std::min(std::max(object->toInt32(), (int)WORLD_DETAIL_LOW), (int)WORLD_DETAIL_HIGH));
}

void LODManager::setLODQualityLevel(float quality) {
    _lodQualityLevel = quality;
}

float LODManager::getLODQualityLevel() const {
    return _lodQualityLevel;
}
