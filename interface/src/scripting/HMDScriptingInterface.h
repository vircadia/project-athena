//
//  HMDScriptingInterface.h
//  interface/src/scripting
//
//  Created by Thijs Wenker on 1/12/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_HMDScriptingInterface_h
#define hifi_HMDScriptingInterface_h

#include <atomic>

#include <GLMHelpers.h>
#include <DependencyManager.h>
#include <display-plugins/AbstractHMDScriptingInterface.h>

#include <QReadWriteLock>
#include <QtCore/QSharedPointer>

class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>HMD</code> API provides access to the HMD used in VR display mode.
 *
 * @namespace HMD
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {Vec3} position - The position of the HMD if currently in VR display mode, otherwise
 *     {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
 * @property {Quat} orientation - The orientation of the HMD if currently in VR display mode, otherwise 
 *     {@link Quat(0)|Quat.IDENTITY}. <em>Read-only.</em>
 * @property {boolean} active - <code>true</code> if the display mode is HMD, otherwise <code>false</code>. <em>Read-only.</em>
 * @property {boolean} mounted - <code>true</code> if currently in VR display mode and the HMD is being worn, otherwise
 *     <code>false</code>. <em>Read-only.</em>
 *
 * @property {number} playerHeight - The real-world height of the user. <em>Read-only.</em> <em>Currently always returns a
 *     value of <code>1.755</code>.</em>
 * @property {number} eyeHeight -  The real-world height of the user's eyes. <em>Read-only.</em> <em>Currently always returns a
 *     value of <code>1.655</code>.</em>
 * @property {number} ipd - The inter-pupillary distance (distance between eyes) of the user, used for rendering. Defaults to
 *     the human average of <code>0.064</code> unless set by the HMD. <em>Read-only.</em>
 * @property {number} ipdScale=1.0 - A scale factor applied to the <code>ipd</code> property value.
 *
 * @property {boolean} showTablet - <code>true</code> if the tablet is being displayed, <code>false</code> otherwise.
 *     <em>Read-only.</em>
 * @property {boolean} tabletContextualMode - <code>true</code> if the tablet has been opened in contextual mode, otherwise 
 *     <code>false</code>. In contextual mode, the tablet has been opened at a specific world position and orientation rather 
 *     than at a position and orientation relative to the user. <em>Read-only.</em>
 * @property {Uuid} tabletID - The UUID of the tablet body model entity.
 * @property {Uuid} tabletScreenID - The UUID of the tablet's screen entity.
 * @property {Uuid} homeButtonID - The UUID of the tablet's "home" button entity.
 * @property {Uuid} homeButtonHighlightID - The UUID of the tablet's "home" button highlight entity.
 * @property {Uuid} miniTabletID - The UUID of the mini tablet's body model entity. <code>null</code> if not in HMD mode.
 * @property {Uuid} miniTabletScreenID - The UUID of the mini tablet's screen entity. <code>null</code> if not in HMD mode.
 * @property {number} miniTabletHand - The hand that the mini tablet is displayed on: <code>0</code> for left hand, 
 *     <code>1</code> for right hand, <code>-1</code> if not in HMD mode.
 * @property {boolean} miniTabletEnabled=true - <code>true</code> if the mini tablet is enabled to be displayed, otherwise 
 *     <code>false</code>.
 * @property {Rect} playArea=0,0,0,0 - The size and position of the HMD play area in sensor coordinates. <em>Read-only.</em>
 * @property {Vec3[]} sensorPositions=[]] - The positions of the VR system sensors in sensor coordinates. <em>Read-only.</em>
 *
 * @property {number} visionSqueezeRatioX=0.0 - The amount of vision squeeze for the x-axis when moving, range <code>0.0</code> 
 *     &ndash; <code>1.0</code>.
 * @property {number} visionSqueezeRatioY=0.0 - The amount of vision squeeze for the y-axis when moving, range <code>0.0</code> 
 *     &ndash; <code>1.0</code>. 
 * @property {number} visionSqueezeTurningXFactor=0.51 - The additional amount of vision squeeze for the x-axis when turning,
 *     range <code>0.0</code> &ndash; <code>1.0</code>.
 * @property {number} visionSqueezeTurningYFactor=0.36 - <em>Currently unused.</em>
 * @property {number} visionSqueezeUnSqueezeDelay=0.2 - The delay in undoing the vision squeeze effect after motion stops, in
 *     seconds.
 * @property {number} visionSqueezeUnSqueezeSpeed=3.0 - How quickly the vision squeeze effect fades, once 
 *     <code>visionSqueezeUnSqueezeDelay</code> has passed.
 * @property {number} visionSqueezeTransition=0.25 - How tightly vision is squeezed, range <code>0.01</code> &ndash; 
 *     <code>0.7</code>.
 * @property {number} visionSqueezePerEye=1 - <code>1</code> if each eye gets a tube to see through, <code>0</code> if the face 
 *     gets a tube.
 * @property {number} visionSqueezeGroundPlaneY=0.0 - Adjusts how far below the camera the vision squeeze grid is displayed at.
 * @property {number} visionSqueezeSpotlightSize=6.0 - The diameter of the circle of vision squeeze grid that is illuminated 
 *     around the camera.
 */
