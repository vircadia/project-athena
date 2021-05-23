//
//  AvatarData.h
//  libraries/avatars/src
//
//  Created by Stephen Birarda on 4/9/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AvatarData_h
#define hifi_AvatarData_h

#include <string>
#include <memory>
#include <queue>
#include <inttypes.h>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QRect>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>
#include <QVector>
#include <QReadWriteLock>
#include <QtCore/QSharedPointer>

#include <AvatarConstants.h>
#include <JointData.h>
#include <NLPacket.h>
#include <Node.h>
#include <NumericalConstants.h>
#include <Packed.h>
#include <RegisteredMetaTypes.h>
#include <SharedUtil.h>
#include <SimpleMovingAverage.h>
#include <SpatiallyNestable.h>
#include <ThreadSafeValueCache.h>
#include <ViewFrustum.h>
#include <shared/RateCounter.h>
#include <udt/SequenceNumber.h>
#include "Scriptable.h"

#include "AABox.h"
#include "AvatarTraits.h"
#include "HeadData.h"
#include "PathUtils.h"

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

using AvatarSharedPointer = std::shared_ptr<AvatarData>;
using AvatarWeakPointer = std::weak_ptr<AvatarData>;
using AvatarHash = QHash<QUuid, AvatarSharedPointer>;

using AvatarEntityMap = QMap<QUuid, QByteArray>;
using PackedAvatarEntityMap = QMap<QUuid, QByteArray>; // similar to AvatarEntityMap, but different internal format
using AvatarEntityIDs = QSet<QUuid>;

using AvatarGrabDataMap = QMap<QUuid, QByteArray>;

using AvatarDataSequenceNumber = uint16_t;

const int MAX_NUM_AVATAR_ENTITIES = 42;

// avatar motion behaviors
const quint32 AVATAR_MOTION_ACTION_MOTOR_ENABLED = 1U << 0;
const quint32 AVATAR_MOTION_SCRIPTED_MOTOR_ENABLED = 1U << 1;

const quint32 AVATAR_MOTION_DEFAULTS =
        AVATAR_MOTION_ACTION_MOTOR_ENABLED |
        AVATAR_MOTION_SCRIPTED_MOTOR_ENABLED;

// these bits will be expanded as features are exposed
const quint32 AVATAR_MOTION_SCRIPTABLE_BITS =
        AVATAR_MOTION_SCRIPTED_MOTOR_ENABLED;

// Bitset of state flags - we store the key state, hand state, Faceshift, eye tracking, and existence of
// referential data in this bit set. The hand state is an octal, but is split into two sections to maintain
// backward compatibility. The bits are ordered as such (0-7 left to right).
// AA 6/1/18 added three more flags bits 8,9, and 10 for procedural audio, blink, and eye saccade enabled
//
//     +-----+-----+-+-+-+--+--+--+--+-----+
//     |K0,K1|H0,H1|F|E|R|H2|Au|Bl|Ey|xxxxx|
//     +-----+-----+-+-+-+--+--+--+--+-----+
//
// Key state - K0,K1 is found in the 1st and 2nd bits
// Hand state - H0,H1,H2 is found in the 3rd, 4th, and 8th bits
// Face tracker - F is found in the 5th bit
// Eye tracker - E is found in the 6th bit
// Referential Data - R is found in the 7th bit
// Procedural audio to mouth movement is enabled 8th bit
// Procedural Blink is enabled 9th bit
// Procedural Eyelid is enabled 10th bit
// Procedural PROCEDURAL_BLINK_FACE_MOVEMENT is enabled 11th bit
// Procedural Collide with other avatars is enabled 12th bit
// Procedural Has Hero Priority is enabled 13th bit

const int KEY_STATE_START_BIT = 0; // 1st and 2nd bits  (UNUSED)
const int HAND_STATE_START_BIT = 2; // 3rd and 4th bits (UNUSED)
const int HAS_SCRIPTED_BLENDSHAPES = 4; // 5th bit
const int HAS_PROCEDURAL_EYE_MOVEMENT = 5; // 6th bit
const int HAS_REFERENTIAL = 6; // 7th bit
const int HAND_STATE_FINGER_POINTING_BIT = 7; // 8th bit (UNUSED)
const int AUDIO_ENABLED_FACE_MOVEMENT = 8; // 9th bit
const int PROCEDURAL_EYE_FACE_MOVEMENT = 9; // 10th bit
const int PROCEDURAL_BLINK_FACE_MOVEMENT = 10; // 11th bit
const int COLLIDE_WITH_OTHER_AVATARS = 11; // 12th bit
const int HAS_HERO_PRIORITY = 12; // 13th bit  (be scared)

/*@jsdoc
 * <p>The pointing state of the hands is specified by the following values:</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Description</th>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>No hand is pointing.</td></tr>
 *     <tr><td><code>1</code></td><td>The left hand is pointing.</td></tr>
 *     <tr><td><code>2</code></td><td>The right hand is pointing.</td></tr>
 *     <tr><td><code>4</code></td><td>It is the index finger that is pointing.</td></tr>
 *   </tbody>
 * </table>
 * <p>The values for the hand states are added together to give the <code>HandState</code> value. For example, if the left
 * hand's finger is pointing, the value is <code>1 + 4 == 5</code>.
 * @typedef {number} HandState
 */
const char HAND_STATE_NULL = 0;
const char LEFT_HAND_POINTING_FLAG = 1;
const char RIGHT_HAND_POINTING_FLAG = 2;
const char IS_FINGER_POINTING_FLAG = 4;

// AvatarData state flags - we store the details about the packet encoding in the first byte,
// before the "header" structure
const char AVATARDATA_FLAGS_MINIMUM = 0;

using SmallFloat = uint16_t; // a compressed float with less precision, user defined radix

namespace AvatarSkeletonTrait {
    enum BoneType {
        SkeletonRoot = 0,
        SkeletonChild,
        NonSkeletonRoot,
        NonSkeletonChild
    };

    PACKED_BEGIN struct Header {
        float maxTranslationDimension;
        float maxScaleDimension;
        uint8_t numJoints;
        uint16_t stringTableLength;
    } PACKED_END;

    PACKED_BEGIN struct JointData {
        uint16_t stringStart;
        uint8_t stringLength;
        uint8_t boneType;
        uint8_t defaultTranslation[6];
        uint8_t defaultRotation[6];
        uint16_t defaultScale;
        uint16_t jointIndex;
        uint16_t parentIndex;
    } PACKED_END;

    struct UnpackedJointData {
        int stringStart;
        int stringLength;
        int boneType;
        glm::vec3 defaultTranslation;
        glm::quat defaultRotation;
        float defaultScale;
        int jointIndex;
        int parentIndex;
        QString jointName;
    };
}

namespace AvatarDataPacket {

    // NOTE: every time AvatarData is sent from mixer to client, it also includes the GUIID for the session
    // this is 16bytes of data at 45hz that's 5.76kbps
    // it might be nice to use a dictionary to compress that

    // Packet State Flags - we store the details about the existence of other records in this bitset:
    // AvatarGlobalPosition, Avatar face tracker, eye tracking, and existence of
    using HasFlags = uint16_t;
    const HasFlags PACKET_HAS_AVATAR_GLOBAL_POSITION   = 1U << 0;
    const HasFlags PACKET_HAS_AVATAR_BOUNDING_BOX      = 1U << 1;
    const HasFlags PACKET_HAS_AVATAR_ORIENTATION       = 1U << 2;
    const HasFlags PACKET_HAS_AVATAR_SCALE             = 1U << 3;
    const HasFlags PACKET_HAS_LOOK_AT_POSITION         = 1U << 4;
    const HasFlags PACKET_HAS_AUDIO_LOUDNESS           = 1U << 5;
    const HasFlags PACKET_HAS_SENSOR_TO_WORLD_MATRIX   = 1U << 6;
    const HasFlags PACKET_HAS_ADDITIONAL_FLAGS         = 1U << 7;
    const HasFlags PACKET_HAS_PARENT_INFO              = 1U << 8;
    const HasFlags PACKET_HAS_AVATAR_LOCAL_POSITION    = 1U << 9;
    const HasFlags PACKET_HAS_HAND_CONTROLLERS         = 1U << 10;
    const HasFlags PACKET_HAS_FACE_TRACKER_INFO        = 1U << 11;
    const HasFlags PACKET_HAS_JOINT_DATA               = 1U << 12;
    const HasFlags PACKET_HAS_JOINT_DEFAULT_POSE_FLAGS = 1U << 13;
    const HasFlags PACKET_HAS_GRAB_JOINTS              = 1U << 14;
    const size_t AVATAR_HAS_FLAGS_SIZE = 2;

    using SixByteQuat = uint8_t[6];
    using SixByteTrans = uint8_t[6];

    // NOTE: AvatarDataPackets start with a uint16_t sequence number that is not reflected in the Header structure.

    PACKED_BEGIN struct Header {
        HasFlags packetHasFlags;        // state flags, indicated which additional records are included in the packet
    } PACKED_END;
    const size_t HEADER_SIZE = 2;
    static_assert(sizeof(Header) == HEADER_SIZE, "AvatarDataPacket::Header size doesn't match.");

    PACKED_BEGIN struct AvatarGlobalPosition {
        float globalPosition[3];          // avatar's position
    } PACKED_END;
    const size_t AVATAR_GLOBAL_POSITION_SIZE = 12;
    static_assert(sizeof(AvatarGlobalPosition) == AVATAR_GLOBAL_POSITION_SIZE, "AvatarDataPacket::AvatarGlobalPosition size doesn't match.");

    PACKED_BEGIN struct AvatarBoundingBox {
        float avatarDimensions[3];        // avatar's bounding box in world space units, but relative to the position.
        float boundOriginOffset[3];       // offset from the position of the avatar to the origin of the bounding box
    } PACKED_END;
    const size_t AVATAR_BOUNDING_BOX_SIZE = 24;
    static_assert(sizeof(AvatarBoundingBox) == AVATAR_BOUNDING_BOX_SIZE, "AvatarDataPacket::AvatarBoundingBox size doesn't match.");

    PACKED_BEGIN struct AvatarOrientation {
        SixByteQuat avatarOrientation;      // encodeded and compressed by packOrientationQuatToSixBytes()
    } PACKED_END;
    const size_t AVATAR_ORIENTATION_SIZE = 6;
    static_assert(sizeof(AvatarOrientation) == AVATAR_ORIENTATION_SIZE, "AvatarDataPacket::AvatarOrientation size doesn't match.");

    PACKED_BEGIN struct AvatarScale {
        SmallFloat scale;                 // avatar's scale, compressed by packFloatRatioToTwoByte()
    } PACKED_END;
    const size_t AVATAR_SCALE_SIZE = 2;
    static_assert(sizeof(AvatarScale) == AVATAR_SCALE_SIZE, "AvatarDataPacket::AvatarScale size doesn't match.");

