//
//  EntityScriptingInterface.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityScriptingInterface.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#include <shared/QtHelpers.h>
#include <VariantMapToScriptValue.h>
#include <SharedUtil.h>
#include <SpatialParentFinder.h>
#include <AvatarHashMap.h>

#include "EntityItemID.h"
#include "EntitiesLogging.h"
#include "EntityDynamicFactoryInterface.h"
#include "EntityDynamicInterface.h"
#include "EntitySimulation.h"
#include "EntityTree.h"
#include "LightEntityItem.h"
#include "ModelEntityItem.h"
#include "QVariantGLM.h"
#include "SimulationOwner.h"
#include "ZoneEntityItem.h"
#include "WebEntityItem.h"
#include <EntityScriptClient.h>
#include <Profile.h>
#include "GrabPropertyGroup.h"

const QString GRABBABLE_USER_DATA = "{\"grabbableKey\":{\"grabbable\":true}}";
const QString NOT_GRABBABLE_USER_DATA = "{\"grabbableKey\":{\"grabbable\":false}}";

EntityScriptingInterface::EntityScriptingInterface(bool bidOnSimulationOwnership) :
    _entityTree(nullptr),
    _bidOnSimulationOwnership(bidOnSimulationOwnership)
{
    auto nodeList = DependencyManager::get<NodeList>();
    connect(nodeList.data(), &NodeList::isAllowedEditorChanged, this, &EntityScriptingInterface::canAdjustLocksChanged);
    connect(nodeList.data(), &NodeList::canRezChanged, this, &EntityScriptingInterface::canRezChanged);
    connect(nodeList.data(), &NodeList::canRezTmpChanged, this, &EntityScriptingInterface::canRezTmpChanged);
    connect(nodeList.data(), &NodeList::canRezCertifiedChanged, this, &EntityScriptingInterface::canRezCertifiedChanged);
    connect(nodeList.data(), &NodeList::canRezTmpCertifiedChanged, this, &EntityScriptingInterface::canRezTmpCertifiedChanged);
    connect(nodeList.data(), &NodeList::canWriteAssetsChanged, this, &EntityScriptingInterface::canWriteAssetsChanged);
    connect(nodeList.data(), &NodeList::canGetAndSetPrivateUserDataChanged, this, &EntityScriptingInterface::canGetAndSetPrivateUserDataChanged);
    connect(nodeList.data(), &NodeList::canRezAvatarEntitiesChanged, this, &EntityScriptingInterface::canRezAvatarEntitiesChanged);

    auto& packetReceiver = nodeList->getPacketReceiver();
    packetReceiver.registerListener(PacketType::EntityScriptCallMethod,
        PacketReceiver::makeSourcedListenerReference<EntityScriptingInterface>(this, &EntityScriptingInterface::handleEntityScriptCallMethodPacket));
}

void EntityScriptingInterface::queueEntityMessage(PacketType packetType,
                                                  EntityItemID entityID, const EntityItemProperties& properties) {
    getEntityPacketSender()->queueEditEntityMessage(packetType, _entityTree, entityID, properties);
}

void EntityScriptingInterface::resetActivityTracking() {
    _activityTracking.addedEntityCount = 0;
    _activityTracking.deletedEntityCount = 0;
    _activityTracking.editedEntityCount = 0;
}

bool EntityScriptingInterface::canAdjustLocks() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->isAllowedEditor();
}

bool EntityScriptingInterface::canRez() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanRez();
}

bool EntityScriptingInterface::canRezTmp() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanRezTmp();
}

bool EntityScriptingInterface::canRezCertified() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanRezCertified();
}

bool EntityScriptingInterface::canRezTmpCertified() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanRezTmpCertified();
}

bool EntityScriptingInterface::canWriteAssets() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanWriteAssets();
}

bool EntityScriptingInterface::canReplaceContent() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanReplaceContent();
}

bool EntityScriptingInterface::canGetAndSetPrivateUserData() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanGetAndSetPrivateUserData();
}

bool EntityScriptingInterface::canRezAvatarEntities() {
    auto nodeList = DependencyManager::get<NodeList>();
    return nodeList->getThisNodeCanRezAvatarEntities();
}

void EntityScriptingInterface::setEntityTree(EntityTreePointer elementTree) {
    if (_entityTree) {
        disconnect(_entityTree.get(), &EntityTree::addingEntityPointer, this, &EntityScriptingInterface::onAddingEntity);
        disconnect(_entityTree.get(), &EntityTree::deletingEntityPointer, this, &EntityScriptingInterface::onDeletingEntity);
        disconnect(_entityTree.get(), &EntityTree::addingEntity, this, &EntityScriptingInterface::addingEntity);
        disconnect(_entityTree.get(), &EntityTree::deletingEntity, this, &EntityScriptingInterface::deletingEntity);
        disconnect(_entityTree.get(), &EntityTree::clearingEntities, this, &EntityScriptingInterface::clearingEntities);
    }

    _entityTree = elementTree;

    if (_entityTree) {
        connect(_entityTree.get(), &EntityTree::addingEntityPointer, this, &EntityScriptingInterface::onAddingEntity, Qt::DirectConnection);
        connect(_entityTree.get(), &EntityTree::deletingEntityPointer, this, &EntityScriptingInterface::onDeletingEntity, Qt::DirectConnection);
        connect(_entityTree.get(), &EntityTree::addingEntity, this, &EntityScriptingInterface::addingEntity);
        connect(_entityTree.get(), &EntityTree::deletingEntity, this, &EntityScriptingInterface::deletingEntity);
        connect(_entityTree.get(), &EntityTree::clearingEntities, this, &EntityScriptingInterface::clearingEntities);
    }
}

EntityItemProperties convertPropertiesToScriptSemantics(const EntityItemProperties& entitySideProperties,
                                                        bool scalesWithParent) {
    // In EntityTree code, properties.position and properties.rotation are relative to the parent.  In javascript,
    // they are in world-space.  The local versions are put into localPosition and localRotation and position and
    // rotation are converted from local to world space.
    EntityItemProperties scriptSideProperties = entitySideProperties;
    scriptSideProperties.setLocalPosition(entitySideProperties.getPosition());
    scriptSideProperties.setLocalRotation(entitySideProperties.getRotation());
    scriptSideProperties.setLocalVelocity(entitySideProperties.getVelocity());
    scriptSideProperties.setLocalAngularVelocity(entitySideProperties.getAngularVelocity());
    scriptSideProperties.setLocalDimensions(entitySideProperties.getDimensions());

    bool success;
    glm::vec3 worldPosition = SpatiallyNestable::localToWorld(entitySideProperties.getPosition(),
                                                              entitySideProperties.getParentID(),
                                                              entitySideProperties.getParentJointIndex(),
                                                              scalesWithParent,
                                                              success);
    glm::quat worldRotation = SpatiallyNestable::localToWorld(entitySideProperties.getRotation(),
                                                              entitySideProperties.getParentID(),
                                                              entitySideProperties.getParentJointIndex(),
                                                              scalesWithParent,
                                                              success);
    glm::vec3 worldVelocity = SpatiallyNestable::localToWorldVelocity(entitySideProperties.getVelocity(),
                                                                      entitySideProperties.getParentID(),
                                                                      entitySideProperties.getParentJointIndex(),
                                                                      scalesWithParent,
                                                                      success);
    glm::vec3 worldAngularVelocity = SpatiallyNestable::localToWorldAngularVelocity(entitySideProperties.getAngularVelocity(),
                                                                                    entitySideProperties.getParentID(),
                                                                                    entitySideProperties.getParentJointIndex(),
                                                                                    scalesWithParent,
                                                                                    success);
    glm::vec3 worldDimensions = SpatiallyNestable::localToWorldDimensions(entitySideProperties.getDimensions(),
                                                                          entitySideProperties.getParentID(),
                                                                          entitySideProperties.getParentJointIndex(),
                                                                          scalesWithParent,
                                                                          success);


    scriptSideProperties.setPosition(worldPosition);
    scriptSideProperties.setRotation(worldRotation);
    scriptSideProperties.setVelocity(worldVelocity);
    scriptSideProperties.setAngularVelocity(worldAngularVelocity);
    scriptSideProperties.setDimensions(worldDimensions);

    return scriptSideProperties;
}


// TODO: this method looks expensive and should take properties by reference, update it, and return void
EntityItemProperties convertPropertiesFromScriptSemantics(const EntityItemProperties& scriptSideProperties,
                                                          bool scalesWithParent) {
    // convert position and rotation properties from world-space to local, unless localPosition and localRotation
    // are set.  If they are set, they overwrite position and rotation.
    EntityItemProperties entitySideProperties = scriptSideProperties;
    bool success;

    if (scriptSideProperties.localPositionChanged()) {
        entitySideProperties.setPosition(scriptSideProperties.getLocalPosition());
    } else if (scriptSideProperties.positionChanged()) {
        glm::vec3 localPosition = SpatiallyNestable::worldToLocal(entitySideProperties.getPosition(),
                                                                  entitySideProperties.getParentID(),
                                                                  entitySideProperties.getParentJointIndex(),
                                                                  scalesWithParent, success);
        entitySideProperties.setPosition(localPosition);
    }

    if (scriptSideProperties.localRotationChanged()) {
        entitySideProperties.setRotation(scriptSideProperties.getLocalRotation());
    } else if (scriptSideProperties.rotationChanged()) {
        glm::quat localRotation = SpatiallyNestable::worldToLocal(entitySideProperties.getRotation(),
                                                                  entitySideProperties.getParentID(),
                                                                  entitySideProperties.getParentJointIndex(),
                                                                  scalesWithParent, success);
        entitySideProperties.setRotation(localRotation);
    }

    if (scriptSideProperties.localVelocityChanged()) {
        entitySideProperties.setVelocity(scriptSideProperties.getLocalVelocity());
    } else if (scriptSideProperties.velocityChanged()) {
        glm::vec3 localVelocity = SpatiallyNestable::worldToLocalVelocity(entitySideProperties.getVelocity(),
                                                                          entitySideProperties.getParentID(),
                                                                          entitySideProperties.getParentJointIndex(),
                                                                          scalesWithParent, success);
        entitySideProperties.setVelocity(localVelocity);
    }

    if (scriptSideProperties.localAngularVelocityChanged()) {
        entitySideProperties.setAngularVelocity(scriptSideProperties.getLocalAngularVelocity());
    } else if (scriptSideProperties.angularVelocityChanged()) {
        glm::vec3 localAngularVelocity =
            SpatiallyNestable::worldToLocalAngularVelocity(entitySideProperties.getAngularVelocity(),
                                                           entitySideProperties.getParentID(),
                                                           entitySideProperties.getParentJointIndex(),
                                                           scalesWithParent, success);
        entitySideProperties.setAngularVelocity(localAngularVelocity);
    }

    if (scriptSideProperties.localDimensionsChanged()) {
        entitySideProperties.setDimensions(scriptSideProperties.getLocalDimensions());
    } else if (scriptSideProperties.dimensionsChanged()) {
        glm::vec3 localDimensions = SpatiallyNestable::worldToLocalDimensions(entitySideProperties.getDimensions(),
                                                                              entitySideProperties.getParentID(),
                                                                              entitySideProperties.getParentJointIndex(),
                                                                              scalesWithParent, success);
        entitySideProperties.setDimensions(localDimensions);
    }

    return entitySideProperties;
}