class HMDScriptingInterface : public AbstractHMDScriptingInterface, public Dependency {
    Q_OBJECT
    Q_PROPERTY(glm::vec3 position READ getPosition)
    Q_PROPERTY(glm::quat orientation READ getOrientation)
    Q_PROPERTY(bool showTablet READ getShouldShowTablet)
    Q_PROPERTY(bool tabletContextualMode READ getTabletContextualMode)
    Q_PROPERTY(QUuid tabletID READ getCurrentTabletFrameID WRITE setCurrentTabletFrameID)
    Q_PROPERTY(QUuid homeButtonID READ getCurrentHomeButtonID WRITE setCurrentHomeButtonID)
    Q_PROPERTY(QUuid tabletScreenID READ getCurrentTabletScreenID WRITE setCurrentTabletScreenID)
    Q_PROPERTY(QUuid homeButtonHighlightID READ getCurrentHomeButtonHighlightID WRITE setCurrentHomeButtonHighlightID)
    Q_PROPERTY(QUuid miniTabletID READ getCurrentMiniTabletID WRITE setCurrentMiniTabletID)
    Q_PROPERTY(QUuid miniTabletScreenID READ getCurrentMiniTabletScreenID WRITE setCurrentMiniTabletScreenID)
    Q_PROPERTY(int miniTabletHand READ getCurrentMiniTabletHand WRITE setCurrentMiniTabletHand)
    Q_PROPERTY(bool miniTabletEnabled READ getMiniTabletEnabled WRITE setMiniTabletEnabled)
    Q_PROPERTY(QVariant playArea READ getPlayAreaRect);
    Q_PROPERTY(QVector<glm::vec3> sensorPositions READ getSensorPositions);

    Q_PROPERTY(float visionSqueezeRatioX READ getVisionSqueezeRatioX WRITE setVisionSqueezeRatioX);
    Q_PROPERTY(float visionSqueezeRatioY READ getVisionSqueezeRatioY WRITE setVisionSqueezeRatioY);
    Q_PROPERTY(float visionSqueezeUnSqueezeDelay READ getVisionSqueezeUnSqueezeDelay WRITE setVisionSqueezeUnSqueezeDelay);
    Q_PROPERTY(float visionSqueezeUnSqueezeSpeed READ getVisionSqueezeUnSqueezeSpeed WRITE setVisionSqueezeUnSqueezeSpeed);
    Q_PROPERTY(float visionSqueezeTransition READ getVisionSqueezeTransition WRITE setVisionSqueezeTransition);
    Q_PROPERTY(int visionSqueezePerEye READ getVisionSqueezePerEye WRITE setVisionSqueezePerEye);
    Q_PROPERTY(float visionSqueezeGroundPlaneY READ getVisionSqueezeGroundPlaneY WRITE setVisionSqueezeGroundPlaneY);
    Q_PROPERTY(float visionSqueezeSpotlightSize READ getVisionSqueezeSpotlightSize WRITE setVisionSqueezeSpotlightSize);
    Q_PROPERTY(float visionSqueezeTurningXFactor READ getVisionSqueezeTurningXFactor WRITE setVisionSqueezeTurningXFactor);
    Q_PROPERTY(float visionSqueezeTurningYFactor READ getVisionSqueezeTurningYFactor WRITE setVisionSqueezeTurningYFactor);

public:

    /*@jsdoc
     * Calculates the intersection of a ray with the HUD overlay.
     * @function HMD.calculateRayUICollisionPoint
     * @param {Vec3} position - The origin of the ray.
     * @param {Vec3} direction - The direction of the ray.
     * @returns {Vec3} The point of intersection with the HUD overlay if it intersects, otherwise {@link Vec3(0)|Vec3.ZERO}.
     * @example <caption>Draw a square on the HUD overlay in the direction you're looking.</caption>
     * var hudIntersection = HMD.calculateRayUICollisionPoint(MyAvatar.getHeadPosition(),
     *     Quat.getForward(MyAvatar.headOrientation));
     * var hudPoint = HMD.overlayFromWorldPoint(hudIntersection);
     *
     * var DIMENSIONS = { x: 50, y: 50 };
     * var square = Overlays.addOverlay("rectangle", {
     *     x: hudPoint.x - DIMENSIONS.x / 2,
     *     y: hudPoint.y - DIMENSIONS.y / 2,
     *     width: DIMENSIONS.x,
     *     height: DIMENSIONS.y,
     *     color: { red: 255, green: 0, blue: 0 }
     * });
     *
     * Script.scriptEnding.connect(function () {
     *     Overlays.deleteOverlay(square);
     * });
     */
    Q_INVOKABLE glm::vec3 calculateRayUICollisionPoint(const glm::vec3& position, const glm::vec3& direction) const;

    glm::vec3 calculateParabolaUICollisionPoint(const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& acceleration, float& parabolicDistance) const;

    /*@jsdoc
     * Gets the 2D HUD overlay coordinates of a 3D point on the HUD overlay.
     * 2D HUD overlay coordinates are pixels with the origin at the top left of the overlay.
     * @function HMD.overlayFromWorldPoint
     * @param {Vec3} position - The point on the HUD overlay in world coordinates.
     * @returns {Vec2} The point on the HUD overlay in HUD coordinates.
     * @example <caption>Draw a square on the HUD overlay in the direction you're looking.</caption>
     * var hudIntersection = HMD.calculateRayUICollisionPoint(MyAvatar.getHeadPosition(),
     *     Quat.getForward(MyAvatar.headOrientation));
     * var hudPoint = HMD.overlayFromWorldPoint(hudIntersection);
     *
     * var DIMENSIONS = { x: 50, y: 50 };
     * var square = Overlays.addOverlay("rectangle", {
     *     x: hudPoint.x - DIMENSIONS.x / 2,
     *     y: hudPoint.y - DIMENSIONS.y / 2,
     *     width: DIMENSIONS.x,
     *     height: DIMENSIONS.y,
     *     color: { red: 255, green: 0, blue: 0 }
     * });
     *
     * Script.scriptEnding.connect(function () {
     *     Overlays.deleteOverlay(square);
     * });
     */
    Q_INVOKABLE glm::vec2 overlayFromWorldPoint(const glm::vec3& position) const;

    /*@jsdoc
     * Gets the 3D world coordinates of a 2D point on the HUD overlay.
     * 2D HUD overlay coordinates are pixels with the origin at the top left of the overlay.
     * @function HMD.worldPointFromOverlay
     * @param {Vec2} coordinates - The point on the HUD overlay in HUD coordinates.
     * @returns {Vec3} The point on the HUD overlay in world coordinates.
     */
    Q_INVOKABLE glm::vec3 worldPointFromOverlay(const glm::vec2& overlay) const;

