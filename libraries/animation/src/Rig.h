//
//  Rig.h
//  libraries/animation/src/
//
//  Produces animation data and hip placement for the current timestamp.
//
//  Created by Howard Stearns, Seth Alves, Anthony Thibault, Andrew Meadows on 7/15/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef __hifi__Rig__
#define __hifi__Rig__

#include <QObject>
#include <QMutex>
#include <vector>
#include <JointData.h>
#include <QReadWriteLock>
#include <QtCore/QSharedPointer>

#include "AnimNode.h"
#include "AnimNodeLoader.h"
#include "SimpleMovingAverage.h"
#include "AnimUtil.h"
#include "Flow.h"
#include "AvatarConstants.h"

class Rig;
class AnimInverseKinematics;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

// Rig instances are reentrant.
// However only specific methods thread-safe.  Noted below.

class Rig : public QObject {
    Q_OBJECT
public:
    struct StateHandler {
        AnimVariantMap results;
        QStringList propertyNames;
        ScriptValuePointer function;
        bool useNames;
    };

    enum PrimaryControllerType {
        PrimaryControllerType_Head = 0,
        PrimaryControllerType_LeftHand,
        PrimaryControllerType_RightHand,
        PrimaryControllerType_Hips,
        PrimaryControllerType_LeftFoot,
        PrimaryControllerType_RightFoot,
        PrimaryControllerType_Spine2,
        NumPrimaryControllerTypes
    };

    // NOTE: These should ordered such that joint parents appear before their children.
    enum SecondaryControllerType {
        SecondaryControllerType_LeftShoulder = 0,
        SecondaryControllerType_RightShoulder,
        SecondaryControllerType_LeftArm,
        SecondaryControllerType_RightArm,
        SecondaryControllerType_LeftForeArm,
        SecondaryControllerType_RightForeArm,
        SecondaryControllerType_LeftUpLeg,
        SecondaryControllerType_RightUpLeg,
        SecondaryControllerType_LeftLeg,
        SecondaryControllerType_RightLeg,
        SecondaryControllerType_LeftToeBase,
        SecondaryControllerType_RightToeBase,
        NumSecondaryControllerTypes
    };

    enum class ControllerFlags : uint8_t {
        Enabled = 0x01,
        Estimated = 0x02
    };

    struct ControllerParameters {
        ControllerParameters() {
            memset(primaryControllerFlags, 0, NumPrimaryControllerTypes);
            memset(secondaryControllerFlags, 0, NumPrimaryControllerTypes);
        }
        glm::mat4 rigToSensorMatrix;
        AnimPose primaryControllerPoses[NumPrimaryControllerTypes];  // rig space
        uint8_t primaryControllerFlags[NumPrimaryControllerTypes];
        AnimPose secondaryControllerPoses[NumSecondaryControllerTypes];  // rig space
        uint8_t secondaryControllerFlags[NumSecondaryControllerTypes];
        bool isTalking;
        float inputX = 0.0f;
        float inputZ = 0.0f;
        bool reactionEnabledFlags[NUM_AVATAR_BEGIN_END_REACTIONS];
        bool reactionTriggers[NUM_AVATAR_TRIGGER_REACTIONS];
        HFMJointShapeInfo hipsShapeInfo;
        HFMJointShapeInfo spineShapeInfo;
        HFMJointShapeInfo spine1ShapeInfo;
        HFMJointShapeInfo spine2ShapeInfo;
    };

    struct EyeParameters {
        glm::vec3 eyeLookAt = glm::vec3();  // world space
        glm::vec3 eyeSaccade = glm::vec3(); // world space
        glm::vec3 modelTranslation = glm::vec3();
        glm::quat modelRotation = glm::quat();
        int leftEyeJointIndex = -1;
        int rightEyeJointIndex = -1;
    };

    enum class CharacterControllerState {
        Ground = 0,
        Takeoff,
        InAir,
        Hover,
        Seated
    };

    Rig();
    virtual ~Rig();

    void destroyAnimGraph();

    void overrideAnimation(const QString& url, float fps, bool loop, float firstFrame, float lastFrame);
    bool isPlayingOverrideAnimation() const { return _userAnimState.clipNodeEnum != UserAnimState::None; };
    void restoreAnimation();
    
