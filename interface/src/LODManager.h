//
//  LODManager.h
//  interface/src/LODManager.h
//
//  Created by Clement on 1/16/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_LODManager_h
#define hifi_LODManager_h

#include <mutex>
#include <QtCore/QSharedPointer>

#include <DependencyManager.h>
#include <NumericalConstants.h>
#include <OctreeConstants.h>
#include <OctreeUtils.h>
#include <PIDController.h>
#include <SimpleMovingAverage.h>
#include <render/Args.h>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * <p>The world detail quality rendered.</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>Low world detail quality.</td></tr>
 *     <tr><td><code>1</code></td><td>Medium world detail quality.</td></tr>
 *     <tr><td><code>2</code></td><td>High world detail quality.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {number} LODManager.WorldDetailQuality
 */
enum WorldDetailQuality {
    WORLD_DETAIL_LOW = 0,
    WORLD_DETAIL_MEDIUM,
    WORLD_DETAIL_HIGH
};
Q_DECLARE_METATYPE(WorldDetailQuality);

const bool DEFAULT_LOD_AUTO_ADJUST = false; // true for auto, false for manual.

#ifdef Q_OS_ANDROID
const float DEFAULT_LOD_QUALITY_LEVEL = 0.2f; // default quality level setting is High (lower framerate)
#else
const float DEFAULT_LOD_QUALITY_LEVEL = 0.5f; // default quality level setting is Mid
#endif

#ifdef Q_OS_ANDROID
const WorldDetailQuality DEFAULT_WORLD_DETAIL_QUALITY = WORLD_DETAIL_LOW;
const std::vector<float> QUALITY_TO_FPS_DESKTOP = { 60.0f, 30.0f, 15.0f };
const std::vector<float> QUALITY_TO_FPS_HMD = { 25.0f, 16.0f, 10.0f };
#else
const WorldDetailQuality DEFAULT_WORLD_DETAIL_QUALITY = WORLD_DETAIL_MEDIUM;
const std::vector<float> QUALITY_TO_FPS_DESKTOP = { 60.0f, 30.0f, 15.0f };
const std::vector<float> QUALITY_TO_FPS_HMD = { 90.0f, 45.0f, 22.5f };
#endif

const float LOD_OFFSET_FPS = 5.0f; // offset of FPS to add for computing the target framerate

class AABox;


/*@jsdoc
 * The <code>LODManager</code> API manages the Level of Detail displayed in Interface. If the LOD is being automatically 
 * adjusted, the LOD is decreased if the measured frame rate is lower than the target FPS, and increased if the measured frame 
 * rate is greater than the target FPS.
 *
 * @namespace LODManager
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {LODManager.WorldDetailQuality} worldDetailQuality - The quality of the rendered world detail.
 *     <p>Setting this value updates the current desktop or HMD target LOD FPS.</p>
 * @property {number} lodQualityLevel - <em>Not used.</em>
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 * @property {boolean} automaticLODAdjust - <code>true</code> to automatically adjust the LOD, <code>false</code> to manually 
 *     adjust it.
 *
 * @property {number} engineRunTime - The time spent in the "render" thread to produce the most recent frame, in ms.
 *     <em>Read-only.</em>
 * @property {number} batchTime - The time spent in the "present" thread processing the batches of the most recent frame, in ms.
 *     <em>Read-only.</em>
 * @property {number} presentTime - The time spent in the "present" thread between the last buffer swap, i.e., the total time 
 *     to submit the most recent frame, in ms.
 *     <em>Read-only.</em>
 * @property {number} gpuTime - The time spent in the GPU executing the most recent frame, in ms.
 *     <em>Read-only.</em>
 *
 * @property {number} nowRenderTime - The current, instantaneous time spend to produce frames, in ms. This is the worst of 
 *     <code>engineRunTime</code>, <code>batchTime</code>, <code>presentTime</code>, and <code>gpuTime</code>.
 *     <em>Read-only.</em>
 * @property {number} nowRenderFPS - The current, instantaneous frame rate, in Hz. 
 *     <em>Read-only.</em>
 *
 * @property {number} smoothScale - The amount of smoothing applied to calculate <code>smoothRenderTime</code> and 
 *     <code>smoothRenderFPS</code>.
 * @property {number} smoothRenderTime - The average time spend to produce frames, in ms.
 *     <em>Read-only.</em>
 * @property {number} smoothRenderFPS - The average frame rate, in Hz. 
 *     <em>Read-only.</em>
 *
 * @property {number} lodTargetFPS - The target LOD FPS per the current desktop or HMD display mode, capped by the target 
 *     refresh rate set by the {@link Performance} API.
 *     <em>Read-only.</em>

 * @property {number} lodAngleDeg - The minimum angular dimension (relative to the camera position) of an entity in order for 
 *     it to be rendered, in degrees. The angular dimension is calculated as a sphere of radius half the diagonal of the 
 *     entity's AA box.
 *
 * @property {number} pidKp - <em>Not used.</em>
 * @property {number} pidKi - <em>Not used.</em>
 * @property {number} pidKd - <em>Not used.</em>
 * @property {number} pidKv - <em>Not used.</em>
 * @property {number} pidOp - <em>Not used.</em> <em>Read-only.</em>
 * @property {number} pidOi - <em>Not used.</em> <em>Read-only.</em>
 * @property {number} pidOd - <em>Not used.</em> <em>Read-only.</em>
 * @property {number} pidO - <em>Not used.</em> <em>Read-only.</em>
 */
