//
//  EntityScriptingInterface.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
// TODO: How will we handle collision callbacks with Entities
//

#ifndef hifi_EntityScriptingInterface_h
#define hifi_EntityScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtQml/QJSValue>
#include <QtQml/QJSValueList>
#include <QtCore/QSharedPointer>

#include <DependencyManager.h>
#include <Octree.h>
#include <OctreeScriptingInterface.h>
#include <RegisteredMetaTypes.h>
#include "PointerEvent.h"
#include <PickFilter.h>
#include "ScriptManager.h"

#include "PolyVoxEntityItem.h"
#include "LineEntityItem.h"
#include "PolyLineEntityItem.h"
#include "EntityTree.h"

#include "EntityEditPacketSender.h"
#include "EntitiesScriptEngineProvider.h"
#include "EntityItemProperties.h"

class EntityTree;
class MeshProxy;
class ScriptContext;
class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

extern const QString GRABBABLE_USER_DATA;
extern const QString NOT_GRABBABLE_USER_DATA;

// helper factory to compose standardized, async metadata queries for "magic" Entity properties
// like .script and .serverScripts.  This is used for automated testing of core scripting features
// as well as to provide early adopters a self-discoverable, consistent way to diagnose common
// problems with their own Entity scripts.
class EntityPropertyMetadataRequest {
public:
    EntityPropertyMetadataRequest(ScriptManager* manager) : _manager(manager == nullptr ? nullptr : manager){};
    bool script(EntityItemID entityID, ScriptValuePointer handler);
    bool serverScripts(EntityItemID entityID, ScriptValuePointer handler);
private:
    QPointer<ScriptManager> _manager;
};

/*@jsdoc
 * The result of a {@link Entities.findRayIntersection|findRayIntersection} search using a {@link PickRay}.
 * @typedef {object} Entities.RayToEntityIntersectionResult
 * @property {boolean} intersects - <code>true</code> if the {@link PickRay} intersected an entity, <code>false</code> if it 
 *     didn't.
 * @property {boolean} accurate - Is always <code>true</code>.
 * @property {Uuid} entityID - The ID if the entity intersected, if any, otherwise <code>null</code>.
 * @property {number} distance - The distance from the {@link PickRay} origin to the intersection point.
 * @property {Vec3} intersection - The intersection point.
 * @property {Vec3} surfaceNormal - The surface normal of the entity at the intersection point.
 * @property {BoxFace} face - The face of the entity's axis-aligned box that the ray intersects.
 * @property {object} extraInfo - Extra information depending on the entity intersected. Currently, only <code>Model</code> 
 *     entities provide extra information, and the information provided depends on the <code>precisionPicking</code> parameter 
 *     value that the search function was called with.
 */
// "accurate" is currently always true because the ray intersection is always performed with an Octree::Lock.
class RayToEntityIntersectionResult {
public:
    bool intersects { false };
    bool accurate { true };
    QUuid entityID;
    float distance { 0.0f };
    BoxFace face { UNKNOWN_FACE };
    glm::vec3 intersection;
    glm::vec3 surfaceNormal;
    QVariantMap extraInfo;
};
Q_DECLARE_METATYPE(RayToEntityIntersectionResult)
ScriptValuePointer RayToEntityIntersectionResultToScriptValue(ScriptEngine* engine, const RayToEntityIntersectionResult& results);
void RayToEntityIntersectionResultFromScriptValue(const ScriptValuePointer& object, RayToEntityIntersectionResult& results);

class ParabolaToEntityIntersectionResult {
public:
    bool intersects { false };
    bool accurate { true };
    QUuid entityID;
    float distance { 0.0f };
    float parabolicDistance { 0.0f };
    BoxFace face { UNKNOWN_FACE };
    glm::vec3 intersection;
    glm::vec3 surfaceNormal;
    QVariantMap extraInfo;
};

/*@jsdoc
 * The <code>Entities</code> API provides facilities to create and interact with entities. Entities are 2D or 3D objects 
 * displayed in-world. Depending on their {@link Entities.EntityHostType|EntityHostType}, they may persist in the domain as 
 * "domain" entities, travel to different domains with a user as "avatar" entities, or be visible only to an individual user as 
 * "local" entities (a.k.a. "overlays").
 *
 * <p>Note: For Interface, avatar, and client entity scripts, the entities available to scripts are those that Interface has 
 * displayed and so knows about. For assignment client scripts, the entities available are those that are "seen" by the 
 * {@link EntityViewer}. For entity server scripts, all entities are available.</p>
 *
 * <h3>Entity Types & Properties</h3>
 *
 * <p>For a list of the entity types that you can use, see {@link Entities.EntityType|Entity Types}.</p>
 * 
 * <p>For the properties of the different entity types, see {@link Entities.EntityProperties|Entity Properties}. Some properties are universal to all entity types, and some are specific to particular entity types.</p>
 *
 * <h3>Entity Methods</h3>
 *
 * <p>Some of the API's signals correspond to entity methods that are called, if present, in the entity being interacted with. 
 * The client or server entity script must expose them as a property. However, unlike {@link Entities.callEntityMethod}, server 
 * entity scripts do not need to list them in an <code>remotelyCallable</code> property. The entity methods are called with 
 * parameters per their corresponding signal.</p>
 * <table>
 *   <thead>
 *     <tr><th>Method Name</th><th>Corresponding Signal</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>clickDownOnEntity</code></td><td>{@link Entities.clickDownOnEntity}</td></tr>
 *     <tr><td><code>clickReleaseOnEntity</code></td><td>{@link Entities.clickReleaseOnEntity}</td></tr>
 *     <tr><td><code>collisionWithEntity</code></td><td>{@link Entities.collisionWithEntity}</td></tr>
 *     <tr><td><code>enterEntity</code></td><td>{@link Entities.enterEntity}</td></tr>
 *     <tr><td><code>holdingClickOnEntity</code></td><td>{@link Entities.holdingClickOnEntity}</td></tr>
 *     <tr><td><code>hoverEnterEntity</code></td><td>{@link Entities.hoverEnterEntity}</td></tr>
 *     <tr><td><code>hoverLeaveEntity</code></td><td>{@link Entities.hoverLeaveEntity}</td></tr>
 *     <tr><td><code>hoverOverEntity</code></td><td>{@link Entities.hoverOverEntity}</td></tr>
 *     <tr><td><code>leaveEntity</code></td><td>{@link Entities.leaveEntity}</td></tr>
 *     <tr><td><code>mouseDoublePressOnEntity</code></td><td>{@link Entities.mouseDoublePressOnEntity}</td></tr>
 *     <tr><td><code>mouseMoveOnEntity</code></td><td>{@link Entities.mouseMoveOnEntity}</td></tr>
 *     <tr><td><code>mouseMoveEvent</code></td><td><span class="important">Deprecated: Use <code>mouseMoveOnEntity</code> 
 *       instead.</span></td></tr>
 *     <tr><td><code>mousePressOnEntity</code></td><td>{@link Entities.mousePressOnEntity}</td></tr>
 *     <tr><td><code>mouseReleaseOnEntity</code></td><td>{@link Entities.mouseReleaseOnEntity}</td></tr>
 *   </tbody>
 * </table>
 * <p>See {@link Entities.clickDownOnEntity} for an example.</p>
 *
 * @namespace Entities
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 *
 * @property {Uuid} keyboardFocusEntity -  The {@link Entities.EntityProperties-Web|Web} entity that has keyboard focus. If no 
 *     Web entity has keyboard focus, returns <code>null</code>; set to <code>null</code> or {@link Uuid(0)|Uuid.NULL} to clear 
 *     keyboard focus.
 */
/// handles scripting of Entity commands from JS passed to assigned clients
class EntityScriptingInterface : public OctreeScriptingInterface, public Dependency  {
    Q_OBJECT
    Q_PROPERTY(QUuid keyboardFocusEntity READ getKeyboardFocusEntity WRITE setKeyboardFocusEntity)

    friend EntityPropertyMetadataRequest;
public:
    EntityScriptingInterface(bool bidOnSimulationOwnership);

    class ActivityTracking {
    public:
        int addedEntityCount { 0 };
        int deletedEntityCount { 0 };
        int editedEntityCount { 0 };
    };

    EntityEditPacketSender* getEntityPacketSender() const { return (EntityEditPacketSender*)getPacketSender(); }
    virtual NodeType_t getServerNodeType() const override { return NodeType::EntityServer; }
    virtual OctreeEditPacketSender* createPacketSender() override { return new EntityEditPacketSender(); }

    void setEntityTree(EntityTreePointer modelTree);
    EntityTreePointer getEntityTree() { return _entityTree; }
    void setPersistentEntitiesScriptEngine(QSharedPointer<EntitiesScriptEngineProvider> manager);
    void setNonPersistentEntitiesScriptEngine(QSharedPointer<EntitiesScriptEngineProvider> manager);

    void resetActivityTracking();
    ActivityTracking getActivityTracking() const { return _activityTracking; }

    RayToEntityIntersectionResult evalRayIntersectionVector(const PickRay& ray, PickFilter searchFilter,
        const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard);
    ParabolaToEntityIntersectionResult evalParabolaIntersectionVector(const PickParabola& parabola, PickFilter searchFilter,
        const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard);

    /*@jsdoc
     * Gets the properties of multiple entities.
     * @function Entities.getMultipleEntityProperties
     * @param {Uuid[]} entityIDs - The IDs of the entities to get the properties of.
     * @param {string[]|string} [desiredProperties=[]] - The name or names of the properties to get. For properties that are 
     *     objects (e.g., the <code>"keyLight"</code> property), use the property and subproperty names in dot notation (e.g., 
     *     <code>"keyLight.color"</code>).
     * @returns {Entities.EntityProperties[]} The specified properties of each entity for each entity that can be found. If 
     *     none of the entities can be found, then an empty array is returned. If no properties are specified, then all 
     *     properties are returned.
     * @example <caption>Retrieve the names of the nearby entities</caption>
     * var SEARCH_RADIUS = 50; // meters
     * var entityIDs = Entities.findEntities(MyAvatar.position, SEARCH_RADIUS);
     * var propertySets = Entities.getMultipleEntityProperties(entityIDs, "name");
     * print("Nearby entity names: " + JSON.stringify(propertySets));
    */
    static ScriptValuePointer getMultipleEntityProperties(ScriptContext* context, ScriptEngine* engine);
    ScriptValuePointer getMultipleEntityPropertiesInternal(ScriptEngine* engine, QVector<QUuid> entityIDs, const ScriptValuePointer& extendedDesiredProperties);

    QUuid addEntityInternal(const EntityItemProperties& properties, entity::HostType entityHostType);

public slots:

    /*@jsdoc
     * Checks whether or not the script can change the <code>locked</code> property of entities. Locked entities have their
     * <code>locked</code> property set to <code>true</code> and cannot be edited or deleted.
     * @function Entities.canAdjustLocks
     * @returns {boolean} <code>true</code> if the domain server will allow the script to change the <code>locked</code> 
     *     property of entities, otherwise <code>false</code>.
     * @example <caption>Lock an entity if you can.</caption>
     * if (Entities.canAdjustLocks()) {
     *     Entities.editEntity(entityID, { locked: true });
     * } else {
     *     Window.alert("You do not have the permissions to set an entity locked!");
     * }
     */
    Q_INVOKABLE bool canAdjustLocks();

    /*@jsdoc
     * Checks whether or not the script can rez (create) new entities in the domain.
     * @function Entities.canRez
     * @returns {boolean} <code>true</code> if the domain server will allow the script to rez (create) new entities, otherwise 
     *     <code>false</code>.
     */
    Q_INVOKABLE bool canRez();

    /*@jsdoc
     * Checks whether or not the script can rez (create) new temporary entities in the domain. Temporary entities are entities 
     * with a finite <code>lifetime</code> property value set.
     * @function Entities.canRezTmp
     * @returns {boolean} <code>true</code> if the domain server will allow the script to rez (create) new temporary entities,
     *     otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canRezTmp();

    /*@jsdoc
     * Checks whether or not the script can rez (create) new certified entities in the domain. Certified entities are entities 
     * that have PoP certificates.
     * @function Entities.canRezCertified
     * @returns {boolean} <code>true</code> if the domain server will allow the script to rez (create) new certified entities,
     *     otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canRezCertified();

    /*@jsdoc
     * Checks whether or not the script can rez (create) new temporary certified entities in the domain. Temporary entities are 
     * entities with a finite <code>lifetime</code> property value set. Certified entities are entities that have PoP 
     * certificates.
     * @function Entities.canRezTmpCertified
     * @returns {boolean} <code>true</code> if the domain server will allow the script to rez (create) new temporary certified 
     *     entities, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canRezTmpCertified();

    /*@jsdoc
     * Checks whether or not the script can make changes to the asset server's assets.
     * @function Entities.canWriteAssets
     * @returns {boolean} <code>true</code> if the domain server will allow the script to make changes to the asset server's 
     *     assets, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canWriteAssets();

    /*@jsdoc
     * Checks whether or not the script can replace the domain's content set.
     * @function Entities.canReplaceContent
     * @returns {boolean} <code>true</code> if the domain server will allow the script to replace the domain's content set, 
     *     otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canReplaceContent();

    /*@jsdoc
     * Checks whether or not the script can get and set the <code>privateUserData</code> property of entities.
     * @function Entities.canGetAndSetPrivateUserData
     * @returns {boolean} <code>true</code> if the domain server will allow the script to get and set the 
     *     <code>privateUserData</code> property of entities, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canGetAndSetPrivateUserData();
    
    /*@jsdoc
     * Checks whether or not the script can rez avatar entities.
     * @function Entities.canRezAvatarEntities
     * @returns {boolean} <code>true</code> if the domain server will allow the script to rez avatar entities,
     *     otherwise <code>false</code>.
     */
    Q_INVOKABLE bool canRezAvatarEntities();