    void overrideHandAnimation(bool isLeft, const QString& url, float fps, bool loop, float firstFrame, float lastFrame);
    void restoreHandAnimation(bool isLeft);

    void overrideNetworkAnimation(const QString& url, float fps, bool loop, float firstFrame, float lastFrame);
    void triggerNetworkRole(const QString& role);
    void restoreNetworkAnimation();

    QStringList getAnimationRoles() const;
    void overrideRoleAnimation(const QString& role, const QString& url, float fps, bool loop, float firstFrame, float lastFrame);
    void restoreRoleAnimation(const QString& role);

    void initJointStates(const HFMModel& hfmModel, const glm::mat4& modelOffset);
    void reset(const HFMModel& hfmModel);
    bool jointStatesEmpty() const;
    int getJointStateCount() const;
    int indexOfJoint(const QString& jointName) const;
    QString nameOfJoint(int jointIndex) const;

    void setModelOffset(const glm::mat4& modelOffsetMat);

    void clearJointState(int index);
    void clearJointStates();
    void clearJointAnimationPriority(int index);

    std::shared_ptr<AnimInverseKinematics> getAnimInverseKinematicsNode() const;

    void clearIKJointLimitHistory();
    void setMaxHipsOffsetLength(float maxLength);
    float getMaxHipsOffsetLength() const;

    float getIKErrorOnLastSolve() const;

    int getJointParentIndex(int childIndex) const;

    // geometry space
    void setJointState(int index, bool valid, const glm::quat& rotation, const glm::vec3& translation, float priority);

    // geometry space
    void setJointTranslation(int index, bool valid, const glm::vec3& translation, float priority);
    void setJointRotation(int index, bool valid, const glm::quat& rotation, float priority);

    bool getIsJointOverridden(int jointIndex) const;

    // if translation and rotation is identity, position will be in rig space
    bool getJointPositionInWorldFrame(int jointIndex, glm::vec3& position,
                                      glm::vec3 translation, glm::quat rotation) const;

    // rig space
    bool getJointPosition(int jointIndex, glm::vec3& position) const;

    // if rotation is identity, result will be in rig space
    bool getJointRotationInWorldFrame(int jointIndex, glm::quat& result, const glm::quat& rotation) const;

    // geometry space (thread-safe)
    bool getJointRotation(int jointIndex, glm::quat& rotation) const;
    bool getJointTranslation(int jointIndex, glm::vec3& translation) const;

    // rig space (thread-safe)
    bool getAbsoluteJointRotationInRigFrame(int jointIndex, glm::quat& rotation) const;
    bool getAbsoluteJointTranslationInRigFrame(int jointIndex, glm::vec3& translation) const;
    bool getAbsoluteJointPoseInRigFrame(int jointIndex, AnimPose& returnPose) const;

    // rig space
    glm::mat4 getJointTransform(int jointIndex) const;
    AnimPose getJointPose(int jointIndex) const;

    // Start or stop animations as needed.
    void computeMotionAnimationState(float deltaTime, const glm::vec3& worldPosition, const glm::vec3& worldVelocity,
                                     const glm::quat& worldRotation, CharacterControllerState ccState, float sensorToWorldScale);

    // Regardless of who started the animations or how many, update the joints.
    void updateAnimations(float deltaTime, const glm::mat4& rootTransform, const glm::mat4& rigToWorldTransform);

    void updateFromControllerParameters(const ControllerParameters& params, float dt);
    void updateFromEyeParameters(const EyeParameters& params);

    void initAnimGraph(const QUrl& url);

    AnimNode::ConstPointer getAnimNode() const { return _animNode; }
    AnimNode::ConstPointer findAnimNodeByName(const QString& name) const;
    AnimSkeleton::ConstPointer getAnimSkeleton() const { return _animSkeleton; }
    ScriptValuePointer addAnimationStateHandler(ScriptValuePointer handler, ScriptValuePointer propertiesList);
    void removeAnimationStateHandler(ScriptValuePointer handler);
    void animationStateHandlerResult(int identifier, ScriptValuePointer result);

