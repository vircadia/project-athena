//
//  AvatarData.cpp
//  libraries/avatars/src
//
//  Created by Stephen Birarda on 4/9/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "AvatarData.h"

#include <cstdio>
#include <cstring>
#include <stdint.h>

#include <QtCore/QDataStream>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <shared/QtHelpers.h>
#include <QVariantGLM.h>
#include <Transform.h>
#include <NetworkAccessManager.h>
#include <NodeList.h>
#include <udt/PacketHeaders.h>
#include <GLMHelpers.h>
#include <StreamUtils.h>
#include <UUID.h>
#include <shared/JSONHelpers.h>
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptValueIterator.h>
#include <ShapeInfo.h>
#include <AudioHelpers.h>
#include <Profile.h>
#include <VariantMapToScriptValue.h>
#include <BitVectorHelpers.h>
#include <ScriptValueUtils.h>

#include "AvatarLogging.h"
#include "AvatarTraits.h"
#include "ClientTraitsHandler.h"
#include "ResourceRequestObserver.h"

//#define WANT_DEBUG

quint64 DEFAULT_FILTERED_LOG_EXPIRY = 2 * USECS_PER_SECOND;

using namespace std;

const QString AvatarData::FRAME_NAME = "com.highfidelity.recording.AvatarData";

static const int TRANSLATION_COMPRESSION_RADIX = 14;
static const int HAND_CONTROLLER_COMPRESSION_RADIX = 12;
static const int SENSOR_TO_WORLD_SCALE_RADIX = 10;
static const float AUDIO_LOUDNESS_SCALE = 1024.0f;
static const float DEFAULT_AVATAR_DENSITY = 1000.0f; // density of water

#define ASSERT(COND)  do { if (!(COND)) { abort(); } } while(0)

size_t AvatarDataPacket::maxFaceTrackerInfoSize(size_t numBlendshapeCoefficients) {
    return FACE_TRACKER_INFO_SIZE + numBlendshapeCoefficients * sizeof(float);
}

size_t AvatarDataPacket::maxJointDataSize(size_t numJoints) {
    const size_t validityBitsSize = calcBitVectorSize((int)numJoints);

    size_t totalSize = sizeof(uint8_t); // numJoints

    totalSize += validityBitsSize; // Orientations mask
    totalSize += numJoints * sizeof(SixByteQuat); // Orientations
    totalSize += validityBitsSize; // Translations mask
    totalSize += sizeof(float); // maxTranslationDimension
    totalSize += numJoints * sizeof(SixByteTrans); // Translations
    return totalSize;
}

size_t AvatarDataPacket::minJointDataSize(size_t numJoints) {
    const size_t validityBitsSize = calcBitVectorSize((int)numJoints);

    size_t totalSize = sizeof(uint8_t); // numJoints

    totalSize += validityBitsSize; // Orientations mask
    // assume no valid rotations
    totalSize += validityBitsSize; // Translations mask
    totalSize += sizeof(float); // maxTranslationDimension
    // assume no valid translations

    return totalSize;
}

size_t AvatarDataPacket::maxJointDefaultPoseFlagsSize(size_t numJoints) {
    const size_t bitVectorSize = calcBitVectorSize((int)numJoints);
    size_t totalSize = sizeof(uint8_t); // numJoints

    // one set of bits for rotation and one for translation
    const size_t NUM_BIT_VECTORS_IN_DEFAULT_POSE_FLAGS_SECTION = 2;
    totalSize += NUM_BIT_VECTORS_IN_DEFAULT_POSE_FLAGS_SECTION * bitVectorSize;

    return totalSize;
}

AvatarData::AvatarData() :
    SpatiallyNestable(NestableType::Avatar, QUuid()),
    _handPosition(0.0f),
    _targetScale(1.0f),
    _handState(0),
    _keyState(NO_KEY_DOWN),
    _headData(NULL),
    _errorLogExpiry(0),
    _owningAvatarMixer(),
    _targetVelocity(0.0f),
    _density(DEFAULT_AVATAR_DENSITY)
{
    connect(this, &AvatarData::lookAtSnappingChanged, this, &AvatarData::markIdentityDataChanged);
}

AvatarData::~AvatarData() {
    delete _headData;
}

// We cannot have a file-level variable (const or otherwise) in the AvatarInfo if it uses PathUtils, because that references Application, which will not yet initialized.
// Thus we have a static class getter, referencing a static class var.
QUrl AvatarData::_defaultFullAvatarModelUrl = {}; // In C++, if this initialization were in the AvatarInfo, every file would have it's own copy, even for class vars.
const QUrl& AvatarData::defaultFullAvatarModelUrl() {
    if (_defaultFullAvatarModelUrl.isEmpty()) {
        _defaultFullAvatarModelUrl = PathUtils::resourcesUrl("/meshes/defaultAvatar_full.fst");
    }
    return _defaultFullAvatarModelUrl;
}

void AvatarData::setTargetScale(float targetScale) {
    auto newValue = glm::clamp(targetScale, MIN_AVATAR_SCALE, MAX_AVATAR_SCALE);
    if (_targetScale != newValue) {
        _targetScale = newValue;
        _scaleChanged = usecTimestampNow();
        _avatarScaleChanged = _scaleChanged;
    }
}

float AvatarData::getDomainLimitedScale() const {
    if (canMeasureEyeHeight()) {
        const float minScale = getDomainMinScale();
        const float maxScale = getDomainMaxScale();
        return glm::clamp(_targetScale, minScale, maxScale);
    } else {
        // We can't make a good estimate.
        return _targetScale;
    }
}


void AvatarData::setHasScriptedBlendshapes(bool hasScriptedBlendshapes) {
    if (hasScriptedBlendshapes == _headData->getHasScriptedBlendshapes()) {
        return;
    }
    if (!hasScriptedBlendshapes) {
        // send a forced avatarData update to make sure the script can send neutal blendshapes on unload
        // without having to wait for the update loop, make sure _hasScriptedBlendShapes is still true
        // before sending the update, or else it won't send the neutal blendshapes to the receiving clients
        sendAvatarDataPacket(true);
    }
    _headData->setHasScriptedBlendshapes(hasScriptedBlendshapes);
}

bool AvatarData::getHasScriptedBlendshapes() const {
    return _headData->getHasScriptedBlendshapes();
}

void AvatarData::setHasProceduralBlinkFaceMovement(bool value) {
    _headData->setProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation, value);
}

bool AvatarData::getHasProceduralBlinkFaceMovement() const {
    return _headData->getProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation);
}

void AvatarData::setHasProceduralEyeFaceMovement(bool value) {
    _headData->setProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation, value);
}

bool AvatarData::getHasProceduralEyeFaceMovement() const {
    return _headData->getProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation);
}

void AvatarData::setHasAudioEnabledFaceMovement(bool value) {
    _headData->setProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation, value);
}

bool AvatarData::getHasAudioEnabledFaceMovement() const {
    return _headData->getProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation);
}

void AvatarData::setDomainMinimumHeight(float domainMinimumHeight) {
    _domainMinimumHeight = glm::clamp(domainMinimumHeight, MIN_AVATAR_HEIGHT, MAX_AVATAR_HEIGHT);
}

void AvatarData::setDomainMaximumHeight(float domainMaximumHeight) {
    _domainMaximumHeight = glm::clamp(domainMaximumHeight, MIN_AVATAR_HEIGHT, MAX_AVATAR_HEIGHT);
}

float AvatarData::getDomainMinScale() const {
    float unscaledHeight = getUnscaledHeight();
    const float EPSILON = 1.0e-4f;
    if (unscaledHeight <= EPSILON) {
        unscaledHeight = DEFAULT_AVATAR_HEIGHT;
    }
    return _domainMinimumHeight / unscaledHeight;
}

float AvatarData::getDomainMaxScale() const {
    float unscaledHeight = getUnscaledHeight();
    const float EPSILON = 1.0e-4f;
    if (unscaledHeight <= EPSILON) {
        unscaledHeight = DEFAULT_AVATAR_HEIGHT;
    }
    return _domainMaximumHeight / unscaledHeight;
}

float AvatarData::getUnscaledHeight() const {
    const float eyeHeight = getUnscaledEyeHeight();
    const float ratio = eyeHeight / DEFAULT_AVATAR_HEIGHT;
    return eyeHeight + ratio * DEFAULT_AVATAR_EYE_TO_TOP_OF_HEAD;
}

float AvatarData::getHeight() const {
    const float eyeHeight = getEyeHeight();
    const float ratio = eyeHeight / DEFAULT_AVATAR_HEIGHT;
    return eyeHeight + ratio * DEFAULT_AVATAR_EYE_TO_TOP_OF_HEAD;
}

glm::vec3 AvatarData::getHandPosition() const {
    return getWorldOrientation() * _handPosition + getWorldPosition();
}

void AvatarData::setHandPosition(const glm::vec3& handPosition) {
    // store relative to position/orientation
    _handPosition = glm::inverse(getWorldOrientation()) * (handPosition - getWorldPosition());
}

void AvatarData::lazyInitHeadData() const {
    // lazily allocate memory for HeadData in case we're not an Avatar instance
    if (!_headData) {
        _headData = new HeadData(const_cast<AvatarData*>(this));
    }
}


float AvatarData::getDistanceBasedMinRotationDOT(glm::vec3 viewerPosition) const {
    auto distance = glm::distance(_globalPosition, viewerPosition);
    float result = ROTATION_CHANGE_179D; // assume worst
    if (distance < AVATAR_DISTANCE_LEVEL_1) {
        result = AVATAR_MIN_ROTATION_DOT;
    } else if (distance < AVATAR_DISTANCE_LEVEL_2) {
        result = ROTATION_CHANGE_2D;
    } else if (distance < AVATAR_DISTANCE_LEVEL_3) {
        result = ROTATION_CHANGE_4D;
    } else if (distance < AVATAR_DISTANCE_LEVEL_4) {
        result = ROTATION_CHANGE_6D;
    } else if (distance < AVATAR_DISTANCE_LEVEL_5) {
        result = ROTATION_CHANGE_15D;
    }
    return result;
}

float AvatarData::getDistanceBasedMinTranslationDistance(glm::vec3 viewerPosition) const {
    return AVATAR_MIN_TRANSLATION; // Eventually make this distance sensitive as well
}


// we want to track outbound data in this case...
QByteArray AvatarData::toByteArrayStateful(AvatarDataDetail dataDetail, bool dropFaceTracking) {
    auto lastSentTime = _lastToByteArray;
    _lastToByteArray = usecTimestampNow();
    AvatarDataPacket::SendStatus sendStatus;
    auto avatarByteArray = AvatarData::toByteArray(dataDetail, lastSentTime, getLastSentJointData(),
        sendStatus, dropFaceTracking, false, glm::vec3(0), nullptr, 0, &_outboundDataRate);
    return avatarByteArray;
}