    /*@jsdoc
     * Gets the 2D point on the HUD overlay represented by given spherical coordinates. 
     * 2D HUD overlay coordinates are pixels with the origin at the top left of the overlay.
     * Spherical coordinates are polar coordinates in radians with <code>{ x: 0, y: 0 }</code> being the center of the HUD 
     * overlay.
     * @function HMD.sphericalToOverlay
     * @param {Vec2} sphericalPos - The point on the HUD overlay in spherical coordinates.
     * @returns {Vec2} The point on the HUD overlay in HUD coordinates.
     */
    Q_INVOKABLE glm::vec2 sphericalToOverlay(const glm::vec2 & sphericalPos) const;

    /*@jsdoc
     * Gets the spherical coordinates of a 2D point on the HUD overlay.
     * 2D HUD overlay coordinates are pixels with the origin at the top left of the overlay.
     * Spherical coordinates are polar coordinates in radians with <code>{ x: 0, y: 0 }</code> being the center of the HUD
     * overlay.
     * @function HMD.overlayToSpherical
     * @param {Vec2} overlayPos - The point on the HUD overlay in HUD coordinates.
     * @returns {Vec2} The point on the HUD overlay in spherical coordinates.
     */
    Q_INVOKABLE glm::vec2 overlayToSpherical(const glm::vec2 & overlayPos) const;

    /*@jsdoc
     * Recenters the HMD HUD to the current HMD position and orientation.
     * @function HMD.centerUI
     */
    Q_INVOKABLE void centerUI();


    /*@jsdoc
     * Gets the name of the HMD audio input device.
     * @function HMD.preferredAudioInput
     * @returns {string} The name of the HMD audio input device if in HMD mode, otherwise an empty string.
     */
    Q_INVOKABLE QString preferredAudioInput() const;

    /*@jsdoc
     * Gets the name of the HMD audio output device.
     * @function HMD.preferredAudioOutput
     * @returns {string} The name of the HMD audio output device if in HMD mode, otherwise an empty string.
     */
    Q_INVOKABLE QString preferredAudioOutput() const;


    /*@jsdoc
     * Checks whether there is an HMD available.
     * @function HMD.isHMDAvailable
     * @param {string} [name=""] - The name of the HMD to check for, e.g., <code>"Oculus Rift"</code>. The name is the same as 
     *     may be displayed in Interface's "Display" menu. If no name is specified, then any HMD matches.
     * @returns {boolean} <code>true</code> if an HMD of the specified <code>name</code> is available, otherwise 
     *     <code>false</code>.
     * @example <caption>Report on HMD availability.</caption>
     * print("Is any HMD available: " + HMD.isHMDAvailable());
     * print("Is an Oculus Rift HMD available: " + HMD.isHMDAvailable("Oculus Rift"));
     * print("Is a Vive HMD available: " + HMD.isHMDAvailable("OpenVR (Vive)"));
     */
    Q_INVOKABLE bool isHMDAvailable(const QString& name = "");

    /*@jsdoc
     * Checks whether there is an HMD head controller available.
     * @function HMD.isHeadControllerAvailable
     * @param {string} [name=""] - The name of the HMD head controller to check for, e.g., <code>"Oculus"</code>. If no name is 
     *     specified, then any HMD head controller matches.
     * @returns {boolean} <code>true</code> if an HMD head controller of the specified <code>name</code> is available, 
     *     otherwise <code>false</code>.
     * @example <caption>Report HMD head controller availability.</caption>
     * print("Is any HMD head controller available: " + HMD.isHeadControllerAvailable());
     * print("Is an Oculus head controller available: " + HMD.isHeadControllerAvailable("Oculus"));
     * print("Is a Vive head controller available: " + HMD.isHeadControllerAvailable("OpenVR"));
     */
    Q_INVOKABLE bool isHeadControllerAvailable(const QString& name = "");

    /*@jsdoc
     * Checks whether there are HMD hand controllers available.
     * @function HMD.isHandControllerAvailable
     * @param {string} [name=""] - The name of the HMD hand controller to check for, e.g., <code>"Oculus"</code>. If no name is 
     *     specified, then any HMD hand controller matches.
     * @returns {boolean} <code>true</code> if an HMD hand controller of the specified <code>name</code> is available, 
     *     otherwise <code>false</code>.
     * @example <caption>Report HMD hand controller availability.</caption>
     * print("Are any HMD hand controllers available: " + HMD.isHandControllerAvailable());
     * print("Are Oculus hand controllers available: " + HMD.isHandControllerAvailable("Oculus"));
     * print("Are Vive hand controllers available: " + HMD.isHandControllerAvailable("OpenVR"));
     */
    Q_INVOKABLE bool isHandControllerAvailable(const QString& name = "");

