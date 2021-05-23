//
//  AvatarManager.cpp
//  interface/src/avatar
//
//  Created by Stephen Birarda on 1/23/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AvatarManager.h"

#include <string>

#include <ScriptEngine.h>
#include <ScriptValue.h>

#include "AvatarLogging.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#include <glm/gtx/string_cast.hpp>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <shared/QtHelpers.h>
#include <AvatarData.h>
#include <PerfStat.h>
#include <PrioritySortUtil.h>
#include <RegisteredMetaTypes.h>
#include <Rig.h>
#include <SettingHandle.h>
#include <UsersScriptingInterface.h>
#include <UUID.h>
#include <shared/ConicalViewFrustum.h>
#include <ui/AvatarInputs.h>

#include "Application.h"
#include "InterfaceLogging.h"
#include "Menu.h"
#include "MyAvatar.h"
#include "DebugDraw.h"
#include "SceneScriptingInterface.h"
#include "ui/AvatarCertifyBanner.h"

// 50 times per second - target is 45hz, but this helps account for any small deviations
// in the update loop - this also results in ~30hz when in desktop mode which is essentially
// what we want

// We add _myAvatar into the hash with all the other AvatarData, and we use the default NULL QUid as the key.
const QUuid MY_AVATAR_KEY;  // NULL key

AvatarManager::AvatarManager(QObject* parent) :
    _myAvatar(new MyAvatar(qApp->thread()), [](MyAvatar* ptr) { ptr->deleteLater(); })
{
    // register a meta type for the weak pointer we'll use for the owning avatar mixer for each avatar
    qRegisterMetaType<QWeakPointer<Node> >("NodeWeakPointer");

    auto nodeList = DependencyManager::get<NodeList>();

    // when we hear that the user has ignored an avatar by session UUID
    // immediately remove that avatar instead of waiting for the absence of packets from avatar mixer
    connect(nodeList.data(), &NodeList::ignoredNode, this, [this](const QUuid& nodeID, bool enabled) {
        if (enabled) {
            removeAvatar(nodeID, KillAvatarReason::AvatarIgnored);
        } else {
            auto avatar = std::static_pointer_cast<Avatar>(getAvatarBySessionID(nodeID));
            if (avatar) {
                avatar->createOrb();
            }
        }
    });

    _transitConfig._totalFrames = AVATAR_TRANSIT_FRAME_COUNT;
    _transitConfig._minTriggerDistance = AVATAR_TRANSIT_MIN_TRIGGER_DISTANCE;
    _transitConfig._maxTriggerDistance = AVATAR_TRANSIT_MAX_TRIGGER_DISTANCE;
    _transitConfig._framesPerMeter = AVATAR_TRANSIT_FRAMES_PER_METER;
    _transitConfig._isDistanceBased = AVATAR_TRANSIT_DISTANCE_BASED;
    _transitConfig._abortDistance = AVATAR_TRANSIT_ABORT_DISTANCE;
}

AvatarSharedPointer AvatarManager::addAvatar(const QUuid& sessionUUID, const QWeakPointer<Node>& mixerWeakPointer) {
    AvatarSharedPointer avatar = AvatarHashMap::addAvatar(sessionUUID, mixerWeakPointer);
    const auto otherAvatar = std::static_pointer_cast<OtherAvatar>(avatar);
    if (otherAvatar && _space) {
        std::unique_lock<std::mutex> lock(_spaceLock);
        auto spaceIndex = _space->allocateID();
        otherAvatar->setSpaceIndex(spaceIndex);
        workload::Sphere sphere(otherAvatar->getWorldPosition(), otherAvatar->getBoundingRadius());
        workload::Transaction transaction;
        SpatiallyNestablePointer nestable = std::static_pointer_cast<SpatiallyNestable>(otherAvatar);
        transaction.reset(spaceIndex, sphere, workload::Owner(nestable));
        _space->enqueueTransaction(transaction);
    }
    return avatar;
}

AvatarManager::~AvatarManager() {
    assert(_otherAvatarsToChangeInPhysics.empty());
}

void AvatarManager::init() {
    _myAvatar->init();
    {
        QWriteLocker locker(&_hashLock);
        _avatarHash.insert(MY_AVATAR_KEY, _myAvatar);
    }

    _shouldRender = DependencyManager::get<SceneScriptingInterface>()->shouldRenderAvatars();
    connect(DependencyManager::get<SceneScriptingInterface>().data(), &SceneScriptingInterface::shouldRenderAvatarsChanged,
            this, &AvatarManager::updateAvatarRenderStatus, Qt::QueuedConnection);

    if (_shouldRender) {
        const render::ScenePointer& scene = qApp->getMain3DScene();
        render::Transaction transaction;
        _myAvatar->addToScene(_myAvatar, scene, transaction);
        scene->enqueueTransaction(transaction);
    }

    setEnableDebugDrawOtherSkeletons(Menu::getInstance()->isOptionChecked(MenuOption::AnimDebugDrawOtherSkeletons));
}

void AvatarManager::setSpace(workload::SpacePointer& space ) {
    assert(!_space);
    _space = space;
}

void AvatarManager::handleTransitAnimations(AvatarTransit::Status status) {
    switch (status) {
        case AvatarTransit::Status::STARTED:
            _myAvatar->getSkeletonModel()->getRig().triggerNetworkRole("preTransitAnim");
            break;
        case AvatarTransit::Status::START_TRANSIT:
            _myAvatar->getSkeletonModel()->getRig().triggerNetworkRole("transitAnim");
            break;
        case AvatarTransit::Status::END_TRANSIT:
            _myAvatar->getSkeletonModel()->getRig().triggerNetworkRole("postTransitAnim");
            break;
        case AvatarTransit::Status::ENDED:
            _myAvatar->getSkeletonModel()->getRig().triggerNetworkRole("idleAnim");
            break;
        case AvatarTransit::Status::PRE_TRANSIT:
            break;
        case AvatarTransit::Status::POST_TRANSIT:
            break;
        case AvatarTransit::Status::IDLE:
            break;
        case AvatarTransit::Status::TRANSITING:
            break;
        case AvatarTransit::Status::ABORT_TRANSIT:
            break;
    }
}