QByteArray AvatarData::toByteArray(AvatarDataDetail dataDetail, quint64 lastSentTime,
                                   const QVector<JointData>& lastSentJointData, AvatarDataPacket::SendStatus& sendStatus,
                                   bool dropFaceTracking, bool distanceAdjust, glm::vec3 viewerPosition,
                                   QVector<JointData>* sentJointDataOut,
                                   int maxDataSize, AvatarDataRate* outboundDataRateOut) const {

    bool cullSmallChanges = (dataDetail == CullSmallData);
    bool sendAll = (dataDetail == SendAllData);
    bool sendMinimum = (dataDetail == MinimumData);
    bool sendPALMinimum = (dataDetail == PALMinimum);

    lazyInitHeadData();
    ASSERT(maxDataSize == 0 || (size_t)maxDataSize >= AvatarDataPacket::MIN_BULK_PACKET_SIZE);

    // Leading flags, to indicate how much data is actually included in the packet...
    AvatarDataPacket::HasFlags wantedFlags = 0;
    AvatarDataPacket::HasFlags includedFlags = 0;
    AvatarDataPacket::HasFlags extraReturnedFlags = 0;  // For partial joint data.

    // special case, if we were asked for no data, then just include the flags all set to nothing
    if (dataDetail == NoData) {
        sendStatus.itemFlags = wantedFlags;

        QByteArray avatarDataByteArray;
        if (sendStatus.sendUUID) {
            avatarDataByteArray.append(getSessionUUID().toRfc4122().data(), NUM_BYTES_RFC4122_UUID);
        }

        avatarDataByteArray.append((char*) &wantedFlags, sizeof wantedFlags);
        return avatarDataByteArray;
    }

    // FIXME -
    //
    //    BUG -- if you enter a space bubble, and then back away, the avatar has wrong orientation until "send all" happens...
    //      this is an iFrame issue... what to do about that?
    //
    //    BUG -- Resizing avatar seems to "take too long"... the avatar doesn't redraw at smaller size right away
    //
    // TODO consider these additional optimizations in the future
    // 1) SensorToWorld - should we only send this for avatars with attachments?? - 20 bytes - 7.20 kbps
    // 2) GUIID for the session change to 2byte index                   (savings) - 14 bytes - 5.04 kbps
    // 3) Improve Joints -- currently we use rotational tolerances, but if we had skeleton/bone length data
    //    we could do a better job of determining if the change in joints actually translates to visible
    //    changes at distance.
    //
    //    Potential savings:
    //              63 rotations   * 6 bytes = 136kbps
    //              3 translations * 6 bytes = 6.48kbps
    //

    QUuid parentID;

    glm::mat4 leftFarGrabMatrix;
    glm::mat4 rightFarGrabMatrix;
    glm::mat4 mouseFarGrabMatrix;

    if (sendStatus.itemFlags == 0) {
        // New avatar ...
        bool hasAvatarGlobalPosition = true; // always include global position
        bool hasAvatarOrientation = false;
        bool hasAvatarBoundingBox = false;
        bool hasAvatarScale = false;
        bool hasLookAtPosition = false;
        bool hasAudioLoudness = false;
        bool hasSensorToWorldMatrix = false;
        bool hasJointData = false;
        bool hasJointDefaultPoseFlags = false;
        bool hasAdditionalFlags = false;

        // local position, and parent info only apply to avatars that are parented. The local position
        // and the parent info can change independently though, so we track their "changed since"
        // separately
        bool hasParentInfo = false;
        bool hasAvatarLocalPosition = false;
        bool hasHandControllers = false;

        bool hasFaceTrackerInfo = false;

        if (sendPALMinimum) {
            hasAudioLoudness = true;
        } else {
            hasAvatarOrientation = sendAll || rotationChangedSince(lastSentTime);
            hasAvatarBoundingBox = sendAll || avatarBoundingBoxChangedSince(lastSentTime);
            hasAvatarScale = sendAll || avatarScaleChangedSince(lastSentTime);
            hasLookAtPosition = sendAll || lookAtPositionChangedSince(lastSentTime);
            hasAudioLoudness = sendAll || audioLoudnessChangedSince(lastSentTime);
            hasSensorToWorldMatrix = sendAll || sensorToWorldMatrixChangedSince(lastSentTime);
            hasAdditionalFlags = sendAll || additionalFlagsChangedSince(lastSentTime);
            hasParentInfo = sendAll || parentInfoChangedSince(lastSentTime);
            hasAvatarLocalPosition = hasParent() && (sendAll ||
                tranlationChangedSince(lastSentTime) ||
                parentInfoChangedSince(lastSentTime));
            hasHandControllers = _controllerLeftHandMatrixCache.isValid() || _controllerRightHandMatrixCache.isValid();
            hasFaceTrackerInfo = !dropFaceTracking && (getHasScriptedBlendshapes() || _headData->_hasInputDrivenBlendshapes) &&
                (sendAll || faceTrackerInfoChangedSince(lastSentTime));
            hasJointData = !sendMinimum;
            hasJointDefaultPoseFlags = hasJointData;
        }

        wantedFlags =
            (hasAvatarGlobalPosition ? AvatarDataPacket::PACKET_HAS_AVATAR_GLOBAL_POSITION : 0)
            | (hasAvatarBoundingBox ? AvatarDataPacket::PACKET_HAS_AVATAR_BOUNDING_BOX : 0)
            | (hasAvatarOrientation ? AvatarDataPacket::PACKET_HAS_AVATAR_ORIENTATION : 0)
            | (hasAvatarScale ? AvatarDataPacket::PACKET_HAS_AVATAR_SCALE : 0)
            | (hasLookAtPosition ? AvatarDataPacket::PACKET_HAS_LOOK_AT_POSITION : 0)
            | (hasAudioLoudness ? AvatarDataPacket::PACKET_HAS_AUDIO_LOUDNESS : 0)
            | (hasSensorToWorldMatrix ? AvatarDataPacket::PACKET_HAS_SENSOR_TO_WORLD_MATRIX : 0)
            | (hasAdditionalFlags ? AvatarDataPacket::PACKET_HAS_ADDITIONAL_FLAGS : 0)
            | (hasParentInfo ? AvatarDataPacket::PACKET_HAS_PARENT_INFO : 0)
            | (hasAvatarLocalPosition ? AvatarDataPacket::PACKET_HAS_AVATAR_LOCAL_POSITION : 0)
            | (hasHandControllers ? AvatarDataPacket::PACKET_HAS_HAND_CONTROLLERS : 0)
            | (hasFaceTrackerInfo ? AvatarDataPacket::PACKET_HAS_FACE_TRACKER_INFO : 0)
            | (hasJointData ? AvatarDataPacket::PACKET_HAS_JOINT_DATA : 0)
            | (hasJointDefaultPoseFlags ? AvatarDataPacket::PACKET_HAS_JOINT_DEFAULT_POSE_FLAGS : 0)
            | (hasJointData ? AvatarDataPacket::PACKET_HAS_GRAB_JOINTS : 0);

            sendStatus.itemFlags = wantedFlags;
            sendStatus.rotationsSent = 0;
            sendStatus.translationsSent = 0;
    } else {  // Continuing avatar ...
        wantedFlags = sendStatus.itemFlags;
        if (wantedFlags & AvatarDataPacket::PACKET_HAS_GRAB_JOINTS) {
            // Must send joints for grab joints -
            wantedFlags |= AvatarDataPacket::PACKET_HAS_JOINT_DATA;
        }
    }

    if (wantedFlags & AvatarDataPacket::PACKET_HAS_GRAB_JOINTS) {
        bool leftValid;
        leftFarGrabMatrix = _farGrabLeftMatrixCache.get(leftValid);
        if (!leftValid) {
            leftFarGrabMatrix = glm::mat4();
        }
        bool rightValid;
        rightFarGrabMatrix = _farGrabRightMatrixCache.get(rightValid);
        if (!rightValid) {
            rightFarGrabMatrix = glm::mat4();
        }
        bool mouseValid;
        mouseFarGrabMatrix = _farGrabMouseMatrixCache.get(mouseValid);
        if (!mouseValid) {
            mouseFarGrabMatrix = glm::mat4();
        }
        if (!(leftValid || rightValid || mouseValid)) {
            wantedFlags &= ~AvatarDataPacket::PACKET_HAS_GRAB_JOINTS;
        }
    }
    if (wantedFlags & (AvatarDataPacket::PACKET_HAS_ADDITIONAL_FLAGS | AvatarDataPacket::PACKET_HAS_PARENT_INFO)) {
        parentID = getParentID();
    }

    const size_t byteArraySize = AvatarDataPacket::MAX_CONSTANT_HEADER_SIZE + NUM_BYTES_RFC4122_UUID +
        AvatarDataPacket::maxFaceTrackerInfoSize(_headData->getBlendshapeCoefficients().size()) +
        AvatarDataPacket::maxJointDataSize(_jointData.size()) +
        AvatarDataPacket::maxJointDefaultPoseFlagsSize(_jointData.size()) +
        AvatarDataPacket::FAR_GRAB_JOINTS_SIZE;

    if (maxDataSize == 0) {
        maxDataSize = (int)byteArraySize;
    }

    QByteArray avatarDataByteArray((int)byteArraySize, 0);
    unsigned char* destinationBuffer = reinterpret_cast<unsigned char*>(avatarDataByteArray.data());
    const unsigned char* const startPosition = destinationBuffer;
    const unsigned char* const packetEnd = destinationBuffer + maxDataSize;

#define AVATAR_MEMCPY(src)                          \
    memcpy(destinationBuffer, &(src), sizeof(src)); \
    destinationBuffer += sizeof(src);

// If we want an item and there's sufficient space:
#define IF_AVATAR_SPACE(flag, space)                              \
    if ((wantedFlags & AvatarDataPacket::flag)                    \
        && (packetEnd - destinationBuffer) >= (ptrdiff_t)(space)  \
        && (includedFlags |= AvatarDataPacket::flag))

    if (sendStatus.sendUUID) {
        memcpy(destinationBuffer, getSessionUUID().toRfc4122(), NUM_BYTES_RFC4122_UUID);
        destinationBuffer += NUM_BYTES_RFC4122_UUID;
    }

    unsigned char * packetFlagsLocation = destinationBuffer;
    destinationBuffer += sizeof(wantedFlags);

    IF_AVATAR_SPACE(PACKET_HAS_AVATAR_GLOBAL_POSITION, sizeof _globalPosition) {
        auto startSection = destinationBuffer;
        AVATAR_MEMCPY(_globalPosition);
        
        int numBytes = destinationBuffer - startSection;

        if (outboundDataRateOut) {
            outboundDataRateOut->globalPositionRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_AVATAR_BOUNDING_BOX, sizeof _globalBoundingBoxDimensions + sizeof _globalBoundingBoxOffset) {
        auto startSection = destinationBuffer;
        AVATAR_MEMCPY(_globalBoundingBoxDimensions);
        AVATAR_MEMCPY(_globalBoundingBoxOffset);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->avatarBoundingBoxRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_AVATAR_ORIENTATION, sizeof(AvatarDataPacket::SixByteQuat)) {
        auto startSection = destinationBuffer;
        auto localOrientation = getOrientationOutbound();
        destinationBuffer += packOrientationQuatToSixBytes(destinationBuffer, localOrientation);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->avatarOrientationRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_AVATAR_SCALE, sizeof(AvatarDataPacket::AvatarScale)) {
        auto startSection = destinationBuffer;
        auto data = reinterpret_cast<AvatarDataPacket::AvatarScale*>(destinationBuffer);
        auto scale = getDomainLimitedScale();
        packFloatRatioToTwoByte((uint8_t*)(&data->scale), scale);
        destinationBuffer += sizeof(AvatarDataPacket::AvatarScale);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->avatarScaleRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_LOOK_AT_POSITION, sizeof(_headData->getLookAtPosition()) ) {
        auto startSection = destinationBuffer;
        AVATAR_MEMCPY(_headData->getLookAtPosition());
        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->lookAtPositionRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_AUDIO_LOUDNESS, sizeof(AvatarDataPacket::AudioLoudness)) {
        auto startSection = destinationBuffer;
        auto data = reinterpret_cast<AvatarDataPacket::AudioLoudness*>(destinationBuffer);
        data->audioLoudness = packFloatGainToByte(getAudioLoudness() / AUDIO_LOUDNESS_SCALE);
        destinationBuffer += sizeof(AvatarDataPacket::AudioLoudness);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->audioLoudnessRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_SENSOR_TO_WORLD_MATRIX, sizeof(AvatarDataPacket::SensorToWorldMatrix)) {
        auto startSection = destinationBuffer;
        auto data = reinterpret_cast<AvatarDataPacket::SensorToWorldMatrix*>(destinationBuffer);
        glm::mat4 sensorToWorldMatrix = getSensorToWorldMatrix();
        packOrientationQuatToSixBytes(data->sensorToWorldQuat, glmExtractRotation(sensorToWorldMatrix));
        glm::vec3 scale = extractScale(sensorToWorldMatrix);
        packFloatScalarToSignedTwoByteFixed((uint8_t*)&data->sensorToWorldScale, scale.x, SENSOR_TO_WORLD_SCALE_RADIX);
        data->sensorToWorldTrans[0] = sensorToWorldMatrix[3][0];
        data->sensorToWorldTrans[1] = sensorToWorldMatrix[3][1];
        data->sensorToWorldTrans[2] = sensorToWorldMatrix[3][2];
        destinationBuffer += sizeof(AvatarDataPacket::SensorToWorldMatrix);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->sensorToWorldRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_ADDITIONAL_FLAGS, sizeof (uint16_t)) {
        auto startSection = destinationBuffer;
        auto data = reinterpret_cast<AvatarDataPacket::AdditionalFlags*>(destinationBuffer);

        uint16_t flags { 0 };

        setSemiNibbleAt(flags, KEY_STATE_START_BIT, _keyState);

        // hand state
        bool isFingerPointing = _handState & IS_FINGER_POINTING_FLAG;
        setSemiNibbleAt(flags, HAND_STATE_START_BIT, _handState & ~IS_FINGER_POINTING_FLAG);
        if (isFingerPointing) {
            setAtBit16(flags, HAND_STATE_FINGER_POINTING_BIT);
        }
        // face tracker state
        if (_headData->_hasScriptedBlendshapes || _headData->_hasInputDrivenBlendshapes) {
            setAtBit16(flags, HAS_SCRIPTED_BLENDSHAPES);
        }
        // eye tracker state
        if (_headData->getProceduralAnimationFlag(HeadData::SaccadeProceduralEyeJointAnimation) &&
            !_headData->getSuppressProceduralAnimationFlag(HeadData::SaccadeProceduralEyeJointAnimation)) {
            setAtBit16(flags, HAS_PROCEDURAL_EYE_MOVEMENT);
        }
        // referential state
        if (!parentID.isNull()) {
            setAtBit16(flags, HAS_REFERENTIAL);
        }
        // audio face movement
        if (_headData->getProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation) &&
            !_headData->getSuppressProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation)) {
            setAtBit16(flags, AUDIO_ENABLED_FACE_MOVEMENT);
        }
        // procedural eye face movement
        if (_headData->getProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation) &&
            !_headData->getSuppressProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation)) {
            setAtBit16(flags, PROCEDURAL_EYE_FACE_MOVEMENT);
        }
        // procedural blink face movement
        if (_headData->getProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation) &&
            !_headData->getSuppressProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation)) {
            setAtBit16(flags, PROCEDURAL_BLINK_FACE_MOVEMENT);
        }
        // avatar collisions enabled
        if (_collideWithOtherAvatars) {
            setAtBit16(flags, COLLIDE_WITH_OTHER_AVATARS);
        }

        // Avatar has hero priority
        if (getHasPriority()) {
            setAtBit16(flags, HAS_HERO_PRIORITY);
        }

        data->flags = flags;
        destinationBuffer += sizeof(AvatarDataPacket::AdditionalFlags);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->additionalFlagsRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_PARENT_INFO, sizeof(AvatarDataPacket::ParentInfo)) {
        auto startSection = destinationBuffer;
        auto parentInfo = reinterpret_cast<AvatarDataPacket::ParentInfo*>(destinationBuffer);
        QByteArray referentialAsBytes = parentID.toRfc4122();
        memcpy(parentInfo->parentUUID, referentialAsBytes.data(), referentialAsBytes.size());
        parentInfo->parentJointIndex = getParentJointIndex();
        destinationBuffer += sizeof(AvatarDataPacket::ParentInfo);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->parentInfoRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_AVATAR_LOCAL_POSITION, AvatarDataPacket::AVATAR_LOCAL_POSITION_SIZE) {
        auto startSection = destinationBuffer;
        const auto localPosition = getLocalPosition();
        AVATAR_MEMCPY(localPosition);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->localPositionRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_HAND_CONTROLLERS, AvatarDataPacket::HAND_CONTROLLERS_SIZE) {
        auto startSection = destinationBuffer;

        Transform controllerLeftHandTransform = Transform(getControllerLeftHandMatrix());
        destinationBuffer += packOrientationQuatToSixBytes(destinationBuffer, controllerLeftHandTransform.getRotation());
        destinationBuffer += packFloatVec3ToSignedTwoByteFixed(destinationBuffer, controllerLeftHandTransform.getTranslation(), HAND_CONTROLLER_COMPRESSION_RADIX);

        Transform controllerRightHandTransform = Transform(getControllerRightHandMatrix());
        destinationBuffer += packOrientationQuatToSixBytes(destinationBuffer, controllerRightHandTransform.getRotation());
        destinationBuffer += packFloatVec3ToSignedTwoByteFixed(destinationBuffer, controllerRightHandTransform.getTranslation(), HAND_CONTROLLER_COMPRESSION_RADIX);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->handControllersRate.increment(numBytes);
        }
    }

    const auto& blendshapeCoefficients = _headData->getBlendshapeCoefficients();
    // If it is connected, pack up the data
    IF_AVATAR_SPACE(PACKET_HAS_FACE_TRACKER_INFO, sizeof(AvatarDataPacket::FaceTrackerInfo) + (size_t)blendshapeCoefficients.size() * sizeof(float)) {
        auto startSection = destinationBuffer;
        auto faceTrackerInfo = reinterpret_cast<AvatarDataPacket::FaceTrackerInfo*>(destinationBuffer);
        // note: we don't use the blink and average loudness, we just use the numBlendShapes and
        // compute the procedural info on the client side.
        faceTrackerInfo->leftEyeBlink = _headData->_leftEyeBlink;
        faceTrackerInfo->rightEyeBlink = _headData->_rightEyeBlink;
        faceTrackerInfo->averageLoudness = _headData->_averageLoudness;
        faceTrackerInfo->browAudioLift = _headData->_browAudioLift;
        faceTrackerInfo->numBlendshapeCoefficients = blendshapeCoefficients.size();
        destinationBuffer += sizeof(AvatarDataPacket::FaceTrackerInfo);

        memcpy(destinationBuffer, blendshapeCoefficients.data(), blendshapeCoefficients.size() * sizeof(float));
        destinationBuffer += blendshapeCoefficients.size() * sizeof(float);

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->faceTrackerRate.increment(numBytes);
        }
    }

    QVector<JointData> jointData;
    if (wantedFlags & (AvatarDataPacket::PACKET_HAS_JOINT_DATA | AvatarDataPacket::PACKET_HAS_JOINT_DEFAULT_POSE_FLAGS)) {
        QReadLocker readLock(&_jointDataLock);
        jointData = _jointData;
    }
    const int numJoints = jointData.size();
    assert(numJoints <= 255);
    const int jointBitVectorSize = calcBitVectorSize(numJoints);

    // include jointData if there is room for the most minimal section. i.e. no translations or rotations.
    IF_AVATAR_SPACE(PACKET_HAS_JOINT_DATA, AvatarDataPacket::minJointDataSize(numJoints)) {
        // Minimum space required for another rotation joint -
        // size of joint + following translation bit-vector + translation scale:
        const ptrdiff_t minSizeForJoint = sizeof(AvatarDataPacket::SixByteQuat) + jointBitVectorSize + sizeof(float);

        auto startSection = destinationBuffer;

        // compute maxTranslationDimension before we send any joint data.
        float maxTranslationDimension = 0.001f;
        for (int i = sendStatus.translationsSent; i < numJoints; ++i) {
            const JointData& data = jointData[i];
            if (!data.translationIsDefaultPose) {
                maxTranslationDimension = glm::max(fabsf(data.translation.x), maxTranslationDimension);
                maxTranslationDimension = glm::max(fabsf(data.translation.y), maxTranslationDimension);
                maxTranslationDimension = glm::max(fabsf(data.translation.z), maxTranslationDimension);
            }
        }

        // joint rotation data
        *destinationBuffer++ = (uint8_t)numJoints;

        unsigned char* validityPosition = destinationBuffer;
        memset(validityPosition, 0, jointBitVectorSize);

#ifdef WANT_DEBUG
        int rotationSentCount = 0;
        unsigned char* beforeRotations = destinationBuffer;
#endif

        destinationBuffer += jointBitVectorSize; // Move pointer past the validity bytes

        // sentJointDataOut and lastSentJointData might be the same vector
        if (sentJointDataOut) {
            sentJointDataOut->resize(numJoints); // Make sure the destination is resized before using it
        }
        const JointData *const joints = jointData.data();
        JointData *const sentJoints = sentJointDataOut ? sentJointDataOut->data() : nullptr;

        float minRotationDOT = (distanceAdjust && cullSmallChanges) ? getDistanceBasedMinRotationDOT(viewerPosition) : AVATAR_MIN_ROTATION_DOT;

        int i = sendStatus.rotationsSent;
        for (; i < numJoints; ++i) {
            const JointData& data = joints[i];
            const JointData& last = lastSentJointData[i];

            if (packetEnd - destinationBuffer >= minSizeForJoint) {
                if (!data.rotationIsDefaultPose) {
                    // The dot product for larger rotations is a lower number,
                    // so if the dot() is less than the value, then the rotation is a larger angle of rotation
                    if (sendAll || last.rotationIsDefaultPose || (!cullSmallChanges && last.rotation != data.rotation)
                        || (cullSmallChanges && fabsf(glm::dot(last.rotation, data.rotation)) < minRotationDOT)) {
                        validityPosition[i / BITS_IN_BYTE] |= 1 << (i % BITS_IN_BYTE);
#ifdef WANT_DEBUG
                        rotationSentCount++;
#endif
                        destinationBuffer += packOrientationQuatToSixBytes(destinationBuffer, data.rotation);

                        if (sentJoints) {
                            sentJoints[i].rotation = data.rotation;
                        }
                    }
                }
            } else {
                break;
            }

            if (sentJoints) {
                sentJoints[i].rotationIsDefaultPose = data.rotationIsDefaultPose;
            }

        }
        sendStatus.rotationsSent = i;

        // joint translation data
        validityPosition = destinationBuffer;

#ifdef WANT_DEBUG
        int translationSentCount = 0;
        unsigned char* beforeTranslations = destinationBuffer;
#endif

        memset(destinationBuffer, 0, jointBitVectorSize);
        destinationBuffer += jointBitVectorSize; // Move pointer past the validity bytes

        // write maxTranslationDimension
        AVATAR_MEMCPY(maxTranslationDimension);

        float minTranslation = (distanceAdjust && cullSmallChanges) ? getDistanceBasedMinTranslationDistance(viewerPosition) : AVATAR_MIN_TRANSLATION;

        i = sendStatus.translationsSent;
        for (; i < numJoints; ++i) {
            const JointData& data = joints[i];
            const JointData& last = lastSentJointData[i];

            // Note minSizeForJoint is conservative since there isn't a following bit-vector + scale.
            if (packetEnd - destinationBuffer >= minSizeForJoint) {
                if (!data.translationIsDefaultPose) {
                    if (sendAll || last.translationIsDefaultPose || (!cullSmallChanges && last.translation != data.translation)
                        || (cullSmallChanges && glm::distance(data.translation, lastSentJointData[i].translation) > minTranslation)) {
                        validityPosition[i / BITS_IN_BYTE] |= 1 << (i % BITS_IN_BYTE);
#ifdef WANT_DEBUG
                        translationSentCount++;
#endif
                        destinationBuffer += packFloatVec3ToSignedTwoByteFixed(destinationBuffer, data.translation / maxTranslationDimension,
                                                                               TRANSLATION_COMPRESSION_RADIX);

                        if (sentJoints) {
                            sentJoints[i].translation = data.translation;
                        }
                    }
                }
            } else {
                break;
            }

            if (sentJoints) {
                sentJoints[i].translationIsDefaultPose = data.translationIsDefaultPose;
            }

        }
        sendStatus.translationsSent = i;

        IF_AVATAR_SPACE(PACKET_HAS_GRAB_JOINTS, sizeof (AvatarDataPacket::FarGrabJoints)) {
            // the far-grab joints may range further than 3 meters, so we can't use packFloatVec3ToSignedTwoByteFixed etc
            auto startSection = destinationBuffer;

            glm::vec3 leftFarGrabPosition = extractTranslation(leftFarGrabMatrix);
            glm::quat leftFarGrabRotation = extractRotation(leftFarGrabMatrix);
            glm::vec3 rightFarGrabPosition = extractTranslation(rightFarGrabMatrix);
            glm::quat rightFarGrabRotation = extractRotation(rightFarGrabMatrix);
            glm::vec3 mouseFarGrabPosition = extractTranslation(mouseFarGrabMatrix);
            glm::quat mouseFarGrabRotation = extractRotation(mouseFarGrabMatrix);

            AvatarDataPacket::FarGrabJoints farGrabJoints = {
                { leftFarGrabPosition.x, leftFarGrabPosition.y, leftFarGrabPosition.z },
                { leftFarGrabRotation.w, leftFarGrabRotation.x, leftFarGrabRotation.y, leftFarGrabRotation.z },
                { rightFarGrabPosition.x, rightFarGrabPosition.y, rightFarGrabPosition.z },
                { rightFarGrabRotation.w, rightFarGrabRotation.x, rightFarGrabRotation.y, rightFarGrabRotation.z },
                { mouseFarGrabPosition.x, mouseFarGrabPosition.y, mouseFarGrabPosition.z },
                { mouseFarGrabRotation.w, mouseFarGrabRotation.x, mouseFarGrabRotation.y, mouseFarGrabRotation.z }
            };

            memcpy(destinationBuffer, &farGrabJoints, sizeof(farGrabJoints));
            destinationBuffer += sizeof(AvatarDataPacket::FarGrabJoints);
            int numBytes = destinationBuffer - startSection;

            if (outboundDataRateOut) {
                outboundDataRateOut->farGrabJointRate.increment(numBytes);
            }
        }

#ifdef WANT_DEBUG
        if (sendAll) {
            qCDebug(avatars) << "AvatarData::toByteArray" << cullSmallChanges << sendAll
                << "rotations:" << rotationSentCount << "translations:" << translationSentCount
                << "largest:" << maxTranslationDimension
                << "size:"
                << (beforeRotations - startPosition) << "+"
                << (beforeTranslations - beforeRotations) << "+"
                << (destinationBuffer - beforeTranslations) << "="
                << (destinationBuffer - startPosition);
        }
#endif

        if (sendStatus.rotationsSent != numJoints || sendStatus.translationsSent != numJoints) {
            extraReturnedFlags |= AvatarDataPacket::PACKET_HAS_JOINT_DATA;
        }

        int numBytes = destinationBuffer - startSection;
        if (outboundDataRateOut) {
            outboundDataRateOut->jointDataRate.increment(numBytes);
        }
    }

    IF_AVATAR_SPACE(PACKET_HAS_JOINT_DEFAULT_POSE_FLAGS, 1 + 2 * jointBitVectorSize) {
        auto startSection = destinationBuffer;

        // write numJoints
        *destinationBuffer++ = (uint8_t)numJoints;

        // write rotationIsDefaultPose bits
        destinationBuffer += writeBitVector(destinationBuffer, numJoints, [&](int i) {
            return jointData[i].rotationIsDefaultPose;
        });

        // write translationIsDefaultPose bits
        destinationBuffer += writeBitVector(destinationBuffer, numJoints, [&](int i) {
            return jointData[i].translationIsDefaultPose;
        });

        if (outboundDataRateOut) {
            size_t numBytes = destinationBuffer - startSection;
            outboundDataRateOut->jointDefaultPoseFlagsRate.increment(numBytes);
        }
    }

    memcpy(packetFlagsLocation, &includedFlags, sizeof(includedFlags));
    // Return dropped items.
    sendStatus.itemFlags = (wantedFlags & ~includedFlags) | extraReturnedFlags;

    int avatarDataSize = destinationBuffer - startPosition;

    if (avatarDataSize > (int)byteArraySize) {
        qCCritical(avatars) << "AvatarData::toByteArray buffer overflow"; // We've overflown into the heap
        ASSERT(false);
    }

    return avatarDataByteArray.left(avatarDataSize);