void synchronizeSpatialKey(const GrabPropertyGroup& grabProperties, QJsonObject& grabbableKey, bool& userDataChanged) {
    if (grabProperties.equippableLeftPositionChanged() ||
        grabProperties.equippableRightPositionChanged() ||
        grabProperties.equippableRightRotationChanged() ||
        grabProperties.equippableIndicatorURLChanged() ||
        grabProperties.equippableIndicatorScaleChanged() ||
        grabProperties.equippableIndicatorOffsetChanged()) {

        QJsonObject spatialKey = grabbableKey["spatialKey"].toObject();

        if (grabProperties.equippableLeftPositionChanged()) {
            if (grabProperties.getEquippableLeftPosition() == INITIAL_LEFT_EQUIPPABLE_POSITION) {
                spatialKey.remove("leftRelativePosition");
            } else {
                spatialKey["leftRelativePosition"] =
                    QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableLeftPosition()));
            }
        }
        if (grabProperties.equippableRightPositionChanged()) {
            if (grabProperties.getEquippableRightPosition() == INITIAL_RIGHT_EQUIPPABLE_POSITION) {
                spatialKey.remove("rightRelativePosition");
            } else {
                spatialKey["rightRelativePosition"] =
                    QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableRightPosition()));
            }
        }
        if (grabProperties.equippableLeftRotationChanged()) {
            spatialKey["relativeRotation"] =
                QJsonValue::fromVariant(quatToQMap(grabProperties.getEquippableLeftRotation()));
        } else if (grabProperties.equippableRightRotationChanged()) {
            spatialKey["relativeRotation"] =
                QJsonValue::fromVariant(quatToQMap(grabProperties.getEquippableRightRotation()));
        }

        grabbableKey["spatialKey"] = spatialKey;
        userDataChanged = true;
    }
}


void synchronizeGrabbableKey(const GrabPropertyGroup& grabProperties, QJsonObject& userData, bool& userDataChanged) {
    if (grabProperties.triggerableChanged() ||
        grabProperties.grabbableChanged() ||
        grabProperties.grabFollowsControllerChanged() ||
        grabProperties.grabKinematicChanged() ||
        grabProperties.equippableChanged() ||
        grabProperties.equippableLeftPositionChanged() ||
        grabProperties.equippableRightPositionChanged() ||
        grabProperties.equippableRightRotationChanged()) {

        QJsonObject grabbableKey = userData["grabbableKey"].toObject();

        if (grabProperties.triggerableChanged()) {
            if (grabProperties.getTriggerable()) {
                grabbableKey["triggerable"] = true;
            } else {
                grabbableKey.remove("triggerable");
            }
        }
        if (grabProperties.grabbableChanged()) {
            if (grabProperties.getGrabbable()) {
                grabbableKey.remove("grabbable");
            } else {
                grabbableKey["grabbable"] = false;
            }
        }
        if (grabProperties.grabFollowsControllerChanged()) {
            if (grabProperties.getGrabFollowsController()) {
                grabbableKey.remove("ignoreIK");
            } else {
                grabbableKey["ignoreIK"] = false;
            }
        }
        if (grabProperties.grabKinematicChanged()) {
            if (grabProperties.getGrabKinematic()) {
                grabbableKey.remove("kinematic");
            } else {
                grabbableKey["kinematic"] = false;
            }
        }
        if (grabProperties.equippableChanged()) {
            if (grabProperties.getEquippable()) {
                grabbableKey["equippable"] = true;
            } else {
                grabbableKey.remove("equippable");
            }
        }

        if (grabbableKey.contains("spatialKey")) {
            synchronizeSpatialKey(grabProperties, grabbableKey, userDataChanged);
        }

        userData["grabbableKey"] = grabbableKey;
        userDataChanged = true;
    }
}

void synchronizeGrabJoints(const GrabPropertyGroup& grabProperties, QJsonObject& joints) {
    QJsonArray rightHand = joints["RightHand"].toArray();
    QJsonObject rightHandPosition = rightHand.size() > 0 ? rightHand[0].toObject() : QJsonObject();
    QJsonObject rightHandRotation = rightHand.size() > 1 ? rightHand[1].toObject() : QJsonObject();
    QJsonArray leftHand = joints["LeftHand"].toArray();
    QJsonObject leftHandPosition = leftHand.size() > 0 ? leftHand[0].toObject() : QJsonObject();
    QJsonObject leftHandRotation = leftHand.size() > 1 ? leftHand[1].toObject() : QJsonObject();

    if (grabProperties.equippableLeftPositionChanged()) {
        leftHandPosition =
            QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableLeftPosition())).toObject();
    }
    if (grabProperties.equippableRightPositionChanged()) {
        rightHandPosition =
            QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableRightPosition())).toObject();
    }
    if (grabProperties.equippableLeftRotationChanged()) {
        leftHandRotation =
            QJsonValue::fromVariant(quatToQMap(grabProperties.getEquippableLeftRotation())).toObject();
    }
    if (grabProperties.equippableRightRotationChanged()) {
        rightHandRotation =
            QJsonValue::fromVariant(quatToQMap(grabProperties.getEquippableRightRotation())).toObject();
    }

    rightHand = QJsonArray();
    rightHand.append(rightHandPosition);
    rightHand.append(rightHandRotation);
    joints["RightHand"] = rightHand;
    leftHand = QJsonArray();
    leftHand.append(leftHandPosition);
    leftHand.append(leftHandRotation);
    joints["LeftHand"] = leftHand;
}

void synchronizeEquipHotspot(const GrabPropertyGroup& grabProperties, QJsonObject& userData, bool& userDataChanged) {
    if (grabProperties.equippableLeftPositionChanged() ||
        grabProperties.equippableRightPositionChanged() ||
        grabProperties.equippableRightRotationChanged() ||
        grabProperties.equippableIndicatorURLChanged() ||
        grabProperties.equippableIndicatorScaleChanged() ||
        grabProperties.equippableIndicatorOffsetChanged()) {

        QJsonArray equipHotspots = userData["equipHotspots"].toArray();
        QJsonObject equipHotspot = equipHotspots[0].toObject();
        QJsonObject joints = equipHotspot["joints"].toObject();

        synchronizeGrabJoints(grabProperties, joints);

        if (grabProperties.equippableIndicatorURLChanged()) {
            equipHotspot["modelURL"] = grabProperties.getEquippableIndicatorURL();
        }
        if (grabProperties.equippableIndicatorScaleChanged()) {
            QJsonObject scale =
                QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableIndicatorScale())).toObject();
            equipHotspot["radius"] = scale;
            equipHotspot["modelScale"] = scale;

        }
        if (grabProperties.equippableIndicatorOffsetChanged()) {
            equipHotspot["position"] =
                QJsonValue::fromVariant(vec3ToQMap(grabProperties.getEquippableIndicatorOffset())).toObject();
        }

        equipHotspot["joints"] = joints;
        equipHotspots = QJsonArray();
        equipHotspots.append(equipHotspot);
        userData["equipHotspots"] = equipHotspots;
        userDataChanged = true;
    }
}

void synchronizeWearable(const GrabPropertyGroup& grabProperties, QJsonObject& userData, bool& userDataChanged) {
    if (grabProperties.equippableLeftPositionChanged() ||
        grabProperties.equippableRightPositionChanged() ||
        grabProperties.equippableRightRotationChanged() ||
        grabProperties.equippableIndicatorURLChanged() ||
        grabProperties.equippableIndicatorScaleChanged() ||
        grabProperties.equippableIndicatorOffsetChanged()) {

        QJsonObject wearable = userData["wearable"].toObject();
        QJsonObject joints = wearable["joints"].toObject();

        synchronizeGrabJoints(grabProperties, joints);

        wearable["joints"] = joints;
        userData["wearable"] = wearable;
        userDataChanged = true;
    }
}

void synchronizeEditedGrabProperties(EntityItemProperties& properties, const QString& previousUserdata) {
    // After sufficient warning to content creators, we should be able to remove this.

    if (properties.grabbingRelatedPropertyChanged()) {
        // This edit touches a new-style grab property, so make userData agree...
        GrabPropertyGroup& grabProperties = properties.getGrab();

        bool userDataChanged { false };

        // if the edit changed userData, use the updated version coming along with the edit.  If not, use
        // what was already in the entity.
        QByteArray userDataString;
        if (properties.userDataChanged()) {
            userDataString = properties.getUserData().toUtf8();
        } else {
            userDataString = previousUserdata.toUtf8();;
        }
        QJsonObject userData = QJsonDocument::fromJson(userDataString).object();

        if (userData.contains("grabbableKey")) {
            synchronizeGrabbableKey(grabProperties, userData, userDataChanged);
        }
        if (userData.contains("equipHotspots")) {
            synchronizeEquipHotspot(grabProperties, userData, userDataChanged);
        }
        if (userData.contains("wearable")) {
            synchronizeWearable(grabProperties, userData, userDataChanged);
        }

        if (userDataChanged) {
            properties.setUserData(QJsonDocument(userData).toJson());
        }

    } else if (properties.userDataChanged()) {
        // This edit touches userData (and doesn't touch a new-style grab property).  Check for grabbableKey in the
        // userdata and make the new-style grab properties agree
        convertGrabUserDataToProperties(properties);
    }
}


QUuid EntityScriptingInterface::addEntityInternal(const EntityItemProperties& properties, entity::HostType entityHostType) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    _activityTracking.addedEntityCount++;

    auto nodeList = DependencyManager::get<NodeList>();

    if (entityHostType == entity::HostType::AVATAR && !nodeList->getThisNodeCanRezAvatarEntities()) {
        qCDebug(entities) << "Ignoring addEntity() because don't have canRezAvatarEntities permission on domain";
        // Only need to intercept methods that may add an avatar entity because avatar entities are removed from the tree when
        // the user doesn't have canRezAvatarEntities permission.
        return QUuid();
    }

    EntityItemProperties propertiesWithSimID = properties;
    propertiesWithSimID.setEntityHostType(entityHostType);
    if (entityHostType == entity::HostType::AVATAR) {
        // only allow adding our own avatar entities from script
        propertiesWithSimID.setOwningAvatarID(AVATAR_SELF_ID);
    } else if (entityHostType == entity::HostType::LOCAL) {
        // For now, local entities are always collisionless
        // TODO: create a separate, local physics simulation that just handles local entities (and MyAvatar?)
        propertiesWithSimID.setCollisionless(true);
    }

    // the created time will be set in EntityTree::addEntity by recordCreationTime()
    auto sessionID = nodeList->getSessionUUID();
    propertiesWithSimID.setLastEditedBy(sessionID);

    bool scalesWithParent = propertiesWithSimID.getScalesWithParent();

    propertiesWithSimID = convertPropertiesFromScriptSemantics(propertiesWithSimID, scalesWithParent);
    propertiesWithSimID.setDimensionsInitialized(properties.dimensionsChanged());
    synchronizeEditedGrabProperties(propertiesWithSimID, QString());

    EntityItemID id;
    // If we have a local entity tree set, then also update it.
    bool success = addLocalEntityCopy(propertiesWithSimID, id);

    // queue the packet
    if (success) {
        queueEntityMessage(PacketType::EntityAdd, id, propertiesWithSimID);
        return id;
    } else {
        return QUuid();
    }
}

