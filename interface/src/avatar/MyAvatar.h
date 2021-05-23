//
//  MyAvatar.h
//  interface/src/avatar
//
//  Created by Mark Peng on 8/16/13.
//  Copyright 2012 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MyAvatar_h
#define hifi_MyAvatar_h

#include <bitset>

#include <glm/glm.hpp>

#include <QUuid>
#include <QtCore/QSharedPointer>

#include <AvatarConstants.h>
#include <avatars-renderer/Avatar.h>
#include <avatars-renderer/ScriptAvatar.h>
#include <controllers/Pose.h>
#include <controllers/Actions.h>
#include <EntityItem.h>
#include <ThreadSafeValueCache.h>
#include <Rig.h>
#include <SettingHandle.h>
#include <Sound.h>
#include <shared/Camera.h>

#include "AtRestDetector.h"
#include "MyCharacterController.h"
#include "RingBufferHistory.h"

class AvatarActionHold;
class ModelItemID;
class MyHead;
class DetailedMotionState;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;

/*@jsdoc
 * <p>Locomotion control types.</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>Default</td><td>Your walking speed is constant; it doesn't change depending on how far 
 *       forward you push your controller's joystick. Fully pushing your joystick forward makes your avatar run.</td></tr>
 *     <tr><td><code>1</code></td><td>Analog</td><td>Your walking speed changes in steps based on how far forward you push your 
 *       controller's joystick. Fully pushing your joystick forward makes your avatar run.</td></tr>
 *     <tr><td><code>2</code></td><td>AnalogPlus</td><td>Your walking speed changes proportionally to how far forward you push 
 *       your controller's joystick. Fully pushing your joystick forward makes your avatar run.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {number} MyAvatar.LocomotionControlsMode
 */
enum LocomotionControlsMode {
    CONTROLS_DEFAULT = 0,
    CONTROLS_ANALOG,
    CONTROLS_ANALOG_PLUS
};

enum LocomotionRelativeMovementMode {
    MOVEMENT_HMD_RELATIVE = 0,
    MOVEMENT_HAND_RELATIVE,
    MOVEMENT_HAND_RELATIVE_LEVELED
};

enum eyeContactTarget {
    LEFT_EYE,
    RIGHT_EYE,
    MOUTH
};

enum AudioListenerMode {
    FROM_HEAD = 0,
    FROM_CAMERA,
    CUSTOM
};

Q_DECLARE_METATYPE(AudioListenerMode);

class MyAvatar : public Avatar {
    Q_OBJECT
    friend class AnimStats;

    /*@jsdoc
     * Your avatar is your in-world representation of you. The <code>MyAvatar</code> API is used to manipulate the avatar.
     * For example, you can customize the avatar's appearance, run custom avatar animations,
     * change the avatar's position within the domain, or manage the avatar's collisions with the environment and other avatars.
     *
     * <p>For assignment client scripts, see {@link Avatar}.</p>
     *
     * @namespace MyAvatar
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @comment IMPORTANT: This group of properties is copied from AvatarData.h; they should NOT be edited here.
     * @property {Vec3} position - The position of the avatar.
     * @property {number} scale=1.0 - The scale of the avatar. The value can be set to anything between <code>0.005</code> and
     *     <code>1000.0</code>. When the scale value is fetched, it may temporarily be further limited by the domain's settings.
     * @property {number} density - The density of the avatar in kg/m<sup>3</sup>. The density is used to work out its mass in
     *     the application of physics. <em>Read-only.</em>
     * @property {Vec3} handPosition - A user-defined hand position, in world coordinates. The position moves with the avatar
     *    but is otherwise not used or changed by Interface.
     * @property {number} bodyYaw - The left or right rotation about an axis running from the head to the feet of the avatar.
     *     Yaw is sometimes called "heading".
     * @property {number} bodyPitch - The rotation about an axis running from shoulder to shoulder of the avatar. Pitch is
     *     sometimes called "elevation".
     * @property {number} bodyRoll - The rotation about an axis running from the chest to the back of the avatar. Roll is
     *     sometimes called "bank".
     * @property {Quat} orientation - The orientation of the avatar.
     * @property {Quat} headOrientation - The orientation of the avatar's head.
     * @property {number} headPitch - The rotation about an axis running from ear to ear of the avatar's head. Pitch is
     *     sometimes called "elevation".
     * @property {number} headYaw - The rotation left or right about an axis running from the base to the crown of the avatar's
     *     head. Yaw is sometimes called "heading".
     * @property {number} headRoll - The rotation about an axis running from the nose to the back of the avatar's head. Roll is
     *     sometimes called "bank".
     * @property {Vec3} velocity - The current velocity of the avatar.
     * @property {Vec3} angularVelocity - The current angular velocity of the avatar.
     * @property {number} audioLoudness - The instantaneous loudness of the audio input that the avatar is injecting into the
     *     domain.
     * @property {number} audioAverageLoudness - The rolling average loudness of the audio input that the avatar is injecting
     *     into the domain.
     * @property {string} displayName - The avatar's display name.
     * @property {string} sessionDisplayName - <code>displayName's</code> sanitized and default version defined by the avatar
     *     mixer rather than Interface clients. The result is unique among all avatars present in the domain at the time.
     * @property {boolean} lookAtSnappingEnabled=true - <code>true</code> if the avatar's eyes snap to look at another avatar's
     *     eyes when the other avatar is in the line of sight and also has <code>lookAtSnappingEnabled == true</code>.
     * @property {string} skeletonModelURL - The avatar's FST file.
     * @property {AttachmentData[]} attachmentData - Information on the avatar's attachments.
     *     <p class="important">Deprecated: This property is deprecated and will be removed. Use avatar entities instead.</p>
     * @property {string[]} jointNames - The list of joints in the current avatar model. <em>Read-only.</em>
     * @property {Uuid} sessionUUID - Unique ID of the avatar in the domain. <em>Read-only.</em>
     * @property {Mat4} sensorToWorldMatrix - The scale, rotation, and translation transform from the user's real world to the
     *     avatar's size, orientation, and position in the virtual world. <em>Read-only.</em>
     * @property {Mat4} controllerLeftHandMatrix - The rotation and translation of the left hand controller relative to the
     *     avatar. <em>Read-only.</em>
     * @property {Mat4} controllerRightHandMatrix - The rotation and translation of the right hand controller relative to the
     *     avatar. <em>Read-only.</em>
     * @property {number} sensorToWorldScale - The scale that transforms dimensions in the user's real world to the avatar's
     *     size in the virtual world. <em>Read-only.</em>
     * @property {boolean} hasPriority - <code>true</code> if the avatar is in a "hero" zone, <code>false</code> if it isn't.
     *     <em>Read-only.</em>
     * @property {boolean} hasScriptedBlendshapes=false - <code>true</code> if blend shapes are controlled by scripted actions,
     *     otherwise <code>false</code>. Set this to <code>true</code> before using the {@link MyAvatar.setBlendshape} method,
     *     and set back to <code>false</code> after you no longer want scripted control over the blend shapes.
     *     <p><strong>Note:</strong> This property will automatically be set to <code>true</code> if the controller system has
     *     valid facial blend shape actions.</p>
     * @property {boolean} hasProceduralBlinkFaceMovement=true - <code>true</code> if avatars blink automatically by animating
     *     facial blend shapes, <code>false</code> if automatic blinking is disabled. Set to <code>false</code> to fully control 
     *     the blink facial blend shapes via the {@link MyAvatar.setBlendshape} method.
     * @property {boolean} hasProceduralEyeFaceMovement=true - <code>true</code> if the facial blend shapes for an avatar's eyes
     *     adjust automatically as the eyes move, <code>false</code> if this automatic movement is disabled. Set this property
     *     to <code>true</code> to prevent the iris from being obscured by the upper or lower lids. Set to <code>false</code> to 
     *     fully control the eye blend shapes via the {@link MyAvatar.setBlendshape} method.
     * @property {boolean} hasAudioEnabledFaceMovement=true - <code>true</code> if the avatar's mouth blend shapes animate
     *     automatically based on detected microphone input, <code>false</code> if this automatic movement is disabled. Set
     *     this property to <code>false</code> to fully control the mouth facial blend shapes via the
     *     {@link MyAvatar.setBlendshape} method.
     *
     * @comment IMPORTANT: This group of properties is copied from Avatar.h; they should NOT be edited here.
     * @property {Vec3} skeletonOffset - Can be used to apply a translation offset between the avatar's position and the
     *     registration point of the 3D model.
     *
     * @property {Vec3} qmlPosition - A synonym for <code>position</code> for use by QML.
     *
     * @property {Vec3} feetPosition - The position of the avatar's feet.
     * @property {boolean} shouldRenderLocally=true - If <code>true</code> then your avatar is rendered for you in Interface,
     *     otherwise it is not rendered for you (but it is still rendered for other users).
     * @property {Vec3} motorVelocity=Vec3.ZERO - The target velocity of your avatar to be achieved by a scripted motor.
     * @property {number} motorTimescale=1000000 - The timescale for the scripted motor to achieve the target 
     *     <code>motorVelocity</code> avatar velocity. Smaller values result in higher acceleration.
     * @property {string} motorReferenceFrame="camera" -  Reference frame of the <code>motorVelocity</code>. Must be one of the 
     *     following: <code>"camera"</code>, <code>"avatar"</code>, and <code>"world"</code>.
     * @property {string} motorMode="simple" - The Type of scripted motor behavior: <code>"simple"</code> to use the 
     *     <code>motorTimescale</code> time scale; <code>"dynamic"</code> to use character controller timescales.
     * @property {string} collisionSoundURL="Body_Hits_Impact.wav" - The sound that's played when the avatar experiences a 
     *     collision. It can be a mono or stereo 16-bit WAV file running at either 24kHz or 48kHz. The latter is down-sampled 
     *     by the audio mixer, so all audio effectively plays back at a 24khz. 48kHz RAW files are also supported.
     * @property {number} audioListenerMode=0 - Specifies the listening position when hearing spatialized audio. Must be one 
     *     of the following property values:
     *     <ul>
     *         <li><code>MyAvatar.audioListenerModeHead</code></li>
     *         <li><code>MyAvatar.audioListenerModeCamera</code></li>
     *         <li><code>MyAvatar.audioListenerModeCustom</code></li>
     *     </ul>
     * @property {number} audioListenerModeHead=0 - The audio listening position is at the avatar's head. <em>Read-only.</em>
     * @property {number} audioListenerModeCamera=1 - The audio listening position is at the camera. <em>Read-only.</em>
     * @property {number} audioListenerModeCustom=2 - The audio listening position is at a the position specified by set by the 
     *     <code>customListenPosition</code> and <code>customListenOrientation</code> property values. <em>Read-only.</em>
     * @property {Vec3} customListenPosition=Vec3.ZERO - The listening position used when the <code>audioListenerMode</code>
     *     property value is <code>audioListenerModeCustom</code>.
     * @property {Quat} customListenOrientation=Quat.IDENTITY - The listening orientation used when the
     *     <code>audioListenerMode</code> property value is <code>audioListenerModeCustom</code>.
     * @property {number} rotationRecenterFilterLength - Configures how quickly the avatar root rotates to recenter its facing 
     *     direction to match that of the user's torso based on head and hands orientation. A smaller value makes the 
     *     recentering happen more quickly. The minimum value is <code>0.01</code>.
     * @property {number} rotationThreshold - The angle in radians that the user's torso facing direction (based on head and 
     *     hands orientation) can differ from that of the avatar before the avatar's root is rotated to match the user's torso.
     * @property {boolean} enableStepResetRotation - If <code>true</code> then after the user's avatar takes a step, the 
     *     avatar's root immediately rotates to recenter its facing direction to match that of the user's torso based on head 
     *     and hands orientation.
     * @property {boolean} enableDrawAverageFacing - If <code>true</code>, debug graphics are drawn that show the average 
     *     facing direction of the user's torso (based on head and hands orientation). This can be useful if you want to try 
     *     out different filter lengths and thresholds.
     *
     * @property {Vec3} leftHandPosition - The position of the left hand in avatar coordinates if it's being positioned by 
     *     controllers, otherwise {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
     * @property {Vec3} rightHandPosition - The position of the right hand in avatar coordinates if it's being positioned by
     *     controllers, otherwise {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
     * @property {Vec3} leftHandTipPosition - The position 0.3m in front of the left hand's position, in the direction along the 
     *     palm, in avatar coordinates. If the hand isn't being positioned by a controller, the value is 
     *     {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
     * @property {Vec3} rightHandTipPosition - The position 0.3m in front of the right hand's position, in the direction along 
     *     the palm, in avatar coordinates. If the hand isn't being positioned by a controller, the value is 
     *     {@link Vec3(0)|Vec3.ZERO}. <em>Read-only.</em>
     *
     * @property {Pose} leftHandPose - The left hand's pose as determined by the hand controllers, relative to the avatar. 
     *     <em>Read-only.</em>
     * @property {Pose} rightHandPose - The right hand's pose as determined by the hand controllers, relative to the avatar.
     *     <em>Read-only.</em>
     * @property {Pose} leftHandTipPose - The left hand's pose as determined by the hand controllers, relative to the avatar, 
     *     with the position adjusted by 0.3m along the direction of the palm. <em>Read-only.</em>
     * @property {Pose} rightHandTipPose - The right hand's pose as determined by the hand controllers, relative to the avatar, 
     *     with the position adjusted by 0.3m along the direction of the palm. <em>Read-only.</em>
     *
     * @property {number} energy - <span class="important">Deprecated: This property will be removed.</span>
     * @property {boolean} isAway - <code>true</code> if your avatar is away (i.e., inactive), <code>false</code> if it is 
     *     active.
     *
     * @property {boolean} centerOfGravityModelEnabled=true - <code>true</code> if the avatar hips are placed according to
     *     the center of gravity model that balances the center of gravity over the base of support of the feet. Set the
     *     value to <code>false</code> for default behavior where the hips are positioned under the head.
     * @property {boolean} hmdLeanRecenterEnabled=true - <code>true</code> IF the avatar is re-centered to be under the 
     *     head's position. In room-scale VR, this behavior is what causes your avatar to follow your HMD as you walk around 
     *     the room. Setting the value <code>false</code> is useful if you want to pin the avatar to a fixed position.
     * @property {boolean} collisionsEnabled - Set to <code>true</code> to enable the avatar to collide with the environment, 
     *     <code>false</code> to disable collisions with the environment. May return <code>true</code> even though the value 
     *     was set <code>false</code> because the zone may disallow collisionless avatars.
     * @property {boolean} otherAvatarsCollisionsEnabled - Set to <code>true</code> to enable the avatar to collide with other 
     *     avatars, <code>false</code> to disable collisions with other avatars.
     * @property {boolean} characterControllerEnabled - Synonym of <code>collisionsEnabled</code>.
     *     <p class="important">Deprecated: This property is deprecated and will be removed. Use <code>collisionsEnabled</code> 
     *     instead.</p>
     * @property {boolean} useAdvancedMovementControls - Returns and sets the value of the Interface setting, Settings > 
     *     Controls > Walking. Note: Setting the value has no effect unless Interface is restarted.
     * @property {boolean} showPlayArea - Returns and sets the value of the Interface setting, Settings > Controls > Show room 
     *     boundaries while teleporting.
     *     <p><strong>Note:</strong> Setting the value has no effect unless Interface is restarted.</p>
     *
     * @property {number} yawSpeed=75 - The mouse X sensitivity value in Settings > General. <em>Read-only.</em>
     * @property {number} pitchSpeed=50 - The mouse Y sensitivity value in Settings > General. <em>Read-only.</em>
     *
     * @property {boolean} hmdRollControlEnabled=true - If <code>true</code>, the roll angle of your HMD turns your avatar 
     *     while flying.
     * @property {number} hmdRollControlDeadZone=8 - The amount of HMD roll, in degrees, required before your avatar turns if 
     *    <code>hmdRollControlEnabled</code> is enabled.
     * @property {number} hmdRollControlRate If <code>MyAvatar.hmdRollControlEnabled</code> is true, this value determines the 
     *     maximum turn rate of your avatar when rolling your HMD in degrees per second.
     *
     * @property {number} userHeight=1.75 - The height of the user in sensor space.
     * @property {number} userEyeHeight=1.65 - The estimated height of the user's eyes in sensor space. <em>Read-only.</em>
     *
     * @property {Uuid} SELF_ID - UUID representing "my avatar". Only use for local-only entities in situations 
     *     where MyAvatar.sessionUUID is not available (e.g., if not connected to a domain). Note: Likely to be deprecated. 
     *     <em>Read-only.</em>
     *
     * @property {number} walkSpeed - The walk speed of your avatar for the current control scheme (see 
     *     {@link MyAvatar.getControlScheme|getControlScheme}).
     * @property {number} walkBackwardSpeed - The walk backward speed of your avatar for the current control scheme (see 
     *     {@link MyAvatar.getControlScheme|getControlScheme}).
     * @property {number} sprintSpeed - The sprint (run) speed of your avatar for the current control scheme (see 
     *     {@link MyAvatar.getControlScheme|getControlScheme}).
     * @property {number} analogPlusWalkSpeed - The walk speed of your avatar for the "AnalogPlus" control scheme.
     *     <p><strong>Warning:</strong> Setting this value also sets the value of <code>analogPlusSprintSpeed</code> to twice 
     *     the value.</p>
     * @property {number} analogPlusSprintSpeed - The sprint (run) speed of your avatar for the "AnalogPlus" control scheme.
     * @property {MyAvatar.SitStandModelType} userRecenterModel - Controls avatar leaning and recentering behavior.
     *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
     * @property {boolean} isInSittingState - <code>true</code> if the user wearing the HMD is determined to be sitting;
     *     <code>false</code> if the user wearing the HMD is determined to be standing.  This can affect whether the avatar
     *     is allowed to stand, lean or recenter its footing, depending on user preferences.
     *     The property value automatically updates as the user sits or stands. Setting the property value overrides the current
     *     sitting / standing state, which is updated when the user next sits or stands.
     * @property {boolean} isSitStandStateLocked - <code>true</code> to lock the avatar sitting/standing state, i.e., use this 
     *     to disable automatically changing state.
     *     <p class="important">Deprecated: This property is deprecated and will be removed.  
     *     See also: <code>getUserRecenterModel</code> and <code>setUserRecenterModel</code>.</p>
     * @property {boolean} allowTeleporting - <code>true</code> if teleporting is enabled in the Interface settings, 
     *     <code>false</code> if it isn't. <em>Read-only.</em>
     *
     * @borrows Avatar.getDomainMinScale as getDomainMinScale
     * @borrows Avatar.getDomainMaxScale as getDomainMaxScale
     * @borrows Avatar.getEyeHeight as getEyeHeight
     * @borrows Avatar.getHeight as getHeight
     * @borrows Avatar.setHandState as setHandState
     * @borrows Avatar.getHandState as getHandState
     * @borrows Avatar.setRawJointData as setRawJointData
     * @borrows Avatar.setJointData as setJointData
     * @borrows Avatar.setJointRotation as setJointRotation
     * @borrows Avatar.setJointTranslation as setJointTranslation
     * @borrows Avatar.clearJointData as clearJointData
     * @borrows Avatar.isJointDataValid as isJointDataValid
     * @borrows Avatar.getJointRotation as getJointRotation
     * @borrows Avatar.getJointTranslation as getJointTranslation
     * @borrows Avatar.getJointRotations as getJointRotations
     * @borrows Avatar.getJointTranslations as getJointTranslations
     * @borrows Avatar.setJointRotations as setJointRotations
     * @borrows Avatar.setJointTranslations as setJointTranslations
     * @borrows Avatar.clearJointsData as clearJointsData
     * @borrows Avatar.getJointIndex as getJointIndex
     * @borrows Avatar.getJointNames as getJointNames
     * @borrows Avatar.setBlendshape as setBlendshape
     * @borrows Avatar.getAttachmentsVariant as getAttachmentsVariant
     * @borrows Avatar.setAttachmentsVariant as setAttachmentsVariant
     * @borrows Avatar.updateAvatarEntity as updateAvatarEntity
     * @borrows Avatar.clearAvatarEntity as clearAvatarEntity
     * @borrows Avatar.setForceFaceTrackerConnected as setForceFaceTrackerConnected
     * @borrows Avatar.setSkeletonModelURL as setSkeletonModelURL
     * @borrows Avatar.getAttachmentData as getAttachmentData
     * @borrows Avatar.setAttachmentData as setAttachmentData
     * @borrows Avatar.attach as attach
     * @borrows Avatar.detachOne as detachOne
     * @borrows Avatar.detachAll as detachAll
     * @comment Avatar.getAvatarEntityData as getAvatarEntityData - Don't borrow because implementation is different.
     * @comment Avatar.setAvatarEntityData as setAvatarEntityData - Don't borrow because implementation is different.
     * @borrows Avatar.getSensorToWorldMatrix as getSensorToWorldMatrix
     * @borrows Avatar.getSensorToWorldScale as getSensorToWorldScale
     * @borrows Avatar.getControllerLeftHandMatrix as getControllerLeftHandMatrix
     * @borrows Avatar.getControllerRightHandMatrix as getControllerRightHandMatrix
     * @borrows Avatar.getDataRate as getDataRate
     * @borrows Avatar.getUpdateRate as getUpdateRate
     * @borrows Avatar.displayNameChanged as displayNameChanged
     * @borrows Avatar.sessionDisplayNameChanged as sessionDisplayNameChanged
     * @borrows Avatar.skeletonModelURLChanged as skeletonModelURLChanged
     * @borrows Avatar.lookAtSnappingChanged as lookAtSnappingChanged
     * @borrows Avatar.sessionUUIDChanged as sessionUUIDChanged
     * @borrows Avatar.sendAvatarDataPacket as sendAvatarDataPacket
     * @borrows Avatar.sendIdentityPacket as sendIdentityPacket
     * @borrows Avatar.setSessionUUID as setSessionUUID
     * @comment Avatar.getAbsoluteJointRotationInObjectFrame as getAbsoluteJointRotationInObjectFrame - Don't borrow because implementation is different.
     * @comment Avatar.getAbsoluteJointTranslationInObjectFrame as getAbsoluteJointTranslationInObjectFrame - Don't borrow because implementation is different.
     * @comment Avatar.setAbsoluteJointRotationInObjectFrame as setAbsoluteJointRotationInObjectFrame - Don't borrow because implementation is different.
     * @comment Avatar.setAbsoluteJointTranslationInObjectFrame as setAbsoluteJointTranslationInObjectFrame - Don't borrow because implementation is different.
     * @borrows Avatar.getTargetScale as getTargetScale
     * @borrows Avatar.resetLastSent as resetLastSent
     */
    // FIXME: `glm::vec3 position` is not accessible from QML, so this exposes position in a QML-native type
    Q_PROPERTY(QVector3D qmlPosition READ getQmlPosition)
    QVector3D getQmlPosition() { auto p = getWorldPosition(); return QVector3D(p.x, p.y, p.z); }

