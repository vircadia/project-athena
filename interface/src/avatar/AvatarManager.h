//
//  AvatarManager.h
//  interface/src/avatar
//
//  Created by Stephen Birarda on 1/23/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AvatarManager_h
#define hifi_AvatarManager_h

#include <set>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <AvatarHashMap.h>
#include <PhysicsEngine.h>
#include <PIDController.h>
#include <SimpleMovingAverage.h>
#include <shared/RateCounter.h>
#include <avatars-renderer/ScriptAvatar.h>
#include <AudioInjectorManager.h>
#include <workload/Space.h>
#include <EntitySimulation.h> // for SetOfEntities

#include "AvatarMotionState.h"
#include "DetailedMotionState.h"
#include "MyAvatar.h"
#include "OtherAvatar.h"

class ScriptEngine;
class ScriptValue;
using SortedAvatar = std::pair<float, std::shared_ptr<Avatar>>;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc 
 * The <code>AvatarManager</code> API provides information about avatars within the current domain. The avatars available are 
 * those that Interface has displayed and therefore knows about.
 *
 * <p><strong>Warning:</strong> This API is also provided to Interface, client entity, and avatar scripts as the synonym, 
 * "<code>AvatarList</code>". For assignment client scripts, see the separate {@link AvatarList} API.</p>
 *
 * @namespace AvatarManager
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @borrows AvatarList.getAvatar as getAvatar
 * @comment AvatarList.getAvatarIdentifiers as getAvatarIdentifiers - Don't borrow because behavior is slightly different.
 * @comment AvatarList.getAvatarsInRange as getAvatarsInRange - Don't borrow because behavior is slightly different.
 * @borrows AvatarList.avatarAddedEvent as avatarAddedEvent
 * @borrows AvatarList.avatarRemovedEvent as avatarRemovedEvent
 * @borrows AvatarList.avatarSessionChangedEvent as avatarSessionChangedEvent
 * @borrows AvatarList.isAvatarInRange as isAvatarInRange
 * @borrows AvatarList.sessionUUIDChanged as sessionUUIDChanged
 * @borrows AvatarList.processAvatarDataPacket as processAvatarDataPacket
 * @borrows AvatarList.processAvatarIdentityPacket as processAvatarIdentityPacket
 * @borrows AvatarList.processBulkAvatarTraits as processBulkAvatarTraits
 * @borrows AvatarList.processKillAvatar as processKillAvatar
 */

class AvatarManager : public AvatarHashMap {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:

    /*@jsdoc
     * Gets the IDs of all avatars known about in the domain.
     * Your own avatar is included in the list as a <code>null</code> value.
     * @function AvatarManager.getAvatarIdentifiers
     * @returns {Uuid[]} The IDs of all known avatars in the domain.
     * @example <caption>Report the IDS of all avatars within the domain.</caption>
     * var avatars = AvatarManager.getAvatarIdentifiers();
     * print("Avatars in the domain: " + JSON.stringify(avatars));
     * // A null item is included for your avatar.
     */

    /*@jsdoc
     * Gets the IDs of all avatars known about within a specified distance from a point.
     * Your own avatar's ID is included in the list if it is in range.
     * @function AvatarManager.getAvatarsInRange
     * @param {Vec3} position - The point about which the search is performed.
     * @param {number} range - The search radius.
     * @returns {Uuid[]} The IDs of all known avatars within the search distance from the position.
     * @example <caption>Report the IDs of all avatars within 10m of your avatar.</caption>
     * var RANGE = 10;
     * var avatars = AvatarManager.getAvatarsInRange(MyAvatar.position, RANGE);
     * print("Nearby avatars: " + JSON.stringify(avatars));
     * print("Own avatar: " + MyAvatar.sessionUUID);
     */

    /// Registers the script types associated with the avatar manager.
    static void registerMetaTypes(ScriptEngine* engine);

    virtual ~AvatarManager();

    void init();
    void setSpace(workload::SpacePointer& space );

    std::shared_ptr<MyAvatar> getMyAvatar() { return _myAvatar; }
    glm::vec3 getMyAvatarPosition() const { return _myAvatar->getWorldPosition(); }

    /*@jsdoc 
     * @comment Uses the base class's JSDoc.
     */
    // Null/Default-constructed QUuids will return MyAvatar
    Q_INVOKABLE virtual ScriptAvatarData* getAvatar(QUuid avatarID) override { return new ScriptAvatar(getAvatarBySessionID(avatarID)); }

    AvatarSharedPointer getAvatarBySessionID(const QUuid& sessionID) const override;

    int getNumAvatarsUpdated() const { return _numAvatarsUpdated; }
    int getNumAvatarsNotUpdated() const { return _numAvatarsNotUpdated; }
    int getNumHeroAvatars() const { return _numHeroAvatars; }
    int getNumHeroAvatarsUpdated() const { return _numHeroAvatarsUpdated; }
    float getAvatarSimulationTime() const { return _avatarSimulationTime; }