bool EntityScriptingInterface::addLocalEntityCopy(EntityItemProperties& properties, EntityItemID& id, bool isClone) {
    bool success = true;
    id = EntityItemID(QUuid::createUuid());

    if (_entityTree) {
        _entityTree->withWriteLock([&] {
            EntityItemPointer entity = _entityTree->addEntity(id, properties, isClone);
            if (entity) {
                if (properties.queryAACubeRelatedPropertyChanged()) {
                    // due to parenting, the server may not know where something is in world-space, so include the bounding cube.
                    bool success;
                    AACube queryAACube = entity->getQueryAACube(success);
                    if (success) {
                        properties.setQueryAACube(queryAACube);
                    }
                }

                entity->setLastBroadcast(usecTimestampNow());
                // since we're creating this object we will immediately volunteer to own its simulation
                entity->upgradeScriptSimulationPriority(VOLUNTEER_SIMULATION_PRIORITY);
                properties.setLastEdited(entity->getLastEdited());
            } else {
                qCDebug(entities) << "script failed to add new Entity to local Octree";
                success = false;
            }
        });
    }

    return success;
}

QUuid EntityScriptingInterface::addModelEntity(const QString& name, const QString& modelUrl, const QString& textures,
                                                const QString& shapeType, bool dynamic, bool collisionless, bool grabbable,
                                                const glm::vec3& position, const glm::vec3& gravity) {
    _activityTracking.addedEntityCount++;

    EntityItemProperties properties;
    properties.setType(EntityTypes::Model);
    properties.setName(name);
    properties.setModelURL(modelUrl);
    properties.setShapeTypeFromString(shapeType);
    properties.setDynamic(dynamic);
    properties.setCollisionless(collisionless);
    properties.setUserData(grabbable ? GRABBABLE_USER_DATA : NOT_GRABBABLE_USER_DATA);
    properties.setPosition(position);
    properties.setGravity(gravity);
    if (!textures.isEmpty()) {
        properties.setTextures(textures);
    }

    auto nodeList = DependencyManager::get<NodeList>();
    auto sessionID = nodeList->getSessionUUID();
    properties.setLastEditedBy(sessionID);

    return addEntity(properties);
}

QUuid EntityScriptingInterface::cloneEntity(const QUuid& entityIDToClone) {
    EntityItemID newEntityID;
    EntityItemProperties properties = getEntityProperties(entityIDToClone);
    bool cloneAvatarEntity = properties.getCloneAvatarEntity();
    properties.convertToCloneProperties(entityIDToClone);

    if (properties.getEntityHostType() == entity::HostType::LOCAL) {
        // Local entities are only cloned locally
        return addEntityInternal(properties, entity::HostType::LOCAL);
    } else if (cloneAvatarEntity) {
        return addEntityInternal(properties, entity::HostType::AVATAR);
    } else {
        // setLastEdited timestamp to 0 to ensure this entity gets updated with the properties 
        // from the server-created entity, don't change this unless you know what you are doing
        properties.setLastEdited(0);
        bool success = addLocalEntityCopy(properties, newEntityID, true);
        if (success) {
            getEntityPacketSender()->queueCloneEntityMessage(entityIDToClone, newEntityID);
            return newEntityID;
        } else {
            return QUuid();
        }
    }
}

EntityItemProperties EntityScriptingInterface::getEntityProperties(const QUuid& entityID) {
    const EntityPropertyFlags noSpecificProperties;
    return getEntityProperties(entityID, noSpecificProperties);
}

EntityItemProperties EntityScriptingInterface::getEntityProperties(const QUuid& entityID, EntityPropertyFlags desiredProperties) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    bool scalesWithParent { false };
    EntityItemProperties results;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
            if (entity) {
                scalesWithParent = entity->getScalesWithParent();
                if (desiredProperties.getHasProperty(PROP_POSITION) ||
                    desiredProperties.getHasProperty(PROP_ROTATION) ||
                    desiredProperties.getHasProperty(PROP_LOCAL_POSITION) ||
                    desiredProperties.getHasProperty(PROP_LOCAL_ROTATION) ||
                    desiredProperties.getHasProperty(PROP_LOCAL_VELOCITY) ||
                    desiredProperties.getHasProperty(PROP_LOCAL_ANGULAR_VELOCITY) ||
                    desiredProperties.getHasProperty(PROP_LOCAL_DIMENSIONS)) {
                    // if we are explicitly getting position or rotation, we need parent information to make sense of them.
                    desiredProperties.setHasProperty(PROP_PARENT_ID);
                    desiredProperties.setHasProperty(PROP_PARENT_JOINT_INDEX);
                }

                if (desiredProperties.isEmpty()) {
                    // these are left out of EntityItem::getEntityProperties so that localPosition and localRotation
                    // don't end up in json saves, etc.  We still want them here, though.
                    EncodeBitstreamParams params; // unknown
                    desiredProperties = entity->getEntityProperties(params);
                    desiredProperties.setHasProperty(PROP_LOCAL_POSITION);
                    desiredProperties.setHasProperty(PROP_LOCAL_ROTATION);
                    desiredProperties.setHasProperty(PROP_LOCAL_VELOCITY);
                    desiredProperties.setHasProperty(PROP_LOCAL_ANGULAR_VELOCITY);
                    desiredProperties.setHasProperty(PROP_LOCAL_DIMENSIONS);
                }

                results = entity->getProperties(desiredProperties);
            }
        });
    }

    return convertPropertiesToScriptSemantics(results, scalesWithParent);
}


struct EntityPropertiesResult {
    EntityPropertiesResult(const EntityItemProperties& properties, bool scalesWithParent) :
        properties(properties),
        scalesWithParent(scalesWithParent) {
    }
    EntityPropertiesResult() = default;
    EntityItemProperties properties;
    bool scalesWithParent{ false };
};

// Static method to make sure that we have the right script engine.
// Using sender() or QtScriptable::engine() does not work for classes used by multiple threads (script-engines)
QScriptValue EntityScriptingInterface::getMultipleEntityProperties(QScriptContext* context, QScriptEngine* engine) {
    const int ARGUMENT_ENTITY_IDS = 0;
    const int ARGUMENT_EXTENDED_DESIRED_PROPERTIES = 1;

    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    const auto entityIDs = qscriptvalue_cast<QVector<QUuid>>(context->argument(ARGUMENT_ENTITY_IDS));
    return entityScriptingInterface->getMultipleEntityPropertiesInternal(engine, entityIDs, context->argument(ARGUMENT_EXTENDED_DESIRED_PROPERTIES));
}

QScriptValue EntityScriptingInterface::getMultipleEntityPropertiesInternal(QScriptEngine* engine, QVector<QUuid> entityIDs, const QScriptValue& extendedDesiredProperties) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    EntityPsuedoPropertyFlags psuedoPropertyFlags;
    const auto readExtendedPropertyStringValue = [&](QScriptValue extendedProperty) {
        const auto extendedPropertyString = extendedProperty.toString();
        if (extendedPropertyString == "id") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::ID);
        } else if (extendedPropertyString == "type") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::Type);
        } else if (extendedPropertyString == "age") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::Age);
        } else if (extendedPropertyString == "ageAsText") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::AgeAsText);
        } else if (extendedPropertyString == "lastEdited") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::LastEdited);
        } else if (extendedPropertyString == "boundingBox") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::BoundingBox);
        } else if (extendedPropertyString == "originalTextures") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::OriginalTextures);
        } else if (extendedPropertyString == "renderInfo") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::RenderInfo);
        } else if (extendedPropertyString == "clientOnly") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::ClientOnly);
        } else if (extendedPropertyString == "avatarEntity") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::AvatarEntity);
        } else if (extendedPropertyString == "localEntity") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::LocalEntity);
        } else if (extendedPropertyString == "faceCamera") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::FaceCamera);
        } else if (extendedPropertyString == "isFacingAvatar") {
            psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::IsFacingAvatar);
        }
    };

    if (extendedDesiredProperties.isString()) {
        readExtendedPropertyStringValue(extendedDesiredProperties);
        psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::FlagsActive);
    } else if (extendedDesiredProperties.isArray()) {
        const quint32 length = extendedDesiredProperties.property("length").toInt32();
        for (quint32 i = 0; i < length; i++) {
            readExtendedPropertyStringValue(extendedDesiredProperties.property(i));
        }
        psuedoPropertyFlags.set(EntityPsuedoPropertyFlag::FlagsActive);
    }

    EntityPropertyFlags desiredProperties = qscriptvalue_cast<EntityPropertyFlags>(extendedDesiredProperties);
    bool needsScriptSemantics = desiredProperties.getHasProperty(PROP_POSITION) ||
        desiredProperties.getHasProperty(PROP_ROTATION) ||
        desiredProperties.getHasProperty(PROP_LOCAL_POSITION) ||
        desiredProperties.getHasProperty(PROP_LOCAL_ROTATION) ||
        desiredProperties.getHasProperty(PROP_LOCAL_VELOCITY) ||
        desiredProperties.getHasProperty(PROP_LOCAL_ANGULAR_VELOCITY) ||
        desiredProperties.getHasProperty(PROP_LOCAL_DIMENSIONS);
    if (needsScriptSemantics) {
        // if we are explicitly getting position or rotation, we need parent information to make sense of them.
        desiredProperties.setHasProperty(PROP_PARENT_ID);
        desiredProperties.setHasProperty(PROP_PARENT_JOINT_INDEX);
    }
    QVector<EntityPropertiesResult> resultProperties;
    if (_entityTree) {
        PROFILE_RANGE(script_entities, "EntityScriptingInterface::getMultipleEntityProperties>Obtaining Properties");
        int i = 0;
        const int lockAmount = 500;
        int size = entityIDs.size();
        while (i < size) {
            _entityTree->withReadLock([&] {
                for (int j = 0; j < lockAmount && i < size; ++i, ++j) {
                    const auto& entityID = entityIDs.at(i);
                    const EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
                    if (entity) {
                        if (psuedoPropertyFlags.none() && desiredProperties.isEmpty()) {
                            // these are left out of EntityItem::getEntityProperties so that localPosition and localRotation
                            // don't end up in json saves, etc.  We still want them here, though.
                            EncodeBitstreamParams params; // unknown
                            desiredProperties = entity->getEntityProperties(params);
                            desiredProperties.setHasProperty(PROP_LOCAL_POSITION);
                            desiredProperties.setHasProperty(PROP_LOCAL_ROTATION);
                            desiredProperties.setHasProperty(PROP_LOCAL_VELOCITY);
                            desiredProperties.setHasProperty(PROP_LOCAL_ANGULAR_VELOCITY);
                            desiredProperties.setHasProperty(PROP_LOCAL_DIMENSIONS);
                            psuedoPropertyFlags.set();
                            needsScriptSemantics = true;
                        }

                        auto properties = entity->getProperties(desiredProperties, true);
                        EntityPropertiesResult result(properties, entity->getScalesWithParent());
                        resultProperties.append(result);
                    }
                }
            });
        }
    }
    QScriptValue finalResult = engine->newArray(resultProperties.size());
    quint32 i = 0;
    if (needsScriptSemantics) {
        PROFILE_RANGE(script_entities, "EntityScriptingInterface::getMultipleEntityProperties>Script Semantics");
        foreach(const auto& result, resultProperties) {
            finalResult.setProperty(i++, convertPropertiesToScriptSemantics(result.properties, result.scalesWithParent)
                .copyToScriptValue(engine, false, false, false, psuedoPropertyFlags));
        }
    } else {
        PROFILE_RANGE(script_entities, "EntityScriptingInterface::getMultipleEntityProperties>Skip Script Semantics");
        foreach(const auto& result, resultProperties) {
            finalResult.setProperty(i++, result.properties.copyToScriptValue(engine, false, false, false, psuedoPropertyFlags));
        }
    }
    return finalResult;
}