    /*@jsdoc
     * <p>How an entity is hosted and sent to others for display.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>"domain"</code></td><td>Domain entities are stored on the domain, are visible to everyone, and are 
     *       sent to everyone by the entity server.</td></tr>
     *     <tr><td><code>"avatar"</code></td><td>Avatar entities are stored on an Interface client, are visible to everyone, 
     *       and are sent to everyone by the avatar mixer. They follow the client to each domain visited, displaying at the 
     *       same domain coordinates unless parented to the client's avatar.</td></tr>
     *     <tr><td><code>"local"</code></td><td>Local entities are ephemeral &mdash; they aren't stored anywhere &mdash; and 
     *       are visible only to the client. They follow the client to each domain visited, displaying at the same domain 
     *       coordinates unless parented to the client's avatar. Additionally, local entities are always 
     *       collisionless.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {string} Entities.EntityHostType
     */

    /*@jsdoc
     * Adds a new domain, avatar, or local entity.
     * @function Entities.addEntity
     * @param {Entities.EntityProperties} properties - The properties of the entity to create.
     * @param {Entities.EntityHostType} [entityHostType="domain"] - The type of entity to create.
     
     * @returns {Uuid} The ID of the entity if successfully created, otherwise {@link Uuid(0)|Uuid.NULL}.
     * @example <caption>Create a box domain entity in front of your avatar.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * print("Entity created: " + entityID);
     */
    Q_INVOKABLE QUuid addEntity(const EntityItemProperties& properties, const QString& entityHostTypeString) {
        entity::HostType entityHostType;
        if (entityHostTypeString == "local") {
            entityHostType = entity::HostType::LOCAL;
        } else if (entityHostTypeString == "avatar") {
            entityHostType = entity::HostType::AVATAR;
        } else {
            entityHostType = entity::HostType::DOMAIN;
        }
        return addEntityInternal(properties, entityHostType);
    }

    /*@jsdoc
     * Adds a new avatar entity (<code>{@link Entities.EntityProperties|entityHostType}</code> property is 
     * <code>"avatar"</code>) or domain entity (<code>{@link Entities.EntityProperties|entityHostType}</code> property is 
     * <code>"domain"</code>).
     * @function Entities.addEntity
     * @param {Entities.EntityProperties} properties - The properties of the entity to create.
     * @param {boolean} [avatarEntity=false] - <code>true</code> to create an avatar entity, <code>false</code> to create a 
     *     domain entity.
     * @returns {Uuid} The ID of the entity if successfully created, otherwise {@link Uuid(0)|Uuid.NULL}.
     */
    Q_INVOKABLE QUuid addEntity(const EntityItemProperties& properties, bool avatarEntity = false) {
        entity::HostType entityHostType = avatarEntity ? entity::HostType::AVATAR : entity::HostType::DOMAIN;
        return addEntityInternal(properties, entityHostType);
    }

    /// temporary method until addEntity can be used from QJSEngine
    /// Deliberately not adding jsdoc, only used internally.
    // FIXME: Deprecate and remove from the API.
    Q_INVOKABLE QUuid addModelEntity(const QString& name, const QString& modelUrl, const QString& textures, const QString& shapeType, bool dynamic,
                                     bool collisionless, bool grabbable, const glm::vec3& position, const glm::vec3& gravity);

    /*@jsdoc
     * Creates a clone of an entity. The clone has the same properties as the original except that: it has a modified
     * <code>name</code> property, clone-related properties are set per the original entity's clone-related
     * {@link Entities.EntityProperties|properties} (e.g., <code>cloneLifetime</code>), and its clone-related properties are 
     * set to their defaults.
     * <p>Domain entities must have their <code>cloneable</code> property value be <code>true</code> in order to be cloned. A 
     * domain entity can be cloned by a client that doesn't have rez permissions in the domain.</p>
     * <p>Avatar entities must have their <code>cloneable</code> and <code>cloneAvatarEntity</code> property values be 
     * <code>true</code> in order to be cloned.</p>
     * @function Entities.cloneEntity
     * @param {Uuid} entityID - The ID of the entity to clone.
     * @returns {Uuid} The ID of the new entity if successfully cloned, otherwise {@link Uuid(0)|Uuid.NULL}.
     */
    Q_INVOKABLE QUuid cloneEntity(const QUuid& entityID);

    /*@jsdoc
     * Gets an entity's property values.
     * @function Entities.getEntityProperties
     * @param {Uuid} entityID - The ID of the entity to get the properties of.
     * @param {string|string[]} [desiredProperties=[]] - The name or names of the properties to get. For properties that are 
     *     objects (e.g., the <code>"keyLight"</code> property), use the property and subproperty names in dot notation (e.g., 
     *     <code>"keyLight.color"</code>).
     * @returns {Entities.EntityProperties} The specified properties of the entity if the entity can be found, otherwise an 
     *     empty object. If no properties are specified, then all properties are returned.
     * @example <caption>Report the color of a new box entity.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var properties = Entities.getEntityProperties(entityID, ["color"]);
     * print("Entity color: " + JSON.stringify(properties.color));
     */
    Q_INVOKABLE EntityItemProperties getEntityProperties(const QUuid& entityID);
    Q_INVOKABLE EntityItemProperties getEntityProperties(const QUuid& entityID, EntityPropertyFlags desiredProperties);

    /*@jsdoc
     * Edits an entity, changing one or more of its property values.
     * @function Entities.editEntity
     * @param {Uuid} entityID - The ID of the entity to edit.
     * @param {Entities.EntityProperties} properties - The new property values.
     * @returns {Uuid} The ID of the entity if the edit was successful, otherwise <code>null</code> or {@link Uuid|Uuid.NULL}.
     * @example <caption>Change the color of an entity.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var properties = Entities.getEntityProperties(entityID, ["color"]);
     * print("Entity color: " + JSON.stringify(properties.color));
     *
     * Script.setTimeout(function () { // Wait for the entity to be created before editing.
     *     Entities.editEntity(entityID, {
     *         color: { red: 255, green: 0, blue: 0 }
     *     });
     *     properties = Entities.getEntityProperties(entityID, ["color"]);
     *     print("Entity color: " + JSON.stringify(properties.color));
     * }, 50);
     */
    Q_INVOKABLE QUuid editEntity(const QUuid& entityID, const EntityItemProperties& properties);

    /*@jsdoc
     * Deletes an entity.
     * @function Entities.deleteEntity
     * @param {Uuid} entityID - The ID of the entity to delete.
     * @example <caption>Delete an entity a few seconds after creating it.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 }
     * });
     *
     * Script.setTimeout(function () {
     *     Entities.deleteEntity(entityID);
     * }, 3000);
     */
    Q_INVOKABLE void deleteEntity(const QUuid& entityID);

    /*@jsdoc
     * Gets an entity's type.
     * @function Entities.getEntityType
     * @param {Uuid} id - The ID of the entity to get the type of.
     * @returns {Entities.EntityType} The type of the entity.
     */
    Q_INVOKABLE QString getEntityType(const QUuid& entityID);

    /*@jsdoc
     * Gets an entity's script object. In particular, this is useful for accessing a {@link Entities.EntityProperties-Web|Web} 
     * entity's HTML <code>EventBridge</code> script object to exchange messages with the web page script.
     * <p>To send a message from an Interface script to a Web entity over its event bridge:</p>
     * <pre class="prettyprint"><code>var entityObject = Entities.getEntityObject(entityID);
     * entityObject.emitScriptEvent(message);</code></pre>
     * <p>To receive a message from a Web entity over its event bridge in an Interface script:</p>
     * <pre class="prettyprint"><code>var entityObject = Entities.getentityObject(entityID);
     * entityObject.webEventReceived.connect(function(message) {
     *     ...
     * };</code></pre>
     * <p>Alternatively, you can use {@link Entities.emitScriptEvent} and {@link Entities.webEventReceived} to exchange
     * messages with a Web entity over its event bridge.</p>
     * @function Entities.getEntityObject
     * @param {Uuid} id - The ID of the entity to get the script object for.
     * @returns {object} The script object for the entity if found.
     * @example <caption>Exchange messages with a Web entity.</caption>
     * // HTML file, name: "webEntity.html".
     * <!DOCTYPE html>
     * <html>
     * <head>
     *     <title>HELLO</title>
     * </head>
     * <body>
     *     <h1>HELLO</h1>
     *     <script>
     *         function onScriptEventReceived(message) {
     *             // Message received from the script.
     *             console.log("Message received: " + message);
     *         }
     *    
     *         EventBridge.scriptEventReceived.connect(onScriptEventReceived);
     *    
     *         setTimeout(function () {
     *             // Send a message to the script.
     *             EventBridge.emitWebEvent("hello");
     *         }, 5000);
     *     </script>
     * </body>
     * </html>
     * 
     * // Interface script file.
     * var webEntity = Entities.addEntity({
     *     type: "Web",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     sourceUrl: Script.resolvePath("webEntity.html"),
     *     alpha: 1.0,
     *     lifetime: 300  // 5 min
     * });
     *
     * var webEntityObject;
     * 
     * function onWebEventReceived(message) {
     *     // Message received.
     *     print("Message received: " + message);
     *
     *     // Send a message back.
     *     webEntityObject.emitScriptEvent(message + " back");
     * }
     * 
     * Script.setTimeout(function () {
     *     webEntityObject = Entities.getEntityObject(webEntity);
     *     webEntityObject.webEventReceived.connect(onWebEventReceived);
     * }, 500);
     * 
     * Script.scriptEnding.connect(function () {
     *     Entities.deleteEntity(webEntity);
     * });
     */
    Q_INVOKABLE QObject* getEntityObject(const QUuid& id);

    /*@jsdoc
     * Checks whether an entity's assets have been loaded. For example, for an <code>Model</code> entity the result indicates
     * whether its textures have been loaded.
     * @function Entities.isLoaded
     * @param {Uuid} id - The ID of the entity to check.
     * @returns {boolean} <code>true</code> if the entity's assets have been loaded, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool isLoaded(const QUuid& id);

    /*@jsdoc
     * Checks if there is an entity with a specified ID.
     * @function Entities.isAddedEntity
     * @param {Uuid} id - The ID to check.
     * @returns {boolean} <code>true</code> if an entity with the specified ID exists, <code>false</code> if it doesn't.
     */
    Q_INVOKABLE bool isAddedEntity(const QUuid& id);

    /*@jsdoc
     * Calculates the size of some text in a {@link Entities.EntityProperties-Text|Text} entity. The entity need not be set 
     * visible.
     * <p><strong>Note:</strong> The size of text in a Text entity cannot be calculated immediately after the
     * entity is created; a short delay is required while the entity finishes being created.</p>
     * @function Entities.textSize
     * @param {Uuid} id - The ID of the Text entity to use for calculation.
     * @param {string} text - The string to calculate the size of.
     * @returns {Size} The size of the <code>text</code> in meters if the object is a text entity, otherwise
     *     <code>{ height: 0, width : 0 }</code>.
     */
    Q_INVOKABLE QSizeF textSize(const QUuid& id, const QString& text);

    /*@jsdoc
     * Calls a method in a client entity script from an Interface, avatar, or client entity script, or calls a method in a 
     * server entity script from a server entity script. The entity script method must be exposed as a property in the target 
     * entity script. Additionally, if calling a server entity script, the server entity script must include the method's name 
     * in an exposed property called <code>remotelyCallable</code> that is an array of method names that can be called.
     * @function Entities.callEntityMethod
     * @param {Uuid} entityID - The ID of the entity to call the method in.
     * @param {string} method - The name of the method to call. The method is called with the entity ID as the first parameter 
     *     and the <code>parameters</code> value as the second parameter.
     * @param {string[]} [parameters=[]] - The additional parameters to call the specified method with.
     * @example <caption>Call a method in a client entity script from an Interface script.</caption>
     * // Client entity script.
     * var entityScript = (function () {
     *     this.entityMethod = function (id, params) {
     *         print("Method at entity : " + id + " ; " + params[0] + ", " + params[1]);
     *     };
     * });
     * 
     * // Entity that hosts the client entity script.
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     script: "(" + entityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * // Interface script call to the client entity script.
     * Script.setTimeout(function () {
     *     Entities.callEntityMethod(entityID, "entityMethod", ["hello", 12]);
     * }, 1000); // Wait for the entity to be created.
     */
    Q_INVOKABLE void callEntityMethod(const QUuid& entityID, const QString& method, const QStringList& params = QStringList());

    /*@jsdoc
     * Calls a method in a server entity script from an Interface, avatar, or client entity script. The server entity script 
     * method must be exposed as a property in the target server entity script. Additionally, the server entity script must 
     * include the method's name in an exposed property called <code>remotelyCallable</code> that is an array of method names 
     * that can be called.
     * @function Entities.callEntityServerMethod
     * @param {Uuid} entityID - The ID of the entity to call the method in.
     * @param {string} method - The name of the method to call. The method is called with the entity ID as the first parameter 
     *     and the <code>parameters</code> value as the second parameter.
     * @param {string[]} [parameters=[]] - The additional parameters to call the specified method with.
     * @example <caption>Call a method in a server entity script from an Interface script.</caption>
     * // Server entity script.
     * var entityScript = (function () {
     *     this.entityMethod = function (id, params) {
     *         print("Method at entity : " + id + " ; " + params[0] + ", " + params[1]); // In server log.
     *     };
     *     this.remotelyCallable = [
     *         "entityMethod"
     *     ];
     * });
     * 
     * // Entity that hosts the server entity script.
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     serverScripts: "(" + entityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * // Interface script call to the server entity script.
     * Script.setTimeout(function () {
     *     Entities.callEntityServerMethod(entityID, "entityMethod", ["hello", 12]);
     * }, 1000); // Wait for the entity to be created.
     */
    Q_INVOKABLE void callEntityServerMethod(const QUuid& entityID, const QString& method, const QStringList& params = QStringList());