    Q_PROPERTY(glm::vec3 feetPosition READ getWorldFeetPosition WRITE goToFeetLocation)
    Q_PROPERTY(bool shouldRenderLocally READ getShouldRenderLocally WRITE setShouldRenderLocally)
    Q_PROPERTY(glm::vec3 motorVelocity READ getScriptedMotorVelocity WRITE setScriptedMotorVelocity)
    Q_PROPERTY(float motorTimescale READ getScriptedMotorTimescale WRITE setScriptedMotorTimescale)
    Q_PROPERTY(QString motorReferenceFrame READ getScriptedMotorFrame WRITE setScriptedMotorFrame)
    Q_PROPERTY(QString motorMode READ getScriptedMotorMode WRITE setScriptedMotorMode)
    Q_PROPERTY(QString collisionSoundURL READ getCollisionSoundURL WRITE setCollisionSoundURL)
    Q_PROPERTY(AudioListenerMode audioListenerMode READ getAudioListenerMode WRITE setAudioListenerMode)
    Q_PROPERTY(AudioListenerMode audioListenerModeHead READ getAudioListenerModeHead)
    Q_PROPERTY(AudioListenerMode audioListenerModeCamera READ getAudioListenerModeCamera)
    Q_PROPERTY(AudioListenerMode audioListenerModeCustom READ getAudioListenerModeCustom)
    Q_PROPERTY(glm::vec3 customListenPosition READ getCustomListenPosition WRITE setCustomListenPosition)
    Q_PROPERTY(glm::quat customListenOrientation READ getCustomListenOrientation WRITE setCustomListenOrientation)
    Q_PROPERTY(float rotationRecenterFilterLength READ getRotationRecenterFilterLength WRITE setRotationRecenterFilterLength)
    Q_PROPERTY(float rotationThreshold READ getRotationThreshold WRITE setRotationThreshold)
    Q_PROPERTY(bool enableStepResetRotation READ getEnableStepResetRotation WRITE setEnableStepResetRotation)
    Q_PROPERTY(bool enableDrawAverageFacing READ getEnableDrawAverageFacing WRITE setEnableDrawAverageFacing)
    //TODO: make gravity feature work Q_PROPERTY(glm::vec3 gravity READ getGravity WRITE setGravity)

    Q_PROPERTY(glm::vec3 leftHandPosition READ getLeftHandPosition)
    Q_PROPERTY(glm::vec3 rightHandPosition READ getRightHandPosition)
    Q_PROPERTY(glm::vec3 leftHandTipPosition READ getLeftHandTipPosition)
    Q_PROPERTY(glm::vec3 rightHandTipPosition READ getRightHandTipPosition)

    Q_PROPERTY(controller::Pose leftHandPose READ getLeftHandPose)
    Q_PROPERTY(controller::Pose rightHandPose READ getRightHandPose)
    Q_PROPERTY(controller::Pose leftHandTipPose READ getLeftHandTipPose)
    Q_PROPERTY(controller::Pose rightHandTipPose READ getRightHandTipPose)

    Q_PROPERTY(float energy READ getEnergy WRITE setEnergy)
    Q_PROPERTY(bool isAway READ getIsAway WRITE setAway)

    Q_PROPERTY(bool centerOfGravityModelEnabled READ getCenterOfGravityModelEnabled WRITE setCenterOfGravityModelEnabled)
    Q_PROPERTY(bool hmdLeanRecenterEnabled READ getHMDLeanRecenterEnabled WRITE setHMDLeanRecenterEnabled)
    Q_PROPERTY(bool collisionsEnabled READ getCollisionsEnabled WRITE setCollisionsEnabled)
    Q_PROPERTY(bool otherAvatarsCollisionsEnabled READ getOtherAvatarsCollisionsEnabled WRITE setOtherAvatarsCollisionsEnabled)
    Q_PROPERTY(bool characterControllerEnabled READ getCharacterControllerEnabled WRITE setCharacterControllerEnabled)
    Q_PROPERTY(bool useAdvancedMovementControls READ useAdvancedMovementControls WRITE setUseAdvancedMovementControls)
    Q_PROPERTY(bool showPlayArea READ getShowPlayArea WRITE setShowPlayArea)

    Q_PROPERTY(float yawSpeed MEMBER _yawSpeed)
    Q_PROPERTY(float pitchSpeed MEMBER _pitchSpeed)

    Q_PROPERTY(bool hmdRollControlEnabled READ getHMDRollControlEnabled WRITE setHMDRollControlEnabled)
    Q_PROPERTY(float hmdRollControlDeadZone READ getHMDRollControlDeadZone WRITE setHMDRollControlDeadZone)
    Q_PROPERTY(float hmdRollControlRate READ getHMDRollControlRate WRITE setHMDRollControlRate)

    Q_PROPERTY(float userHeight READ getUserHeight WRITE setUserHeight)
    Q_PROPERTY(float userEyeHeight READ getUserEyeHeight)

    Q_PROPERTY(QUuid SELF_ID READ getSelfID CONSTANT)

    Q_PROPERTY(float walkSpeed READ getWalkSpeed WRITE setWalkSpeed);
    Q_PROPERTY(float analogPlusWalkSpeed READ getAnalogPlusWalkSpeed WRITE setAnalogPlusWalkSpeed NOTIFY analogPlusWalkSpeedChanged);
    Q_PROPERTY(float analogPlusSprintSpeed READ getAnalogPlusSprintSpeed WRITE setAnalogPlusSprintSpeed NOTIFY analogPlusSprintSpeedChanged);
    Q_PROPERTY(float walkBackwardSpeed READ getWalkBackwardSpeed WRITE setWalkBackwardSpeed NOTIFY walkBackwardSpeedChanged);
    Q_PROPERTY(float sprintSpeed READ getSprintSpeed WRITE setSprintSpeed NOTIFY sprintSpeedChanged);
    Q_PROPERTY(bool isInSittingState READ getIsInSittingState WRITE setIsInSittingState);
    Q_PROPERTY(MyAvatar::SitStandModelType userRecenterModel READ getUserRecenterModel WRITE setUserRecenterModel);  // Deprecated
    Q_PROPERTY(bool isSitStandStateLocked READ getIsSitStandStateLocked WRITE setIsSitStandStateLocked);      // Deprecated
    Q_PROPERTY(bool allowTeleporting READ getAllowTeleporting)

    const QString DOMINANT_LEFT_HAND = "left";
    const QString DOMINANT_RIGHT_HAND = "right";
    const QString DEFAULT_HMD_AVATAR_ALIGNMENT_TYPE = "head";

    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    const float DEFAULT_GEAR_1 = 0.2f;
    const float DEFAULT_GEAR_2 = 0.4f;
    const float DEFAULT_GEAR_3 = 0.8f;
    const float DEFAULT_GEAR_4 = 0.9f;
    const float DEFAULT_GEAR_5 = 1.0f;

    const bool DEFAULT_STRAFE_ENABLED = true;
public:

    /*@jsdoc
     * The <code>DriveKeys</code> API provides constant numeric values that represent different logical keys that drive your 
     * avatar and camera.
     *
     * @namespace DriveKeys
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @property {number} TRANSLATE_X - Move the user's avatar in the direction of its x-axis, if the camera isn't in 
     *     independent or mirror modes.
     * @property {number} TRANSLATE_Y - Move the user's avatar in the direction of its y-axis, if the camera isn't in 
     *     independent or mirror modes.
     * @property {number} TRANSLATE_Z - Move the user's avatar in the direction of its z-axis, if the camera isn't in 
     *     independent or mirror modes.
     * @property {number} YAW - Rotate the user's avatar about its y-axis at a rate proportional to the control value, if the 
     *     camera isn't in independent or mirror modes.
     * @property {number} STEP_TRANSLATE_X - No action.
     * @property {number} STEP_TRANSLATE_Y - No action.
     * @property {number} STEP_TRANSLATE_Z - No action.
     * @property {number} STEP_YAW - Rotate the user's avatar about its y-axis in a step increment, if the camera isn't in 
     *     independent or mirror modes.
     * @property {number} PITCH - Rotate the user's avatar head and attached camera about its negative x-axis (i.e., positive 
     *     values pitch down) at a rate proportional to the control value, if the camera isn't in HMD, independent, or mirror 
     *     modes.
     * @property {number} ZOOM - Zoom the camera in or out.
     * @property {number} DELTA_YAW - Rotate the user's avatar about its y-axis by an amount proportional to the control value, 
     *     if the camera isn't in independent or mirror modes.
     * @property {number} DELTA_PITCH - Rotate the user's avatar head and attached camera about its negative x-axis (i.e., 
     *     positive values pitch down) by an amount proportional to the control value, if the camera isn't in HMD, independent, 
     *     or mirror modes.
     */

    /*@jsdoc
     * <p>Logical keys that drive your avatar and camera.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.TRANSLATE_X}</code></td><td>Move the user's avatar in the direction of its 
     *       x-axis, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.TRANSLATE_Y}</code></td><td>Move the user's avatar in the direction of its 
     *       -axis, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.TRANSLATE_Z}</code></td><td>Move the user's avatar in the direction of its 
     *       z-axis, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.YAW}</code></td><td>Rotate the user's avatar about its y-axis at a rate 
     *       proportional to the control value, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.STEP_TRANSLATE_X}</code></td><td>No action.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.STEP_TRANSLATE_Y}</code></td><td>No action.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.STEP_TRANSLATE_Z}</code></td><td>No action.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.STEP_YAW}</code></td><td>Rotate the user's avatar about its y-axis in a 
     *       step increment, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.PITCH}</code></td><td>Rotate the user's avatar head and attached camera 
     *       about its negative x-axis (i.e., positive values pitch down) at a rate proportional to the control value, if the 
     *       camera isn't in HMD, independent, or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.ZOOM}</code></td><td>Zoom the camera in or out.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.DELTA_YAW}</code></td><td>Rotate the user's avatar about its y-axis by an 
     *       amount proportional to the control value, if the camera isn't in independent or mirror modes.</td></tr>
     *     <tr><td><code>{@link DriveKeys|DriveKeys.DELTA_PITCH}</code></td><td>Rotate the user's avatar head and attached 
     *       camera about its negative x-axis (i.e., positive values pitch down) by an amount proportional to the control 
     *       value, if the camera isn't in HMD, independent, or mirror modes.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {number} DriveKey
     */
    enum DriveKeys {
        TRANSLATE_X = 0,
        TRANSLATE_Y,
        TRANSLATE_Z,
        YAW,
        STEP_TRANSLATE_X,
        STEP_TRANSLATE_Y,
        STEP_TRANSLATE_Z,
        STEP_YAW,
        PITCH,
        ZOOM,
        DELTA_YAW,
        DELTA_PITCH,
        MAX_DRIVE_KEYS
    };
    Q_ENUM(DriveKeys)

    /*@jsdoc
     * <p>Specifies different avatar leaning and recentering behaviors.</p>
     * <p class="important">Deprecated: This type is deprecated and will be removed.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>0</code></td><td>ForceSit</td><td>Assumes the user is seated in the real world. Disables avatar 
     *       leaning regardless of what the avatar is doing in the virtual world (i.e., avatar always recenters).</td></tr>
     *     <tr><td><code>1</code></td><td>ForceStand</td><td>Assumes the user is standing in the real world. Enables avatar 
     *       leaning regardless of what the avatar is doing in the virtual world (i.e., avatar leans, then if leans too far it 
     *       recenters).</td></tr>
     *     <tr><td><code>2</code></td><td>Auto</td><td>Interface detects when the user is standing or seated in the real world. 
     *       Avatar leaning is disabled when the user is sitting (i.e., avatar always recenters), and avatar leaning is enabled 
     *       when the user is standing (i.e., avatar leans, then if leans too far it recenters).</td></tr>
     *     <tr><td><code>3</code></td><td>DisableHMDLean</td><td><p>Both avatar leaning and recentering are disabled regardless of 
     *       what the user is doing in the real world and no matter what their avatar is doing in the virtual world. Enables 
     *       the avatar to sit on the floor when the user sits on the floor.</p>
     *       <p><strong>Note:</strong> Experimental.</p></td></tr>
     *   </tbody>
     * </table>
     * @typedef {number} MyAvatar.SitStandModelType
     */
    enum SitStandModelType {
        ForceSit = 0,
        ForceStand,
        Auto,
        DisableHMDLean,
        NumSitStandTypes
    };
    Q_ENUM(SitStandModelType)

    // Note: The option strings in setupPreferences (PreferencesDialog.cpp) must match this order.
    enum class AllowAvatarStandingPreference : uint {
        WhenUserIsStanding,
        Always,
        Count,
        Default = Always
    };
    Q_ENUM(AllowAvatarStandingPreference)

    // Note: The option strings in setupPreferences (PreferencesDialog.cpp) must match this order.
    enum class AllowAvatarLeaningPreference : uint {
        WhenUserIsStanding,
        Always,
        Never,
        AlwaysNoRecenter,  // experimental
        Count,
        Default = WhenUserIsStanding
    };
    Q_ENUM(AllowAvatarLeaningPreference)

    static const std::array<QString, (uint)AllowAvatarStandingPreference::Count> allowAvatarStandingPreferenceStrings;
    static const std::array<QString, (uint)AllowAvatarLeaningPreference::Count> allowAvatarLeaningPreferenceStrings;

    explicit MyAvatar(QThread* thread);
    virtual ~MyAvatar();

    void instantiableAvatar() override {};
    void registerMetaTypes(ScriptEnginePointer engine);

    virtual void simulateAttachments(float deltaTime) override;

    AudioListenerMode getAudioListenerModeHead() const { return FROM_HEAD; }
    AudioListenerMode getAudioListenerModeCamera() const { return FROM_CAMERA; }
    AudioListenerMode getAudioListenerModeCustom() const { return CUSTOM; }

    void reset(bool andRecenter = false, bool andReload = true, bool andHead = true);

    void setCollisionWithOtherAvatarsFlags() override;

    /*@jsdoc
     * Resets the sensor positioning of your HMD (if in use) and recenters your avatar body and head.
     * @function MyAvatar.resetSensorsAndBody
     */
    Q_INVOKABLE void resetSensorsAndBody();

    /*@jsdoc
     * Moves and orients the avatar, such that it is directly underneath the HMD, with toes pointed forward in the direction of 
     * the HMD.
     * @function MyAvatar.centerBody
     */
    Q_INVOKABLE void centerBody(); // thread-safe


    /*@jsdoc
     * Clears inverse kinematics joint limit history.
     * <p>The internal inverse-kinematics system maintains a record of which joints are "locked". Sometimes it is useful to 
     * forget this history to prevent contorted joints, e.g., after finishing with an override animation.</p>
     * @function MyAvatar.clearIKJointLimitHistory
     */
    Q_INVOKABLE void clearIKJointLimitHistory(); // thread-safe

    void update(float deltaTime);
    virtual void postUpdate(float deltaTime, const render::ScenePointer& scene) override;
    void preDisplaySide(const RenderArgs* renderArgs);

    const glm::mat4& getHMDSensorMatrix() const { return _hmdSensorMatrix; }
    const glm::vec3& getHMDSensorPosition() const { return _hmdSensorPosition; }
    const glm::quat& getHMDSensorOrientation() const { return _hmdSensorOrientation; }

    /*@jsdoc
     * Gets the avatar orientation. Suitable for use in QML.
     * @function MyAvatar.setOrientationVar
     * @param {object} newOrientationVar - The avatar's orientation.
     */
    Q_INVOKABLE void setOrientationVar(const QVariant& newOrientationVar);

    /*@jsdoc
     * Gets the avatar orientation. Suitable for use in QML.
     * @function MyAvatar.getOrientationVar
     * @returns {object} The avatar's orientation.
     */
    Q_INVOKABLE QVariant getOrientationVar() const;

    // A method intended to be overriden by MyAvatar for polling orientation for network transmission.
    glm::quat getOrientationOutbound() const override;

    // Pass a recent sample of the HMD to the avatar.
    // This can also update the avatar's position to follow the HMD
    // as it moves through the world.
    void updateFromHMDSensorMatrix(const glm::mat4& hmdSensorMatrix);

    // compute the hip to hand average azimuth.
    glm::vec2 computeHandAzimuth() const;

    // read the location of a hand controller and save the transform
    void updateJointFromController(controller::Action poseKey, ThreadSafeValueCache<glm::mat4>& matrixCache);

    // best called at end of main loop, just before rendering.
    // update sensor to world matrix from current body position and hmd sensor.
    // This is so the correct camera can be used for rendering.
    void updateSensorToWorldMatrix();

    void setRealWorldFieldOfView(float realWorldFov) { _realWorldFieldOfView.set(realWorldFov); }