class LODManager : public QObject, public Dependency {
    Q_OBJECT
        SINGLETON_DEPENDENCY

        Q_PROPERTY(WorldDetailQuality worldDetailQuality READ getWorldDetailQuality WRITE setWorldDetailQuality 
            NOTIFY worldDetailQualityChanged)

        Q_PROPERTY(float lodQualityLevel READ getLODQualityLevel WRITE setLODQualityLevel NOTIFY lodQualityLevelChanged)

        Q_PROPERTY(bool automaticLODAdjust READ getAutomaticLODAdjust WRITE setAutomaticLODAdjust NOTIFY autoLODChanged)

        Q_PROPERTY(float presentTime READ getPresentTime)
        Q_PROPERTY(float engineRunTime READ getEngineRunTime)
        Q_PROPERTY(float batchTime READ getBatchTime)
        Q_PROPERTY(float gpuTime READ getGPUTime)

        Q_PROPERTY(float nowRenderTime READ getNowRenderTime)
        Q_PROPERTY(float nowRenderFPS READ getNowRenderFPS)

        Q_PROPERTY(float smoothScale READ getSmoothScale WRITE setSmoothScale)
        Q_PROPERTY(float smoothRenderTime READ getSmoothRenderTime)
        Q_PROPERTY(float smoothRenderFPS READ getSmoothRenderFPS)

        Q_PROPERTY(float lodTargetFPS READ getLODTargetFPS)

        Q_PROPERTY(float lodAngleDeg READ getLODAngleDeg WRITE setLODAngleDeg)

        Q_PROPERTY(float pidKp READ getPidKp WRITE setPidKp)
        Q_PROPERTY(float pidKi READ getPidKi WRITE setPidKi)
        Q_PROPERTY(float pidKd READ getPidKd WRITE setPidKd)
        Q_PROPERTY(float pidKv READ getPidKv WRITE setPidKv)

        Q_PROPERTY(float pidOp READ getPidOp)
        Q_PROPERTY(float pidOi READ getPidOi)
        Q_PROPERTY(float pidOd READ getPidOd)
        Q_PROPERTY(float pidO READ getPidO)

public:

    /*@jsdoc
     * Sets whether the LOD should be automatically adjusted.
     * @function LODManager.setAutomaticLODAdjust
     * @param {boolean} value - <code>true</code> to automatically adjust the LOD, <code>false</code> to manually adjust it.
     */
    Q_INVOKABLE void setAutomaticLODAdjust(bool value);