void AvatarManager::updateMyAvatar(float deltaTime) {
    bool showWarnings = Menu::getInstance()->isOptionChecked(MenuOption::PipelineWarnings);
    PerformanceWarning warn(showWarnings, "AvatarManager::updateMyAvatar()");

    AvatarTransit::Status status = _myAvatar->updateTransit(deltaTime, _myAvatar->getNextPosition(), _myAvatar->getSensorToWorldScale(), _transitConfig);
    handleTransitAnimations(status);

    _myAvatar->update(deltaTime);
    render::Transaction transaction;
    _myAvatar->updateRenderItem(transaction);
    qApp->getMain3DScene()->enqueueTransaction(transaction);

    quint64 now = usecTimestampNow();
    quint64 dt = now - _lastSendAvatarDataTime;

    if (dt > MIN_TIME_BETWEEN_MY_AVATAR_DATA_SENDS && !_myAvatarDataPacketsPaused) {
        // send head/hand data to the avatar mixer and voxel server
        PerformanceTimer perfTimer("send"); 
        _myAvatar->sendAvatarDataPacket();
        _lastSendAvatarDataTime = now;
        _myAvatarSendRate.increment();
    }

    static AvatarCertifyBanner theftBanner;
    if (_myAvatar->isCertifyFailed()) {
        theftBanner.show(_myAvatar->getSessionUUID());
    } else {
        theftBanner.clear();
    }
}


Q_LOGGING_CATEGORY(trace_simulation_avatar, "trace.simulation.avatar");

float AvatarManager::getAvatarDataRate(const QUuid& sessionID, const QString& rateName) const {
    auto avatar = getAvatarBySessionID(sessionID);
    return avatar ? avatar->getDataRate(rateName) : 0.0f;
}

void AvatarManager::setMyAvatarDataPacketsPaused(bool pause) {
    if (_myAvatarDataPacketsPaused != pause) {
        _myAvatarDataPacketsPaused = pause;

        if (!_myAvatarDataPacketsPaused) {
            _myAvatar->sendAvatarDataPacket(true);
        }
    }
}

float AvatarManager::getAvatarUpdateRate(const QUuid& sessionID, const QString& rateName) const {
    auto avatar = getAvatarBySessionID(sessionID);
    return avatar ? avatar->getUpdateRate(rateName) : 0.0f;
}

float AvatarManager::getAvatarSimulationRate(const QUuid& sessionID, const QString& rateName) const {
    auto avatar = std::static_pointer_cast<Avatar>(getAvatarBySessionID(sessionID));
    return avatar ? avatar->getSimulationRate(rateName) : 0.0f;
}