    void updateMyAvatar(float deltaTime);
    void updateOtherAvatars(float deltaTime);

    void setMyAvatarDataPacketsPaused(bool puase);

    void postUpdate(float deltaTime, const render::ScenePointer& scene);

    void clearOtherAvatars() override;
    void deleteAllAvatars();

    void getObjectsToRemoveFromPhysics(VectorOfMotionStates& motionStates);
    void getObjectsToAddToPhysics(VectorOfMotionStates& motionStates);
    void getObjectsToChange(VectorOfMotionStates& motionStates);

    void handleChangedMotionStates(const VectorOfMotionStates& motionStates);
    void handleCollisionEvents(const CollisionEvents& collisionEvents);

    /*@jsdoc
     * Gets the amount of avatar mixer data being generated by an avatar other than your own.
     * @function AvatarManager.getAvatarDataRate
     * @param {Uuid} sessionID - The ID of the avatar whose data rate you're retrieving.
     * @param {AvatarDataRate} [rateName=""] - The type of avatar mixer data to get the data rate of.
     * @returns {number} The data rate in kbps; <code>0</code> if the avatar is your own.
     */
    Q_INVOKABLE float getAvatarDataRate(const QUuid& sessionID, const QString& rateName = QString("")) const;

    /*@jsdoc
     * Gets the update rate of avatar mixer data being generated by an avatar other than your own.
     * @function AvatarManager.getAvatarUpdateRate
     * @param {Uuid} sessionID - The ID of the avatar whose update rate you're retrieving.
     * @param {AvatarUpdateRate} [rateName=""] - The type of avatar mixer data to get the update rate of.
     * @returns {number} The update rate in Hz; <code>0</code> if the avatar is your own.
     */
    Q_INVOKABLE float getAvatarUpdateRate(const QUuid& sessionID, const QString& rateName = QString("")) const;

    /*@jsdoc
     * Gets the simulation rate of an avatar other than your own.
     * @function AvatarManager.getAvatarSimulationRate
     * @param {Uuid} sessionID - The ID of the avatar whose simulation you're retrieving.
     * @param {AvatarSimulationRate} [rateName=""] - The type of avatar data to get the simulation rate of.
     * @returns {number} The simulation rate in Hz; <code>0</code> if the avatar is your own.
     */
    Q_INVOKABLE float getAvatarSimulationRate(const QUuid& sessionID, const QString& rateName = QString("")) const;

    /*@jsdoc
     * Find the first avatar intersected by a {@link PickRay}.
     * @function AvatarManager.findRayIntersection
     * @param {PickRay} ray - The ray to use for finding avatars.
     * @param {Uuid[]} [avatarsToInclude=[]] - If not empty then search is restricted to these avatars.
     * @param {Uuid[]} [avatarsToDiscard=[]] - Avatars to ignore in the search.
     * @param {boolean} [pickAgainstMesh=true] - If <code>true</code> then the exact intersection with the avatar mesh is 
     *     calculated, if <code>false</code> then the intersection is approximate.
     * @returns {RayToAvatarIntersectionResult} The result of the search for the first intersected avatar.
     * @example <caption>Find the first avatar directly in front of you.</caption>
     * var pickRay = {
     *     origin: MyAvatar.position,
     *     direction: Quat.getFront(MyAvatar.orientation)
     * };
     * 
     * var intersection = AvatarManager.findRayIntersection(pickRay);
     * if (intersection.intersects) {
     *     print("Avatar found: " + JSON.stringify(intersection));
     * } else {
     *     print("No avatar found.");
     * }
     */
    Q_INVOKABLE RayToAvatarIntersectionResult findRayIntersection(const PickRay& ray,
                                                                  const ScriptValuePointer& avatarIdsToInclude = ScriptValuePointer(),
                                                                  const ScriptValuePointer& avatarIdsToDiscard = ScriptValuePointer(),
                                                                  bool pickAgainstMesh = true);
    /*@jsdoc
     * @function AvatarManager.findRayIntersectionVector
     * @param {PickRay} ray - Ray.
     * @param {Uuid[]} avatarsToInclude - Avatars to include.
     * @param {Uuid[]} avatarsToDiscard - Avatars to discard.
     * @param {boolean} pickAgainstMesh - Pick against mesh.
     * @returns {RayToAvatarIntersectionResult} Intersection result.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE RayToAvatarIntersectionResult findRayIntersectionVector(const PickRay& ray,
                                                                        const QVector<EntityItemID>& avatarsToInclude,
                                                                        const QVector<EntityItemID>& avatarsToDiscard,
                                                                        bool pickAgainstMesh);

    /*@jsdoc
     * @function AvatarManager.findParabolaIntersectionVector
     * @param {PickParabola} pick - Pick.
     * @param {Uuid[]} avatarsToInclude - Avatars to include.
     * @param {Uuid[]} avatarsToDiscard - Avatars to discard.
     * @returns {ParabolaToAvatarIntersectionResult} Intersection result.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE ParabolaToAvatarIntersectionResult findParabolaIntersectionVector(const PickParabola& pick,
                                                                                  const QVector<EntityItemID>& avatarsToInclude,
                                                                                  const QVector<EntityItemID>& avatarsToDiscard);

    /*@jsdoc
     * @function AvatarManager.getAvatarSortCoefficient
     * @param {string} name - Name.
     * @returns {number} Value.
     * @deprecated This function is deprecated and will be removed.
     */
    // TODO: remove this HACK once we settle on optimal default sort coefficients
    Q_INVOKABLE float getAvatarSortCoefficient(const QString& name);