QUuid EntityScriptingInterface::editEntity(const QUuid& id, const EntityItemProperties& scriptSideProperties) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    _activityTracking.editedEntityCount++;

    const auto sessionID = DependencyManager::get<NodeList>()->getSessionUUID();

    EntityItemProperties properties = scriptSideProperties;

    EntityItemID entityID(id);
    if (!_entityTree) {
        properties.setLastEditedBy(sessionID);
        queueEntityMessage(PacketType::EntityEdit, entityID, properties);
        return id;
    }

    EntityItemPointer entity(nullptr);
    SimulationOwner simulationOwner;
    _entityTree->withReadLock([&] {
        // make a copy of entity for local logic outside of tree lock
        entity = _entityTree->findEntityByEntityItemID(entityID);
        if (!entity) {
            return;
        }

        if (entity->isAvatarEntity() && !entity->isMyAvatarEntity()) {
            // don't edit other avatar's avatarEntities
            properties = EntityItemProperties();
            return;
        }
        // make a copy of simulationOwner for local logic outside of tree lock
        simulationOwner = entity->getSimulationOwner();
    });

    QString previousUserdata;
    if (entity) {
        if (properties.hasTransformOrVelocityChanges() && entity->hasGrabs()) {
            // if an entity is grabbed, the grab will override any position changes
            properties.clearTransformOrVelocityChanges();
        }
        if (properties.hasSimulationRestrictedChanges()) {
            if (_bidOnSimulationOwnership) {
                // flag for simulation ownership, or upgrade existing ownership priority
                // (actual bids for simulation ownership are sent by the PhysicalEntitySimulation)
                entity->upgradeScriptSimulationPriority(properties.computeSimulationBidPriority());
                if (entity->isLocalEntity() || entity->isMyAvatarEntity() || simulationOwner.getID() == sessionID) {
                    // we own the simulation --> copy ALL restricted properties
                    properties.copySimulationRestrictedProperties(entity);
                } else {
                    // we don't own the simulation but think we would like to

                    uint8_t desiredPriority = entity->getScriptSimulationPriority();
                    if (desiredPriority < simulationOwner.getPriority()) {
                        // the priority at which we'd like to own it is not high enough
                        // --> assume failure and clear all restricted property changes
                        properties.clearSimulationRestrictedProperties();
                    } else {
                        // the priority at which we'd like to own it is high enough to win.
                        // --> assume success and copy ALL restricted properties
                        properties.copySimulationRestrictedProperties(entity);
                    }
                }
            } else if (!simulationOwner.getID().isNull()) {
                // someone owns this but not us
                // clear restricted properties
                properties.clearSimulationRestrictedProperties();
            }
            // clear the cached simulationPriority level
            entity->upgradeScriptSimulationPriority(0);
        }

        // set these to make EntityItemProperties::getScalesWithParent() work correctly
        entity::HostType entityHostType = entity->getEntityHostType();
        properties.setEntityHostType(entityHostType);
        if (entityHostType == entity::HostType::LOCAL) {
            properties.setCollisionless(true);
        }
        properties.setOwningAvatarID(entity->getOwningAvatarID());

        // make sure the properties has a type, so that the encode can know which properties to include
        properties.setType(entity->getType());

        previousUserdata = entity->getUserData();
    } else if (_bidOnSimulationOwnership) {
        // bail when simulation participants don't know about entity
        return QUuid();
    }
    // TODO: it is possible there is no remaining useful changes in properties and we should bail early.
    // How to check for this cheaply?

    properties = convertPropertiesFromScriptSemantics(properties, properties.getScalesWithParent());
    synchronizeEditedGrabProperties(properties, previousUserdata);
    properties.setLastEditedBy(sessionID);

    // done reading and modifying properties --> start write
    bool updatedEntity = false;
    _entityTree->withWriteLock([&] {
        updatedEntity = _entityTree->updateEntity(entityID, properties);
    });

    // FIXME: We need to figure out a better way to handle this. Allowing these edits to go through potentially
    // breaks entities that are parented.
    //
    // To handle cases where a script needs to edit an entity with a _known_ entity id but doesn't exist
    // in the local entity tree, we need to allow those edits to go through to the server.
    // if (!updatedEntity) {
    //     return QUuid();
    // }

    bool hasQueryAACubeRelatedChanges = properties.queryAACubeRelatedPropertyChanged();
    // done writing, send update
    _entityTree->withReadLock([&] {
        // find the entity again: maybe it was removed since we last found it
        entity = _entityTree->findEntityByEntityItemID(entityID);
        if (entity) {
            uint64_t now = usecTimestampNow();
            entity->setLastBroadcast(now);

            if (hasQueryAACubeRelatedChanges) {
                properties.setQueryAACube(entity->getQueryAACube());

                // if we've moved an entity with children, check/update the queryAACube of all descendents and tell the server
                // if they've changed.
                entity->forEachDescendant([&](SpatiallyNestablePointer descendant) {
                    if (descendant->getNestableType() == NestableType::Entity) {
                        if (descendant->updateQueryAACube()) {
                            EntityItemPointer entityDescendant = std::static_pointer_cast<EntityItem>(descendant);
                            EntityItemProperties newQueryCubeProperties;
                            newQueryCubeProperties.setQueryAACube(descendant->getQueryAACube());
                            newQueryCubeProperties.setLastEdited(properties.getLastEdited());
                            queueEntityMessage(PacketType::EntityEdit, descendant->getID(), newQueryCubeProperties);
                            entityDescendant->setLastBroadcast(now);
                        }
                    }
                });
            }
        }
    });
    if (!entity) {
        if (hasQueryAACubeRelatedChanges) {
            // Sometimes ESS don't have the entity they are trying to edit in their local tree.  In this case,
            // convertPropertiesFromScriptSemantics doesn't get called and local* edits will get dropped.
            // This is because, on the script side, "position" is in world frame, but in the network
            // protocol and in the internal data-structures, "position" is "relative to parent".
            // Compensate here.  The local* versions will get ignored during the edit-packet encoding.
            if (properties.localPositionChanged()) {
                properties.setPosition(properties.getLocalPosition());
            }
            if (properties.localRotationChanged()) {
                properties.setRotation(properties.getLocalRotation());
            }
            if (properties.localVelocityChanged()) {
                properties.setVelocity(properties.getLocalVelocity());
            }
            if (properties.localAngularVelocityChanged()) {
                properties.setAngularVelocity(properties.getLocalAngularVelocity());
            }
            if (properties.localDimensionsChanged()) {
                properties.setDimensions(properties.getLocalDimensions());
            }
        }
        // we've made an edit to an entity we don't know about, or to a non-entity.  If it's a known non-entity,
        // print a warning and don't send an edit packet to the entity-server.
        QSharedPointer<SpatialParentFinder> parentFinder = DependencyManager::get<SpatialParentFinder>();
        if (parentFinder) {
            bool success;
            auto nestableWP = parentFinder->find(id, success, static_cast<SpatialParentTree*>(_entityTree.get()));
            if (success) {
                auto nestable = nestableWP.lock();
                if (nestable) {
                    NestableType nestableType = nestable->getNestableType();
                    if (nestableType == NestableType::Avatar) {
                        qCWarning(entities) << "attempted edit on non-entity: " << id << nestable->getName();
                        return QUuid(); // null script value to indicate failure
                    }
                }
            }
        }
    }
    // we queue edit packets even if we don't know about the entity.  This is to allow AC agents
    // to edit entities they know only by ID.
    queueEntityMessage(PacketType::EntityEdit, entityID, properties);
    return id;
}

void EntityScriptingInterface::deleteEntity(const QUuid& id) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    _activityTracking.deletedEntityCount++;

    if (!_entityTree) {
        return;
    }

    EntityItemID entityID(id);

    // If we have a local entity tree set, then also update it.
    std::vector<EntityItemPointer> entitiesToDeleteImmediately;
    _entityTree->withWriteLock([&] {
        EntityItemPointer entity = _entityTree->findEntityByEntityItemID(entityID);
        if (entity) {
            if (entity->isAvatarEntity() && !entity->isMyAvatarEntity()) {
                // don't delete other avatar's avatarEntities
                return;
            }
            if (entity->getLocked()) {
                return;
            }

            // Deleting an entity has consequences for linked children: some can be deleted but others can't.
            // Local- and my-avatar-entities can be deleted immediately, but other-avatar-entities can't be deleted
            // by this context, and a domain-entity must round trip through the entity-server for authorization.
            if (entity->isDomainEntity() && !_entityTree->isServerlessMode()) {
                getEntityPacketSender()->queueEraseEntityMessage(id);
            } else {
                entitiesToDeleteImmediately.push_back(entity);
                const auto sessionID = DependencyManager::get<NodeList>()->getSessionUUID();
                entity->collectChildrenForDelete(entitiesToDeleteImmediately, sessionID);
                _entityTree->deleteEntitiesByPointer(entitiesToDeleteImmediately);
            }
        }
    });

    for (auto entity : entitiesToDeleteImmediately) {
        if (entity->isMyAvatarEntity()) {
            getEntityPacketSender()->getMyAvatar()->clearAvatarEntityInternal(entity->getID());
        }
    }
}

QString EntityScriptingInterface::getEntityType(const QUuid& entityID) {
    QString toReturn;
    _entityTree->withReadLock([&] {
        EntityItemPointer entity = _entityTree->findEntityByEntityItemID(entityID);
        if (entity) {
            toReturn = EntityTypes::getEntityTypeName(entity->getType());
        }
    });
    return toReturn;
}

QObject* EntityScriptingInterface::getEntityObject(const QUuid& id) {
    return EntityTree::getEntityObject(id);
}

bool EntityScriptingInterface::isLoaded(const QUuid& id) {
    bool toReturn = false;
    _entityTree->withReadLock([&] {
        EntityItemPointer entity = _entityTree->findEntityByEntityItemID(id);
        if (entity) {
            toReturn = entity->isVisuallyReady();
        }
    });
    return toReturn;
}

bool EntityScriptingInterface::isAddedEntity(const QUuid& id) {
    bool toReturn = false;
    _entityTree->withReadLock([&] {
        EntityItemPointer entity = _entityTree->findEntityByEntityItemID(id);
        toReturn = (bool)entity;
    });
    return toReturn;
}

QSizeF EntityScriptingInterface::textSize(const QUuid& id, const QString& text) {
    return EntityTree::textSize(id, text);
}

void EntityScriptingInterface::setPersistentEntitiesScriptEngine(QSharedPointer<EntitiesScriptEngineProvider> engine) {
    std::lock_guard<std::recursive_mutex> lock(_entitiesScriptEngineLock);
    _persistentEntitiesScriptEngine = engine;
}

void EntityScriptingInterface::setNonPersistentEntitiesScriptEngine(QSharedPointer<EntitiesScriptEngineProvider> engine) {
    std::lock_guard<std::recursive_mutex> lock(_entitiesScriptEngineLock);
    _nonPersistentEntitiesScriptEngine = engine;
}