    // rig space
    bool getModelRegistrationPoint(glm::vec3& modelRegistrationPointOut) const;

    // rig space
    AnimPose getAbsoluteDefaultPose(int index) const;

    // rig space
    const AnimPoseVec& getAbsoluteDefaultPoses() const;

    // geometry space
    bool getRelativeDefaultJointRotation(int index, glm::quat& rotationOut) const;
    bool getRelativeDefaultJointTranslation(int index, glm::vec3& translationOut) const;

    void copyJointsIntoJointData(QVector<JointData>& jointDataVec) const;
    void copyJointsFromJointData(const QVector<JointData>& jointDataVec);
    void computeExternalPoses(const glm::mat4& modelOffsetMat);

    void computeAvatarBoundingCapsule(const HFMModel& hfmModel, float& radiusOut, float& heightOut, glm::vec3& offsetOut) const;

    void setEnableInverseKinematics(bool enable);
    void setEnableAnimations(bool enable);

    const glm::mat4& getGeometryToRigTransform() const { return _geometryToRigTransform; }

    const AnimPose& getModelOffsetPose() const { return _modelOffset; }
    const AnimPose& getGeometryOffsetPose() const { return _geometryOffset; }

    void setEnableDebugDrawIKTargets(bool enableDebugDrawIKTargets) { _enableDebugDrawIKTargets = enableDebugDrawIKTargets; }
    void setEnableDebugDrawIKConstraints(bool enableDebugDrawIKConstraints) { _enableDebugDrawIKConstraints = enableDebugDrawIKConstraints; }
    void setEnableDebugDrawIKChains(bool enableDebugDrawIKChains) { _enableDebugDrawIKChains = enableDebugDrawIKChains; }

    // input assumed to be in rig space
    void computeHeadFromHMD(const AnimPose& hmdPose, glm::vec3& headPositionOut, glm::quat& headOrientationOut) const;

    // used to debug animation playback
    const AnimContext::DebugAlphaMap& getDebugAlphaMap() const { return _lastContext.getDebugAlphaMap(); }
    const AnimVariantMap& getAnimVars() const { return _lastAnimVars; }
    const AnimContext::DebugStateMachineMap& getStateMachineMap() const { return _lastContext.getStateMachineMap(); }
    void initFlow(bool isActive);
    Flow& getFlow() { return _internalFlow; }

    float getUnscaledEyeHeight() const;
    float getUnscaledHipsHeight() const;
    void buildAbsoluteRigPoses(const AnimPoseVec& relativePoses, AnimPoseVec& absolutePosesOut) const;

    int getOverrideJointCount() const;
    bool getFlowActive() const;
    bool getNetworkGraphActive() const;
    void setDirectionalBlending(const QString& targetName, const glm::vec3& blendingTarget, const QString& alphaName, float alpha);

signals:
    void onLoadComplete();
    void onLoadFailed();

protected:
    bool isIndexValid(int index) const { return _animSkeleton && index >= 0 && index < _animSkeleton->getNumJoints(); }
    void updateAnimationStateHandlers();
    void applyOverridePoses();

    void updateHead(bool headEnabled, bool hipsEnabled, const AnimPose& headMatrix);
    void updateHands(bool leftHandEnabled, bool rightHandEnabled, bool hipsEnabled, bool hipsEstimated,
                     bool leftArmEnabled, bool rightArmEnabled, bool headEnabled, float dt,
                     const AnimPose& leftHandPose, const AnimPose& rightHandPose,
                     const HFMJointShapeInfo& hipsShapeInfo, const HFMJointShapeInfo& spineShapeInfo,
                     const HFMJointShapeInfo& spine1ShapeInfo, const HFMJointShapeInfo& spine2ShapeInfo,
                     const glm::mat4& rigToSensorMatrix, const glm::mat4& sensorToRigMatrix);
    void updateFeet(bool leftFootEnabled, bool rightFootEnabled, bool headEnabled,
                    const AnimPose& leftFootPose, const AnimPose& rightFootPose,
                    const glm::mat4& rigToSensorMatrix, const glm::mat4& sensorToRigMatrix);
    void updateReactions(const ControllerParameters& params);