void AvatarManager::updateOtherAvatars(float deltaTime) {
    {
        // lock the hash for read to check the size
        QReadLocker lock(&_hashLock);
        if (_avatarHash.size() < 2) {
            return;
        }
    }

    PerformanceTimer perfTimer("otherAvatars");

    class SortableAvatar: public PrioritySortUtil::Sortable {
    public:
        SortableAvatar() = delete;
        SortableAvatar(const std::shared_ptr<Avatar>& avatar) : _avatar(avatar) {}
        glm::vec3 getPosition() const override { return _avatar->getWorldPosition(); }
        float getRadius() const override { return _avatar->getBoundingRadius(); }
        uint64_t getTimestamp() const override { return _avatar->getLastRenderUpdateTime(); }
        std::shared_ptr<Avatar> getAvatar() const { return _avatar; }
    private:
        std::shared_ptr<Avatar> _avatar;
    };

    auto avatarMap = getHashCopy();

    const auto& views = qApp->getConicalViews();
    // Prepare 2 queues for heros and for crowd avatars
    using AvatarPriorityQueue = PrioritySortUtil::PriorityQueue<SortableAvatar>;
    // Keep two independent queues, one for heroes and one for the riff-raff.
    enum PriorityVariants
    {
        kHero = 0,
        kNonHero,
        NumVariants
    };
    AvatarPriorityQueue avatarPriorityQueues[NumVariants] = {
             { views,
               AvatarData::_avatarSortCoefficientSize,
               AvatarData::_avatarSortCoefficientCenter,
               AvatarData::_avatarSortCoefficientAge },
             { views,
               AvatarData::_avatarSortCoefficientSize,
               AvatarData::_avatarSortCoefficientCenter,
               AvatarData::_avatarSortCoefficientAge } };
    // Reserve space
    //avatarPriorityQueues[kHero].reserve(10);  // just few
    avatarPriorityQueues[kNonHero].reserve(avatarMap.size() - 1);  // don't include MyAvatar

    // Build vector and compute priorities
    auto nodeList = DependencyManager::get<NodeList>();
    AvatarHash::iterator itr = avatarMap.begin();
    while (itr != avatarMap.end()) {
        auto avatar = std::static_pointer_cast<Avatar>(*itr);
        // DO NOT update _myAvatar!  Its update has already been done earlier in the main loop.
        // DO NOT update or fade out uninitialized Avatars
        if (avatar != _myAvatar && avatar->isInitialized() && !nodeList->isPersonalMutingNode(avatar->getID())) {
            if (avatar->getHasPriority()) {
                avatarPriorityQueues[kHero].push(SortableAvatar(avatar));
            } else {
                avatarPriorityQueues[kNonHero].push(SortableAvatar(avatar));
            }
        }
        ++itr;
    }

    _numHeroAvatars = (int)avatarPriorityQueues[kHero].size();

    // process in sorted order
    uint64_t startTime = usecTimestampNow();

    const uint64_t MAX_UPDATE_HEROS_TIME_BUDGET = uint64_t(0.8 * MAX_UPDATE_AVATARS_TIME_BUDGET);

    uint64_t updatePriorityExpiries[NumVariants] = { startTime + MAX_UPDATE_HEROS_TIME_BUDGET, startTime + MAX_UPDATE_AVATARS_TIME_BUDGET };
    int numHerosUpdated = 0;
    int numAvatarsUpdated = 0;
    int numAvatarsNotUpdated = 0;

    render::Transaction renderTransaction;
    workload::Transaction workloadTransaction;

    for (int p = kHero; p < NumVariants; p++) {
        auto& priorityQueue = avatarPriorityQueues[p];
        // Sorting the current queue HERE as part of the measured timing.
        const auto& sortedAvatarVector = priorityQueue.getSortedVector();

        auto passExpiry = updatePriorityExpiries[p];

        for (auto it = sortedAvatarVector.begin(); it != sortedAvatarVector.end(); ++it) {
            const SortableAvatar& sortData = *it;
            const auto avatar = std::static_pointer_cast<OtherAvatar>(sortData.getAvatar());
            if (!avatar->_isClientAvatar) {
                avatar->setIsClientAvatar(true);
            }
            // TODO: to help us scale to more avatars it would be nice to not have to poll this stuff every update
            if (avatar->getSkeletonModel()->isLoaded()) {
                // remove the orb if it is there
                avatar->removeOrb();
                if (avatar->needsPhysicsUpdate()) {
                    _otherAvatarsToChangeInPhysics.insert(avatar);
                }
            } else {
                avatar->updateOrbPosition();
            }

            // for ALL avatars...
            if (_shouldRender) {
                avatar->ensureInScene(avatar, qApp->getMain3DScene());
            }

            avatar->animateScaleChanges(deltaTime);

            uint64_t now = usecTimestampNow();
            if (now < passExpiry) {
                // we're within budget
                bool inView = sortData.getPriority() > OUT_OF_VIEW_THRESHOLD;
                if (inView && avatar->hasNewJointData()) {
                    numAvatarsUpdated++;
                }
                auto transitStatus = avatar->_transit.update(deltaTime, avatar->_serverPosition, _transitConfig);
                if (avatar->getIsNewAvatar() && (transitStatus == AvatarTransit::Status::START_TRANSIT ||
                                                 transitStatus == AvatarTransit::Status::ABORT_TRANSIT)) {
                    avatar->_transit.reset();
                    avatar->setIsNewAvatar(false);
                }
                avatar->simulate(deltaTime, inView);
                if (avatar->getSkeletonModel()->isLoaded() && avatar->getWorkloadRegion() == workload::Region::R1) {
                    _myAvatar->addAvatarHandsToFlow(avatar);
                }
                if (_drawOtherAvatarSkeletons) {
                    avatar->debugJointData();
                }
                avatar->setEnableMeshVisible(!_drawOtherAvatarSkeletons);
                avatar->updateRenderItem(renderTransaction);
                avatar->updateSpaceProxy(workloadTransaction);
                avatar->setLastRenderUpdateTime(startTime);

            } else {
                // we've spent our time budget for this priority bucket
                // let's deal with the reminding avatars if this pass and BREAK from the for loop

                if (p == kHero) {
                    // Hero,
                    // --> put them back in the non hero queue

                    auto& crowdQueue = avatarPriorityQueues[kNonHero];
                    while (it != sortedAvatarVector.end()) {
                        crowdQueue.push(SortableAvatar((*it).getAvatar()));
                        ++it;
                    }
                } else {
                    // Non Hero
                    // --> bail on the rest of the avatar updates
                    // --> more avatars may freeze until their priority trickles up
                    // --> some scale animations may glitch
                    // --> some avatar velocity measurements may be a little off

                    // no time to simulate, but we take the time to count how many were tragically missed
                    numAvatarsNotUpdated = sortedAvatarVector.end() - it;
                }

                // We had to cut short this pass, we must break out of the for loop here
                break;
            }
        }

        if (p == kHero) {
            numHerosUpdated = numAvatarsUpdated;
        }
    }

    if (_shouldRender) {
        qApp->getMain3DScene()->enqueueTransaction(renderTransaction);
    }

    _space->enqueueTransaction(workloadTransaction);

    _numAvatarsUpdated = numAvatarsUpdated;
    _numAvatarsNotUpdated = numAvatarsNotUpdated;
    _numHeroAvatarsUpdated = numHerosUpdated;

    _avatarSimulationTime = (float)(usecTimestampNow() - startTime) / (float)USECS_PER_MSEC;
}

void AvatarManager::postUpdate(float deltaTime, const render::ScenePointer& scene) {
    auto hashCopy = getHashCopy();
    AvatarHash::iterator avatarIterator = hashCopy.begin();
    for (avatarIterator = hashCopy.begin(); avatarIterator != hashCopy.end(); avatarIterator++) {
        auto avatar = std::static_pointer_cast<Avatar>(avatarIterator.value());
        avatar->postUpdate(deltaTime, scene);
    }
}

AvatarSharedPointer AvatarManager::newSharedAvatar(const QUuid& sessionUUID) {
    auto otherAvatar = new OtherAvatar(qApp->thread());
    otherAvatar->setSessionUUID(sessionUUID);
    auto nodeList = DependencyManager::get<NodeList>();
    if (nodeList && !nodeList->isIgnoringNode(sessionUUID)) {
        otherAvatar->createOrb();
    }
    return AvatarSharedPointer(otherAvatar, [](OtherAvatar* ptr) { ptr->deleteLater(); });
}

void AvatarManager::queuePhysicsChange(const OtherAvatarPointer& avatar) {
    _otherAvatarsToChangeInPhysics.insert(avatar);
}

DetailedMotionState* AvatarManager::createDetailedMotionState(OtherAvatarPointer avatar, int32_t jointIndex) {
    bool isBound = false;
    std::vector<int32_t> boundJoints;
    const btCollisionShape* shape = avatar->createCollisionShape(jointIndex, isBound, boundJoints);
    if (shape) {
        DetailedMotionState* motionState = new DetailedMotionState(avatar, shape, jointIndex);
        motionState->setMass(0.0f); // DetailedMotionState has KINEMATIC MotionType, so zero mass is ok
        motionState->setIsBound(isBound, boundJoints);
        return motionState;
    }
    return nullptr;
}