    /*@jsdoc
     * @function AvatarManager.setAvatarSortCoefficient
     * @param {string} name - Name
     * @param {number} value - Value.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void setAvatarSortCoefficient(const QString& name, const ScriptValuePointer& value);

    /*@jsdoc
     * Gets PAL (People Access List) data for one or more avatars. Using this method is faster than iterating over each avatar 
     * and obtaining data about each individually.
     * @function AvatarManager.getPalData
     * @param {string[]} [avatarIDs=[]] - The IDs of the avatars to get the PAL data for. If empty, then PAL data is obtained 
     *     for all avatars.
     * @returns {Object<"data", AvatarManager.PalData[]>} An array of objects, each object being the PAL data for an avatar.
     * @example <caption>Report the PAL data for an avatar nearby.</caption>
     * var palData = AvatarManager.getPalData();
     * print("PAL data for one avatar: " + JSON.stringify(palData.data[0]));
     */
    Q_INVOKABLE QVariantMap getPalData(const QStringList& specificAvatarIdentifiers = QStringList());

    float getMyAvatarSendRate() const { return _myAvatarSendRate.rate(); }

    void queuePhysicsChange(const OtherAvatarPointer& avatar);
    void buildPhysicsTransaction(PhysicsEngine::Transaction& transaction);
    void handleProcessedPhysicsTransaction(PhysicsEngine::Transaction& transaction);
    void removeDeadAvatarEntities(const SetOfEntities& deadEntities);

    void accumulateGrabPositions(std::map<QUuid, GrabLocationAccumulator>& grabAccumulators);

public slots:
    /*@jsdoc
     * @function AvatarManager.updateAvatarRenderStatus
     * @param {boolean} shouldRenderAvatars - Should render avatars.
     * @deprecated This function is deprecated and will be removed.
     */
    void updateAvatarRenderStatus(bool shouldRenderAvatars);

    /*@jsdoc
    * Displays other avatars skeletons debug graphics.
    * @function AvatarManager.setEnableDebugDrawOtherSkeletons
    * @param {boolean} enabled - <code>true</code> to show the debug graphics, <code>false</code> to hide.
    */
    void setEnableDebugDrawOtherSkeletons(bool isEnabled) {
        _drawOtherAvatarSkeletons = isEnabled;
    }

protected:
    AvatarSharedPointer addAvatar(const QUuid& sessionUUID, const QWeakPointer<Node>& mixerWeakPointer) override;
    DetailedMotionState* createDetailedMotionState(OtherAvatarPointer avatar, int32_t jointIndex);
    void rebuildAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar);
    void removeDetailedAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar);
    void rebuildDetailedAvatarPhysics(PhysicsEngine::Transaction& transaction, const OtherAvatarPointer& avatar);

private:
    explicit AvatarManager(QObject* parent = 0);
    explicit AvatarManager(const AvatarManager& other);

    AvatarSharedPointer newSharedAvatar(const QUuid& sessionUUID) override;

    // called only from the AvatarHashMap thread - cannot be called while this thread holds the
    // hash lock, since handleRemovedAvatar needs a write lock on the entity tree and the entity tree
    // frequently grabs a read lock on the hash to get a given avatar by ID
    void handleRemovedAvatar(const AvatarSharedPointer& removedAvatar,
                             KillAvatarReason removalReason = KillAvatarReason::NoReason) override;
    void handleTransitAnimations(AvatarTransit::Status status);

    using SetOfOtherAvatars = std::set<OtherAvatarPointer>;
    SetOfOtherAvatars _otherAvatarsToChangeInPhysics;

    std::shared_ptr<MyAvatar> _myAvatar;
    quint64 _lastSendAvatarDataTime = 0; // Controls MyAvatar send data rate.

    std::list<QWeakPointer<AudioInjector>> _collisionInjectors;

    RateCounter<> _myAvatarSendRate;
    int _numAvatarsUpdated { 0 };
    int _numAvatarsNotUpdated { 0 };
    int _numHeroAvatars{ 0 };
    int _numHeroAvatarsUpdated{ 0 };
    float _avatarSimulationTime { 0.0f };
    bool _shouldRender { true };
    bool _myAvatarDataPacketsPaused { false };

    mutable std::mutex _spaceLock;
    workload::SpacePointer _space;

    AvatarTransit::TransitConfig  _transitConfig;
    bool _drawOtherAvatarSkeletons { false };
};

#endif // hifi_AvatarManager_h