    PACKED_BEGIN struct LookAtPosition {
        float lookAtPosition[3];          // world space position that eyes are focusing on.
                                          // FIXME - unless the person has an eye tracker, this is simulated...
                                          //    a) maybe we can just have the client calculate this
                                          //    b) at distance this will be hard to discern and can likely be
                                          //       descimated or dropped completely
                                          //
                                          // POTENTIAL SAVINGS - 12 bytes
    } PACKED_END;
    const size_t LOOK_AT_POSITION_SIZE = 12;
    static_assert(sizeof(LookAtPosition) == LOOK_AT_POSITION_SIZE, "AvatarDataPacket::LookAtPosition size doesn't match.");

    PACKED_BEGIN struct AudioLoudness {
        uint8_t audioLoudness;            // current loudness of microphone compressed with packFloatGainToByte()
    } PACKED_END;
    const size_t AUDIO_LOUDNESS_SIZE = 1;
    static_assert(sizeof(AudioLoudness) == AUDIO_LOUDNESS_SIZE, "AvatarDataPacket::AudioLoudness size doesn't match.");

    PACKED_BEGIN struct SensorToWorldMatrix {
        // FIXME - these 20 bytes are only used by viewers if my avatar has "attachments"
        // we could save these bytes if no attachments are active.
        //
        // POTENTIAL SAVINGS - 20 bytes

        SixByteQuat sensorToWorldQuat;    // 6 byte compressed quaternion part of sensor to world matrix
        uint16_t sensorToWorldScale;      // uniform scale of sensor to world matrix
        float sensorToWorldTrans[3];      // fourth column of sensor to world matrix
                                          // FIXME - sensorToWorldTrans might be able to be better compressed if it was
                                          // relative to the avatar position.
    } PACKED_END;
    const size_t SENSOR_TO_WORLD_SIZE = 20;
    static_assert(sizeof(SensorToWorldMatrix) == SENSOR_TO_WORLD_SIZE, "AvatarDataPacket::SensorToWorldMatrix size doesn't match.");

    PACKED_BEGIN struct AdditionalFlags {
        uint16_t flags;                    // additional flags: hand state, key state, eye tracking
    } PACKED_END;
    const size_t ADDITIONAL_FLAGS_SIZE = 2;
    static_assert(sizeof(AdditionalFlags) == ADDITIONAL_FLAGS_SIZE, "AvatarDataPacket::AdditionalFlags size doesn't match.");

    // only present if HAS_REFERENTIAL flag is set in AvatarInfo.flags
    PACKED_BEGIN struct ParentInfo {
        uint8_t parentUUID[16];       // rfc 4122 encoded
        uint16_t parentJointIndex;
    } PACKED_END;
    const size_t PARENT_INFO_SIZE = 18;
    static_assert(sizeof(ParentInfo) == PARENT_INFO_SIZE, "AvatarDataPacket::ParentInfo size doesn't match.");

    // will only ever be included if the avatar has a parent but can change independent of changes to parent info
    // and so we keep it a separate record
    PACKED_BEGIN struct AvatarLocalPosition {
        float localPosition[3];           // parent frame translation of the avatar
    } PACKED_END;

    const size_t AVATAR_LOCAL_POSITION_SIZE = 12;
    static_assert(sizeof(AvatarLocalPosition) == AVATAR_LOCAL_POSITION_SIZE, "AvatarDataPacket::AvatarLocalPosition size doesn't match.");

    PACKED_BEGIN struct HandControllers {
        SixByteQuat leftHandRotation;
        SixByteTrans leftHandTranslation;
        SixByteQuat rightHandRotation;
        SixByteTrans rightHandTranslation;
    } PACKED_END;
    static const size_t HAND_CONTROLLERS_SIZE = 24;
    static_assert(sizeof(HandControllers) == HAND_CONTROLLERS_SIZE, "AvatarDataPacket::HandControllers size doesn't match.");

    const size_t MAX_CONSTANT_HEADER_SIZE = HEADER_SIZE +
        AVATAR_GLOBAL_POSITION_SIZE +
        AVATAR_BOUNDING_BOX_SIZE +
        AVATAR_ORIENTATION_SIZE +
        AVATAR_SCALE_SIZE +
        LOOK_AT_POSITION_SIZE +
        AUDIO_LOUDNESS_SIZE +
        SENSOR_TO_WORLD_SIZE +
        ADDITIONAL_FLAGS_SIZE +
        PARENT_INFO_SIZE +
        AVATAR_LOCAL_POSITION_SIZE +
        HAND_CONTROLLERS_SIZE;

    // variable length structure follows

    // only present if HAS_SCRIPTED_BLENDSHAPES flag is set in AvatarInfo.flags
    PACKED_BEGIN struct FaceTrackerInfo {
        float leftEyeBlink;
        float rightEyeBlink;
        float averageLoudness;
        float browAudioLift;
        uint8_t numBlendshapeCoefficients;
        // float blendshapeCoefficients[numBlendshapeCoefficients];
    } PACKED_END;
    const size_t FACE_TRACKER_INFO_SIZE = 17;
    static_assert(sizeof(FaceTrackerInfo) == FACE_TRACKER_INFO_SIZE, "AvatarDataPacket::FaceTrackerInfo size doesn't match.");
    size_t maxFaceTrackerInfoSize(size_t numBlendshapeCoefficients);

    /*
    struct JointData {
        uint8_t numJoints;
        uint8_t rotationValidityBits[ceil(numJoints / 8)];     // one bit per joint, if true then a compressed rotation follows.
        SixByteQuat rotation[numValidRotations];               // encodeded and compressed by packOrientationQuatToSixBytes()
        uint8_t translationValidityBits[ceil(numJoints / 8)];  // one bit per joint, if true then a compressed translation follows.
        float maxTranslationDimension;                         // used to normalize fixed point translation values.
        SixByteTrans translation[numValidTranslations];        // normalized and compressed by packFloatVec3ToSignedTwoByteFixed()
        SixByteQuat leftHandControllerRotation;
        SixByteTrans leftHandControllerTranslation;
        SixByteQuat rightHandControllerRotation;
        SixByteTrans rightHandControllerTranslation;
    };
    */
    size_t maxJointDataSize(size_t numJoints);
    size_t minJointDataSize(size_t numJoints);

    /*
    struct JointDefaultPoseFlags {
       uint8_t numJoints;
       uint8_t rotationIsDefaultPoseBits[ceil(numJoints / 8)];
       uint8_t translationIsDefaultPoseBits[ceil(numJoints / 8)];
    };
    */
    size_t maxJointDefaultPoseFlagsSize(size_t numJoints);

    PACKED_BEGIN struct FarGrabJoints {
        float leftFarGrabPosition[3]; // left controller far-grab joint position
        float leftFarGrabRotation[4]; // left controller far-grab joint rotation
        float rightFarGrabPosition[3]; // right controller far-grab joint position
        float rightFarGrabRotation[4]; // right controller far-grab joint rotation
        float mouseFarGrabPosition[3]; // mouse far-grab joint position
        float mouseFarGrabRotation[4]; // mouse far-grab joint rotation
    } PACKED_END;
    const size_t FAR_GRAB_JOINTS_SIZE = 84;
    static_assert(sizeof(FarGrabJoints) == FAR_GRAB_JOINTS_SIZE, "AvatarDataPacket::FarGrabJoints size doesn't match.");

    static const size_t MIN_BULK_PACKET_SIZE = NUM_BYTES_RFC4122_UUID + HEADER_SIZE;

    // AvatarIdentity packet:
    enum class IdentityFlag: quint32 {none, isReplicated = 0x1, lookAtSnapping = 0x2, verificationFailed = 0x4};
    Q_DECLARE_FLAGS(IdentityFlags, IdentityFlag)

    struct SendStatus {
        HasFlags itemFlags { 0 };
        bool sendUUID { false };
        int rotationsSent { 0 };  // ie: index of next unsent joint
        int translationsSent { 0 };
        operator bool() { return itemFlags == 0; }
    };
}

const float MAX_AUDIO_LOUDNESS = 1000.0f; // close enough for mouth animation

// See also static AvatarData::defaultFullAvatarModelUrl().
const QString DEFAULT_FULL_AVATAR_MODEL_NAME = QString("Default");

// how often should we send a full report about joint rotations, even if they haven't changed?
const float AVATAR_SEND_FULL_UPDATE_RATIO = 0.02f;
// this controls how large a change in joint-rotation must be before the interface sends it to the avatar mixer
const float AVATAR_MIN_ROTATION_DOT = 0.9999999f;
const float AVATAR_MIN_TRANSLATION = 0.0001f;

// quaternion dot products
const float ROTATION_CHANGE_2D = 0.99984770f; // 2 degrees
const float ROTATION_CHANGE_4D = 0.99939083f; // 4 degrees
const float ROTATION_CHANGE_6D = 0.99862953f; // 6 degrees
const float ROTATION_CHANGE_15D = 0.99144486f; // 15 degrees
const float ROTATION_CHANGE_179D = 0.00872653f; // 179 degrees

// rotation culling distance thresholds
const float AVATAR_DISTANCE_LEVEL_1 = 12.5f; // meters
const float AVATAR_DISTANCE_LEVEL_2 = 16.6f; // meters
const float AVATAR_DISTANCE_LEVEL_3 = 25.0f; // meters
const float AVATAR_DISTANCE_LEVEL_4 = 50.0f; // meters
const float AVATAR_DISTANCE_LEVEL_5 = 200.0f; // meters

// Where one's own Avatar begins in the world (will be overwritten if avatar data file is found).
// This is the start location in the Sandbox (xyz: 6270, 211, 6000).
const glm::vec3 START_LOCATION(6270, 211, 6000);

// Avatar Transit Constants
const float AVATAR_TRANSIT_MIN_TRIGGER_DISTANCE = 1.0f;
const float AVATAR_TRANSIT_MAX_TRIGGER_DISTANCE = 30.0f;
const int AVATAR_TRANSIT_FRAME_COUNT = 5;
const float AVATAR_TRANSIT_FRAMES_PER_METER = 0.5f;
const float AVATAR_TRANSIT_ABORT_DISTANCE = 0.1f;
const bool AVATAR_TRANSIT_DISTANCE_BASED = false;
const float AVATAR_TRANSIT_FRAMES_PER_SECOND = 30.0f;
const float AVATAR_PRE_TRANSIT_FRAME_COUNT = 10.0f;
const float AVATAR_POST_TRANSIT_FRAME_COUNT = 27.0f;

enum KeyState {
    NO_KEY_DOWN = 0,
    INSERT_KEY_DOWN,
    DELETE_KEY_DOWN
};

enum KillAvatarReason : uint8_t {
    NoReason = 0,
    AvatarDisconnected,
    AvatarIgnored,
    TheirAvatarEnteredYourBubble,
    YourAvatarEnteredTheirBubble
};

Q_DECLARE_METATYPE(KillAvatarReason);

class QDataStream;

class AttachmentData;
class Transform;
using TransformPointer = std::shared_ptr<Transform>;