void AvatarManager::rebuildAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar) {
    if (!avatar->_motionState) {
        avatar->_motionState = new AvatarMotionState(avatar, nullptr);
    }
    AvatarMotionState* motionState = avatar->_motionState;
    ShapeInfo shapeInfo;
    avatar->computeShapeInfo(shapeInfo);
    const btCollisionShape* shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
    assert(shape);
    motionState->setShape(shape);
    motionState->setMass(avatar->computeMass());
    if (motionState->getRigidBody()) {
        transaction.objectsToReinsert.push_back(motionState);
    } else {
        transaction.objectsToAdd.push_back(motionState);
    }
    motionState->clearIncomingDirtyFlags();
}

void AvatarManager::removeDetailedAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar) {
    // delete old detailedMotionStates
    auto& detailedMotionStates = avatar->getDetailedMotionStates();
    if (detailedMotionStates.size() != 0) {
        for (auto& detailedMotionState : detailedMotionStates) {
            transaction.objectsToRemove.push_back(detailedMotionState);
        }
        avatar->forgetDetailedMotionStates();
    }
}

void AvatarManager::rebuildDetailedAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar) {
    // Rather than reconcile numbers of joints after change to model or LOD
    // we blow away old detailedMotionStates and create anew all around.
    removeDetailedAvatarPhysics(transaction, avatar);
    auto& detailedMotionStates = avatar->getDetailedMotionStates();
    OtherAvatar::BodyLOD lod = avatar->getBodyLOD();
    if (lod == OtherAvatar::BodyLOD::Sphere) {
        auto dMotionState = createDetailedMotionState(avatar, -1);
        if (dMotionState) {
            detailedMotionStates.push_back(dMotionState);
            transaction.objectsToAdd.push_back(dMotionState);
        }
    } else {
        int32_t numJoints = avatar->getJointCount();
        for (int32_t i = 0; i < numJoints; i++) {
            auto dMotionState = createDetailedMotionState(avatar, i);
            if (dMotionState) {
                detailedMotionStates.push_back(dMotionState);
                transaction.objectsToAdd.push_back(dMotionState);
            }
        }
    }
    avatar->_needsDetailedRebuild = false;
}

void AvatarManager::buildPhysicsTransaction(PhysicsEngine::Transaction& transaction) {
    _myAvatar->getCharacterController()->buildPhysicsTransaction(transaction);
    for (auto avatar : _otherAvatarsToChangeInPhysics) {
        bool isInPhysics = avatar->isInPhysicsSimulation();
        if (isInPhysics != avatar->shouldBeInPhysicsSimulation()) {
            if (isInPhysics) {
                transaction.objectsToRemove.push_back(avatar->_motionState);
                avatar->_motionState = nullptr;
                removeDetailedAvatarPhysics(transaction, avatar);
            } else {
                rebuildAvatarPhysics(transaction, avatar);
                rebuildDetailedAvatarPhysics(transaction, avatar);
            }
        } else if (isInPhysics) {
            AvatarMotionState* motionState = avatar->_motionState;
            uint32_t flags = motionState->getIncomingDirtyFlags();
            if (flags & EASY_DIRTY_PHYSICS_FLAGS) {
                motionState->handleEasyChanges(flags);
            }
            // NOTE: we don't call detailedMotionState->handleEasyChanges() here because they are KINEMATIC
            // and Bullet will automagically call DetailedMotionState::getWorldTransform() on all that are active.

            if (motionState->needsNewShape()) {
                rebuildAvatarPhysics(transaction, avatar);
            } else {
                if (flags & (Simulation::DIRTY_MOTION_TYPE | Simulation::DIRTY_COLLISION_GROUP)) {
                    transaction.objectsToReinsert.push_back(motionState);
                }
                motionState->clearIncomingDirtyFlags();
            }

            if (avatar->_needsDetailedRebuild) {
                rebuildDetailedAvatarPhysics(transaction, avatar);
            }
        }
    }
    _otherAvatarsToChangeInPhysics.clear();
}

void AvatarManager::handleProcessedPhysicsTransaction(PhysicsEngine::Transaction& transaction) {
    // things on objectsToRemove are ready for delete
    for (auto object : transaction.objectsToRemove) {
        delete object;
    }
    transaction.clear();
}

void AvatarManager::removeDeadAvatarEntities(const SetOfEntities& deadEntities) {
    auto treeRenderer = DependencyManager::get<EntityTreeRenderer>();
    EntityTreePointer entityTree = treeRenderer ? treeRenderer->getTree() : nullptr;
    for (auto entity : deadEntities) {
        QUuid entityOwnerID = entity->getOwningAvatarID();
        AvatarSharedPointer avatar = getAvatarBySessionID(entityOwnerID);
        if (avatar) {
            avatar->clearAvatarEntityInternal(entity->getID());
        }
    }
}

