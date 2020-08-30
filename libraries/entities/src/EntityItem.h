//
//  EntityItem.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityItem_h
#define hifi_EntityItem_h

#include <memory>
#include <stdint.h>

#include <glm/glm.hpp>

#include <QtGui/QWindow>
#include <QSet>

#include <Octree.h> // for EncodeBitstreamParams class
#include <OctreeElement.h> // for OctreeElement::AppendState
#include <OctreePacketData.h>
#include <PhysicsCollisionGroups.h>
#include <SimulationFlags.h>
#include <ShapeInfo.h>
#include <Transform.h>
#include <SpatiallyNestable.h>
#include <Interpolate.h>

#include "EntityItemID.h"
#include "EntityItemPropertiesDefaults.h"
#include "EntityPropertyFlags.h"
#include "EntityTypes.h"
#include "SimulationOwner.h"
#include "EntityDynamicInterface.h"
#include "GrabPropertyGroup.h"

class EntitySimulation;
using EntitySimulationPointer = std::shared_ptr<EntitySimulation>;
class EntityTreeElement;
class EntityTreeElementExtraEncodeData;
class EntityDynamicInterface;
class EntityItemProperties;
class EntityTree;
class btCollisionShape;
typedef std::shared_ptr<EntityTree> EntityTreePointer;
typedef std::shared_ptr<EntityDynamicInterface> EntityDynamicPointer;
typedef std::shared_ptr<EntityTreeElement> EntityTreeElementPointer;
using EntityTreeElementExtraEncodeDataPointer = std::shared_ptr<EntityTreeElementExtraEncodeData>;
using SetOfEntities = QSet<EntityItemPointer>;

#define DONT_ALLOW_INSTANTIATION virtual void pureVirtualFunctionPlaceHolder() = 0;
#define ALLOW_INSTANTIATION virtual void pureVirtualFunctionPlaceHolder() override { };

#define debugTime(T, N) qPrintable(QString("%1 [ %2 ago]").arg(T, 16, 10).arg(formatUsecTime(N - T), 15))
#define debugTimeOnly(T) qPrintable(QString("%1").arg(T, 16, 10))
#define debugTreeVector(V) V << "[" << V << " in meters ]"

const uint64_t MAX_OUTGOING_SIMULATION_UPDATE_PERIOD = 9 * USECS_PER_SECOND;
const uint64_t MAX_INCOMING_SIMULATION_UPDATE_PERIOD = MAX_OUTGOING_SIMULATION_UPDATE_PERIOD + USECS_PER_SECOND;

class MeshProxyList;

#ifdef DOMAIN
#undef DOMAIN
#endif

namespace entity {
enum class HostType {
    DOMAIN = 0,
    AVATAR,
    LOCAL
};
}

/// EntityItem class this is the base class for all entity types. It handles the basic properties and functionality available
/// to all other entity types. In particular: postion, size, rotation, age, lifetime, velocity, gravity. You can not instantiate
/// one directly, instead you must only construct one of it's derived classes with additional features.
class EntityItem : public QObject, public SpatiallyNestable, public ReadWriteLockable {
    Q_OBJECT
    // These two classes manage lists of EntityItem pointers and must be able to cleanup pointers when an EntityItem is deleted.
    // To make the cleanup robust each EntityItem has backpointers to its manager classes (which are only ever set/cleared by
    // the managers themselves, hence they are fiends) whose NULL status can be used to determine which managers still need to
    // do cleanup.
    friend class EntityTreeElement;
    friend class EntitySimulation;
public:

    DONT_ALLOW_INSTANTIATION // This class can not be instantiated directly

    EntityItem(const EntityItemID& entityItemID);
    virtual ~EntityItem();

    inline EntityItemPointer getThisPointer() const {
        return std::static_pointer_cast<EntityItem>(std::const_pointer_cast<SpatiallyNestable>(shared_from_this()));
    }

    EntityItemID getEntityItemID() const { return EntityItemID(_id); }

    // methods for getting/setting all properties of an entity
    virtual EntityItemProperties getProperties(const EntityPropertyFlags& desiredProperties = EntityPropertyFlags(), bool allowEmptyDesiredProperties = false) const;

    /// returns true if something changed
    // This function calls setSubClass properties and detects if any property changes value.
    // If something changed then the "somethingChangedNotification" calls happens
    virtual bool setProperties(const EntityItemProperties& properties);