void EntityScriptingInterface::callEntityMethod(const QUuid& id, const QString& method, const QStringList& params) {
    PROFILE_RANGE(script_entities, __FUNCTION__);
    
    auto entity = getEntityTree()->findEntityByEntityItemID(id);
    if (entity) {
        std::lock_guard<std::recursive_mutex> lock(_entitiesScriptEngineLock);
        auto& scriptEngine = (entity->isLocalEntity() || entity->isMyAvatarEntity()) ? _persistentEntitiesScriptEngine : _nonPersistentEntitiesScriptEngine;
        if (scriptEngine) {
            scriptEngine->callEntityScriptMethod(id, method, params);
        }
    }
}

void EntityScriptingInterface::callEntityServerMethod(const QUuid& id, const QString& method, const QStringList& params) {
    PROFILE_RANGE(script_entities, __FUNCTION__);
    DependencyManager::get<EntityScriptClient>()->callEntityServerMethod(id, method, params);
}

void EntityScriptingInterface::callEntityClientMethod(const QUuid& clientSessionID, const QUuid& entityID, const QString& method, const QStringList& params) {
    PROFILE_RANGE(script_entities, __FUNCTION__);
    auto scriptServerServices = DependencyManager::get<EntityScriptServerServices>();

    // this won't be available on clients
    if (scriptServerServices) {
        scriptServerServices->callEntityClientMethod(clientSessionID, entityID, method, params);
    } else {
        qWarning() << "Entities.callEntityClientMethod() not allowed in client";
    }
}


void EntityScriptingInterface::handleEntityScriptCallMethodPacket(QSharedPointer<ReceivedMessage> receivedMessage, SharedNodePointer senderNode) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    auto nodeList = DependencyManager::get<NodeList>();
    SharedNodePointer entityScriptServer = nodeList->soloNodeOfType(NodeType::EntityScriptServer);

    if (entityScriptServer == senderNode) {
        auto entityID = QUuid::fromRfc4122(receivedMessage->read(NUM_BYTES_RFC4122_UUID));

        auto method = receivedMessage->readString();

        quint16 paramCount;
        receivedMessage->readPrimitive(&paramCount);

        QStringList params;
        for (int param = 0; param < paramCount; param++) {
            auto paramString = receivedMessage->readString();
            params << paramString;
        }

        auto entity = getEntityTree()->findEntityByEntityItemID(entityID);
        if (entity) {
            std::lock_guard<std::recursive_mutex> lock(_entitiesScriptEngineLock);
            auto& scriptEngine = (entity->isLocalEntity() || entity->isMyAvatarEntity()) ? _persistentEntitiesScriptEngine : _nonPersistentEntitiesScriptEngine;
            if (scriptEngine) {
                scriptEngine->callEntityScriptMethod(entityID, method, params, senderNode->getUUID());
            }
        }
    }
}

void EntityScriptingInterface::onAddingEntity(EntityItem* entity) {
    if (entity->isWearable()) {
        QMetaObject::invokeMethod(this, "addingWearable", Q_ARG(EntityItemID, entity->getEntityItemID()));
    }
}

void EntityScriptingInterface::onDeletingEntity(EntityItem* entity) {
    if (entity->isWearable()) {
        QMetaObject::invokeMethod(this, "deletingWearable", Q_ARG(EntityItemID, entity->getEntityItemID()));
    }
}

QUuid EntityScriptingInterface::findClosestEntity(const glm::vec3& center, float radius) const {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    EntityItemID result;
    if (_entityTree) {
        unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
        _entityTree->withReadLock([&] {
            result = _entityTree->evalClosestEntity(center, radius, PickFilter(searchFilter));
        });
    }
    return result;
}


void EntityScriptingInterface::dumpTree() const {
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            _entityTree->dumpTree();
        });
    }
}

QVector<QUuid> EntityScriptingInterface::findEntities(const glm::vec3& center, float radius) const {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QVector<QUuid> result;
    if (_entityTree) {
        unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
        _entityTree->withReadLock([&] {
            _entityTree->evalEntitiesInSphere(center, radius, PickFilter(searchFilter), result);
        });
    }
    return result;
}

QVector<QUuid> EntityScriptingInterface::findEntitiesInBox(const glm::vec3& corner, const glm::vec3& dimensions) const {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QVector<QUuid> result;
    if (_entityTree) {
        unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
        _entityTree->withReadLock([&] {
            AABox box(corner, dimensions);
            _entityTree->evalEntitiesInBox(box, PickFilter(searchFilter), result);
        });
    }
    return result;
}

QVector<QUuid> EntityScriptingInterface::findEntitiesInFrustum(QVariantMap frustum) const {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QVector<QUuid> result;

    const QString POSITION_PROPERTY = "position";
    bool positionOK = frustum.contains(POSITION_PROPERTY);
    glm::vec3 position = positionOK ? qMapToVec3(frustum[POSITION_PROPERTY]) : glm::vec3();

    const QString ORIENTATION_PROPERTY = "orientation";
    bool orientationOK = frustum.contains(ORIENTATION_PROPERTY);
    glm::quat orientation = orientationOK ? qMapToQuat(frustum[ORIENTATION_PROPERTY]) : glm::quat();

    const QString PROJECTION_PROPERTY = "projection";
    bool projectionOK = frustum.contains(PROJECTION_PROPERTY);
    glm::mat4 projection = projectionOK ? qMapToMat4(frustum[PROJECTION_PROPERTY]) : glm::mat4();

    const QString CENTER_RADIUS_PROPERTY = "centerRadius";
    bool centerRadiusOK = frustum.contains(CENTER_RADIUS_PROPERTY);
    float centerRadius = centerRadiusOK ? frustum[CENTER_RADIUS_PROPERTY].toFloat() : 0.0f;

    if (positionOK && orientationOK && projectionOK && centerRadiusOK) {
        ViewFrustum viewFrustum;
        viewFrustum.setPosition(position);
        viewFrustum.setOrientation(orientation);
        viewFrustum.setProjection(projection);
        viewFrustum.setCenterRadius(centerRadius);
        viewFrustum.calculate();

        if (_entityTree) {
            unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
            _entityTree->withReadLock([&] {
                _entityTree->evalEntitiesInFrustum(viewFrustum, PickFilter(searchFilter), result);
            });
        }
    }

    return result;
}

QVector<QUuid> EntityScriptingInterface::findEntitiesByType(const QString entityType, const glm::vec3& center, float radius) const {
    EntityTypes::EntityType type = EntityTypes::getEntityTypeFromName(entityType);

    QVector<QUuid> result;
    if (_entityTree) {
        unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
        _entityTree->withReadLock([&] {
            _entityTree->evalEntitiesInSphereWithType(center, radius, type, PickFilter(searchFilter), result);
        });
    }
    return result;
}

QVector<QUuid> EntityScriptingInterface::findEntitiesByName(const QString entityName, const glm::vec3& center, float radius, bool caseSensitiveSearch) const {
    QVector<QUuid> result;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);
            _entityTree->evalEntitiesInSphereWithName(center, radius, entityName, caseSensitiveSearch, PickFilter(searchFilter), result);
        });
    }
    return result;
}

RayToEntityIntersectionResult EntityScriptingInterface::findRayIntersection(const PickRay& ray, bool precisionPicking,
        const QScriptValue& entityIdsToInclude, const QScriptValue& entityIdsToDiscard, bool visibleOnly, bool collidableOnly) const {
    PROFILE_RANGE(script_entities, __FUNCTION__);
    QVector<EntityItemID> entitiesToInclude = qVectorEntityItemIDFromScriptValue(entityIdsToInclude);
    QVector<EntityItemID> entitiesToDiscard = qVectorEntityItemIDFromScriptValue(entityIdsToDiscard);

    unsigned int searchFilter = PickFilter::getBitMask(PickFilter::FlagBit::DOMAIN_ENTITIES) | PickFilter::getBitMask(PickFilter::FlagBit::AVATAR_ENTITIES);

    if (!precisionPicking) {
        searchFilter = searchFilter | PickFilter::getBitMask(PickFilter::FlagBit::COARSE);
    }

    if (visibleOnly) {
        searchFilter = searchFilter | PickFilter::getBitMask(PickFilter::FlagBit::VISIBLE);
    }

    if (collidableOnly) {
        searchFilter = searchFilter | PickFilter::getBitMask(PickFilter::FlagBit::COLLIDABLE);
    }

    return evalRayIntersectionWorker(ray, Octree::Lock, PickFilter(searchFilter), entitiesToInclude, entitiesToDiscard);
}

RayToEntityIntersectionResult EntityScriptingInterface::evalRayIntersectionVector(const PickRay& ray, PickFilter searchFilter,
        const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return evalRayIntersectionWorker(ray, Octree::Lock, searchFilter, entityIdsToInclude, entityIdsToDiscard);
}

RayToEntityIntersectionResult EntityScriptingInterface::evalRayIntersectionWorker(const PickRay& ray,
        Octree::lockType lockType, PickFilter searchFilter, const QVector<EntityItemID>& entityIdsToInclude,
        const QVector<EntityItemID>& entityIdsToDiscard) const {

    RayToEntityIntersectionResult result;
    if (_entityTree) {
        OctreeElementPointer element;
        result.entityID = _entityTree->evalRayIntersection(ray.origin, ray.direction,
            entityIdsToInclude, entityIdsToDiscard, searchFilter,
            element, result.distance, result.face, result.surfaceNormal,
            result.extraInfo, lockType, &result.accurate);
        result.intersects = !result.entityID.isNull();
        if (result.intersects) {
            result.intersection = ray.origin + (ray.direction * result.distance);
        }
    }
    return result;
}

ParabolaToEntityIntersectionResult EntityScriptingInterface::evalParabolaIntersectionVector(const PickParabola& parabola, PickFilter searchFilter,
        const QVector<EntityItemID>& entityIdsToInclude, const QVector<EntityItemID>& entityIdsToDiscard) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return evalParabolaIntersectionWorker(parabola, Octree::Lock, searchFilter, entityIdsToInclude, entityIdsToDiscard);
}

ParabolaToEntityIntersectionResult EntityScriptingInterface::evalParabolaIntersectionWorker(const PickParabola& parabola,
    Octree::lockType lockType, PickFilter searchFilter, const QVector<EntityItemID>& entityIdsToInclude,
    const QVector<EntityItemID>& entityIdsToDiscard) const {

    ParabolaToEntityIntersectionResult result;
    if (_entityTree) {
        OctreeElementPointer element;
        result.entityID = _entityTree->evalParabolaIntersection(parabola,
            entityIdsToInclude, entityIdsToDiscard, searchFilter,
            element, result.intersection, result.distance, result.parabolicDistance, result.face, result.surfaceNormal,
            result.extraInfo, lockType, &result.accurate);
        result.intersects = !result.entityID.isNull();
    }
    return result;
}

void EntityScriptingInterface::reloadModelURL(const QUuid& entityID) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        modelEntity->forceReloadModelURL();
    }
}

bool EntityScriptingInterface::reloadServerScripts(const QUuid& entityID) {
    auto client = DependencyManager::get<EntityScriptClient>();
    return client->reloadServerScript(entityID);
}