#undef AVATAR_MEMCPY
#undef IF_AVATAR_SPACE
}

// NOTE: This is never used in a "distanceAdjust" mode, so it's ok that it doesn't use a variable minimum rotation/translation
void AvatarData::doneEncoding(bool cullSmallChanges) {
    // The server has finished sending this version of the joint-data to other nodes.  Update _lastSentJointData.
    QReadLocker readLock(&_jointDataLock);
    _lastSentJointData.resize(_jointData.size());
    for (int i = 0; i < _jointData.size(); i ++) {
        const JointData& data = _jointData[ i ];
        if (_lastSentJointData[i].rotation != data.rotation) {
            if (!cullSmallChanges ||
                fabsf(glm::dot(data.rotation, _lastSentJointData[i].rotation)) <= AVATAR_MIN_ROTATION_DOT) {
                if (!data.rotationIsDefaultPose) {
                    _lastSentJointData[i].rotation = data.rotation;
                }
            }
        }
        if (_lastSentJointData[i].translation != data.translation) {
            if (!cullSmallChanges ||
                glm::distance(data.translation, _lastSentJointData[i].translation) > AVATAR_MIN_TRANSLATION) {
                if (!data.translationIsDefaultPose) {
                    _lastSentJointData[i].translation = data.translation;
                }
            }
        }
    }
}

bool AvatarData::shouldLogError(const quint64& now) {
#ifdef WANT_DEBUG
    if (now > 0) {
        return true;
    }
#endif

    if (now > _errorLogExpiry) {
        _errorLogExpiry = now + DEFAULT_FILTERED_LOG_EXPIRY;
        return true;
    }
    return false;
}


const unsigned char* unpackHandController(const unsigned char* sourceBuffer, ThreadSafeValueCache<glm::mat4>& matrixCache) {
    glm::quat orientation;
    glm::vec3 position;
    Transform transform;
    sourceBuffer += unpackOrientationQuatFromSixBytes(sourceBuffer, orientation);
    sourceBuffer += unpackFloatVec3FromSignedTwoByteFixed(sourceBuffer, position, HAND_CONTROLLER_COMPRESSION_RADIX);
    transform.setTranslation(position);
    transform.setRotation(orientation);
    matrixCache.set(transform.getMatrix());
    return sourceBuffer;
}


#define PACKET_READ_CHECK(ITEM_NAME, SIZE_TO_READ)                                        \
    if ((endPosition - sourceBuffer) < (int)SIZE_TO_READ) {                               \
        if (shouldLogError(now)) {                                                        \
            qCWarning(avatars) << "AvatarData packet too small, attempting to read " <<   \
                #ITEM_NAME << ", only " << (endPosition - sourceBuffer) <<                \
                " bytes left, " << getSessionUUID();                                      \
        }                                                                                 \
        return buffer.size();                                                             \
    }