    // Set properties for sub class so they can add their own properties
    // it does nothing in the root class
    // This function is called by setProperties which then can detects if any property changes value in the SubClass (see aboe comment on setProperties)
    virtual bool setSubClassProperties(const EntityItemProperties& properties) { return false; }

    // Update properties with empty parent id and globalized/absolute values (applying offset), and apply (non-empty) log template to args id, name-or-type, parent id.
    void globalizeProperties(EntityItemProperties& properties, const QString& messageTemplate = QString(), const glm::vec3& offset = glm::vec3(0.0f)) const;

    void recordCreationTime();    // set _created to 'now'
    quint64 getLastSimulated() const; /// Last simulated time of this entity universal usecs
    void setLastSimulated(quint64 now);

     /// Last edited time of this entity universal usecs
    quint64 getLastEdited() const;
    void setLastEdited(quint64 lastEdited);
    float getEditedAgo() const /// Elapsed seconds since this entity was last edited
        { return (float)(usecTimestampNow() - getLastEdited()) / (float)USECS_PER_SECOND; }

    /// Last time we sent out an edit packet for this entity
    quint64 getLastBroadcast() const { return _lastBroadcast; }
    void setLastBroadcast(quint64 lastBroadcast) { _lastBroadcast = lastBroadcast; }

    void markAsChangedOnServer();
    quint64 getLastChangedOnServer() const;

    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const;