class AvatarDataRate {
public:
    RateCounter<> globalPositionRate;
    RateCounter<> localPositionRate;
    RateCounter<> handControllersRate;
    RateCounter<> avatarBoundingBoxRate;
    RateCounter<> avatarOrientationRate;
    RateCounter<> avatarScaleRate;
    RateCounter<> lookAtPositionRate;
    RateCounter<> audioLoudnessRate;
    RateCounter<> sensorToWorldRate;
    RateCounter<> additionalFlagsRate;
    RateCounter<> parentInfoRate;
    RateCounter<> faceTrackerRate;
    RateCounter<> jointDataRate;
    RateCounter<> jointDefaultPoseFlagsRate;
    RateCounter<> farGrabJointRate;
};

class AvatarPriority {
public:
    AvatarPriority(AvatarSharedPointer a, float p) : avatar(a), priority(p) {}
    AvatarSharedPointer avatar;
    float priority;
    // NOTE: we invert the less-than operator to sort high priorities to front
    bool operator<(const AvatarPriority& other) const { return priority < other.priority; }
};

class ClientTraitsHandler;

class AvatarData : public QObject, public SpatiallyNestable {
    Q_OBJECT

    // IMPORTANT: The JSDoc for the following properties should be copied to MyAvatar.h and ScriptableAvatar.h.
    /*
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
     *     otherwise <code>false</code>. Set this to <code>true</code> before using the {@link Avatar.setBlendshape} method, 
     *     and set back to <code>false</code> after you no longer want scripted control over the blend shapes.
     *     <p><strong>Note:</strong> This property will automatically be set to <code>true</code> if the controller system has 
     *     valid facial blend shape actions.</p>
     * @property {boolean} hasProceduralBlinkFaceMovement=true - <code>true</code> if avatars blink automatically by animating 
     *     facial blend shapes, <code>false</code> if automatic blinking is disabled. Set to <code>false</code> to fully control 
     *     the blink facial blend shapes via the {@link Avatar.setBlendshape} method.
     * @property {boolean} hasProceduralEyeFaceMovement=true - <code>true</code> if the facial blend shapes for an avatar's eyes 
     *     adjust automatically as the eyes move, <code>false</code> if this automatic movement is disabled. Set this property 
     *     to <code>true</code> to prevent the iris from being obscured by the upper or lower lids. Set to <code>false</code> to
     *     fully control the eye blend shapes via the {@link Avatar.setBlendshape} method.
     * @property {boolean} hasAudioEnabledFaceMovement=true - <code>true</code> if the avatar's mouth blend shapes animate 
     *     automatically based on detected microphone input, <code>false</code> if this automatic movement is disabled. Set 
     *     this property to <code>false</code> to fully control the mouth facial blend shapes via the 
     *     {@link Avatar.setBlendshape} method.
     */
    Q_PROPERTY(glm::vec3 position READ getWorldPosition WRITE setPositionViaScript)
    Q_PROPERTY(float scale READ getDomainLimitedScale WRITE setTargetScale)
    Q_PROPERTY(float density READ getDensity)
    Q_PROPERTY(glm::vec3 handPosition READ getHandPosition WRITE setHandPosition)
    Q_PROPERTY(float bodyYaw READ getBodyYaw WRITE setBodyYaw)
    Q_PROPERTY(float bodyPitch READ getBodyPitch WRITE setBodyPitch)
    Q_PROPERTY(float bodyRoll READ getBodyRoll WRITE setBodyRoll)

    Q_PROPERTY(glm::quat orientation READ getWorldOrientation WRITE setOrientationViaScript)
    Q_PROPERTY(glm::quat headOrientation READ getHeadOrientation WRITE setHeadOrientation)
    Q_PROPERTY(float headPitch READ getHeadPitch WRITE setHeadPitch)
    Q_PROPERTY(float headYaw READ getHeadYaw WRITE setHeadYaw)
    Q_PROPERTY(float headRoll READ getHeadRoll WRITE setHeadRoll)

    Q_PROPERTY(glm::vec3 velocity READ getWorldVelocity WRITE setWorldVelocity)
    Q_PROPERTY(glm::vec3 angularVelocity READ getWorldAngularVelocity WRITE setWorldAngularVelocity)

    Q_PROPERTY(float audioLoudness READ getAudioLoudness WRITE setAudioLoudness)
    Q_PROPERTY(float audioAverageLoudness READ getAudioAverageLoudness WRITE setAudioAverageLoudness)

    Q_PROPERTY(QString displayName READ getDisplayName WRITE setDisplayName NOTIFY displayNameChanged)
    // sessionDisplayName is sanitized, defaulted version displayName that is defined by the AvatarMixer rather than by Interface clients.
    // The result is unique among all avatars present at the time.
    Q_PROPERTY(QString sessionDisplayName READ getSessionDisplayName WRITE setSessionDisplayName NOTIFY sessionDisplayNameChanged)
    Q_PROPERTY(bool lookAtSnappingEnabled MEMBER _lookAtSnappingEnabled NOTIFY lookAtSnappingChanged)
    Q_PROPERTY(QString skeletonModelURL READ getSkeletonModelURLFromScript WRITE setSkeletonModelURLFromScript NOTIFY skeletonModelURLChanged)
    Q_PROPERTY(QVector<AttachmentData> attachmentData READ getAttachmentData WRITE setAttachmentData)

    Q_PROPERTY(QStringList jointNames READ getJointNames)

    Q_PROPERTY(QUuid sessionUUID READ getSessionUUID NOTIFY sessionUUIDChanged)

    Q_PROPERTY(glm::mat4 sensorToWorldMatrix READ getSensorToWorldMatrix)
    Q_PROPERTY(glm::mat4 controllerLeftHandMatrix READ getControllerLeftHandMatrix)
    Q_PROPERTY(glm::mat4 controllerRightHandMatrix READ getControllerRightHandMatrix)

    Q_PROPERTY(float sensorToWorldScale READ getSensorToWorldScale)

    Q_PROPERTY(bool hasPriority READ getHasPriority)

    Q_PROPERTY(bool hasScriptedBlendshapes READ getHasScriptedBlendshapes WRITE setHasScriptedBlendshapes)
    Q_PROPERTY(bool hasProceduralBlinkFaceMovement READ getHasProceduralBlinkFaceMovement WRITE setHasProceduralBlinkFaceMovement)
    Q_PROPERTY(bool hasProceduralEyeFaceMovement READ getHasProceduralEyeFaceMovement WRITE setHasProceduralEyeFaceMovement)
    Q_PROPERTY(bool hasAudioEnabledFaceMovement READ getHasAudioEnabledFaceMovement WRITE setHasAudioEnabledFaceMovement)

public:
    virtual QString getName() const override { return QString("Avatar:") + _displayName; }

    static const QString FRAME_NAME;

    static void fromFrame(const QByteArray& frameData, AvatarData& avatar, bool useFrameSkeleton = true);
    static QByteArray toFrame(const AvatarData& avatar);

    AvatarData();
    virtual ~AvatarData();

    static const QUrl& defaultFullAvatarModelUrl();

    const QUuid getSessionUUID() const { return getID(); }

    glm::vec3 getHandPosition() const;
    void setHandPosition(const glm::vec3& handPosition);

    typedef enum {
        NoData,
        PALMinimum,
        MinimumData,
        CullSmallData,
        IncludeSmallData,
        SendAllData
    } AvatarDataDetail;

    virtual QByteArray toByteArrayStateful(AvatarDataDetail dataDetail, bool dropFaceTracking = false);

    virtual QByteArray toByteArray(AvatarDataDetail dataDetail, quint64 lastSentTime, const QVector<JointData>& lastSentJointData,
        AvatarDataPacket::SendStatus& sendStatus, bool dropFaceTracking, bool distanceAdjust, glm::vec3 viewerPosition,
        QVector<JointData>* sentJointDataOut, int maxDataSize = 0, AvatarDataRate* outboundDataRateOut = nullptr) const;

    virtual void doneEncoding(bool cullSmallChanges);

    /// \return true if an error should be logged
    bool shouldLogError(const quint64& now);

    /// \param packet byte array of data
    /// \param offset number of bytes into packet where data starts
    /// \return number of bytes parsed
    virtual int parseDataFromBuffer(const QByteArray& buffer);

    virtual void setCollisionWithOtherAvatarsFlags() {};

    // Body Rotation (degrees)
    float getBodyYaw() const;
    void setBodyYaw(float bodyYaw);
    float getBodyPitch() const;
    void setBodyPitch(float bodyPitch);
    float getBodyRoll() const;
    void setBodyRoll(float bodyRoll);

    virtual void setPositionViaScript(const glm::vec3& position);
    virtual void setOrientationViaScript(const glm::quat& orientation);

    virtual void updateAttitude(const glm::quat& orientation) {}

    glm::quat getHeadOrientation() const {
        lazyInitHeadData();
        return _headData->getOrientation();
    }
    void setHeadOrientation(const glm::quat& orientation) {
        if (_headData) {
            _headData->setOrientation(orientation);
        }
    }

    void setLookAtPosition(const glm::vec3& lookAtPosition) {
        if (_headData) {
            _headData->setLookAtPosition(lookAtPosition);
        }
    }

    void setBlendshapeCoefficients(const QVector<float>& blendshapeCoefficients) {
        if (_headData) {
            _headData->setBlendshapeCoefficients(blendshapeCoefficients);
        }
    }

    // access to Head().set/getMousePitch (degrees)
    float getHeadPitch() const { return _headData->getBasePitch(); }
    void setHeadPitch(float value) { _headData->setBasePitch(value); }

    float getHeadYaw() const { return _headData->getBaseYaw(); }
    void setHeadYaw(float value) { _headData->setBaseYaw(value); }

    float getHeadRoll() const { return _headData->getBaseRoll(); }
    void setHeadRoll(float value) { _headData->setBaseRoll(value); }

    // access to Head().set/getAverageLoudness

    float getAudioLoudness() const { return _audioLoudness; }
    void setAudioLoudness(float audioLoudness) {
        if (audioLoudness != _audioLoudness) {
            _audioLoudnessChanged = usecTimestampNow();
        }
        _audioLoudness = audioLoudness;
    }
    bool audioLoudnessChangedSince(quint64 time) const { return _audioLoudnessChanged >= time; }

    float getAudioAverageLoudness() const { return _audioAverageLoudness; }
    void setAudioAverageLoudness(float audioAverageLoudness) { _audioAverageLoudness = audioAverageLoudness; }

    //  Scale
    virtual void setTargetScale(float targetScale);

    float getDomainLimitedScale() const;

    void setHasScriptedBlendshapes(bool hasScriptedBlendshapes);
    bool getHasScriptedBlendshapes() const;
    void setHasProceduralBlinkFaceMovement(bool hasProceduralBlinkFaceMovement);
    bool getHasProceduralBlinkFaceMovement() const;
    void setHasProceduralEyeFaceMovement(bool hasProceduralEyeFaceMovement);
    bool getHasProceduralEyeFaceMovement() const;
    void setHasAudioEnabledFaceMovement(bool hasAudioEnabledFaceMovement);
    bool getHasAudioEnabledFaceMovement() const;

    /*@jsdoc
     * Gets the minimum scale allowed for this avatar in the current domain.
     * This value can change as the user changes avatars or when changing domains.
     * @function Avatar.getDomainMinScale
     * @returns {number} The minimum scale allowed for this avatar in the current domain.
     */
    Q_INVOKABLE float getDomainMinScale() const;