    void updateEyeJoint(int index, const glm::vec3& modelTranslation, const glm::quat& modelRotation, const glm::vec3& lookAt, const glm::vec3& saccade);
    void calcAnimAlpha(float speed, const std::vector<float>& referenceSpeeds, float* alphaOut) const;

    bool calculateElbowPoleVector(int handIndex, int elbowIndex, int armIndex, int oppositeArmIndex, glm::vec3& poleVector) const;
    glm::vec3 calculateKneePoleVector(int footJointIndex, int kneeJoint, int upLegIndex, int hipsIndex, const AnimPose& targetFootPose) const;
    glm::vec3 deflectHandFromTorso(const glm::vec3& handPosition, const HFMJointShapeInfo& hipsShapeInfo, const HFMJointShapeInfo& spineShapeInfo,
                                   const HFMJointShapeInfo& spine1ShapeInfo, const HFMJointShapeInfo& spine2ShapeInfo) const;

    // Get the scale factor to convert distances in the geometry frame into the unscaled rig frame.
    float GetScaleFactorGeometryToUnscaledRig() const;

    // The ground plane Y position in geometry space.
    static constexpr float GEOMETRY_GROUND_Y = 0.0f;

    AnimPose _modelOffset;  // model to rig space
    AnimPose _geometryOffset; // geometry to model space (includes unit offset & fst offsets)
    AnimPose _invGeometryOffset;

    struct PoseSet {
        AnimPoseVec _relativePoses; // geometry space relative to parent.
        AnimPoseVec _absolutePoses; // rig space, not relative to parent.
        AnimPoseVec _overridePoses; // geometry space relative to parent.
        std::vector<bool> _overrideFlags;
    };

    // Only accessed by the main thread
    PoseSet _internalPoseSet;
    PoseSet _networkPoseSet;

    // Copy of the _poseSet for external threads.
    PoseSet _externalPoseSet;
    mutable QReadWriteLock _externalPoseSetLock;

    AnimPoseVec _absoluteDefaultPoses; // rig space, not relative to parent.

    glm::mat4 _geometryToRigTransform;
    glm::mat4 _rigToGeometryTransform;

    int _rootJointIndex { -1 };

    int _leftEyeJointIndex { -1 };
    int _rightEyeJointIndex { -1 };
    std::vector<int> _leftEyeJointChildren;
    std::vector<int> _rightEyeJointChildren;

    int _leftHandJointIndex { -1 };
    int _leftElbowJointIndex { -1 };
    int _leftShoulderJointIndex { -1 };

    int _rightHandJointIndex { -1 };
    int _rightElbowJointIndex { -1 };
    int _rightShoulderJointIndex { -1 };

    glm::vec3 _lastForward;
    glm::vec3 _lastPosition;
    glm::vec3 _lastVelocity;
    bool _isMovingWithMomentum{ false };

    QUrl _animGraphURL;
    std::shared_ptr<AnimNode> _animNode;
    std::shared_ptr<AnimNode> _networkNode;
    std::shared_ptr<AnimSkeleton> _animSkeleton;
    std::unique_ptr<AnimNodeLoader> _animLoader;
    std::unique_ptr<AnimNodeLoader> _networkLoader;
    AnimVariantMap _animVars;
    AnimVariantMap _networkVars;

    enum class RigRole {
        Idle = 0,
        Turn,
        Move,
        Hover,
        Takeoff,
        InAir,
        Seated
    };
    RigRole _state { RigRole::Idle };
    RigRole _desiredState { RigRole::Idle };
    float _desiredStateAge { 0.0f };

    struct NetworkAnimState {
        enum ClipNodeEnum {
            None = 0,
            PreTransit,
            Transit,
            PostTransit,
            A,
            B
        };
        NetworkAnimState() : clipNodeEnum(NetworkAnimState::None), fps(30.0f), loop(false), firstFrame(0.0f), lastFrame(0.0f), blendTime(FLT_MAX) {}
        NetworkAnimState(ClipNodeEnum clipNodeEnumIn, const QString& urlIn, float fpsIn, bool loopIn, float firstFrameIn, float lastFrameIn) :
            clipNodeEnum(clipNodeEnumIn), url(urlIn), fps(fpsIn), loop(loopIn), firstFrame(firstFrameIn), lastFrame(lastFrameIn) {}

