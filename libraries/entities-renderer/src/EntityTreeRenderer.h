//
//  EntityTreeRenderer.h
//  interface/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityTreeRenderer_h
#define hifi_EntityTreeRenderer_h

#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtGui/QMouseEvent>
#include <QtCore/QSharedPointer>

#include <AudioInjectorManager.h>
#include <EntityScriptingInterface.h> // for RayToEntityIntersectionResult
#include <EntityTree.h>
#include <PointerEvent.h>
#include <ScriptCache.h>
#include <TextureCache.h>
#include <OctreeProcessor.h>
#include <render/Forward.h>
#include <workload/Space.h>

class AbstractScriptingServicesInterface;
class AbstractViewStateInterface;
class Model;
class ScriptEngine;
class ZoneEntityItem;
class EntityItem;
class ScriptEngine;
class ScriptManager;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;

namespace render { namespace entities {
    class EntityRenderer;
    using EntityRendererPointer = std::shared_ptr<EntityRenderer>;
    using EntityRendererWeakPointer = std::weak_ptr<EntityRenderer>;

} }

using EntityRenderer = render::entities::EntityRenderer;
using EntityRendererPointer = render::entities::EntityRendererPointer;
using EntityRendererWeakPointer = render::entities::EntityRendererWeakPointer;
class Model;
using ModelPointer = std::shared_ptr<Model>;
using ModelWeakPointer = std::weak_ptr<Model>;

using CalculateEntityLoadingPriority = std::function<float(const EntityItem& item)>;

// Generic client side Octree renderer class.
class EntityTreeRenderer : public OctreeProcessor, public Dependency {
    Q_OBJECT
public:
    static void setEntitiesShouldFadeFunction(std::function<bool()> func) { _entitiesShouldFadeFunction = func; }
    static std::function<bool()> getEntitiesShouldFadeFunction() { return _entitiesShouldFadeFunction; }

    EntityTreeRenderer(bool wantScripts, AbstractViewStateInterface* viewState,
                                AbstractScriptingServicesInterface* scriptingServices);
    virtual ~EntityTreeRenderer();

    QSharedPointer<EntityTreeRenderer> getSharedFromThis() {
        return qSharedPointerCast<EntityTreeRenderer>(sharedFromThis());
    }

    virtual char getMyNodeType() const override { return NodeType::EntityServer; }
    virtual PacketType getMyQueryMessageType() const override { return PacketType::EntityQuery; }
    virtual PacketType getExpectedPacketType() const override { return PacketType::EntityData; }

    // Returns the priority at which an entity should be loaded. Higher values indicate higher priority.
    static CalculateEntityLoadingPriority getEntityLoadingPriorityOperator() { return _calculateEntityLoadingPriorityFunc; }
    static float getEntityLoadingPriority(const EntityItem& item) { return _calculateEntityLoadingPriorityFunc(item); }
    static void setEntityLoadingPriorityFunction(CalculateEntityLoadingPriority fn) { _calculateEntityLoadingPriorityFunc = fn; }

    void setMouseRayPickID(unsigned int rayPickID) { _mouseRayPickID = rayPickID; }
    unsigned int getMouseRayPickID() { return _mouseRayPickID; }
    void setMouseRayPickResultOperator(std::function<RayToEntityIntersectionResult(unsigned int)> getPrevRayPickResultOperator) { _getPrevRayPickResultOperator = getPrevRayPickResultOperator;  }
    void setSetPrecisionPickingOperator(std::function<void(unsigned int, bool)> setPrecisionPickingOperator) { _setPrecisionPickingOperator = setPrecisionPickingOperator; }

    void shutdown();
    void preUpdate();
    void update(bool simulate);

    EntityTreePointer getTree() { return std::static_pointer_cast<EntityTree>(_tree); }

    void processEraseMessage(ReceivedMessage& message, const SharedNodePointer& sourceNode);

    virtual void init() override;

    /// clears the tree
    virtual void clearDomainAndNonOwnedEntities() override;
    virtual void clear() override;

    /// reloads the entity scripts, calling unload and preload
    void reloadEntityScripts();

    void fadeOutRenderable(const EntityRendererPointer& renderable);