    /*@jsdoc
     * Calls a method in a specific user's client entity script from a server entity script. The entity script method must be
     * exposed as a property in the target client entity script. Additionally, the client entity script must 
     * include the method's name in an exposed property called <code>remotelyCallable</code> that is an array of method names 
     * that can be called.
     * @function Entities.callEntityClientMethod
     * @param {Uuid} clientSessionID - The session ID of the user to call the method in.
     * @param {Uuid} entityID - The ID of the entity to call the method in.
     * @param {string} method - The name of the method to call. The method is called with the entity ID as the first parameter 
     *     and the <code>parameters</code> value as the second parameter.
     * @param {string[]} [parameters=[]] - The additional parameters to call the specified method with.
     * @example <caption>Call a method in a client entity script from a server entity script.</caption>
     * // Client entity script.
     * var clientEntityScript = (function () {
     *     this.entityMethod = function (id, params) {
     *         print("Method at client entity : " + id + " ; " + params[0] + ", " + params[1]);
     *     };
     *     this.remotelyCallable = [
     *         "entityMethod"
     *     ];
     * });
     * 
     * // Server entity script.
     * var serverEntityScript = (function () {
     *     var clientSessionID,
     *         clientEntityID;
     * 
     *     function callClientMethod() {
     *         // Server entity script call to client entity script.
     *         Entities.callEntityClientMethod(clientSessionID, clientEntityID, "entityMethod", ["hello", 12]);
     *     }
     * 
     *     // Obtain client entity details then call client entity method.
     *     this.entityMethod = function (id, params) {
     *         clientSessionID = params[0];
     *         clientEntityID = params[1];
     *         callClientMethod();
     *     };
     *     this.remotelyCallable = [
     *         "entityMethod"
     *     ];
     * });
     * 
     * // Entity that hosts the client entity script.
     * var clientEntityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: -1, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     script: "(" + clientEntityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * // Entity that hosts the server entity script.
     * var serverEntityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 1, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     serverScripts: "(" + serverEntityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * // Interface script call to the server entity script.
     * Script.setTimeout(function () {
     *     Entities.callEntityServerMethod(serverEntityID, "entityMethod", [MyAvatar.sessionUUID, clientEntityID]);
     * }, 1000); // Wait for the entities to be created.
     */
    Q_INVOKABLE void callEntityClientMethod(const QUuid& clientSessionID, const QUuid& entityID, const QString& method,
        const QStringList& params = QStringList());

    /*@jsdoc
     * Finds the domain or avatar entity with a position closest to a specified point and within a specified radius.
     * @function Entities.findClosestEntity
     * @param {Vec3} center - The point about which to search.
     * @param {number} radius - The radius within which to search.
     * @returns {Uuid} The ID of the entity that is closest to the <code>center</code> and within the <code>radius</code>, if
     *     there is one, otherwise <code>null</code>.
     * @example <caption>Find the closest entity within 10m of your avatar.</caption>
     * var entityID = Entities.findClosestEntity(MyAvatar.position, 10);
     * print("Closest entity: " + entityID);
     */
    /// this function will not find any models in script engine contexts which don't have access to models
    Q_INVOKABLE QUuid findClosestEntity(const glm::vec3& center, float radius) const;

    /*@jsdoc
     * Finds all domain and avatar entities that intersect a sphere.
     * <p><strong>Note:</strong> Server entity scripts only find entities that have a server entity script
     * running in them or a parent entity. You can apply a dummy script to entities that you want found in a search.</p>
     * @function Entities.findEntities
     * @param {Vec3} center - The point about which to search.
     * @param {number} radius - The radius within which to search.
     * @returns {Uuid[]} An array of entity IDs that intersect the search sphere. The array is empty if no entities could be 
     *     found.
     * @example <caption>Report how many entities are within 10m of your avatar.</caption>
     * var entityIDs = Entities.findEntities(MyAvatar.position, 10);
     * print("Number of entities within 10m: " + entityIDs.length);
     */
    /// this function will not find any models in script engine contexts which don't have access to models
    Q_INVOKABLE QVector<QUuid> findEntities(const glm::vec3& center, float radius) const;

    /*@jsdoc
     * Finds all domain and avatar entities whose axis-aligned boxes intersect a search axis-aligned box.
     * <p><strong>Note:</strong> Server entity scripts only find entities that have a server entity script
     * running in them or a parent entity. You can apply a dummy script to entities that you want found in a search.</p>
     * @function Entities.findEntitiesInBox
     * @param {Vec3} corner - The corner of the search AA box with minimum co-ordinate values.
     * @param {Vec3} dimensions - The dimensions of the search AA box.
     * @returns {Uuid[]} An array of entity IDs whose AA boxes intersect the search AA box. The array is empty if no entities
     *     could be found.
     */
    /// this function will not find any models in script engine contexts which don't have access to models
    Q_INVOKABLE QVector<QUuid> findEntitiesInBox(const glm::vec3& corner, const glm::vec3& dimensions) const;

    /*@jsdoc
     * Finds all domain and avatar entities whose axis-aligned boxes intersect a search frustum.
     * <p><strong>Note:</strong> Server entity scripts only find entities that have a server entity script
     * running in them or a parent entity. You can apply a dummy script to entities that you want found in a search.</p>
     * @function Entities.findEntitiesInFrustum
     * @param {ViewFrustum} frustum - The frustum to search in. The <code>position</code>, <code>orientation</code>, 
     *     <code>projection</code>, and <code>centerRadius</code> properties must be specified. The <code>fieldOfView</code> 
     *     and <code>aspectRatio</code> properties are not used; these values are specified by the <code>projection</code>.
     * @returns {Uuid[]} An array of entity IDs whose axis-aligned boxes intersect the search frustum. The array is empty if no 
     *     entities could be found.
     * @example <caption>Report the number of entities in view.</caption>
     * var entityIDs = Entities.findEntitiesInFrustum(Camera.frustum);
     * print("Number of entities in view: " + entityIDs.length);
     */
    /// this function will not find any models in script engine contexts which don't have access to entities
    Q_INVOKABLE QVector<QUuid> findEntitiesInFrustum(QVariantMap frustum) const;

    /*@jsdoc
     * Finds all domain and avatar entities of a particular type that intersect a sphere.
     * <p><strong>Note:</strong> Server entity scripts only find entities that have a server entity script
     * running in them or a parent entity. You can apply a dummy script to entities that you want found in a search.</p>
     * @function Entities.findEntitiesByType
     * @param {Entities.EntityType} entityType - The type of entity to search for.
     * @param {Vec3} center - The point about which to search.
     * @param {number} radius - The radius within which to search.
     * @returns {Uuid[]} An array of entity IDs of the specified type that intersect the search sphere. The array is empty if
     *     no entities could be found.
     * @example <caption>Report the number of Model entities within 10m of your avatar.</caption>
     * var entityIDs = Entities.findEntitiesByType("Model", MyAvatar.position, 10);
     * print("Number of Model entities within 10m: " + entityIDs.length);
     */
    /// this function will not find any entities in script engine contexts which don't have access to entities
    Q_INVOKABLE QVector<QUuid> findEntitiesByType(const QString entityType, const glm::vec3& center, float radius) const;

    /*@jsdoc
     * Finds all domain and avatar entities with a particular name that intersect a sphere.
     * <p><strong>Note:</strong> Server entity scripts only find entities that have a server entity script
     * running in them or a parent entity. You can apply a dummy script to entities that you want found in a search.</p>
     * @function Entities.findEntitiesByName
     * @param {string} entityName - The name of the entity to search for.
     * @param {Vec3} center - The point about which to search.
     * @param {number} radius - The radius within which to search.
     * @param {boolean} [caseSensitive=false] - <code>true</code> if the search is case-sensitive, <code>false</code> if it is 
     *     case-insensitive.
     * @returns {Uuid[]} An array of entity IDs that have the specified name and intersect the search sphere. The array is 
     *     empty if no entities could be found.
     * @example <caption>Report the number of entities with the name, "Light-Target".</caption>
     * var entityIDs = Entities.findEntitiesByName("Light-Target", MyAvatar.position, 10, false);
     * print("Number of entities with the name Light-Target: " + entityIDs.length);
     */
    Q_INVOKABLE QVector<QUuid> findEntitiesByName(const QString entityName, const glm::vec3& center, float radius,
        bool caseSensitiveSearch = false) const;

    /*@jsdoc
     * Finds the first avatar or domain entity intersected by a {@link PickRay}. <code>Light</code> and <code>Zone</code> 
     * entities are not intersected unless they've been configured as pickable using 
     * {@link Entities.setLightsArePickable|setLightsArePickable} and {@link Entities.setZonesArePickable|setZonesArePickable}, 
     * respectively.
     * @function Entities.findRayIntersection
     * @param {PickRay} pickRay - The pick ray to use for finding entities.
     * @param {boolean} [precisionPicking=false] - <code>true</code> to pick against precise meshes, <code>false</code> to pick 
     *     against coarse meshes. If <code>true</code> and the intersected entity is a <code>Model</code> entity, the result's 
     *     <code>extraInfo</code> property includes more information than it otherwise would.
     * @param {Uuid[]} [entitiesToInclude=[]] - If not empty, then the search is restricted to these entities.
     * @param {Uuid[]} [entitiesToDiscard=[]] - Entities to ignore during the search.
     * @param {boolean} [visibleOnly=false] - <code>true</code> if only entities that are 
     *     <code>{@link Entities.EntityProperties|visible}</code> are searched for, <code>false</code> if their visibility 
     *     doesn't matter.
     * @param {boolean} [collideableOnly=false] - <code>true</code> if only entities that are not 
     *     <code>{@link Entities.EntityProperties|collisionless}</code> are searched, <code>false</code> if their 
     *     collideability doesn't matter.
     * @returns {Entities.RayToEntityIntersectionResult} The result of the search for the first intersected entity.
     * @example <caption>Find the entity directly in front of your avatar.</caption>
     * var pickRay = {
     *     origin: MyAvatar.position,
     *     direction: Quat.getFront(MyAvatar.orientation)
     * };
     *
     * var intersection = Entities.findRayIntersection(pickRay, true);
     * if (intersection.intersects) {
     *     print("Entity in front of avatar: " + intersection.entityID);
     * } else {
     *     print("No entity in front of avatar.");
     * }
     */
    /// If the scripting context has visible entities, this will determine a ray intersection, the results
    /// may be inaccurate if the engine is unable to access the visible entities, in which case result.accurate
    /// will be false.
    Q_INVOKABLE RayToEntityIntersectionResult findRayIntersection(const PickRay& ray, bool precisionPicking = false,
            const ScriptValuePointer& entityIdsToInclude = ScriptValuePointer(), const ScriptValuePointer& entityIdsToDiscard = ScriptValuePointer(),
            bool visibleOnly = false, bool collidableOnly = false) const;

    /*@jsdoc
     * Reloads an entity's server entity script such that the latest version re-downloaded.
     * @function Entities.reloadServerScripts
     * @param {Uuid} entityID - The ID of the entity to reload the server entity script of.
     * @returns {boolean} <code>true</code> if the reload request was successfully sent to the server, otherwise 
     *     <code>false</code>.
     */
    Q_INVOKABLE bool reloadServerScripts(const QUuid& entityID);

    /*@jsdoc
     * Gets the status of a server entity script attached to an entity.
     * @function Entities.getServerScriptStatus
     * @param {Uuid} entityID - The ID of the entity to get the server entity script status of.
     * @param {Entities~getServerScriptStatusCallback} callback - The function to call upon completion.
     * @returns {boolean} <code>true</code> always.
     */
    /*@jsdoc
     * Called when a {@link Entities.getServerScriptStatus} call is complete.
     * @callback Entities~getServerScriptStatusCallback
     * @param {boolean} success - <code>true</code> if the server entity script status could be obtained, otherwise 
     *     <code>false</code>.
     * @param {boolean} isRunning - <code>true</code> if there is a server entity script running, otherwise <code>false</code>.
     * @param {string} status - <code>"running"</code> if there is a server entity script running, otherwise an error string.
     * @param {string} errorInfo - <code>""</code> if there is a server entity script running, otherwise it may contain extra 
     *     information on the error.
     */
    Q_INVOKABLE bool getServerScriptStatus(const QUuid& entityID, ScriptValuePointer callback);

    /*@jsdoc
     * Gets metadata for certain entity properties such as <code>script</code> and <code>serverScripts</code>.
     * @function Entities.queryPropertyMetadata
     * @param {Uuid} entityID - The ID of the entity to get the metadata for.
     * @param {string} property - The property name to get the metadata for.
     * @param {Entities~queryPropertyMetadataCallback} callback - The function to call upon completion.
     * @returns {boolean} <code>true</code> if the request for metadata was successfully sent to the server, otherwise 
     *     <code>false</code>.
     * @throws Throws an error if <code>property</code> is not handled yet or <code>callback</code> is not a function.
     */
    /*@jsdoc
     * Gets metadata for certain entity properties such as <code>script</code> and <code>serverScripts</code>.
     * @function Entities.queryPropertyMetadata
     * @param {Uuid} entityID - The ID of the entity to get the metadata for.
     * @param {string} property - The property name to get the metadata for.
     * @param {object} scope - The "<code>this</code>" context that the callback will be executed within.
     * @param {Entities~queryPropertyMetadataCallback} callback - The function to call upon completion.
     * @returns {boolean} <code>true</code> if the request for metadata was successfully sent to the server, otherwise 
     *     <code>false</code>.
     * @throws Throws an error if <code>property</code> is not handled yet or <code>callback</code> is not a function.
     */
    /*@jsdoc
     * Called when a {@link Entities.queryPropertyMetadata} call is complete.
     * @callback Entities~queryPropertyMetadataCallback
     * @param {string} error - <code>undefined</code> if there was no error, otherwise an error message.
     * @param {object} result - The metadata for the requested entity property if there was no error, otherwise
     *     <code>undefined</code>.
     */
    Q_INVOKABLE bool queryPropertyMetadata(const QUuid& entityID, ScriptValuePointer property, ScriptValuePointer scopeOrCallback,
        ScriptValuePointer methodOrName = ScriptValuePointer());