    virtual OctreeElement::AppendState appendEntityData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                                        EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                                        const bool destinationNodeCanGetAndSetPrivateUserData = false) const;

    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const { /* do nothing*/ };

    static EntityItemID readEntityItemIDFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                    ReadBitstreamToTreeParams& args);

    int readEntityDataFromBuffer(const unsigned char* data, int bytesLeftToRead, ReadBitstreamToTreeParams& args);

    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged)
                                                { somethingChanged = false; return 0; }
    static int expectedBytes();

    static void adjustEditPacketForClockSkew(QByteArray& buffer, qint64 clockSkew);

    // perform update
    virtual void update(const quint64& now);
    quint64 getLastUpdated() const;

    // perform linear extrapolation for SimpleEntitySimulation
    void simulate(const quint64& now);
    bool stepKinematicMotion(float timeElapsed); // return 'true' if moving

    virtual bool needsToCallUpdate() const { return false; }

    virtual void debugDump() const;

    virtual bool supportsDetailedIntersection() const { return false; }
    virtual bool findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                         const glm::vec3& viewFrustumPos, OctreeElementPointer& element, float& distance,
                         BoxFace& face, glm::vec3& surfaceNormal,
                         QVariantMap& extraInfo, bool precisionPicking) const { return true; }
    virtual bool findDetailedParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity,
                        const glm::vec3& acceleration, const glm::vec3& viewFrustumPos, OctreeElementPointer& element,
                        float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal,
                        QVariantMap& extraInfo, bool precisionPicking) const { return true; }

    // attributes applicable to all entity types
    EntityTypes::EntityType getType() const { return _type; }

    inline glm::vec3 getCenterPosition(bool& success) const { return getTransformToCenter(success).getTranslation(); }
    void setCenterPosition(const glm::vec3& position);

    const Transform getTransformToCenter(bool& success) const;
    const Transform getTransformToCenterWithOnlyLocalRotation(bool& success) const;

    void requiresRecalcBoxes();

    // Hyperlink related getters and setters
    QString getHref() const;
    void setHref(QString value);

    QString getDescription() const;
    void setDescription(const QString& value);

    /// Dimensions in meters (0.0 - TREE_SCALE)
    virtual glm::vec3 getScaledDimensions() const;
    virtual void setScaledDimensions(const glm::vec3& value);

    virtual glm::vec3 getPivot() const { return glm::vec3(0.0f); } // pivot offset for positioning, mainly for model entities

    glm::vec3 getUnscaledDimensions() const;
    virtual void setUnscaledDimensions(const glm::vec3& value);

    void setDensity(float density);
    float computeMass() const;
    void setMass(float mass);

    float getDensity() const;

    bool hasVelocity() const { return getWorldVelocity() != ENTITY_ITEM_ZERO_VEC3; }
    bool hasLocalVelocity() const { return getLocalVelocity() != ENTITY_ITEM_ZERO_VEC3; }

    glm::vec3 getGravity() const; /// get gravity in meters
    void setGravity(const glm::vec3& value); /// gravity in meters
    bool hasGravity() const { return getGravity() != ENTITY_ITEM_ZERO_VEC3; }

    glm::vec3 getAcceleration() const; /// get acceleration in meters/second/second
    void setAcceleration(const glm::vec3& value); /// acceleration in meters/second/second
    bool hasAcceleration() const { return getAcceleration() != ENTITY_ITEM_ZERO_VEC3; }

    float getDamping() const;
    void setDamping(float value);

    float getRestitution() const;
    void setRestitution(float value);

    float getFriction() const;
    void setFriction(float value);

    // lifetime related properties.
    float getLifetime() const; /// get the lifetime in seconds for the entity
    void setLifetime(float value); /// set the lifetime in seconds for the entity

    quint64 getCreated() const; /// get the created-time in useconds for the entity
    void setCreated(quint64 value); /// set the created-time in useconds for the entity

    /// is this entity immortal, in that it has no lifetime set, and will exist until manually deleted
    bool isImmortal() const { return getLifetime() == ENTITY_ITEM_IMMORTAL_LIFETIME; }

    /// is this entity mortal, in that it has a lifetime set, and will automatically be deleted when that lifetime expires
    bool isMortal() const { return getLifetime() != ENTITY_ITEM_IMMORTAL_LIFETIME; }

    /// age of this entity in seconds
    float getAge() const { return (float)(usecTimestampNow() - getCreated()) / (float)USECS_PER_SECOND; }
    bool lifetimeHasExpired() const;
    quint64 getExpiry() const;

    // position, size, and bounds related helpers
    virtual AACube getMaximumAACube(bool& success) const override;
    AACube getMinimumAACube(bool& success) const;
    virtual AABox getAABox(bool& success) const; /// axis aligned bounding box in world-frame (meters)

    using SpatiallyNestable::getQueryAACube;
    virtual AACube getQueryAACube(bool& success) const override;
    virtual bool shouldPuffQueryAACube() const override;

    QString getScript() const;
    void setScript(const QString& value);

    quint64 getScriptTimestamp() const;
    void setScriptTimestamp(const quint64 value);

    QString getServerScripts() const;
    void setServerScripts(const QString& serverScripts);

    QString getCollisionSoundURL() const;
    void setCollisionSoundURL(const QString& value);

    glm::vec3 getRegistrationPoint() const; /// registration point as ratio of entity
    /// registration point as ratio of entity
    virtual void setRegistrationPoint(const glm::vec3& value); // FIXME: this is suspicious!

    bool hasAngularVelocity() const { return getWorldAngularVelocity() != ENTITY_ITEM_ZERO_VEC3; }
    bool hasLocalAngularVelocity() const { return getLocalAngularVelocity() != ENTITY_ITEM_ZERO_VEC3; }

    virtual void setAngularVelocity(const glm::vec3& angularVelocity);

    float getAngularDamping() const;
    void setAngularDamping(float value);

    virtual QString getName() const override;
    void setName(const QString& value);
    QString getDebugName();

    bool getVisible() const;
    void setVisible(bool value);

    bool isVisibleInSecondaryCamera() const;
    void setIsVisibleInSecondaryCamera(bool value);

    RenderLayer getRenderLayer() const;
    void setRenderLayer(RenderLayer value);

    PrimitiveMode getPrimitiveMode() const;
    void setPrimitiveMode(PrimitiveMode value);

    bool getIgnorePickIntersection() const;
    void setIgnorePickIntersection(bool value);

    bool getCanCastShadow() const;
    void setCanCastShadow(bool value);

    bool getCullWithParent() const;
    void setCullWithParent(bool value);

    void setCauterized(bool value);
    bool getCauterized() const;

    inline bool isVisible() const { return getVisible(); }
    inline bool isInvisible() const { return !getVisible(); }

    bool isChildOfMyAvatar() const;

    bool getCollisionless() const;
    void setCollisionless(bool value);

    uint16_t getCollisionMask() const;
    void setCollisionMask(uint16_t value);

    void computeCollisionGroupAndFinalMask(int32_t& group, int32_t& mask) const;

    bool getDynamic() const;
    void setDynamic(bool value);

    virtual bool shouldBePhysical() const { return !isDead() && getShapeType() != SHAPE_TYPE_NONE && !isLocalEntity(); }
    bool isVisuallyReady() const { return _visuallyReady; }

    bool getLocked() const;
    void setLocked(bool value);

    QString getUserData() const;
    virtual void setUserData(const QString& value); // FIXME: This is suspicious

    QString getPrivateUserData() const;
    void setPrivateUserData(const QString& value);

    // FIXME not thread safe?
    const SimulationOwner& getSimulationOwner() const { return _simulationOwner; }
    void setSimulationOwner(const QUuid& id, uint8_t priority);
    void setSimulationOwner(const SimulationOwner& owner);

    uint8_t getSimulationPriority() const { return _simulationOwner.getPriority(); }
    QUuid getSimulatorID() const { return _simulationOwner.getID(); }
    void clearSimulationOwnership();

    // TODO: move this "ScriptSimulationPriority" and "PendingOwnership" stuff into EntityMotionState
    // but first would need to do some other cleanup. In the meantime these live here as "scratch space"
    // to allow libs that don't know about each other to communicate.
    void upgradeScriptSimulationPriority(uint8_t priority);
    void clearScriptSimulationPriority();
    uint8_t getScriptSimulationPriority() const { return _scriptSimulationPriority; }
    void setPendingOwnershipPriority(uint8_t priority);
    uint8_t getPendingOwnershipPriority() const { return _pendingOwnershipPriority; }
    bool pendingRelease(uint64_t timestamp) const;
    bool stillWaitingToTakeOwnership(uint64_t timestamp) const;

    bool getCloneable() const;
    void setCloneable(bool value);
    float getCloneLifetime() const;
    void setCloneLifetime(float value);
    float getCloneLimit() const;
    void setCloneLimit(float value);
    bool getCloneDynamic() const;
    void setCloneDynamic(bool value);
    bool getCloneAvatarEntity() const;
    void setCloneAvatarEntity(bool value);
    const QUuid getCloneOriginID() const;
    void setCloneOriginID(const QUuid& value);

    // TODO: get rid of users of getRadius()...
    float getRadius() const;

    virtual void adjustShapeInfoByRegistration(ShapeInfo& info, bool includePivot = true) const;
    virtual bool contains(const glm::vec3& point) const;

    virtual bool isReadyToComputeShape() const { return !isDead(); }
    virtual void computeShapeInfo(ShapeInfo& info);
    virtual float getVolumeEstimate() const;

    /// return preferred shape type (actual physical shape may differ)
    virtual ShapeType getShapeType() const { return SHAPE_TYPE_NONE; }

    void setPosition(const glm::vec3& value);
    virtual void setParentID(const QUuid& parentID) override;
    virtual void setShapeType(ShapeType type) { /* do nothing */ }

    void setRotation(glm::quat orientation);
    void setVelocity(const glm::vec3& velocity);

    uint32_t getDirtyFlags() const;
    void markDirtyFlags(uint32_t mask);
    void clearDirtyFlags(uint32_t mask = 0x0000ffff);

    uint32_t getSpecialFlags() const;
    void markSpecialFlags(uint32_t mask);
    void clearSpecialFlags(uint32_t mask = 0xffff0000);

    bool isMoving() const;
    bool isMovingRelativeToParent() const;

    bool isSimulated() const { return _simulated; }

    bool isInPhysicsSimulation() const { return (bool)(_flags & Simulation::SPECIAL_FLAG_IN_PHYSICS_SIMULATION); }

    void* getPhysicsInfo() const { return _physicsInfo; }
    void setPhysicsInfo(void* data) { _physicsInfo = data; }

    EntityTreeElementPointer getElement() const { return _element; }
    EntityTreePointer getTree() const;
    virtual SpatialParentTree* getParentTree() const override;
    bool wantTerseEditLogging() const;

    glm::mat4 getEntityToWorldMatrix() const;
    glm::mat4 getWorldToEntityMatrix() const;
    glm::vec3 worldToEntity(const glm::vec3& point) const;
    glm::vec3 entityToWorld(const glm::vec3& point) const;

    quint64 getLastEditedFromRemote() const { return _lastEditedFromRemote; }
    void updateLastEditedFromRemote() { _lastEditedFromRemote = usecTimestampNow(); }

    void getTransformAndVelocityProperties(EntityItemProperties& properties) const;

    void flagForMotionStateChange() { _flags |= Simulation::DIRTY_MOTION_TYPE; }

    QString actionsToDebugString();
    bool addAction(EntitySimulationPointer simulation, EntityDynamicPointer action);
    bool updateAction(EntitySimulationPointer simulation, const QUuid& actionID, const QVariantMap& arguments);
    bool removeAction(EntitySimulationPointer simulation, const QUuid& actionID);
    bool clearActions(EntitySimulationPointer simulation);
    void setDynamicData(QByteArray dynamicData);
    const QByteArray getDynamicData() const;
    bool hasActions() const { return !_objectActions.empty() || !_grabActions.empty(); }
    QList<QUuid> getActionIDs() const { return _objectActions.keys(); }
    QVariantMap getActionArguments(const QUuid& actionID) const;
    void deserializeActions();

    void setDynamicDataDirty(bool value) const { _dynamicDataDirty = value; }
    bool dynamicDataDirty() const { return _dynamicDataDirty; }

    void setDynamicDataNeedsTransmit(bool value) const { _dynamicDataNeedsTransmit = value; }
    bool dynamicDataNeedsTransmit() const { return _dynamicDataNeedsTransmit; }
    void setTransitingWithAvatar(bool value) { _transitingWithAvatar = value; }
    bool getTransitingWithAvatar() { return _transitingWithAvatar; }

    bool shouldSuppressLocationEdits() const;

    void setSourceUUID(const QUuid& sourceUUID) { _sourceUUID = sourceUUID; }
    const QUuid& getSourceUUID() const { return _sourceUUID; }
    bool matchesSourceUUID(const QUuid& sourceUUID) const { return _sourceUUID == sourceUUID; }

    QList<EntityDynamicPointer> getActionsOfType(EntityDynamicType typeToGet) const;

    // these are in the frame of this object
    virtual glm::quat getAbsoluteJointRotationInObjectFrame(int index) const override { return glm::quat(); }
    virtual glm::vec3 getAbsoluteJointTranslationInObjectFrame(int index) const override { return glm::vec3(0.0f); }
    virtual int getJointParent(int index) const override { return -1; }

    virtual bool setLocalJointRotation(int index, const glm::quat& rotation) override { return false; }
    virtual bool setLocalJointTranslation(int index, const glm::vec3& translation) override { return false; }

    virtual int getJointIndex(const QString& name) const { return -1; }
    virtual QStringList getJointNames() const { return QStringList(); }

    virtual void loader() {} // called indirectly when urls for geometry are updated

    /// Should the external entity script mechanism call a preload for this entity.
    /// Due to the asyncronous nature of signals for add entity and script changing
    /// it's possible for two similar signals to cross paths. This method allows the
    /// entity to definitively state if the preload signal should be sent.
    ///
    /// We only want to preload if:
    ///    there is some script, and either the script value or the scriptTimestamp
    ///    value have changed since our last preload
    bool shouldPreloadScript() const;
    void scriptHasPreloaded();
    void scriptHasUnloaded();
    void setScriptHasFinishedPreload(bool value);
    bool isScriptPreloadFinished();
    virtual bool isWearable() const;
    bool isDomainEntity() const { return _hostType == entity::HostType::DOMAIN; }
    bool isAvatarEntity() const { return _hostType == entity::HostType::AVATAR; }
    bool isMyAvatarEntity() const;
    bool isLocalEntity() const { return _hostType == entity::HostType::LOCAL; }
    entity::HostType getEntityHostType() const { return _hostType; }
    virtual void setEntityHostType(entity::HostType hostType) { _hostType = hostType; }

    // if this entity is an avatar entity, which avatar is it associated with?
    QUuid getOwningAvatarID() const { return _owningAvatarID; }
    QUuid getOwningAvatarIDForProperties() const;
    void setOwningAvatarID(const QUuid& owningAvatarID);

    virtual bool wantsHandControllerPointerEvents() const { return false; }
    virtual bool wantsKeyboardFocus() const { return false; }
    virtual void setProxyWindow(QWindow* proxyWindow) {}
    virtual QObject* getEventHandler() { return nullptr; }

    QUuid getLastEditedBy() const { return _lastEditedBy; }
    void setLastEditedBy(QUuid value) { _lastEditedBy = value; }

    virtual bool matchesJSONFilters(const QJsonObject& jsonFilters) const;

    virtual bool getMeshes(MeshProxyList& result) { return true; }

    virtual void locationChanged(bool tellPhysics = true, bool tellChildren = true) override;

    virtual bool getScalesWithParent() const override;

    using ChangeHandlerCallback = std::function<void(const EntityItemID&)>;
    using ChangeHandlerId = QUuid;
    ChangeHandlerId registerChangeHandler(const ChangeHandlerCallback& handler);
    void deregisterChangeHandler(const ChangeHandlerId& changeHandlerId);

    void collectChildrenForDelete(std::vector<EntityItemPointer>& entitiesToDelete, const QUuid& sessionID) const;

    float getBoundingRadius() const { return _boundingRadius; }
    void setSpaceIndex(int32_t index);
    int32_t getSpaceIndex() const { return _spaceIndex; }

    virtual void preDelete();
    virtual void postParentFixup() {}

    void setSimulationOwnershipExpiry(uint64_t expiry) { _simulationOwnershipExpiry = expiry; }
    uint64_t getSimulationOwnershipExpiry() const { return _simulationOwnershipExpiry; }

    void addCloneID(const QUuid& cloneID);
    void removeCloneID(const QUuid& cloneID);
    const QVector<QUuid> getCloneIDs() const;
    void setCloneIDs(const QVector<QUuid>& cloneIDs);
    void setVisuallyReady(bool visuallyReady) { _visuallyReady = visuallyReady; }

    const GrabPropertyGroup& getGrabProperties() const { return _grabProperties; }

    void prepareForSimulationOwnershipBid(EntityItemProperties& properties, uint64_t now, uint8_t priority);

    virtual void addGrab(GrabPointer grab) override;
    virtual void removeGrab(GrabPointer grab) override;
    virtual void disableGrab(GrabPointer grab) override;

    bool stillHasMyGrab() const;

    bool needsRenderUpdate() const { return _needsRenderUpdate; }
    void setNeedsRenderUpdate(bool needsRenderUpdate) { _needsRenderUpdate = needsRenderUpdate; }

    void setRenderWithZones(const QVector<QUuid>& renderWithZones);
    QVector<QUuid> getRenderWithZones() const;
    bool needsZoneOcclusionUpdate() const { return _needsZoneOcclusionUpdate; }
    void resetNeedsZoneOcclusionUpdate() { withWriteLock([&] { _needsZoneOcclusionUpdate = false; }); }

    void setBillboardMode(BillboardMode value);
    BillboardMode getBillboardMode() const;
    virtual bool getRotateForPicking() const { return false; }