bool EntityPropertyMetadataRequest::script(EntityItemID entityID, QScriptValue handler) {
    using LocalScriptStatusRequest = QFutureWatcher<QVariant>;

    LocalScriptStatusRequest* request = new LocalScriptStatusRequest;
    QObject::connect(request, &LocalScriptStatusRequest::finished, _engine, [=]() mutable {
        auto details = request->result().toMap();
        QScriptValue err, result;
        if (details.contains("isError")) {
            if (!details.contains("message")) {
                details["message"] = details["errorInfo"];
            }
            err = _engine->makeError(_engine->toScriptValue(details));
        } else {
            details["success"] = true;
            result = _engine->toScriptValue(details);
        }
        callScopedHandlerObject(handler, err, result);
        request->deleteLater();
    });
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    entityScriptingInterface->withEntitiesScriptEngine([&](QSharedPointer<EntitiesScriptEngineProvider> entitiesScriptEngine) {
        if (entitiesScriptEngine) {
            request->setFuture(entitiesScriptEngine->getLocalEntityScriptDetails(entityID));
        }
    }, entityID);
    if (!request->isStarted()) {
        request->deleteLater();
        callScopedHandlerObject(handler, _engine->makeError("Entities Scripting Provider unavailable", "InternalError"), QScriptValue());
        return false;
    }
    return true;
}

bool EntityPropertyMetadataRequest::serverScripts(EntityItemID entityID, QScriptValue handler) {
    auto client = DependencyManager::get<EntityScriptClient>();
    auto request = client->createScriptStatusRequest(entityID);
    QPointer<BaseScriptEngine> engine = _engine;
    QObject::connect(request, &GetScriptStatusRequest::finished, _engine, [=](GetScriptStatusRequest* request) mutable {
        auto engine = _engine;
        if (!engine) {
            qCDebug(entities) << __FUNCTION__ << " -- engine destroyed while inflight" << entityID;
            return;
        }
        QVariantMap details;
        details["success"] = request->getResponseReceived();
        details["isRunning"] = request->getIsRunning();
        details["status"] = EntityScriptStatus_::valueToKey(request->getStatus()).toLower();
        details["errorInfo"] = request->getErrorInfo();

        QScriptValue err, result;
        if (!details["success"].toBool()) {
            if (!details.contains("message") && details.contains("errorInfo")) {
                details["message"] = details["errorInfo"];
            }
            if (details["message"].toString().isEmpty()) {
                details["message"] = "entity server script details not found";
            }
            err = engine->makeError(engine->toScriptValue(details));
        } else {
            result = engine->toScriptValue(details);
        }
        callScopedHandlerObject(handler, err, result);
        request->deleteLater();
    });
    request->start();
    return true;
}

bool EntityScriptingInterface::queryPropertyMetadata(const QUuid& entityID, QScriptValue property, QScriptValue scopeOrCallback, QScriptValue methodOrName) {
    auto name = property.toString();
    auto handler = makeScopedHandlerObject(scopeOrCallback, methodOrName);
    QPointer<BaseScriptEngine> engine = dynamic_cast<BaseScriptEngine*>(handler.engine());
    if (!engine) {
        qCDebug(entities) << "queryPropertyMetadata without detectable engine" << entityID << name;
        return false;
    }
#ifdef DEBUG_ENGINE_STATE
    connect(engine, &QObject::destroyed, this, [=]() {
        qDebug() << "queryPropertyMetadata -- engine destroyed!" << (!engine ? "nullptr" : "engine");
    });
#endif
    if (!handler.property("callback").isFunction()) {
        qDebug() << "!handler.callback.isFunction" << engine;
        engine->raiseException(engine->makeError("callback is not a function", "TypeError"));
        return false;
    }

    // NOTE: this approach is a work-in-progress and for now just meant to work 100% correctly and provide
    // some initial structure for organizing metadata adapters around.

    // The extra layer of indirection is *essential* because in real world conditions errors are often introduced
    // by accident and sometimes without exact memory of "what just changed."

    // Here the scripter only needs to know an entityID and a property name -- which means all scripters can
    // level this method when stuck in dead-end scenarios or to learn more about "magic" Entity properties
    // like .script that work in terms of side-effects.

    // This is an async callback pattern -- so if needed C++ can easily throttle or restrict queries later.

    EntityPropertyMetadataRequest request(engine);

    if (name == "script") {
        return request.script(entityID, handler);
    } else if (name == "serverScripts") {
        return request.serverScripts(entityID, handler);
    } else {
        engine->raiseException(engine->makeError("metadata for property " + name + " is not yet queryable"));
        engine->maybeEmitUncaughtException(__FUNCTION__);
        return false;
    }
}

bool EntityScriptingInterface::getServerScriptStatus(const QUuid& entityID, QScriptValue callback) {
    auto client = DependencyManager::get<EntityScriptClient>();
    auto request = client->createScriptStatusRequest(entityID);
    connect(request, &GetScriptStatusRequest::finished, callback.engine(), [callback](GetScriptStatusRequest* request) mutable {
        QString statusString = EntityScriptStatus_::valueToKey(request->getStatus());;
        QScriptValueList args { request->getResponseReceived(), request->getIsRunning(), statusString.toLower(), request->getErrorInfo() };
        callback.call(QScriptValue(), args);
        request->deleteLater();
    });
    request->start();
    return true;
}

void EntityScriptingInterface::setLightsArePickable(bool value) {
    LightEntityItem::setLightsArePickable(value);
}

bool EntityScriptingInterface::getLightsArePickable() const {
    return LightEntityItem::getLightsArePickable();
}

void EntityScriptingInterface::setZonesArePickable(bool value) {
    ZoneEntityItem::setZonesArePickable(value);
}

bool EntityScriptingInterface::getZonesArePickable() const {
    return ZoneEntityItem::getZonesArePickable();
}

void EntityScriptingInterface::setDrawZoneBoundaries(bool value) {
    ZoneEntityItem::setDrawZoneBoundaries(value);
}

bool EntityScriptingInterface::getDrawZoneBoundaries() const {
    return ZoneEntityItem::getDrawZoneBoundaries();
}

QScriptValue RayToEntityIntersectionResultToScriptValue(QScriptEngine* engine, const RayToEntityIntersectionResult& value) {
    QScriptValue obj = engine->newObject();
    obj.setProperty("intersects", value.intersects);
    obj.setProperty("accurate", value.accurate);
    QScriptValue entityItemValue = EntityItemIDtoScriptValue(engine, value.entityID);
    obj.setProperty("entityID", entityItemValue);
    obj.setProperty("distance", value.distance);
    obj.setProperty("face", boxFaceToString(value.face));

    QScriptValue intersection = vec3ToScriptValue(engine, value.intersection);
    obj.setProperty("intersection", intersection);
    QScriptValue surfaceNormal = vec3ToScriptValue(engine, value.surfaceNormal);
    obj.setProperty("surfaceNormal", surfaceNormal);
    obj.setProperty("extraInfo", engine->toScriptValue(value.extraInfo));
    return obj;
}

void RayToEntityIntersectionResultFromScriptValue(const QScriptValue& object, RayToEntityIntersectionResult& value) {
    value.intersects = object.property("intersects").toVariant().toBool();
    value.accurate = object.property("accurate").toVariant().toBool();
    QScriptValue entityIDValue = object.property("entityID");
    quuidFromScriptValue(entityIDValue, value.entityID);
    value.distance = object.property("distance").toVariant().toFloat();
    value.face = boxFaceFromString(object.property("face").toVariant().toString());

    QScriptValue intersection = object.property("intersection");
    if (intersection.isValid()) {
        vec3FromScriptValue(intersection, value.intersection);
    }
    QScriptValue surfaceNormal = object.property("surfaceNormal");
    if (surfaceNormal.isValid()) {
        vec3FromScriptValue(surfaceNormal, value.surfaceNormal);
    }
    value.extraInfo = object.property("extraInfo").toVariant().toMap();
}

bool EntityScriptingInterface::polyVoxWorker(QUuid entityID, std::function<bool(PolyVoxEntityItem&)> actor) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    if (!_entityTree) {
        return false;
    }

    EntityItemPointer entity = _entityTree->findEntityByEntityItemID(entityID);
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::setVoxels no entity with ID" << entityID;
        return false;
    }

    EntityTypes::EntityType entityType = entity->getType();
    if (entityType != EntityTypes::PolyVox) {
        return false;
    }

    auto polyVoxEntity = std::dynamic_pointer_cast<PolyVoxEntityItem>(entity);
    bool result;
    _entityTree->withWriteLock([&] {
        result = actor(*polyVoxEntity);
    });
    return result;
}

bool EntityScriptingInterface::setPoints(QUuid entityID, std::function<bool(LineEntityItem&)> actor) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    if (!_entityTree) {
        return false;
    }

    EntityItemPointer entity = static_cast<EntityItemPointer>(_entityTree->findEntityByEntityItemID(entityID));
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::setPoints no entity with ID" << entityID;
    }

    EntityTypes::EntityType entityType = entity->getType();

    if (entityType != EntityTypes::Line) {
        return false;
    }

    auto now = usecTimestampNow();

    auto lineEntity = std::static_pointer_cast<LineEntityItem>(entity);
    bool success;
    _entityTree->withWriteLock([&] {
        success = actor(*lineEntity);
        entity->setLastEdited(now);
        entity->setLastBroadcast(now);
    });

    EntityItemProperties properties;
    _entityTree->withReadLock([&] {
        properties = entity->getProperties();
    });

    properties.setLinePointsDirty();
    properties.setLastEdited(now);

    queueEntityMessage(PacketType::EntityEdit, entityID, properties);
    return success;
}

bool EntityScriptingInterface::setVoxelSphere(const QUuid& entityID, const glm::vec3& center, float radius, int value) {
    PROFILE_RANGE(script_entities, __FUNCTION__);
    return polyVoxWorker(entityID, [center, radius, value](PolyVoxEntityItem& polyVoxEntity) {
        return polyVoxEntity.setSphere(center, radius, value);
    });
}

bool EntityScriptingInterface::setVoxelCapsule(const QUuid& entityID,
                                               const glm::vec3& start, const glm::vec3& end,
                                               float radius, int value) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return polyVoxWorker(entityID, [start, end, radius, value](PolyVoxEntityItem& polyVoxEntity) {
        return polyVoxEntity.setCapsule(start, end, radius, value);
    });
}

bool EntityScriptingInterface::setVoxel(const QUuid& entityID, const glm::vec3& position, int value) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return polyVoxWorker(entityID, [position, value](PolyVoxEntityItem& polyVoxEntity) {
        return polyVoxEntity.setVoxelInVolume(position, value);
    });
}

bool EntityScriptingInterface::setAllVoxels(const QUuid& entityID, int value) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return polyVoxWorker(entityID, [value](PolyVoxEntityItem& polyVoxEntity) {
        return polyVoxEntity.setAll(value);
    });
}

bool EntityScriptingInterface::setVoxelsInCuboid(const QUuid& entityID, const glm::vec3& lowPosition,
                                                 const glm::vec3& cuboidSize, int value) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return polyVoxWorker(entityID, [lowPosition, cuboidSize, value](PolyVoxEntityItem& polyVoxEntity) {
        return polyVoxEntity.setCuboid(lowPosition, cuboidSize, value);
    });
}

bool EntityScriptingInterface::setAllPoints(const QUuid& entityID, const QVector<glm::vec3>& points) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    EntityItemPointer entity = static_cast<EntityItemPointer>(_entityTree->findEntityByEntityItemID(entityID));
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::setPoints no entity with ID" << entityID;
    }

    EntityTypes::EntityType entityType = entity->getType();

    if (entityType == EntityTypes::Line) {
        return setPoints(entityID, [points](LineEntityItem& lineEntity) -> bool
        {
            return (LineEntityItem*)lineEntity.setLinePoints(points);
        });
    }

    return false;
}