    /*@jsdoc
     * Sets whether or not ray picks intersect the bounding box of {@link Entities.EntityProperties-Light|Light} entities. By 
     * default, Light entities are not intersected. The setting lasts for the Interface session. Ray picks are performed using 
     * {@link Entities.findRayIntersection|findRayIntersection}, or the {@link Picks} API.
     * @function Entities.setLightsArePickable
     * @param {boolean} value - <code>true</code> to make ray picks intersect the bounding box of 
     *     {@link Entities.EntityProperties-Light|Light} entities, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE void setLightsArePickable(bool value);

    /*@jsdoc
     * Gets whether or not ray picks intersect the bounding box of {@link Entities.EntityProperties-Light|Light} entities. Ray 
     * picks are performed using {@link Entities.findRayIntersection|findRayIntersection}, or the {@link Picks} API.
     * @function Entities.getLightsArePickable
     * @returns {boolean} <code>true</code> if ray picks intersect the bounding box of 
     *     {@link Entities.EntityProperties-Light|Light} entities, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool getLightsArePickable() const;

    /*@jsdoc
     * Sets whether or not ray picks intersect the bounding box of {@link Entities.EntityProperties-Zone|Zone} entities. By 
     * default, Zone entities are not intersected. The setting lasts for the Interface session. Ray picks are performed using 
     * {@link Entities.findRayIntersection|findRayIntersection}, or the {@link Picks} API.
     * @function Entities.setZonesArePickable
     * @param {boolean} value - <code>true</code> to make ray picks intersect the bounding box of 
     *     {@link Entities.EntityProperties-Zone|Zone} entities, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE void setZonesArePickable(bool value);

    /*@jsdoc
     * Gets whether or not ray picks intersect the bounding box of {@link Entities.EntityProperties-Zone|Zone} entities. Ray 
     * picks are performed using {@link Entities.findRayIntersection|findRayIntersection}, or the {@link Picks} API.
     * @function Entities.getZonesArePickable
     * @returns {boolean} <code>true</code> if ray picks intersect the bounding box of 
     *      {@link Entities.EntityProperties-Zone|Zone} entities, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool getZonesArePickable() const;

    /*@jsdoc
     * Sets whether or not {@link Entities.EntityProperties-Zone|Zone} entities' boundaries should be drawn. <em>Currently not 
     * used.</em>
     * @function Entities.setDrawZoneBoundaries
     * @param {boolean} value - <code>true</code> if {@link Entities.EntityProperties-Zone|Zone} entities' boundaries should be 
     *     drawn, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE void setDrawZoneBoundaries(bool value);

    /*@jsdoc
     * Gets whether or not {@link Entities.EntityProperties-Zone|Zone} entities' boundaries should be drawn. <em>Currently 
     * not used.</em>
     * @function Entities.getDrawZoneBoundaries
     * @returns {boolean} <code>true</code> if {@link Entities.EntityProperties-Zone|Zone} entities' boundaries should be 
     *    drawn, otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool getDrawZoneBoundaries() const;


    /*@jsdoc
     * Sets the values of all voxels in a spherical portion of a {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @function Entities.setVoxelSphere
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} center - The center of the sphere of voxels to set, in world coordinates.
     * @param {number} radius - The radius of the sphere of voxels to set, in world coordinates.
     * @param {number} value - If <code>value % 256 == 0</code> then each voxel is cleared, otherwise each voxel is set.
     * @example <caption>Create a PolyVox sphere.</caption>
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 }));
     * var polyVox = Entities.addEntity({
     *     type: "PolyVox",
     *     position: position,
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 32, y: 32, z: 32 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * Entities.setVoxelSphere(polyVox, position, 0.9, 255);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setVoxelSphere(const QUuid& entityID, const glm::vec3& center, float radius, int value);
    
    /*@jsdoc
     * Sets the values of all voxels in a capsule-shaped portion of a {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @function Entities.setVoxelCapsule
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} start - The center of the sphere of voxels to set, in world coordinates.
     * @param {Vec3} end - The center of the sphere of voxels to set, in world coordinates.
     * @param {number} radius - The radius of the capsule cylinder and spherical ends, in world coordinates.
     * @param {number} value - If <code>value % 256 == 0</code> then each voxel is cleared, otherwise each voxel is set.
     * @example <caption>Create a PolyVox capsule shape.</caption>
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 }));
     * var polyVox = Entities.addEntity({
     *     type: "PolyVox",
     *     position: position,
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 32, y: 32, z: 32 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var startPosition = Vec3.sum({ x: -0.5, y: 0, z: 0 }, position);
     * var endPosition = Vec3.sum({ x: 0.5, y: 0, z: 0 }, position);
     * Entities.setVoxelCapsule(polyVox, startPosition, endPosition, 0.5, 255);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setVoxelCapsule(const QUuid& entityID, const glm::vec3& start, const glm::vec3& end, float radius, int value);

    /*@jsdoc
     * Sets the value of a particular voxel in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @function Entities.setVoxel
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} position - The position relative to the minimum axes values corner of the entity. The 
     *     <code>position</code> coordinates are rounded to the nearest integer to get the voxel coordinate. The minimum axes 
     *     corner voxel is <code>{ x: 0, y: 0, z: 0 }</code>.
     * @param {number} value - If <code>value % 256 == 0</code> then voxel is cleared, otherwise the voxel is set.
     * @example <caption>Create a cube PolyVox entity and clear the minimum axes' corner voxel.</caption>
     * var entity = Entities.addEntity({
     *     type: "PolyVox",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 })),
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 16, y: 16, z: 16 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * Entities.setAllVoxels(entity, 1);
     * Entities.setVoxel(entity, { x: 0, y: 0, z: 0 }, 0);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setVoxel(const QUuid& entityID, const glm::vec3& position, int value);

    /*@jsdoc
     * Sets the values of all voxels in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @function Entities.setAllVoxels
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {number} value - If <code>value % 256 == 0</code> then each voxel is cleared, otherwise each voxel is set.
     * @example <caption>Create a PolyVox cube.</caption>
     * var entity = Entities.addEntity({
     *     type: "PolyVox",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 })),
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 16, y: 16, z: 16 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * Entities.setAllVoxels(entity, 1);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setAllVoxels(const QUuid& entityID, int value);

    /*@jsdoc
     * Sets the values of all voxels in a cubic portion of a {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @function Entities.setVoxelsInCuboid
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} lowPosition - The position of the minimum axes value corner of the cube of voxels to set, in voxel 
     *     coordinates.
     * @param {Vec3} cuboidSize - The size of the cube of voxels to set, in voxel coordinates.
     * @param {number} value - If <code>value % 256 == 0</code> then each voxel is cleared, otherwise each voxel is set.
     * @example <caption>Create a PolyVox cube and clear the voxels in one corner.</caption>
     * var polyVox = Entities.addEntity({
     *     type: "PolyVox",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 16, y: 16, z: 16 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * Entities.setAllVoxels(polyVox, 1);
     * var cuboidPosition = { x: 12, y: 12, z: 12 };
     * var cuboidSize = { x: 4, y: 4, z: 4 };
     * Entities.setVoxelsInCuboid(polyVox, cuboidPosition, cuboidSize, 0);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setVoxelsInCuboid(const QUuid& entityID, const glm::vec3& lowPosition, const glm::vec3& cuboidSize, int value);

    /*@jsdoc
     * Converts voxel coordinates in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity to world coordinates. Voxel 
     * coordinates are relative to the minimum axes values corner of the entity with a scale of <code>Vec3.ONE</code> being the 
     * dimensions of each voxel.
     * @function Entities.voxelCoordsToWorldCoords
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} voxelCoords - The voxel coordinates. May be fractional and outside the entity's bounding box.
     * @returns {Vec3} The world coordinates of the <code>voxelCoords</code> if the <code>entityID</code> is a 
     *     {@link Entities.EntityProperties-PolyVox|PolyVox} entity, otherwise {@link Vec3(0)|Vec3.ZERO}.
     * @example <caption>Create a PolyVox cube with the 0,0,0 voxel replaced by a sphere.</caption>
     * // Cube PolyVox with 0,0,0 voxel missing.
     * var polyVox = Entities.addEntity({
     *     type: "PolyVox",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 })),
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 16, y: 16, z: 16 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * Entities.setAllVoxels(polyVox, 1);
     * Entities.setVoxel(polyVox, { x: 0, y: 0, z: 0 }, 0);
     *
     * // Red sphere in 0,0,0 corner position.
     * var cornerPosition = Entities.voxelCoordsToWorldCoords(polyVox, { x: 0, y: 0, z: 0 });
     * var voxelDimensions = Vec3.multiply(2 / 16, Vec3.ONE);
     * var sphere = Entities.addEntity({
     *     type: "Sphere",
     *     position: Vec3.sum(cornerPosition, Vec3.multiply(0.5, voxelDimensions)),
     *     dimensions: voxelDimensions,
     *     color: { red: 255, green: 0, blue: 0 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 voxelCoordsToWorldCoords(const QUuid& entityID, glm::vec3 voxelCoords);

    /*@jsdoc
     * Converts world coordinates to voxel coordinates in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity. Voxel 
     * coordinates are relative to the minimum axes values corner of the entity, with a scale of <code>Vec3.ONE</code> being 
     * the dimensions of each voxel.
     * @function Entities.worldCoordsToVoxelCoords
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} worldCoords - The world coordinates. The value may be outside the entity's bounding box.
     * @returns {Vec3} The voxel coordinates of the <code>worldCoords</code> if the <code>entityID</code> is a 
     *     {@link Entities.EntityProperties-PolyVox|PolyVox} entity, otherwise {@link Vec3(0)|Vec3.ZERO}. The value may be 
     *     fractional and outside the entity's bounding box.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 worldCoordsToVoxelCoords(const QUuid& entityID, glm::vec3 worldCoords);

    /*@jsdoc
     * Converts voxel coordinates in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity to local coordinates. Local 
     * coordinates are relative to the minimum axes value corner of the entity, with the scale being the same as world 
     * coordinates. Voxel coordinates are relative to the minimum axes values corner of the entity, with a scale of 
     * <code>Vec3.ONE</code> being the dimensions of each voxel.
     * @function Entities.voxelCoordsToLocalCoords
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} voxelCoords - The voxel coordinates. The value may be fractional and outside the entity's bounding box.
     * @returns {Vec3} The local coordinates of the <code>voxelCoords</code> if the <code>entityID</code> is a 
     *     {@link Entities.EntityProperties-PolyVox|PolyVox} entity, otherwise {@link Vec3(0)|Vec3.ZERO}.
     * @example <caption>Get the world dimensions of a voxel in a PolyVox entity.</caption>
     * var polyVox = Entities.addEntity({
     *     type: "PolyVox",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -8 })),
     *     dimensions: { x: 2, y: 2, z: 2 },
     *     voxelVolumeSize: { x: 16, y: 16, z: 16 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var voxelDimensions = Entities.voxelCoordsToLocalCoords(polyVox, Vec3.ONE);
     * print("Voxel dimensions: " + JSON.stringify(voxelDimensions));
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 voxelCoordsToLocalCoords(const QUuid& entityID, glm::vec3 voxelCoords);

    /*@jsdoc
     * Converts local coordinates to voxel coordinates in a {@link Entities.EntityProperties-PolyVox|PolyVox} entity. Local 
     * coordinates are relative to the minimum axes value corner of the entity, with the scale being the same as world 
     * coordinates. Voxel coordinates are relative to the minimum axes values corner of the entity, with a scale of 
     * <code>Vec3.ONE</code> being the dimensions of each voxel.
     * @function Entities.localCoordsToVoxelCoords
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-PolyVox|PolyVox} entity.
     * @param {Vec3} localCoords - The local coordinates. The value may be outside the entity's bounding box.
     * @returns {Vec3} The voxel coordinates of the <code>worldCoords</code> if the <code>entityID</code> is a 
     *     {@link Entities.EntityProperties-PolyVox|PolyVox} entity, otherwise {@link Vec3(0)|Vec3.ZERO}. The value may be 
     *     fractional and outside the entity's bounding box.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 localCoordsToVoxelCoords(const QUuid& entityID, glm::vec3 localCoords);

    /*@jsdoc
     * Sets all the points in a {@link Entities.EntityProperties-Line|Line} entity.
     * @function Entities.setAllPoints
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-Line|Line} entity.
     * @param {Vec3[]} points - The points that the entity should draw lines between.
     * @returns {boolean} <code>true</code> if the entity was updated, otherwise <code>false</code>. The property may fail to 
     *     be updated if the entity does not exist, the entity is not a {@link Entities.EntityProperties-Line|Line} entity, 
     *     one of the points is outside the entity's dimensions, or the number of points is greater than the maximum allowed.
     * @deprecated This function is deprecated and will be removed. Use {@link Entities.EntityProperties-PolyLine|PolyLine}
     *     entities instead.
     * @example <caption>Change the shape of a Line entity.</caption>
     * // Draw a horizontal line between two points.
     * var entity = Entities.addEntity({
     *     type: "Line",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 2, y: 2, z: 1 },
     *     linePoints: [
     *         { x: -1, y: 0, z: 0 },
     *         { x:1, y: -0, z: 0 }
     *     ],
     *     color: { red: 255, green: 0, blue: 0 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * // Change the line to be a "V".
     * Script.setTimeout(function () {
     *     Entities.setAllPoints(entity, [
     *         { x: -1, y: 1, z: 0 },
     *         { x: 0, y: -1, z: 0 },
     *         { x: 1, y: 1, z: 0 },
     *     ]);
     * }, 2000);
     */
    Q_INVOKABLE bool setAllPoints(const QUuid& entityID, const QVector<glm::vec3>& points);
    