    /*@jsdoc
     * Gets the maximum scale allowed for this avatar in the current domain.
     * This value can change as the user changes avatars or when changing domains.
     * @function Avatar.getDomainMaxScale
     * @returns {number} The maximum scale allowed for this avatar in the current domain.
     */
    Q_INVOKABLE float getDomainMaxScale() const;

    // Returns eye height of avatar in meters, ignoring avatar scale.
    // if _targetScale is 1 then this will be identical to getEyeHeight;
    virtual float getUnscaledEyeHeight() const { return DEFAULT_AVATAR_EYE_HEIGHT; }

    // returns true, if an acurate eye height estimage can be obtained by inspecting the avatar model skeleton and geometry,
    // not all subclasses of AvatarData have access to this data.
    virtual bool canMeasureEyeHeight() const { return false; }

    /*@jsdoc
     * Gets the current eye height of the avatar.
     * This height is only an estimate and might be incorrect for avatars that are missing standard joints.
     * @function Avatar.getEyeHeight
     * @returns {number} The eye height of the avatar.
     */
    Q_INVOKABLE virtual float getEyeHeight() const { return _targetScale * getUnscaledEyeHeight(); }

    /*@jsdoc
     * Gets the current height of the avatar.
     * This height is only an estimate and might be incorrect for avatars that are missing standard joints.
     * @function Avatar.getHeight
     * @returns {number} The height of the avatar.
     */
    Q_INVOKABLE virtual float getHeight() const;

    float getUnscaledHeight() const;

    void setDomainMinimumHeight(float domainMinimumHeight);
    void setDomainMaximumHeight(float domainMaximumHeight);

    /*@jsdoc
     * Sets the pointing state of the hands to control where the laser emanates from. If the right index finger is pointing, the 
     * laser emanates from the tip of that finger, otherwise it emanates from the palm.
     * @function Avatar.setHandState
     * @param {HandState} state - The pointing state of the hand.
     */
    Q_INVOKABLE void setHandState(char s) { _handState = s; }

    /*@jsdoc
     * Gets the pointing state of the hands to control where the laser emanates from. If the right index finger is pointing, the
     * laser emanates from the tip of that finger, otherwise it emanates from the palm.
     * @function Avatar.getHandState
     * @returns {HandState} The pointing state of the hand.
     */
    Q_INVOKABLE char getHandState() const { return _handState; }

    const QVector<JointData>& getRawJointData() const { return _jointData; }

    /*@jsdoc
     * Sets joint translations and rotations from raw joint data.
     * @function Avatar.setRawJointData
     * @param {JointData[]} data - The raw joint data.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void setRawJointData(QVector<JointData> data);

    /*@jsdoc
     * Sets a specific joint's rotation and position relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointData
     * @param {number} index - The index of the joint.
     * @param {Quat} rotation - The rotation of the joint relative to its parent.
     * @param {Vec3} translation - The translation of the joint relative to its parent, in model coordinates.
     * @example <caption>Set your avatar to it's default T-pose for a while.<br />
     * <img alt="Avatar in T-pose" src="https://apidocs.vircadia.dev/images/t-pose.png" /></caption>
     * // Set all joint translations and rotations to defaults.
     * var i, length, rotation, translation;
     * for (i = 0, length = MyAvatar.getJointNames().length; i < length; i++) {
     *     rotation = MyAvatar.getDefaultJointRotation(i);
     *     translation = MyAvatar.getDefaultJointTranslation(i);
     *     MyAvatar.setJointData(i, rotation, translation);
     * }
     *
     * // Restore your avatar's motion after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointsData();
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void setJointData(int index, const glm::quat& rotation, const glm::vec3& translation);

    /*@jsdoc
     * Sets a specific joint's rotation relative to its parent.
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse 
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointRotation
     * @param {number} index - The index of the joint.
     * @param {Quat} rotation - The rotation of the joint relative to its parent.
     */
    Q_INVOKABLE virtual void setJointRotation(int index, const glm::quat& rotation);

    /*@jsdoc
     * Sets a specific joint's translation relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointTranslation
     * @param {number} index - The index of the joint.
     * @param {Vec3} translation - The translation of the joint relative to its parent, in model coordinates.
     */
    Q_INVOKABLE virtual void setJointTranslation(int index, const glm::vec3& translation);

    /*@jsdoc
     * Clears joint translations and rotations set by script for a specific joint. This restores all motion from the default 
     * animation system including inverse kinematics for that joint.
     * <p>Note: This is slightly faster than the function variation that specifies the joint name.</p>
     * @function Avatar.clearJointData
     * @param {number} index - The index of the joint.
     */
    Q_INVOKABLE virtual void clearJointData(int index);

    /*@jsdoc
     * Checks that the data for a joint are valid.
     * @function Avatar.isJointDataValid
     * @param {number} index - The index of the joint.
     * @returns {boolean} <code>true</code> if the joint data are valid, <code>false</code> if not.
     */
    Q_INVOKABLE bool isJointDataValid(int index) const;

    /*@jsdoc
     * Gets the rotation of a joint relative to its parent. For information on the joint hierarchy used, see 
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.
     * @function Avatar.getJointRotation
     * @param {number} index - The index of the joint.
     * @returns {Quat} The rotation of the joint relative to its parent.
     */
    Q_INVOKABLE virtual glm::quat getJointRotation(int index) const;

    /*@jsdoc
     * Gets the translation of a joint relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>For information on the joint hierarchy used, see 
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.</p>
     * @function Avatar.getJointTranslation
     * @param {number} index - The index of the joint.
     * @returns {Vec3} The translation of the joint relative to its parent, in model coordinates.
     */
    Q_INVOKABLE virtual glm::vec3 getJointTranslation(int index) const;

    /*@jsdoc
     * Sets a specific joint's rotation and position relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointData
     * @param {string} name - The name of the joint.
     * @param {Quat} rotation - The rotation of the joint relative to its parent.
     * @param {Vec3} translation - The translation of the joint relative to its parent, in model coordinates.
     */
    Q_INVOKABLE virtual void setJointData(const QString& name, const glm::quat& rotation, const glm::vec3& translation);

    /*@jsdoc
     * Sets a specific joint's rotation relative to its parent.
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse 
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointRotation
     * @param {string} name - The name of the joint.
     * @param {Quat} rotation - The rotation of the joint relative to its parent.
     * @example <caption>Set your avatar to its default T-pose then rotate its right arm.<br />
     * <img alt="Avatar in T-pose with arm rotated" src="https://apidocs.vircadia.dev/images/armpose.png" /></caption>
     * // Set all joint translations and rotations to defaults.
     * var i, length, rotation, translation;
     * for (i = 0, length = MyAvatar.getJointNames().length; i < length; i++) {
     *     rotation = MyAvatar.getDefaultJointRotation(i);
     *     translation = MyAvatar.getDefaultJointTranslation(i);
     *     MyAvatar.setJointData(i, rotation, translation);
     * }
     *
     * // Rotate the right arm.
     * var newArmRotation = { x: 0.47, y: 0.22, z: -0.02, w: 0.87 };
     * MyAvatar.setJointRotation("RightArm", newArmRotation);
     *
     * // Restore your avatar's motion after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointsData();
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void setJointRotation(const QString& name, const glm::quat& rotation);

    /*@jsdoc
     * Sets a specific joint's translation relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints, 
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate 
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set 
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointTranslation
     * @param {string} name - The name of the joint.
     * @param {Vec3} translation - The translation of the joint relative to its parent, in model coordinates.
     * @example <caption>Stretch your avatar's neck. Depending on the avatar you are using, you will either see a gap between 
     * the head and body or you will see the neck stretched.<br />
     * <img alt="Avatar with neck stretched" src="https://apidocs.vircadia.dev/images/stretched-neck.png" /></caption>
     * // Stretch your avatar's neck.
     * MyAvatar.setJointTranslation("Neck", Vec3.multiply(2, MyAvatar.getJointTranslation("Neck")));
     * 
     * // Restore your avatar's neck after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointData("Neck");
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void setJointTranslation(const QString& name, const glm::vec3& translation);

    /*@jsdoc
     * Clears joint translations and rotations set by script for a specific joint. This restores all motion from the default 
     * animation system including inverse kinematics for that joint.
     * <p>Note: This is slightly slower than the function variation that specifies the joint index.</p>
     * @function Avatar.clearJointData
     * @param {string} name - The name of the joint.
     * @example <caption>Offset and restore the position of your avatar's head.</caption>
     * // Stretch your avatar's neck.
     * MyAvatar.setJointTranslation("Neck", Vec3.multiply(2, MyAvatar.getJointTranslation("Neck")));
     *
     * // Restore your avatar's neck after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointData("Neck");
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void clearJointData(const QString& name);

    /*@jsdoc
     * Checks if the data for a joint are valid.
     * @function Avatar.isJointDataValid
     * @param {string} name - The name of the joint.
     * @returns {boolean} <code>true</code> if the joint data are valid, <code>false</code> if not.
     */
    Q_INVOKABLE virtual bool isJointDataValid(const QString& name) const;

    /*@jsdoc
     * Gets the rotation of a joint relative to its parent. For information on the joint hierarchy used, see 
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.
     * @function Avatar.getJointRotation
     * @param {string} name - The name of the joint.
     * @returns {Quat} The rotation of the joint relative to its parent.
     * @example <caption>Report the rotation of your avatar's hips joint.</caption>
     * print(JSON.stringify(MyAvatar.getJointRotation("Hips")));
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual glm::quat getJointRotation(const QString& name) const;

    /*@jsdoc
     * Gets the translation of a joint relative to its parent, in model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>For information on the joint hierarchy used, see
     * <a href="https://docs.vircadia.dev/create/avatars/avatar-standards.html">Avatar Standards</a>.</p>
     * @function Avatar.getJointTranslation
     * @param {number} name - The name of the joint.
     * @returns {Vec3} The translation of the joint relative to its parent, in model coordinates.
     * @example <caption>Report the translation of your avatar's hips joint.</caption>
     * print(JSON.stringify(MyAvatar.getJointRotation("Hips")));
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual glm::vec3 getJointTranslation(const QString& name) const;

    /*@jsdoc
     * Gets the rotations of all joints in the current avatar. Each joint's rotation is relative to its parent joint.
     * @function Avatar.getJointRotations
     * @returns {Quat[]} The rotations of all joints relative to each's parent. The values are in the same order as the array 
     * returned by {@link MyAvatar.getJointNames}, or {@link Avatar.getJointNames} if using the <code>Avatar</code> API.
     * @example <caption>Report the rotations of all your avatar's joints.</caption>
     * print(JSON.stringify(MyAvatar.getJointRotations()));
     *
     * // Note: If using from the Avatar API, replace all "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual QVector<glm::quat> getJointRotations() const;

    /*@jsdoc
     * Gets the translations of all joints in the current avatar. Each joint's translation is relative to its parent joint, in
     * model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * @function Avatar.getJointTranslations
     * @returns {Vec3[]} The translations of all joints relative to each's parent, in model coordinates. The values are in the 
     *     same order as the array returned by {@link MyAvatar.getJointNames}, or {@link Avatar.getJointNames} if using the 
     *     <code>Avatar</code> API.
     */
    Q_INVOKABLE virtual QVector<glm::vec3> getJointTranslations() const;