    /*@jsdoc
     * Checks whether there are specific HMD controllers available.
     * @function HMD.isSubdeviceContainingNameAvailable
     * @param {string} name - The name of the HMD controller to check for, e.g., <code>"OculusTouch"</code>.
     * @returns {boolean} <code>true</code> if an HMD controller with a name containing the specified <code>name</code> is 
     *     available, otherwise <code>false</code>.
     * @example <caption>Report if particular Oculus controllers are available.</caption>
     * print("Is an Oculus Touch controller available: " + HMD.isSubdeviceContainingNameAvailable("Touch"));
     * print("Is an Oculus Remote controller available: " + HMD.isSubdeviceContainingNameAvailable("Remote"));
     */
    Q_INVOKABLE bool isSubdeviceContainingNameAvailable(const QString& name);

    /*@jsdoc
     * Signals that models of the HMD hand controllers being used should be displayed. The models are displayed at their actual, 
     * real-world locations.
     * @function HMD.requestShowHandControllers
     * @example <caption>Show your hand controllers for 10 seconds.</caption>
     * HMD.requestShowHandControllers();
     * Script.setTimeout(function () {
     *     HMD.requestHideHandControllers();
     * }, 10000);
     */
    Q_INVOKABLE void requestShowHandControllers();

    /*@jsdoc
     * Signals that it is no longer necessary to display models of the HMD hand controllers being used. If no other scripts 
     * want the models displayed then they are no longer displayed.
     * @function HMD.requestHideHandControllers
     */
    Q_INVOKABLE void requestHideHandControllers();

    /*@jsdoc
     * Checks whether any script wants models of the HMD hand controllers displayed. Requests are made and canceled using 
     * {@link HMD.requestShowHandControllers|requestShowHandControllers} and 
     * {@link HMD.requestHideHandControllers|requestHideHandControllers}.
     * @function HMD.shouldShowHandControllers
     * @returns {boolean} <code>true</code> if any script is requesting that HMD hand controller models be displayed. 
     */
    Q_INVOKABLE bool shouldShowHandControllers() const;


    /*@jsdoc
     * Causes the borders in HUD windows to be enlarged when the laser intersects them in HMD mode. By default, borders are not 
     * enlarged.
     * @function HMD.activateHMDHandMouse
     */
    Q_INVOKABLE void activateHMDHandMouse();

    /*@jsdoc
     * Causes the border in HUD windows to no longer be enlarged when the laser intersects them in HMD mode. By default, 
     * borders are not enlarged.
     * @function HMD.deactivateHMDHandMouse
    */
    Q_INVOKABLE void deactivateHMDHandMouse();


    /*@jsdoc
     * Suppresses the activation of the HMD-provided keyboard, if any. Successful calls should be balanced with a call to 
     * {@link HMD.unsuppressKeyboard|unsuppressKeyboard} within a reasonable amount of time.
     * @function HMD.suppressKeyboard
     * @returns {boolean} <code>true</code> if the current HMD provides a keyboard and it was successfully suppressed (e.g., it 
     * isn't being displayed), otherwise <code>false</code>. 
     */
    /// Suppress the activation of any on-screen keyboard so that a script operation will
    /// not be interrupted by a keyboard popup
    /// Returns false if there is already an active keyboard displayed.
    /// Clients should re-enable the keyboard when the operation is complete and ensure
    /// that they balance any call to suppressKeyboard() that returns true with a corresponding
    /// call to unsuppressKeyboard() within a reasonable amount of time
    Q_INVOKABLE bool suppressKeyboard();

    /*@jsdoc
     * Unsuppresses the activation of the HMD-provided keyboard, if any.
     * @function HMD.unsuppressKeyboard
     */
    /// Enable the keyboard following a suppressKeyboard call
    Q_INVOKABLE void unsuppressKeyboard();