    /*@jsdoc
     * Appends a point to a {@link Entities.EntityProperties-Line|Line} entity.
     * @function Entities.appendPoint
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-Line|Line} entity.
     * @param {Vec3} point - The point to add to the line. The coordinates are relative to the entity's position.
     * @returns {boolean} <code>true</code> if the point was added to the line, otherwise <code>false</code>. The point may 
     *     fail to be added if the entity does not exist, the entity is not a {@link Entities.EntityProperties-Line|Line}
     *     entity, the point is outside the entity's dimensions, or the maximum number of points has been reached.
     * @deprecated This function is deprecated and will be removed. Use {@link Entities.EntityProperties-PolyLine|PolyLine} 
     *     entities instead.
     * @example <caption>Append a point to a Line entity.</caption>
     * // Draw a line between two points.
     * var entity = Entities.addEntity({
     *     type: "Line",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 2, y: 2, z: 1 },
     *     linePoints: [
     *         { x: -1, y: 1, z: 0 },
     *         { x: 0, y: -1, z: 0 }
     *     ],
     *     color: { red: 255, green: 0, blue: 0 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * // Add a third point to create a "V".
     * Script.setTimeout(function () {
     *     Entities.appendPoint(entity, { x: 1, y: 1, z: 0 });
     * }, 50); // Wait for the entity to be created.
     */
    Q_INVOKABLE bool appendPoint(const QUuid& entityID, const glm::vec3& point);

    /*@jsdoc
     * Dumps debug information about all entities in Interface's local in-memory tree of entities it knows about to the program 
     * log.
     * @function Entities.dumpTree
     */
    Q_INVOKABLE void dumpTree() const;


    /*@jsdoc
     * Adds an action to an entity. An action is registered with the physics engine and is applied every physics simulation
     * step. Any entity may have more than one action associated with it, but only as many as will fit in an entity's 
     * <code>{@link Entities.EntityProperties|actionData}</code> property.
     * @function Entities.addAction
     * @param {Entities.ActionType} actionType - The type of action.
     * @param {Uuid} entityID - The ID of the entity to add the action to.
     * @param {Entities.ActionArguments} arguments - Configures the action.
     * @returns {Uuid} The ID of the action if successfully added, otherwise <code>null</code>.
     * @example <caption>Constrain a cube to move along a vertical line.</caption>
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.75, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     dynamic: true,
     *     collisionless: false,
     *     userData: "{ \"grabbableKey\": { \"grabbable\": true, \"kinematic\": false } }",
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * var actionID = Entities.addAction("slider", entityID, {
     *     axis: { x: 0, y: 1, z: 0 },
     *     linearLow: 0,
     *     linearHigh: 0.6
     * });
     */
    Q_INVOKABLE QUuid addAction(const QString& actionTypeString, const QUuid& entityID, const QVariantMap& arguments);

    /*@jsdoc
     * Updates an entity action.
     * @function Entities.updateAction
     * @param {Uuid} entityID - The ID of the entity with the action to update.
     * @param {Uuid} actionID - The ID of the action to update.
     * @param {Entities.ActionArguments} arguments - The arguments to update.
     * @returns {boolean} <code>true</code> if the update was successful, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool updateAction(const QUuid& entityID, const QUuid& actionID, const QVariantMap& arguments);

    /*@jsdoc
     * Deletes an action from an entity.
     * @function Entities.deleteAction
     * @param {Uuid} entityID - The ID of entity to delete the action from.
     * @param {Uuid} actionID - The ID of the action to delete.
     * @returns {boolean} <code>true</code> if the delete was successful, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool deleteAction(const QUuid& entityID, const QUuid& actionID);

    /*@jsdoc
     * Gets the IDs of the actions that are associated with an entity.
     * @function Entities.getActionIDs
     * @param {Uuid} entityID - The entity to get the action IDs for.
     * @returns {Uuid[]} The action IDs if any are found, otherwise an empty array.
     */
    Q_INVOKABLE QVector<QUuid> getActionIDs(const QUuid& entityID);

    /*@jsdoc
     * Gets the arguments of an action.
     * @function Entities.getActionArguments
     * @param {Uuid} entityID - The ID of the entity with the action.
     * @param {Uuid} actionID - The ID of the action to get the arguments of.
     * @returns {Entities.ActionArguments} The arguments of the action if found, otherwise an empty object.
     */
    Q_INVOKABLE QVariantMap getActionArguments(const QUuid& entityID, const QUuid& actionID);


    /*@jsdoc
     * Gets the translation of a joint in a {@link Entities.EntityProperties-Model|Model} entity relative to the entity's 
     * position and orientation.
     * @function Entities.getAbsoluteJointTranslationInObjectFrame
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @returns {Vec3} The translation of the joint relative to the entity's position and orientation if the entity is a
     *     {@link Entities.EntityProperties-Model|Model} entity, the entity is loaded, and the joint index is valid; otherwise 
     *     <code>{@link Vec3(0)|Vec3.ZERO}</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 getAbsoluteJointTranslationInObjectFrame(const QUuid& entityID, int jointIndex);
    
    /*@jsdoc
     * Gets the index of the parent joint of a joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.getJointParent
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} index - The integer index of the joint.
     * @returns {number} The index of the parent joint if found, otherwise <code>-1</code>.
     */
    Q_INVOKABLE int getJointParent(const QUuid& entityID, int index);
    
    /*@jsdoc
     * Gets the rotation of a joint in a {@link Entities.EntityProperties-Model|Model} entity relative to the entity's 
     * position and orientation.
     * @function Entities.getAbsoluteJointRotationInObjectFrame
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @returns {Quat} The rotation of the joint relative to the entity's orientation if the entity is a
     *     {@link Entities.EntityProperties-Model|Model} entity, the entity is loaded, and the joint index is valid; otherwise 
     *     <code>{@link Quat(0)|Quat.IDENTITY}</code>.
     * @example <caption>Compare the local and absolute rotations of an avatar model's left hand joint.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var index = Entities.getJointIndex(entityID, "LeftHand");
     *     var localRotation = Entities.getLocalJointRotation(entityID, index);
     *     var absoluteRotation = Entities.getAbsoluteJointRotationInObjectFrame(entityID, index);
     *     print("Left hand local rotation: " + JSON.stringify(Quat.safeEulerAngles(localRotation)));
     *     print("Left hand absolute rotation: " + JSON.stringify(Quat.safeEulerAngles(absoluteRotation)));
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::quat getAbsoluteJointRotationInObjectFrame(const QUuid& entityID, int jointIndex);

    /*@jsdoc
     * Sets the translation of a joint in a {@link Entities.EntityProperties-Model|Model} entity relative to the entity's 
     * position and orientation.
     * @function Entities.setAbsoluteJointTranslationInObjectFrame
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @param {Vec3} translation - The translation to set the joint to relative to the entity's position and orientation.
     * @returns {boolean} <code>true</code>if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the joint index is valid, and the translation is different to the joint's current translation; otherwise 
     *     <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setAbsoluteJointTranslationInObjectFrame(const QUuid& entityID, int jointIndex, glm::vec3 translation);

    /*@jsdoc
     * Sets the rotation of a joint in a {@link Entities.EntityProperties-Model|Model} entity relative to the entity's position 
     * and orientation.
     * @function Entities.setAbsoluteJointRotationInObjectFrame
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @param {Quat} rotation - The rotation to set the joint to relative to the entity's orientation.
     * @returns {boolean} <code>true</code> if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the joint index is valid, and the rotation is different to the joint's current rotation; otherwise 
     *     <code>false</code>.
     * @example <caption>Raise an avatar model's left palm.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var index = Entities.getJointIndex(entityID, "LeftHand");
     *     var absoluteRotation = Entities.getAbsoluteJointRotationInObjectFrame(entityID, index);
     *     absoluteRotation = Quat.multiply(Quat.fromPitchYawRollDegrees(0, 0, 90), absoluteRotation);
     *     var success = Entities.setAbsoluteJointRotationInObjectFrame(entityID, index, absoluteRotation);
     *     print("Success: " + success);
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setAbsoluteJointRotationInObjectFrame(const QUuid& entityID, int jointIndex, glm::quat rotation);


    /*@jsdoc
     * Gets the local translation of a joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.getLocalJointTranslation
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @returns {Vec3} The local translation of the joint if the entity is a {@link Entities.EntityProperties-Model|Model} 
     *     entity, the entity is loaded, and the joint index is valid; otherwise <code>{@link Vec3(0)|Vec3.ZERO}</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::vec3 getLocalJointTranslation(const QUuid& entityID, int jointIndex);

    /*@jsdoc
     * Gets the local rotation of a joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.getLocalJointRotation
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @returns {Quat} The local rotation of the joint if the entity is a {@link Entities.EntityProperties-Model|Model} entity, 
     *     the entity is loaded, and the joint index is valid; otherwise <code>{@link Quat(0)|Quat.IDENTITY}</code>.
     * @example <caption>Report the local rotation of an avatar model's head joint.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var index = Entities.getJointIndex(entityID, "Head");
     *     var rotation = Entities.getLocalJointRotation(entityID,  index);
     *     print("Head local rotation: " + JSON.stringify(Quat.safeEulerAngles(rotation)));
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE glm::quat getLocalJointRotation(const QUuid& entityID, int jointIndex);

    /*@jsdoc
     * Sets the local translation of a joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.setLocalJointTranslation
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @param {Vec3} translation - The local translation to set the joint to.
     * @returns {boolean} <code>true</code>if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the joint index is valid, and the translation is different to the joint's current translation; otherwise 
     *     <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setLocalJointTranslation(const QUuid& entityID, int jointIndex, glm::vec3 translation);

    /*@jsdoc
     * Sets the local rotation of a joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.setLocalJointRotation
     * @param {Uuid} entityID - The ID of the entity.
     * @param {number} jointIndex - The integer index of the joint.
     * @param {Quat} rotation - The local rotation to set the joint to.
     * @returns {boolean} <code>true</code> if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the joint index is valid, and the rotation is different to the joint's current rotation; otherwise 
     *     <code>false</code>.
     * @example <caption>Make an avatar model turn its head left.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var index = Entities.getJointIndex(entityID, "Head");
     *     var rotation = Quat.fromPitchYawRollDegrees(0, 60, 0);
     *     var success = Entities.setLocalJointRotation(entityID, index, rotation);
     *     print("Success: " + success);
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setLocalJointRotation(const QUuid& entityID, int jointIndex, glm::quat rotation);


    /*@jsdoc
     * Sets the local translations of joints in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.setLocalJointTranslations
     * @param {Uuid} entityID - The ID of the entity.
     * @param {Vec3[]} translations - The local translations to set the joints to.
     * @returns {boolean} <code>true</code>if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the model has joints, and at least one of the translations is different to the model's current 
     *     translations; otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setLocalJointTranslations(const QUuid& entityID, const QVector<glm::vec3>& translations);

    /*@jsdoc
     * Sets the local rotations of joints in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.setLocalJointRotations
     * @param {Uuid} entityID - The ID of the entity.
     * @param {Quat[]} rotations - The local rotations to set the joints to.
     * @returns {boolean} <code>true</code> if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the model has joints, and at least one of the rotations is different to the model's current rotations; 
     *     otherwise <code>false</code>.
     * @example <caption>Raise both palms of an avatar model.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *
     *     // Get all the joint rotations.
     *     var jointNames = Entities.getJointNames(entityID);
     *     var jointRotations = [];
     *     for (var i = 0, length = jointNames.length; i < length; i++) {
     *         var index = Entities.getJointIndex(entityID, jointNames[i]);
     *         jointRotations.push(Entities.getLocalJointRotation(entityID, index));
     *     }
     *
     *     // Raise both palms.
     *     var index = jointNames.indexOf("LeftHand");
     *     jointRotations[index] = Quat.multiply(Quat.fromPitchYawRollDegrees(-90, 0, 0), jointRotations[index]);
     *     index = jointNames.indexOf("RightHand");
     *     jointRotations[index] = Quat.multiply(Quat.fromPitchYawRollDegrees(-90, 0, 0), jointRotations[index]);
     *
     *     // Update all the joint rotations.
     *     var success = Entities.setLocalJointRotations(entityID, jointRotations);
     *     print("Success: " + success);
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setLocalJointRotations(const QUuid& entityID, const QVector<glm::quat>& rotations);

    /*@jsdoc
     * Sets the local rotations and translations of joints in a {@link Entities.EntityProperties-Model|Model} entity. This is 
     * the same as calling both {@link Entities.setLocalJointRotations|setLocalJointRotations} and 
     * {@link Entities.setLocalJointTranslations|setLocalJointTranslations} at the same time.
     * @function Entities.setLocalJointsData
     * @param {Uuid} entityID - The ID of the entity.
     * @param {Quat[]} rotations - The local rotations to set the joints to.
     * @param {Vec3[]} translations - The local translations to set the joints to.
     * @returns {boolean} <code>true</code> if the entity is a {@link Entities.EntityProperties-Model|Model} entity, the entity 
     *     is loaded, the model has joints, and at least one of the rotations or translations is different to the model's 
     *     current values; otherwise <code>false</code>.
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE bool setLocalJointsData(const QUuid& entityID,
                                        const QVector<glm::quat>& rotations,
                                        const QVector<glm::vec3>& translations);


    /*@jsdoc
     * Gets the index of a named joint in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.getJointIndex
     * @param {Uuid} entityID - The ID of the entity.
     * @param {string} name - The name of the joint.
     * @returns {number} The integer index of the joint if the entity is a {@link Entities.EntityProperties-Model|Model} 
     *     entity, the entity is loaded, and the joint is present; otherwise <code>-1</code>. The joint indexes are in order 
     *     per {@link Entities.getJointNames|getJointNames}.
     * @example <caption>Report the index of a model's head joint.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var index = Entities.getJointIndex(entityID, "Head");
     *     print("Head joint index: " + index);
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE int getJointIndex(const QUuid& entityID, const QString& name);

    /*@jsdoc
     * Gets the names of all the joints in a {@link Entities.EntityProperties-Model|Model} entity.
     * @function Entities.getJointNames
     * @param {Uuid} entityID - The ID of the {@link Entities.EntityProperties-Model|Model} entity.
     * @returns {string[]} The names of all the joints in the entity if it is a {@link Entities.EntityProperties-Model|Model} 
     *     entity and is loaded, otherwise an empty array. The joint names are in order per 
     *     {@link Entities.getJointIndex|getJointIndex}.
     * @example <caption>Report a model's joint names.</caption>
     * entityID = Entities.addEntity({
     *     type: "Model",
     *     modelURL: "https://github.com/highfidelity/hifi-api-docs/blob/master/docs/blue_suited.fbx?raw=true",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     rotation: MyAvatar.orientation,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * Script.setTimeout(function () {
     *     // Joint data aren't available until after the model has loaded.
     *     var jointNames = Entities.getJointNames(entityID);
     *     print("Joint names: " + JSON.stringify(jointNames));
     * }, 2000);
     */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE QStringList getJointNames(const QUuid& entityID);