    /*@jsdoc
     * Sets the rotations of all joints in the current avatar. Each joint's rotation is relative to its parent joint.
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints,
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointRotations
     * @param {Quat[]} jointRotations - The rotations for all joints in the avatar. The values are in the same order as the 
     * array returned by {@link MyAvatar.getJointNames}, or {@link Avatar.getJointNames} if using the <code>Avatar</code> API.
     * @example <caption>Set your avatar to its default T-pose then rotate its right arm.<br />
     * <img alt="Avatar in T-pose" src="https://apidocs.vircadia.dev/images/armpose.png" /></caption>
     * // Set all joint translations and rotations to defaults.
     * var i, length, rotation, translation;
     * for (i = 0, length = MyAvatar.getJointNames().length; i < length; i++) {
     *     rotation = MyAvatar.getDefaultJointRotation(i);
     *     translation = MyAvatar.getDefaultJointTranslation(i);
     *     MyAvatar.setJointData(i, rotation, translation);
     * }
     *
     * // Get all join rotations.
     * var jointRotations = MyAvatar.getJointRotations(); 
     *
     * // Update the rotation of the right arm in the array.
     * jointRotations[MyAvatar.getJointIndex("RightArm")] = { x: 0.47, y: 0.22, z: -0.02, w: 0.87 };
     *
     * // Update all joint rotations.
     * MyAvatar.setJointRotations(jointRotations);
     *
     * // Restore your avatar's motion after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointsData();
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void setJointRotations(const QVector<glm::quat>& jointRotations);
    
    /*@jsdoc
     * Sets the translations of all joints in the current avatar. Each joint's translation is relative to its parent joint, in 
     * model coordinates.
     * <p><strong>Warning:</strong> These coordinates are not necessarily in meters.</p>
     * <p>Setting joint data completely overrides/replaces all motion from the default animation system including inverse
     * kinematics, but just for the specified joint. So for example, if you were to procedurally manipulate the finger joints,
     * the avatar's hand and head would still do inverse kinematics properly. However, as soon as you start to manipulate
     * joints in the inverse kinematics chain, the inverse kinematics might not function as you expect. For example, if you set
     * the rotation of the elbow, the hand inverse kinematics position won't end up in the right place.</p>
     * @function Avatar.setJointTranslations
     * @param {Vec3[]} translations - The translations for all joints in the avatar, in model coordinates. The values are in 
     *     the same order as the array returned by {@link MyAvatar.getJointNames}, or {@link Avatar.getJointNames} if using the 
     *     <code>Avatar</code> API.
     */
    Q_INVOKABLE virtual void setJointTranslations(const QVector<glm::vec3>& jointTranslations);

    /*@jsdoc
     * Clears all joint translations and rotations that have been set by script. This restores all motion from the default 
     * animation system including inverse kinematics for all joints.
     * @function Avatar.clearJointsData
     * @example <caption>Set your avatar to it's default T-pose for a while.</caption>
     * // Set all joint translations and rotations to defaults.
     * var i, length, rotation, translation;
     * for (i = 0, length = MyAvatar.getJointNames().length; i < length; i++) {
     *     rotation = MyAvatar.getDefaultJointRotation(i);
     *     translation = MyAvatar.getDefaultJointTranslation(i);
     *     MyAvatar.setJointData(i, rotation, translation);
     * }
     *
     * // Restore your avatar's motion after 5s.
     * Script.setTimeout(function () {
     *     MyAvatar.clearJointsData();
     * }, 5000);
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void clearJointsData();

    /*@jsdoc
     * Gets the joint index for a named joint. The joint index value is the position of the joint in the array returned by 
     * {@link MyAvatar.getJointNames}, or {@link Avatar.getJointNames} if using the <code>Avatar</code> API.
     * @function Avatar.getJointIndex
     * @param {string} name - The name of the joint.
     * @returns {number} The index of the joint if valid, otherwise <code>-1</code>.
     * @example <caption>Report the index of your avatar's left arm joint.</caption>
     * print(JSON.stringify(MyAvatar.getJointIndex("LeftArm")));
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    /// Returns the index of the joint with the specified name, or -1 if not found/unknown.
    Q_INVOKABLE virtual int getJointIndex(const QString& name) const;

    /*@jsdoc
     * Gets the names of all the joints in the current avatar.
     * @function Avatar.getJointNames
     * @returns {string[]} The joint names.
     * @example <caption>Report the names of all the joints in your current avatar.</caption>
     * print(JSON.stringify(MyAvatar.getJointNames()));
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual QStringList getJointNames() const;


    /*@jsdoc
     * Sets the value of a blend shape to animate your avatar's face. In order for other users to see the resulting animations 
     * on your avatar's face, set <code>hasScriptedBlendshapes</code> to <code>true</code>. When you are done using this API, 
     * set <code>hasScriptedBlendshapes</code> back to <code>false</code> when the animation is complete. 
     * @function Avatar.setBlendshape
     * @param {string} name - The name of the blendshape, per the 
     *     {@link https://docs.vircadia.dev/create/avatars/avatar-standards.html#blendshapes Avatar Standards}.
     * @param {number} value - A value between <code>0.0</code> and <code>1.0</code>.
     * @example <caption>Open your avatar's mouth wide.</caption>
     * MyAvatar.hasScriptedBlendshapes = true;
     * MyAvatar.setBlendshape("JawOpen", 1.0);
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE void setBlendshape(QString name, float val) { _headData->setBlendshape(name, val); }


    /*@jsdoc
     * Gets information about the models currently attached to your avatar.
     * @function Avatar.getAttachmentsVariant
     * @returns {AttachmentData[]} Information about all models attached to your avatar.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     */
    // FIXME: Can this name be improved? Can it be deprecated?
    Q_INVOKABLE virtual QVariantList getAttachmentsVariant() const;

    /*@jsdoc
     * Sets all models currently attached to your avatar. For example, if you retrieve attachment data using
     * {@link MyAvatar.getAttachmentsVariant} or {@link Avatar.getAttachmentsVariant}, make changes to it, and then want to 
     * update your avatar's attachments per the changed data.
     * @function Avatar.setAttachmentsVariant
     * @param {AttachmentData[]} variant - The attachment data defining the models to have attached to your avatar.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     */
    // FIXME: Can this name be improved? Can it be deprecated?
    Q_INVOKABLE virtual void setAttachmentsVariant(const QVariantList& variant);

    virtual void storeAvatarEntityDataPayload(const QUuid& entityID, const QByteArray& payload);

    /*@jsdoc
     * @function Avatar.updateAvatarEntity
     * @param {Uuid} entityID - The entity ID.
     * @param {ArrayBuffer} entityData - Entity data.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE virtual void updateAvatarEntity(const QUuid& entityID, const QByteArray& entityData);

    /*@jsdoc
     * @function Avatar.clearAvatarEntity
     * @param {Uuid} entityID - The entity ID.
     * @param {boolean} [requiresRemovalFromTree=true] - unused
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE virtual void clearAvatarEntity(const QUuid& entityID, bool requiresRemovalFromTree = true);
    
    // FIXME: Rename to clearAvatarEntity() once the API call is removed.
    virtual void clearAvatarEntityInternal(const QUuid& entityID);

    void clearAvatarEntities();

    QList<QUuid> getAvatarEntityIDs() const;

    /*@jsdoc
     * Enables blend shapes set using {@link Avatar.setBlendshape} or {@link MyAvatar.setBlendshape} to be transmitted to other 
     * users so that they can see the animation of your avatar's face.
     * <p class="important">Deprecated: This method is deprecated and will be removed. Use the 
     * <code>Avatar.hasScriptedBlendshapes</code> or <code>MyAvatar.hasScriptedBlendshapes</code>  property instead.</p>
     * @function Avatar.setForceFaceTrackerConnected
     * @param {boolean} connected - <code>true</code> to enable blend shape changes to be transmitted to other users, 
     *     <code>false</code> to disable.
     */
    Q_INVOKABLE void setForceFaceTrackerConnected(bool connected) { setHasScriptedBlendshapes(connected); }

    // key state
    void setKeyState(KeyState s) { _keyState = s; }
    KeyState keyState() const { return _keyState; }

    const HeadData* getHeadData() const { return _headData; }

    struct Identity {
        QVector<AttachmentData> attachmentData;
        QString displayName;
        QString sessionDisplayName;
        bool isReplicated;
        bool lookAtSnappingEnabled;
        AvatarDataPacket::IdentityFlags identityFlags;
    };

    // identityChanged returns true if identity has changed, false otherwise.
    // identityChanged returns true if identity has changed, false otherwise. Similarly for displayNameChanged and skeletonModelUrlChange.
    void processAvatarIdentity(QDataStream& packetStream, bool& identityChanged, bool& displayNameChanged);

    QByteArray packTrait(AvatarTraits::TraitType traitType) const;
    QByteArray packTraitInstance(AvatarTraits::TraitType traitType, AvatarTraits::TraitInstanceID instanceID);

    void processTrait(AvatarTraits::TraitType traitType, QByteArray traitBinaryData);
    void processTraitInstance(AvatarTraits::TraitType traitType,
                              AvatarTraits::TraitInstanceID instanceID, QByteArray traitBinaryData);
    void processDeletedTraitInstance(AvatarTraits::TraitType traitType, AvatarTraits::TraitInstanceID instanceID);

    void prepareResetTraitInstances();

    QByteArray identityByteArray(bool setIsReplicated = false) const;

    QUrl getWireSafeSkeletonModelURL() const;
    virtual const QUrl& getSkeletonModelURL() const;

    const QString& getDisplayName() const { return _displayName; }
    const QString& getSessionDisplayName() const { return _sessionDisplayName; }
    bool getLookAtSnappingEnabled() const { return _lookAtSnappingEnabled; }

    /*@jsdoc
     * Sets the avatar's skeleton model.
     * @function Avatar.setSkeletonModelURL
     * @param {string} url - The avatar's FST file.
     */
    Q_INVOKABLE virtual void setSkeletonModelURL(const QUrl& skeletonModelURL);

    virtual void setDisplayName(const QString& displayName);
    virtual void setSessionDisplayName(const QString& sessionDisplayName) {
        _sessionDisplayName = sessionDisplayName;
        markIdentityDataChanged();
    }
    virtual bool isCertifyFailed() const { return _verificationFailed; }