void AvatarManager::handleRemovedAvatar(const AvatarSharedPointer& removedAvatar, KillAvatarReason removalReason) {
    auto avatar = std::static_pointer_cast<OtherAvatar>(removedAvatar);
    AvatarHashMap::handleRemovedAvatar(avatar, removalReason);
    avatar->tearDownGrabs();

    avatar->die();
    queuePhysicsChange(avatar);
    avatar->removeOrb();

    // remove this avatar's entities from the tree now, if we wait (as we did previously) for this Avatar's destructor
    // it might not fire until after we create a new instance for the same remote avatar, which creates a race
    // on the creation of entities for that avatar instance and the deletion of entities for this instance
    avatar->removeAvatarEntitiesFromTree();
    if (removalReason != KillAvatarReason::AvatarDisconnected) {
        if (removalReason == KillAvatarReason::TheirAvatarEnteredYourBubble) {
            emit AvatarInputs::getInstance()->avatarEnteredIgnoreRadius(avatar->getSessionUUID());
            emit DependencyManager::get<UsersScriptingInterface>()->enteredIgnoreRadius();
        }

        workload::Transaction workloadTransaction;
        workloadTransaction.remove(avatar->getSpaceIndex());
        _space->enqueueTransaction(workloadTransaction);

        const render::ScenePointer& scene = qApp->getMain3DScene();
        render::Transaction transaction;
        avatar->removeFromScene(avatar, scene, transaction);
        scene->enqueueTransaction(transaction);
    } else {
        // remove from node sets, if present
        DependencyManager::get<NodeList>()->removeFromIgnoreMuteSets(avatar->getSessionUUID());
        DependencyManager::get<UsersScriptingInterface>()->avatarDisconnected(avatar->getSessionUUID());
        render::Transaction transaction;
        auto scene = qApp->getMain3DScene();
        avatar->fadeOut(transaction, removalReason);

        workload::SpacePointer space = _space;
        transaction.setTransitionFinishedOperator(avatar->getRenderItemID(), [space, avatar]() {
            if (avatar->getLastFadeRequested() != render::Transition::Type::USER_LEAVE_DOMAIN) {
                // The avatar is using another transition besides the fade-out transition, which means it is still in use.
                // Deleting the avatar now could cause state issues, so abort deletion and show message.
                qCWarning(interfaceapp) << "An ending fade-out transition wants to delete an avatar, but the avatar is still in use. Avatar deletion has aborted. (avatar ID: " << avatar->getSessionUUID() << ")";
            } else {
                const render::ScenePointer& scene = qApp->getMain3DScene();
                render::Transaction transaction;
                avatar->removeFromScene(avatar, scene, transaction);
                scene->enqueueTransaction(transaction);

                workload::Transaction workloadTransaction;
                workloadTransaction.remove(avatar->getSpaceIndex());
                space->enqueueTransaction(workloadTransaction);
            }
        });
        scene->enqueueTransaction(transaction);
    }
}

void AvatarManager::clearOtherAvatars() {
    _myAvatar->clearLookAtTargetAvatar();

    // setup a vector of removed avatars outside the scope of the hash lock
    std::vector<AvatarSharedPointer> removedAvatars;

    {
        QWriteLocker locker(&_hashLock);

        removedAvatars.reserve(_avatarHash.size());

        auto avatarIterator =  _avatarHash.begin();
        while (avatarIterator != _avatarHash.end()) {
            auto avatar = std::static_pointer_cast<Avatar>(avatarIterator.value());
            if (avatar != _myAvatar) {
                removedAvatars.push_back(avatar);
                avatarIterator = _avatarHash.erase(avatarIterator);
            } else {
                ++avatarIterator;
            }
        }
    }

    for (auto& av : removedAvatars) {
        handleRemovedAvatar(av);
    }
}

void AvatarManager::deleteAllAvatars() {
    _otherAvatarsToChangeInPhysics.clear();
    QReadLocker locker(&_hashLock);
    AvatarHash::iterator avatarIterator = _avatarHash.begin();
    while (avatarIterator != _avatarHash.end()) {
        auto avatar = std::static_pointer_cast<Avatar>(avatarIterator.value());
        avatarIterator = _avatarHash.erase(avatarIterator);
        avatar->die();
        if (avatar != _myAvatar) {
            auto otherAvatar = std::static_pointer_cast<OtherAvatar>(avatar);
            assert(!otherAvatar->_motionState);
            assert(otherAvatar->getDetailedMotionStates().size() == 0);
        }
    }
}

void AvatarManager::handleChangedMotionStates(const VectorOfMotionStates& motionStates) {
    // TODO: extract the MyAvatar results once we use a MotionState for it.
}

void AvatarManager::handleCollisionEvents(const CollisionEvents& collisionEvents) {
    bool playedCollisionSound { false };
    for (Collision collision : collisionEvents) {
        // TODO: The plan is to handle MOTIONSTATE_TYPE_AVATAR, and then MOTIONSTATE_TYPE_MYAVATAR. As it is, other
        // people's avatars will have an id that doesn't match any entities, and one's own avatar will have
        // an id of null. Thus this code handles any collision in which one of the participating objects is
        // my avatar. (Other user machines will make a similar analysis and inject sound for their collisions.)
        if (collision.idA.isNull() || collision.idB.isNull()) {
            auto myAvatar = getMyAvatar();
            myAvatar->collisionWithEntity(collision);

            if (!playedCollisionSound) {
                playedCollisionSound = true;
                auto collisionSound = myAvatar->getCollisionSound();
                if (collisionSound) {
                    const auto characterController = myAvatar->getCharacterController();
                    const float avatarVelocityChange =
                        (characterController ? glm::length(characterController->getVelocityChange()) : 0.0f);
                    const float velocityChange = glm::length(collision.velocityChange) + avatarVelocityChange;
                    const float MIN_AVATAR_COLLISION_ACCELERATION = 2.4f;  // walking speed
                    const bool isSound =
                        (collision.type == CONTACT_EVENT_TYPE_START) && (velocityChange > MIN_AVATAR_COLLISION_ACCELERATION);

                    if (!isSound) {
                        return;  // No sense iterating for others. We only have one avatar.
                    }
                    // Your avatar sound is personal to you, so let's say the "mass" part of the kinetic energy is already accounted for.
                    const float energy = velocityChange * velocityChange;
                    const float COLLISION_ENERGY_AT_FULL_VOLUME = 10.0f;
                    const float energyFactorOfFull = fmin(1.0f, energy / COLLISION_ENERGY_AT_FULL_VOLUME);

                    // For general entity collisionSoundURL, playSound supports changing the pitch for the sound based on the size of the object,
                    // but most avatars are roughly the same size, so let's not be so fancy yet.
                    const float AVATAR_STRETCH_FACTOR = 1.0f;

                    _collisionInjectors.remove_if([](const AudioInjectorPointer& injector) { return !injector; });

                    static const int MAX_INJECTOR_COUNT = 3;
                    if (_collisionInjectors.size() < MAX_INJECTOR_COUNT) {
                        AudioInjectorOptions options;
                        options.stereo = collisionSound->isStereo();
                        options.position = myAvatar->getWorldPosition();
                        options.volume = energyFactorOfFull;
                        options.pitch = 1.0f / AVATAR_STRETCH_FACTOR;

                        auto injector = DependencyManager::get<AudioInjectorManager>()->playSound(collisionSound, options, true);
                        _collisionInjectors.emplace_back(injector);
                    }
                }
            }
        }
    }
}