        ClipNodeEnum clipNodeEnum;
        QString url;
        float fps;
        bool loop;
        float firstFrame;
        float lastFrame;
        float blendTime;
    };

    struct HandAnimState {
        enum ClipNodeEnum {
            None = 0,
            A,
            B
        };

        HandAnimState() : clipNodeEnum(HandAnimState::None) {}
        HandAnimState(ClipNodeEnum clipNodeEnumIn, const QString& urlIn, float fpsIn, bool loopIn, float firstFrameIn, float lastFrameIn) :
            clipNodeEnum(clipNodeEnumIn), url(urlIn), fps(fpsIn), loop(loopIn), firstFrame(firstFrameIn), lastFrame(lastFrameIn) {
        }


        ClipNodeEnum clipNodeEnum;
        QString url;
        float fps;
        bool loop;
        float firstFrame;
        float lastFrame;
    };

    struct UserAnimState {
        enum ClipNodeEnum {
            None = 0,
            A,
            B
        };

        UserAnimState() : clipNodeEnum(UserAnimState::None) {}
        UserAnimState(ClipNodeEnum clipNodeEnumIn, const QString& urlIn, float fpsIn, bool loopIn, float firstFrameIn, float lastFrameIn) :
            clipNodeEnum(clipNodeEnumIn), url(urlIn), fps(fpsIn), loop(loopIn), firstFrame(firstFrameIn), lastFrame(lastFrameIn) {}

        ClipNodeEnum clipNodeEnum;
        QString url;
        float fps;
        bool loop;
        float firstFrame;
        float lastFrame;
    };

    struct RoleAnimState {
       RoleAnimState() {}
       RoleAnimState(const QString& roleId, const QString& urlIn, float fpsIn, bool loopIn, float firstFrameIn, float lastFrameIn) :
            role(roleId), url(urlIn), fps(fpsIn), loop(loopIn), firstFrame(firstFrameIn), lastFrame(lastFrameIn) {}

        QString role;
        QString url;
        float fps;
        bool loop;
        float firstFrame;
        float lastFrame;
    };

    UserAnimState _userAnimState;
    NetworkAnimState _networkAnimState;
    HandAnimState _rightHandAnimState;
    HandAnimState _leftHandAnimState;
    std::map<QString, RoleAnimState> _roleAnimStates;
    int _evaluationCount{ 0 };

    float _leftHandOverlayAlpha { 0.0f };
    float _rightHandOverlayAlpha { 0.0f };
    float _talkIdleInterpTime { 0.0f };
    bool _previousIsTalking { false };

    SimpleMovingAverage _averageForwardSpeed { 10 };
    SimpleMovingAverage _averageLateralSpeed { 10 };

    std::map<QString, AnimNode::Pointer> _origRoleAnimations;

    int32_t _numOverrides { 0 };
    bool _lastEnableInverseKinematics { true };
    bool _enableInverseKinematics { true };
    bool _enabledAnimations { true };

    mutable uint32_t _jointNameWarningCount { 0 };

    bool _enableDebugDrawIKTargets { false };
    bool _enableDebugDrawIKConstraints { false };
    bool _enableDebugDrawIKChains { false };

    QMap<int, StateHandler> _stateHandlers;
    int _nextStateHandlerId { 0 };
    QMutex _stateMutex;

    glm::vec3 _prevRightFootPoleVector { Vectors::UNIT_Z }; // sensor space
    bool _prevRightFootPoleVectorValid { false };

    glm::vec3 _prevLeftFootPoleVector { Vectors::UNIT_Z }; // sensor space
    bool _prevLeftFootPoleVectorValid { false };

    int _rigId;
    bool _headEnabled { false };
    bool _computeNetworkAnimation { false };
    bool _sendNetworkNode { false };

    AnimContext _lastContext;
    AnimVariantMap _lastAnimVars;

    SnapshotBlendPoseHelper _hipsBlendHelper;
    ControllerParameters _previousControllerParameters;
    Flow _internalFlow;
    Flow _networkFlow;
};

#endif /* defined(__hifi__Rig__) */