    /*@jsdoc
     * Gets information about the models currently attached to your avatar.
     * @function Avatar.getAttachmentData
     * @returns {AttachmentData[]} Information about all models attached to your avatar.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     * @example <caption>Report the URLs of all current attachments.</caption>
     * var attachments = MyAvatar.getaAttachmentData();
     * for (var i = 0; i < attachments.length; i++) {
     *     print(attachments[i].modelURL);
     * }
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual QVector<AttachmentData> getAttachmentData() const;

    /*@jsdoc
     * Sets all models currently attached to your avatar. For example, if you retrieve attachment data using
     * {@link MyAvatar.getAttachmentData} or {@link Avatar.getAttachmentData}, make changes to it, and then want to update your avatar's attachments per the 
     * changed data. You can also remove all attachments by using setting <code>attachmentData</code> to <code>null</code>.
     * @function Avatar.setAttachmentData
     * @param {AttachmentData[]} attachmentData - The attachment data defining the models to have attached to your avatar. Use
     *     <code>null</code> to remove all attachments.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     * @example <caption>Remove a hat attachment if your avatar is wearing it.</caption>
     * var hatURL = "https://apidocs.vircadia.dev/models/cowboy-hat.fbx";
     * var attachments = MyAvatar.getAttachmentData();
     *
     * for (var i = 0; i < attachments.length; i++) {
     *     if (attachments[i].modelURL === hatURL) {
     *         attachments.splice(i, 1);
     *         MyAvatar.setAttachmentData(attachments);
     *         break;
     *     }
     *  }
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void setAttachmentData(const QVector<AttachmentData>& attachmentData);

    /*@jsdoc
     * Attaches a model to your avatar. For example, you can give your avatar a hat to wear, a guitar to hold, or a surfboard to
     * stand on.
     * @function Avatar.attach
     * @param {string} modelURL - The URL of the glTF, FBX, or OBJ model to attach. glTF models may be in JSON or binary format 
     *     (".gltf" or ".glb" URLs respectively).
     * @param {string} [jointName=""] - The name of the avatar joint (see {@link MyAvatar.getJointNames} or 
     *     {@link Avatar.getJointNames}) to attach the model to.
     * @param {Vec3} [translation=Vec3.ZERO] - The offset to apply to the model relative to the joint position.
     * @param {Quat} [rotation=Quat.IDENTITY] - The rotation to apply to the model relative to the joint orientation.
     * @param {number} [scale=1.0] - The scale to apply to the model.
     * @param {boolean} [isSoft=false] -  If the model has a skeleton, set this to <code>true</code> so that the bones of the 
     *     attached model's skeleton are rotated to fit the avatar's current pose. <code>isSoft</code> is used, for example, 
     *     to have clothing that moves with the avatar.
     *     <p>If <code>true</code>, the <code>translation</code>, <code>rotation</code>, and <code>scale</code> parameters are 
     *     ignored.</p>
     * @param {boolean} [allowDuplicates=false] - If <code>true</code> then more than one copy of any particular model may be 
     *     attached to the same joint; if <code>false</code> then the same model cannot be attached to the same joint.
     * @param {boolean} [useSaved=true] - <em>Not used.</em>
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     * @example <caption>Attach a cowboy hat to your avatar's head.</caption>
     * var attachment = {
     *     modelURL: "https://apidocs.vircadia.dev/models/cowboy-hat.fbx",
     *     jointName: "Head",
     *     translation: {"x": 0, "y": 0.25, "z": 0},
     *     rotation: {"x": 0, "y": 0, "z": 0, "w": 1},
     *     scale: 0.01,
     *     isSoft: false
     * };
     *
     *  MyAvatar.attach(attachment.modelURL,
     *                  attachment.jointName,
     *                  attachment.translation,
     *                  attachment.rotation,
     *                  attachment.scale,
     *                  attachment.isSoft);
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    Q_INVOKABLE virtual void attach(const QString& modelURL, const QString& jointName = QString(),
                                    const glm::vec3& translation = glm::vec3(), const glm::quat& rotation = glm::quat(),
                                    float scale = 1.0f, bool isSoft = false,
                                    bool allowDuplicates = false, bool useSaved = true);

    /*@jsdoc
     * Detaches the most recently attached instance of a particular model from either a specific joint or any joint.
     * @function Avatar.detachOne
     * @param {string} modelURL - The URL of the model to detach.
     * @param {string} [jointName=""] - The name of the joint to detach the model from. If <code>""</code>, then the most 
     *     recently attached model is removed from which ever joint it was attached to.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     */
    Q_INVOKABLE virtual void detachOne(const QString& modelURL, const QString& jointName = QString());

    /*@jsdoc
     * Detaches all instances of a particular model from either a specific joint or all joints.
     * @function Avatar.detachAll
     * @param {string} modelURL - The URL of the model to detach.
     * @param {string} [jointName=""] - The name of the joint to detach the model from. If <code>""</code>, then the model is 
     *     detached from all joints.
     * @deprecated This function is deprecated and will be removed. Use avatar entities instead.
     */
    Q_INVOKABLE virtual void detachAll(const QString& modelURL, const QString& jointName = QString());

    QString getSkeletonModelURLFromScript() const { return _skeletonModelURL.toString(); }
    void setSkeletonModelURLFromScript(const QString& skeletonModelString) { setSkeletonModelURL(QUrl(skeletonModelString)); }

    void setOwningAvatarMixer(const QWeakPointer<Node>& owningAvatarMixer) { _owningAvatarMixer = owningAvatarMixer; }

    int getAverageBytesReceivedPerSecond() const;
    int getReceiveRate() const;

    // An Avatar can be set Priority from the AvatarMixer side.
    bool getHasPriority() const { return _hasPriority; }
    // regular setHasPriority does a check of state changed and if true reset 'additionalFlagsChanged' timestamp
    void setHasPriority(bool hasPriority) {
        if (_hasPriority != hasPriority) {
            _additionalFlagsChanged = usecTimestampNow();
            _hasPriority = hasPriority;
        }
    }
    // In some cases, we want to assign the hasPRiority flag without reseting timestamp
    void setHasPriorityWithoutTimestampReset(bool hasPriority) { _hasPriority = hasPriority; }

    const glm::vec3& getTargetVelocity() const { return _targetVelocity; }

    void clearRecordingBasis();
    TransformPointer getRecordingBasis() const;
    void setRecordingBasis(TransformPointer recordingBasis = TransformPointer());
    void createRecordingIDs();
    virtual void avatarEntityDataToJson(QJsonObject& root) const;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json, bool useFrameSkeleton = true);

    glm::vec3 getClientGlobalPosition() const { return _globalPosition; }
    AABox getGlobalBoundingBox() const { return AABox(_globalPosition + _globalBoundingBoxOffset - _globalBoundingBoxDimensions, _globalBoundingBoxDimensions); }
    AABox getDefaultBubbleBox() const;

    /*@jsdoc
     * @comment Documented in derived classes' JSDoc because implementations are different.
     */
     // Get avatar entity data with all property values. Used in API.
     Q_INVOKABLE virtual AvatarEntityMap getAvatarEntityData() const;

    // Get avatar entity data with non-default property values. Used internally.
    virtual AvatarEntityMap getAvatarEntityDataNonDefault() const;

    /*@jsdoc
     * @comment Documented in derived classes' JSDoc because implementations are different.
     */
    Q_INVOKABLE virtual void setAvatarEntityData(const AvatarEntityMap& avatarEntityData);

    void setAvatarEntityDataChanged(bool value) { _avatarEntityDataChanged = value; }
    AvatarEntityIDs getAndClearRecentlyRemovedIDs();

    /*@jsdoc
     * Gets the transform from the user's real world to the avatar's size, orientation, and position in the virtual world.
     * @function Avatar.getSensorToWorldMatrix
     * @returns {Mat4} The scale, rotation, and translation transform from the user's real world to the avatar's size, 
     *     orientation, and position in the virtual world.
     * @example <caption>Report the sensor to world matrix.</caption>
     * var sensorToWorldMatrix = MyAvatar.getSensorToWorldMatrix();
     * print("Sensor to woprld matrix: " + JSON.stringify(sensorToWorldMatrix));
     * print("Rotation: " + JSON.stringify(Mat4.extractRotation(sensorToWorldMatrix)));
     * print("Translation: " + JSON.stringify(Mat4.extractTranslation(sensorToWorldMatrix)));
     * print("Scale: " + JSON.stringify(Mat4.extractScale(sensorToWorldMatrix)));
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    // thread safe
    Q_INVOKABLE glm::mat4 getSensorToWorldMatrix() const;

    /*@jsdoc
     * Gets the scale that transforms dimensions in the user's real world to the avatar's size in the virtual world.
     * @function Avatar.getSensorToWorldScale
     * @returns {number} The scale that transforms dimensions in the user's real world to the avatar's size in the virtual 
     *     world.
     */
    // thread safe
    Q_INVOKABLE float getSensorToWorldScale() const;

    /*@jsdoc
     * Gets the rotation and translation of the left hand controller relative to the avatar.
     * @function Avatar.getControllerLeftHandMatrix
     * @returns {Mat4} The rotation and translation of the left hand controller relative to the avatar.
     * @example <caption>Report the left hand controller matrix.</caption>
     * var leftHandMatrix = MyAvatar.getControllerLeftHandMatrix();
     * print("Controller left hand matrix: " + JSON.stringify(leftHandMatrix));
     * print("Rotation: " + JSON.stringify(Mat4.extractRotation(leftHandMatrix)));
     * print("Translation: " + JSON.stringify(Mat4.extractTranslation(leftHandMatrix)));
     * print("Scale: " + JSON.stringify(Mat4.extractScale(leftHandMatrix))); // Always 1,1,1.
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    // thread safe
    Q_INVOKABLE glm::mat4 getControllerLeftHandMatrix() const;

    /*@jsdoc
     * Gets the rotation and translation of the right hand controller relative to the avatar.
     * @function Avatar.getControllerRightHandMatrix
     * @returns {Mat4} The rotation and translation of the right hand controller relative to the avatar.
     */
    // thread safe
    Q_INVOKABLE glm::mat4 getControllerRightHandMatrix() const;


    /*@jsdoc
     * Gets the amount of avatar mixer data being generated by the avatar.
     * @function Avatar.getDataRate
     * @param {AvatarDataRate} [rateName=""] - The type of avatar mixer data to get the data rate of.
     * @returns {number} The data rate in kbps.
     */
    Q_INVOKABLE float getDataRate(const QString& rateName = QString("")) const;

    /*@jsdoc
     * Gets the update rate of avatar mixer data being generated by the avatar.
     * @function Avatar.getUpdateRate
     * @param {AvatarUpdateRate} [rateName=""] - The type of avatar mixer data to get the update rate of.
     * @returns {number} The update rate in Hz.
     */
    Q_INVOKABLE float getUpdateRate(const QString& rateName = QString("")) const;

    int getJointCount() const { return _jointData.size(); }

    QVector<JointData> getLastSentJointData() {
        QReadLocker readLock(&_jointDataLock);
        _lastSentJointData.resize(_jointData.size());
        return _lastSentJointData;
    }

    // A method intended to be overriden by MyAvatar for polling orientation for network transmission.
    virtual glm::quat getOrientationOutbound() const;

    // TODO: remove this HACK once we settle on optimal sort coefficients
    // These coefficients exposed for fine tuning the sort priority for transfering new _jointData to the render pipeline.
    static float _avatarSortCoefficientSize;
    static float _avatarSortCoefficientCenter;
    static float _avatarSortCoefficientAge;

    bool getIdentityDataChanged() const { return _identityDataChanged; } // has the identity data changed since the last time sendIdentityPacket() was called
    void markIdentityDataChanged() { _identityDataChanged = true; }

    void pushIdentitySequenceNumber() { ++_identitySequenceNumber; };
    bool hasProcessedFirstIdentity() const { return _hasProcessedFirstIdentity; }

    float getDensity() const { return _density; }

    bool getIsReplicated() const { return _isReplicated; }

    void setReplicaIndex(int replicaIndex) { _replicaIndex = replicaIndex; }
    int getReplicaIndex() { return _replicaIndex; }

    static const float DEFAULT_BUBBLE_SCALE;  /* = 2.4 */
    AABox computeBubbleBox(float bubbleScale = DEFAULT_BUBBLE_SCALE) const;

    void setIsNewAvatar(bool isNewAvatar) { _isNewAvatar = isNewAvatar; }
    bool getIsNewAvatar() { return _isNewAvatar; }
    void setIsClientAvatar(bool isClientAvatar) { _isClientAvatar = isClientAvatar; }
    void setSkeletonData(const std::vector<AvatarSkeletonTrait::UnpackedJointData>& skeletonData);
    std::vector<AvatarSkeletonTrait::UnpackedJointData> getSkeletonData() const;
    void sendSkeletonData() const;
    QVector<JointData> getJointData() const;
    glm::vec3 getHeadJointFrontVector() const;