    /*@jsdoc
     * Gets the position in world coordinates of the point directly between your avatar's eyes assuming your avatar was in its
     * default pose. This is a reference position; it does not change as your avatar's head moves relative to the avatar 
     * position.
     * @function MyAvatar.getDefaultEyePosition
     * @returns {Vec3} Default position between your avatar's eyes in world coordinates.
     * @example <caption>Report your avatar's default eye position.</caption>
     * var defaultEyePosition = MyAvatar.getDefaultEyePosition();
     * print(JSON.stringify(defaultEyePosition));
     */
    Q_INVOKABLE glm::vec3 getDefaultEyePosition() const;

    float getRealWorldFieldOfView() { return _realWorldFieldOfView.get(); }

    /*@jsdoc
     * Overrides the default avatar animations.
     * <p>The avatar animation system includes a set of default animations along with rules for how those animations are blended
     * together with procedural data (such as look at vectors, hand sensors etc.). <code>overrideAnimation()</code> is used to 
     * completely override all motion from the default animation system (including inverse kinematics for hand and head 
     * controllers) and play a set of specified animations. To end these animations and restore the default animations, use 
     * {@link MyAvatar.restoreAnimation}.</p>
     * <p>Note: When using pre-built animation data, it's critical that the joint orientation of the source animation and target 
     * rig are equivalent, since the animation data applies absolute values onto the joints. If the orientations are different, 
     * the avatar will move in unpredictable ways. For more information about avatar joint orientation standards, see 
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.</p>
     * @function MyAvatar.overrideAnimation
     * @param {string} url - The URL to the animation file. Animation files may be in glTF or FBX format, but only need to 
     *     contain the avatar skeleton and animation data. glTF models may be in JSON or binary format (".gltf" or ".glb" URLs 
     *     respectively).
     *     <p><strong>Warning:</strong> glTF animations currently do not always animate correctly.</p>
     * @param {number} fps - The frames per second (FPS) rate for the animation playback. 30 FPS is normal speed.
     * @param {boolean} loop - <code>true</code> if the animation should loop, <code>false</code> if it shouldn't.
     * @param {number} firstFrame - The frame to start the animation at.
     * @param {number} lastFrame - The frame to end the animation at.
     * @example <caption> Play a clapping animation on your avatar for three seconds. </caption>
     * var ANIM_URL = "https://apidocs.vircadia.dev/models/ClapHands_Standing.fbx";
     * MyAvatar.overrideAnimation(ANIM_URL, 30, true, 0, 53);
     * Script.setTimeout(function () {
     *     MyAvatar.restoreAnimation();
     *     MyAvatar.clearIKJointLimitHistory();
     * }, 3000);
     */
    Q_INVOKABLE void overrideAnimation(const QString& url, float fps, bool loop, float firstFrame, float lastFrame);

    /*@jsdoc
     * Overrides the default hand poses that are triggered with controller buttons.
     * Use {@link MyAvatar.restoreHandAnimation} to restore the default poses.
     * @function MyAvatar.overrideHandAnimation
     * @param isLeft {boolean} <code>true</code> to override the left hand, <code>false</code> to override the right hand.
     * @param {string} url - The URL of the animation file. Animation files need to be in glTF or FBX format, but only need to 
     *     contain the avatar skeleton and animation data. glTF models may be in JSON or binary format (".gltf" or ".glb" URLs 
     *     respectively).
     *     <p><strong>Warning:</strong> glTF animations currently do not always animate correctly.</p>
     * @param {number} fps - The frames per second (FPS) rate for the animation playback. 30 FPS is normal speed.
     * @param {boolean} loop - <code>true</code> if the animation should loop, <code>false</code> if it shouldn't.
     * @param {number} firstFrame - The frame to start the animation at.
     * @param {number} lastFrame - The frame to end the animation at.
     * @example <caption> Override left hand animation for three seconds.</caption>
     * var ANIM_URL = "https://apidocs.vircadia.dev/models/ClapHands_Standing.fbx";
     * MyAvatar.overrideHandAnimation(isLeft, ANIM_URL, 30, true, 0, 53);
     * Script.setTimeout(function () {
     *     MyAvatar.restoreHandAnimation();
     * }, 3000);
     */
    Q_INVOKABLE void overrideHandAnimation(bool isLeft, const QString& url, float fps, bool loop, float firstFrame, float lastFrame);

    /*@jsdoc
     * Restores the default animations.
     * <p>The avatar animation system includes a set of default animations along with rules for how those animations are blended 
     * together with procedural data (such as look at vectors, hand sensors etc.). Playing your own custom animations will 
     * override the  default animations. <code>restoreAnimation()</code> is used to restore all motion from the default 
     * animation system including inverse kinematics for hand and head controllers. If you aren't currently playing an override 
     * animation, this function has no effect.</p>
     * @function MyAvatar.restoreAnimation
     * @example <caption> Play a clapping animation on your avatar for three seconds. </caption>
     * var ANIM_URL = "https://apidocs.vircadia.dev/models/ClapHands_Standing.fbx";
     * MyAvatar.overrideAnimation(ANIM_URL, 30, true, 0, 53);
     * Script.setTimeout(function () {
     *     MyAvatar.restoreAnimation();
     * }, 3000);
     */
    Q_INVOKABLE void restoreAnimation();

    /*@jsdoc
     * Restores the default hand animation state machine that is driven by the state machine in the avatar-animation JSON.
     * <p>The avatar animation system includes a set of default animations along with rules for how those animations are blended
     * together with procedural data (such as look at vectors, hand sensors etc.). Playing your own custom animations will
     * override the  default animations. <code>restoreHandAnimation()</code> is used to restore the default hand poses.
     * If you aren't currently playing an override hand animation, this function has no effect.</p>
     * @function MyAvatar.restoreHandAnimation
     * @param isLeft {boolean} Set to true if using the left hand
     * @example <caption> Override left hand animation for three seconds. </caption>
     * var ANIM_URL = "https://apidocs.vircadia.dev/models/ClapHands_Standing.fbx";
     * MyAvatar.overrideHandAnimation(isLeft, ANIM_URL, 30, true, 0, 53);
     * Script.setTimeout(function () {
     *     MyAvatar.restoreHandAnimation();
     * }, 3000);
     */
    Q_INVOKABLE void restoreHandAnimation(bool isLeft);

    /*@jsdoc
     * Gets the current animation roles.
     * <p>Each avatar has an avatar-animation.json file that defines which animations are used and how they are blended together 
     * with procedural data (such as look at vectors, hand sensors etc.). Each animation specified in the avatar-animation.json 
     * file is known as an animation role. Animation roles map to easily understandable actions that the avatar can perform, 
     * such as <code>"idleStand"</code>, <code>"idleTalk"</code>, or <code>"walkFwd"</code>. <code>getAnimationRoles()</code> 
     * is used get the list of animation roles defined in the avatar-animation.json.</p>
     * @function MyAvatar.getAnimationRoles
     * @returns {string[]} Array of role strings.
     * @example <caption>Print the list of animation roles defined in the avatar's avatar-animation.json file to the debug log.</caption>
     * var roles = MyAvatar.getAnimationRoles();
     * print("Animation Roles:");
     * for (var i = 0; i < roles.length; i++) {
     *     print(roles[i]);
     * }
     */
    Q_INVOKABLE QStringList getAnimationRoles();

    /*@jsdoc
     * Overrides a specific animation role.
     * <p>Each avatar has an avatar-animation.json file that defines a set of animation roles. Animation roles map to easily 
     * understandable actions that the avatar can perform, such as <code>"idleStand"</code>, <code>"idleTalk"</code>, or 
     * <code>"walkFwd"</code>. To get the full list of roles, use {@ link MyAvatar.getAnimationRoles}.
     * For each role, the avatar-animation.json defines when the animation is used, the animation clip (glTF or FBX) used, and 
     * how animations are blended together with procedural data (such as look at vectors, hand sensors etc.).
     * <code>overrideRoleAnimation()</code> is used to change the animation clip (glTF or FBX) associated with a specified 
     * animation role. To end the role animation and restore the default, use {@link MyAvatar.restoreRoleAnimation}.</p>
     * <p>Note: Hand roles only affect the hand. Other "main" roles, like "idleStand", "idleTalk", and "takeoffStand", are full 
     * body.</p>
     * <p>Note: When using pre-built animation data, it's critical that the joint orientation of the source animation and target
     * rig are equivalent, since the animation data applies absolute values onto the joints. If the orientations are different,
     * the avatar will move in unpredictable ways. For more information about avatar joint orientation standards, see 
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.
     * @function MyAvatar.overrideRoleAnimation
     * @param {string} role - The animation role to override
     * @param {string} url - The URL to the animation file. Animation files need to be in glTF or FBX format, but only need to 
     *     contain the avatar skeleton and animation data. glTF models may be in JSON or binary format (".gltf" or ".glb" URLs 
     *     respectively).
     *     <p><strong>Warning:</strong> glTF animations currently do not always animate correctly.</p>
     * @param {number} fps - The frames per second (FPS) rate for the animation playback. 30 FPS is normal speed.
     * @param {boolean} loop - <code>true</code> if the animation should loop, <code>false</code> if it shouldn't.
     * @param {number} firstFrame - The frame the animation should start at.
     * @param {number} lastFrame - The frame the animation should end at.
     * @example <caption>The default avatar-animation.json defines an "idleStand" animation role. This role specifies that when the avatar is not moving,
     * an animation clip of the avatar idling with hands hanging at its side will be used. It also specifies that when the avatar moves, the animation
     * will smoothly blend to the walking animation used by the "walkFwd" animation role.
     * In this example, the "idleStand" role animation clip has been replaced with a clapping animation clip. Now instead of standing with its arms
     * hanging at its sides when it is not moving, the avatar will stand and clap its hands. Note that just as it did before, as soon as the avatar
     * starts to move, the animation will smoothly blend into the walk animation used by the "walkFwd" animation role.</caption>
     * // An animation of the avatar clapping its hands while standing. Restore default after 30s.
     * var ANIM_URL = "https://apidocs.vircadia.dev/models/ClapHands_Standing.fbx";
     * MyAvatar.overrideRoleAnimation("idleStand", ANIM_URL, 30, true, 0, 53);
     * Script.setTimeout(function () {
     *     MyAvatar.restoreRoleAnimation();
     * }, 30000);
     */
    Q_INVOKABLE void overrideRoleAnimation(const QString& role, const QString& url, float fps, bool loop, float firstFrame, float lastFrame);

    /*@jsdoc
     * Restores a default role animation.
     * <p>Each avatar has an avatar-animation.json file that defines a set of animation roles. Animation roles map to easily 
     * understandable actions that the avatar can perform, such as <code>"idleStand"</code>, <code>"idleTalk"</code>, or 
     * <code>"walkFwd"</code>. To get the full list of roles, use {@link MyAvatar.getAnimationRoles}. For each role,
     * the avatar-animation.json defines when the animation is used, the animation clip (glTF or FBX) used, and how animations 
     * are blended together with procedural data (such as look-at vectors, hand sensors etc.). You can change the animation 
     * clip (glTF or FBX) associated with a specified animation role using {@link MyAvatar.overrideRoleAnimation}.
     * <code>restoreRoleAnimation()</code> is used to restore a specified animation role's default animation clip. If you have 
     * not specified an override animation for the specified role, this function has no effect.
     * @function MyAvatar.restoreRoleAnimation
     * @param role {string} The animation role clip to restore.
     */
    Q_INVOKABLE void restoreRoleAnimation(const QString& role);

    // Adds handler(animStateDictionaryIn) => animStateDictionaryOut, which will be invoked just before each animGraph state update.
    // The handler will be called with an animStateDictionaryIn that has all those properties specified by the (possibly empty)
    // propertiesList argument. However for debugging, if the properties argument is null, all internal animGraph state is provided.
    // The animStateDictionaryOut can be a different object than animStateDictionaryIn. Any properties set in animStateDictionaryOut
    // will override those of the internal animation machinery.
    // The animStateDictionaryIn may be shared among multiple handlers, and thus may contain additional properties specified when
    // adding one of the other handlers. While any handler may change a value in animStateDictionaryIn (or supply different values in animStateDictionaryOut)
    // a handler must not remove properties from animStateDictionaryIn, nor change property values that it does not intend to change.
    // It is not specified in what order multiple handlers are called.
    /*@jsdoc
     * Adds an animation state handler function that is invoked just before each animation graph update. More than one 
     * animation state handler function may be added by calling <code>addAnimationStateHandler</code> multiple times. It is not 
     * specified in what order multiple handlers are called.
     * <p>The animation state handler function is called with an {@link MyAvatar.AnimStateDictionary|AnimStateDictionary}
     * "<code>animStateDictionaryIn</code>" parameter and is expected to return an 
     * {@link MyAvatar.AnimStateDictionary|AnimStateDictionary} "<code>animStateDictionaryOut</code>" object. The 
     * <code>animStateDictionaryOut</code> object can be the same object as <code>animStateDictionaryIn</code>, or it can be a 
     * different object. The <code>animStateDictionaryIn</code> may be shared among multiple handlers and thus may contain 
     * additional properties specified when adding the different handlers.</p>
     * <p>A handler may change a value from <code>animStateDictionaryIn</code> or add different values in the 
     * <code>animStateDictionaryOut</code> returned. Any property values set in <code>animStateDictionaryOut</code> will 
     * override those of the internal animation machinery.</p>
     * @function MyAvatar.addAnimationStateHandler
     * @param {function} handler - The animation state handler function to add.
     * @param {Array<string>|null} propertiesList - The list of {@link MyAvatar.AnimStateDictionary|AnimStateDictionary} 
     *     properties that should be included in the parameter that the handler function is called with. If <code>null</code> 
     *     then all properties are included in the call parameter.
     * @returns {number} The ID of the animation state handler function if successfully added, <code>undefined</code> if not.
     * @example <caption>Log all the animation state dictionary parameters for a short while.</caption>
     * function animStateHandler(dictionary) {
     *     print("Anim state dictionary: " + JSON.stringify(dictionary));
     * }
     * 
     * var handler = MyAvatar.addAnimationStateHandler(animStateHandler, null);
     * 
     * Script.setTimeout(function () {
     *     MyAvatar.removeAnimationStateHandler(handler);
     * }, 100);
     */
    Q_INVOKABLE ScriptValuePointer addAnimationStateHandler(ScriptValuePointer handler, ScriptValuePointer propertiesList) { return _skeletonModel->getRig().addAnimationStateHandler(handler, propertiesList); }

    /*@jsdoc
     * Removes an animation state handler function.
     * @function MyAvatar.removeAnimationStateHandler
     * @param {number} handler - The ID of the animation state handler function to remove.
     */
    // Removes a handler previously added by addAnimationStateHandler.
    Q_INVOKABLE void removeAnimationStateHandler(ScriptValuePointer handler) { _skeletonModel->getRig().removeAnimationStateHandler(handler); }


    /*@jsdoc
     * Gets whether you do snap turns in HMD mode.
     * @function MyAvatar.getSnapTurn
     * @returns {boolean} <code>true</code> if you do snap turns in HMD mode; <code>false</code> if you do smooth turns in HMD 
     *     mode.
     */
    Q_INVOKABLE bool getSnapTurn() const { return _useSnapTurn; }

    /*@jsdoc
     * Sets whether you do snap turns or smooth turns in HMD mode.
     * @function MyAvatar.setSnapTurn
     * @param {boolean} on - <code>true</code> to do snap turns in HMD mode; <code>false</code> to do smooth turns in HMD mode.
     */
    Q_INVOKABLE void setSnapTurn(bool on) { _useSnapTurn = on; }

    /*@jsdoc
     * Gets the control scheme that is in use.
     * @function MyAvatar.getControlScheme
     * @returns {MyAvatar.LocomotionControlsMode} The control scheme that is in use.
     */
    Q_INVOKABLE int getControlScheme() const { return _controlSchemeIndex; }

    /*@jsdoc
     * Sets the control scheme to use.
     * @function MyAvatar.setControlScheme
     * @param {MyAvatar.LocomotionControlsMode} controlScheme - The control scheme to use.
     */
    Q_INVOKABLE void setControlScheme(int index) { _controlSchemeIndex = (index >= 0 && index <= 2) ? index : 0; }
    
    /*@jsdoc
     * Gets whether your avatar hovers when its feet are not on the ground.
     * @function MyAvatar.hoverWhenUnsupported
     * @returns {boolean} <code>true</code> if your avatar hovers when its feet are not on the ground, <code>false</code> if it 
     *     falls.
     */
    // FIXME: Should be named, getHoverWhenUnsupported().
    Q_INVOKABLE bool hoverWhenUnsupported() const { return _hoverWhenUnsupported; }

    /*@jsdoc
     * Sets whether your avatar hovers when its feet are not on the ground.
     * @function MyAvatar.setHoverWhenUnsupported
     * @param {boolean} hover - <code>true</code> if your avatar hovers when its feet are not on the ground, <code>false</code> 
     *     if it falls.
     */
    Q_INVOKABLE void setHoverWhenUnsupported(bool on) { _hoverWhenUnsupported = on; }

    /*@jsdoc
     * Sets the avatar's dominant hand.
     * @function MyAvatar.setDominantHand
     * @param {string} hand - The dominant hand: <code>"left"</code> for the left hand or <code>"right"</code> for the right 
     *     hand. Any other value has no effect.
     */
    Q_INVOKABLE void setDominantHand(const QString& hand);

    /*@jsdoc
     * Gets the avatar's dominant hand.
     * @function MyAvatar.getDominantHand
     * @returns {string} <code>"left"</code> for the left hand, <code>"right"</code> for the right hand.
     */
    Q_INVOKABLE QString getDominantHand() const;

    /*@jsdoc
     * Sets whether strafing is enabled.
     * @function MyAvatar.setStrafeEnabled
     * @param {boolean} enabled - <code>true</code> if strafing is enabled, <code>false</code> if it isn't.
     */
    Q_INVOKABLE void setStrafeEnabled(bool enabled);

    /*@jsdoc
     * Gets whether strafing is enabled.
     * @function MyAvatar.getStrafeEnabled
     * @returns {boolean} <code>true</code> if strafing is enabled, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool getStrafeEnabled() const;

    /*@jsdoc
     * Sets the HMD alignment relative to your avatar.
     * @function MyAvatar.setHmdAvatarAlignmentType
     * @param {string} type - <code>"head"</code> to align your head and your avatar's head, <code>"eyes"</code> to align your 
     *     eyes and your avatar's eyes.
     */
    Q_INVOKABLE void setHmdAvatarAlignmentType(const QString& type);

    /*@jsdoc
     * Gets the HMD alignment relative to your avatar.
     * @function MyAvatar.getHmdAvatarAlignmentType
     * @returns {string} <code>"head"</code> if aligning your head and your avatar's head, <code>"eyes"</code> if aligning your 
     *     eyes and your avatar's eyes.
     */
    Q_INVOKABLE QString getHmdAvatarAlignmentType() const;

    /*@jsdoc
     * Sets whether the avatar's hips are balanced over the feet or positioned under the head.
     * @function MyAvatar.setCenterOfGravityModelEnabled
     * @param {boolean} enabled - <code>true</code> to balance the hips over the feet, <code>false</code> to position the hips 
     *     under the head.
     */
    Q_INVOKABLE void setCenterOfGravityModelEnabled(bool value) { _centerOfGravityModelEnabled = value; }

    /*@jsdoc
     * Gets whether the avatar hips are being balanced over the feet or placed under the head.
     * @function MyAvatar.getCenterOfGravityModelEnabled
     * @returns {boolean} <code>true</code> if the hips are being balanced over the feet, <code>false</code> if the hips are 
     *     being positioned under the head.
     */
    Q_INVOKABLE bool getCenterOfGravityModelEnabled() const { return _centerOfGravityModelEnabled; }