void AvatarManager::updateAvatarRenderStatus(bool shouldRenderAvatars) {
    _shouldRender = shouldRenderAvatars;
    const render::ScenePointer& scene = qApp->getMain3DScene();
    render::Transaction transaction;
    auto avatarHashCopy = getHashCopy();
    if (_shouldRender) {
        for (auto avatarData : avatarHashCopy) {
            auto avatar = std::static_pointer_cast<Avatar>(avatarData);
            avatar->addToScene(avatar, scene, transaction);
        }
    } else {
        for (auto avatarData : avatarHashCopy) {
            auto avatar = std::static_pointer_cast<Avatar>(avatarData);
            avatar->removeFromScene(avatar, scene, transaction);
        }
    }
    scene->enqueueTransaction(transaction);
}

AvatarSharedPointer AvatarManager::getAvatarBySessionID(const QUuid& sessionID) const {
    if (sessionID == AVATAR_SELF_ID || sessionID == _myAvatar->getSessionUUID()) {
        return _myAvatar;
    }

    return findAvatar(sessionID);
}

RayToAvatarIntersectionResult AvatarManager::findRayIntersection(const PickRay& ray,
                                                                 const ScriptValuePointer& avatarIdsToInclude,
                                                                 const ScriptValuePointer& avatarIdsToDiscard,
                                                                 bool pickAgainstMesh) {
    QVector<EntityItemID> avatarsToInclude = qVectorEntityItemIDFromScriptValue(avatarIdsToInclude);
    QVector<EntityItemID> avatarsToDiscard = qVectorEntityItemIDFromScriptValue(avatarIdsToDiscard);
    return findRayIntersectionVector(ray, avatarsToInclude, avatarsToDiscard, pickAgainstMesh);
}

RayToAvatarIntersectionResult AvatarManager::findRayIntersectionVector(const PickRay& ray,
                                                                       const QVector<EntityItemID>& avatarsToInclude,
                                                                       const QVector<EntityItemID>& avatarsToDiscard,
                                                                       bool pickAgainstMesh) {
    RayToAvatarIntersectionResult result;
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(const_cast<AvatarManager*>(this), "findRayIntersectionVector",
                                  Q_RETURN_ARG(RayToAvatarIntersectionResult, result),
                                  Q_ARG(const PickRay&, ray),
                                  Q_ARG(const QVector<EntityItemID>&, avatarsToInclude),
                                  Q_ARG(const QVector<EntityItemID>&, avatarsToDiscard),
                                  Q_ARG(bool, pickAgainstMesh));
        return result;
    }

    PROFILE_RANGE(simulation_physics, __FUNCTION__);

    float bulletDistance = (float)INT_MAX;  // with FLT_MAX bullet rayTest does not return results 
    glm::vec3 rayDirection = glm::normalize(ray.direction);
    std::vector<MyCharacterController::RayAvatarResult> physicsResults = _myAvatar->getCharacterController()->rayTest(glmToBullet(ray.origin), glmToBullet(rayDirection), bulletDistance, QVector<uint>());
    if (physicsResults.size() > 0) {
        glm::vec3 rayDirectionInv = { rayDirection.x != 0.0f ? 1.0f / rayDirection.x : INFINITY,
                                      rayDirection.y != 0.0f ? 1.0f / rayDirection.y : INFINITY,
                                      rayDirection.z != 0.0f ? 1.0f / rayDirection.z : INFINITY };
        glm::vec3 viewFrustumPos = BillboardModeHelpers::getPrimaryViewFrustumPosition();

        for (auto &hit : physicsResults) {
            auto avatarID = hit._intersectWithAvatar;
            if ((avatarsToInclude.size() > 0 && !avatarsToInclude.contains(avatarID)) ||
                (avatarsToDiscard.size() > 0 && avatarsToDiscard.contains(avatarID))) {
                continue;
            }

            MyCharacterController::RayAvatarResult rayAvatarResult;
            BoxFace face = BoxFace::UNKNOWN_FACE;
            QVariantMap extraInfo;
            AvatarPointer avatar = nullptr;
            if (_myAvatar->getSessionUUID() != avatarID) {
                auto avatarMap = getHashCopy();
                AvatarHash::iterator itr = avatarMap.find(avatarID);
                if (itr != avatarMap.end()) {
                    avatar = std::static_pointer_cast<Avatar>(*itr);
                }
            } else {
                avatar = _myAvatar;
            }

            if (!hit._isBound) {
                rayAvatarResult = hit;
            } else if (avatar) {
                auto &multiSpheres = avatar->getMultiSphereShapes();
                if (multiSpheres.size() > 0) {
                    MyCharacterController::RayAvatarResult boxHit;
                    boxHit._distance = FLT_MAX;

                    for (size_t i = 0; i < hit._boundJoints.size(); i++) {
                        assert(hit._boundJoints[i] < (int)multiSpheres.size());
                        auto &mSphere = multiSpheres[hit._boundJoints[i]];
                        if (mSphere.isValid()) {
                            float boundDistance = FLT_MAX;
                            BoxFace boundFace = BoxFace::UNKNOWN_FACE;
                            glm::vec3 boundSurfaceNormal;
                            auto &bbox = mSphere.getBoundingBox();
                            if (bbox.findRayIntersection(ray.origin, rayDirection, rayDirectionInv, boundDistance, boundFace, boundSurfaceNormal)) {
                                if (boundDistance < boxHit._distance) {
                                    boxHit._intersect = true;
                                    boxHit._intersectWithAvatar = avatarID;
                                    boxHit._intersectWithJoint = mSphere.getJointIndex();
                                    boxHit._distance = boundDistance;
                                    boxHit._intersectionPoint = ray.origin + boundDistance * rayDirection;
                                    boxHit._intersectionNormal = boundSurfaceNormal;
                                    face = boundFace;
                                }
                            }
                        }
                    }
                    if (boxHit._distance < FLT_MAX) {
                        rayAvatarResult = boxHit;
                    }
                }
            }

            if (avatar && rayAvatarResult._intersect && pickAgainstMesh) {
                glm::vec3 localRayOrigin = avatar->worldToJointPoint(ray.origin, rayAvatarResult._intersectWithJoint);
                glm::vec3 localRayPoint = avatar->worldToJointPoint(ray.origin + rayAvatarResult._distance * rayDirection, rayAvatarResult._intersectWithJoint);

                auto avatarOrientation = avatar->getWorldOrientation();
                auto avatarPosition = avatar->getWorldPosition();

                auto jointOrientation = avatarOrientation * avatar->getAbsoluteDefaultJointRotationInObjectFrame(rayAvatarResult._intersectWithJoint);
                auto jointPosition = avatarPosition + (avatarOrientation * avatar->getAbsoluteDefaultJointTranslationInObjectFrame(rayAvatarResult._intersectWithJoint));

                auto defaultFrameRayOrigin = jointPosition + jointOrientation * localRayOrigin;
                auto defaultFrameRayPoint = jointPosition + jointOrientation * localRayPoint;
                auto defaultFrameRayDirection = glm::normalize(defaultFrameRayPoint - defaultFrameRayOrigin);

                float subMeshDistance = FLT_MAX;
                BoxFace subMeshFace = BoxFace::UNKNOWN_FACE;
                glm::vec3 subMeshSurfaceNormal;
                QVariantMap subMeshExtraInfo;
                if (avatar->getSkeletonModel()->findRayIntersectionAgainstSubMeshes(defaultFrameRayOrigin, defaultFrameRayDirection, viewFrustumPos, subMeshDistance,
                                                                                    subMeshFace, subMeshSurfaceNormal, subMeshExtraInfo, true, false)) {
                    rayAvatarResult._distance = subMeshDistance;
                    rayAvatarResult._intersectionPoint = ray.origin + subMeshDistance * rayDirection;
                    rayAvatarResult._intersectionNormal = subMeshSurfaceNormal;
                    face = subMeshFace;
                    extraInfo = subMeshExtraInfo;
                } else {
                    rayAvatarResult._intersect = false;
                }
            }

            if (rayAvatarResult._intersect) {
                result.intersects = true;
                result.avatarID = rayAvatarResult._intersectWithAvatar;
                result.distance = rayAvatarResult._distance;
                result.face = face;
                result.intersection = ray.origin + rayAvatarResult._distance * rayDirection;
                result.surfaceNormal = rayAvatarResult._intersectionNormal;
                result.jointIndex = rayAvatarResult._intersectWithJoint;
                result.extraInfo = extraInfo;
                break;
            }
        }
    }

    return result;
}