signals:

    /*@jsdoc
     * Triggered when the avatar's <code>displayName</code> property value changes.
     * @function Avatar.displayNameChanged
     * @returns {Signal}
     * @example <caption>Report when your avatar display name changes.</caption>
     * MyAvatar.displayNameChanged.connect(function () {
     *     print("Avatar display name changed to: " + MyAvatar.displayName);
     * });
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    void displayNameChanged();

    /*@jsdoc
     * Triggered when the avatar's <code>sessionDisplayName</code> property value changes.
     * @function Avatar.sessionDisplayNameChanged
     * @returns {Signal}
     * @example <caption>Report when your avatar's session display name changes.</caption>
     * MyAvatar.sessionDisplayNameChanged.connect(function () {
     *     print("Avatar session display name changed to: " + MyAvatar.sessionDisplayName);
     * });
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    void sessionDisplayNameChanged();

    /*@jsdoc
     * Triggered when the avatar's model (i.e., <code>skeletonModelURL</code> property value) changes.
     * @function Avatar.skeletonModelURLChanged
     * @returns {Signal}
     * @example <caption>Report when your avatar's skeleton model changes.</caption>
     * MyAvatar.skeletonModelURLChanged.connect(function () {
     *     print("Skeleton model changed to: " + MyAvatar.skeletonModelURL);
     * });
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    void skeletonModelURLChanged();

    /*@jsdoc
     * Triggered when the avatar's <code>lookAtSnappingEnabled</code> property value changes.
     * @function Avatar.lookAtSnappingChanged
     * @param {boolean} enabled - <code>true</code> if look-at snapping is enabled, <code>false</code> if not.
     * @returns {Signal}
     * @example <caption>Report when your look-at snapping setting changes.</caption>
     * MyAvatar.lookAtSnappingChanged.connect(function () {
     *     print("Avatar look-at snapping changed to: " + MyAvatar.lookAtSnappingEnabled);
     * });
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    void lookAtSnappingChanged(bool enabled);

    /*@jsdoc
     * Triggered when the avatar's <code>sessionUUID</code> property value changes.
     * @function Avatar.sessionUUIDChanged
     * @returns {Signal}
     * @example <caption>Report when your avatar's session UUID changes.</caption>
     * MyAvatar.sessionUUIDChanged.connect(function () {
     *     print("Avatar session UUID changed to: " + MyAvatar.sessionUUID);
     * });
     *
     * // Note: If using from the Avatar API, replace "MyAvatar" with "Avatar".
     */
    void sessionUUIDChanged();

public slots:

/*@jsdoc
     * @function Avatar.sendAvatarDataPacket
     * @param {boolean} [sendAll=false] - Send all.
     * @returns {number}
     * @deprecated This function is deprecated and will be removed.
     */
    virtual int sendAvatarDataPacket(bool sendAll = false);

    /*@jsdoc
     * @function Avatar.sendIdentityPacket
     * @returns {number}
     * @deprecated This function is deprecated and will be removed.
     */
    int sendIdentityPacket();

    /*@jsdoc
     * @function Avatar.setSessionUUID
     * @param {Uuid} sessionUUID - Session UUID.
     * @deprecated This function is deprecated and will be removed.
     */
    virtual void setSessionUUID(const QUuid& sessionUUID) {
        if (sessionUUID != getID()) {
            if (sessionUUID == QUuid()) {
                setID(AVATAR_SELF_ID);
            } else {
                setID(sessionUUID);
            }
            emit sessionUUIDChanged();
        }
    }


    /*@jsdoc
     * Gets the rotation of a joint relative to the avatar.
     * <p><strong>Warning:</strong> Not able to be used in the <code>Avatar</code> API.</p>
     * @function Avatar.getAbsoluteJointRotationInObjectFrame
     * @param {number} index - The index of the joint. <em>Not used.</em>
     * @returns {Quat} <code>Quat.IDENTITY</code>.
     */
    virtual glm::quat getAbsoluteJointRotationInObjectFrame(int index) const override;

    /*@jsdoc
     * Gets the translation of a joint relative to the avatar.
     * <p><strong>Warning:</strong> Not able to be used in the <code>Avatar</code> API.</p>
     * @function Avatar.getAbsoluteJointTranslationInObjectFrame
     * @param {number} index - The index of the joint. <em>Not used.</em>
     * @returns {Vec3} <code>Vec3.ZERO</code>.
     */
    virtual glm::vec3 getAbsoluteJointTranslationInObjectFrame(int index) const override;

    /*@jsdoc
     * Sets the rotation of a joint relative to the avatar.
     * <p><strong>Warning:</strong> Not able to be used in the <code>Avatar</code> API.</p>
     * @function Avatar.setAbsoluteJointRotationInObjectFrame
     * @param {number} index - The index of the joint. <em>Not used.</em>
     * @param {Quat} rotation - The rotation of the joint relative to the avatar. <em>Not used.</em>
     * @returns {boolean} <code>false</code>.
     */
    virtual bool setAbsoluteJointRotationInObjectFrame(int index, const glm::quat& rotation) override { return false; }

    /*@jsdoc
     * Sets the translation of a joint relative to the avatar.
     * <p><strong>Warning:</strong> Not able to be used in the <code>Avatar</code> API.</p>
     * @function Avatar.setAbsoluteJointTranslationInObjectFrame
     * @param {number} index - The index of the joint. <em>Not used.</em>
     * @param {Vec3} translation - The translation of the joint relative to the avatar. <em>Not used.</em>
     * @returns {boolean} <code>false</code>.
     */
    virtual bool setAbsoluteJointTranslationInObjectFrame(int index, const glm::vec3& translation) override { return false; }

    /*@jsdoc
     * Gets the target scale of the avatar without any restrictions on permissible values imposed by the domain. In contrast, the 
     * <code>scale</code> property's value may be limited by the domain's settings.
     * @function Avatar.getTargetScale
     * @returns {number} The target scale of the avatar.
     * @example <caption>Compare the target and current avatar scales.</caption>
     * print("Current avatar scale: " + MyAvatar.scale);
     * print("Target avatar scale:  " + MyAvatar.getTargetScale());
     *
     * // Note: If using from the Avatar API, replace all occurrences of "MyAvatar" with "Avatar".
     */
    float getTargetScale() const { return _targetScale; } // why is this a slot?

    /*@jsdoc
     * @function Avatar.resetLastSent
     * @deprecated This function is deprecated and will be removed.
     */
    void resetLastSent() { _lastToByteArray = 0; }