    /*@jsdoc
     * Sets whether the avatar's position updates to recenter the avatar under the head. In room-scale VR, recentering 
     * causes your avatar to follow your HMD as you walk around the room. Disabling recentering is useful if you want to pin 
     * the avatar to a fixed position.
     * @function MyAvatar.setHMDLeanRecenterEnabled
     * @param {boolean} enabled - <code>true</code> to recenter the avatar under the head as it moves, <code>false</code> to 
     *     disable recentering.
     */
    Q_INVOKABLE void setHMDLeanRecenterEnabled(bool value) { _hmdLeanRecenterEnabled = value; }

    /*@jsdoc
     * Gets whether the avatar's position updates to recenter the avatar under the head. In room-scale VR, recentering
     * causes your avatar to follow your HMD as you walk around the room.
     * @function MyAvatar.getHMDLeanRecenterEnabled
     * @returns {boolean} <code>true</code> if recentering is enabled, <code>false</code> if not.
     */
    Q_INVOKABLE bool getHMDLeanRecenterEnabled() const { return _hmdLeanRecenterEnabled; }

    /*@jsdoc
     * Requests that the hand touch effect is disabled for your avatar. Any resulting change in the status of the hand touch 
     * effect will be signaled by {@link MyAvatar.shouldDisableHandTouchChanged}.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that 
     * it is really touching that object.</p>
     * @function MyAvatar.requestEnableHandTouch
     */
    Q_INVOKABLE void requestEnableHandTouch();

    /*@jsdoc
     * Requests that the hand touch effect is enabled for your avatar. Any resulting change in the status of the hand touch
     * effect will be signaled by {@link MyAvatar.shouldDisableHandTouchChanged}.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that
     * it is really touching that object.</p>
     * @function MyAvatar.requestDisableHandTouch
     */
    Q_INVOKABLE void requestDisableHandTouch();

    /*@jsdoc
     * Disables the hand touch effect on a specific entity.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that
     * it is really touching that object.</p>
     * @function MyAvatar.disableHandTouchForID
     * @param {Uuid} entityID - The entity that the hand touch effect will be disabled for.
     */
    Q_INVOKABLE void disableHandTouchForID(const QUuid& entityID);

    /*@jsdoc
     * Enables the hand touch effect on a specific entity.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that
     * it is really touching that object.</p>
     * @function MyAvatar.enableHandTouchForID
     * @param {Uuid} entityID - The entity that the hand touch effect will be enabled for.
     */
    Q_INVOKABLE void enableHandTouchForID(const QUuid& entityID);

    bool useAdvancedMovementControls() const { return _useAdvancedMovementControls.get(); }
    void setUseAdvancedMovementControls(bool useAdvancedMovementControls)
        { _useAdvancedMovementControls.set(useAdvancedMovementControls); }

    bool getAllowTeleporting() { return _allowTeleportingSetting.get(); }
    void setAllowTeleporting(bool allowTeleporting) { _allowTeleportingSetting.set(allowTeleporting); }

    bool getShowPlayArea() const { return _showPlayArea.get(); }
    void setShowPlayArea(bool showPlayArea) { _showPlayArea.set(showPlayArea); }

    void setHMDRollControlEnabled(bool value) { _hmdRollControlEnabled = value; }
    bool getHMDRollControlEnabled() const { return _hmdRollControlEnabled; }
    void setHMDRollControlDeadZone(float value) { _hmdRollControlDeadZone = value; }
    float getHMDRollControlDeadZone() const { return _hmdRollControlDeadZone; }
    void setHMDRollControlRate(float value) { _hmdRollControlRate = value; }
    float getHMDRollControlRate() const { return _hmdRollControlRate; }

    // get/set avatar data
    void resizeAvatarEntitySettingHandles(uint32_t maxIndex);
    void saveData();
    void saveAvatarEntityDataToSettings();
    void loadData();
    void loadAvatarEntityDataFromSettings();

    void saveAttachmentData(const AttachmentData& attachment) const;
    AttachmentData loadAttachmentData(const QUrl& modelURL, const QString& jointName = QString()) const;

    //  Set what driving keys are being pressed to control thrust levels
    void clearDriveKeys();
    void setDriveKey(DriveKeys key, float val);
    void setSprintMode(bool sprint);
    float getDriveKey(DriveKeys key) const;

    /*@jsdoc
     * Gets the value of a drive key, regardless of whether it is disabled.
     * @function MyAvatar.getRawDriveKey
     * @param {DriveKey} key - The drive key.
     * @returns {number} The value of the drive key.
     */
    Q_INVOKABLE float getRawDriveKey(DriveKeys key) const;

    void relayDriveKeysToCharacterController();
    
    /*@jsdoc
     * Disables the action associated with a drive key.
     * @function MyAvatar.disableDriveKey
     * @param {DriveKey} key - The drive key to disable.
     * @example <caption>Disable rotating your avatar using the keyboard for a couple of seconds.</caption>
     * print("Disable");
     * MyAvatar.disableDriveKey(DriveKeys.YAW);
     * Script.setTimeout(function () {
     *     print("Enable");
     *     MyAvatar.enableDriveKey(YAW);
     * }, 5000);
     */
    Q_INVOKABLE void disableDriveKey(DriveKeys key);

    /*@jsdoc
     * Enables the action associated with a drive key. The action may have been disabled with 
     * {@link MyAvatar.disableDriveKey|disableDriveKey}.
     * @function MyAvatar.enableDriveKey
     * @param {DriveKey} key - The drive key to enable.
     */
    Q_INVOKABLE void enableDriveKey(DriveKeys key);

    /*@jsdoc
     * Checks whether a drive key is disabled.
     * @function MyAvatar.isDriveKeyDisabled
     * @param {DriveKey} key - The drive key to check.
     * @returns {boolean} <code>true</code> if the drive key is disabled, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isDriveKeyDisabled(DriveKeys key) const;


    /*@jsdoc
     * Recenter the avatar in the vertical direction, if <code>{@link MyAvatar|MyAvatar.hmdLeanRecenterEnabled}</code> is 
     * <code>false</code>.
     * @function MyAvatar.triggerVerticalRecenter
     */
    Q_INVOKABLE void triggerVerticalRecenter();

    /*@jsdoc
     * Recenter the avatar in the horizontal direction, if <code>{@link MyAvatar|MyAvatar.hmdLeanRecenterEnabled}</code> is 
     * <code>false</code>.
     * @function MyAvatar.triggerHorizontalRecenter
     */
    Q_INVOKABLE void triggerHorizontalRecenter();

    /*@jsdoc
     * Recenter the avatar's rotation, if <code>{@link MyAvatar|MyAvatar.hmdLeanRecenterEnabled}</code> is <code>false</code>.
     * @function MyAvatar.triggerRotationRecenter
     */
    Q_INVOKABLE void triggerRotationRecenter();

    /*@jsdoc
     * Gets whether the avatar is configured to keep its center of gravity under its head.
     * @function MyAvatar.isRecenteringHorizontally
     * @returns {boolean} <code>true</code> if the avatar is keeping its center of gravity under its head position, 
     *     <code>false</code> if not.
    */
    Q_INVOKABLE bool isRecenteringHorizontally() const;

    eyeContactTarget getEyeContactTarget();

    const MyHead* getMyHead() const;

    /*@jsdoc
     * Gets the current position of the avatar's "Head" joint.
     * @function MyAvatar.getHeadPosition
     * @returns {Vec3} The current position of the avatar's "Head" joint.
     * @example <caption>Report the current position of your avatar's head.</caption>
     * print(JSON.stringify(MyAvatar.getHeadPosition()));
     */
    Q_INVOKABLE glm::vec3 getHeadPosition() const { return getHead()->getPosition(); }

    /*@jsdoc
     * Gets the yaw of the avatar's head relative to its body.
     * @function MyAvatar.getHeadFinalYaw
     * @returns {number} The yaw of the avatar's head, in degrees.
     */
    Q_INVOKABLE float getHeadFinalYaw() const { return getHead()->getFinalYaw(); }

    /*@jsdoc
     * Gets the roll of the avatar's head relative to its body.
     * @function MyAvatar.getHeadFinalRoll
     * @returns {number} The roll of the avatar's head, in degrees.
     */
    Q_INVOKABLE float getHeadFinalRoll() const { return getHead()->getFinalRoll(); }

    /*@jsdoc
     * Gets the pitch of the avatar's head relative to its body.
     * @function MyAvatar.getHeadFinalPitch
     * @returns {number} The pitch of the avatar's head, in degrees.
     */
    Q_INVOKABLE float getHeadFinalPitch() const { return getHead()->getFinalPitch(); }

    /*@jsdoc
     * If a face tracker is connected and being used, gets the estimated pitch of the user's head scaled. This is scale such 
     * that the avatar looks at the edge of the view frustum when the user looks at the edge of their screen.
     * @function MyAvatar.getHeadDeltaPitch
     * @returns {number} The pitch that the avatar's head should be if a face tracker is connected and being used, otherwise
     *     <code>0</code>, in degrees.
     */
    Q_INVOKABLE float getHeadDeltaPitch() const { return getHead()->getDeltaPitch(); }

    /*@jsdoc
     * Gets the current position of the point directly between the avatar's eyes.
     * @function MyAvatar.getEyePosition
     * @returns {Vec3} The current position of the point directly between the avatar's eyes.
     * @example <caption>Report your avatar's current eye position.</caption>
     * var eyePosition = MyAvatar.getEyePosition();
     * print(JSON.stringify(eyePosition));
     */
    Q_INVOKABLE glm::vec3 getEyePosition() const { return getHead()->getEyePosition(); }

    /*@jsdoc
     * Gets the position of the avatar your avatar is currently looking at.
     * @function MyAvatar.getTargetAvatarPosition
     * @returns {Vec3} The position of the avatar beeing looked at.
     * @example <caption>Report the position of the avatar you're currently looking at.</caption>
     * print(JSON.stringify(MyAvatar.getTargetAvatarPosition()));
     */
    // FIXME: If not looking at an avatar, the most recently looked-at position is returned. This should be fixed to return 
    // undefined or {NaN, NaN, NaN} or similar.
    Q_INVOKABLE glm::vec3 getTargetAvatarPosition() const { return _targetAvatarPosition; }

    /*@jsdoc
     * Gets information on the avatar your avatar is currently looking at.
     * @function MyAvatar.getTargetAvatar
     * @returns {ScriptAvatar} Information on the avatar being looked at, <code>null</code> if no avatar is being looked at.
     */
    // FIXME: The return type doesn't have a conversion to a script value so the function always returns undefined in 
    // JavaScript. Note: When fixed, JSDoc is needed for the return type.
    Q_INVOKABLE ScriptAvatarData* getTargetAvatar() const;


    /*@jsdoc
     * Gets the position of the avatar's left hand, relative to the avatar, as positioned by a hand controller (e.g., Oculus 
     * Touch or Vive).
     * <p>Note: The Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints 
     * for hand animation.)</p>
     * @function MyAvatar.getLeftHandPosition
     * @returns {Vec3} The position of the left hand in avatar coordinates if positioned by a hand controller, otherwise 
     *     <code>{@link Vec3(0)|Vec3.ZERO}</code>.
     * @example <caption>Report the position of your left hand relative to your avatar.</caption>
     * print(JSON.stringify(MyAvatar.getLeftHandPosition()));
     */
    Q_INVOKABLE glm::vec3 getLeftHandPosition() const;

    /*@jsdoc
     * Gets the position of the avatar's right hand, relative to the avatar, as positioned by a hand controller (e.g., Oculus 
     * Touch or Vive).
     * <p>Note: The Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints 
     * for hand animation.)</p>
     * @function MyAvatar.getRightHandPosition
     * @returns {Vec3} The position of the right hand in avatar coordinates if positioned by a hand controller, otherwise 
     *     <code>{@link Vec3(0)|Vec3.ZERO}</code>.
     * @example <caption>Report the position of your right hand relative to your avatar.</caption>
     * print(JSON.stringify(MyAvatar.getLeftHandPosition()));
     */
    Q_INVOKABLE glm::vec3 getRightHandPosition() const;

    /*@jsdoc
     * Gets the position 0.3m in front of the left hand's position in the direction along the palm, in avatar coordinates, as 
     * positioned by a hand controller.
     * @function MyAvatar.getLeftHandTipPosition
     * @returns {Vec3} The position 0.3m in front of the left hand's position in the direction along the palm, in avatar 
     *     coordinates. If the hand isn't being positioned by a controller, <code>{@link Vec3(0)|Vec3.ZERO}</code> is returned.
     */
    Q_INVOKABLE glm::vec3 getLeftHandTipPosition() const;

    /*@jsdoc
     * Gets the position 0.3m in front of the right hand's position in the direction along the palm, in avatar coordinates, as
     * positioned by a hand controller.
     * @function MyAvatar.getRightHandTipPosition
     * @returns {Vec3} The position 0.3m in front of the right hand's position in the direction along the palm, in avatar 
     *     coordinates. If the hand isn't being positioned by a controller, <code>{@link Vec3(0)|Vec3.ZERO}</code> is returned.
     */
    Q_INVOKABLE glm::vec3 getRightHandTipPosition() const;


    /*@jsdoc
     * Gets the pose (position, rotation, velocity, and angular velocity) of the avatar's left hand as positioned by a 
     * hand controller (e.g., Oculus Touch or Vive).
     * <p>Note: The Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints 
     * for hand animation.) If you are using the Leap Motion, the return value's <code>valid</code> property will be 
     * <code>false</code> and any pose values returned will not be meaningful.</p>
     * @function MyAvatar.getLeftHandPose
     * @returns {Pose} The pose of the avatar's left hand, relative to the avatar, as positioned by a hand controller.
     * @example <caption>Report the pose of your avatar's left hand.</caption>
     * print(JSON.stringify(MyAvatar.getLeftHandPose()));
     */
    Q_INVOKABLE controller::Pose getLeftHandPose() const;

    /*@jsdoc
     * Gets the pose (position, rotation, velocity, and angular velocity) of the avatar's left hand as positioned by a 
     * hand controller (e.g., Oculus Touch or Vive).
     * <p>Note: The Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints 
     * for hand animation.) If you are using the Leap Motion, the return value's <code>valid</code> property will be 
     * <code>false</code> and any pose values returned will not be meaningful.</p>
     * @function MyAvatar.getRightHandPose
     * @returns {Pose} The pose of the avatar's right hand, relative to the avatar, as positioned by a hand controller. 
     * @example <caption>Report the pose of your avatar's right hand.</caption>
     * print(JSON.stringify(MyAvatar.getRightHandPose()));
     */
    Q_INVOKABLE controller::Pose getRightHandPose() const;

    /*@jsdoc
     * Gets the pose (position, rotation, velocity, and angular velocity) of the avatar's left hand, relative to the avatar, as 
     * positioned by a hand controller (e.g., Oculus Touch or Vive), and translated 0.3m along the palm.
     * <p>Note: Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints
     * for hand animation.) If you are using Leap Motion, the return value's <code>valid</code> property will be
     * <code>false</code> and any pose values returned will not be meaningful.</p>
     * @function MyAvatar.getLeftHandTipPose
     * @returns {Pose} The pose of the avatar's left hand, relative to the avatar, as positioned by a hand controller, and 
     *     translated 0.3m along the palm.
     */
    Q_INVOKABLE controller::Pose getLeftHandTipPose() const;

    /*@jsdoc
     * Gets the pose (position, rotation, velocity, and angular velocity) of the avatar's right hand, relative to the avatar, as
     * positioned by a hand controller (e.g., Oculus Touch or Vive), and translated 0.3m along the palm.
     * <p>Note: Leap Motion isn't part of the hand controller input system. (Instead, it manipulates the avatar's joints
     * for hand animation.) If you are using Leap Motion, the return value's <code>valid</code> property will be
     * <code>false</code> and any pose values returned will not be meaningful.</p>
     * @function MyAvatar.getRightHandTipPose
     * @returns {Pose} The pose of the avatar's right hand, relative to the avatar, as positioned by a hand controller, and
     *     translated 0.3m along the palm.
     */
    Q_INVOKABLE controller::Pose getRightHandTipPose() const;

    AvatarWeakPointer getLookAtTargetAvatar() const { return _lookAtTargetAvatar; }
    void updateLookAtTargetAvatar();
    void computeMyLookAtTarget(const AvatarHash& hash);
    void snapOtherAvatarLookAtTargetsToMe(const AvatarHash& hash);
    void clearLookAtTargetAvatar();

    virtual void setJointRotations(const QVector<glm::quat>& jointRotations) override;
    virtual void setJointData(int index, const glm::quat& rotation, const glm::vec3& translation) override;
    virtual void setJointRotation(int index, const glm::quat& rotation) override;
    virtual void setJointTranslation(int index, const glm::vec3& translation) override;
    virtual void clearJointData(int index) override;

    virtual void setJointData(const QString& name, const glm::quat& rotation, const glm::vec3& translation)  override;
    virtual void setJointRotation(const QString& name, const glm::quat& rotation)  override;
    virtual void setJointTranslation(const QString& name, const glm::vec3& translation)  override;
    virtual void clearJointData(const QString& name) override;
    virtual void clearJointsData() override;

    /*@jsdoc
     * Sets and locks a joint's position and orientation.
     * <p><strong>Note:</strong> Only works on the hips joint.</p>
     * @function MyAvatar.pinJoint
     * @param {number} index - The index of the joint.
     * @param {Vec3} position - The position of the joint in world coordinates.
     * @param {Quat} orientation - The orientation of the joint in world coordinates.
     * @returns {boolean} <code>true</code> if the joint was pinned, <code>false</code> if it wasn't.
     */
    Q_INVOKABLE bool pinJoint(int index, const glm::vec3& position, const glm::quat& orientation);

    bool isJointPinned(int index);

    /*@jsdoc
     * Clears a lock on a joint's position and orientation, as set by {@link MyAvatar.pinJoint|pinJoint}.
     * <p><strong>Note:</strong> Only works on the hips joint.</p>
     * @function MyAvatar.clearPinOnJoint
     * @param {number} index - The index of the joint.
     * @returns {boolean} <code>true</code> if the joint was unpinned, <code>false</code> if it wasn't.
     */
    Q_INVOKABLE bool clearPinOnJoint(int index);

    /*@jsdoc
     * Gets the maximum error distance from the most recent inverse kinematics (IK) solution.
     * @function MyAvatar.getIKErrorOnLastSolve
     * @returns {number} The maximum IK error distance.
     */
    Q_INVOKABLE float getIKErrorOnLastSolve() const;

    /*@jsdoc
     * Changes the user's avatar and associated descriptive name.
     * @function MyAvatar.useFullAvatarURL
     * @param {string} fullAvatarURL - The URL of the avatar's <code>.fst</code> file.
     * @param {string} [modelName=""] - Descriptive name of the avatar.
     */
    Q_INVOKABLE void useFullAvatarURL(const QUrl& fullAvatarURL, const QString& modelName = QString());

    /*@jsdoc
     * Gets the complete URL for the current avatar.
     * @function MyAvatar.getFullAvatarURLFromPreferences
     * @returns {string} The full avatar model name.
     * @example <caption>Report the URL for the current avatar.</caption>
     * print(MyAvatar.getFullAvatarURLFromPreferences());
     */
    Q_INVOKABLE QUrl getFullAvatarURLFromPreferences() const { return _fullAvatarURLFromPreferences; }

    /*@jsdoc
     * Gets the full avatar model name for the current avatar.
     * @function MyAvatar.getFullAvatarModelName
     * @returns {string} The full avatar model name.
     * @example <caption>Report the current full avatar model name.</caption>
     * print(MyAvatar.getFullAvatarModelName());
     */
    Q_INVOKABLE QString getFullAvatarModelName() const { return _fullAvatarModelName; }

    void resetFullAvatarURL();

    MyCharacterController* getCharacterController() { return &_characterController; }
    const MyCharacterController* getCharacterController() const { return &_characterController; }

    void updateMotors();
    void prepareForPhysicsSimulation();
    void nextAttitude(glm::vec3 position, glm::quat orientation); // Can be safely called at any time.
    void harvestResultsFromPhysicsSimulation(float deltaTime);