ParabolaToAvatarIntersectionResult AvatarManager::findParabolaIntersectionVector(const PickParabola& pick,
                                                                                 const QVector<EntityItemID>& avatarsToInclude,
                                                                                 const QVector<EntityItemID>& avatarsToDiscard) {
    ParabolaToAvatarIntersectionResult result;
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(const_cast<AvatarManager*>(this), "findParabolaIntersectionVector",
                               Q_RETURN_ARG(ParabolaToAvatarIntersectionResult, result),
                               Q_ARG(const PickParabola&, pick),
                               Q_ARG(const QVector<EntityItemID>&, avatarsToInclude),
                               Q_ARG(const QVector<EntityItemID>&, avatarsToDiscard));
        return result;
    }

    // It's better to intersect the ray against the avatar's actual mesh, but this is currently difficult to
    // do, because the transformed mesh data only exists over in GPU-land.  As a compromise, this code
    // intersects against the avatars capsule and then against the (T-pose) mesh.  The end effect is that picking
    // against the avatar is sort-of right, but you likely wont be able to pick against the arms.

    // TODO -- find a way to extract transformed avatar mesh data from the rendering engine.

    std::vector<SortedAvatar> sortedAvatars;
    auto avatarHashCopy = getHashCopy();
    for (auto avatarData : avatarHashCopy) {
        auto avatar = std::static_pointer_cast<Avatar>(avatarData);
        if ((avatarsToInclude.size() > 0 && !avatarsToInclude.contains(avatar->getID())) ||
            (avatarsToDiscard.size() > 0 && avatarsToDiscard.contains(avatar->getID()))) {
            continue;
        }

        float distance = FLT_MAX;
#if 0
        // if we weren't picking against the capsule, we would want to pick against the avatarBounds...
        SkeletonModelPointer avatarModel = avatar->getSkeletonModel();
        AABox avatarBounds = avatarModel->getRenderableMeshBound();
        if (avatarBounds.contains(pick.origin)) {
            distance = 0.0f;
        } else {
            float boundDistance = FLT_MAX;
            BoxFace face;
            glm::vec3 surfaceNormal;
            if (avatarBounds.findParabolaIntersection(pick.origin, pick.velocity, pick.acceleration, boundDistance, face, surfaceNormal)) {
                distance = boundDistance;
            }
        }
#else
        glm::vec3 start;
        glm::vec3 end;
        float radius;
        avatar->getCapsule(start, end, radius);
        findParabolaCapsuleIntersection(pick.origin, pick.velocity, pick.acceleration, start, end, radius, avatar->getWorldOrientation(), distance);
#endif

        if (distance < FLT_MAX) {
            sortedAvatars.emplace_back(distance, avatar);
        }
    }

    if (sortedAvatars.size() > 1) {
        static auto comparator = [](const SortedAvatar& left, const SortedAvatar& right) { return left.first < right.first; };
        std::sort(sortedAvatars.begin(), sortedAvatars.end(), comparator);
    }

    glm::vec3 viewFrustumPos = BillboardModeHelpers::getPrimaryViewFrustumPosition();
    for (auto it = sortedAvatars.begin(); it != sortedAvatars.end(); ++it) {
        const SortedAvatar& sortedAvatar = *it;
        // We can exit once avatarCapsuleDistance > bestDistance
        if (sortedAvatar.first > result.parabolicDistance) {
            break;
        }

        float parabolicDistance = FLT_MAX;
        BoxFace face;
        glm::vec3 surfaceNormal;
        QVariantMap extraInfo;
        SkeletonModelPointer avatarModel = sortedAvatar.second->getSkeletonModel();
        if (avatarModel->findParabolaIntersectionAgainstSubMeshes(pick.origin, pick.velocity, pick.acceleration, viewFrustumPos, parabolicDistance, face, surfaceNormal, extraInfo, true)) {
            if (parabolicDistance < result.parabolicDistance) {
                result.intersects = true;
                result.avatarID = sortedAvatar.second->getID();
                result.parabolicDistance = parabolicDistance;
                result.face = face;
                result.surfaceNormal = surfaceNormal;
                result.extraInfo = extraInfo;
            }
        }
    }

    if (result.intersects) {
        result.intersection = pick.origin + pick.velocity * result.parabolicDistance + 0.5f * pick.acceleration * result.parabolicDistance * result.parabolicDistance;
        result.distance = glm::distance(pick.origin, result.intersection);
    }

    return result;
}