bool EntityScriptingInterface::appendPoint(const QUuid& entityID, const glm::vec3& point) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    EntityItemPointer entity = static_cast<EntityItemPointer>(_entityTree->findEntityByEntityItemID(entityID));
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::setPoints no entity with ID" << entityID;
        // There is no entity
        return false;
    }

    EntityTypes::EntityType entityType = entity->getType();

    if (entityType == EntityTypes::Line) {
        return setPoints(entityID, [point](LineEntityItem& lineEntity) -> bool
        {
            return (LineEntityItem*)lineEntity.appendPoint(point);
        });
    }

    return false;
}


bool EntityScriptingInterface::actionWorker(const QUuid& entityID,
                                            std::function<bool(EntitySimulationPointer, EntityItemPointer)> actor) {
    if (!_entityTree) {
        return false;
    }

    EntityItemPointer entity;
    bool doTransmit = false;
    _entityTree->withWriteLock([this, &entity, entityID, &doTransmit, actor] {
        EntitySimulationPointer simulation = _entityTree->getSimulation();
        entity = _entityTree->findEntityByEntityItemID(entityID);
        if (!entity) {
            qCDebug(entities) << "actionWorker -- unknown entity" << entityID;
            return;
        }

        if (!simulation) {
            qCDebug(entities) << "actionWorker -- no simulation" << entityID;
            return;
        }

        if (entity->isAvatarEntity() && !entity->isMyAvatarEntity()) {
            return;
        }

        doTransmit = actor(simulation, entity);
        _entityTree->entityChanged(entity);
    });

    // transmit the change
    if (doTransmit) {
        EntityItemProperties properties = _entityTree->resultWithReadLock<EntityItemProperties>([&] {
            return entity->getProperties();
        });

        properties.setActionDataDirty();
        auto now = usecTimestampNow();
        properties.setLastEdited(now);
        queueEntityMessage(PacketType::EntityEdit, entityID, properties);
    }

    return doTransmit;
}


QUuid EntityScriptingInterface::addAction(const QString& actionTypeString,
                                          const QUuid& entityID,
                                          const QVariantMap& arguments) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QUuid actionID = QUuid::createUuid();
    auto actionFactory = DependencyManager::get<EntityDynamicFactoryInterface>();
    bool success = false;
    actionWorker(entityID, [&](EntitySimulationPointer simulation, EntityItemPointer entity) {
        // create this action even if the entity doesn't have physics info.  it will often be the
        // case that a script adds an action immediately after an object is created, and the physicsInfo
        // is computed asynchronously.
        // if (!entity->getPhysicsInfo()) {
        //     return false;
        // }
        EntityDynamicType dynamicType = EntityDynamicInterface::dynamicTypeFromString(actionTypeString);
        if (dynamicType == DYNAMIC_TYPE_NONE) {
            return false;
        }
        EntityDynamicPointer action = actionFactory->factory(dynamicType, actionID, entity, arguments);
        if (!action) {
            return false;
        }
        action->setIsMine(true);
        success = entity->addAction(simulation, action);
        entity->upgradeScriptSimulationPriority(SCRIPT_GRAB_SIMULATION_PRIORITY);
        return false; // Physics will cause a packet to be sent, so don't send from here.
    });
    if (success) {
        return actionID;
    }
    return QUuid();
}


bool EntityScriptingInterface::updateAction(const QUuid& entityID, const QUuid& actionID, const QVariantMap& arguments) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    return actionWorker(entityID, [&](EntitySimulationPointer simulation, EntityItemPointer entity) {
        bool success = entity->updateAction(simulation, actionID, arguments);
        if (success) {
            entity->upgradeScriptSimulationPriority(SCRIPT_GRAB_SIMULATION_PRIORITY);
        }
        return success;
    });
}

bool EntityScriptingInterface::deleteAction(const QUuid& entityID, const QUuid& actionID) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    bool success = false;
    actionWorker(entityID, [&](EntitySimulationPointer simulation, EntityItemPointer entity) {
        success = entity->removeAction(simulation, actionID);
        if (success) {
            // reduce from grab to poke
            entity->upgradeScriptSimulationPriority(SCRIPT_POKE_SIMULATION_PRIORITY);
        }
        return false; // Physics will cause a packet to be sent, so don't send from here.
    });
    return success;
}

QVector<QUuid> EntityScriptingInterface::getActionIDs(const QUuid& entityID) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QVector<QUuid> result;
    actionWorker(entityID, [&](EntitySimulationPointer simulation, EntityItemPointer entity) {
        QList<QUuid> actionIDs = entity->getActionIDs();
        result = QVector<QUuid>::fromList(actionIDs);
        return false; // don't send an edit packet
    });
    return result;
}

QVariantMap EntityScriptingInterface::getActionArguments(const QUuid& entityID, const QUuid& actionID) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    QVariantMap result;
    actionWorker(entityID, [&](EntitySimulationPointer simulation, EntityItemPointer entity) {
        result = entity->getActionArguments(actionID);
        return false; // don't send an edit packet
    });
    return result;
}

EntityItemPointer EntityScriptingInterface::checkForTreeEntityAndTypeMatch(const QUuid& entityID,
                                                                           EntityTypes::EntityType entityType) {
    if (!_entityTree) {
        return EntityItemPointer();
    }
    
    EntityItemPointer entity = _entityTree->findEntityByEntityItemID(entityID);
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::checkForTreeEntityAndTypeMatch - no entity with ID" << entityID;
        return entity;
    }
    
    if (entityType != EntityTypes::Unknown && entity->getType() != entityType) {
        return EntityItemPointer();
    }
    
    return entity;
}

glm::vec3 EntityScriptingInterface::voxelCoordsToWorldCoords(const QUuid& entityID, glm::vec3 voxelCoords) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::PolyVox)) {
        auto polyVoxEntity = std::dynamic_pointer_cast<PolyVoxEntityItem>(entity);
        return polyVoxEntity->voxelCoordsToWorldCoords(voxelCoords);
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::worldCoordsToVoxelCoords(const QUuid& entityID, glm::vec3 worldCoords) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::PolyVox)) {
        auto polyVoxEntity = std::dynamic_pointer_cast<PolyVoxEntityItem>(entity);
        return polyVoxEntity->worldCoordsToVoxelCoords(worldCoords);
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::voxelCoordsToLocalCoords(const QUuid& entityID, glm::vec3 voxelCoords) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::PolyVox)) {
        auto polyVoxEntity = std::dynamic_pointer_cast<PolyVoxEntityItem>(entity);
        return polyVoxEntity->voxelCoordsToLocalCoords(voxelCoords);
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::localCoordsToVoxelCoords(const QUuid& entityID, glm::vec3 localCoords) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::PolyVox)) {
        auto polyVoxEntity = std::dynamic_pointer_cast<PolyVoxEntityItem>(entity);
        return polyVoxEntity->localCoordsToVoxelCoords(localCoords);
    } else {
        return glm::vec3(0.0f);
    }
}

int EntityScriptingInterface::getJointParent(const QUuid& entityID, int index) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        return modelEntity->getJointParent(index);
    } else {
        return -1;
    }
}

glm::vec3 EntityScriptingInterface::getAbsoluteJointTranslationInObjectFrame(const QUuid& entityID, int jointIndex) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        return modelEntity->getAbsoluteJointTranslationInObjectFrame(jointIndex);
    } else {
        return glm::vec3(0.0f);
    }
}

glm::quat EntityScriptingInterface::getAbsoluteJointRotationInObjectFrame(const QUuid& entityID, int jointIndex) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        return modelEntity->getAbsoluteJointRotationInObjectFrame(jointIndex);
    } else {
        return glm::quat();
    }
}

bool EntityScriptingInterface::setAbsoluteJointTranslationInObjectFrame(const QUuid& entityID,
                                                                        int jointIndex, glm::vec3 translation) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        bool result = modelEntity->setAbsoluteJointTranslationInObjectFrame(jointIndex, translation);
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                properties = entity->getProperties();
                entity->setLastBroadcast(now);
            });

            properties.setJointTranslationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}

bool EntityScriptingInterface::setAbsoluteJointRotationInObjectFrame(const QUuid& entityID,
                                                                     int jointIndex, glm::quat rotation) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        bool result = modelEntity->setAbsoluteJointRotationInObjectFrame(jointIndex, rotation);
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                properties = entity->getProperties();
                entity->setLastBroadcast(now);
            });

            properties.setJointRotationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}

glm::vec3 EntityScriptingInterface::getLocalJointTranslation(const QUuid& entityID, int jointIndex) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        return modelEntity->getLocalJointTranslation(jointIndex);
    } else {
        return glm::vec3(0.0f);
    }
}

glm::quat EntityScriptingInterface::getLocalJointRotation(const QUuid& entityID, int jointIndex) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        return modelEntity->getLocalJointRotation(jointIndex);
    } else {
        return glm::quat();
    }
}

bool EntityScriptingInterface::setLocalJointTranslation(const QUuid& entityID, int jointIndex, glm::vec3 translation) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        bool result = modelEntity->setLocalJointTranslation(jointIndex, translation);
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                properties = entity->getProperties();
                entity->setLastBroadcast(now);
            });

            properties.setJointTranslationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}

bool EntityScriptingInterface::setLocalJointRotation(const QUuid& entityID, int jointIndex, glm::quat rotation) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);
        bool result = modelEntity->setLocalJointRotation(jointIndex, rotation);
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                properties = entity->getProperties();
                entity->setLastBroadcast(now);
            });

            properties.setJointRotationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}



bool EntityScriptingInterface::setLocalJointRotations(const QUuid& entityID, const QVector<glm::quat>& rotations) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);

        bool result = false;
        for (int index = 0; index < rotations.size(); index++) {
            result |= modelEntity->setLocalJointRotation(index, rotations[index]);
        }
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                entity->setLastEdited(now);
                entity->setLastBroadcast(now);
                properties = entity->getProperties();
            });

            properties.setJointRotationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}


bool EntityScriptingInterface::setLocalJointTranslations(const QUuid& entityID, const QVector<glm::vec3>& translations) {
    if (auto entity = checkForTreeEntityAndTypeMatch(entityID, EntityTypes::Model)) {
        auto now = usecTimestampNow();
        auto modelEntity = std::dynamic_pointer_cast<ModelEntityItem>(entity);

        bool result = false;
        for (int index = 0; index < translations.size(); index++) {
            result |= modelEntity->setLocalJointTranslation(index, translations[index]);
        }
        if (result) {
            EntityItemProperties properties;
            _entityTree->withWriteLock([&] {
                entity->setLastEdited(now);
                entity->setLastBroadcast(now);
                properties = entity->getProperties();
            });

            properties.setJointTranslationsDirty();
            properties.setLastEdited(now);
            queueEntityMessage(PacketType::EntityEdit, entityID, properties);
            return true;
        }
    }
    return false;
}

bool EntityScriptingInterface::setLocalJointsData(const QUuid& entityID,
                                                  const QVector<glm::quat>& rotations,
                                                  const QVector<glm::vec3>& translations) {
    // for a model with 80 joints, sending both these in one edit packet causes the packet to be too large.
    return setLocalJointRotations(entityID, rotations) ||
        setLocalJointTranslations(entityID, translations);
}

int EntityScriptingInterface::getJointIndex(const QUuid& entityID, const QString& name) {
    if (!_entityTree) {
        return -1;
    }
    int result;
    _entityTree->withReadLock([&] {
       result = _entityTree->getJointIndex(entityID, name);
    });
    return result;
}