// read data in packet starting at byte offset and return number of bytes parsed
int AvatarData::parseDataFromBuffer(const QByteArray& buffer) {
    // lazily allocate memory for HeadData in case we're not an Avatar instance
    lazyInitHeadData();

    AvatarDataPacket::HasFlags packetStateFlags;

    const unsigned char* startPosition = reinterpret_cast<const unsigned char*>(buffer.data());
    const unsigned char* endPosition = startPosition + buffer.size();
    const unsigned char* sourceBuffer = startPosition;

    // read the packet flags
    memcpy(&packetStateFlags, sourceBuffer, sizeof(packetStateFlags));
    sourceBuffer += sizeof(packetStateFlags);

    #define HAS_FLAG(B,F) ((B & F) == F)

    bool hasAvatarGlobalPosition  = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AVATAR_GLOBAL_POSITION);
    bool hasAvatarBoundingBox     = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AVATAR_BOUNDING_BOX);
    bool hasAvatarOrientation     = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AVATAR_ORIENTATION);
    bool hasAvatarScale           = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AVATAR_SCALE);
    bool hasLookAtPosition        = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_LOOK_AT_POSITION);
    bool hasAudioLoudness         = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AUDIO_LOUDNESS);
    bool hasSensorToWorldMatrix   = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_SENSOR_TO_WORLD_MATRIX);
    bool hasAdditionalFlags       = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_ADDITIONAL_FLAGS);
    bool hasParentInfo            = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_PARENT_INFO);
    bool hasAvatarLocalPosition   = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_AVATAR_LOCAL_POSITION);
    bool hasHandControllers       = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_HAND_CONTROLLERS);
    bool hasFaceTrackerInfo       = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_FACE_TRACKER_INFO);
    bool hasJointData             = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_JOINT_DATA);
    bool hasJointDefaultPoseFlags = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_JOINT_DEFAULT_POSE_FLAGS);
    bool hasGrabJoints            = HAS_FLAG(packetStateFlags, AvatarDataPacket::PACKET_HAS_GRAB_JOINTS);

    quint64 now = usecTimestampNow();

    if (hasAvatarGlobalPosition) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AvatarGlobalPosition, sizeof(AvatarDataPacket::AvatarGlobalPosition));
        auto data = reinterpret_cast<const AvatarDataPacket::AvatarGlobalPosition*>(sourceBuffer);

        glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);

        if (_replicaIndex > 0) {
            const float SPACE_BETWEEN_AVATARS = 2.0f;
            const int AVATARS_PER_ROW = 3;
            int row = _replicaIndex % AVATARS_PER_ROW;
            int col = floor(_replicaIndex / AVATARS_PER_ROW);
            offset = glm::vec3(row * SPACE_BETWEEN_AVATARS, 0.0f, col * SPACE_BETWEEN_AVATARS);
        }

        _serverPosition = glm::vec3(data->globalPosition[0], data->globalPosition[1], data->globalPosition[2]) + offset;
        if (_isClientAvatar) {
            auto oneStepDistance = glm::length(_globalPosition - _serverPosition);
            if (oneStepDistance <= AVATAR_TRANSIT_MIN_TRIGGER_DISTANCE || oneStepDistance >= AVATAR_TRANSIT_MAX_TRIGGER_DISTANCE) {
                _globalPosition = _serverPosition;
                // if we don't have a parent, make sure to also set our local position
                if (!hasParent()) {
                    setLocalPosition(_serverPosition);
                }
            }
            if (_globalPosition != _serverPosition) {
                _globalPositionChanged = now;
            }
        } else {
            if (_globalPosition != _serverPosition) {
                _globalPosition = _serverPosition;
                _globalPositionChanged = now;
            }
            if (!hasParent()) {
                setLocalPosition(_serverPosition);
            }
        }
        sourceBuffer += sizeof(AvatarDataPacket::AvatarGlobalPosition);
        int numBytesRead = sourceBuffer - startSection;
        _globalPositionRate.increment(numBytesRead);
        _globalPositionUpdateRate.increment();
    }

    if (hasAvatarBoundingBox) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AvatarBoundingBox, sizeof(AvatarDataPacket::AvatarBoundingBox));
        auto data = reinterpret_cast<const AvatarDataPacket::AvatarBoundingBox*>(sourceBuffer);
        auto newDimensions = glm::vec3(data->avatarDimensions[0], data->avatarDimensions[1], data->avatarDimensions[2]);
        auto newOffset = glm::vec3(data->boundOriginOffset[0], data->boundOriginOffset[1], data->boundOriginOffset[2]);


        if (_globalBoundingBoxDimensions != newDimensions) {
            _globalBoundingBoxDimensions = newDimensions;
            _avatarBoundingBoxChanged = now;
        }
        if (_globalBoundingBoxOffset != newOffset) {
            _globalBoundingBoxOffset = newOffset;
            _avatarBoundingBoxChanged = now;
        }

        _defaultBubbleBox = computeBubbleBox();

        sourceBuffer += sizeof(AvatarDataPacket::AvatarBoundingBox);
        int numBytesRead = sourceBuffer - startSection;
        _avatarBoundingBoxRate.increment(numBytesRead);
        _avatarBoundingBoxUpdateRate.increment();
    }

    if (hasAvatarOrientation) {
        auto startSection = sourceBuffer;
        PACKET_READ_CHECK(AvatarOrientation, sizeof(AvatarDataPacket::AvatarOrientation));
        glm::quat newOrientation;
        sourceBuffer += unpackOrientationQuatFromSixBytes(sourceBuffer, newOrientation);
        glm::quat currentOrientation = getLocalOrientation();

        if (currentOrientation != newOrientation) {
            _hasNewJointData = true;
            setLocalOrientation(newOrientation);
        }
        int numBytesRead = sourceBuffer - startSection;
        _avatarOrientationRate.increment(numBytesRead);
        _avatarOrientationUpdateRate.increment();
    }

    if (hasAvatarScale) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AvatarScale, sizeof(AvatarDataPacket::AvatarScale));
        auto data = reinterpret_cast<const AvatarDataPacket::AvatarScale*>(sourceBuffer);
        float scale;
        unpackFloatRatioFromTwoByte((uint8_t*)&data->scale, scale);
        if (isNaN(scale)) {
            if (shouldLogError(now)) {
                qCWarning(avatars) << "Discard AvatarData packet: scale NaN, uuid " << getSessionUUID();
            }
            return buffer.size();
        }
        setTargetScale(scale);
        sourceBuffer += sizeof(AvatarDataPacket::AvatarScale);
        int numBytesRead = sourceBuffer - startSection;
        _avatarScaleRate.increment(numBytesRead);
        _avatarScaleUpdateRate.increment();
    }

    if (hasLookAtPosition) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(LookAtPosition, sizeof(AvatarDataPacket::LookAtPosition));
        auto data = reinterpret_cast<const AvatarDataPacket::LookAtPosition*>(sourceBuffer);
        glm::vec3 lookAt = glm::vec3(data->lookAtPosition[0], data->lookAtPosition[1], data->lookAtPosition[2]);
        if (isNaN(lookAt)) {
            if (shouldLogError(now)) {
                qCWarning(avatars) << "Discard AvatarData packet: lookAtPosition is NaN, uuid " << getSessionUUID();
            }
            return buffer.size();
        }
        _headData->setLookAtPosition(lookAt);
        sourceBuffer += sizeof(AvatarDataPacket::LookAtPosition);
        int numBytesRead = sourceBuffer - startSection;
        _lookAtPositionRate.increment(numBytesRead);
        _lookAtPositionUpdateRate.increment();
    }

    if (hasAudioLoudness) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AudioLoudness, sizeof(AvatarDataPacket::AudioLoudness));
        auto data = reinterpret_cast<const AvatarDataPacket::AudioLoudness*>(sourceBuffer);
        float audioLoudness;
        audioLoudness = unpackFloatGainFromByte(data->audioLoudness) * AUDIO_LOUDNESS_SCALE;
        sourceBuffer += sizeof(AvatarDataPacket::AudioLoudness);

        if (isNaN(audioLoudness)) {
            if (shouldLogError(now)) {
                qCWarning(avatars) << "Discard AvatarData packet: audioLoudness is NaN, uuid " << getSessionUUID();
            }
            return buffer.size();
        }
        setAudioLoudness(audioLoudness);
        int numBytesRead = sourceBuffer - startSection;
        _audioLoudnessRate.increment(numBytesRead);
        _audioLoudnessUpdateRate.increment();
    }

    if (hasSensorToWorldMatrix) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(SensorToWorldMatrix, sizeof(AvatarDataPacket::SensorToWorldMatrix));
        auto data = reinterpret_cast<const AvatarDataPacket::SensorToWorldMatrix*>(sourceBuffer);
        glm::quat sensorToWorldQuat;
        unpackOrientationQuatFromSixBytes(data->sensorToWorldQuat, sensorToWorldQuat);
        float sensorToWorldScale;
        // Grab a local copy of sensorToWorldScale to be able to use the unpack function with a pointer on it,
        // a direct pointer on the struct attribute triggers warnings because of potential misalignement.
        auto srcSensorToWorldScale = data->sensorToWorldScale;
        unpackFloatScalarFromSignedTwoByteFixed((int16_t*)&srcSensorToWorldScale, &sensorToWorldScale, SENSOR_TO_WORLD_SCALE_RADIX);
        glm::vec3 sensorToWorldTrans(data->sensorToWorldTrans[0], data->sensorToWorldTrans[1], data->sensorToWorldTrans[2]);
        glm::mat4 sensorToWorldMatrix = createMatFromScaleQuatAndPos(glm::vec3(sensorToWorldScale), sensorToWorldQuat, sensorToWorldTrans);
        if (_sensorToWorldMatrixCache.get() != sensorToWorldMatrix) {
            _sensorToWorldMatrixCache.set(sensorToWorldMatrix);
            _sensorToWorldMatrixChanged = now;
        }
        sourceBuffer += sizeof(AvatarDataPacket::SensorToWorldMatrix);
        int numBytesRead = sourceBuffer - startSection;
        _sensorToWorldRate.increment(numBytesRead);
        _sensorToWorldUpdateRate.increment();
    }

    if (hasAdditionalFlags) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AdditionalFlags, sizeof(AvatarDataPacket::AdditionalFlags));
        auto data = reinterpret_cast<const AvatarDataPacket::AdditionalFlags*>(sourceBuffer);
        uint16_t bitItems = data->flags;

        // key state, stored as a semi-nibble in the bitItems
        auto newKeyState = (KeyState)getSemiNibbleAt(bitItems, KEY_STATE_START_BIT);

        // hand state, stored as a semi-nibble plus a bit in the bitItems
        // we store the hand state as well as other items in a shared bitset. The hand state is an octal, but is split
        // into two sections to maintain backward compatibility. The bits are ordered as such (0-7 left to right).
        // AA 6/1/18 added three more flags bits 8,9, and 10 for procedural audio, blink, and eye saccade enabled
        //     +---+-----+-----+--+--+--+--+--+----+
        //     |x,x|H0,H1|x,x,x|H2|Au|Bl|Ey|He|xxxx|
        //     +---+-----+-----+--+--+--+--+--+----+
        // Hand state - H0,H1,H2 is found in the 3rd, 4th, and 8th bits
        // Hero-avatar status (He) - 12th bit
        auto newHandState = getSemiNibbleAt(bitItems, HAND_STATE_START_BIT)
            + (oneAtBit16(bitItems, HAND_STATE_FINGER_POINTING_BIT) ? IS_FINGER_POINTING_FLAG : 0);

        auto newHasScriptedBlendshapes = oneAtBit16(bitItems, HAS_SCRIPTED_BLENDSHAPES);
        auto newHasProceduralEyeMovement = oneAtBit16(bitItems, HAS_PROCEDURAL_EYE_MOVEMENT);
        auto newHasAudioEnabledFaceMovement = oneAtBit16(bitItems, AUDIO_ENABLED_FACE_MOVEMENT);
        auto newHasProceduralEyeFaceMovement = oneAtBit16(bitItems, PROCEDURAL_EYE_FACE_MOVEMENT);
        auto newHasProceduralBlinkFaceMovement = oneAtBit16(bitItems, PROCEDURAL_BLINK_FACE_MOVEMENT);

        auto newCollideWithOtherAvatars = oneAtBit16(bitItems, COLLIDE_WITH_OTHER_AVATARS);
        auto newHasPriority = oneAtBit16(bitItems, HAS_HERO_PRIORITY);        

        bool keyStateChanged = (_keyState != newKeyState);
        bool handStateChanged = (_handState != newHandState);
        bool faceStateChanged = (_headData->getHasScriptedBlendshapes() != newHasScriptedBlendshapes);

        bool eyeStateChanged = (_headData->getProceduralAnimationFlag(HeadData::SaccadeProceduralEyeJointAnimation) != newHasProceduralEyeMovement);
        bool audioEnableFaceMovementChanged = (_headData->getProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation) != newHasAudioEnabledFaceMovement);
        bool proceduralEyeFaceMovementChanged = (_headData->getProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation) != newHasProceduralEyeFaceMovement);
        bool proceduralBlinkFaceMovementChanged = (_headData->getProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation) != newHasProceduralBlinkFaceMovement);
        bool collideWithOtherAvatarsChanged = (_collideWithOtherAvatars != newCollideWithOtherAvatars);
        bool hasPriorityChanged = (getHasPriority() != newHasPriority);
        bool somethingChanged = keyStateChanged || handStateChanged || faceStateChanged || eyeStateChanged || audioEnableFaceMovementChanged || 
                                proceduralEyeFaceMovementChanged ||
                                proceduralBlinkFaceMovementChanged || collideWithOtherAvatarsChanged || hasPriorityChanged;

        _keyState = newKeyState;
        _handState = newHandState;
        if (!newHasScriptedBlendshapes && getHasScriptedBlendshapes()) {
            // if scripted blendshapes have just been turned off, slam blendshapes back to zero.
            _headData->clearBlendshapeCoefficients();
        }
        _headData->setHasScriptedBlendshapes(newHasScriptedBlendshapes);
        _headData->setProceduralAnimationFlag(HeadData::SaccadeProceduralEyeJointAnimation, newHasProceduralEyeMovement);
        _headData->setProceduralAnimationFlag(HeadData::AudioProceduralBlendshapeAnimation, newHasAudioEnabledFaceMovement);
        _headData->setProceduralAnimationFlag(HeadData::LidAdjustmentProceduralBlendshapeAnimation, newHasProceduralEyeFaceMovement);
        _headData->setProceduralAnimationFlag(HeadData::BlinkProceduralBlendshapeAnimation, newHasProceduralBlinkFaceMovement);
        _collideWithOtherAvatars = newCollideWithOtherAvatars;
        setHasPriorityWithoutTimestampReset(newHasPriority);

        sourceBuffer += sizeof(AvatarDataPacket::AdditionalFlags);

        if (collideWithOtherAvatarsChanged) {
            setCollisionWithOtherAvatarsFlags();
        }
        if (somethingChanged) {
            _additionalFlagsChanged = now;
        }
        int numBytesRead = sourceBuffer - startSection;
        _additionalFlagsRate.increment(numBytesRead);
        _additionalFlagsUpdateRate.increment();
    }

    if (hasParentInfo) {
        auto startSection = sourceBuffer;
        PACKET_READ_CHECK(ParentInfo, sizeof(AvatarDataPacket::ParentInfo));
        auto parentInfo = reinterpret_cast<const AvatarDataPacket::ParentInfo*>(sourceBuffer);
        sourceBuffer += sizeof(AvatarDataPacket::ParentInfo);

        QByteArray byteArray((const char*)parentInfo->parentUUID, NUM_BYTES_RFC4122_UUID);

        auto newParentID = QUuid::fromRfc4122(byteArray);

        if ((getParentID() != newParentID) || (getParentJointIndex() != parentInfo->parentJointIndex)) {
            SpatiallyNestable::setParentID(newParentID);
            SpatiallyNestable::setParentJointIndex(parentInfo->parentJointIndex);
            _parentChanged = now;
        }

        int numBytesRead = sourceBuffer - startSection;
        _parentInfoRate.increment(numBytesRead);
        _parentInfoUpdateRate.increment();
    }

    if (hasAvatarLocalPosition) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(AvatarLocalPosition, sizeof(AvatarDataPacket::AvatarLocalPosition));
        auto data = reinterpret_cast<const AvatarDataPacket::AvatarLocalPosition*>(sourceBuffer);
        glm::vec3 position = glm::vec3(data->localPosition[0], data->localPosition[1], data->localPosition[2]);
        if (isNaN(position)) {
            if (shouldLogError(now)) {
                qCWarning(avatars) << "Discard AvatarData packet: position NaN, uuid " << getSessionUUID();
            }
            return buffer.size();
        }
        if (hasParent()) {
            setLocalPosition(position);
        } else {
            qCWarning(avatars) << "received localPosition for avatar with no parent";
        }
        sourceBuffer += sizeof(AvatarDataPacket::AvatarLocalPosition);
        int numBytesRead = sourceBuffer - startSection;
        _localPositionRate.increment(numBytesRead);
        _localPositionUpdateRate.increment();
    }

    if (hasHandControllers) {
        auto startSection = sourceBuffer;

        sourceBuffer = unpackHandController(sourceBuffer, _controllerLeftHandMatrixCache);
        sourceBuffer = unpackHandController(sourceBuffer, _controllerRightHandMatrixCache);

        int numBytesRead = sourceBuffer - startSection;
        _handControllersRate.increment(numBytesRead);
        _handControllersUpdateRate.increment();
    } else {
        _controllerLeftHandMatrixCache.invalidate();
        _controllerRightHandMatrixCache.invalidate();
    }

    if (hasFaceTrackerInfo) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(FaceTrackerInfo, sizeof(AvatarDataPacket::FaceTrackerInfo));
        auto faceTrackerInfo = reinterpret_cast<const AvatarDataPacket::FaceTrackerInfo*>(sourceBuffer);
        int numCoefficients = faceTrackerInfo->numBlendshapeCoefficients;
        const int coefficientsSize = sizeof(float) * numCoefficients;
        sourceBuffer += sizeof(AvatarDataPacket::FaceTrackerInfo);

        PACKET_READ_CHECK(FaceTrackerCoefficients, coefficientsSize);
        _headData->_blendshapeCoefficients.resize(std::min(numCoefficients, (int)Blendshapes::BlendshapeCount));  // make sure there's room for the copy!
        //only copy the blendshapes to headData, not the procedural face info
        memcpy(_headData->_blendshapeCoefficients.data(), sourceBuffer, coefficientsSize);
        sourceBuffer += coefficientsSize;

        int numBytesRead = sourceBuffer - startSection;
        _faceTrackerRate.increment(numBytesRead);
        _faceTrackerUpdateRate.increment();
    }

    if (hasJointData) {
        auto startSection = sourceBuffer;

        PACKET_READ_CHECK(NumJoints, sizeof(uint8_t));
        int numJoints = *sourceBuffer++;
        const int bytesOfValidity = (int)ceil((float)numJoints / (float)BITS_IN_BYTE);
        PACKET_READ_CHECK(JointRotationValidityBits, bytesOfValidity);

        int numValidJointRotations = 0;
        QVector<bool> validRotations;
        validRotations.resize(numJoints);
        { // rotation validity bits
            unsigned char validity = 0;
            int validityBit = 0;
            for (int i = 0; i < numJoints; i++) {
                if (validityBit == 0) {
                    validity = *sourceBuffer++;
                }
                bool valid = (bool)(validity & (1 << validityBit));
                if (valid) {
                    ++numValidJointRotations;
                }
                validRotations[i] = valid;
                validityBit = (validityBit + 1) % BITS_IN_BYTE;
            }
        }

        // each joint rotation is stored in 6 bytes.
        QWriteLocker writeLock(&_jointDataLock);
        _jointData.resize(numJoints);

        const int COMPRESSED_QUATERNION_SIZE = 6;
        PACKET_READ_CHECK(JointRotations, numValidJointRotations * COMPRESSED_QUATERNION_SIZE);
        for (int i = 0; i < numJoints; i++) {
            JointData& data = _jointData[i];
            if (validRotations[i]) {
                sourceBuffer += unpackOrientationQuatFromSixBytes(sourceBuffer, data.rotation);
                _hasNewJointData = true;
                data.rotationIsDefaultPose = false;
            }
        }

        PACKET_READ_CHECK(JointTranslationValidityBits, bytesOfValidity);

        // get translation validity bits -- these indicate which translations were packed
        int numValidJointTranslations = 0;
        QVector<bool> validTranslations;
        validTranslations.resize(numJoints);
        { // translation validity bits
            unsigned char validity = 0;
            int validityBit = 0;
            for (int i = 0; i < numJoints; i++) {
                if (validityBit == 0) {
                    validity = *sourceBuffer++;
                }
                bool valid = (bool)(validity & (1 << validityBit));
                if (valid) {
                    ++numValidJointTranslations;
                }
                validTranslations[i] = valid;
                validityBit = (validityBit + 1) % BITS_IN_BYTE;
            }
        } // 1 + bytesOfValidity bytes

        // read maxTranslationDimension
        float maxTranslationDimension;
        PACKET_READ_CHECK(JointMaxTranslationDimension, sizeof(float));
        memcpy(&maxTranslationDimension, sourceBuffer, sizeof(float));
        sourceBuffer += sizeof(float);

        // each joint translation component is stored in 6 bytes.
        const int COMPRESSED_TRANSLATION_SIZE = 6;
        PACKET_READ_CHECK(JointTranslation, numValidJointTranslations * COMPRESSED_TRANSLATION_SIZE);

        for (int i = 0; i < numJoints; i++) {
            JointData& data = _jointData[i];
            if (validTranslations[i]) {
                sourceBuffer += unpackFloatVec3FromSignedTwoByteFixed(sourceBuffer, data.translation, TRANSLATION_COMPRESSION_RADIX);
                data.translation *= maxTranslationDimension;
                _hasNewJointData = true;
                data.translationIsDefaultPose = false;
            }
        }

#ifdef WANT_DEBUG
        if (numValidJointRotations > 15) {
            qCDebug(avatars) << "RECEIVING -- rotations:" << numValidJointRotations
                << "translations:" << numValidJointTranslations
                << "size:" << (int)(sourceBuffer - startPosition);
        }
#endif
        int numBytesRead = sourceBuffer - startSection;
        _jointDataRate.increment(numBytesRead);
        _jointDataUpdateRate.increment();

        if (hasGrabJoints) {
            auto startSection = sourceBuffer;

            PACKET_READ_CHECK(FarGrabJoints, sizeof(AvatarDataPacket::FarGrabJoints));

            AvatarDataPacket::FarGrabJoints farGrabJoints;
            memcpy(&farGrabJoints, sourceBuffer, sizeof(farGrabJoints)); // to avoid misaligned floats

            glm::vec3 leftFarGrabPosition = glm::vec3(farGrabJoints.leftFarGrabPosition[0],
                                                      farGrabJoints.leftFarGrabPosition[1],
                                                      farGrabJoints.leftFarGrabPosition[2]);
            glm::quat leftFarGrabRotation = glm::quat(farGrabJoints.leftFarGrabRotation[0],
                                                      farGrabJoints.leftFarGrabRotation[1],
                                                      farGrabJoints.leftFarGrabRotation[2],
                                                      farGrabJoints.leftFarGrabRotation[3]);
            glm::vec3 rightFarGrabPosition = glm::vec3(farGrabJoints.rightFarGrabPosition[0],
                                                       farGrabJoints.rightFarGrabPosition[1],
                                                       farGrabJoints.rightFarGrabPosition[2]);
            glm::quat rightFarGrabRotation = glm::quat(farGrabJoints.rightFarGrabRotation[0],
                                                       farGrabJoints.rightFarGrabRotation[1],
                                                       farGrabJoints.rightFarGrabRotation[2],
                                                       farGrabJoints.rightFarGrabRotation[3]);
            glm::vec3 mouseFarGrabPosition = glm::vec3(farGrabJoints.mouseFarGrabPosition[0],
                                                       farGrabJoints.mouseFarGrabPosition[1],
                                                       farGrabJoints.mouseFarGrabPosition[2]);
            glm::quat mouseFarGrabRotation = glm::quat(farGrabJoints.mouseFarGrabRotation[0],
                                                       farGrabJoints.mouseFarGrabRotation[1],
                                                       farGrabJoints.mouseFarGrabRotation[2],
                                                       farGrabJoints.mouseFarGrabRotation[3]);

            _farGrabLeftMatrixCache.set(createMatFromQuatAndPos(leftFarGrabRotation, leftFarGrabPosition));
            _farGrabRightMatrixCache.set(createMatFromQuatAndPos(rightFarGrabRotation, rightFarGrabPosition));
            _farGrabMouseMatrixCache.set(createMatFromQuatAndPos(mouseFarGrabRotation, mouseFarGrabPosition));

            sourceBuffer += sizeof(AvatarDataPacket::FarGrabJoints);
            int numBytesRead = sourceBuffer - startSection;
            _farGrabJointRate.increment(numBytesRead);
            _farGrabJointUpdateRate.increment();
        }
    }

    if (hasJointDefaultPoseFlags) {
        auto startSection = sourceBuffer;

        QWriteLocker writeLock(&_jointDataLock);

        PACKET_READ_CHECK(JointDefaultPoseFlagsNumJoints, sizeof(uint8_t));
        int numJoints = (int)*sourceBuffer++;

        _jointData.resize(numJoints);

        size_t bitVectorSize = calcBitVectorSize(numJoints);
        PACKET_READ_CHECK(JointDefaultPoseFlagsRotationFlags, bitVectorSize);
        sourceBuffer += readBitVector(sourceBuffer, numJoints, [&](int i, bool value) {
            _jointData[i].rotationIsDefaultPose = value;
        });

        PACKET_READ_CHECK(JointDefaultPoseFlagsTranslationFlags, bitVectorSize);
        sourceBuffer += readBitVector(sourceBuffer, numJoints, [&](int i, bool value) {
            _jointData[i].translationIsDefaultPose = value;
        });

        int numBytesRead = sourceBuffer - startSection;
        _jointDefaultPoseFlagsRate.increment(numBytesRead);
        _jointDefaultPoseFlagsUpdateRate.increment();
    }

    int numBytesRead = sourceBuffer - startPosition;
    _averageBytesReceived.updateAverage(numBytesRead);

    _parseBufferRate.increment(numBytesRead);
    _parseBufferUpdateRate.increment();

    return numBytesRead;
}

/*@jsdoc
 * <p>The avatar mixer data comprises different types of data, with the data rates of each being tracked in kbps.</p>
 *
 * <table>
 *   <thead>
 *     <tr><th>Rate Name</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>"globalPosition"</code></td><td>Incoming global position.</td></tr>
 *     <tr><td><code>"localPosition"</code></td><td>Incoming local position.</td></tr>
 *     <tr><td><code>"handControllers"</code></td><td>Incoming hand controllers.</td></tr>
 *     <tr><td><code>"avatarBoundingBox"</code></td><td>Incoming avatar bounding box.</td></tr>
 *     <tr><td><code>"avatarOrientation"</code></td><td>Incoming avatar orientation.</td></tr>
 *     <tr><td><code>"avatarScale"</code></td><td>Incoming avatar scale.</td></tr>
 *     <tr><td><code>"lookAtPosition"</code></td><td>Incoming look-at position.</td></tr>
 *     <tr><td><code>"audioLoudness"</code></td><td>Incoming audio loudness.</td></tr>
 *     <tr><td><code>"sensorToWorkMatrix"</code></td><td>Incoming sensor-to-world matrix.</td></tr>
 *     <tr><td><code>"additionalFlags"</code></td><td>Incoming additional avatar flags.</td></tr>
 *     <tr><td><code>"parentInfo"</code></td><td>Incoming parent information.</td></tr>
 *     <tr><td><code>"faceTracker"</code></td><td>Incoming face tracker data.</td></tr>
 *     <tr><td><code>"jointData"</code></td><td>Incoming joint data.</td></tr>
 *     <tr><td><code>"jointDefaultPoseFlagsRate"</code></td><td>Incoming joint default pose flags.</td></tr>
 *     <tr><td><code>"farGrabJointRate"</code></td><td>Incoming far grab joint.</td></tr>
 *     <tr><td><code>"globalPositionOutbound"</code></td><td>Outgoing global position.</td></tr>
 *     <tr><td><code>"localPositionOutbound"</code></td><td>Outgoing local position.</td></tr>
 *     <tr><td><code>"avatarBoundingBoxOutbound"</code></td><td>Outgoing avatar bounding box.</td></tr>
 *     <tr><td><code>"avatarOrientationOutbound"</code></td><td>Outgoing avatar orientation.</td></tr>
 *     <tr><td><code>"avatarScaleOutbound"</code></td><td>Outgoing avatar scale.</td></tr>
 *     <tr><td><code>"lookAtPositionOutbound"</code></td><td>Outgoing look-at position.</td></tr>
 *     <tr><td><code>"audioLoudnessOutbound"</code></td><td>Outgoing audio loudness.</td></tr>
 *     <tr><td><code>"sensorToWorkMatrixOutbound"</code></td><td>Outgoing sensor-to-world matrix.</td></tr>
 *     <tr><td><code>"additionalFlagsOutbound"</code></td><td>Outgoing additional avatar flags.</td></tr>
 *     <tr><td><code>"parentInfoOutbound"</code></td><td>Outgoing parent information.</td></tr>
 *     <tr><td><code>"faceTrackerOutbound"</code></td><td>Outgoing face tracker data.</td></tr>
 *     <tr><td><code>"jointDataOutbound"</code></td><td>Outgoing joint data.</td></tr>
 *     <tr><td><code>"jointDefaultPoseFlagsOutbound"</code></td><td>Outgoing joint default pose flags.</td></tr>
 *     <tr><td><code>""</code></td><td>When no rate name is specified, the total incoming data rate is provided.</td></tr>
 *   </tbody>
 * </table>
 *
 * @typedef {string} AvatarDataRate
 */