// HACK
float AvatarManager::getAvatarSortCoefficient(const QString& name) {
    if (name == "size") {
        return AvatarData::_avatarSortCoefficientSize;
    } else if (name == "center") {
        return AvatarData::_avatarSortCoefficientCenter;
    } else if (name == "age") {
        return AvatarData::_avatarSortCoefficientAge;
    }
    return 0.0f;
}

// HACK
void AvatarManager::setAvatarSortCoefficient(const QString& name, const ScriptValuePointer& value) {
    bool somethingChanged = false;
    if (value->isNumber()) {
        float numericalValue = (float)value->toNumber();
        if (name == "size") {
            AvatarData::_avatarSortCoefficientSize = numericalValue;
            somethingChanged = true;
        } else if (name == "center") {
            AvatarData::_avatarSortCoefficientCenter = numericalValue;
            somethingChanged = true;
        } else if (name == "age") {
            AvatarData::_avatarSortCoefficientAge = numericalValue;
            somethingChanged = true;
        }
    }
    if (somethingChanged) {
        size_t packetSize = sizeof(AvatarData::_avatarSortCoefficientSize) + 
                            sizeof(AvatarData::_avatarSortCoefficientCenter) +
                            sizeof(AvatarData::_avatarSortCoefficientAge);

        auto packet = NLPacket::create(PacketType::AdjustAvatarSorting, packetSize);
        packet->writePrimitive(AvatarData::_avatarSortCoefficientSize);
        packet->writePrimitive(AvatarData::_avatarSortCoefficientCenter);
        packet->writePrimitive(AvatarData::_avatarSortCoefficientAge);
        DependencyManager::get<NodeList>()->broadcastToNodes(std::move(packet), NodeSet() << NodeType::AvatarMixer);
    }
}

/*@jsdoc
 * PAL (People Access List) data for an avatar.
 * @typedef {object} AvatarManager.PalData
 * @property {Uuid} sessionUUID - The avatar's session ID. <code>""</code> if the avatar is your own.
 * @property {string} sessionDisplayName - The avatar's display name, sanitized and versioned, as defined by the avatar mixer. 
 *     It is unique among all avatars present in the domain at the time.
 * @property {number} audioLoudness - The instantaneous loudness of the audio input that the avatar is injecting into the 
 *     domain.
 * @property {boolean} isReplicated - <span class="important">Deprecated: This property is deprecated and will be 
 *     removed.</span>
 * @property {Vec3} position - The position of the avatar.
 * @property {number} palOrbOffset - The vertical offset from the avatar's position that an overlay orb should be displayed at.
 */
QVariantMap AvatarManager::getPalData(const QStringList& specificAvatarIdentifiers) {
    QJsonArray palData;

    auto avatarMap = getHashCopy();
    AvatarHash::iterator itr = avatarMap.begin();
    while (itr != avatarMap.end()) {
        std::shared_ptr<Avatar> avatar = std::static_pointer_cast<Avatar>(*itr);
        QString currentSessionUUID = avatar->getSessionUUID().toString();
        if (specificAvatarIdentifiers.isEmpty() || specificAvatarIdentifiers.contains(currentSessionUUID)) {
            QJsonObject thisAvatarPalData;

            auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();

            if (currentSessionUUID == myAvatar->getSessionUUID().toString()) {
                currentSessionUUID = "";
            }

            thisAvatarPalData.insert("sessionUUID", currentSessionUUID);
            thisAvatarPalData.insert("sessionDisplayName", avatar->getSessionDisplayName());
            thisAvatarPalData.insert("audioLoudness", avatar->getAudioLoudness());
            thisAvatarPalData.insert("isReplicated", avatar->getIsReplicated());

            glm::vec3 position = avatar->getWorldPosition();
            QJsonObject jsonPosition;
            jsonPosition.insert("x", position.x);
            jsonPosition.insert("y", position.y);
            jsonPosition.insert("z", position.z);
            thisAvatarPalData.insert("position", jsonPosition);

            float palOrbOffset = 0.2f;
            int headIndex = avatar->getJointIndex("Head");
            if (headIndex > 0) {
                glm::vec3 jointTranslation = avatar->getAbsoluteJointTranslationInObjectFrame(headIndex);
                palOrbOffset = jointTranslation.y / 2;
            }
            thisAvatarPalData.insert("palOrbOffset", palOrbOffset);

            palData.append(thisAvatarPalData);
        }
        ++itr;
    }
    QJsonObject doc;
    doc.insert("data", palData);
    return doc.toVariantMap();
}

void AvatarManager::accumulateGrabPositions(std::map<QUuid, GrabLocationAccumulator>& grabAccumulators) {
    auto avatarMap = getHashCopy();
    AvatarHash::iterator itr = avatarMap.begin();
    while (itr != avatarMap.end()) {
        const auto& avatar = std::static_pointer_cast<Avatar>(*itr);
        avatar->accumulateGrabPositions(grabAccumulators);
        itr++;
    }
}