    /*@jsdoc
     * Checks whether the HMD-provided keyboard, if any, is visible.
     * @function HMD.isKeyboardVisible
     * @returns {boolean} <code>true</code> if the current HMD provides a keyboard and it is visible, otherwise 
     * <code>false</code>.
     */
    /// Query the display plugin to determine the current VR keyboard visibility
    Q_INVOKABLE bool isKeyboardVisible();

    /*@jsdoc
     * Closes the tablet if it is open.
     * @function HMD.closeTablet
     */
    Q_INVOKABLE void closeTablet();

    /*@jsdoc
     * Opens the tablet if the tablet is used in the current display mode and it isn't already showing, and sets the tablet to 
     * contextual mode if requested. In contextual mode, the page displayed on the tablet is wholly controlled by script (i.e., 
     * the user cannot navigate to another).
     * @function HMD.openTablet
     * @param {boolean} [contextualMode=false] - If <code>true</code> then the tablet is opened at a specific position and 
     *     orientation already set by the script, otherwise it opens at a position and orientation relative to the user. For 
     *     contextual mode, set the world or local position and orientation of the <code>HMD.tabletID</code> overlay.
     */
    Q_INVOKABLE void openTablet(bool contextualMode = false);

    float getVisionSqueezeRatioX() const;
    float getVisionSqueezeRatioY() const;
    void setVisionSqueezeRatioX(float value);
    void setVisionSqueezeRatioY(float value);
    float getVisionSqueezeUnSqueezeDelay() const;
    void setVisionSqueezeUnSqueezeDelay(float value);
    float getVisionSqueezeUnSqueezeSpeed() const;
    void setVisionSqueezeUnSqueezeSpeed(float value);
    float getVisionSqueezeTransition() const;
    void setVisionSqueezeTransition(float value);
    int getVisionSqueezePerEye() const;
    void setVisionSqueezePerEye(int value);
    float getVisionSqueezeGroundPlaneY() const;
    void setVisionSqueezeGroundPlaneY(float value);
    float getVisionSqueezeSpotlightSize() const;
    void setVisionSqueezeSpotlightSize(float value);
    float getVisionSqueezeTurningXFactor() const;
    void setVisionSqueezeTurningXFactor(float value);
    float getVisionSqueezeTurningYFactor() const;
    void setVisionSqueezeTurningYFactor(float value);

signals:
    /*@jsdoc
     * Triggered when a request to show or hide models of the HMD hand controllers is made using 
     * {@link HMD.requestShowHandControllers|requestShowHandControllers} or
     * {@link HMD.requestHideHandControllers|requestHideHandControllers}.
     * @function HMD.shouldShowHandControllersChanged
     * @returns {Signal}
     * @example <caption>Report when showing of hand controllers changes.</caption>
     * function onShouldShowHandControllersChanged() {
     *     print("Should show hand controllers: " + HMD.shouldShowHandControllers());
     * }
     * HMD.shouldShowHandControllersChanged.connect(onShouldShowHandControllersChanged);
     *
     * HMD.requestShowHandControllers();
     * Script.setTimeout(function () {
     *     HMD.requestHideHandControllers();
     * }, 10000);
     */
    bool shouldShowHandControllersChanged();

    /*@jsdoc
     * Triggered when the tablet is shown or hidden.
     * @function HMD.showTabletChanged
     * @param {boolean} showTablet - <code>true</code> if the tablet is showing, otherwise <code>false</code>.
     * @returns {Signal}
     */
    void showTabletChanged(bool showTablet);

    /*@jsdoc
     * Triggered when the ability to display the mini tablet has changed.
     * @function HMD.miniTabletEnabledChanged
     * @param {boolean} enabled - <code>true</code> if the mini tablet is enabled to be displayed, otherwise <code>false</code>.
     * @returns {Signal}
     */
    bool miniTabletEnabledChanged(bool enabled);