float AvatarData::getDataRate(const QString& rateName) const {
    if (rateName == "") {
        return _parseBufferRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "globalPosition") {
        return _globalPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "localPosition") {
        return _localPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "handControllers") {
        return _handControllersRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarBoundingBox") {
        return _avatarBoundingBoxRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarOrientation") {
        return _avatarOrientationRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarScale") {
        return _avatarScaleRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "lookAtPosition") {
        return _lookAtPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "audioLoudness") {
        return _audioLoudnessRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "sensorToWorkMatrix") {
        return _sensorToWorldRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "additionalFlags") {
        return _additionalFlagsRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "parentInfo") {
        return _parentInfoRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "faceTracker") {
        return _faceTrackerRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "jointData") {
        return _jointDataRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "jointDefaultPoseFlagsRate") {
        return _jointDefaultPoseFlagsRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "farGrabJointRate") {
        return _farGrabJointRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "globalPositionOutbound") {
        return _outboundDataRate.globalPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "localPositionOutbound") {
        return _outboundDataRate.localPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarBoundingBoxOutbound") {
        return _outboundDataRate.avatarBoundingBoxRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarOrientationOutbound") {
        return _outboundDataRate.avatarOrientationRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "avatarScaleOutbound") {
        return _outboundDataRate.avatarScaleRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "lookAtPositionOutbound") {
        return _outboundDataRate.lookAtPositionRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "audioLoudnessOutbound") {
        return _outboundDataRate.audioLoudnessRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "sensorToWorkMatrixOutbound") {
        return _outboundDataRate.sensorToWorldRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "additionalFlagsOutbound") {
        return _outboundDataRate.additionalFlagsRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "parentInfoOutbound") {
        return _outboundDataRate.parentInfoRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "faceTrackerOutbound") {
        return _outboundDataRate.faceTrackerRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "jointDataOutbound") {
        return _outboundDataRate.jointDataRate.rate() / BYTES_PER_KILOBIT;
    } else if (rateName == "jointDefaultPoseFlagsOutbound") {
        return _outboundDataRate.jointDefaultPoseFlagsRate.rate() / BYTES_PER_KILOBIT;
    }
    return 0.0f;
}

/*@jsdoc
 * <p>The avatar mixer data comprises different types of data updated at different rates, in Hz.</p>
 *
 * <table>
 *   <thead>
 *     <tr><th>Rate Name</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>"globalPosition"</code></td><td>Global position.</td></tr>
 *     <tr><td><code>"localPosition"</code></td><td>Local position.</td></tr>
 *     <tr><td><code>"handControllers"</code></td><td>Hand controller positions and orientations.</td></tr>
 *     <tr><td><code>"avatarBoundingBox"</code></td><td>Avatar bounding box.</td></tr>
 *     <tr><td><code>"avatarOrientation"</code></td><td>Avatar orientation.</td></tr>
 *     <tr><td><code>"avatarScale"</code></td><td>Avatar scale.</td></tr>
 *     <tr><td><code>"lookAtPosition"</code></td><td>Look-at position.</td></tr>
 *     <tr><td><code>"audioLoudness"</code></td><td>Audio loudness.</td></tr>
 *     <tr><td><code>"sensorToWorkMatrix"</code></td><td>Sensor-to-world matrix.</td></tr>
 *     <tr><td><code>"additionalFlags"</code></td><td>Additional avatar flags.</td></tr>
 *     <tr><td><code>"parentInfo"</code></td><td>Parent information.</td></tr>
 *     <tr><td><code>"faceTracker"</code></td><td>Face tracker data.</td></tr>
 *     <tr><td><code>"jointData"</code></td><td>Joint data.</td></tr>
 *     <tr><td><code>"farGrabJointData"</code></td><td>Far grab joint data.</td></tr>
 *     <tr><td><code>""</code></td><td>When no rate name is specified, the overall update rate is provided.</td></tr>
 *   </tbody>
 * </table>
 *
 * @typedef {string} AvatarUpdateRate
 */
float AvatarData::getUpdateRate(const QString& rateName) const {
    if (rateName == "") {
        return _parseBufferUpdateRate.rate();
    } else if (rateName == "globalPosition") {
        return _globalPositionUpdateRate.rate();
    } else if (rateName == "localPosition") {
        return _localPositionUpdateRate.rate();
    } else if (rateName == "handControllers") {
        return _handControllersUpdateRate.rate();
    } else if (rateName == "avatarBoundingBox") {
        return _avatarBoundingBoxUpdateRate.rate();
    } else if (rateName == "avatarOrientation") {
        return _avatarOrientationUpdateRate.rate();
    } else if (rateName == "avatarScale") {
        return _avatarScaleUpdateRate.rate();
    } else if (rateName == "lookAtPosition") {
        return _lookAtPositionUpdateRate.rate();
    } else if (rateName == "audioLoudness") {
        return _audioLoudnessUpdateRate.rate();
    } else if (rateName == "sensorToWorkMatrix") {
        return _sensorToWorldUpdateRate.rate();
    } else if (rateName == "additionalFlags") {
        return _additionalFlagsUpdateRate.rate();
    } else if (rateName == "parentInfo") {
        return _parentInfoUpdateRate.rate();
    } else if (rateName == "faceTracker") {
        return _faceTrackerUpdateRate.rate();
    } else if (rateName == "jointData") {
        return _jointDataUpdateRate.rate();
    } else if (rateName == "farGrabJointData") {
        return _farGrabJointUpdateRate.rate();
    }
    return 0.0f;
}

int AvatarData::getAverageBytesReceivedPerSecond() const {
    return lrint(_averageBytesReceived.getAverageSampleValuePerSecond());
}

int AvatarData::getReceiveRate() const {
    return lrint(1.0f / _averageBytesReceived.getEventDeltaAverage());
}

std::shared_ptr<Transform> AvatarData::getRecordingBasis() const {
    return _recordingBasis;
}

void AvatarData::setRawJointData(QVector<JointData> data) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setRawJointData", Q_ARG(QVector<JointData>, data));
        return;
    }
    QWriteLocker writeLock(&_jointDataLock);
    _jointData = data;
}

void AvatarData::setJointData(int index, const glm::quat& rotation, const glm::vec3& translation) {
    if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
        return;
    }
    QWriteLocker writeLock(&_jointDataLock);
    if (_jointData.size() <= index) {
        _jointData.resize(index + 1);
    }
    JointData& data = _jointData[index];
    data.rotation = rotation;
    data.rotationIsDefaultPose = false;
    data.translation = translation;
    data.translationIsDefaultPose = false;
}

QVector<JointData> AvatarData::getJointData() const {
    QVector<JointData> jointData;
    QReadLocker readLock(&_jointDataLock);
    jointData = _jointData;
    return jointData;
}

void AvatarData::clearJointData(int index) {
    if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
        return;
    }
    QWriteLocker writeLock(&_jointDataLock);
    // FIXME: I don't understand how this "clears" the joint data at index
    if (_jointData.size() <= index) {
        _jointData.resize(index + 1);
    }
    _jointData[index] = {};
}

bool AvatarData::isJointDataValid(int index) const {
    switch (index) {
        case FARGRAB_RIGHTHAND_INDEX: {
            bool valid;
            _farGrabRightMatrixCache.get(valid);
            return valid;
        }
        case FARGRAB_LEFTHAND_INDEX: {
            bool valid;
            _farGrabLeftMatrixCache.get(valid);
            return valid;
        }
        case FARGRAB_MOUSE_INDEX: {
            bool valid;
            _farGrabMouseMatrixCache.get(valid);
            return valid;
        }
        default: {
            if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
                return false;
            }
            QReadLocker readLock(&_jointDataLock);
            return index < _jointData.size();
        }
    }
}

glm::quat AvatarData::getJointRotation(int index) const {
    switch (index) {
        case FARGRAB_RIGHTHAND_INDEX: {
            return extractRotation(_farGrabRightMatrixCache.get());
        }
        case FARGRAB_LEFTHAND_INDEX: {
            return extractRotation(_farGrabLeftMatrixCache.get());
        }
        case FARGRAB_MOUSE_INDEX: {
            return extractRotation(_farGrabMouseMatrixCache.get());
        }
        default: {
            if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
                return glm::quat();
            }
            QReadLocker readLock(&_jointDataLock);
            return index < _jointData.size() ? _jointData.at(index).rotation : glm::quat();
        }
    }
}

glm::vec3 AvatarData::getJointTranslation(int index) const {
    switch (index) {
        case FARGRAB_RIGHTHAND_INDEX: {
            return extractTranslation(_farGrabRightMatrixCache.get());
        }
        case FARGRAB_LEFTHAND_INDEX: {
            return extractTranslation(_farGrabLeftMatrixCache.get());
        }
        case FARGRAB_MOUSE_INDEX: {
            return extractTranslation(_farGrabMouseMatrixCache.get());
        }
        default: {
            if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
                return glm::vec3();
            }
            QReadLocker readLock(&_jointDataLock);
            return index < _jointData.size() ? _jointData.at(index).translation : glm::vec3();
        }
    }
}

glm::vec3 AvatarData::getJointTranslation(const QString& name) const {
    // Can't do this, because the lock needs to cover the entire set of logic.  In theory, the joints could change 
    // on another thread in between the call to getJointIndex and getJointTranslation
    // return getJointTranslation(getJointIndex(name));
    return readLockWithNamedJointIndex<glm::vec3>(name, [this](int index) {
        return getJointTranslation(index);
    });
}

void AvatarData::setJointData(const QString& name, const glm::quat& rotation, const glm::vec3& translation) {
    // Can't do this, not thread safe
    // setJointData(getJointIndex(name), rotation, translation);

    writeLockWithNamedJointIndex(name, [&](int index) {
        auto& jointData = _jointData[index];
        jointData.rotation = rotation;
        jointData.translation = translation;
        jointData.rotationIsDefaultPose = false;
        jointData.translationIsDefaultPose = false;
    });
}

void AvatarData::setJointRotation(const QString& name, const glm::quat& rotation) {
    // Can't do this, not thread safe
    // setJointRotation(getJointIndex(name), rotation);
    writeLockWithNamedJointIndex(name, [&](int index) {
        auto& data = _jointData[index];
        data.rotation = rotation;
        data.rotationIsDefaultPose = false;
    });
}

void AvatarData::setJointTranslation(const QString& name, const glm::vec3& translation) {
    // Can't do this, not thread safe
    // setJointTranslation(getJointIndex(name), translation);
    writeLockWithNamedJointIndex(name, [&](int index) {
        auto& data = _jointData[index];
        data.translation = translation;
        data.translationIsDefaultPose = false;
    });
}

void AvatarData::setJointRotation(int index, const glm::quat& rotation) {
    if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
        return;
    }
    QWriteLocker writeLock(&_jointDataLock);
    if (_jointData.size() <= index) {
        _jointData.resize(index + 1);
    }
    JointData& data = _jointData[index];
    data.rotation = rotation;
    data.rotationIsDefaultPose = false;
}

void AvatarData::setJointTranslation(int index, const glm::vec3& translation) {
    if (index < 0 || index >= LOWEST_PSEUDO_JOINT_INDEX) {
        return;
    }
    QWriteLocker writeLock(&_jointDataLock);
    if (_jointData.size() <= index) {
        _jointData.resize(index + 1);
    }
    JointData& data = _jointData[index];
    data.translation = translation;
    data.translationIsDefaultPose = false;
}

void AvatarData::clearJointData(const QString& name) {
    // Can't do this, not thread safe
    // clearJointData(getJointIndex(name));
    writeLockWithNamedJointIndex(name, [&](int index) {
        _jointData[index] = {};
    });
}

bool AvatarData::isJointDataValid(const QString& name) const {
    // Can't do this, not thread safe
    // return isJointDataValid(getJointIndex(name));

    return readLockWithNamedJointIndex<bool>(name, false, [&](int index) {
        // This is technically superfluous....  the lambda is only called if index is a valid 
        // offset for _jointData.  Nevertheless, it would be confusing to leave the lamdba as
        // `return true`
        return index < _jointData.size();
    });
}

glm::quat AvatarData::getJointRotation(const QString& name) const {
    // Can't do this, not thread safe
    // return getJointRotation(getJointIndex(name));

    return readLockWithNamedJointIndex<glm::quat>(name, [this](int index) {
        return getJointRotation(index);
    });
}

QVector<glm::quat> AvatarData::getJointRotations() const {
    if (QThread::currentThread() != thread()) {
        QVector<glm::quat> result;
        BLOCKING_INVOKE_METHOD(const_cast<AvatarData*>(this), "getJointRotations",
                                  Q_RETURN_ARG(QVector<glm::quat>, result));
        return result;
    }
    QReadLocker readLock(&_jointDataLock);
    QVector<glm::quat> jointRotations(_jointData.size());
    for (int i = 0; i < _jointData.size(); ++i) {
        jointRotations[i] = _jointData[i].rotation;
    }
    return jointRotations;
}

void AvatarData::setJointRotations(const QVector<glm::quat>& jointRotations) {
    QWriteLocker writeLock(&_jointDataLock);
    auto size = jointRotations.size();
    if (_jointData.size() < size) {
        _jointData.resize(size);
    }
    for (int i = 0; i < size; ++i) {
        auto& data = _jointData[i];
        data.rotation = jointRotations[i];
        data.rotationIsDefaultPose = false;
    }
}

QVector<glm::vec3> AvatarData::getJointTranslations() const {
    QReadLocker readLock(&_jointDataLock);
    QVector<glm::vec3> jointTranslations(_jointData.size());
    for (int i = 0; i < _jointData.size(); ++i) {
        jointTranslations[i] = _jointData[i].translation;
    }
    return jointTranslations;
}

void AvatarData::setJointTranslations(const QVector<glm::vec3>& jointTranslations) {
    QWriteLocker writeLock(&_jointDataLock);
    auto size = jointTranslations.size();
    if (_jointData.size() < size) {
        _jointData.resize(size);
    }
    for (int i = 0; i < size; ++i) {
        auto& data = _jointData[i];
        data.translation = jointTranslations[i];
        data.translationIsDefaultPose = false;
    }
}

void AvatarData::clearJointsData() {
    QWriteLocker writeLock(&_jointDataLock);
    QVector<JointData> newJointData;
    newJointData.resize(_jointData.size());
    _jointData.swap(newJointData);
}

int AvatarData::getFauxJointIndex(const QString& name) const {
    static constexpr QChar fauxJointFirstChar('_');// The first character of all the faux joint names.
    if (!name.startsWith(fauxJointFirstChar)) {
        return -1;
    };
    if (name == "_SENSOR_TO_WORLD_MATRIX") {
        return SENSOR_TO_WORLD_MATRIX_INDEX;
    }
    if (name == "_CONTROLLER_LEFTHAND") {
        return CONTROLLER_LEFTHAND_INDEX;
    }
    if (name == "_CONTROLLER_RIGHTHAND") {
        return CONTROLLER_RIGHTHAND_INDEX;
    }
    if (name == "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND") {
        return CAMERA_RELATIVE_CONTROLLER_LEFTHAND_INDEX;
    }
    if (name == "_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND") {
        return CAMERA_RELATIVE_CONTROLLER_RIGHTHAND_INDEX;
    }
    if (name == "_CAMERA_MATRIX") {
        return CAMERA_MATRIX_INDEX;
    }
    if (name == "_FARGRAB_RIGHTHAND") {
        return FARGRAB_RIGHTHAND_INDEX;
    }
    if (name == "_FARGRAB_LEFTHAND") {
        return FARGRAB_LEFTHAND_INDEX;
    }
    if (name == "_FARGRAB_MOUSE") {
        return FARGRAB_MOUSE_INDEX;
    }
    return -1;
}

int AvatarData::getJointIndex(const QString& name) const {
    int result = getFauxJointIndex(name);
    return result;
}

QStringList AvatarData::getJointNames() const {
    return QStringList();
}

glm::quat AvatarData::getOrientationOutbound() const {
    return (getLocalOrientation());
}