QStringList EntityScriptingInterface::getJointNames(const QUuid& entityID) {
    if (!_entityTree) {
        return QStringList();
    }
    QStringList result;
    _entityTree->withReadLock([&] {
        result = _entityTree->getJointNames(entityID);
    });
    return result;
}

QVector<QUuid> EntityScriptingInterface::getChildrenIDs(const QUuid& parentID) {
    QVector<QUuid> result;
    if (!_entityTree) {
        return result;
    }
    _entityTree->withReadLock([&] {
        QSharedPointer<SpatialParentFinder> parentFinder = DependencyManager::get<SpatialParentFinder>();
        if (!parentFinder) {
            return;
        }
        bool success;
        SpatiallyNestableWeakPointer parentWP = parentFinder->find(parentID, success);
        if (!success) {
            return;
        }
        SpatiallyNestablePointer parent = parentWP.lock();
        if (!parent) {
            return;
        }
        parent->forEachChild([&](SpatiallyNestablePointer child) {
            result.push_back(child->getID());
        });
    });

    return result;
}

bool EntityScriptingInterface::isChildOfParent(const QUuid& childID, const QUuid& parentID) {
    bool isChild = false;

    if (!_entityTree) {
        return isChild;
    }

    _entityTree->withReadLock([&] {
        EntityItemPointer parent = _entityTree->findEntityByEntityItemID(parentID);
        if (parent) {
            parent->forEachDescendant([&](SpatiallyNestablePointer descendant) {
                if (descendant->getID() == childID) {
                    isChild = true;
                    return;
                }
            });
        }
    });

    return isChild;
}

QString EntityScriptingInterface::getNestableType(const QUuid& id) {
    QSharedPointer<SpatialParentFinder> parentFinder = DependencyManager::get<SpatialParentFinder>();
    if (!parentFinder) {
        return "unknown";
    }
    bool success;
    SpatiallyNestableWeakPointer objectWP = parentFinder->find(id, success);
    if (!success) {
        return "unknown";
    }
    SpatiallyNestablePointer object = objectWP.lock();
    if (!object) {
        return "unknown";
    }
    NestableType nestableType = object->getNestableType();
    return SpatiallyNestable::nestableTypeToString(nestableType);
}

QVector<QUuid> EntityScriptingInterface::getChildrenIDsOfJoint(const QUuid& parentID, int jointIndex) {
    QVector<QUuid> result;
    if (!_entityTree) {
        return result;
    }
    _entityTree->withReadLock([&] {
        QSharedPointer<SpatialParentFinder> parentFinder = DependencyManager::get<SpatialParentFinder>();
        if (!parentFinder) {
            return;
        }
        bool success;
        SpatiallyNestableWeakPointer parentWP = parentFinder->find(parentID, success);
        if (!success) {
            return;
        }
        SpatiallyNestablePointer parent = parentWP.lock();
        if (!parent) {
            return;
        }
        parent->forEachChild([&](SpatiallyNestablePointer child) {
            if (child->getParentJointIndex() == jointIndex) {
                result.push_back(child->getID());
            }
        });
    });
    return result;
}

QUuid EntityScriptingInterface::getKeyboardFocusEntity() const {
    QUuid result;
    QMetaObject::invokeMethod(qApp, "getKeyboardFocusEntity", Qt::DirectConnection, Q_RETURN_ARG(QUuid, result));
    return result;
}

void EntityScriptingInterface::setKeyboardFocusEntity(const QUuid& id) {
    QMetaObject::invokeMethod(qApp, "setKeyboardFocusEntity", Qt::DirectConnection, Q_ARG(const QUuid&, id));
}

void EntityScriptingInterface::sendMousePressOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit mousePressOnEntity(id, event);
}

void EntityScriptingInterface::sendMouseMoveOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit mouseMoveOnEntity(id, event);
}

void EntityScriptingInterface::sendMouseReleaseOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit mouseReleaseOnEntity(id, event);
}

void EntityScriptingInterface::sendClickDownOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit clickDownOnEntity(id, event);
}

void EntityScriptingInterface::sendHoldingClickOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit holdingClickOnEntity(id, event);
}

void EntityScriptingInterface::sendClickReleaseOnEntity(const EntityItemID& id, const PointerEvent& event) {
    emit clickReleaseOnEntity(id, event);
}

void EntityScriptingInterface::sendHoverEnterEntity(const EntityItemID& id, const PointerEvent& event) {
    emit hoverEnterEntity(id, event);
}

void EntityScriptingInterface::sendHoverOverEntity(const EntityItemID& id, const PointerEvent& event) {
    emit hoverOverEntity(id, event);
}

void EntityScriptingInterface::sendHoverLeaveEntity(const EntityItemID& id, const PointerEvent& event) {
    emit hoverLeaveEntity(id, event);
}

bool EntityScriptingInterface::wantsHandControllerPointerEvents(const QUuid& id) {
    bool result = false;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(id));
            if (entity) {
                result = entity->wantsHandControllerPointerEvents();
            }
        });
    }
    return result;
}

void EntityScriptingInterface::emitScriptEvent(const EntityItemID& entityID, const QVariant& message) {
    EntityTree::emitScriptEvent(entityID, message);
}

// TODO move this someplace that makes more sense...
bool EntityScriptingInterface::AABoxIntersectsCapsule(const glm::vec3& low, const glm::vec3& dimensions,
                                                      const glm::vec3& start, const glm::vec3& end, float radius) {
    glm::vec3 penetration;
    AABox aaBox(low, dimensions);
    return aaBox.findCapsulePenetration(start, end, radius, penetration);
}

void EntityScriptingInterface::getMeshes(const QUuid& entityID, QScriptValue callback) {
    PROFILE_RANGE(script_entities, __FUNCTION__);

    EntityItemPointer entity = static_cast<EntityItemPointer>(_entityTree->findEntityByEntityItemID(entityID));
    if (!entity) {
        qCDebug(entities) << "EntityScriptingInterface::getMeshes no entity with ID" << entityID;
        QScriptValueList args { callback.engine()->undefinedValue(), false };
        callback.call(QScriptValue(), args);
        return;
    }

    MeshProxyList result;
    bool success = entity->getMeshes(result);

    if (success) {
        QScriptValue resultAsScriptValue = meshesToScriptValue(callback.engine(), result);
        QScriptValueList args { resultAsScriptValue, true };
        callback.call(QScriptValue(), args);
    } else {
        QScriptValueList args { callback.engine()->undefinedValue(), false };
        callback.call(QScriptValue(), args);
    }
}

glm::mat4 EntityScriptingInterface::getEntityTransform(const QUuid& entityID) {
    glm::mat4 result;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
            if (entity) {
                glm::mat4 translation = glm::translate(entity->getWorldPosition());
                glm::mat4 rotation = glm::mat4_cast(entity->getWorldOrientation());
                result = translation * rotation;
            }
        });
    }
    return result;
}

glm::mat4 EntityScriptingInterface::getEntityLocalTransform(const QUuid& entityID) {
    glm::mat4 result;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
            if (entity) {
                glm::mat4 translation = glm::translate(entity->getLocalPosition());
                glm::mat4 rotation = glm::mat4_cast(entity->getLocalOrientation());
                result = translation * rotation;
            }
        });
    }
    return result;
}

QString EntityScriptingInterface::getStaticCertificateJSON(const QUuid& entityID) {
    QByteArray result;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
            if (entity) {
                result = entity->getProperties().getStaticCertificateJSON();
            }
        });
    }
    return result;
}

bool EntityScriptingInterface::verifyStaticCertificateProperties(const QUuid& entityID) {
    bool result = false;
    if (_entityTree) {
        _entityTree->withReadLock([&] {
            EntityItemPointer entity = _entityTree->findEntityByEntityItemID(EntityItemID(entityID));
            if (entity) {
                result = entity->getProperties().verifyStaticCertificateProperties();
            }
        });
    }
    return result;
}

const EntityPropertyInfo EntityScriptingInterface::getPropertyInfo(const QString& propertyName) const {
    EntityPropertyInfo propertyInfo;
    EntityItemProperties::getPropertyInfo(propertyName, propertyInfo);
    return propertyInfo;
}

glm::vec3 EntityScriptingInterface::worldToLocalPosition(glm::vec3 worldPosition, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 localPosition = SpatiallyNestable::worldToLocal(worldPosition, parentID, parentJointIndex,
                                                              scalesWithParent, success);
    if (success) {
        return localPosition;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::quat EntityScriptingInterface::worldToLocalRotation(glm::quat worldRotation, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::quat localRotation = SpatiallyNestable::worldToLocal(worldRotation, parentID, parentJointIndex,
                                                              scalesWithParent, success);
    if (success) {
        return localRotation;
    } else {
        return glm::quat();
    }
}

glm::vec3 EntityScriptingInterface::worldToLocalVelocity(glm::vec3 worldVelocity, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 localVelocity = SpatiallyNestable::worldToLocalVelocity(worldVelocity, parentID, parentJointIndex,
                                                                      scalesWithParent, success);
    if (success) {
        return localVelocity;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::worldToLocalAngularVelocity(glm::vec3 worldAngularVelocity, const QUuid& parentID,
                                                                int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 localAngularVelocity = SpatiallyNestable::worldToLocalAngularVelocity(worldAngularVelocity, parentID,
                                                                                    parentJointIndex, scalesWithParent,
                                                                                    success);
    if (success) {
        return localAngularVelocity;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::worldToLocalDimensions(glm::vec3 worldDimensions, const QUuid& parentID,
                                                           int parentJointIndex, bool scalesWithParent) {

    bool success;
    glm::vec3 localDimensions = SpatiallyNestable::worldToLocalDimensions(worldDimensions, parentID, parentJointIndex,
                                                                          scalesWithParent, success);
    if (success) {
        return localDimensions;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::localToWorldPosition(glm::vec3 localPosition, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 worldPosition = SpatiallyNestable::localToWorld(localPosition, parentID, parentJointIndex,
                                                              scalesWithParent, success);
    if (success) {
        return worldPosition;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::quat EntityScriptingInterface::localToWorldRotation(glm::quat localRotation, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::quat worldRotation = SpatiallyNestable::localToWorld(localRotation, parentID, parentJointIndex,
                                                              scalesWithParent, success);
    if (success) {
        return worldRotation;
    } else {
        return glm::quat();
    }
}

glm::vec3 EntityScriptingInterface::localToWorldVelocity(glm::vec3 localVelocity, const QUuid& parentID,
                                                         int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 worldVelocity = SpatiallyNestable::localToWorldVelocity(localVelocity, parentID, parentJointIndex,
                                                                      scalesWithParent, success);
    if (success) {
        return worldVelocity;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::localToWorldAngularVelocity(glm::vec3 localAngularVelocity, const QUuid& parentID,
                                                                int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 worldAngularVelocity = SpatiallyNestable::localToWorldAngularVelocity(localAngularVelocity,
                                                                                    parentID, parentJointIndex,
                                                                                    scalesWithParent, success);
    if (success) {
        return worldAngularVelocity;
    } else {
        return glm::vec3(0.0f);
    }
}

glm::vec3 EntityScriptingInterface::localToWorldDimensions(glm::vec3 localDimensions, const QUuid& parentID,
                                                           int parentJointIndex, bool scalesWithParent) {
    bool success;
    glm::vec3 worldDimensions = SpatiallyNestable::localToWorldDimensions(localDimensions, parentID, parentJointIndex,
                                                                          scalesWithParent, success);
    if (success) {
        return worldDimensions;
    } else {
        return glm::vec3(0.0f);
    }
}