    const QString& getCollisionSoundURL() { return _collisionSoundURL; }
    void setCollisionSoundURL(const QString& url);

    SharedSoundPointer getCollisionSound();
    void setCollisionSound(SharedSoundPointer sound) { _collisionSound = sound; }

    void clearScriptableSettings();

    float getBoomLength() const { return _boomLength; }
    void setBoomLength(float boomLength) { _boomLength = boomLength; }

    float getPitchSpeed() const { return _pitchSpeed; }
    void setPitchSpeed(float speed) { _pitchSpeed = speed; }

    float getYawSpeed() const { return _yawSpeed; }
    void setYawSpeed(float speed) { _yawSpeed = speed; }

    static const float ZOOM_MIN;
    static const float ZOOM_MAX;
    static const float ZOOM_DEFAULT;

    void destroyAnimGraph();

    AudioListenerMode getAudioListenerMode() { return _audioListenerMode; }
    void setAudioListenerMode(AudioListenerMode audioListenerMode);
    glm::vec3 getCustomListenPosition() { return _customListenPosition; }
    void setCustomListenPosition(glm::vec3 customListenPosition) { _customListenPosition = customListenPosition; }
    glm::quat getCustomListenOrientation() { return _customListenOrientation; }
    void setCustomListenOrientation(glm::quat customListenOrientation) { _customListenOrientation = customListenOrientation; }

    virtual void rebuildCollisionShape() override;

    const glm::vec2& getHipToHandController() const { return _hipToHandController; }
    void setHipToHandController(glm::vec2 currentHipToHandController) { _hipToHandController = currentHipToHandController; }
    const glm::vec2& getHeadControllerFacing() const { return _headControllerFacing; }
    void setHeadControllerFacing(glm::vec2 currentHeadControllerFacing) { _headControllerFacing = currentHeadControllerFacing; }
    const glm::vec2& getHeadControllerFacingMovingAverage() const { return _headControllerFacingMovingAverage; }
    void setHeadControllerFacingMovingAverage(glm::vec2 currentHeadControllerFacing) { _headControllerFacingMovingAverage = currentHeadControllerFacing; }
    float getCurrentStandingHeight() const { return _currentStandingHeight; }
    void setCurrentStandingHeight(float newMode) { _currentStandingHeight = newMode; }
    const glm::quat getAverageHeadRotation() const { return _averageHeadRotation; }
    void setAverageHeadRotation(glm::quat rotation) { _averageHeadRotation = rotation; }
    bool getResetMode() const { return _resetMode; }
    void setResetMode(bool hasBeenReset) { _resetMode = hasBeenReset; }

    void setControllerPoseInSensorFrame(controller::Action action, const controller::Pose& pose);
    controller::Pose getControllerPoseInSensorFrame(controller::Action action) const;
    controller::Pose getControllerPoseInWorldFrame(controller::Action action) const;
    controller::Pose getControllerPoseInAvatarFrame(controller::Action action) const;

    bool hasDriveInput() const;

    /*@jsdoc
     * Gets the current avatar entity IDs and their properties.
     * @function MyAvatar.getAvatarEntitiesVariant
     * @returns {MyAvatar.AvatarEntityData[]} The current avatar entity IDs and their properties.
     */
    Q_INVOKABLE QVariantList getAvatarEntitiesVariant();

    void removeWornAvatarEntity(const EntityItemID& entityID);
    void clearWornAvatarEntities();
    bool hasAvatarEntities() const;

    /*@jsdoc
     * Checks whether your avatar is flying.
     * @function MyAvatar.isFlying
     * @returns {boolean} <code>true</code> if your avatar is flying and not taking off or falling, <code>false</code> if not.
     */
    Q_INVOKABLE bool isFlying();

    /*@jsdoc
     * Checks whether your avatar is in the air.
     * @function MyAvatar.isInAir
     * @returns {boolean} <code>true</code> if your avatar is taking off, flying, or falling, otherwise <code>false</code> 
     *     because your avatar is on the ground.
     */
    Q_INVOKABLE bool isInAir();

    /*@jsdoc
     * Sets your preference for flying in your current desktop or HMD display mode. Note that your ability to fly also depends 
     * on whether the domain you're in allows you to fly.
     * @function MyAvatar.setFlyingEnabled
     * @param {boolean} enabled - Set <code>true</code> if you want to enable flying in your current desktop or HMD display 
     *     mode, otherwise set <code>false</code>.
     */
    Q_INVOKABLE void setFlyingEnabled(bool enabled);

    /*@jsdoc
     * Gets your preference for flying in your current desktop or HMD display mode. Note that your ability to fly also depends 
     * on whether the domain you're in allows you to fly.
     * @function MyAvatar.getFlyingEnabled
     * @returns {boolean} <code>true</code> if your preference is to enable flying in your current desktop or HMD display mode, 
     *     otherwise <code>false</code>.
     */
    Q_INVOKABLE bool getFlyingEnabled();

    /*@jsdoc
     * Sets your preference for flying in desktop display mode. Note that your ability to fly also depends on whether the domain 
     * you're in allows you to fly.
     * @function MyAvatar.setFlyingDesktopPref
     * @param {boolean} enabled - Set <code>true</code> if you want to enable flying in desktop display mode, otherwise set 
     *     <code>false</code>.
     */
    Q_INVOKABLE void setFlyingDesktopPref(bool enabled);

    /*@jsdoc
     * Gets your preference for flying in desktop display mode. Note that your ability to fly also depends on whether the domain
     * you're in allows you to fly.
     * @function MyAvatar.getFlyingDesktopPref
     * @returns {boolean} <code>true</code> if your preference is to enable flying in desktop display mode, otherwise 
     *     <code>false</code>.
     */
    Q_INVOKABLE bool getFlyingDesktopPref();

    /*@jsdoc
     * Sets your preference for flying in HMD display mode. Note that your ability to fly also depends on whether the domain
     * you're in allows you to fly.
     * @function MyAvatar.setFlyingHMDPref
     * @param {boolean} enabled - Set <code>true</code> if you want to enable flying in HMD display mode, otherwise set
     *     <code>false</code>.
     */
    Q_INVOKABLE void setFlyingHMDPref(bool enabled);

    /*@jsdoc
     * Gets your preference for flying in HMD display mode. Note that your ability to fly also depends on whether the domain
     * you're in allows you to fly.
     * @function MyAvatar.getFlyingHMDPref
     * @returns {boolean} <code>true</code> if your preference is to enable flying in HMD display mode, otherwise
     *     <code>false</code>.
     */
    Q_INVOKABLE bool getFlyingHMDPref();

    /*@jsdoc
     * Set your preference for hand-relative movement.
     * @function MyAvatar.setHandRelativeMovement
     * @param {number} enabled - Set <code>true</code> if you want to enable hand-relative movement, otherwise set to <code>false</code>.
     *
    */
    Q_INVOKABLE void setMovementReference(int enabled);

    /*@jsdoc
     * Get your preference for hand-relative movement.
     * @function MyAvatar.getHandRelativeMovement
     * @returns {number} <code>true</code> if your preference is for user locomotion to be relative to the direction your
     * controller is pointing, otherwise <code>false</code>.
    */
    Q_INVOKABLE int getMovementReference();

    /*@jsdoc
     * Set the first 'shifting point' for acceleration step function.
     * @function MyAvatar.setDriveGear1
     * @param {number} shiftPoint - Set the first shift point for analog movement acceleration step function, between [0.0, 1.0]. Must be less than or equal to Gear 2.
    */
    Q_INVOKABLE void setDriveGear1(float shiftPoint);

    /*@jsdoc
     * Get the first 'shifting point' for acceleration step function.
     * @function MyAvatar.getDriveGear1
     * @returns {number} Value between [0.0, 1.0].
    */
    Q_INVOKABLE float getDriveGear1();

    /*@jsdoc
    * Set the second 'shifting point' for acceleration step function.
    * @function MyAvatar.setDriveGear2
    * @param {number} shiftPoint - Defines the second shift point for analog movement acceleration step function, between [0, 1]. Must be greater than or equal to Gear 1 and less than or equal to Gear 2.
    */
    Q_INVOKABLE void setDriveGear2(float shiftPoint);

    /*@jsdoc
    * Get the second 'shifting point' for acceleration step function.
    * @function MyAvatar.getDriveGear2
    * @returns {number} Value between [0.0, 1.0].
    */
    Q_INVOKABLE float getDriveGear2();

    /*@jsdoc
    * Set the third 'shifting point' for acceleration step function.
    * @function MyAvatar.setDriveGear3
    * @param {number} shiftPoint - Defines the third shift point for analog movement acceleration step function, between [0, 1]. Must be greater than or equal to Gear 2 and less than or equal to Gear 4.
    */
    Q_INVOKABLE void setDriveGear3(float shiftPoint);

    /*@jsdoc
    * Get the third 'shifting point' for acceleration step function.
    * @function MyAvatar.getDriveGear3
    * @returns {number} Value between [0.0, 1.0].
    */
    Q_INVOKABLE float getDriveGear3();

    /*@jsdoc
    * Set the fourth 'shifting point' for acceleration step function.
    * @function MyAvatar.setDriveGear4
    * @param {number} shiftPoint - Defines the fourth shift point for analog movement acceleration step function, between [0, 1]. Must be greater than Gear 3 and less than Gear 5.
    */
    Q_INVOKABLE void setDriveGear4(float shiftPoint);

    /*@jsdoc
    * Get the fourth 'shifting point' for acceleration step function.
    * @function MyAvatar.getDriveGear4
    * @returns {number} Value between [0.0, 1.0].
    */
    Q_INVOKABLE float getDriveGear4();

    /*@jsdoc
    * Set the fifth 'shifting point' for acceleration step function.
    * @function MyAvatar.setDriveGear5
    * @param {number} shiftPoint - Defines the fifth shift point for analog movement acceleration step function, between [0, 1]. Must be greater than or equal to Gear 4.
    */
    Q_INVOKABLE void setDriveGear5(float shiftPoint);

    /*@jsdoc
    * Get the fifth 'shifting point' for acceleration step function.
    * @function MyAvatar.getDriveGear5
    * @returns {number} Value between [0.0, 1.0].
    */
    Q_INVOKABLE float getDriveGear5();

    void setControlSchemeIndex(int index);

    int getControlSchemeIndex();

    /*@jsdoc
     * Gets the target scale of the avatar. The target scale is the desired scale of the avatar without any restrictions on
     * permissible scale values imposed by the domain.
     * @function MyAvatar.getAvatarScale
     * @returns {number} The target scale for the avatar, range <code>0.005</code> &ndash; <code>1000.0</code>.
     */
    Q_INVOKABLE float getAvatarScale() const;

    /*@jsdoc
     * Sets the target scale of the avatar. The target scale is the desired scale of the avatar without any restrictions on 
     * permissible scale values imposed by the domain. 
     * @function MyAvatar.setAvatarScale
     * @param {number} scale - The target scale for the avatar, range <code>0.005</code> &ndash; <code>1000.0</code>.
     */
    Q_INVOKABLE void setAvatarScale(float scale);

    /*@jsdoc
     * Sets whether the avatar should collide with entities.
     * <p><strong>Note:</strong> A <code>false</code> value won't disable collisions if the avatar is in a zone that disallows 
     * collisionless avatars. However, the <code>false</code> value will be set so that collisions are disabled as soon as the 
     * avatar moves to a position where collisionless avatars are allowed.
     * @function MyAvatar.setCollisionsEnabled
     * @param {boolean} enabled - <code>true</code> to enable the avatar to collide with entities, <code>false</code> to 
     *     disable.
     */
    Q_INVOKABLE void setCollisionsEnabled(bool enabled);

    /*@jsdoc
     * Gets whether the avatar will currently collide with entities.
     * <p><strong>Note:</strong> The avatar will always collide with entities if in a zone that disallows collisionless avatars.
     * @function MyAvatar.getCollisionsEnabled
     * @returns {boolean} <code>true</code> if the avatar will currently collide with entities, <code>false</code> if it won't.
     */
    Q_INVOKABLE bool getCollisionsEnabled();

    /*@jsdoc
     * Sets whether the avatar should collide with other avatars.
     * @function MyAvatar.setOtherAvatarsCollisionsEnabled
     * @param {boolean} enabled - <code>true</code> to enable the avatar to collide with other avatars, <code>false</code> 
     *     to disable.
     */
    Q_INVOKABLE void setOtherAvatarsCollisionsEnabled(bool enabled);

    /*@jsdoc
     * Gets whether the avatar will collide with other avatars.
     * @function MyAvatar.getOtherAvatarsCollisionsEnabled
     * @returns {boolean} <code>true</code> if the avatar will collide with other avatars, <code>false</code> if it won't.
     */
    Q_INVOKABLE bool getOtherAvatarsCollisionsEnabled();

    /*@jsdoc
     * Gets the avatar's collision capsule: a cylinder with hemispherical ends that approximates the extents or the avatar.
     * <p><strong>Warning:</strong> The values returned are in world coordinates but aren't necessarily up to date with the 
     * avatar's current position.</p>
     * @function MyAvatar.getCollisionCapsule
     * @returns {MyAvatar.CollisionCapsule} The avatar's collision capsule.
     */
    Q_INVOKABLE QVariantMap getCollisionCapsule() const;

    /*@jsdoc
     * @function MyAvatar.setCharacterControllerEnabled
     * @param {boolean} enabled - <code>true</code> to enable the avatar to collide with entities, <code>false</code> to
     *     disable.
     * @deprecated This function is deprecated and will be removed. Use {@link MyAvatar.setCollisionsEnabled} instead.
     */
    Q_INVOKABLE void setCharacterControllerEnabled(bool enabled); // deprecated

    /*@jsdoc
     * @function MyAvatar.getCharacterControllerEnabled
     * @returns {boolean} <code>true</code> if the avatar will currently collide with entities, <code>false</code> if it won't.
     * @deprecated This function is deprecated and will be removed. Use {@link MyAvatar.getCollisionsEnabled} instead.
     */
    Q_INVOKABLE bool getCharacterControllerEnabled(); // deprecated

    /*@jsdoc
     * Gets the rotation of a joint relative to the avatar.
     * @comment Different behavior to the Avatar version of this method.
     * @function MyAvatar.getAbsoluteJointRotationInObjectFrame
     * @param {number} index - The index of the joint.
     * @returns {Quat} The rotation of the joint relative to the avatar.
     * @example <caption>Report the rotation of your avatar's head joint relative to your avatar.</caption>
     * var headIndex = MyAvatar.getJointIndex("Head");
     * var headRotation = MyAvatar.getAbsoluteJointRotationInObjectFrame(headIndex);
     * print("Head rotation: " + JSON.stringify(Quat.safeEulerAngles(headRotation))); // Degrees
     */
    virtual glm::quat getAbsoluteJointRotationInObjectFrame(int index) const override;

    /*@jsdoc
     * Gets the translation of a joint relative to the avatar.
     * @comment Different behavior to the Avatar version of this method.
     * @function MyAvatar.getAbsoluteJointTranslationInObjectFrame
     * @param {number} index - The index of the joint.
     * @returns {Vec3} The translation of the joint relative to the avatar.
     * @example <caption>Report the translation of your avatar's head joint relative to your avatar.</caption>
     * var headIndex = MyAvatar.getJointIndex("Head");
     * var headTranslation = MyAvatar.getAbsoluteJointTranslationInObjectFrame(headIndex);
     * print("Head translation: " + JSON.stringify(headTranslation));
     */
    virtual glm::vec3 getAbsoluteJointTranslationInObjectFrame(int index) const override;

    // all calibration matrices are in absolute sensor space.
    glm::mat4 getCenterEyeCalibrationMat() const;
    glm::mat4 getHeadCalibrationMat() const;
    glm::mat4 getSpine2CalibrationMat() const;
    glm::mat4 getHipsCalibrationMat() const;
    glm::mat4 getLeftFootCalibrationMat() const;
    glm::mat4 getRightFootCalibrationMat() const;
    glm::mat4 getRightArmCalibrationMat() const;
    glm::mat4 getLeftArmCalibrationMat() const;
    glm::mat4 getLeftHandCalibrationMat() const;
    glm::mat4 getRightHandCalibrationMat() const;

    void addHoldAction(AvatarActionHold* holdAction);  // thread-safe
    void removeHoldAction(AvatarActionHold* holdAction);  // thread-safe
    void updateHoldActions(const AnimPose& prePhysicsPose, const AnimPose& postUpdatePose);


    // derive avatar body position and orientation from the current HMD Sensor location.
    // results are in sensor frame (-z forward)
    glm::mat4 deriveBodyFromHMDSensor(const bool forceFollowYPos = false) const;

    glm::mat4 getSpine2RotationRigSpace() const;

    glm::vec3 computeCounterBalance();

    // derive avatar body position and orientation from using the current HMD Sensor location in relation to the previous
    // location of the base of support of the avatar.
    // results are in sensor frame (-z foward)
    glm::mat4 deriveBodyUsingCgModel();

    /*@jsdoc
     * Tests whether a vector is pointing in the general direction of the avatar's "up" direction (i.e., dot product of vectors 
     *     is <code>&gt; 0</code>).
     * @function MyAvatar.isUp
     * @param {Vec3} direction - The vector to test.
     * @returns {boolean} <code>true</code> if the direction vector is pointing generally in the direction of the avatar's "up" 
     *     direction.
     */
    Q_INVOKABLE bool isUp(const glm::vec3& direction) { return glm::dot(direction, _worldUpDirection) > 0.0f; }; // true iff direction points up wrt avatar's definition of up.

    /*@jsdoc
     * Tests whether a vector is pointing in the general direction of the avatar's "down" direction (i.e., dot product of 
     *     vectors is  <code>&lt; 0</code>).
     * @function MyAvatar.isDown
     * @param {Vec3} direction - The vector to test.
     * @returns {boolean} <code>true</code> if the direction vector is pointing generally in the direction of the avatar's 
     *     "down" direction.
     */
    Q_INVOKABLE bool isDown(const glm::vec3& direction) { return glm::dot(direction, _worldUpDirection) < 0.0f; };

    void setUserHeight(float value);
    float getUserHeight() const;
    float getUserEyeHeight() const;

    virtual SpatialParentTree* getParentTree() const override;
    virtual glm::vec3 scaleForChildren() const override { return glm::vec3(getSensorToWorldScale()); }

    const QUuid& getSelfID() const { return AVATAR_SELF_ID; }

    void setIsInWalkingState(bool isWalking);
    bool getIsInWalkingState() const;
    void setIsInSittingState(bool isSitting);
    bool getIsInSittingState() const;
    void setUserRecenterModel(MyAvatar::SitStandModelType modelName);  // Deprecated, will be removed.
    MyAvatar::SitStandModelType getUserRecenterModel() const;          // Deprecated, will be removed.
    void setIsSitStandStateLocked(bool isLocked);                      // Deprecated, will be removed.
    bool getIsSitStandStateLocked() const;                             // Deprecated, will be removed.
    void setAllowAvatarStandingPreference(const AllowAvatarStandingPreference preference);
    AllowAvatarStandingPreference getAllowAvatarStandingPreference() const;
    void setAllowAvatarLeaningPreference(const AllowAvatarLeaningPreference preference);
    AllowAvatarLeaningPreference getAllowAvatarLeaningPreference() const;
    void setWalkSpeed(float value);
    float getWalkSpeed() const;
    void setWalkBackwardSpeed(float value);
    float getWalkBackwardSpeed() const;
    void setSprintSpeed(float value);
    float getSprintSpeed() const;
    void setAnalogWalkSpeed(float value);
    float getAnalogWalkSpeed() const;
    void setAnalogSprintSpeed(float value);
    float getAnalogSprintSpeed() const;
    void setAnalogPlusWalkSpeed(float value);
    float getAnalogPlusWalkSpeed() const;
    void setAnalogPlusSprintSpeed(float value);
    float getAnalogPlusSprintSpeed() const;
    void setSitStandStateChange(bool stateChanged);
    bool getSitStandStateChange() const;
    void updateSitStandState(float newHeightReading, float dt);