    /*@jsdoc
     * Gets the IDs of entities and avatars that are directly parented to an entity or avatar model. To get all descendants, 
     * you can recurse on the IDs returned.
     * @function Entities.getChildrenIDs
     * @param {Uuid} parentID - The ID of the entity or avatar to get the children IDs of.
     * @returns {Uuid[]} An array of entity and avatar IDs that are parented directly to the <code>parentID</code> 
     *     entity or avatar. Does not include children's children, etc. The array is empty if no children can be found or 
     *     <code>parentID</code> cannot be found.
     * @example <caption>Report the children of an entity.</caption>
     * function createEntity(description, position, parent) {
     *     var entity = Entities.addEntity({
     *         type: "Sphere",
     *         position: position,
     *         dimensions: Vec3.HALF,
     *         parentID: parent,
     *         lifetime: 300  // Delete after 5 minutes.
     *     });
     *     print(description + ": " + entity);
     *     return entity;
     * }
     *
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 2, z: -5 }));
     * var root = createEntity("Root", position, Uuid.NULL);
     * var child = createEntity("Child", Vec3.sum(position, { x: 0, y: -1, z: 0 }), root);
     * var grandChild = createEntity("Grandchild", Vec3.sum(position, { x: 0, y: -2, z: 0 }), child);
     *
     * var children = Entities.getChildrenIDs(root);
     * print("Children of root: " + JSON.stringify(children));  // Only the child entity.
     */
    Q_INVOKABLE QVector<QUuid> getChildrenIDs(const QUuid& parentID);

    /*@jsdoc
     * Gets the IDs of entities and avatars that are directly parented to an entity or avatar model's joint. To get all 
     * descendants, you can use {@link Entities.getChildrenIDs|getChildrenIDs} to recurse on the IDs returned.
     * @function Entities.getChildrenIDsOfJoint
     * @param {Uuid} parentID - The ID of the entity or avatar to get the children IDs of.
     * @param {number} jointIndex - Integer number of the model joint to get the children IDs of.
     * @returns {Uuid[]} An array of entity and avatar IDs that are parented directly to the <code>parentID</code> 
     *     entity or avatar at the <code>jointIndex</code> joint. Does not include children's children, etc. The 
     *     array is empty if no children can be found or <code>parentID</code> cannot be found.
     * @example <caption>Report the children of your avatar's right hand.</caption>
     * function createEntity(description, position, parent) {
     *     var entity = Entities.addEntity({
     *         type: "Sphere",
     *         position: position,
     *         dimensions: Vec3.HALF,
     *         parentID: parent,
     *         lifetime: 300  // Delete after 5 minutes.
     *     });
     *     print(description + ": " + entity);
     *     return entity;
     * }
     *
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 2, z: -5 }));
     * var root = createEntity("Root", position, Uuid.NULL);
     * var child = createEntity("Child", Vec3.sum(position, { x: 0, y: -1, z: 0 }), root);
     *
     * Script.setTimeout(function () { // Wait for the entity to be created before editing.
     *     Entities.editEntity(root, {
     *         parentID: MyAvatar.sessionUUID,
     *         parentJointIndex: MyAvatar.getJointIndex("RightHand")
     *     });
     * 
     *     var children = Entities.getChildrenIDsOfJoint(MyAvatar.sessionUUID, MyAvatar.getJointIndex("RightHand"));
     *     print("Children of hand: " + JSON.stringify(children));  // Only the root entity.
     * }, 50);
     */
    Q_INVOKABLE QVector<QUuid> getChildrenIDsOfJoint(const QUuid& parentID, int jointIndex);

    /*@jsdoc
     * Checks whether an entity has an entity as an ancestor (parent, parent's parent, etc.).
     * @function Entities.isChildOfParent
     * @param {Uuid} childID - The ID of the child entity to test for being a child, grandchild, etc.
     * @param {Uuid} parentID - The ID of the parent entity to test for being a parent, grandparent, etc.
     * @returns {boolean} <code>true</code> if the <code>childID</code> entity has the <code>parentID</code> entity 
     *     as a parent or grandparent etc., otherwise <code>false</code>.
     * @example <caption>Check that a grandchild entity is a child of its grandparent.</caption>
     * function createEntity(description, position, parent) {
     *     var entity = Entities.addEntity({
     *         type: "Sphere",
     *         position: position,
     *         dimensions: Vec3.HALF,
     *         parentID: parent,
     *         lifetime: 300  // Delete after 5 minutes.
     *     });
     *     print(description + ": " + entity);
     *     return entity;
     * }
     *
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 2, z: -5 }));
     * var root = createEntity("Root", position, Uuid.NULL);
     * var child = createEntity("Child", Vec3.sum(position, { x: 0, y: -1, z: 0 }), root);
     * var grandChild = createEntity("Grandchild", Vec3.sum(position, { x: 0, y: -2, z: 0 }), child);
     *
     * print("grandChild has root as parent: " + Entities.isChildOfParent(grandChild, root));  // true
     */
    Q_INVOKABLE bool isChildOfParent(const QUuid& childID, const QUuid& parentID);

    /*@jsdoc
     * Gets the type &mdash; entity or avatar &mdash; of an in-world item.
     * @function Entities.getNestableType
     * @param {Uuid} id - The ID of the item to get the type of.
     * @returns {Entities.NestableType} The type of the item.
     * @example <caption>Report some nestable types.</caption>
     * var entity = Entities.addEntity({
     *     type: "Sphere",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 1, z: -2 })),
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * print(Entities.getNestableType(entity));  // "entity"
     * print(Entities.getNestableType(Uuid.generate()));  // "unknown"
     */
    Q_INVOKABLE QString getNestableType(const QUuid& id);

    /*@jsdoc
     * Gets the ID of the {@link Entities.EntityProperties-Web|Web} entity that has keyboard focus.
     * @function Entities.getKeyboardFocusEntity
     * @returns {Uuid} The ID of the {@link Entities.EntityProperties-Web|Web} entity that has focus, if any, otherwise <code>null</code>.
     */
    Q_INVOKABLE QUuid getKeyboardFocusEntity() const;

    /*@jsdoc
     * Sets the {@link Entities.EntityProperties-Web|Web} entity that has keyboard focus.
     * @function Entities.setKeyboardFocusEntity
     * @param {Uuid} id - The ID of the {@link Entities.EntityProperties-Web|Web} entity to set keyboard focus to. Use 
     *     <code>null</code> or {@link Uuid(0)|Uuid.NULL} to unset keyboard focus from an entity.
     */
    Q_INVOKABLE void setKeyboardFocusEntity(const QUuid& id);

    /*@jsdoc
     * Emits a {@link Entities.mousePressOnEntity|mousePressOnEntity} event.
     * @function Entities.sendMousePressOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendMousePressOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.mouseMoveOnEntity|mouseMoveOnEntity} event.
     * @function Entities.sendMouseMoveOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendMouseMoveOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.mouseReleaseOnEntity|mouseReleaseOnEntity} event.
     * @function Entities.sendMouseReleaseOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendMouseReleaseOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.clickDownOnEntity|clickDownOnEntity} event.
     * @function Entities.sendClickDownOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendClickDownOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.holdingClickOnEntity|holdingClickOnEntity} event.
     * @function Entities.sendHoldingClickOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendHoldingClickOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.clickReleaseOnEntity|clickReleaseOnEntity} event.
     * @function Entities.sendClickReleaseOnEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendClickReleaseOnEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.hoverEnterEntity|hoverEnterEntity} event.
     * @function Entities.sendHoverEnterEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendHoverEnterEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.hoverOverEntity|hoverOverEntity} event.
     * @function Entities.sendHoverOverEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendHoverOverEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Emits a {@link Entities.hoverLeaveEntity|hoverLeaveEntity} event.
     * @function Entities.sendHoverLeaveEntity
     * @param {Uuid} entityID - The ID of the entity to emit the event for.
     * @param {PointerEvent} event - The event details.
     */
    Q_INVOKABLE void sendHoverLeaveEntity(const EntityItemID& id, const PointerEvent& event);

    /*@jsdoc
     * Checks whether an entity wants hand controller pointer events. For example, a {@link Entities.EntityProperties-Web|Web} 
     * entity does but a {@link Entities.EntityProperties-Shape|Shape} entity doesn't.
     * @function Entities.wantsHandControllerPointerEvents
     * @param {Uuid} entityID -  The ID of the entity.
     * @returns {boolean} <code>true</code> if the entity can be found and it wants hand controller pointer events, otherwise 
     *     <code>false</code>.
     */
    Q_INVOKABLE bool wantsHandControllerPointerEvents(const QUuid& id);

    /*@jsdoc
     * Sends a message to a {@link Entities.EntityProperties-Web|Web} entity's HTML page. To receive the message, the web 
     * page's script must connect to the <code>EventBridge</code> that is automatically provided to the script:
     * <pre class="prettyprint"><code>EventBridge.scriptEventReceived.connect(function(message) {
     *     ...
     * });</code></pre>
     * <p>Use {@link Entities.webEventReceived} to receive messages from the Web entity's HTML page.</p>
     * <p>Alternatively, you can use {@link Entities.getEntityObject} to exchange messages over a Web entity's HTML event
     * bridge.</p>
     * @function Entities.emitScriptEvent
     * @param {Uuid} entityID - The ID of the Web entity to send the message to.
     * @param {string} message - The message to send.
     * @example <caption>Exchange messages with a Web entity.</caption>
     * // HTML file, name: "webEntity.html".
     * <!DOCTYPE html>
     * <html>
     * <head>
     *     <title>HELLO</title>
     * </head>
     * <body>
     *     <h1>HELLO</h1>
     *     <script>
     *         function onScriptEventReceived(message) {
     *             // Message received from the script.
     *             console.log("Message received: " + message);
     *         }
     *
     *         EventBridge.scriptEventReceived.connect(onScriptEventReceived);
     *
     *         setTimeout(function () {
     *             // Send a message to the script.
     *             EventBridge.emitWebEvent("hello");
     *         }, 5000);
     *     </script>
     * </body>
     * </html>
     *
     * // Script file.
     * var webEntity = Entities.addEntity({
     *     type: "Web",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.5, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     sourceUrl: Script.resolvePath("webEntity.html"),
     *     alpha: 1.0,
     *     lifetime: 300  // 5 min
     * });
     *
     * function onWebEventReceived(entityID, message) {
     *     if (entityID === webEntity) {
     *         // Message received.
     *         print("Message received: " + message);
     * 
     *         // Send a message back.
     *         Entities.emitScriptEvent(webEntity, message + " back");
     *     }
     * }
     * 
     * Entities.webEventReceived.connect(onWebEventReceived);
     */
    Q_INVOKABLE void emitScriptEvent(const EntityItemID& entityID, const QVariant& message);

    /*@jsdoc
     * Checks whether an axis-aligned box and a capsule intersect.
     * @function Entities.AABoxIntersectsCapsule
     * @param {Vec3} brn - The bottom right near (minimum axes values) corner of the AA box.
     * @param {Vec3} dimensions - The dimensions of the AA box.
     * @param {Vec3} start - One end of the capsule.
     * @param {Vec3} end - The other end of the capsule.
     * @param {number} radius - The radius of the capsule.
     * @returns {boolean} <code>true</code> if the AA box and capsule intersect, otherwise <code>false</code>.
     */
    Q_INVOKABLE bool AABoxIntersectsCapsule(const glm::vec3& low, const glm::vec3& dimensions,
                                            const glm::vec3& start, const glm::vec3& end, float radius);