signals:
    void spaceUpdate(std::pair<int32_t, glm::vec4> data);

protected:
    QHash<ChangeHandlerId, ChangeHandlerCallback> _changeHandlers;

    void somethingChangedNotification();

    void setSimulated(bool simulated) { _simulated = simulated; }

    const QByteArray getDynamicDataInternal() const;
    bool stillHasGrab() const;
    void setDynamicDataInternal(QByteArray dynamicData);

    virtual void dimensionsChanged() override;

    glm::vec3 _unscaledDimensions { ENTITY_ITEM_DEFAULT_DIMENSIONS };
    EntityTypes::EntityType _type { EntityTypes::Unknown };
    quint64 _lastSimulated { 0 }; // last time this entity called simulate(), this includes velocity, angular velocity,
                            // and physics changes
    quint64 _lastUpdated { 0 }; // last time this entity called update(), this includes animations and non-physics changes
    quint64 _lastEdited { 0 }; // last official local or remote edit time
    QUuid _lastEditedBy { ENTITY_ITEM_DEFAULT_LAST_EDITED_BY }; // id of last editor
    quint64 _lastBroadcast; // the last time we sent an edit packet about this entity

    quint64 _lastEditedFromRemote { 0 }; // last time we received and edit from the server
    quint64 _lastEditedFromRemoteInRemoteTime { 0 }; // last time we received an edit from the server (in server-time-frame)
    quint64 _created { 0 };
    quint64 _changedOnServer { 0 };

    mutable AABox _cachedAABox;
    mutable AACube _maxAACube;
    mutable AACube _minAACube;
    mutable bool _recalcAABox { true };
    mutable bool _recalcMinAACube { true };
    mutable bool _recalcMaxAACube { true };

    float _density { ENTITY_ITEM_DEFAULT_DENSITY }; // kg/m^3
    // NOTE: _volumeMultiplier is used to allow some mass properties code exist in the EntityItem base class
    // rather than in all of the derived classes.  If we ever collapse these classes to one we could do it a
    // different way.
    float _volumeMultiplier { 1.0f };
    glm::vec3 _gravity { ENTITY_ITEM_DEFAULT_GRAVITY };
    glm::vec3 _acceleration { ENTITY_ITEM_DEFAULT_ACCELERATION };
    float _damping { ENTITY_ITEM_DEFAULT_DAMPING };
    float _restitution { ENTITY_ITEM_DEFAULT_RESTITUTION };
    float _friction { ENTITY_ITEM_DEFAULT_FRICTION };
    float _lifetime { ENTITY_ITEM_DEFAULT_LIFETIME };

    QString _script { ENTITY_ITEM_DEFAULT_SCRIPT }; /// the value of the script property
    QString _loadedScript; /// the value of _script when the last preload signal was sent
    quint64 _scriptTimestamp { ENTITY_ITEM_DEFAULT_SCRIPT_TIMESTAMP }; /// the script loaded property used for forced reload
    bool _scriptPreloadFinished { false };

    QString _serverScripts;
    /// keep track of time when _serverScripts property was last changed
    quint64 _serverScriptsChangedTimestamp { ENTITY_ITEM_DEFAULT_SCRIPT_TIMESTAMP };

    /// the value of _scriptTimestamp when the last preload signal was sent
    // NOTE: on construction we want this to be different from _scriptTimestamp so we intentionally bump it
    quint64 _loadedScriptTimestamp { ENTITY_ITEM_DEFAULT_SCRIPT_TIMESTAMP + 1 };

    QString _collisionSoundURL { ENTITY_ITEM_DEFAULT_COLLISION_SOUND_URL };
    glm::vec3 _registrationPoint { ENTITY_ITEM_DEFAULT_REGISTRATION_POINT };
    float _angularDamping { ENTITY_ITEM_DEFAULT_ANGULAR_DAMPING };
    bool _visible { ENTITY_ITEM_DEFAULT_VISIBLE };
    bool _isVisibleInSecondaryCamera { ENTITY_ITEM_DEFAULT_VISIBLE_IN_SECONDARY_CAMERA };
    RenderLayer _renderLayer { RenderLayer::WORLD };
    PrimitiveMode _primitiveMode { PrimitiveMode::SOLID };
    bool _canCastShadow{ ENTITY_ITEM_DEFAULT_CAN_CAST_SHADOW };
    bool _ignorePickIntersection { false };
    bool _collisionless { ENTITY_ITEM_DEFAULT_COLLISIONLESS };
    uint16_t _collisionMask { ENTITY_COLLISION_MASK_DEFAULT };
    bool _dynamic { ENTITY_ITEM_DEFAULT_DYNAMIC };
    bool _locked { ENTITY_ITEM_DEFAULT_LOCKED };
    QString _userData { ENTITY_ITEM_DEFAULT_USER_DATA };
    QString _privateUserData{ ENTITY_ITEM_DEFAULT_PRIVATE_USER_DATA };
    SimulationOwner _simulationOwner;
    bool _shouldHighlight { false };
    QString _name { ENTITY_ITEM_DEFAULT_NAME };
    QString _href; //Hyperlink href
    QString _description; //Hyperlink description


    // NOTE: Damping is applied like this:  v *= pow(1 - damping, dt)
    //
    // Hence the damping coefficient must range from 0 (no damping) to 1 (immediate stop).
    // Each damping value relates to a corresponding exponential decay timescale as follows:
    //
    // timescale = -1 / ln(1 - damping)
    //
    // damping = 1 - exp(-1 / timescale)
    //

    // DirtyFlags are set whenever a property changes that the EntitySimulation needs to know about.
    std::atomic_uint _flags { 0 };   // things that have changed from EXTERNAL changes (via script or packet) but NOT from simulation

    // these backpointers are only ever set/cleared by friends:
    EntityTreeElementPointer _element; // set by EntityTreeElement
    void* _physicsInfo { nullptr }; // set by EntitySimulation
    bool _simulated { false }; // set by EntitySimulation
    bool _visuallyReady { true };

    void enableNoBootstrap();
    void disableNoBootstrap();

    bool addActionInternal(EntitySimulationPointer simulation, EntityDynamicPointer action);
    bool removeActionInternal(const QUuid& actionID, EntitySimulationPointer simulation = nullptr);
    void deserializeActionsInternal();
    void serializeActions(bool& success, QByteArray& result) const;
    QHash<QUuid, EntityDynamicPointer> _objectActions;

    static int _maxActionsDataSize;
    mutable QByteArray _allActionsDataCache;

    // when an entity-server starts up, EntityItem::setDynamicData is called before the entity-tree is
    // ready.  This means we can't find our EntityItemPointer or add the action to the simulation.  These
    // are used to keep track of and work around this situation.
    void checkWaitingToRemove(EntitySimulationPointer simulation = nullptr);
    mutable QSet<QUuid> _actionsToRemove;
    mutable bool _dynamicDataDirty { false };
    mutable bool _dynamicDataNeedsTransmit { false };
    // _previouslyDeletedActions is used to avoid an action being re-added due to server round-trip lag
    static quint64 _rememberDeletedActionTime;
    mutable QHash<QUuid, quint64> _previouslyDeletedActions;

    QUuid _sourceUUID; /// the server node UUID we came from

    entity::HostType _hostType { entity::HostType::DOMAIN };
    bool _transitingWithAvatar{ false };
    QUuid _owningAvatarID;

    // physics related changes from the network to suppress any duplicates and make
    // sure redundant applications are idempotent
    glm::vec3 _lastUpdatedPositionValue;
    glm::quat _lastUpdatedRotationValue;
    glm::vec3 _lastUpdatedVelocityValue;
    glm::vec3 _lastUpdatedAngularVelocityValue;
    glm::vec3 _lastUpdatedAccelerationValue;
    AACube _lastUpdatedQueryAACubeValue;

    quint64 _lastUpdatedPositionTimestamp { 0 };
    quint64 _lastUpdatedRotationTimestamp { 0 };
    quint64 _lastUpdatedVelocityTimestamp { 0 };
    quint64 _lastUpdatedAngularVelocityTimestamp { 0 };
    quint64 _lastUpdatedAccelerationTimestamp { 0 };
    quint64 _lastUpdatedQueryAACubeTimestamp { 0 };
    uint64_t _simulationOwnershipExpiry { 0 };

    float _boundingRadius { 0.0f };
    int32_t _spaceIndex { -1 }; // index to proxy in workload::Space

    // TODO: move this "scriptSimulationPriority" and "pendingOwnership" stuff into EntityMotionState
    // but first would need to do some other cleanup. In the meantime these live here as "scratch space"
    // to allow libs that don't know about each other to communicate.
    uint64_t _pendingOwnershipTimestamp { 0 }; // timestamp of last owenership change request
    uint8_t _pendingOwnershipPriority { 0 }; // priority of last ownership change request
    uint8_t _pendingOwnershipState { 0 }; // TAKE or RELEASE
    uint8_t _scriptSimulationPriority { 0 }; // target priority based on script operations

    bool _cauterized { false }; // if true, don't draw because it would obscure 1st-person camera

    bool _cloneable { ENTITY_ITEM_DEFAULT_CLONEABLE };
    float _cloneLifetime { ENTITY_ITEM_DEFAULT_CLONE_LIFETIME };
    float _cloneLimit { ENTITY_ITEM_DEFAULT_CLONE_LIMIT };
    bool _cloneDynamic { ENTITY_ITEM_DEFAULT_CLONE_DYNAMIC };
    bool _cloneAvatarEntity { ENTITY_ITEM_DEFAULT_CLONE_AVATAR_ENTITY };
    QUuid _cloneOriginID;
    QVector<QUuid> _cloneIDs;

    GrabPropertyGroup _grabProperties;

    QHash<QUuid, EntityDynamicPointer> _grabActions;

    QVector<QUuid> _renderWithZones;
    mutable bool _needsZoneOcclusionUpdate { false };

    BillboardMode _billboardMode { BillboardMode::NONE };

    bool _cullWithParent { false };

    mutable bool _needsRenderUpdate { false };
};

#endif // hifi_EntityItem_h