    /*@jsdoc
     * Triggered when the altering the mode for going into an away state when the interface focus is lost in VR.
     * @function HMD.awayStateWhenFocusLostInVRChanged
     * @param {boolean} enabled - <code>true</code> if the setting to go into an away state in VR when the interface focus is lost is enabled, otherwise <code>false</code>.
     * @returns {Signal}
     */
    bool awayStateWhenFocusLostInVRChanged(bool enabled);

public:
    HMDScriptingInterface();

    /*@jsdoc
     * Gets the position on the HUD overlay that your HMD is looking at, in HUD coordinates.
     * @function HMD.getHUDLookAtPosition2D
     * @returns {Vec2} The position on the HUD overlay that your HMD is looking at, in pixels.
     */
    static ScriptValuePointer getHUDLookAtPosition2D(ScriptContext* context, ScriptEngine* engine);

    /*@jsdoc
     * Gets the position on the HUD overlay that your HMD is looking at, in world coordinates.
     * @function HMD.getHUDLookAtPosition3D
     * @returns {Vec3} The position on the HUD overlay the your HMD is looking at, in world coordinates.
     */
    static ScriptValuePointer getHUDLookAtPosition3D(ScriptContext* context, ScriptEngine* engine);

    bool isMounted() const override;

    void toggleShouldShowTablet();
    void setShouldShowTablet(bool value);
    bool getShouldShowTablet() const { return _showTablet; }
    bool getTabletContextualMode() const { return _tabletContextualMode; }

    void setCurrentTabletFrameID(QUuid tabletID) { _tabletUIID = tabletID; }
    QUuid getCurrentTabletFrameID() const { return _tabletUIID; }

    void setCurrentHomeButtonID(QUuid homeButtonID) { _homeButtonID = homeButtonID; }
    QUuid getCurrentHomeButtonID() const { return _homeButtonID; }

    void setCurrentHomeButtonHighlightID(QUuid homeButtonHighlightID) { _homeButtonHighlightID = homeButtonHighlightID; }
    QUuid getCurrentHomeButtonHighlightID() const { return _homeButtonHighlightID; }

    void setCurrentTabletScreenID(QUuid tabletID) { _tabletScreenID = tabletID; }
    QUuid getCurrentTabletScreenID() const { return _tabletScreenID; }

    void setCurrentMiniTabletID(QUuid miniTabletID) { _miniTabletID = miniTabletID; }
    QUuid getCurrentMiniTabletID() const { return _miniTabletID; }

    void setCurrentMiniTabletScreenID(QUuid miniTabletScreenID) { _miniTabletScreenID = miniTabletScreenID; }
    QUuid getCurrentMiniTabletScreenID() const { return _miniTabletScreenID; }

    void setCurrentMiniTabletHand(int miniTabletHand) { _miniTabletHand = miniTabletHand; }
    int getCurrentMiniTabletHand() const { return _miniTabletHand; }

    void setMiniTabletEnabled(bool enabled);
    bool getMiniTabletEnabled();

    void setAwayStateWhenFocusLostInVREnabled(bool enabled);
    bool getAwayStateWhenFocusLostInVREnabled();

    QVariant getPlayAreaRect();
    QVector<glm::vec3> getSensorPositions();

    glm::vec3 getPosition() const;

private:
    bool _showTablet { false };
    bool _tabletContextualMode { false };
    QUuid _tabletUIID; // this is the entityID of the tablet frame
    QUuid _tabletScreenID; // this is the overlayID which is part of (a child of) the tablet-ui.
    QUuid _homeButtonID;
    QUuid _tabletEntityID;
    QUuid _homeButtonHighlightID;
    QUuid _miniTabletID;
    QUuid _miniTabletScreenID;
    int _miniTabletHand { -1 };
    bool _miniTabletEnabled { true };

    // Get the orientation of the HMD
    glm::quat getOrientation() const;

    bool getHUDLookAtPosition3D(glm::vec3& result) const;
    glm::mat4 getWorldHMDMatrix() const;
    std::atomic<int> _showHandControllersCount { 0 };

    QReadWriteLock _hmdHandMouseLock;
    int _hmdHandMouseCount;
};

#endif // hifi_HMDScriptingInterface_h