    QVector<QString> getScriptUrls();

    bool isReadyForPhysics() const;

    float computeStandingHeightMode(const controller::Pose& head);
    glm::quat computeAverageHeadRotation(const controller::Pose& head);

    virtual void setAttachmentData(const QVector<AttachmentData>& attachmentData) override;
    virtual QVector<AttachmentData> getAttachmentData() const override;

    virtual QVariantList getAttachmentsVariant() const override;
    virtual void setAttachmentsVariant(const QVariantList& variant) override;

    glm::vec3 getNextPosition() { return _goToPending ? _goToPosition : getWorldPosition(); }
    void prepareAvatarEntityDataForReload();

    /*@jsdoc
     * Turns the avatar's head until it faces the target point within a +90/-90 degree range.
     * Once this method is called, API calls have full control of the head for a limited time.
     * If this method is not called for 2 seconds, the engine regains control of the head.
     * @function MyAvatar.setHeadLookAt
     * @param {Vec3} lookAtTarget - The target point in world coordinates.
     */
    Q_INVOKABLE void setHeadLookAt(const glm::vec3& lookAtTarget);

    /*@jsdoc
     * Gets the current target point of the head's look direction in world coordinates.
     * @function MyAvatar.getHeadLookAt
     * @returns {Vec3} The head's look-at target in world coordinates.
     */
    Q_INVOKABLE glm::vec3 getHeadLookAt() { return _lookAtCameraTarget; }

    /*@jsdoc
     * Returns control of the avatar's head to the engine, and releases control from API calls.
     * @function MyAvatar.releaseHeadLookAtControl
     */
    Q_INVOKABLE void releaseHeadLookAtControl();

    /*@jsdoc
     * Forces the avatar's eyes to look at a specified location. Once this method is called, API calls
     * full control of the eyes for a limited time. If this method is not called for 2 seconds, 
     * the engine regains control of the eyes.
     * @function MyAvatar.setEyesLookAt
     * @param {Vec3} lookAtTarget - The target point in world coordinates.
     */
    Q_INVOKABLE void setEyesLookAt(const glm::vec3& lookAtTarget);

    /*@jsdoc
     * Gets the current target point of the eyes look direction in world coordinates.
     * @function MyAvatar.getEyesLookAt
     * @returns {Vec3} The eyes' look-at target in world coordinates.
     */
    Q_INVOKABLE glm::vec3 getEyesLookAt() { return _eyesLookAtTarget.get(); }

    /*@jsdoc
     * Returns control of the avatar's eyes to the engine, and releases control from API calls.
     * @function MyAvatar.releaseEyesLookAtControl
     */
    Q_INVOKABLE void releaseEyesLookAtControl();

    /*@jsdoc
     * Sets the point-at target for the <code>"point"</code> reaction that may be started with {@link MyAvatar.beginReaction}. 
     * The point-at target is set only if it is in front of the avatar.
     * <p>Note: The <code>"point"</code> reaction should be started before calling this method.</p>
     * @function MyAvatar.setPointAt
     * @param {Vec3} pointAtTarget - The target to point at, in world coordinates.
     * @returns {boolean} <code>true</code> if the target point was set, <code>false</code> if it wasn't.
     */
    Q_INVOKABLE bool setPointAt(const glm::vec3& pointAtTarget);

    glm::quat getLookAtRotation() { return _lookAtYaw * _lookAtPitch; }

    /*@jsdoc
     * Creates a new grab that grabs an entity.
     * @function MyAvatar.grab
     * @param {Uuid} targetID - The ID of the entity to grab.
     * @param {number} parentJointIndex - The avatar joint to use to grab the entity.
     * @param {Vec3} offset - The target's local position relative to the joint.
     * @param {Quat} rotationalOffset - The target's local rotation relative to the joint.
     * @returns {Uuid} The ID of the new grab.
     * @example <caption>Create and grab an entity for a short while.</caption>
     * var entityPosition = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 }));
     * var entityRotation = MyAvatar.orientation;
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: entityPosition,
     *     rotation: entityRotation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 }
     * });
     * var rightHandJoint = MyAvatar.getJointIndex("RightHand");
     * var relativePosition = Entities.worldToLocalPosition(entityPosition, MyAvatar.SELF_ID, rightHandJoint);
     * var relativeRotation = Entities.worldToLocalRotation(entityRotation, MyAvatar.SELF_ID, rightHandJoint);
     * var grabID = MyAvatar.grab(entityID, rightHandJoint, relativePosition, relativeRotation);
     *
     * Script.setTimeout(function () {
     *     MyAvatar.releaseGrab(grabID);
     *     Entities.deleteEntity(entityID);
     * }, 10000);
     */
    Q_INVOKABLE const QUuid grab(const QUuid& targetID, int parentJointIndex,
                                 glm::vec3 positionalOffset, glm::quat rotationalOffset);

    /*@jsdoc
     * Releases (deletes) a grab to stop grabbing an entity.
     * @function MyAvatar.releaseGrab
     * @param {Uuid} grabID - The ID of the grab to release.
     */
    Q_INVOKABLE void releaseGrab(const QUuid& grabID);

    /*@jsdoc
     * Gets details of all avatar entities.
     * <p><strong>Warning:</strong> Potentially an expensive call. Do not use if possible.</p>
     * @function MyAvatar.getAvatarEntityData
     * @returns {AvatarEntityMap} Details of all avatar entities.
     * @example <caption>Report the current avatar entities.</caption>
     * var avatarEntityData = MyAvatar.getAvatarEntityData();
     * print("Avatar entities: " + JSON.stringify(avatarEntityData));
     */
    AvatarEntityMap getAvatarEntityData() const override;

    AvatarEntityMap getAvatarEntityDataNonDefault() const override;

    /*@jsdoc
     * Sets all avatar entities from an object.
     * @function MyAvatar.setAvatarEntityData
     * @param {AvatarEntityMap} avatarEntityData - Details of the avatar entities.
     */
    void setAvatarEntityData(const AvatarEntityMap& avatarEntityData) override;

    /*@jsdoc
     * @comment Uses the base class's JSDoc.
     */
    void updateAvatarEntity(const QUuid& entityID, const QByteArray& entityData) override;

    void avatarEntityDataToJson(QJsonObject& root) const override;

    void storeAvatarEntityDataPayload(const QUuid& entityID, const QByteArray& payload) override;

    /*@jsdoc
     * @comment Uses the base class's JSDoc.
     */
    int sendAvatarDataPacket(bool sendAll = false) override;
    
    void addAvatarHandsToFlow(const std::shared_ptr<Avatar>& otherAvatar);

    /*@jsdoc
     * Enables and disables flow simulation of physics on the avatar's hair, clothes, and body parts. See 
     * {@link https://docs.vircadia.dev/create/avatars/add-flow.html|Add Flow to Your Avatar} for more 
     * information.
     * @function MyAvatar.useFlow
     * @param {boolean} isActive - <code>true</code> if flow simulation is enabled on the joint, <code>false</code> if it isn't.
     * @param {boolean} isCollidable - <code>true</code> to enable collisions in the flow simulation, <code>false</code> to 
     *     disable.
     * @param {Object<JointName, MyAvatar.FlowPhysicsOptions>} [physicsConfig] - Physics configurations for particular entity 
     *     and avatar joints.
     * @param {Object<JointName, MyAvatar.FlowCollisionsOptions>} [collisionsConfig] - Collision configurations for particular 
     *     entity and avatar joints.
     */
    Q_INVOKABLE void useFlow(bool isActive, bool isCollidable, const QVariantMap& physicsConfig = QVariantMap(), const QVariantMap& collisionsConfig = QVariantMap());

    /*@jsdoc
     * Gets the current flow configuration.
     * @function MyAvatar.getFlowData
     * @returns {MyAvatar.FlowData}
     */
    Q_INVOKABLE QVariantMap getFlowData();

    /*@jsdoc
     * Gets the indexes of currently colliding flow joints.
     * @function MyAvatar.getCollidingFlowJoints
     * @returns {number[]} The indexes of currently colliding flow joints.
     */
    Q_INVOKABLE QVariantList getCollidingFlowJoints();

    /*@jsdoc
     * Starts a sitting action for the avatar.
     * @function MyAvatar.beginSit
     * @param {Vec3} position - The position where the avatar should sit.
     * @param {Quat} rotation - The initial orientation of the seated avatar.
     */
    Q_INVOKABLE void beginSit(const glm::vec3& position, const glm::quat& rotation);

    /*@jsdoc
     * Ends a sitting action for the avatar.
     * @function MyAvatar.endSit
     * @param {Vec3} position - The position of the avatar when standing up.
     * @param {Quat} rotation - The orientation of the avatar when standing up.
     */
    Q_INVOKABLE void endSit(const glm::vec3& position, const glm::quat& rotation);

    /*@jsdoc
     * Gets whether the avatar is in a seated pose. The seated pose is set by calling {@link MyAvatar.beginSit}.
     * @function MyAvatar.isSeated
     * @returns {boolean} <code>true</code> if the avatar is in a seated pose, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool isSeated() { return _characterController.getSeated(); }

    int getOverrideJointCount() const;
    bool getFlowActive() const;
    bool getNetworkGraphActive() const;

    void updateEyesLookAtPosition(float deltaTime);

    // sets the reaction enabled and triggered parameters of the passed in params
    // also clears internal reaction triggers
    void updateRigControllerParameters(Rig::ControllerParameters& params);

    // Don't substitute verify-fail:
    virtual const QUrl& getSkeletonModelURL() const override { return _skeletonModelURL; }

    void debugDrawPose(controller::Action action, const char* channelName, float size);

    bool getIsJointOverridden(int jointIndex) const;
    glm::vec3 getLookAtPivotPoint();
    glm::vec3 getCameraEyesPosition(float deltaTime);
    bool isJumping();
    bool getHMDCrouchRecenterEnabled() const;
    bool isAllowedToLean() const;
    bool areFeetTracked() const { return _isBodyPartTracked._feet; };  // Determine if the feet are under direct control.
    bool areHipsTracked() const { return _isBodyPartTracked._hips; };  // Determine if the hips are under direct control.

public slots:

   /*@jsdoc
    * @comment Uses the base class's JSDoc.
    */
    virtual void setSessionUUID(const QUuid& sessionUUID) override;

    /*@jsdoc
     * Increases the avatar's scale by five percent, up to a minimum scale of <code>1000</code>.
     * @function MyAvatar.increaseSize
     * @example <caption>Reset your avatar's size to default then grow it 5 times.</caption>
     * MyAvatar.resetSize();
     *
     * for (var i = 0; i < 5; i++){
     *     print("Growing by 5 percent");
     *     MyAvatar.increaseSize();
     * }
     */
    void increaseSize();

    /*@jsdoc
     * Decreases the avatar's scale by five percent, down to a minimum scale of <code>0.25</code>.
     * @function MyAvatar.decreaseSize
     * @example <caption>Reset your avatar's size to default then shrink it 5 times.</caption>
     * MyAvatar.resetSize();
     *
     * for (var i = 0; i < 5; i++){
     *     print("Shrinking by 5 percent");
     *     MyAvatar.decreaseSize();
     * }
     */
    void decreaseSize();

    /*@jsdoc
     * Resets the avatar's scale back to the default scale of <code>1.0</code>.
     * @function MyAvatar.resetSize
     */
    void resetSize();

    /*@jsdoc
     * @function MyAvatar.animGraphLoaded
     * @deprecated This function is deprecated and will be removed.
     */
    void animGraphLoaded();

    /*@jsdoc
     * Sets the amount of gravity applied to the avatar in the y-axis direction. (Negative values are downward.)
     * @function MyAvatar.setGravity
     * @param {number} gravity - The amount of gravity to be applied to the avatar, in m/s<sup>2</sup>.
     */
    void setGravity(float gravity);

    /*@jsdoc
     * Sets the amount of gravity applied to the avatar in the y-axis direction. (Negative values are downward.) The default 
     * value is <code>-5</code> m/s<sup>2</sup>.
     * @function MyAvatar.getGravity
     * @returns {number} The amount of gravity currently applied to the avatar, in m/s<sup>2</sup>.
     */
    float getGravity();

    /*@jsdoc
     * Moves the avatar to a new position and/or orientation in the domain, with safe landing, while taking into account avatar 
     * leg length.
     * @function MyAvatar.goToFeetLocation
     * @param {Vec3} position - The new position for the avatar, in world coordinates.
     * @param {boolean} [hasOrientation=false] - Set to <code>true</code> to set the orientation of the avatar.
     * @param {Quat} [orientation=Quat.IDENTITY] - The new orientation for the avatar.
     * @param {boolean} [shouldFaceLocation=false] - Set to <code>true</code> to position the avatar a short distance away from
     *      the new position and orientate the avatar to face the position.
     */
    void goToFeetLocation(const glm::vec3& newPosition, bool hasOrientation = false, 
        const glm::quat& newOrientation = glm::quat(), bool shouldFaceLocation = false);

    /*@jsdoc
     * Moves the avatar to a new position and/or orientation in the domain.
     * @function MyAvatar.goToLocation
     * @param {Vec3} position - The new position for the avatar, in world coordinates.
     * @param {boolean} [hasOrientation=false] - Set to <code>true</code> to set the orientation of the avatar.
     * @param {Quat} [orientation=Quat.IDENTITY] - The new orientation for the avatar.
     * @param {boolean} [shouldFaceLocation=false] - Set to <code>true</code> to position the avatar a short distance away from 
     *     the new position and orientate the avatar to face the position.
     * @param {boolean} [withSafeLanding=true] - Set to <code>false</code> to disable safe landing when teleporting.
     */
    void goToLocation(const glm::vec3& newPosition,
                      bool hasOrientation = false, const glm::quat& newOrientation = glm::quat(),
                      bool shouldFaceLocation = false, bool withSafeLanding = true);
    /*@jsdoc
     * Moves the avatar to a new position and (optional) orientation in the domain, with safe landing.
     * @function MyAvatar.goToLocation
     * @param {MyAvatar.GoToProperties} target - The goto target.
     */
    void goToLocation(const QVariant& properties);

    /*@jsdoc
     * Moves the avatar to a new position, with safe landing, and enables collisions.
     * @function MyAvatar.goToLocationAndEnableCollisions
     * @param {Vec3} position - The new position for the avatar, in world coordinates.
     */
    void goToLocationAndEnableCollisions(const glm::vec3& newPosition);

    /*@jsdoc
     * @function MyAvatar.safeLanding
     * @param {Vec3} position -The new position for the avatar, in world coordinates.
     * @returns {boolean} <code>true</code> if the avatar was moved, <code>false</code> if it wasn't.
     * @deprecated This function is deprecated and will be removed.
     */
    bool safeLanding(const glm::vec3& position);


    /*@jsdoc
     * @function MyAvatar.restrictScaleFromDomainSettings
     * @param {object} domainSettings - Domain settings.
     * @deprecated This function is deprecated and will be removed.
     */
    void restrictScaleFromDomainSettings(const QJsonObject& domainSettingsObject);

    /*@jsdoc
     * @function MyAvatar.clearScaleRestriction
     * @deprecated This function is deprecated and will be removed.
     */
    void clearScaleRestriction();


    /*@jsdoc
     * Adds a thrust to your avatar's current thrust to be applied for a short while.
     * @function MyAvatar.addThrust
     * @param {Vec3} thrust - The thrust direction and magnitude.
     * @deprecated This function is deprecated and will be removed. Use {@link MyAvatar|MyAvatar.motorVelocity} and related 
     *     properties instead.
     */
    //  Set/Get update the thrust that will move the avatar around
    void addThrust(glm::vec3 newThrust) { _thrust += newThrust; };

    /*@jsdoc
     * Gets the thrust currently being applied to your avatar.
     * @function MyAvatar.getThrust
     * @returns {Vec3} The thrust currently being applied to your avatar.
     * @deprecated This function is deprecated and will be removed. Use {@link MyAvatar|MyAvatar.motorVelocity} and related 
     *     properties instead.
     */
    glm::vec3 getThrust() { return _thrust; };

    /*@jsdoc
     * Sets the thrust to be applied to your avatar for a short while.
     * @function MyAvatar.setThrust
     * @param {Vec3} thrust - The thrust direction and magnitude.
     * @deprecated This function is deprecated and will be removed. Use {@link MyAvatar|MyAvatar.motorVelocity} and related 
     *     properties instead.
     */
    void setThrust(glm::vec3 newThrust) { _thrust = newThrust; }


    /*@jsdoc
     * Updates avatar motion behavior from the Developer &gt; Avatar &gt; Enable Default Motor Control and Enable Scripted 
     * Motor Control menu items.
     * @function MyAvatar.updateMotionBehaviorFromMenu
     */
    Q_INVOKABLE void updateMotionBehaviorFromMenu();

    /*@jsdoc
     * @function MyAvatar.setToggleHips
     * @param {boolean} enabled - Enabled.
     * @deprecated This function is deprecated and will be removed.
     */
    void setToggleHips(bool followHead);

    /*@jsdoc
     * Displays the base of support area debug graphics if in HMD mode. If your head goes outside this area your avatar's hips 
     * are moved to counterbalance your avatar, and if your head moves too far then your avatar's position is moved (i.e., a 
     * step happens).
     * @function MyAvatar.setEnableDebugDrawBaseOfSupport
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawBaseOfSupport(bool isEnabled);

    /*@jsdoc
     * Displays default pose debug graphics.
     * @function MyAvatar.setEnableDebugDrawDefaultPose
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawDefaultPose(bool isEnabled);

    /*@jsdoc
     * Displays animation debug graphics. By default, the animation poses used for rendering are displayed. However, 
     * {@link MyAvatar.setDebugDrawAnimPoseName} can be used to set a specific animation node to display.
     * @function MyAvatar.setEnableDebugDrawAnimPose
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawAnimPose(bool isEnabled);

    /*@jsdoc
     * Sets the animation node to display when animation debug graphics are enabled with 
     * {@link MyAvatar.setEnableDebugDrawAnimPose}.
     * @function MyAvatar.setDebugDrawAnimPoseName
     * @param {string} poseName - The name of the animation node to display debug graphics for. Use <code>""</code> to reset to 
     *     default.
     */
    void setDebugDrawAnimPoseName(QString poseName);

    /*@jsdoc
     * Displays position debug graphics.
     * @function MyAvatar.setEnableDebugDrawPosition
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawPosition(bool isEnabled);

    /*@jsdoc
     * Displays controller hand target debug graphics.
     * @function MyAvatar.setEnableDebugDrawHandControllers
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawHandControllers(bool isEnabled);

    /*@jsdoc
     * Displays sensor-to-world matrix debug graphics.
     * @function MyAvatar.setEnableDebugDrawSensorToWorldMatrix
     * @param {boolean} enable - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawSensorToWorldMatrix(bool isEnabled);

    /*@jsdoc
     * Displays inverse kinematics targets debug graphics.
     * @function MyAvatar.setEnableDebugDrawIKTargets
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawIKTargets(bool isEnabled);

    /*@jsdoc
     * Displays inverse kinematics constraints debug graphics.
     * @function MyAvatar.setEnableDebugDrawIKConstraints
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawIKConstraints(bool isEnabled);

    /*@jsdoc
     * Displays inverse kinematics chains debug graphics.
     * @function MyAvatar.setEnableDebugDrawIKChains
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawIKChains(bool isEnabled);

    /*@jsdoc
     * Displays detailed collision debug graphics.
     * @function MyAvatar.setEnableDebugDrawDetailedCollision
     * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
     */
    void setEnableDebugDrawDetailedCollision(bool isEnabled);

    /*@jsdoc
     * Gets whether your avatar mesh is visible.
     * @function MyAvatar.getEnableMeshVisible
     * @returns {boolean} <code>true</code> if your avatar's mesh is visible, otherwise <code>false</code>.
     */
    bool getEnableMeshVisible() const override;