    // event handles which may generate entity related events
    QUuid mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseDoublePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);

    /// connect our signals to anEntityScriptingInterface for firing of events related clicking,
    /// hovering over, and entering entities
    void connectSignalsToSlots(EntityScriptingInterface* entityScriptingInterface);

    // For Scene.shouldRenderEntities
    QList<EntityItemID>& getEntitiesLastInScene() { return _entityIDsLastInScene; }

    std::pair<bool, bool> getZoneInteractionProperties();

    bool wantsKeyboardFocus(const EntityItemID& id) const;
    QObject* getEventHandler(const EntityItemID& id);
    bool wantsHandControllerPointerEvents(const EntityItemID& id) const;
    void setProxyWindow(const EntityItemID& id, QWindow* proxyWindow);
    void setCollisionSound(const EntityItemID& id, const SharedSoundPointer& sound);
    EntityItemPointer getEntity(const EntityItemID& id);
    void deleteEntity(const EntityItemID& id) const;
    void onEntityChanged(const EntityItemID& id);

    // Access the workload Space
    workload::SpacePointer getWorkloadSpace() const { return _space; }

    EntityEditPacketSender* getPacketSender();

    static void setAddMaterialToEntityOperator(std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> addMaterialToEntityOperator) { _addMaterialToEntityOperator = addMaterialToEntityOperator; }
    static void setRemoveMaterialFromEntityOperator(std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> removeMaterialFromEntityOperator) { _removeMaterialFromEntityOperator = removeMaterialFromEntityOperator; }
    static bool addMaterialToEntity(const QUuid& entityID, graphics::MaterialLayer material, const std::string& parentMaterialName);
    static bool removeMaterialFromEntity(const QUuid& entityID, graphics::MaterialPointer material, const std::string& parentMaterialName);

    static void setAddMaterialToAvatarOperator(std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> addMaterialToAvatarOperator) { _addMaterialToAvatarOperator = addMaterialToAvatarOperator; }
    static void setRemoveMaterialFromAvatarOperator(std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> removeMaterialFromAvatarOperator) { _removeMaterialFromAvatarOperator = removeMaterialFromAvatarOperator; }
    static bool addMaterialToAvatar(const QUuid& avatarID, graphics::MaterialLayer material, const std::string& parentMaterialName);
    static bool removeMaterialFromAvatar(const QUuid& avatarID, graphics::MaterialPointer material, const std::string& parentMaterialName);

    size_t getPrevNumEntityUpdates() const { return _prevNumEntityUpdates; }
    size_t getPrevTotalNeededEntityUpdates() const { return _prevTotalNeededEntityUpdates; }

signals:
    void enterEntity(const EntityItemID& entityItemID);
    void leaveEntity(const EntityItemID& entityItemID);
    void collisionWithEntity(const EntityItemID& idA, const EntityItemID& idB, const Collision& collision);

public slots:
    void addingEntity(const EntityItemID& entityID);
    void deletingEntity(const EntityItemID& entityID);
    void entityScriptChanging(const EntityItemID& entityID, const bool reload);
    void entityCollisionWithEntity(const EntityItemID& idA, const EntityItemID& idB, const Collision& collision);
    void updateEntityRenderStatus(bool shouldRenderEntities);
    void updateZone(const EntityItemID& id);

    // optional slots that can be wired to menu items
    void setDisplayModelBounds(bool value) { _displayModelBounds = value; }
    void setPrecisionPicking(bool value) { _setPrecisionPickingOperator(_mouseRayPickID, value); }
    EntityRendererPointer renderableForEntityId(const EntityItemID& id) const;
    render::ItemID renderableIdForEntityId(const EntityItemID& id) const;

    void handleSpaceUpdate(std::pair<int32_t, glm::vec4> proxyUpdate);

protected:
    virtual OctreePointer createTree() override {
        EntityTreePointer newTree = EntityTreePointer(new EntityTree(true));
        newTree->createRootElement();
        return newTree;
    }