void AvatarData::processAvatarIdentity(QDataStream& packetStream, bool& identityChanged,
                                       bool& displayNameChanged) {

    QUuid avatarSessionID;

    // peek the sequence number, this will tell us if we should be processing this identity packet at all
    udt::SequenceNumber::Type incomingSequenceNumberType;
    packetStream >> avatarSessionID >> incomingSequenceNumberType;
    udt::SequenceNumber incomingSequenceNumber(incomingSequenceNumberType);

    if (!_hasProcessedFirstIdentity) {
        _identitySequenceNumber = incomingSequenceNumber - 1;
        _hasProcessedFirstIdentity = true;
        qCDebug(avatars) << "Processing first identity packet for" << avatarSessionID << "-"
            << (udt::SequenceNumber::Type) incomingSequenceNumber;
    }

    Identity identity;

    packetStream
        >> identity.attachmentData
        >> identity.displayName
        >> identity.sessionDisplayName
        >> identity.identityFlags
        ;

    if (incomingSequenceNumber > _identitySequenceNumber) {

        // set the store identity sequence number to match the incoming identity
        _identitySequenceNumber = incomingSequenceNumber;

        if (identity.displayName != _displayName) {
            _displayName = identity.displayName;
            identityChanged = true;
            displayNameChanged = true;
        }
        maybeUpdateSessionDisplayNameFromTransport(identity.sessionDisplayName);

        bool flagValue;
        flagValue = identity.identityFlags.testFlag(AvatarDataPacket::IdentityFlag::isReplicated);
        if ( flagValue != _isReplicated) {
            _isReplicated = flagValue;
            identityChanged = true;
        }

        flagValue = identity.identityFlags.testFlag(AvatarDataPacket::IdentityFlag::lookAtSnapping);
        if ( flagValue != _lookAtSnappingEnabled) {
            setProperty("lookAtSnappingEnabled", flagValue);
            identityChanged = true;
        }

        flagValue = identity.identityFlags.testFlag(AvatarDataPacket::IdentityFlag::verificationFailed);
        if (flagValue != _verificationFailed) {
            _verificationFailed = flagValue;
            identityChanged = true;
            setSkeletonModelURL(_skeletonModelURL);
            if (_verificationFailed) {
                qCDebug(avatars) << "Avatar" << getSessionDisplayName() << "marked as VERIFY-FAILED";
            }
        };

        if (identity.attachmentData != _attachmentData) {
            setAttachmentData(identity.attachmentData);
            identityChanged = true;
        }

#ifdef WANT_DEBUG
        qCDebug(avatars) << __FUNCTION__
            << "identity.uuid:" << identity.uuid
            << "identity.displayName:" << identity.displayName
            << "identity.sessionDisplayName:" << identity.sessionDisplayName;
    } else {

        qCDebug(avatars) << "Refusing to process identity for" << uuidStringWithoutCurlyBraces(avatarSessionID) << "since"
            << (udt::SequenceNumber::Type) _identitySequenceNumber
            << "is >=" << (udt::SequenceNumber::Type) incomingSequenceNumber;
#endif
    }
}

QUrl AvatarData::getWireSafeSkeletonModelURL() const {
    if (_skeletonModelURL.scheme() != "file" && _skeletonModelURL.scheme() != "qrc") {
        return _skeletonModelURL;
    } else {
        return QUrl();
    }
}

static const QString VERIFY_FAIL_MODEL { "/meshes/verifyFailed.fst" };

const QUrl& AvatarData::getSkeletonModelURL() const {
    if (_verificationFailed) {
        static QUrl VERIFY_FAIL_MODEL_URL = PathUtils::resourcesUrl(VERIFY_FAIL_MODEL);
        return VERIFY_FAIL_MODEL_URL;
    } else {
        return _skeletonModelURL;
    }
}

QByteArray AvatarData::packSkeletonData() const {
    // Send an avatar trait packet with the skeleton data before the mesh is loaded
    int avatarDataSize = 0;
    QByteArray avatarDataByteArray;
    _avatarSkeletonDataLock.withReadLock([&] {
        // Add header
        AvatarSkeletonTrait::Header header;
        header.maxScaleDimension = 0.0f;
        header.maxTranslationDimension = 0.0f;
        header.numJoints = (uint8_t)_avatarSkeletonData.size();
        header.stringTableLength = 0;

        for (size_t i = 0; i < _avatarSkeletonData.size(); i++) {
            header.stringTableLength += (uint16_t)_avatarSkeletonData[i].jointName.size();
            auto& translation = _avatarSkeletonData[i].defaultTranslation;
            header.maxTranslationDimension = std::max(header.maxTranslationDimension, std::max(std::max(translation.x, translation.y), translation.z));
            header.maxScaleDimension = std::max(header.maxScaleDimension, _avatarSkeletonData[i].defaultScale);
        }

        const int byteArraySize = (int)sizeof(AvatarSkeletonTrait::Header) + (int)(header.numJoints * sizeof(AvatarSkeletonTrait::JointData)) + header.stringTableLength;
        avatarDataByteArray = QByteArray(byteArraySize, 0);
        unsigned char* destinationBuffer = reinterpret_cast<unsigned char*>(avatarDataByteArray.data());
        const unsigned char* const startPosition = destinationBuffer;

        memcpy(destinationBuffer, &header, sizeof(header));
        destinationBuffer += sizeof(AvatarSkeletonTrait::Header);

        QString stringTable = "";
        for (size_t i = 0; i < _avatarSkeletonData.size(); i++) {
            AvatarSkeletonTrait::JointData jdata;
            jdata.boneType = _avatarSkeletonData[i].boneType;
            jdata.parentIndex = _avatarSkeletonData[i].parentIndex;
            packFloatRatioToTwoByte((uint8_t*)(&jdata.defaultScale), _avatarSkeletonData[i].defaultScale / header.maxScaleDimension);
            packOrientationQuatToSixBytes(jdata.defaultRotation, _avatarSkeletonData[i].defaultRotation);
            packFloatVec3ToSignedTwoByteFixed(jdata.defaultTranslation, _avatarSkeletonData[i].defaultTranslation / header.maxTranslationDimension, TRANSLATION_COMPRESSION_RADIX);
            jdata.jointIndex = (uint16_t)i;
            jdata.stringStart = (uint16_t)_avatarSkeletonData[i].stringStart;
            jdata.stringLength = (uint8_t)_avatarSkeletonData[i].stringLength;
            stringTable += _avatarSkeletonData[i].jointName;
            memcpy(destinationBuffer, &jdata, sizeof(AvatarSkeletonTrait::JointData));
            destinationBuffer += sizeof(AvatarSkeletonTrait::JointData);
        }

        memcpy(destinationBuffer, stringTable.toUtf8(), header.stringTableLength);
        destinationBuffer += header.stringTableLength;

        avatarDataSize = destinationBuffer - startPosition;
    });
    return avatarDataByteArray.left(avatarDataSize);
}

QByteArray AvatarData::packSkeletonModelURL() const {
    return getWireSafeSkeletonModelURL().toEncoded();
}

void AvatarData::unpackSkeletonData(const QByteArray& data) {

    const unsigned char* startPosition = reinterpret_cast<const unsigned char*>(data.data());
    const unsigned char* sourceBuffer = startPosition;
    
    auto header = reinterpret_cast<const AvatarSkeletonTrait::Header*>(sourceBuffer);
    sourceBuffer += sizeof(const AvatarSkeletonTrait::Header);

    std::vector<AvatarSkeletonTrait::UnpackedJointData> joints;
    for (uint8_t i = 0; i < header->numJoints; i++) {
        auto jointData = reinterpret_cast<const AvatarSkeletonTrait::JointData*>(sourceBuffer);
        sourceBuffer += sizeof(const AvatarSkeletonTrait::JointData);
        AvatarSkeletonTrait::UnpackedJointData uJointData;
        uJointData.boneType = (int)jointData->boneType;
        uJointData.jointIndex = (int)i;
        uJointData.stringLength = (int)jointData->stringLength;
        uJointData.stringStart = (int)jointData->stringStart;
        uJointData.parentIndex = ((uJointData.boneType == AvatarSkeletonTrait::BoneType::SkeletonRoot) || 
                                  (uJointData.boneType == AvatarSkeletonTrait::BoneType::NonSkeletonRoot)) ? -1 : (int)jointData->parentIndex;
        unpackOrientationQuatFromSixBytes(reinterpret_cast<const unsigned char*>(&jointData->defaultRotation), uJointData.defaultRotation);
        unpackFloatVec3FromSignedTwoByteFixed(reinterpret_cast<const unsigned char*>(&jointData->defaultTranslation), uJointData.defaultTranslation, TRANSLATION_COMPRESSION_RADIX);
        unpackFloatRatioFromTwoByte(reinterpret_cast<const unsigned char*>(&jointData->defaultScale), uJointData.defaultScale);
        uJointData.defaultTranslation *= header->maxTranslationDimension;
        uJointData.defaultScale *= header->maxScaleDimension;
        joints.push_back(uJointData);
    }
    QString table = QString::fromUtf8(reinterpret_cast<const char*>(sourceBuffer), (int)header->stringTableLength);
    for (size_t i = 0; i < joints.size(); i++) {
        QStringRef subString(&table, joints[i].stringStart, joints[i].stringLength);
        joints[i].jointName = subString.toString();
    }
    if (_clientTraitsHandler) {
        _clientTraitsHandler->markTraitUpdated(AvatarTraits::SkeletonData);
    }
    setSkeletonData(joints);
}

void AvatarData::unpackSkeletonModelURL(const QByteArray& data) {
    auto skeletonModelURL = QUrl::fromEncoded(data);
    setSkeletonModelURL(skeletonModelURL);
}

QByteArray AvatarData::packAvatarEntityTraitInstance(AvatarTraits::TraitInstanceID traitInstanceID) {
    // grab a read lock on the avatar entities and check for entity data for the given ID
    QByteArray entityBinaryData;
    _avatarEntitiesLock.withReadLock([this, &entityBinaryData, &traitInstanceID] {
        if (_packedAvatarEntityData.contains(traitInstanceID)) {
            entityBinaryData = _packedAvatarEntityData[traitInstanceID];
        }
    });

    return entityBinaryData;
}

QByteArray AvatarData::packGrabTraitInstance(AvatarTraits::TraitInstanceID traitInstanceID) {
    // grab a read lock on the avatar grabs and check for grab data for the given ID
    QByteArray grabBinaryData;
    _avatarGrabsLock.withReadLock([this, &grabBinaryData, &traitInstanceID] {
        if (_avatarGrabData.contains(traitInstanceID)) {
            grabBinaryData = _avatarGrabData[traitInstanceID];
        }
    });

    return grabBinaryData;
}

QByteArray AvatarData::packTrait(AvatarTraits::TraitType traitType) const {
    QByteArray traitBinaryData;

    // Call packer function
    if (traitType == AvatarTraits::SkeletonModelURL) {
        traitBinaryData = packSkeletonModelURL();
    } else if (traitType == AvatarTraits::SkeletonData) {
        traitBinaryData = packSkeletonData();
    }

    return traitBinaryData;
}

QByteArray AvatarData::packTraitInstance(AvatarTraits::TraitType traitType, AvatarTraits::TraitInstanceID traitInstanceID) {
    QByteArray traitBinaryData;

    // Call packer function
    if (traitType == AvatarTraits::AvatarEntity) {
        traitBinaryData = packAvatarEntityTraitInstance(traitInstanceID);
    } else if (traitType == AvatarTraits::Grab) {
        traitBinaryData = packGrabTraitInstance(traitInstanceID);
    }

    return traitBinaryData;
}

void AvatarData::processTrait(AvatarTraits::TraitType traitType, QByteArray traitBinaryData) {
    if (traitType == AvatarTraits::SkeletonModelURL) {
        unpackSkeletonModelURL(traitBinaryData);
    } else if (traitType == AvatarTraits::SkeletonData) {
        unpackSkeletonData(traitBinaryData);
    }
}

void AvatarData::processTraitInstance(AvatarTraits::TraitType traitType,
                                      AvatarTraits::TraitInstanceID instanceID, QByteArray traitBinaryData) {
    if (traitType == AvatarTraits::AvatarEntity) {
        storeAvatarEntityDataPayload(instanceID, traitBinaryData);
    } else if (traitType == AvatarTraits::Grab) {
        updateAvatarGrabData(instanceID, traitBinaryData);
    }
}

void AvatarData::processDeletedTraitInstance(AvatarTraits::TraitType traitType, AvatarTraits::TraitInstanceID instanceID) {
    if (traitType == AvatarTraits::AvatarEntity) {
        clearAvatarEntityInternal(instanceID);
    } else if (traitType == AvatarTraits::Grab) {
        clearAvatarGrabData(instanceID);
    }
}

void AvatarData::prepareResetTraitInstances() {
    if (_clientTraitsHandler) {
        _avatarEntitiesLock.withReadLock([this]{
            foreach (auto entityID, _packedAvatarEntityData.keys()) {
                _clientTraitsHandler->markInstancedTraitUpdated(AvatarTraits::AvatarEntity, entityID);
            }
            foreach (auto grabID, _avatarGrabData.keys()) {
                _clientTraitsHandler->markInstancedTraitUpdated(AvatarTraits::Grab, grabID);
            }
        });
    }
}

QByteArray AvatarData::identityByteArray(bool setIsReplicated) const {
    QByteArray identityData;
    QDataStream identityStream(&identityData, QIODevice::Append);
    using namespace AvatarDataPacket;

    // when mixers send identity packets to agents, they simply forward along the last incoming sequence number they received
    // whereas agents send a fresh outgoing sequence number when identity data has changed
    IdentityFlags identityFlags = IdentityFlag::none;
    if (_isReplicated || setIsReplicated) {
        identityFlags.setFlag(IdentityFlag::isReplicated);
    }
    if (_lookAtSnappingEnabled) {
        identityFlags.setFlag(IdentityFlag::lookAtSnapping);
    }
    if (isCertifyFailed()) {
        identityFlags.setFlag(IdentityFlag::verificationFailed);
    }

    identityStream << getSessionUUID()
        << (udt::SequenceNumber::Type) _identitySequenceNumber
        << _attachmentData
        << _displayName
        << getSessionDisplayNameForTransport() // depends on _sessionDisplayName
        << identityFlags;

    return identityData;
}

void AvatarData::setSkeletonModelURL(const QUrl& skeletonModelURL) {
    if (skeletonModelURL.isEmpty()) {
        qCDebug(avatars) << __FUNCTION__ << "caller called with empty URL.";
    }

    const QUrl& expanded = skeletonModelURL.isEmpty() ? AvatarData::defaultFullAvatarModelUrl() : skeletonModelURL;
    if (expanded == _skeletonModelURL) {
        return;
    }
    
    _skeletonModelURL = expanded;
    if (_clientTraitsHandler) {
        _clientTraitsHandler->markTraitUpdated(AvatarTraits::SkeletonModelURL);
    }

    emit skeletonModelURLChanged();
}

void AvatarData::setDisplayName(const QString& displayName) {
    _displayName = displayName;
    _sessionDisplayName = "";

    qCDebug(avatars) << "Changing display name for avatar to" << displayName;
    markIdentityDataChanged();
}

QVector<AttachmentData> AvatarData::getAttachmentData() const {
    if (QThread::currentThread() != thread()) {
        QVector<AttachmentData> result;
        BLOCKING_INVOKE_METHOD(const_cast<AvatarData*>(this), "getAttachmentData",
            Q_RETURN_ARG(QVector<AttachmentData>, result));
        return result;
    }
    return _attachmentData;
}

void AvatarData::setAttachmentData(const QVector<AttachmentData>& attachmentData) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setAttachmentData", Q_ARG(const QVector<AttachmentData>&, attachmentData));
        return;
    }
    _attachmentData = attachmentData;
    markIdentityDataChanged();
}

void AvatarData::attach(const QString& modelURL, const QString& jointName,
                        const glm::vec3& translation, const glm::quat& rotation,
                        float scale, bool isSoft,
                        bool allowDuplicates, bool useSaved) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "attach", Q_ARG(const QString&, modelURL), Q_ARG(const QString&, jointName),
                                  Q_ARG(const glm::vec3&, translation), Q_ARG(const glm::quat&, rotation),
                                  Q_ARG(float, scale), Q_ARG(bool, isSoft),
                                  Q_ARG(bool, allowDuplicates), Q_ARG(bool, useSaved));
        return;
    }
    QVector<AttachmentData> attachmentData = getAttachmentData();
    if (!allowDuplicates) {
        foreach (const AttachmentData& data, attachmentData) {
            if (data.modelURL == modelURL && (jointName.isEmpty() || data.jointName == jointName)) {
                return;
            }
        }
    }
    AttachmentData data;
    data.modelURL = modelURL;
    data.jointName = jointName;
    data.translation = translation;
    data.rotation = rotation;
    data.scale = scale;
    data.isSoft = isSoft;
    attachmentData.append(data);
    setAttachmentData(attachmentData);
}

void AvatarData::detachOne(const QString& modelURL, const QString& jointName) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "detachOne", Q_ARG(const QString&, modelURL), Q_ARG(const QString&, jointName));
        return;
    }
    QVector<AttachmentData> attachmentData = getAttachmentData();
    for (QVector<AttachmentData>::iterator it = attachmentData.begin(); it != attachmentData.end(); ++it) {
        if (it->modelURL == modelURL && (jointName.isEmpty() || it->jointName == jointName)) {
            attachmentData.erase(it);
            setAttachmentData(attachmentData);
            return;
        }
    }
}