    /*@jsdoc
     * @comment Uses the base class's JSDoc.
     */
    void clearAvatarEntity(const QUuid& entityID, bool requiresRemovalFromTree = true) override;

    /*@jsdoc
     * @function MyAvatar.sanitizeAvatarEntityProperties
     * @param {EntityItemProperties} properties - Properties.
     * @deprecated This function is deprecated and will be removed.
     */
    void sanitizeAvatarEntityProperties(EntityItemProperties& properties) const;

    /*@jsdoc
     * Sets whether your avatar mesh is visible to you.
     * @function MyAvatar.setEnableMeshVisible
     * @param {boolean} enabled - <code>true</code> to show your avatar mesh, <code>false</code> to hide.
     * @example <caption>Make your avatar invisible for 10s.</caption>
     * MyAvatar.setEnableMeshVisible(false);
     * Script.setTimeout(function () {
     *     MyAvatar.setEnableMeshVisible(true);
     * }, 10000);
     */
    virtual void setEnableMeshVisible(bool isEnabled) override;

    /*@jsdoc
     * Sets whether inverse kinematics (IK) is enabled for your avatar.
     * @function MyAvatar.setEnableInverseKinematics
     * @param {boolean} enabled - <code>true</code> to enable IK, <code>false</code> to disable.
     */
    void setEnableInverseKinematics(bool isEnabled);


    /*@jsdoc
     * Gets the URL of the override animation graph.
     * <p>See {@link https://docs.vircadia.dev/create/avatars/custom-animations.html|Custom Avatar Animations} for
     * information on animation graphs.</p>
     * @function MyAvatar.getAnimGraphOverrideUrl
     * @returns {string} The URL of the override animation graph JSON file. <code>""</code> if there is no override animation 
     *     graph.
     */
    QUrl getAnimGraphOverrideUrl() const;  // thread-safe

    /*@jsdoc
     * Sets the animation graph to use in preference to the default animation graph.
     * <p>See {@link https://docs.vircadia.dev/create/avatars/custom-animations.html|Custom Avatar Animations} for
     * information on animation graphs.</p>
     * @function MyAvatar.setAnimGraphOverrideUrl
     * @param {string} url - The URL of the animation graph JSON file to use. Set to <code>""</code> to clear an override.
     */
    void setAnimGraphOverrideUrl(QUrl value);  // thread-safe

    /*@jsdoc
     * Gets the URL of animation graph (i.e., the avatar animation JSON) that's currently being used for avatar animations.
     * <p>See {@link https://docs.vircadia.dev/create/avatars/custom-animations.html|Custom Avatar Animations} for 
     * information on animation graphs.</p>
     * @function MyAvatar.getAnimGraphUrl
     * @returns {string} The URL of the current animation graph JSON file.
     * @Example <caption>Report the current avatar animation JSON being used.</caption>
     * print(MyAvatar.getAnimGraphUrl());
     */
    QUrl getAnimGraphUrl() const;  // thread-safe

    /*@jsdoc
     * Sets the current animation graph  (i.e., the avatar animation JSON) to use for avatar animations and makes it the default.
     * <p>See {@link https://docs.vircadia.dev/create/avatars/custom-animations.html|Custom Avatar Animations} for
     * information on animation graphs.</p>
     * @function MyAvatar.setAnimGraphUrl
     * @param {string} url - The URL of the animation graph JSON file to use.
     */
    void setAnimGraphUrl(const QUrl& url);  // thread-safe

    /*@jsdoc
     * Gets your listening position for spatialized audio. The position depends on the value of the
     * {@link Myavatar|audioListenerMode} property.
     * @function MyAvatar.getPositionForAudio
     * @returns {Vec3} Your listening position.
     */
    glm::vec3 getPositionForAudio();

    /*@jsdoc
     * Gets the orientation of your listening position for spatialized audio. The orientation depends on the value of the 
     * {@link Myavatar|audioListenerMode} property.
     * @function MyAvatar.getOrientationForAudio
     * @returns {Quat} The orientation of your listening position.
     */
    glm::quat getOrientationForAudio();

    /*@jsdoc
     * @function MyAvatar.setModelScale
     * @param {number} scale - The scale.
     * @deprecated This function is deprecated and will be removed.
     */
    virtual void setModelScale(float scale) override;

    /*@jsdoc
     * Gets the list of reactions names that can be triggered using {@link MyAvatar.triggerReaction}.
     * <p>See also: {@link MyAvatar.getBeginEndReactions}.
     * @function MyAvatar.getTriggerReactions
     * @returns {string[]} List of reaction names that can be triggered using {@link MyAvatar.triggerReaction}.
     * @example <caption>List the available trigger reactions.</caption>
     * print("Trigger reactions:", JSON.stringify(MyAvatar.getTriggerReactions()));
     */
    QStringList getTriggerReactions() const;


    /*@jsdoc
     * Gets the list of reactions names that can be enabled using {@link MyAvatar.beginReaction} and 
     * {@link MyAvatar.endReaction}.
     * <p>See also: {@link MyAvatar.getTriggerReactions}.
     * @function MyAvatar.getBeginEndReactions
     * @returns {string[]} List of reaction names that can be enabled using {@link MyAvatar.beginReaction} and
     *     {@link MyAvatar.endReaction}.
     * @example <caption>List the available begin-end reactions.</caption>
     * print("Begin-end reactions:", JSON.stringify(MyAvatar.getBeginEndReactions()));
     */
    QStringList getBeginEndReactions() const;

    /*@jsdoc
     * Plays a reaction on the avatar. Once the reaction is complete it will stop playing.
     * <p>Only reaction names returned by {@link MyAvatar.getTriggerReactions} are available.</p>
     * @function MyAvatar.triggerReaction
     * @param {string} reactionName - The reaction to trigger.
     * @returns {boolean} <code>true</code> if the reaction was played, <code>false</code> if the reaction is not supported.
     */
    bool triggerReaction(QString reactionName);

    /*@jsdoc
     * Starts playing a reaction on the avatar. The reaction will continue to play until stopped using 
     * {@link MyAvatar.endReaction} or superseded by another reaction.
     * <p>Only reactions returned by {@link MyAvatar.getBeginEndReactions} are available.</p>
     * @function MyAvatar.beginReaction
     * @param {string} reactionName - The reaction to start playing.
     * @returns {boolean} <code>true</code> if the reaction was started, <code>false</code> if the reaction is not supported.
     */
    bool beginReaction(QString reactionName);

    /*@jsdoc
     * Stops playing a reaction that was started using {@link MyAvatar.beginReaction}.
     * @function MyAvatar.endReaction
     * @param {string} reactionName - The reaction to stop playing.
     * @returns {boolean} <code>true</code> if the reaction was stopped, <code>false</code> if the reaction is not supported.
     */
    bool endReaction(QString reactionName);

signals:

    /*@jsdoc
     * Triggered when the {@link MyAvatar|audioListenerMode} property value changes.
     * @function MyAvatar.audioListenerModeChanged
     * @returns {Signal} 
     */
    void audioListenerModeChanged();

    /*@jsdoc
     * Triggered when the walk speed set for the "AnalogPlus" control scheme changes.
     * @function MyAvatar.analogPlusWalkSpeedChanged
     * @param {number} speed - The new walk speed set for the "AnalogPlus" control scheme.
     * @returns {Signal} 
     */
    void analogPlusWalkSpeedChanged(float value);

    /*@jsdoc
     * Triggered when the sprint (run) speed set for the "AnalogPlus" control scheme changes.
     * @function MyAvatar.analogPlusSprintSpeedChanged
     * @param {number} speed - The new sprint speed set for the "AnalogPlus" control scheme.
     * @returns {Signal} 
     */
    void analogPlusSprintSpeedChanged(float value);

    /*@jsdoc
     * Triggered when the sprint (run) speed set for the current control scheme (see 
     * {@link MyAvatar.getControlScheme|getControlScheme}) changes.
     * @function MyAvatar.sprintSpeedChanged
     * @param {number} speed -The new sprint speed set for the current control scheme.
     * @returns {Signal} 
     */
    void sprintSpeedChanged(float value);

    /*@jsdoc
     * Triggered when the walk backward speed set for the current control scheme (see 
     * {@link MyAvatar.getControlScheme|getControlScheme}) changes.
     * @function MyAvatar.walkBackwardSpeedChanged
     * @param {number} speed - The new walk backward speed set for the current control scheme.
     * @returns {Signal} 
     */
    void walkBackwardSpeedChanged(float value);

    /*@jsdoc
     * @function MyAvatar.transformChanged
     * @returns {Signal} 
     * @deprecated This signal is deprecated and will be removed.
     */
    void transformChanged();

    /*@jsdoc
     * Triggered when the {@link MyAvatar|collisionSoundURL} property value changes.
     * @function MyAvatar.newCollisionSoundURL
     * @param {string} url - The URL of the new collision sound.
     * @returns {Signal} 
     */
    void newCollisionSoundURL(const QUrl& url);

    /*@jsdoc
     * Triggered when the avatar collides with an entity.
     * @function MyAvatar.collisionWithEntity
     * @param {Collision} collision - Details of the collision.
     * @returns {Signal}
     * @example <caption>Report each time your avatar collides with an entity.</caption>
     * MyAvatar.collisionWithEntity.connect(function (collision) {
     *     print("Your avatar collided with an entity.");
     * });
     */
    void collisionWithEntity(const Collision& collision);

    /*@jsdoc
     * Triggered when collisions with the environment are enabled or disabled.
     * @function MyAvatar.collisionsEnabledChanged
     * @param {boolean} enabled - <code>true</code> if collisions with the environment are enabled, <code>false</code> if 
     *     they're not.
     * @returns {Signal}
     */
    void collisionsEnabledChanged(bool enabled);

    /*@jsdoc
     * Triggered when collisions with other avatars are enabled or disabled.
     * @function MyAvatar.otherAvatarsCollisionsEnabledChanged
     * @param {boolean} enabled - <code>true</code> if collisions with other avatars are enabled, <code>false</code> if they're 
     *     not.
     * @returns {Signal}
     */
    void otherAvatarsCollisionsEnabledChanged(bool enabled);

    /*@jsdoc
     * Triggered when the avatar's animation graph being used changes.
     * @function MyAvatar.animGraphUrlChanged
     * @param {string} url - The URL of the new animation graph JSON file.
     * @returns {Signal}
     * @example <caption>Report when the current avatar animation JSON being used changes.</caption>
     * MyAvatar.animGraphUrlChanged.connect(function (url) {
     *     print("Avatar animation JSON changed to: " + url);
     * });     */
    void animGraphUrlChanged(const QUrl& url);

    /*@jsdoc
     * @function MyAvatar.energyChanged
     * @param {number} energy - Avatar energy.
     * @returns {Signal} 
     * @deprecated This signal is deprecated and will be removed.
     */
    void energyChanged(float newEnergy);

    /*@jsdoc
     * Triggered when the avatar has been moved to a new position by one of the MyAvatar "goTo" functions.
     * @function MyAvatar.positionGoneTo
     * @returns {Signal} 
     */
    // FIXME: Better name would be goneToLocation().
    void positionGoneTo();

    /*@jsdoc
     * Triggered when the avatar's model finishes loading.
     * @function MyAvatar.onLoadComplete
     * @returns {Signal}
     */
    void onLoadComplete();

    /*@jsdoc
     * Triggered when the avatar's model has failed to load.
     * @function MyAvatar.onLoadFailed
     * @returns {Signal}
     */
    void onLoadFailed();

    /*@jsdoc
     * Triggered when your avatar changes from being active to being away.
     * @function MyAvatar.wentAway
     * @returns {Signal} 
     * @example <caption>Report when your avatar goes away.</caption>
     * MyAvatar.wentAway.connect(function () {
     *     print("My avatar went away");
     * });
     * // In desktop mode, pres the Esc key to go away.
     */
    void wentAway();

    /*@jsdoc
     * Triggered when your avatar changes from being away to being active.
     * @function MyAvatar.wentActive
     * @returns {Signal} 
     */
    void wentActive();

    /*@jsdoc
     * Triggered when the avatar's model (i.e., {@link MyAvatar|skeletonModelURL} property value) is changed.
     * <p>Synonym of {@link MyAvatar.skeletonModelURLChanged|skeletonModelURLChanged}.</p>
     * @function MyAvatar.skeletonChanged
     * @returns {Signal} 
     */
    void skeletonChanged();

    /*@jsdoc
     * Triggered when the avatar's dominant hand changes.
     * @function MyAvatar.dominantHandChanged
     * @param {string} hand - The dominant hand: <code>"left"</code> for the left hand, <code>"right"</code> for the right hand.
     * @returns {Signal} 
     */
    void dominantHandChanged(const QString& hand);

    /*@jsdoc
     * Triggered when the HMD alignment for your avatar changes.
     * @function MyAvatar.hmdAvatarAlignmentTypeChanged
     * @param {string} type - <code>"head"</code> if aligning your head and your avatar's head, <code>"eyes"</code> if aligning 
     *     your eyes and your avatar's eyes.
     * @returns {Signal}
     */
    void hmdAvatarAlignmentTypeChanged(const QString& type);

    /*@jsdoc
     * Triggered when the avatar's <code>sensorToWorldScale</code> property value changes.
     * @function MyAvatar.sensorToWorldScaleChanged
     * @param {number} scale - The scale that transforms dimensions in the user's real world to the avatar's size in the virtual 
     *     world.
     * @returns {Signal} 
     */
    void sensorToWorldScaleChanged(float sensorToWorldScale);

    /*@jsdoc
     * Triggered when the a model is attached to or detached from one of the avatar's joints using one of 
     * {@link MyAvatar.attach|attach}, {@link MyAvatar.detachOne|detachOne}, {@link MyAvatar.detachAll|detachAll}, or 
     * {@link MyAvatar.setAttachmentData|setAttachmentData}.
     * @function MyAvatar.attachmentsChanged
     * @returns {Signal} 
     * @deprecated This signal is deprecated and will be removed. Use avatar entities instead.
     */
    void attachmentsChanged();

    /*@jsdoc
     * Triggered when the avatar's size changes. This can be due to the user changing the size of their avatar or the domain 
     * limiting the size of their avatar.
     * @function MyAvatar.scaleChanged
     * @returns {Signal} 
     */
    void scaleChanged();

    /*@jsdoc
     * Triggered when the hand touch effect is enabled or disabled for the avatar.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that
     * it is really touching that object.</p>
     * @function MyAvatar.shouldDisableHandTouchChanged
     * @param {boolean} disabled - <code>true</code> if the hand touch effect is disabled for the avatar, 
     *     <code>false</code> if it isn't disabled.
     * @returns {Signal}
     */
    void shouldDisableHandTouchChanged(bool shouldDisable);

    /*@jsdoc
     * Triggered when the hand touch is enabled or disabled on a specific entity.
     * <p>The hand touch effect makes the avatar's fingers adapt to the shape of any object grabbed, creating the effect that
     * it is really touching that object.</p>
     * @function MyAvatar.disableHandTouchForIDChanged
     * @param {Uuid} entityID - The entity that the hand touch effect has been enabled or disabled for.
     * @param {boolean} disabled - <code>true</code> if the hand touch effect is disabled for the entity,
     *     <code>false</code> if it isn't disabled.
     * @returns {Signal}
     */
    void disableHandTouchForIDChanged(const QUuid& entityID, bool disable);

private slots:
    void leaveDomain();
    void updateCollisionCapsuleCache();

protected:
    void handleChangedAvatarEntityData();
    void handleCanRezAvatarEntitiesChanged(bool canRezAvatarEntities);
    virtual void beParentOfChild(SpatiallyNestablePointer newChild) const override;
    virtual void forgetChild(SpatiallyNestablePointer newChild) const override;
    virtual void recalculateChildCauterization() const override;

private:
    bool updateStaleAvatarEntityBlobs() const;

    bool requiresSafeLanding(const glm::vec3& positionIn, glm::vec3& positionOut);

    virtual QByteArray toByteArrayStateful(AvatarDataDetail dataDetail, bool dropFaceTracking) override;

    void simulate(float deltaTime, bool inView) override;
    void saveAvatarUrl();
    virtual void render(RenderArgs* renderArgs) override;
    virtual bool shouldRenderHead(const RenderArgs* renderArgs) const override;
    void setShouldRenderLocally(bool shouldRender) { _shouldRender = shouldRender; setEnableMeshVisible(shouldRender); }
    bool getShouldRenderLocally() const { return _shouldRender; }
    void setRotationRecenterFilterLength(float length);
    float getRotationRecenterFilterLength() const { return _rotationRecenterFilterLength; }
    void setRotationThreshold(float angleRadians);
    float getRotationThreshold() const { return _rotationThreshold; }
    void setEnableStepResetRotation(bool stepReset) { _stepResetRotationEnabled = stepReset; }
    bool getEnableStepResetRotation() const { return _stepResetRotationEnabled; }
    void setEnableDrawAverageFacing(bool drawAverage) { _drawAverageFacingEnabled = drawAverage; }
    bool getEnableDrawAverageFacing() const { return _drawAverageFacingEnabled; }
    virtual bool isMyAvatar() const override { return true; }
    virtual int parseDataFromBuffer(const QByteArray& buffer) override;
    virtual glm::vec3 getSkeletonPosition() const override;
    int _skeletonModelChangeCount { 0 };

    void saveAvatarScale();

    glm::vec3 getScriptedMotorVelocity() const { return _scriptedMotorVelocity; }
    float getScriptedMotorTimescale() const { return _scriptedMotorTimescale; }
    QString getScriptedMotorFrame() const;
    QString getScriptedMotorMode() const;
    void setScriptedMotorVelocity(const glm::vec3& velocity);
    void setScriptedMotorTimescale(float timescale);
    void setScriptedMotorFrame(QString frame);
    void setScriptedMotorMode(QString mode);

    // Attachments
    virtual void attach(const QString& modelURL, const QString& jointName = QString(),
                        const glm::vec3& translation = glm::vec3(), const glm::quat& rotation = glm::quat(),
                        float scale = 1.0f, bool isSoft = false,
                        bool allowDuplicates = false, bool useSaved = true) override;

    virtual void detachOne(const QString& modelURL, const QString& jointName = QString()) override;
    virtual void detachAll(const QString& modelURL, const QString& jointName = QString()) override;
    
    // Attachments/Avatar Entity
    void attachmentDataToEntityProperties(const AttachmentData& data, EntityItemProperties& properties);
    AttachmentData entityPropertiesToAttachmentData(const EntityItemProperties& properties) const;
    bool findAvatarEntity(const QString& modelURL, const QString& jointName, QUuid& entityID);
    void addAvatarEntitiesToTree();

    // FIXME: Rename to clearAvatarEntity() once the API call is removed.
    void clearAvatarEntityInternal(const QUuid& entityID) override;

    bool cameraInsideHead(const glm::vec3& cameraPosition) const;

    void updateEyeContactTarget(float deltaTime);

    // These are made private for MyAvatar so that you will use the "use" methods instead
    /*@jsdoc
     * @comment Borrows the base class's JSDoc.
     */
    Q_INVOKABLE virtual void setSkeletonModelURL(const QUrl& skeletonModelURL) override;

    virtual void updatePalms() override {}
    void lateUpdatePalms();
    void setSitDriveKeysStatus(bool enabled);

    void clampTargetScaleToDomainLimits();
    void clampScaleChangeToDomainLimits(float desiredScale);
    glm::mat4 computeCameraRelativeHandControllerMatrix(const glm::mat4& controllerSensorMatrix) const;

    std::array<float, MAX_DRIVE_KEYS> _driveKeys;
    std::bitset<MAX_DRIVE_KEYS> _disabledDriveKeys;

    bool _enableFlying { false };
    bool _flyingPrefDesktop { true };
    bool _flyingPrefHMD { true };
    bool _wasPushing { false };
    bool _isPushing { false };
    bool _isBeingPushed { false };
    bool _isBraking { false };
    bool _isAway { false };