    /*@jsdoc
     * Gets whether the LOD is being automatically adjusted.
     * @function LODManager.getAutomaticLODAdjust
     * @returns {boolean} <code>true</code> if the LOD is being automatically adjusted, <code>false</code> if it is being 
     *     manually adjusted.
     */
    Q_INVOKABLE bool getAutomaticLODAdjust() const { return _automaticLODAdjust; }

    /*@jsdoc
     * Sets the target desktop LOD FPS.
     * @function LODManager.setDesktopLODTargetFPS
     * @param {number} value - The target desktop LOD FPS, in Hz.
     */
    Q_INVOKABLE void setDesktopLODTargetFPS(float value);

    /*@jsdoc
     * Gets the target desktop LOD FPS.
     * @function LODManager.getDesktopLODTargetFPS
     * @returns {number}  The target desktop LOD FPS, in Hz.
     */

    Q_INVOKABLE float getDesktopLODTargetFPS() const;

    /*@jsdoc
     * Sets the target HMD LOD FPS.
     * @function LODManager.setHMDLODTargetFPS
     * @param {number} value - The target HMD LOD FPS, in Hz.
     */

    Q_INVOKABLE void setHMDLODTargetFPS(float value);

    /*@jsdoc
     * Gets the target HMD LOD FPS.
     * The target FPS in HMD mode. The LOD is adjusted to ...
     * @function LODManager.getHMDLODTargetFPS
     * @returns {number} The target HMD LOD FPS, in Hz.
     */
    Q_INVOKABLE float getHMDLODTargetFPS() const;


    // User Tweakable LOD Items

    /*@jsdoc
     * Gets a text description of the current level of detail rendered.
     * @function LODManager.getLODFeedbackText
     * @returns {string} A text description of the current level of detail rendered.
     * @example <caption>Report the current level of detail rendered.</caption>
     * print("You can currently see: " + LODManager.getLODFeedbackText());
     */
    Q_INVOKABLE QString getLODFeedbackText();

    /*@jsdoc
     * @function LODManager.setOctreeSizeScale
     * @param {number} sizeScale - The octree size scale.
     * @deprecated This function is deprecated and will be removed. Use the <code>lodAngleDeg</code> property instead.
     */
    Q_INVOKABLE void setOctreeSizeScale(float sizeScale);

    /*@jsdoc
     * @function LODManager.getOctreeSizeScale
     * @returns {number} The octree size scale.
     * @deprecated This function is deprecated and will be removed. Use the <code>lodAngleDeg</code> property instead.
     */
    Q_INVOKABLE float getOctreeSizeScale() const;

    /*@jsdoc
     * @function LODManager.setBoundaryLevelAdjust
     * @param {number} boundaryLevelAdjust - The boundary level adjust factor.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void setBoundaryLevelAdjust(int boundaryLevelAdjust);

    /*@jsdoc
     * @function LODManager.getBoundaryLevelAdjust
     * @returns {number} The boundary level adjust factor.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE int getBoundaryLevelAdjust() const { return _boundaryLevelAdjust; }

    /*@jsdoc
     * The target LOD FPS per the current desktop or HMD display mode, capped by the target refresh rate.
     * @function LODManager.getLODTargetFPS
     * @returns {number} The target LOD FPS, in Hz.
     */
    Q_INVOKABLE float getLODTargetFPS() const;

    float getPresentTime() const { return _presentTime; }
    float getEngineRunTime() const { return _engineRunTime; }
    float getBatchTime() const { return _batchTime; }
    float getGPUTime() const { return _gpuTime; }

    static bool shouldRender(const RenderArgs* args, const AABox& bounds);
    void setRenderTimes(float presentTime, float engineRunTime, float batchTime, float gpuTime);
    void autoAdjustLOD(float realTimeDelta);

    void loadSettings();
    void saveSettings();
    void resetLODAdjust();

    float getNowRenderTime() const { return _nowRenderTime; };
    float getNowRenderFPS() const { return (_nowRenderTime > 0.0f ? (float)MSECS_PER_SECOND / _nowRenderTime : 0.0f); };

    void setSmoothScale(float t);
    float getSmoothScale() const { return _smoothScale; }