void AvatarData::detachAll(const QString& modelURL, const QString& jointName) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "detachAll", Q_ARG(const QString&, modelURL), Q_ARG(const QString&, jointName));
        return;
    }
    QVector<AttachmentData> attachmentData = getAttachmentData();
    for (QVector<AttachmentData>::iterator it = attachmentData.begin(); it != attachmentData.end(); ) {
        if (it->modelURL == modelURL && (jointName.isEmpty() || it->jointName == jointName)) {
            it = attachmentData.erase(it);
        } else {
            ++it;
        }
    }
    setAttachmentData(attachmentData);
}

int AvatarData::sendAvatarDataPacket(bool sendAll) {
    auto nodeList = DependencyManager::get<NodeList>();

    // about 2% of the time, we send a full update (meaning, we transmit all the joint data), even if nothing has changed.
    // this is to guard against a joint moving once, the packet getting lost, and the joint never moving again.

    bool cullSmallData = !sendAll && (randFloat() < AVATAR_SEND_FULL_UPDATE_RATIO);
    auto dataDetail = cullSmallData ? SendAllData : CullSmallData;
    QByteArray avatarByteArray = toByteArrayStateful(dataDetail);

    int maximumByteArraySize = NLPacket::maxPayloadSize(PacketType::AvatarData) - sizeof(AvatarDataSequenceNumber);

    if (avatarByteArray.size() > maximumByteArraySize) {
        avatarByteArray = toByteArrayStateful(dataDetail, true);

        if (avatarByteArray.size() > maximumByteArraySize) {
            avatarByteArray = toByteArrayStateful(MinimumData, true);

            if (avatarByteArray.size() > maximumByteArraySize) {
                qCWarning(avatars) << "toByteArrayStateful() MinimumData resulted in very large buffer:" << avatarByteArray.size() << "... FAIL!!";
                return 0;
            }
        }
    }

    doneEncoding(cullSmallData);

    static AvatarDataSequenceNumber sequenceNumber = 0;

    auto avatarPacket = NLPacket::create(PacketType::AvatarData, avatarByteArray.size() + sizeof(sequenceNumber));
    avatarPacket->writePrimitive(sequenceNumber++);
    avatarPacket->write(avatarByteArray);
    auto packetSize = avatarPacket->getWireSize();

    nodeList->broadcastToNodes(std::move(avatarPacket), NodeSet() << NodeType::AvatarMixer);

    return packetSize;
}

int AvatarData::sendIdentityPacket() {
    auto nodeList = DependencyManager::get<NodeList>();

    if (_identityDataChanged) {
        // if the identity data has changed, push the sequence number forwards
        ++_identitySequenceNumber;
    }
    QByteArray identityData = identityByteArray();

    auto packetList = NLPacketList::create(PacketType::AvatarIdentity, QByteArray(), true, true);
    packetList->write(identityData);
    nodeList->eachMatchingNode(
        [](const SharedNodePointer& node)->bool {
            return node->getType() == NodeType::AvatarMixer && node->getActiveSocket();
        },
        [&](const SharedNodePointer& node) {
            nodeList->sendPacketList(std::move(packetList), *node);
    });

    _identityDataChanged = false;
    return identityData.size();
}

static const QString JSON_ATTACHMENT_URL = QStringLiteral("modelUrl");
static const QString JSON_ATTACHMENT_JOINT_NAME = QStringLiteral("jointName");
static const QString JSON_ATTACHMENT_TRANSFORM = QStringLiteral("transform");
static const QString JSON_ATTACHMENT_IS_SOFT = QStringLiteral("isSoft");

QJsonObject AttachmentData::toJson() const {
    QJsonObject result;
    if (modelURL.isValid() && !modelURL.isEmpty()) {
        result[JSON_ATTACHMENT_URL] = modelURL.toString();
    }
    if (!jointName.isEmpty()) {
        result[JSON_ATTACHMENT_JOINT_NAME] = jointName;
    }
    // FIXME the transform constructor that takes rot/scale/translation
    // doesn't return the correct value for isIdentity()
    Transform transform;
    transform.setRotation(rotation);
    transform.setScale(scale);
    transform.setTranslation(translation);
    if (!transform.isIdentity()) {
        result[JSON_ATTACHMENT_TRANSFORM] = Transform::toJson(transform);
    }
    result[JSON_ATTACHMENT_IS_SOFT] = isSoft;
    return result;
}

void AttachmentData::fromJson(const QJsonObject& json) {
    if (json.contains(JSON_ATTACHMENT_URL)) {
        const QString modelURLTemp = json[JSON_ATTACHMENT_URL].toString();
        if (modelURLTemp != modelURL.toString()) {
            modelURL = modelURLTemp;
        }
    }

    if (json.contains(JSON_ATTACHMENT_JOINT_NAME)) {
        const QString jointNameTemp = json[JSON_ATTACHMENT_JOINT_NAME].toString();
        if (jointNameTemp != jointName) {
            jointName = jointNameTemp;
        }
    }

    if (json.contains(JSON_ATTACHMENT_TRANSFORM)) {
        Transform transform = Transform::fromJson(json[JSON_ATTACHMENT_TRANSFORM]);
        translation = transform.getTranslation();
        rotation = transform.getRotation();
        scale = transform.getScale().x;
    }

    if (json.contains(JSON_ATTACHMENT_IS_SOFT)) {
        isSoft = json[JSON_ATTACHMENT_IS_SOFT].toBool();
    }
}

bool AttachmentData::operator==(const AttachmentData& other) const {
    return modelURL == other.modelURL && jointName == other.jointName && translation == other.translation &&
        rotation == other.rotation && scale == other.scale && isSoft == other.isSoft;
}

QDataStream& operator<<(QDataStream& out, const AttachmentData& attachment) {
    return out << attachment.modelURL << attachment.jointName <<
        attachment.translation << attachment.rotation << attachment.scale << attachment.isSoft;
}

QDataStream& operator>>(QDataStream& in, AttachmentData& attachment) {
    return in >> attachment.modelURL >> attachment.jointName >>
        attachment.translation >> attachment.rotation >> attachment.scale >> attachment.isSoft;
}

void AttachmentDataObject::setModelURL(const QString& modelURL) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.modelURL = modelURL;
    thisObject() = engine()->toScriptValue(data);
}

QString AttachmentDataObject::getModelURL() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).modelURL.toString();
}

void AttachmentDataObject::setJointName(const QString& jointName) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.jointName = jointName;
    thisObject() = engine()->toScriptValue(data);
}

QString AttachmentDataObject::getJointName() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).jointName;
}

void AttachmentDataObject::setTranslation(const glm::vec3& translation) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.translation = translation;
    thisObject() = engine()->toScriptValue(data);
}

glm::vec3 AttachmentDataObject::getTranslation() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).translation;
}

void AttachmentDataObject::setRotation(const glm::quat& rotation) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.rotation = rotation;
    thisObject() = engine()->toScriptValue(data);
}

glm::quat AttachmentDataObject::getRotation() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).rotation;
}

void AttachmentDataObject::setScale(float scale) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.scale = scale;
    thisObject() = engine()->toScriptValue(data);
}

float AttachmentDataObject::getScale() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).scale;
}

void AttachmentDataObject::setIsSoft(bool isSoft) {
    AttachmentData data = scriptvalue_cast<AttachmentData>(thisObject());
    data.isSoft = isSoft;
    thisObject() = engine()->toScriptValue(data);
}

bool AttachmentDataObject::getIsSoft() const {
    return scriptvalue_cast<AttachmentData>(thisObject()).isSoft;
}

void registerAvatarTypes(ScriptEngine* engine) {
    scriptRegisterSequenceMetaType<QVector<AttachmentData> >(engine);
    engine->setDefaultPrototype(qMetaTypeId<AttachmentData>(), engine->newQObject(
        new AttachmentDataObject(), ScriptEngine::ScriptOwnership));
}

void AvatarData::setRecordingBasis(std::shared_ptr<Transform> recordingBasis) {
    if (!recordingBasis) {
        recordingBasis = std::make_shared<Transform>();
        recordingBasis->setRotation(getWorldOrientation());
        recordingBasis->setTranslation(getWorldPosition());
        // TODO: find a  different way to record/playback the Scale of the avatar
        //recordingBasis->setScale(getTargetScale());
    }
    _recordingBasis = recordingBasis;
}

void AvatarData::createRecordingIDs() {
    _avatarEntitiesLock.withReadLock([&] {
        _avatarEntityForRecording.clear();
        for (int i = 0; i < _packedAvatarEntityData.size(); i++) {
            _avatarEntityForRecording.insert(QUuid::createUuid());
        }
    });
}

void AvatarData::clearRecordingBasis() {
    _recordingBasis.reset();
}

static const QString JSON_AVATAR_BASIS = QStringLiteral("basisTransform");
static const QString JSON_AVATAR_RELATIVE = QStringLiteral("relativeTransform");
static const QString JSON_AVATAR_JOINT_ARRAY = QStringLiteral("jointArray");
static const QString JSON_AVATAR_HEAD = QStringLiteral("head");
static const QString JSON_AVATAR_HEAD_MODEL = QStringLiteral("headModel");
static const QString JSON_AVATAR_BODY_MODEL = QStringLiteral("bodyModel");
static const QString JSON_AVATAR_DISPLAY_NAME = QStringLiteral("displayName");
// It isn't meaningful to persist sessionDisplayName.
static const QString JSON_AVATAR_ATTACHMENTS = QStringLiteral("attachments");
static const QString JSON_AVATAR_ENTITIES = QStringLiteral("attachedEntities");
static const QString JSON_AVATAR_SCALE = QStringLiteral("scale");
static const QString JSON_AVATAR_VERSION = QStringLiteral("version");

enum class JsonAvatarFrameVersion : int {
    JointRotationsInRelativeFrame = 0,
    JointRotationsInAbsoluteFrame,
    JointDefaultPoseBits,
    JointUnscaledTranslations,
    ARKitBlendshapes
};

QJsonValue toJsonValue(const JointData& joint) {
    QJsonArray result;
    result.push_back(toJsonValue(joint.rotation));
    result.push_back(toJsonValue(joint.translation));
    result.push_back(QJsonValue(joint.rotationIsDefaultPose));
    result.push_back(QJsonValue(joint.translationIsDefaultPose));
    return result;
}

JointData jointDataFromJsonValue(int version, const QJsonValue& json) {
    JointData result;
    if (json.isArray()) {
        QJsonArray array = json.toArray();
        result.rotation = quatFromJsonValue(array[0]);

        result.translation = vec3FromJsonValue(array[1]);

        // In old recordings, translations are scaled by _geometryOffset.  Undo that scaling.
        if (version < (int)JsonAvatarFrameVersion::JointUnscaledTranslations) {
            // because we don't have access to the actual _geometryOffset used. we have to guess.
            // most avatar FBX files were authored in centimeters.
            const float METERS_TO_CENTIMETERS = 100.0f;
            result.translation *= METERS_TO_CENTIMETERS;
        }
        if (version >= (int)JsonAvatarFrameVersion::JointDefaultPoseBits) {
            result.rotationIsDefaultPose = array[2].toBool();
            result.translationIsDefaultPose = array[3].toBool();
        } else {
            result.rotationIsDefaultPose = false;
            result.translationIsDefaultPose = false;
        }
    }
    return result;
}

void AvatarData::avatarEntityDataToJson(QJsonObject& root) const {
    // overridden where needed
}

QJsonObject AvatarData::toJson() const {
    QJsonObject root;

    root[JSON_AVATAR_VERSION] = (int)JsonAvatarFrameVersion::ARKitBlendshapes;

    if (!getSkeletonModelURL().isEmpty()) {
        root[JSON_AVATAR_BODY_MODEL] = getSkeletonModelURL().toString();
    }
    if (!getDisplayName().isEmpty()) {
        root[JSON_AVATAR_DISPLAY_NAME] = getDisplayName();
    }

    avatarEntityDataToJson(root);

    auto recordingBasis = getRecordingBasis();
    bool success;
    Transform avatarTransform = getTransform(success);
    if (!success) {
        qCWarning(avatars) << "Warning -- AvatarData::toJson couldn't get avatar transform";
    }
    avatarTransform.setScale(getDomainLimitedScale());
    if (recordingBasis) {
        root[JSON_AVATAR_BASIS] = Transform::toJson(*recordingBasis);
        // Find the relative transform
        auto relativeTransform = recordingBasis->relativeTransform(avatarTransform);
        if (!relativeTransform.isIdentity()) {
            root[JSON_AVATAR_RELATIVE] = Transform::toJson(relativeTransform);
        }
    } else {
        root[JSON_AVATAR_RELATIVE] = Transform::toJson(avatarTransform);
    }

    auto scale = getDomainLimitedScale();
    if (scale != 1.0f) {
        root[JSON_AVATAR_SCALE] = scale;
    }

    // Skeleton pose
    QJsonArray jointArray;
    for (const auto& joint : getRawJointData()) {
        jointArray.push_back(toJsonValue(joint));
    }
    root[JSON_AVATAR_JOINT_ARRAY] = jointArray;

    const HeadData* head = getHeadData();
    if (head) {
        auto headJson = head->toJson();
        if (!headJson.isEmpty()) {
            root[JSON_AVATAR_HEAD] = headJson;
        }
    }
    return root;
}

void AvatarData::fromJson(const QJsonObject& json, bool useFrameSkeleton) {
    int version;
    if (json.contains(JSON_AVATAR_VERSION)) {
        version = json[JSON_AVATAR_VERSION].toInt();
    } else {
        // initial data did not have a version field.
        version = (int)JsonAvatarFrameVersion::JointRotationsInRelativeFrame;
    }

    if (json.contains(JSON_AVATAR_BODY_MODEL)) {
        auto bodyModelURL = json[JSON_AVATAR_BODY_MODEL].toString();
        if (useFrameSkeleton && bodyModelURL != getSkeletonModelURL().toString()) {
            setSkeletonModelURL(bodyModelURL);
        }
    }

    QString newDisplayName = "";
    if (json.contains(JSON_AVATAR_DISPLAY_NAME)) {
        newDisplayName = json[JSON_AVATAR_DISPLAY_NAME].toString();
    }
    if (newDisplayName != getDisplayName()) {
        setDisplayName(newDisplayName);
    }

    auto currentBasis = getRecordingBasis();
    if (!currentBasis) {
        currentBasis = std::make_shared<Transform>(Transform::fromJson(json[JSON_AVATAR_BASIS]));
    }

    glm::quat orientation;
    if (json.contains(JSON_AVATAR_RELATIVE)) {
        // During playback you can either have the recording basis set to the avatar current state
        // meaning that all playback is relative to this avatars starting position, or
        // the basis can be loaded from the recording, meaning the playback is relative to the
        // original avatar location
        // The first is more useful for playing back recordings on your own avatar, while
        // the latter is more useful for playing back other avatars within your scene.

        auto relativeTransform = Transform::fromJson(json[JSON_AVATAR_RELATIVE]);
        auto worldTransform = currentBasis->worldTransform(relativeTransform);
        setWorldPosition(worldTransform.getTranslation());
        orientation = worldTransform.getRotation();
    } else {
        // We still set the position in the case that there is no movement.
        setWorldPosition(currentBasis->getTranslation());
        orientation = currentBasis->getRotation();
    }
    setWorldOrientation(orientation);
    updateAttitude(orientation);

    // Do after avatar orientation because head look-at needs avatar orientation.
    if (json.contains(JSON_AVATAR_HEAD)) {
        if (!_headData) {
            _headData = new HeadData(this);
        }
        _headData->fromJson(json[JSON_AVATAR_HEAD].toObject());
    }

    if (json.contains(JSON_AVATAR_SCALE)) {
        setTargetScale((float)json[JSON_AVATAR_SCALE].toDouble());
    }

    QVector<AttachmentData> attachments;
    if (json.contains(JSON_AVATAR_ATTACHMENTS) && json[JSON_AVATAR_ATTACHMENTS].isArray()) {
        QJsonArray attachmentsJson = json[JSON_AVATAR_ATTACHMENTS].toArray();
        for (auto attachmentJson : attachmentsJson) {
            AttachmentData attachment;
            attachment.fromJson(attachmentJson.toObject());
            attachments.push_back(attachment);
        }
    }
    if (attachments != getAttachmentData()) {
        setAttachmentData(attachments);
    }

    if (json.contains(JSON_AVATAR_ENTITIES) && json[JSON_AVATAR_ENTITIES].isArray()) {
        QJsonArray attachmentsJson = json[JSON_AVATAR_ENTITIES].toArray();
        for (auto attachmentJson : attachmentsJson) {
            if (attachmentJson.isObject()) {
                QVariantMap entityData = attachmentJson.toObject().toVariantMap();
                QUuid id = entityData.value("id").toUuid();
                QByteArray data = QByteArray::fromBase64(entityData.value("properties").toByteArray());
                updateAvatarEntity(id, data);
            }
        }
    }

    if (json.contains(JSON_AVATAR_JOINT_ARRAY)) {
        if (version == (int)JsonAvatarFrameVersion::JointRotationsInRelativeFrame) {
            // because we don't have the full joint hierarchy skeleton of the model,
            // we can't properly convert from relative rotations into absolute rotations.
            quint64 now = usecTimestampNow();
            if (shouldLogError(now)) {
                qCWarning(avatars) << "Version 0 avatar recordings not supported. using default rotations";
            }
        } else {
            QVector<JointData> jointArray;
            QJsonArray jointArrayJson = json[JSON_AVATAR_JOINT_ARRAY].toArray();
            jointArray.reserve(jointArrayJson.size());
            for (const auto& jointJson : jointArrayJson) {
                auto joint = jointDataFromJsonValue(version, jointJson);
                jointArray.push_back(joint);
            }
            setRawJointData(jointArray);
        }
    }
}