    // Indicates which parts of the body are under direct control (tracked).
    struct {
        bool _feet { false };  // Left or right foot.
        bool _feetPreviousUpdate{ false };// Value of _feet on the previous update.
        bool _hips{ false };
        bool _leftHand{ false };
        bool _rightHand{ false };
        bool _head{ false };
    } _isBodyPartTracked;

    float _boomLength { ZOOM_DEFAULT };
    float _yawSpeed; // degrees/sec
    float _pitchSpeed; // degrees/sec
    float _driveGear1 { DEFAULT_GEAR_1 };
    float _driveGear2 { DEFAULT_GEAR_2 };
    float _driveGear3 { DEFAULT_GEAR_3 };
    float _driveGear4 { DEFAULT_GEAR_4 };
    float _driveGear5 { DEFAULT_GEAR_5 };
    int _controlSchemeIndex { CONTROLS_DEFAULT };
    int _movementReference{ 0 };

    glm::vec3 _thrust { 0.0f };  // impulse accumulator for outside sources

    glm::vec3 _actionMotorVelocity; // target local-frame velocity of avatar (default controller actions)
    glm::vec3 _scriptedMotorVelocity; // target local-frame velocity of avatar (analog script)
    float _scriptedMotorTimescale; // timescale for avatar to achieve its target velocity
    int _scriptedMotorFrame;
    int _scriptedMotorMode;
    quint32 _motionBehaviors;
    QString _collisionSoundURL;

    SharedSoundPointer _collisionSound;

    MyCharacterController _characterController;
    int32_t _previousCollisionMask { BULLET_COLLISION_MASK_MY_AVATAR };

    AvatarWeakPointer _lookAtTargetAvatar;
    glm::vec3 _targetAvatarPosition;
    bool _shouldRender { true };
    float _oculusYawOffset;

    eyeContactTarget _eyeContactTarget;
    float _eyeContactTargetTimer { 0.0f };
    ThreadSafeValueCache<glm::vec3> _eyesLookAtTarget { glm::vec3() };
    bool _scriptControlsEyesLookAt{ false };
    float _scriptEyesControlTimer{ 0.0f };

    glm::vec3 _trackedHeadPosition;

    const float MAX_LOOK_AT_TIME_SCRIPT_CONTROL = 2.0f;
    glm::quat _lookAtPitch;
    glm::quat _lookAtYaw;
    float _lookAtYawSpeed { 0.0f };
    glm::vec3 _lookAtCameraTarget;
    glm::vec3 _lookAtScriptTarget;
    bool _headLookAtActive { false };
    bool _shouldTurnToFaceCamera { false };
    bool _scriptControlsHeadLookAt { false };
    float _scriptHeadControlTimer { 0.0f };
    float _firstPersonSteadyHeadTimer { 0.0f };
    bool _pointAtActive { false };
    bool _isPointTargetValid { true };

    Setting::Handle<float> _realWorldFieldOfView;
    Setting::Handle<bool> _useAdvancedMovementControls;
    Setting::Handle<bool> _showPlayArea;

    // Smoothing.
    const float SMOOTH_TIME_ORIENTATION = 0.5f;

    // Smoothing data for blending from one position/orientation to another on remote agents.
    float _smoothOrientationTimer;
    glm::quat _smoothOrientationInitial;
    glm::quat _smoothOrientationTarget;

    // private methods
    void updateOrientation(float deltaTime);
    glm::vec3 calculateScaledDirection();
    float calculateGearedSpeed(const float driveKey);
    glm::vec3 scaleMotorSpeed(const glm::vec3 forward, const glm::vec3 right);
    void updateActionMotor(float deltaTime);
    void updatePosition(float deltaTime);
    void updateViewBoom();
    void updateCollisionSound(const glm::vec3& penetration, float deltaTime, float frequency);
    void initHeadBones();
    void initAnimGraph();
    void initFlowFromFST();
    void updateHeadLookAt(float deltaTime);
    void resetHeadLookAt();
    void resetLookAtRotation(const glm::vec3& avatarPosition, const glm::quat& avatarOrientation);
    void resetPointAt();
    static glm::vec3 aimToBlendValues(const glm::vec3& aimVector, const glm::quat& frameOrientation);
    void centerBodyInternal(const bool forceFollowYPos = false);

    // Avatar Preferences
    QUrl _fullAvatarURLFromPreferences;
    QString _fullAvatarModelName;
    ThreadSafeValueCache<QUrl> _currentAnimGraphUrl;
    ThreadSafeValueCache<QUrl> _prefOverrideAnimGraphUrl;
    QUrl _fstAnimGraphOverrideUrl;
    bool _useSnapTurn { true };
    bool _hoverWhenUnsupported{ true };
    ThreadSafeValueCache<QString> _dominantHand { DOMINANT_RIGHT_HAND };
    ThreadSafeValueCache<QString> _hmdAvatarAlignmentType { DEFAULT_HMD_AVATAR_ALIGNMENT_TYPE };
    ThreadSafeValueCache<bool> _strafeEnabled{ DEFAULT_STRAFE_ENABLED };

    const float ROLL_CONTROL_DEAD_ZONE_DEFAULT = 8.0f; // degrees
    const float ROLL_CONTROL_RATE_DEFAULT = 114.0f; // degrees / sec

    bool _hmdRollControlEnabled { true };
    float _hmdRollControlDeadZone { ROLL_CONTROL_DEAD_ZONE_DEFAULT };
    float _hmdRollControlRate { ROLL_CONTROL_RATE_DEFAULT };
    std::atomic<bool> _hasScriptedBlendShapes { false };
    std::atomic<float> _rotationRecenterFilterLength { 4.0f };
    std::atomic<float> _rotationThreshold { 0.5235f };  // 30 degrees in radians
    std::atomic<bool> _stepResetRotationEnabled { true };
    std::atomic<bool> _drawAverageFacingEnabled { false };

    // working copy -- see AvatarData for thread-safe _sensorToWorldMatrixCache, used for outward facing access
    glm::mat4 _sensorToWorldMatrix { glm::mat4() };

    // cache of the current HMD sensor position and orientation in sensor space.
    glm::mat4 _hmdSensorMatrix;
    glm::quat _hmdSensorOrientation;
    glm::vec3 _hmdSensorPosition;
    // cache head controller pose in sensor space
    glm::vec2 _headControllerFacing;  // facing vector in xz plane (sensor space)
    glm::vec2 _headControllerFacingMovingAverage { 0.0f, 0.0f };   // facing vector in xz plane (sensor space)
    glm::quat _averageHeadRotation { 0.0f, 0.0f, 0.0f, 1.0f };

    glm::vec2 _hipToHandController { 0.0f, 1.0f };  // spine2 facing vector in xz plane (spine2 space)

    float _currentStandingHeight { 0.0f };
    bool _resetMode { true };
    RingBufferHistory<int> _recentModeReadings;

    // cache of the current body position and orientation of the avatar's body,
    // in sensor space.
    glm::mat4 _bodySensorMatrix;

    struct FollowHelper {
        FollowHelper();

        CharacterController::FollowTimePerType _timeRemaining;

        void deactivate();
        void deactivate(CharacterController::FollowType type);
        void activate(CharacterController::FollowType type, const bool snapFollow);
        bool isActive() const;
        bool isActive(CharacterController::FollowType followType) const;
        void decrementTimeRemaining(float dt);
        bool shouldActivateRotation(const MyAvatar& myAvatar, const glm::mat4& desiredBodyMatrix, const glm::mat4& currentBodyMatrix, bool& shouldSnapOut) const;
        bool shouldActivateVertical(const MyAvatar& myAvatar, const glm::mat4& desiredBodyMatrix, const glm::mat4& currentBodyMatrix) const;
        bool shouldActivateHorizontal(const MyAvatar& myAvatar,
                                      const glm::mat4& desiredBodyMatrix,
                                      const glm::mat4& currentBodyMatrix,
                                      bool& resetModeOut,
                                      bool& goToWalkingStateOut) const;
        void prePhysicsUpdate(MyAvatar& myAvatar, const glm::mat4& bodySensorMatrix, const glm::mat4& currentBodyMatrix, bool hasDriveInput);
        glm::mat4 postPhysicsUpdate(MyAvatar& myAvatar, const glm::mat4& currentBodyMatrix);
        bool getForceActivateRotation() const;
        void setForceActivateRotation(bool val);
        bool getForceActivateVertical() const;
        void setForceActivateVertical(bool val);
        bool getForceActivateHorizontal() const;
        void setForceActivateHorizontal(bool val);
        std::atomic<bool> _forceActivateRotation { false };
        std::atomic<bool> _forceActivateVertical { false };
        std::atomic<bool> _forceActivateHorizontal { false };
        std::atomic<bool> _toggleHipsFollowing { true };

    private:
        bool shouldActivateHorizontal_userSitting(const MyAvatar& myAvatar,
                                                  const glm::mat4& desiredBodyMatrix,
                                                  const glm::mat4& currentBodyMatrix) const;
        bool shouldActivateHorizontal_userStanding(const MyAvatar& myAvatar,
                                                   bool& resetModeOut,
                                                   bool& goToWalkingStateOut) const;
    };

    FollowHelper _follow;

    bool isFollowActive(CharacterController::FollowType followType) const;

    bool _goToPending { false };
    bool _physicsSafetyPending { false };
    bool _goToSafe { true };
    bool _goToFeetAjustment { false };
    glm::vec3 _goToPosition;
    glm::quat _goToOrientation;

    std::unordered_set<int> _headBoneSet;
    std::unordered_set<SpatiallyNestablePointer> _cauterizedChildrenOfHead;
    bool _prevShouldDrawHead;
    bool _rigEnabled { true };

    bool _enableDebugDrawBaseOfSupport { false };
    bool _enableDebugDrawDefaultPose { false };
    bool _enableDebugDrawAnimPose { false };
    bool _enableDebugDrawHandControllers { false };
    bool _enableDebugDrawSensorToWorldMatrix { false };
    bool _enableDebugDrawIKTargets { false };
    bool _enableDebugDrawIKConstraints { false };
    bool _enableDebugDrawIKChains { false };
    bool _enableDebugDrawDetailedCollision { false };

    ThreadSafeValueCache<QString> _debugDrawAnimPoseName;

    mutable bool _cauterizationNeedsUpdate { false }; // do we need to scan children and update their "cauterized" state?

    AudioListenerMode _audioListenerMode;
    glm::vec3 _customListenPosition;
    glm::quat _customListenOrientation;

    AtRestDetector _leftHandAtRestDetector;
    AtRestDetector _rightHandAtRestDetector;

    // all poses are in sensor-frame
    std::map<controller::Action, controller::Pose> _controllerPoseMap;
    mutable std::mutex _controllerPoseMapMutex;
    mutable std::mutex _disableHandTouchMutex;

    bool _centerOfGravityModelEnabled { true };
    bool _hmdLeanRecenterEnabled { true };
    bool _hmdCrouchRecenterEnabled {
        true
    };  // Is MyAvatar allowed to recenter vertically (stand) when the user is sitting in the real world.
    bool _sprint { false };

    AnimPose _prePhysicsRoomPose;
    std::mutex _holdActionsMutex;
    std::vector<AvatarActionHold*> _holdActions;

    float AVATAR_MOVEMENT_ENERGY_CONSTANT { 0.001f };
    float AUDIO_ENERGY_CONSTANT { 0.000001f };
    float MAX_AVATAR_MOVEMENT_PER_FRAME { 30.0f };
    float currentEnergy { 0.0f };
    float energyChargeRate { 0.003f };
    glm::vec3 priorVelocity;
    glm::vec3 lastPosition;
    float getAudioEnergy();
    float getAccelerationEnergy();
    float getEnergy();
    void setEnergy(float value);
    bool didTeleport();
    bool getIsAway() const { return _isAway; }
    void setAway(bool value);
    void sendPacket(const QUuid& entityID) const override;

    std::mutex _pinnedJointsMutex;
    std::vector<int> _pinnedJoints;

    void updateChildCauterization(SpatiallyNestablePointer object, bool cauterize);

    // height of user in sensor space, when standing erect.
    ThreadSafeValueCache<float> _userHeight { DEFAULT_AVATAR_HEIGHT };
    float _averageUserHeightSensorSpace { _userHeight.get() };
    bool _sitStandStateChange { false };

    // max unscaled forward movement speed
    ThreadSafeValueCache<float> _defaultWalkSpeed { DEFAULT_AVATAR_MAX_WALKING_SPEED };
    ThreadSafeValueCache<float> _defaultWalkBackwardSpeed { DEFAULT_AVATAR_MAX_WALKING_BACKWARD_SPEED };
    ThreadSafeValueCache<float> _defaultSprintSpeed { DEFAULT_AVATAR_MAX_SPRINT_SPEED };
    ThreadSafeValueCache<float> _analogWalkSpeed { ANALOG_AVATAR_MAX_WALKING_SPEED };
    ThreadSafeValueCache<float> _analogWalkBackwardSpeed { ANALOG_AVATAR_MAX_WALKING_BACKWARD_SPEED };
    ThreadSafeValueCache<float> _analogSprintSpeed { ANALOG_AVATAR_MAX_SPRINT_SPEED };
    ThreadSafeValueCache<float> _analogPlusWalkSpeed { ANALOG_PLUS_AVATAR_MAX_WALKING_SPEED };
    ThreadSafeValueCache<float> _analogPlusWalkBackwardSpeed { ANALOG_PLUS_AVATAR_MAX_WALKING_BACKWARD_SPEED };
    ThreadSafeValueCache<float> _analogPlusSprintSpeed { ANALOG_PLUS_AVATAR_MAX_SPRINT_SPEED };

    float _walkSpeedScalar { AVATAR_WALK_SPEED_SCALAR };
    bool _isInWalkingState { false };
    ThreadSafeValueCache<bool> _isInSittingState { false };
    ThreadSafeValueCache<MyAvatar::AllowAvatarStandingPreference> _allowAvatarStandingPreference{
        MyAvatar::AllowAvatarStandingPreference::Default
    };  // The user preference of when MyAvatar may stand.
    ThreadSafeValueCache<MyAvatar::AllowAvatarLeaningPreference> _allowAvatarLeaningPreference{
        MyAvatar::AllowAvatarLeaningPreference::Default
    };  // The user preference of when MyAvatar may lean.
    float _sitStandStateTimer { 0.0f };
    float _tippingPoint { _userHeight.get() };

    // load avatar scripts once when rig is ready
    bool _shouldLoadScripts { false };

    bool _haveReceivedHeightLimitsFromDomain { false };
    int _disableHandTouchCount { 0 };
    bool _reloadAvatarEntityDataFromSettings { true };

    TimePoint _nextTraitsSendWindow;

    Setting::Handle<QString> _dominantHandSetting;
    Setting::Handle<bool> _strafeEnabledSetting;
    Setting::Handle<QString> _hmdAvatarAlignmentTypeSetting;
    Setting::Handle<float> _headPitchSetting;
    Setting::Handle<float> _scaleSetting;
    Setting::Handle<float> _yawSpeedSetting;
    Setting::Handle<float> _pitchSpeedSetting;
    Setting::Handle<QUrl> _fullAvatarURLSetting;
    Setting::Handle<QUrl> _fullAvatarModelNameSetting;
    Setting::Handle<QUrl> _animGraphURLSetting;
    Setting::Handle<QString> _displayNameSetting;
    Setting::Handle<QUrl> _collisionSoundURLSetting;
    Setting::Handle<bool> _useSnapTurnSetting;
    Setting::Handle<bool> _hoverWhenUnsupportedSetting;
    Setting::Handle<float> _userHeightSetting;
    Setting::Handle<bool> _flyingHMDSetting;
    Setting::Handle<int> _movementReferenceSetting;
    Setting::Handle<int> _avatarEntityCountSetting;
    Setting::Handle<bool> _allowTeleportingSetting { "allowTeleporting", true };
    Setting::Handle<float> _driveGear1Setting;
    Setting::Handle<float> _driveGear2Setting;
    Setting::Handle<float> _driveGear3Setting;
    Setting::Handle<float> _driveGear4Setting;
    Setting::Handle<float> _driveGear5Setting;
    Setting::Handle<float> _analogWalkSpeedSetting;
    Setting::Handle<float> _analogPlusWalkSpeedSetting;
    Setting::Handle<int> _controlSchemeIndexSetting;
    std::vector<Setting::Handle<QUuid>> _avatarEntityIDSettings;
    std::vector<Setting::Handle<QByteArray>> _avatarEntityDataSettings;
    Setting::Handle<QString> _allowAvatarStandingPreferenceSetting;
    Setting::Handle<QString> _allowAvatarLeaningPreferenceSetting;

    // AvatarEntities stuff:
    // We cache the "map of unfortunately-formatted-binary-blobs" because they are expensive to compute
    // Do not confuse these with AvatarData::_packedAvatarEntityData which are in wire-format.
    mutable AvatarEntityMap _cachedAvatarEntityBlobs;

    // We collect changes to AvatarEntities and then handle them all in one spot per frame: updateAvatarEntities().
    // Basically this is a "transaction pattern" with an extra complication: these changes can come from two
    // "directions" and the "authoritative source" of each direction is different, so maintain two distinct sets of
    // transaction lists;
    //
    // The _entitiesToDelete/Add/Update lists are for changes whose "authoritative sources" are already
    // correctly stored in _cachedAvatarEntityBlobs.  These come from loadAvatarEntityDataFromSettings() and
    // setAvatarEntityData().  These changes need to be extracted from _cachedAvatarEntityBlobs and applied to
    // real EntityItems.
    std::vector<EntityItemID> _entitiesToDelete;
    std::vector<EntityItemID> _entitiesToAdd;
    std::vector<EntityItemID> _entitiesToUpdate;
    //
    // The _cachedAvatarEntityBlobsToDelete/Add/Update lists are for changes whose "authoritative sources" are
    // already reflected in real EntityItems. These changes need to be propagated to _cachedAvatarEntityBlobs
    // and eventually to settings.
    std::vector<EntityItemID> _cachedAvatarEntityBlobsToDelete;
    std::vector<EntityItemID> _cachedAvatarEntityBlobsToAddOrUpdate;
    std::vector<EntityItemID> _cachedAvatarEntityBlobUpdatesToSkip;
    //
    // Also these lists for tracking delayed changes to blobs and Settings
    mutable std::set<EntityItemID> _staleCachedAvatarEntityBlobs;
    //
    // keep a ScriptEngine around so we don't have to instantiate on the fly (these are very slow to create/delete)
    mutable std::mutex _scriptEngineLock;
    ScriptEnginePointer _scriptEngine { nullptr };
    bool _needToSaveAvatarEntitySettings { false };

    bool _reactionTriggers[NUM_AVATAR_TRIGGER_REACTIONS] { false, false };
    int _reactionEnabledRefCounts[NUM_AVATAR_BEGIN_END_REACTIONS] { 0, 0, 0 };

    mutable std::mutex _reactionLock;

    // used to prevent character from jumping after endSit is called.
    bool _endSitKeyPressComplete { false };

    glm::vec3 _cameraEyesOffset;
    float _landingAfterJumpTime { 0.0f };

    QTimer _addAvatarEntitiesToTreeTimer;
};

ScriptValuePointer audioListenModeToScriptValue(ScriptEngine* engine, const AudioListenerMode& audioListenerMode);
void audioListenModeFromScriptValue(const ScriptValuePointer& object, AudioListenerMode& audioListenerMode);

ScriptValuePointer driveKeysToScriptValue(ScriptEngine* engine, const MyAvatar::DriveKeys& driveKeys);
void driveKeysFromScriptValue(const ScriptValuePointer& object, MyAvatar::DriveKeys& driveKeys);

bool isWearableEntity(const EntityItemPointer& entity);

#endif // hifi_MyAvatar_h