protected:
    void insertRemovedEntityID(const QUuid entityID);
    void lazyInitHeadData() const;

    float getDistanceBasedMinRotationDOT(glm::vec3 viewerPosition) const;
    float getDistanceBasedMinTranslationDistance(glm::vec3 viewerPosition) const;

    bool avatarBoundingBoxChangedSince(quint64 time) const { return _avatarBoundingBoxChanged >= time; }
    bool avatarScaleChangedSince(quint64 time) const { return _avatarScaleChanged >= time; }
    bool lookAtPositionChangedSince(quint64 time) const { return _headData->lookAtPositionChangedSince(time); }
    bool sensorToWorldMatrixChangedSince(quint64 time) const { return _sensorToWorldMatrixChanged >= time; }
    bool additionalFlagsChangedSince(quint64 time) const { return _additionalFlagsChanged >= time; }
    bool parentInfoChangedSince(quint64 time) const { return _parentChanged >= time; }
    bool faceTrackerInfoChangedSince(quint64 time) const { return true; } // FIXME

    bool hasParent() const { return !getParentID().isNull(); }

    QByteArray packSkeletonData() const;
    QByteArray packSkeletonModelURL() const;
    QByteArray packAvatarEntityTraitInstance(AvatarTraits::TraitInstanceID traitInstanceID);
    QByteArray packGrabTraitInstance(AvatarTraits::TraitInstanceID traitInstanceID);

    void unpackSkeletonModelURL(const QByteArray& data);
    void unpackSkeletonData(const QByteArray& data);
    
    // isReplicated will be true on downstream Avatar Mixers and their clients, but false on the upstream "master"
    // Audio Mixer that the replicated avatar is connected to.
    bool _isReplicated{ false };

    glm::vec3 _handPosition;
    virtual const QString& getSessionDisplayNameForTransport() const { return _sessionDisplayName; }
    virtual void maybeUpdateSessionDisplayNameFromTransport(const QString& sessionDisplayName) { } // No-op in AvatarMixer

    // Body scale
    float _targetScale;
    float _domainMinimumHeight { MIN_AVATAR_HEIGHT };
    float _domainMaximumHeight { MAX_AVATAR_HEIGHT };

    //  Hand state (are we grabbing something or not)
    char _handState;

    QVector<JointData> _jointData; ///< the state of the skeleton joints
    QVector<JointData> _lastSentJointData; ///< the state of the skeleton joints last time we transmitted
    mutable QReadWriteLock _jointDataLock;

    // key state
    KeyState _keyState;

    bool _hasNewJointData { true }; // set in AvatarData, cleared in Avatar

    mutable HeadData* _headData { nullptr };

    QUrl _skeletonModelURL;
    QVector<AttachmentData> _attachmentData;
    QVector<AttachmentData> _oldAttachmentData;
    QString _displayName;
    QString _sessionDisplayName { };
    bool _lookAtSnappingEnabled { true };
    bool _verificationFailed { false };

    quint64 _errorLogExpiry; ///< time in future when to log an error

    QWeakPointer<Node> _owningAvatarMixer;

    glm::vec3 _targetVelocity;

    SimpleMovingAverage _averageBytesReceived;

    // During recording, this holds the starting position, orientation & scale of the recorded avatar
    // During playback, it holds the origin from which to play the relative positions in the clip
    TransformPointer _recordingBasis;

    // _globalPosition is sent along with localPosition + parent because the avatar-mixer doesn't know
    // where Entities are located.  This is currently only used by the mixer to decide how often to send
    // updates about one avatar to another.
    glm::vec3 _globalPosition { 0, 0, 0 };
    glm::vec3 _serverPosition { 0, 0, 0 };

    quint64 _globalPositionChanged { 0 };
    quint64 _avatarBoundingBoxChanged { 0 };
    quint64 _avatarScaleChanged { 0 };
    quint64 _sensorToWorldMatrixChanged { 0 };
    quint64 _additionalFlagsChanged { 0 };
    quint64 _parentChanged { 0 };

    quint64  _lastToByteArray { 0 }; // tracks the last time we did a toByteArray

    // Some rate data for incoming data in bytes
    RateCounter<> _parseBufferRate;
    RateCounter<> _globalPositionRate;
    RateCounter<> _localPositionRate;
    RateCounter<> _handControllersRate;
    RateCounter<> _avatarBoundingBoxRate;
    RateCounter<> _avatarOrientationRate;
    RateCounter<> _avatarScaleRate;
    RateCounter<> _lookAtPositionRate;
    RateCounter<> _audioLoudnessRate;
    RateCounter<> _sensorToWorldRate;
    RateCounter<> _additionalFlagsRate;
    RateCounter<> _parentInfoRate;
    RateCounter<> _faceTrackerRate;
    RateCounter<> _jointDataRate;
    RateCounter<> _jointDefaultPoseFlagsRate;
    RateCounter<> _farGrabJointRate;

    // Some rate data for incoming data updates
    RateCounter<> _parseBufferUpdateRate;
    RateCounter<> _globalPositionUpdateRate;
    RateCounter<> _localPositionUpdateRate;
    RateCounter<> _handControllersUpdateRate;
    RateCounter<> _avatarBoundingBoxUpdateRate;
    RateCounter<> _avatarOrientationUpdateRate;
    RateCounter<> _avatarScaleUpdateRate;
    RateCounter<> _lookAtPositionUpdateRate;
    RateCounter<> _audioLoudnessUpdateRate;
    RateCounter<> _sensorToWorldUpdateRate;
    RateCounter<> _additionalFlagsUpdateRate;
    RateCounter<> _parentInfoUpdateRate;
    RateCounter<> _faceTrackerUpdateRate;
    RateCounter<> _jointDataUpdateRate;
    RateCounter<> _jointDefaultPoseFlagsUpdateRate;
    RateCounter<> _farGrabJointUpdateRate;

    // Some rate data for outgoing data
    AvatarDataRate _outboundDataRate;

    glm::vec3 _globalBoundingBoxDimensions;
    glm::vec3 _globalBoundingBoxOffset;

    AABox _defaultBubbleBox;
    AABox _fitBoundingBox;

    mutable ReadWriteLockable _avatarEntitiesLock;
    AvatarEntityIDs _avatarEntityRemoved; // recently removed AvatarEntity ids
    AvatarEntityIDs _avatarEntityForRecording; // create new entities id for avatar recording
    PackedAvatarEntityMap _packedAvatarEntityData;
    bool _avatarEntityDataChanged { false };

    mutable ReadWriteLockable _avatarGrabsLock;
    AvatarGrabDataMap _avatarGrabData;
    bool _avatarGrabDataChanged { false }; // by network

    mutable ReadWriteLockable _avatarSkeletonDataLock;
    std::vector<AvatarSkeletonTrait::UnpackedJointData> _avatarSkeletonData;

    // used to transform any sensor into world space, including the _hmdSensorMat, or hand controllers.
    ThreadSafeValueCache<glm::mat4> _sensorToWorldMatrixCache { glm::mat4() };
    ThreadSafeValueCache<glm::mat4> _controllerLeftHandMatrixCache { glm::mat4() };
    ThreadSafeValueCache<glm::mat4> _controllerRightHandMatrixCache { glm::mat4() };

    ThreadSafeValueCache<glm::mat4> _farGrabRightMatrixCache { glm::mat4() };
    ThreadSafeValueCache<glm::mat4> _farGrabLeftMatrixCache { glm::mat4() };
    ThreadSafeValueCache<glm::mat4> _farGrabMouseMatrixCache { glm::mat4() };

    ThreadSafeValueCache<QVariantMap> _collisionCapsuleCache{ QVariantMap() };

    int getFauxJointIndex(const QString& name) const;

    float _audioLoudness { 0.0f };
    quint64 _audioLoudnessChanged { 0 };
    float _audioAverageLoudness { 0.0f };

    bool _identityDataChanged { false };
    udt::SequenceNumber _identitySequenceNumber { 0 };
    bool _hasProcessedFirstIdentity { false };
    float _density;
    int _replicaIndex { 0 };
    bool _isNewAvatar { true };
    bool _isClientAvatar { false };
    bool _collideWithOtherAvatars { true };
    bool _hasPriority{ false };

    // null unless MyAvatar or ScriptableAvatar sending traits data to mixer
    std::unique_ptr<ClientTraitsHandler, LaterDeleter> _clientTraitsHandler;

    template <typename T, typename F>
    T readLockWithNamedJointIndex(const QString& name, const T& defaultValue, F f) const {
        QReadLocker readLock(&_jointDataLock);
        int index = getJointIndex(name);
        if (index == -1) {
            return defaultValue;
        }
        return f(index);
    }

    template <typename T, typename F>
    T readLockWithNamedJointIndex(const QString& name, F f) const {
        return readLockWithNamedJointIndex(name, T(), f);
    }

    template <typename F>
    void writeLockWithNamedJointIndex(const QString& name, F f) {
        QWriteLocker writeLock(&_jointDataLock);
        int index = getJointIndex(name);
        if (index == -1) {
            return;
        }
        if (_jointData.size() <= index) {
            _jointData.resize(index + 1);
        }
        f(index);
    }

    bool updateAvatarGrabData(const QUuid& grabID, const QByteArray& grabData);
    virtual void clearAvatarGrabData(const QUuid& grabID);

private:
    friend void avatarStateFromFrame(const QByteArray& frameData, AvatarData* _avatar);
    static QUrl _defaultFullAvatarModelUrl;
    // privatize the copy constructor and assignment operator so they cannot be called
    AvatarData(const AvatarData&);
    AvatarData& operator= (const AvatarData&);
};
Q_DECLARE_METATYPE(AvatarData*)

QJsonValue toJsonValue(const JointData& joint);
JointData jointDataFromJsonValue(const QJsonValue& q);

class AttachmentData {
public:
    QUrl modelURL;
    QString jointName;
    glm::vec3 translation;
    glm::quat rotation;
    float scale { 1.0f };
    bool isSoft { false };

    bool isValid() const { return modelURL.isValid(); }

    bool operator==(const AttachmentData& other) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    QVariant toVariant() const;
    bool fromVariant(const QVariant& variant);
};

QDataStream& operator<<(QDataStream& out, const AttachmentData& attachment);
QDataStream& operator>>(QDataStream& in, AttachmentData& attachment);

Q_DECLARE_METATYPE(AttachmentData)
Q_DECLARE_METATYPE(QVector<AttachmentData>)

/// Scriptable wrapper for attachments.
class AttachmentDataObject : public QObject, protected Scriptable {
    Q_OBJECT
    Q_PROPERTY(QString modelURL READ getModelURL WRITE setModelURL)
    Q_PROPERTY(QString jointName READ getJointName WRITE setJointName)
    Q_PROPERTY(glm::vec3 translation READ getTranslation WRITE setTranslation)
    Q_PROPERTY(glm::quat rotation READ getRotation WRITE setRotation)
    Q_PROPERTY(float scale READ getScale WRITE setScale)
    Q_PROPERTY(bool isSoft READ getIsSoft WRITE setIsSoft)

public:

    Q_INVOKABLE void setModelURL(const QString& modelURL);
    Q_INVOKABLE QString getModelURL() const;

    Q_INVOKABLE void setJointName(const QString& jointName);
    Q_INVOKABLE QString getJointName() const;

    Q_INVOKABLE void setTranslation(const glm::vec3& translation);
    Q_INVOKABLE glm::vec3 getTranslation() const;

    Q_INVOKABLE void setRotation(const glm::quat& rotation);
    Q_INVOKABLE glm::quat getRotation() const;

    Q_INVOKABLE void setScale(float scale);
    Q_INVOKABLE float getScale() const;

    Q_INVOKABLE void setIsSoft(bool scale);
    Q_INVOKABLE bool getIsSoft() const;
};

void registerAvatarTypes(ScriptEngine* engine);

class RayToAvatarIntersectionResult {
public:
    bool intersects { false };
    QUuid avatarID;
    float distance { FLT_MAX };
    BoxFace face;
    glm::vec3 intersection;
    glm::vec3 surfaceNormal;
    int jointIndex { -1 };
    QVariantMap extraInfo;
};
Q_DECLARE_METATYPE(RayToAvatarIntersectionResult)
ScriptValuePointer RayToAvatarIntersectionResultToScriptValue(ScriptEngine* engine, const RayToAvatarIntersectionResult& results);
void RayToAvatarIntersectionResultFromScriptValue(const ScriptValuePointer& object, RayToAvatarIntersectionResult& results);

// No JSDoc because it's not provided as a type to the script engine.
class ParabolaToAvatarIntersectionResult {
public:
    bool intersects { false };
    QUuid avatarID;
    float distance { 0.0f };
    float parabolicDistance { 0.0f };
    BoxFace face;
    glm::vec3 intersection;
    glm::vec3 surfaceNormal;
    QVariantMap extraInfo;
};

Q_DECLARE_METATYPE(AvatarEntityMap)

ScriptValuePointer AvatarEntityMapToScriptValue(ScriptEngine* engine, const AvatarEntityMap& value);
void AvatarEntityMapFromScriptValue(const ScriptValuePointer& object, AvatarEntityMap& value);

// faux joint indexes (-1 means invalid)
const int NO_JOINT_INDEX = 65535; // -1
const int SENSOR_TO_WORLD_MATRIX_INDEX = 65534; // -2
const int CONTROLLER_RIGHTHAND_INDEX = 65533; // -3
const int CONTROLLER_LEFTHAND_INDEX = 65532; // -4
const int CAMERA_RELATIVE_CONTROLLER_RIGHTHAND_INDEX = 65531; // -5
const int CAMERA_RELATIVE_CONTROLLER_LEFTHAND_INDEX = 65530; // -6
const int CAMERA_MATRIX_INDEX = 65529; // -7
const int FARGRAB_RIGHTHAND_INDEX = 65528; // -8
const int FARGRAB_LEFTHAND_INDEX = 65527; // -9
const int FARGRAB_MOUSE_INDEX = 65526; // -10

const int LOWEST_PSEUDO_JOINT_INDEX = 65526;

const int MAX_NUM_AVATAR_GRABS = 6;

#endif // hifi_AvatarData_h