    float getSmoothRenderTime() const { return _smoothRenderTime; };
    float getSmoothRenderFPS() const { return (_smoothRenderTime > 0.0f ? (float)MSECS_PER_SECOND / _smoothRenderTime : 0.0f); };

    void setWorldDetailQuality(WorldDetailQuality quality);
    WorldDetailQuality getWorldDetailQuality() const;

    void setLODQualityLevel(float quality);
    float getLODQualityLevel() const;

    float getLODAngleDeg() const;
    void setLODAngleDeg(float lodAngle);
    float getLODHalfAngleTan() const;
    float getLODAngle() const;
    float getVisibilityDistance() const;
    void setVisibilityDistance(float distance);

    float getPidKp() const;
    float getPidKi() const;
    float getPidKd() const;
    float getPidKv() const;
    void setPidKp(float k);
    void setPidKi(float k);
    void setPidKd(float k);
    void setPidKv(float t);

    float getPidOp() const;
    float getPidOi() const;
    float getPidOd() const;
    float getPidO() const;

signals:

    /*@jsdoc
     * <em>Not triggered.</em>
     * @function LODManager.LODIncreased
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void LODIncreased();

    /*@jsdoc
     * <em>Not triggered.</em>
     * @function LODManager.LODDecreased
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void LODDecreased();

    /*@jsdoc
     * Triggered when whether or not the LOD is being automatically adjusted changes. 
     * @function LODManager.autoLODChanged
     * @returns {Signal}
     */
    void autoLODChanged();

    /*@jsdoc
     * Triggered when the <code>lodQualityLevel</code> property value changes.
     * @function LODManager.lodQualityLevelChanged
     * @returns {Signal}
     * @deprecated This signal is deprecated and will be removed.
     */
    void lodQualityLevelChanged();

    /*@jsdoc
     * Triggered when the world detail quality changes.
     * @function LODManager.worldDetailQualityChanged
     * @returns {Signal}
     */
    void worldDetailQualityChanged();

private:
    LODManager();

    void setWorldDetailQuality(WorldDetailQuality quality, bool isHMDMode);

    std::mutex _automaticLODLock;
    bool _automaticLODAdjust = DEFAULT_LOD_AUTO_ADJUST;

    float _presentTime{ 0.0f }; // msec
    float _engineRunTime{ 0.0f }; // msec
    float _batchTime{ 0.0f }; // msec
    float _gpuTime{ 0.0f }; // msec

    float _nowRenderTime{ 0.0f }; // msec
    float _smoothScale{ 10.0f }; // smooth is evaluated over 10 times longer than now
    float _smoothRenderTime{ 0.0f }; // msec

    float _lodQualityLevel{ DEFAULT_LOD_QUALITY_LEVEL };

    WorldDetailQuality _desktopWorldDetailQuality { DEFAULT_WORLD_DETAIL_QUALITY };
    WorldDetailQuality _hmdWorldDetailQuality { DEFAULT_WORLD_DETAIL_QUALITY };

    float _desktopTargetFPS { QUALITY_TO_FPS_DESKTOP[_desktopWorldDetailQuality] };
    float _hmdTargetFPS { QUALITY_TO_FPS_HMD[_hmdWorldDetailQuality] };

    float _lodHalfAngle = getHalfAngleFromVisibilityDistance(DEFAULT_VISIBILITY_DISTANCE_FOR_UNIT_ELEMENT);
    int _boundaryLevelAdjust = 0;

    glm::vec4 _pidCoefs{ 1.0f, 0.0f, 0.0f, 1.0f }; // Kp, Ki, Kd, Kv
    glm::vec4 _pidHistory{ 0.0f };
    glm::vec4 _pidOutputs{ 0.0f };
};

ScriptValuePointer worldDetailQualityToScriptValue(ScriptEngine* engine, const WorldDetailQuality& worldDetailQuality);
void worldDetailQualityFromScriptValue(const ScriptValuePointer& object, WorldDetailQuality& worldDetailQuality);

#endif // hifi_LODManager_h