    /*@jsdoc
     * Gets the meshes in a {@link Entities.EntityProperties-Model|Model} or {@link Entities.EntityProperties-PolyVox|PolyVox} 
     * entity.
     * @function Entities.getMeshes
     * @param {Uuid} entityID - The ID of the <code>Model</code> or <code>PolyVox</code> entity to get the meshes of.
     * @param {Entities~getMeshesCallback} callback - The function to call upon completion.
     * @deprecated This function is deprecated and will be removed. It no longer works for Model entities. Use the 
     *     {@link Graphics} API instead.
     */
     /*@jsdoc
      * Called when a {@link Entities.getMeshes} call is complete.
      * @callback Entities~getMeshesCallback
      * @param {MeshProxy[]} meshes - If <code>success</code> is <code>true</code>, a {@link MeshProxy} per mesh in the 
      *     <code>Model</code> or <code>PolyVox</code> entity; otherwise <code>undefined</code>. 
      * @param {boolean} success - <code>true</code> if the {@link Entities.getMeshes} call was successful, <code>false</code> 
      *     otherwise. The call may be unsuccessful if the requested entity could not be found.
      * @deprecated This function is deprecated and will be removed. It no longer works for Model entities. Use the 
      *     {@link Graphics} API instead. 
      */
    // FIXME move to a renderable entity interface
    Q_INVOKABLE void getMeshes(const QUuid& entityID, ScriptValuePointer callback);

    /*@jsdoc
     * Gets the object to world transform, excluding scale, of an entity.
     * @function Entities.getEntityTransform
     * @param {Uuid} entityID - The ID of the entity.
     * @returns {Mat4} The entity's object to world transform excluding scale (i.e., translation and rotation, with scale of 1) 
     *    if the entity can be found, otherwise a transform with zero translation and rotation and a scale of 1.
     * @example <caption>Position and rotation in an entity's world transform.</caption>
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 1, z: -2 }));
     * var orientation = MyAvatar.orientation;
     * print("Position: " + JSON.stringify(position));
     * print("Orientation: " + JSON.stringify(orientation));
     *
     * var entityID = Entities.addEntity({
     *     type: "Sphere",
     *     position: position,
     *     rotation: orientation,
     *     dimensions: Vec3.HALF,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     *
     * var transform = Entities.getEntityTransform(entityID);
     * print("Transform: " + JSON.stringify(transform));
     * print("Translation: " + JSON.stringify(Mat4.extractTranslation(transform)));  // Same as position.
     * print("Rotation: " + JSON.stringify(Mat4.extractRotation(transform)));  // Same as orientation.
     * print("Scale: " + JSON.stringify(Mat4.extractScale(transform)));  // { x: 1, y: 1, z: 1 }
     */
    Q_INVOKABLE glm::mat4 getEntityTransform(const QUuid& entityID);

    /*@jsdoc
     * Gets the object to parent transform, excluding scale, of an entity.
     * @function Entities.getEntityLocalTransform
     * @param {Uuid} entityID - The ID of the entity.
     * @returns {Mat4} The entity's object to parent transform excluding scale (i.e., translation and rotation, with scale of 
     *     1) if the entity can be found, otherwise a transform with zero translation and rotation and a scale of 1. If the 
     *     entity doesn't have a parent, its world transform is returned.
     * @example <caption>Position and rotation in an entity's local transform.</caption>
     * function createEntity(position, rotation, parent) {
     *     var entity = Entities.addEntity({
     *         type: "Box",
     *         position: position,
     *         rotation: rotation,
     *         dimensions: Vec3.HALF,
     *         parentID: parent,
     *         lifetime: 300  // Delete after 5 minutes.
     *     });
     *     return entity;
     * }
     *
     * var position = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 2, z: -5 }));
     *
     * var parent = createEntity(position, MyAvatar.orientation, Uuid.NULL);
     *
     * var childTranslation = { x: 0, y: -1.5, z: 0 };
     * var childRotation = Quat.fromPitchYawRollDegrees(0, 45, 0);
     * var child = createEntity(Vec3.sum(position, childTranslation), Quat.multiply(childRotation, MyAvatar.orientation), parent);
     *
     * var transform = Entities.getEntityLocalTransform(child);
     * print("Transform: " + JSON.stringify(transform));
     * print("Translation: " + JSON.stringify(Mat4.extractTranslation(transform)));  // childTranslation
     * print("Rotation: " + JSON.stringify(Quat.safeEulerAngles(Mat4.extractRotation(transform))));  // childRotation
     * print("Scale: " + JSON.stringify(Mat4.extractScale(transform)));  // { x: 1, y: 1, z: 1 }     */
    Q_INVOKABLE glm::mat4 getEntityLocalTransform(const QUuid& entityID);