private:
    void addPendingEntities(const render::ScenePointer& scene, render::Transaction& transaction);
    void updateChangedEntities(const render::ScenePointer& scene, render::Transaction& transaction);
    EntityRendererPointer renderableForEntity(const EntityItemPointer& entity) const { return renderableForEntityId(entity->getID()); }
    render::ItemID renderableIdForEntity(const EntityItemPointer& entity) const { return renderableIdForEntityId(entity->getID()); }

    void resetPersistentEntitiesScriptEngine();
    void resetNonPersistentEntitiesScriptEngine();
    void setupEntityScriptEngineSignals(const ScriptManagerPointer& scriptManager);

    void findBestZoneAndMaybeContainingEntities(QSet<EntityItemID>& entitiesContainingAvatar);

    bool applyLayeredZones();
    void stopDomainAndNonOwnedEntities();

    void checkAndCallPreload(const EntityItemID& entityID, bool reload = false, bool unloadFirst = false);

    EntityItemID _currentHoverOverEntityID;
    EntityItemID _currentClickingOnEntityID;

    ScriptValueList createEntityArgs(const EntityItemID& entityID);
    void checkEnterLeaveEntities();
    void leaveDomainAndNonOwnedEntities();
    void leaveAllEntities();
    void forceRecheckEntities();

    glm::vec3 _avatarPosition { 0.0f };
    bool _forceRecheckEntities { true };
    QSet<EntityItemID> _currentEntitiesInside;

    bool _wantScripts;
    ScriptManagerPointer _nonPersistentEntitiesScriptManager; // used for domain + non-owned avatar entities, cleared on domain switch
    ScriptManagerPointer _persistentEntitiesScriptManager; // used for local + owned avatar entities, persists on domain switch, cleared on reload content

    void playEntityCollisionSound(const EntityItemPointer& entity, const Collision& collision);

    bool _lastPointerEventValid;
    PointerEvent _lastPointerEvent;
    AbstractViewStateInterface* _viewState;
    AbstractScriptingServicesInterface* _scriptingServices;
    bool _displayModelBounds;

    bool _shuttingDown { false };

    QMultiMap<QUrl, EntityItemID> _waitingOnPreload;

    unsigned int _mouseRayPickID;
    std::function<RayToEntityIntersectionResult(unsigned int)> _getPrevRayPickResultOperator;
    std::function<void(unsigned int, bool)> _setPrecisionPickingOperator;

    class LayeredZone {
    public:
        LayeredZone(std::shared_ptr<ZoneEntityItem> zone) : zone(zone), id(zone->getID()), volume(zone->getVolumeEstimate()) {}

        // We need to sort on volume AND id so that different clients sort zones with identical volumes the same way
        bool operator<(const LayeredZone& r) const { return volume < r.volume || (volume == r.volume && id < r.id); }
        bool operator==(const LayeredZone& r) const { return zone.lock() && zone.lock() == r.zone.lock(); }
        bool operator!=(const LayeredZone& r) const { return !(*this == r); }
        bool operator<=(const LayeredZone& r) const { return (*this < r) || (*this == r); }

        std::weak_ptr<ZoneEntityItem> zone;
        QUuid id;
        float volume;
    };

    class LayeredZones : public std::vector<LayeredZone> {
    public:
        bool clearDomainAndNonOwnedZones();

        void sort() { std::sort(begin(), end(), std::less<LayeredZone>()); }
        bool equals(const LayeredZones& other) const;
        bool update(std::shared_ptr<ZoneEntityItem> zone, const glm::vec3& position, EntityTreeRenderer* entityTreeRenderer);

        void appendRenderIDs(render::ItemIDs& list, EntityTreeRenderer* entityTreeRenderer) const;
        std::pair<bool, bool> getZoneInteractionProperties() const;
    };

    LayeredZones _layeredZones;
    uint64_t _lastZoneCheck { 0 };
    const uint64_t ZONE_CHECK_INTERVAL = USECS_PER_MSEC * 100; // ~10hz
    const float ZONE_CHECK_DISTANCE = 0.001f;

    float _avgRenderableUpdateCost { 0.0f };

    ReadWriteLockable _changedEntitiesGuard;
    std::unordered_set<EntityItemID> _changedEntities;
    size_t _prevNumEntityUpdates { 0 };
    size_t _prevTotalNeededEntityUpdates { 0 };

    std::unordered_set<EntityRendererPointer> _renderablesToUpdate;
    std::unordered_map<EntityItemID, EntityRendererPointer> _entitiesInScene;
    std::unordered_map<EntityItemID, EntityItemWeakPointer> _entitiesToAdd;

    // For Scene.shouldRenderEntities
    QList<EntityItemID> _entityIDsLastInScene;

    static int _entitiesScriptEngineCount;
    static CalculateEntityLoadingPriority _calculateEntityLoadingPriorityFunc;
    static std::function<bool()> _entitiesShouldFadeFunction;

    mutable std::mutex _spaceLock;
    workload::SpacePointer _space{ new workload::Space() };
    workload::Transaction::Updates _spaceUpdates;

    static std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> _addMaterialToEntityOperator;
    static std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> _removeMaterialFromEntityOperator;
    static std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> _addMaterialToAvatarOperator;
    static std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> _removeMaterialFromAvatarOperator;

};


#endif // hifi_EntityTreeRenderer_h