// Every frame will store both a basis for the recording and a relative transform
// This allows the application to decide whether playback should be relative to an avatar's
// transform at the start of playback, or relative to the transform of the recorded
// avatar
QByteArray AvatarData::toFrame(const AvatarData& avatar) {
    QJsonObject root = avatar.toJson();
#ifdef WANT_JSON_DEBUG
    {
        QJsonObject obj = root;
        obj.remove(JSON_AVATAR_JOINT_ARRAY);
        qCDebug(avatars).noquote() << QJsonDocument(obj).toJson(QJsonDocument::JsonFormat::Indented);
    }
#endif
    return QJsonDocument(root).toBinaryData();
}


void AvatarData::fromFrame(const QByteArray& frameData, AvatarData& result, bool useFrameSkeleton) {
    QJsonDocument doc = QJsonDocument::fromBinaryData(frameData);

#ifdef WANT_JSON_DEBUG
    {
        QJsonObject obj = doc.object();
        obj.remove(JSON_AVATAR_JOINT_ARRAY);
        qCDebug(avatars).noquote() << QJsonDocument(obj).toJson(QJsonDocument::JsonFormat::Indented);
    }
#endif
    result.fromJson(doc.object(), useFrameSkeleton);
}

float AvatarData::getBodyYaw() const {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    return eulerAngles.y;
}

void AvatarData::setBodyYaw(float bodyYaw) {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    eulerAngles.y = bodyYaw;
    setWorldOrientation(glm::quat(glm::radians(eulerAngles)));
}

float AvatarData::getBodyPitch() const {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    return eulerAngles.x;
}

void AvatarData::setBodyPitch(float bodyPitch) {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    eulerAngles.x = bodyPitch;
    setWorldOrientation(glm::quat(glm::radians(eulerAngles)));
}

float AvatarData::getBodyRoll() const {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    return eulerAngles.z;
}

void AvatarData::setBodyRoll(float bodyRoll) {
    glm::vec3 eulerAngles = glm::degrees(safeEulerAngles(getWorldOrientation()));
    eulerAngles.z = bodyRoll;
    setWorldOrientation(glm::quat(glm::radians(eulerAngles)));
}

void AvatarData::setPositionViaScript(const glm::vec3& position) {
    SpatiallyNestable::setWorldPosition(position);
}

void AvatarData::setOrientationViaScript(const glm::quat& orientation) {
    SpatiallyNestable::setWorldOrientation(orientation);
}

glm::quat AvatarData::getAbsoluteJointRotationInObjectFrame(int index) const {
    assert(false);
    return glm::quat();
}

glm::vec3 AvatarData::getAbsoluteJointTranslationInObjectFrame(int index) const {
    assert(false);
    return glm::vec3();
}

/*@jsdoc
 * Information on an attachment worn by the avatar.
 * @typedef {object} AttachmentData
 * @property {string} modelUrl - The URL of the glTF, FBX, or OBJ model file. glTF models may be in JSON or binary format 
 *     (".gltf" or ".glb" URLs respectively).
 * @property {string} jointName - The name of the joint that the attachment is parented to.
 * @property {Vec3} translation - The offset from the joint that the attachment is positioned at.
 * @property {Vec3} rotation - The rotation applied to the model relative to the joint orientation.
 * @property {number} scale - The scale applied to the attachment model.
 * @property {boolean} soft - If <code>true</code> and the model has a skeleton, the bones of the attached model's skeleton are 
 *   rotated to fit the avatar's current pose. If <code>true</code>, the <code>translation</code>, <code>rotation</code>, and 
 *   <code>scale</code> parameters are ignored.
 */
QVariant AttachmentData::toVariant() const {
    QVariantMap result;
    result["modelUrl"] = modelURL;
    result["jointName"] = jointName;
    result["translation"] = vec3ToQMap(translation);
    result["rotation"] = vec3ToQMap(glm::degrees(safeEulerAngles(rotation)));
    result["scale"] = scale;
    result["soft"] = isSoft;
    return result;
}

glm::vec3 variantToVec3(const QVariant& var) {
    auto map = var.toMap();
    glm::vec3 result;
    result.x = map["x"].toFloat();
    result.y = map["y"].toFloat();
    result.z = map["z"].toFloat();
    return result;
}

bool AttachmentData::fromVariant(const QVariant& variant) {
    bool isValid = false;
    auto map = variant.toMap();
    if (map.contains("modelUrl")) {
        auto urlString = map["modelUrl"].toString();
        modelURL = urlString;
        isValid = true;
    }
    if (map.contains("jointName")) {
        jointName = map["jointName"].toString();
    }
    if (map.contains("translation")) {
        translation = variantToVec3(map["translation"]);
    }
    if (map.contains("rotation")) {
        rotation = glm::quat(glm::radians(variantToVec3(map["rotation"])));
    }
    if (map.contains("scale")) {
        scale = map["scale"].toFloat();
    }
    if (map.contains("soft")) {
        isSoft = map["soft"].toBool();
    }
    return isValid;
}

QVariantList AvatarData::getAttachmentsVariant() const {
    QVariantList result;
    for (const auto& attachment : getAttachmentData()) {
        result.append(attachment.toVariant());
    }
    return result;
}

void AvatarData::setAttachmentsVariant(const QVariantList& variant) {
    QVector<AttachmentData> newAttachments;
    newAttachments.reserve(variant.size());
    for (const auto& attachmentVar : variant) {
        AttachmentData attachment;
        if (attachment.fromVariant(attachmentVar)) {
            newAttachments.append(attachment);
        }
    }
    setAttachmentData(newAttachments);
}

void AvatarData::storeAvatarEntityDataPayload(const QUuid& entityID, const QByteArray& data) {
    bool changed = false;
    _avatarEntitiesLock.withWriteLock([&] {
        auto itr = _packedAvatarEntityData.find(entityID);
        if (itr == _packedAvatarEntityData.end()) {
            if (_packedAvatarEntityData.size() < MAX_NUM_AVATAR_ENTITIES) {
                _packedAvatarEntityData.insert(entityID, data);
                changed = true;
            }
        } else {
            itr.value() = data;
            changed = true;
        }
    });

    if (changed) {
        _avatarEntityDataChanged = true;

        if (_clientTraitsHandler) {
            // we have a client traits handler, so we need to mark this instanced trait as changed
            // so that changes will be sent next frame
            _clientTraitsHandler->markInstancedTraitUpdated(AvatarTraits::AvatarEntity, entityID);
        }
    }
}

void AvatarData::updateAvatarEntity(const QUuid& entityID, const QByteArray& entityData) {
    // overridden where needed
    // expects 'entityData' to be a JavaScript EntityItemProperties Object in QByteArray form
}

void AvatarData::clearAvatarEntity(const QUuid& entityID, bool requiresRemovalFromTree) {
    // NOTE: requiresRemovalFromTree is unused
    clearAvatarEntityInternal(entityID);
}

void AvatarData::clearAvatarEntityInternal(const QUuid& entityID) {
    bool removedEntity = false;
    _avatarEntitiesLock.withWriteLock([this, &removedEntity, &entityID] {
        removedEntity = _packedAvatarEntityData.remove(entityID);
    });
    insertRemovedEntityID(entityID);
    if (removedEntity && _clientTraitsHandler) {
        // we have a client traits handler, so we need to mark this removed instance trait as deleted
        // so that changes are sent next frame
        _clientTraitsHandler->markInstancedTraitDeleted(AvatarTraits::AvatarEntity, entityID);
    }
}

void AvatarData::clearAvatarEntities() {
    QList<QUuid> avatarEntityIDs;
    _avatarEntitiesLock.withReadLock([&] {
        avatarEntityIDs = _packedAvatarEntityData.keys();
    });
    for (const auto& entityID : avatarEntityIDs) {
        clearAvatarEntityInternal(entityID);
    }
}

QList<QUuid> AvatarData::getAvatarEntityIDs() const {
    QList<QUuid> avatarEntityIDs;
    _avatarEntitiesLock.withReadLock([&] {
        avatarEntityIDs = _packedAvatarEntityData.keys();
    });
    return avatarEntityIDs;
}

AvatarEntityMap AvatarData::getAvatarEntityData() const {
    // overridden where needed
    // NOTE: the return value is expected to be a map of unfortunately-formatted-binary-blobs
    return AvatarEntityMap();
}

AvatarEntityMap AvatarData::getAvatarEntityDataNonDefault() const {
    // overridden where needed
    // NOTE: the return value is expected to be a map of unfortunately-formatted-binary-blobs
    return AvatarEntityMap();
}

void AvatarData::setAvatarEntityData(const AvatarEntityMap& avatarEntityData) {
    // overridden where needed
    // avatarEntityData is expected to be a map of QByteArrays
    // each QByteArray represents an EntityItemProperties object from JavaScript
}

void AvatarData::insertRemovedEntityID(const QUuid entityID) {
    _avatarEntitiesLock.withWriteLock([&] {
        _avatarEntityRemoved.insert(entityID);
    });
    _avatarEntityDataChanged = true;
}

AvatarEntityIDs AvatarData::getAndClearRecentlyRemovedIDs() {
    AvatarEntityIDs result;
    _avatarEntitiesLock.withWriteLock([&] {
        result = _avatarEntityRemoved;
        _avatarEntityRemoved.clear();
    });
    return result;
}

// thread-safe
glm::mat4 AvatarData::getSensorToWorldMatrix() const {
    return _sensorToWorldMatrixCache.get();
}

// thread-safe
float AvatarData::getSensorToWorldScale() const {
    return extractUniformScale(_sensorToWorldMatrixCache.get());
}

// thread-safe
glm::mat4 AvatarData::getControllerLeftHandMatrix() const {
    return _controllerLeftHandMatrixCache.get();
}

// thread-safe
glm::mat4 AvatarData::getControllerRightHandMatrix() const {
    return _controllerRightHandMatrixCache.get();
}

/*@jsdoc
 * Information about a ray-to-avatar intersection.
 * @typedef {object} RayToAvatarIntersectionResult
 * @property {boolean} intersects - <code>true</code> if an avatar is intersected, <code>false</code> if it isn't.
 * @property {string} avatarID - The ID of the avatar that is intersected.
 * @property {number} distance - The distance from the ray origin to the intersection.
 * @property {string} face - The name of the box face that is intersected; <code>"UNKNOWN_FACE"</code> if mesh was picked 
 *     against.
 * @property {Vec3} intersection - The ray intersection point in world coordinates.
 * @property {Vec3} surfaceNormal - The surface normal at the intersection point.
 * @property {number} jointIndex - The index of the joint intersected.
 * @property {SubmeshIntersection} extraInfo - Extra information on the mesh intersected if mesh was picked against, 
 *     <code>{}</code> if it wasn't.
 */
ScriptValuePointer RayToAvatarIntersectionResultToScriptValue(ScriptEngine* engine, const RayToAvatarIntersectionResult& value) {
    ScriptValuePointer obj = engine->newObject();
    obj->setProperty("intersects", value.intersects);
    ScriptValuePointer avatarIDValue = quuidToScriptValue(engine, value.avatarID);
    obj->setProperty("avatarID", avatarIDValue);
    obj->setProperty("distance", value.distance);
    obj->setProperty("face", boxFaceToString(value.face));
    ScriptValuePointer intersection = vec3ToScriptValue(engine, value.intersection);

    obj->setProperty("intersection", intersection);
    ScriptValuePointer surfaceNormal = vec3ToScriptValue(engine, value.surfaceNormal);
    obj->setProperty("surfaceNormal", surfaceNormal);
    obj->setProperty("jointIndex", value.jointIndex);
    obj->setProperty("extraInfo", engine->toScriptValue(value.extraInfo));
    return obj;
}

void RayToAvatarIntersectionResultFromScriptValue(const ScriptValuePointer& object, RayToAvatarIntersectionResult& value) {
    value.intersects = object->property("intersects")->toVariant().toBool();
    ScriptValuePointer avatarIDValue = object->property("avatarID");
    quuidFromScriptValue(avatarIDValue, value.avatarID);
    value.distance = object->property("distance")->toVariant().toFloat();
    value.face = boxFaceFromString(object->property("face")->toVariant().toString());

    ScriptValuePointer intersection = object->property("intersection");
    if (intersection->isValid()) {
        vec3FromScriptValue(intersection, value.intersection);
    }
    ScriptValuePointer surfaceNormal = object->property("surfaceNormal");
    if (surfaceNormal->isValid()) {
        vec3FromScriptValue(surfaceNormal, value.surfaceNormal);
    }
    value.jointIndex = object->property("jointIndex")->toInt32();
    value.extraInfo = object->property("extraInfo")->toVariant().toMap();
}

// these coefficients can be changed via JS for experimental tuning
// use AvatatManager.setAvatarSortCoefficient("name", value) by a user with domain kick-rights
float AvatarData::_avatarSortCoefficientSize { 8.0f };
float AvatarData::_avatarSortCoefficientCenter { 0.25f };
float AvatarData::_avatarSortCoefficientAge { 1.0f };

/*@jsdoc
 * An object with the UUIDs of avatar entities as keys and avatar entity properties objects as values.
 * @typedef {Object.<Uuid, Entities.EntityProperties>} AvatarEntityMap
 */
ScriptValuePointer AvatarEntityMapToScriptValue(ScriptEngine* engine, const AvatarEntityMap& value) {
    ScriptValuePointer obj = engine->newObject();
    for (auto entityID : value.keys()) {
        QByteArray entityProperties = value.value(entityID);
        QJsonDocument jsonEntityProperties = QJsonDocument::fromBinaryData(entityProperties);
        if (!jsonEntityProperties.isObject()) {
            qCDebug(avatars) << "bad AvatarEntityData in AvatarEntityMap" << QString(entityProperties.toHex());
        }

        QVariant variantEntityProperties = jsonEntityProperties.toVariant();
        QVariantMap entityPropertiesMap = variantEntityProperties.toMap();
        ScriptValuePointer scriptEntityProperties = variantMapToScriptValue(entityPropertiesMap, *engine);

        QString key = entityID.toString();
        obj->setProperty(key, scriptEntityProperties);
    }
    return obj;
}

void AvatarEntityMapFromScriptValue(const ScriptValuePointer& object, AvatarEntityMap& value) {
    ScriptValueIteratorPointer itr(object->newIterator());
    while (itr->hasNext()) {
        itr->next();
        QUuid EntityID = QUuid(itr->name());

        ScriptValuePointer scriptEntityProperties = itr->value();
        QVariant variantEntityProperties = scriptEntityProperties->toVariant();
        QJsonDocument jsonEntityProperties = QJsonDocument::fromVariant(variantEntityProperties);
        QByteArray binaryEntityProperties = jsonEntityProperties.toBinaryData();

        value[EntityID] = binaryEntityProperties;
    }
}

const float AvatarData::DEFAULT_BUBBLE_SCALE = 2.4f; // magic number determined empirically

AABox AvatarData::computeBubbleBox(float bubbleScale) const {
    AABox box = AABox(_globalBoundingBoxOffset - _globalBoundingBoxDimensions, _globalBoundingBoxDimensions);
    glm::vec3 size = box.getScale();
    size *= bubbleScale;
    const glm::vec3 MIN_BUBBLE_SCALE(0.3f, 1.3f, 0.3);
    size= glm::max(size, MIN_BUBBLE_SCALE);
    box.setScaleStayCentered(size);
    return box;
}

void AvatarData::setSkeletonData(const std::vector<AvatarSkeletonTrait::UnpackedJointData>& skeletonData) {
    _avatarSkeletonDataLock.withWriteLock([&] {
        _avatarSkeletonData = skeletonData;
    });
}

std::vector<AvatarSkeletonTrait::UnpackedJointData> AvatarData::getSkeletonData() const {
    std::vector<AvatarSkeletonTrait::UnpackedJointData> skeletonData;
    _avatarSkeletonDataLock.withReadLock([&] {
        skeletonData = _avatarSkeletonData;
    });
    return skeletonData;
}

void AvatarData::sendSkeletonData() const{
    if (_clientTraitsHandler) {
        _clientTraitsHandler->markTraitUpdated(AvatarTraits::SkeletonData);
    }
}

AABox AvatarData::getDefaultBubbleBox() const {
    AABox bubbleBox(_defaultBubbleBox);
    bubbleBox.translate(_globalPosition);
    return bubbleBox;
}

bool AvatarData::updateAvatarGrabData(const QUuid& grabID, const QByteArray& grabData) {
    bool changed { false };
    _avatarGrabsLock.withWriteLock([&] {
        AvatarGrabDataMap::iterator itr = _avatarGrabData.find(grabID);
        if (itr == _avatarGrabData.end()) {
            // create a new one
            if (_avatarGrabData.size() < MAX_NUM_AVATAR_GRABS) {
                _avatarGrabData.insert(grabID, grabData);
                _avatarGrabDataChanged = true;
                changed = true;
            } else {
                qCWarning(avatars) << "Can't create more grabs on avatar, limit reached.";
            }
        } else {
            // update an existing one
            if (itr.value() != grabData) {
                itr.value() = grabData;
                _avatarGrabDataChanged = true;
                changed = true;
            }
        }
    });

    return changed;
}

void AvatarData::clearAvatarGrabData(const QUuid& grabID) {
    _avatarGrabsLock.withWriteLock([&] {
        if (_avatarGrabData.remove(grabID)) {
            _avatarGrabDataChanged = true;
        }
    });
}

glm::vec3 AvatarData::getHeadJointFrontVector() const {
    int headJointIndex = getJointIndex("Head");
    glm::quat headJointRotation = Quaternions::Y_180 * getAbsoluteJointRotationInObjectFrame(headJointIndex);//    getAbsoluteJointRotationInRigFrame(headJointIndex, headJointRotation);
    headJointRotation = getWorldOrientation() * headJointRotation;
    float headYaw = safeEulerAngles(headJointRotation).y;
    glm::quat headYawRotation = glm::angleAxis(headYaw, Vectors::UP);
    return headYawRotation * IDENTITY_FORWARD;
}