    /*@jsdoc
     * Converts a position in world coordinates to a position in an avatar, entity, or joint's local coordinates.
     * @function Entities.worldToLocalPosition
     * @param {Vec3} worldPosition - The world position to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent= false] - <code>true</code> to scale the local position per the parent's scale, 
     *     <code>false</code> for the local position to be at world scale.
     * @returns {Vec3} The position converted to local coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     * @example <caption>Report the local coordinates of an entity parented to another.</caption>
     * var parentPosition = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 }));
     * var childPosition = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 1, z: -5 }));
     * 
     * var parentEntity = Entities.addEntity({
     *     type: "Box",
     *     position: parentPosition,
     *     rotation: MyAvatar.orientation,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var childEntity = Entities.addEntity({
     *     type: "Sphere",
     *     position: childPosition,
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     parentID: parentEntity,
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * var localPosition = Entities.worldToLocalPosition(childPosition, parentEntity);
     * print("Local position: " + JSON.stringify(localPosition));  // 0, 1, 0.
     * localPosition = Entities.getEntityProperties(childEntity, "localPosition").localPosition;
     * print("Local position: " + JSON.stringify(localPosition));  // The same.
     */
    Q_INVOKABLE glm::vec3 worldToLocalPosition(glm::vec3 worldPosition, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a rotation or orientation in world coordinates to rotation in an avatar, entity, or joint's local coordinates.
     * @function Entities.worldToLocalRotation
     * @param {Quat} worldRotation - The world rotation to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent=false] - <em>Not used in the calculation.</em>
     * @returns {Quat} The rotation converted to local coordinates if successful, otherwise {@link Quat(0)|Quat.IDENTITY}.
     */
    Q_INVOKABLE glm::quat worldToLocalRotation(glm::quat worldRotation, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a velocity in world coordinates to a velocity in an avatar, entity, or joint's local coordinates.
     * @function Entities.worldToLocalVelocity
     * @param {Vec3} worldVelocity - The world velocity to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent=false] - <code>true</code> to scale the local velocity per the parent's scale, 
     *     <code>false</code> for the local velocity to be at world scale.
     * @returns {Vec3} The velocity converted to local coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 worldToLocalVelocity(glm::vec3 worldVelocity, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a Euler angular velocity in world coordinates to an angular velocity in an avatar, entity, or joint's local 
     * coordinates.
     * @function Entities.worldToLocalAngularVelocity
     * @param {Vec3} worldAngularVelocity - The world Euler angular velocity to convert. (Can be in any unit, e.g., deg/s or 
     *     rad/s.)
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent=false] - <em>Not used in the calculation.</em>
     * @returns {Vec3} The angular velocity converted to local coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 worldToLocalAngularVelocity(glm::vec3 worldAngularVelocity, const QUuid& parentID,
                                                      int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts dimensions in world coordinates to dimensions in an avatar or entity's local coordinates.
     * @function Entities.worldToLocalDimensions
     * @param {Vec3} worldDimensions - The world dimensions to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - <em>Not used in the calculation.</em>
     * @param {boolean} [scalesWithParent=false] - <code>true</code> to scale the local dimensions per the parent's scale, 
     *     <code>false</code> for the local dimensions to be at world scale.
     * @returns {Vec3} The dimensions converted to local coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 worldToLocalDimensions(glm::vec3 worldDimensions, const QUuid& parentID,
                                                 int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a position in an avatar, entity, or joint's local coordinate to a position in world coordinates.
     * @function Entities.localToWorldPosition
     * @param {Vec3} localPosition - The local position to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithparent=false] - <code>true</code> if the local dimensions are scaled per the parent's scale, 
     *     <code>false</code> if the local dimensions are at world scale.
     * @returns {Vec3} The position converted to world coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 localToWorldPosition(glm::vec3 localPosition, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a rotation or orientation in an avatar, entity, or joint's local coordinate to a rotation in world coordinates.
     * @function Entities.localToWorldRotation
     * @param {Quat} localRotation - The local rotation to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent= false] - <em>Not used in the calculation.</em>
     * @returns {Quat} The rotation converted to local coordinates if successful, otherwise {@link Quat(0)|Quat.IDENTITY}.
     */
    Q_INVOKABLE glm::quat localToWorldRotation(glm::quat localRotation, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a velocity in an avatar, entity, or joint's local coordinate to a velocity in world coordinates.
     * @function Entities.localToWorldVelocity
     * @param {Vec3} localVelocity - The local velocity to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent= false] - <code>true</code> if the local velocity is scaled per the parent's scale, 
     *     <code>false</code> if the local velocity is at world scale.
     * @returns {Vec3} The velocity converted to world coordinates it successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 localToWorldVelocity(glm::vec3 localVelocity, const QUuid& parentID,
                                               int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts a Euler angular velocity in an avatar, entity, or joint's local coordinate to an angular velocity in world 
     * coordinates.
     * @function Entities.localToWorldAngularVelocity
     * @param {Vec3} localAngularVelocity - The local Euler angular velocity to convert. (Can be in any unit, e.g., deg/s or 
     * rad/s.)
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - The joint in the avatar or entity that the local coordinates are based on. If 
     *     <code>-1</code> then no joint is used and the local coordinates are based solely on the avatar or entity.
     * @param {boolean} [scalesWithParent= false] - <em>Not used in the calculation.</em>
     * @returns {Vec3} The angular velocity converted to world coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 localToWorldAngularVelocity(glm::vec3 localAngularVelocity, const QUuid& parentID,
                                                      int parentJointIndex = -1, bool scalesWithParent = false);
    /*@jsdoc
     * Converts dimensions in an avatar or entity's local coordinates to dimensions in world coordinates.
     * @function Entities.localToWorldDimensions
     * @param {Vec3} localDimensions - The local dimensions to convert.
     * @param {Uuid} parentID - The avatar or entity that the local coordinates are based on.
     * @param {number} [parentJointIndex=-1] - <em>Not used in the calculation.</em>
     * @param {boolean} [scalesWithParent= false] - <code>true</code> if the local dimensions are scaled per the parent's 
     *     scale, <code>false</code> if the local dimensions are at world scale.
     * @returns {Vec3} The dimensions converted to world coordinates if successful, otherwise {@link Vec3(0)|Vec3.ZERO}.
     */
    Q_INVOKABLE glm::vec3 localToWorldDimensions(glm::vec3 localDimensions, const QUuid& parentID,
                                                 int parentJointIndex = -1, bool scalesWithParent = false);


    /*@jsdoc
     * Gets the static certificate for an entity. The static certificate contains static properties of the item which cannot
     * be altered.
     * @function Entities.getStaticCertificateJSON
     * @param {Uuid} entityID - The ID of the entity to get the static certificate for.
     * @returns {string} The entity's static certificate as a JSON string if the entity can be found, otherwise <code>""</code>.
     */
    Q_INVOKABLE QString getStaticCertificateJSON(const QUuid& entityID);

    /*@jsdoc
     * Verifies the entity's proof of provenance, i.e., that the entity's <code>certificateID</code> property was produced by
     * High Fidelity signing the entity's static certificate JSON.
     * @function Entities.verifyStaticCertificateProperties
     * @param {Uuid} entityID - The ID of the entity to verify.
     * @returns {boolean} <code>true</code> if the entity can be found, its <code>certificateID</code> property is present, and  
     *     its value matches the entity's static certificate JSON; otherwise <code>false</code>.
     */
    Q_INVOKABLE bool verifyStaticCertificateProperties(const QUuid& entityID);

    /*@jsdoc
     * Gets information about an entity property, including a minimum to maximum range for some numerical properties.
     * @function Entities.getPropertyInfo
     * @param {string} propertyName - The name of the property to get the information for.
     * @returns {Entities.EntityPropertyInfo} The information about the property if it can be found, otherwise an empty object.
     * @example <caption>Report property information for some properties.</caption>
     * print("alpha: " + JSON.stringify(Entities.getPropertyInfo("alpha")));
     * print("script: " + JSON.stringify(Entities.getPropertyInfo("script")));
     */
    Q_INVOKABLE const EntityPropertyInfo getPropertyInfo(const QString& propertyName) const;

signals:
    /*@jsdoc
     * Triggered on the client that is the physics simulation owner during the collision of two entities. Note: Isn't triggered
     * for a collision with an avatar.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.collisionWithEntity
     * @param {Uuid} idA - The ID of one entity in the collision. For an entity script, this is the ID of the entity containing 
     *     the script.
     * @param {Uuid} idB - The ID of the other entity in the collision.
     * @param {Collision} collision - The details of the collision.
     * @returns {Signal}
     * @example <caption>Change the color of an entity when it collides with another entity.</caption>
     * var entityScript = (function () {
     *     function randomInteger(min, max) {
     *         return Math.floor(Math.random() * (max - min + 1)) + min;
     *     }
     *
     *     this.collisionWithEntity = function (myID, otherID, collision) {
     *         Entities.editEntity(myID, {
     *             color: {
     *                 red: randomInteger(128, 255),
     *                 green: randomInteger(128, 255),
     *                 blue: randomInteger(128, 255)
     *             }
     *         });
     *     };
     * });
     *
     * var entityID = Entities.addEntity({
     *     type: "Box",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
     *     dimensions: { x: 0.5, y: 0.5, z: 0.5 },
     *     color: { red: 128, green: 128, blue: 128 },
     *     gravity: { x: 0, y: -9.8, z: 0 },
     *     velocity: { x: 0, y: 0.1, z: 0 },  // Kick off physics.
     *     dynamic: true,
     *     collisionless: false,  // So that collision events are generated.
     *     script: "(" + entityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     */
    void collisionWithEntity(const EntityItemID& idA, const EntityItemID& idB, const Collision& collision);

    /*@jsdoc
     * Triggered when your ability to change the <code>locked</code> property of entities changes.
     * @function Entities.canAdjustLocksChanged
     * @param {boolean} canAdjustLocks - <code>true</code> if the script can change the <code>locked</code> property of an 
     *     entity, <code>false</code> if it can't.
     * @returns {Signal}
     * @example <caption>Report when your ability to change locks changes.</caption>
     * function onCanAdjustLocksChanged(canAdjustLocks) {
     *     print("You can adjust entity locks: " + canAdjustLocks);
     * }
     * Entities.canAdjustLocksChanged.connect(onCanAdjustLocksChanged);
     */
    void canAdjustLocksChanged(bool canAdjustLocks);

    /*@jsdoc
     * Triggered when your ability to rez (create) entities changes.
     * @function Entities.canRezChanged
     * @param {boolean} canRez - <code>true</code> if the script can rez (create) entities, <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canRezChanged(bool canRez);

    /*@jsdoc
     * Triggered when your ability to rez (create) temporary entities changes. Temporary entities are entities with a finite
     * <code>lifetime</code> property value set.
     * @function Entities.canRezTmpChanged
     * @param {boolean} canRezTmp - <code>true</code> if the script can rez (create) temporary entities, <code>false</code> if 
     *   it can't.
     * @returns {Signal}
     */
    void canRezTmpChanged(bool canRezTmp);

    /*@jsdoc
     * Triggered when your ability to rez (create) certified entities changes. Certified entities are entities that have PoP
     * certificates.
     * @function Entities.canRezCertifiedChanged
     * @param {boolean} canRezCertified - <code>true</code> if the script can rez (create) certified entities, 
     *     <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canRezCertifiedChanged(bool canRezCertified);

    /*@jsdoc
     * Triggered when your ability to rez (create) temporary certified entities changes. Temporary entities are entities with a
     * finite <code>lifetime</code> property value set. Certified entities are entities that have PoP certificates.
     * @function Entities.canRezTmpCertifiedChanged
     * @param {boolean} canRezTmpCertified - <code>true</code> if the script can rez (create) temporary certified entities,
     *     <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canRezTmpCertifiedChanged(bool canRezTmpCertified);

    /*@jsdoc
     * Triggered when your ability to make changes to the asset server's assets changes.
     * @function Entities.canWriteAssetsChanged
     * @param {boolean} canWriteAssets - <code>true</code> if the script can change the asset server's assets,
     *     <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canWriteAssetsChanged(bool canWriteAssets);

    /*@jsdoc
     * Triggered when your ability to get and set private user data changes.
     * @function Entities.canGetAndSetPrivateUserDataChanged
     * @param {boolean} canGetAndSetPrivateUserData - <code>true</code> if the script can change the <code>privateUserData</code> 
     *     property of an entity, <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canGetAndSetPrivateUserDataChanged(bool canGetAndSetPrivateUserData);
    
    /*@jsdoc
     * Triggered when your ability to use avatar entities is changed.
     * @function Entities.canRezAvatarEntitiesChanged
     * @param {boolean} canRezAvatarEntities - <code>true</code> if the script can change edit avatar entities,
     *     <code>false</code> if it can't.
     * @returns {Signal}
     */
    void canRezAvatarEntitiesChanged(bool canRezAvatarEntities);


    /*@jsdoc
     * Triggered when a mouse button is clicked while the mouse cursor is on an entity, or a controller trigger is fully 
     * pressed while its laser is on an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.mousePressOnEntity
     * @param {Uuid} entityID - The ID of the entity that was pressed on.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     * @example <caption>Report when an entity is clicked with the mouse or laser.</caption>
     * function onMousePressOnEntity(entityID, event) {
     *     print("Clicked on entity: " + entityID);
     * }
     *
     * Entities.mousePressOnEntity.connect(onMousePressOnEntity);
     */
    void mousePressOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse button is double-clicked while the mouse cursor is on an entity.
     * @function Entities.mouseDoublePressOnEntity
     * @param {Uuid} entityID - The ID of the entity that was double-pressed on.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void mouseDoublePressOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Repeatedly triggered while the mouse cursor or controller laser moves on an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.mouseMoveOnEntity
     * @param {Uuid} entityID - The ID of the entity that was moved on.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void mouseMoveOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse button is released after clicking on an entity or the controller trigger is partly or fully 
     * released after pressing on an entity, even if the mouse pointer or controller laser has moved off the entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.mouseReleaseOnEntity
     * @param {Uuid} entityID - The ID of the entity that was originally pressed.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void mouseReleaseOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse button is clicked while the mouse cursor is not on an entity.
     * @function Entities.mousePressOffEntity
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void mousePressOffEntity();

    /*@jsdoc
     * Triggered when a mouse button is double-clicked while the mouse cursor is not on an entity.
     * @function Entities.mouseDoublePressOffEntity
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void mouseDoublePressOffEntity();


    /*@jsdoc
     * Triggered when a mouse button is clicked while the mouse cursor is on an entity. Note: Not triggered by controllers.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.clickDownOnEntity
     * @param {Uuid} entityID - The ID of the entity that was clicked on.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     * @example <caption>Compare clickDownOnEntity signal and entity script method.</caption>
     * var entityScript = (function () {
     *     // Method is called for only this entity.
     *     this.clickDownOnEntity = function (entityID, event) {
     *         print("Entity : Clicked sphere ; " + event.type);
     *     };
     * });
     * 
     * var sphereID = Entities.addEntity({
     *     type: "Sphere",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -3 })),
     *     script: "(" + entityScript + ")",  // Could host the script on a Web server instead.
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * Entities.clickDownOnEntity.connect(function (entityID, event) {
     *     // Signal is triggered for all entities.
     *     if (entityID === sphereID) {
     *         print("Interface : Clicked sphere ; " + event.type);
     *     } else {
     *         print("Interface : Clicked another entity ; " + event.type);
     *     }
     * });
     */
    void clickDownOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Repeatedly triggered while a mouse button continues to be held after clicking an entity, even if the mouse cursor has 
     * moved off the entity. Note: Not triggered by controllers.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.holdingClickOnEntity
     * @param {Uuid} entityID - The ID of the entity that was originally clicked.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void holdingClickOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when a mouse button is released after clicking on an entity, even if the mouse cursor has moved off the 
     * entity. Note: Not triggered by controllers.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.clickReleaseOnEntity
     * @param {Uuid} entityID - The ID of the entity that was originally clicked.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void clickReleaseOnEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when the mouse cursor or controller laser starts hovering on an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.hoverEnterEntity
     * @param {Uuid} entityID - The ID of the entity that is being hovered.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void hoverEnterEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Repeatedly triggered when the mouse cursor or controller laser moves while hovering over an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.hoverOverEntity
     * @param {Uuid} entityID - The ID of the entity that is being hovered.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void hoverOverEntity(const EntityItemID& entityItemID, const PointerEvent& event);

    /*@jsdoc
     * Triggered when the mouse cursor or controller laser stops hovering over an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.hoverLeaveEntity
     * @param {Uuid} entityID - The ID of the entity that was being hovered.
     * @param {PointerEvent} event - Details of the event.
     * @returns {Signal}
     */
    void hoverLeaveEntity(const EntityItemID& entityItemID, const PointerEvent& event);


    /*@jsdoc
     * Triggered when an avatar enters an entity.
     * Note: At the initial loading of the script, if the avatar is already present inside the entity, it might be too late 
     * to catch this event when the script runs, so it won't trigger. The {@link Entities.preload|preload} signal can be used to handle those cases.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.enterEntity
     * @param {Uuid} entityID - The ID of the entity that the avatar entered.
     * @returns {Signal}
    */
    void enterEntity(const EntityItemID& entityItemID);

    /*@jsdoc
     * Triggered when an avatar leaves an entity.
     * <p>See also, {@link Entities|Entity Methods} and {@link Script.addEventHandler}.</p>
     * @function Entities.leaveEntity
     * @param {Uuid} entityID - The ID of the entity that the avatar left.
     * @returns {Signal}
     */
    void leaveEntity(const EntityItemID& entityItemID);


    /*@jsdoc
     * Triggered when an entity is deleted.
     * @function Entities.deletingEntity
     * @param {Uuid} entityID - The ID of the entity deleted.
     * @returns {Signal}
     * @example <caption>Report when an entity is deleted.</caption>
     * Entities.deletingEntity.connect(function (entityID) {
     *     print("Deleted entity: " + entityID);
     * });
     */
    void deletingEntity(const EntityItemID& entityID);

    /*@jsdoc
     * Triggered when an entity is added to Interface's local in-memory tree of entities it knows about. This may occur when
     * entities are loaded upon visiting a domain, when the user rotates their view so that more entities become visible, and 
     * when any type of entity is created (e.g., by {@link Entities.addEntity|addEntity}).
     * @function Entities.addingEntity
     * @param {Uuid} entityID - The ID of the entity added.
     * @returns {Signal}
     * @example <caption>Report when an entity is added.</caption>
     * Entities.addingEntity.connect(function (entityID) {
     *     print("Added entity: " + entityID);
     * });
     */
    void addingEntity(const EntityItemID& entityID);

    /*@jsdoc
     * Triggered when a "wearable" entity is deleted, for example when removing a "wearable" from your avatar.
     * @function Entities.deletingWearable
     * @param {Uuid} entityID - The ID of the "wearable" entity deleted.
     * @returns {Signal}
     * @example <caption>Report when a "wearable" entity is deleted.</caption>
     * Entities.deletingWearable.connect(function (entityID) {
     *     print("Deleted wearable: " + entityID);
     * });
     */
    void deletingWearable(const EntityItemID& entityID);

    /*@jsdoc
     * Triggered when a "wearable" entity is added to Interface's local in-memory tree of entities it knows about, for example 
     * when adding a "wearable" to your avatar.
     * @function Entities.addingWearable
     * @param {Uuid} entityID - The ID of the "wearable" entity added.
     * @returns {Signal}
     * @example <caption>Report when a "wearable" entity is added.</caption>
     * Entities.addingWearable.connect(function (entityID) {
     *     print("Added wearable: " + entityID);
     * });
     */
    void addingWearable(const EntityItemID& entityID);

    /*@jsdoc
     * Triggered when you disconnect from a domain, at which time Interface's local in-memory tree of entities that it knows 
     * about is cleared.
     * @function Entities.clearingEntities
     * @returns {Signal}
     * @example <caption>Report when Interfaces's entity tree is cleared.</caption>
     * Entities.clearingEntities.connect(function () {
     *     print("Entities cleared");
     * });
     */
    void clearingEntities();
    
    /*@jsdoc
     * Triggered when a script in a {@link Entities.EntityProperties-Web|Web} entity's HTML sends an event over the entity's 
     * HTML event bridge. The HTML web page can send a message by calling:
     * <pre class="prettyprint"><code>EventBridge.emitWebEvent(message);</code></pre>
     * <p>Use {@link Entities.emitScriptEvent} to send messages to the Web entity's HTML page.</p>
     * <p>Alternatively, you can use {@link Entities.getEntityObject} to exchange messages over a Web entity's HTML event 
     * bridge.</p>
     * @function Entities.webEventReceived
     * @param {Uuid} entityID - The ID of the Web entity that the message was received from.
     * @param {string} message - The message received.
     * @returns {Signal}
     */
    void webEventReceived(const EntityItemID& entityItemID, const QVariant& message);

protected:
    void withEntitiesScriptEngine(std::function<void(QSharedPointer<EntitiesScriptEngineProvider>)> function, const EntityItemID& id) {
        auto entity = getEntityTree()->findEntityByEntityItemID(id);
        if (entity) {
            std::lock_guard<std::recursive_mutex> lock(_entitiesScriptEngineLock);
            function((entity->isLocalEntity() || entity->isMyAvatarEntity()) ? _persistentEntitiesScriptManager : _nonPersistentEntitiesScriptManager);
        }
    };

private slots:
    void handleEntityScriptCallMethodPacket(QSharedPointer<ReceivedMessage> receivedMessage, SharedNodePointer senderNode);
    void onAddingEntity(EntityItem* entity);
    void onDeletingEntity(EntityItem* entity);

private:
    bool actionWorker(const QUuid& entityID, std::function<bool(EntitySimulationPointer, EntityItemPointer)> actor);
    bool polyVoxWorker(QUuid entityID, std::function<bool(PolyVoxEntityItem&)> actor);
    bool setPoints(QUuid entityID, std::function<bool(LineEntityItem&)> actor);
    void queueEntityMessage(PacketType packetType, EntityItemID entityID, const EntityItemProperties& properties);
    bool addLocalEntityCopy(EntityItemProperties& propertiesWithSimID, EntityItemID& id, bool isClone = false);

    EntityItemPointer checkForTreeEntityAndTypeMatch(const QUuid& entityID,
                                                     EntityTypes::EntityType entityType = EntityTypes::Unknown);


    /// actually does the work of finding the ray intersection, can be called in locking mode or tryLock mode
    RayToEntityIntersectionResult evalRayIntersectionWorker(const PickRay& ray, Octree::lockType lockType,
        PickFilter searchFilter, const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard) const;

    /// actually does the work of finding the parabola intersection, can be called in locking mode or tryLock mode
    ParabolaToEntityIntersectionResult evalParabolaIntersectionWorker(const PickParabola& parabola, Octree::lockType lockType,
        PickFilter searchFilter, const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard) const;

    EntityTreePointer _entityTree;

    std::recursive_mutex _entitiesScriptEngineLock;
    QSharedPointer<EntitiesScriptEngineProvider> _persistentEntitiesScriptManager;
    QSharedPointer<EntitiesScriptEngineProvider> _nonPersistentEntitiesScriptManager;

    bool _bidOnSimulationOwnership { false };

    ActivityTracking _activityTracking;
};

#endif // hifi_EntityScriptingInterface_h
