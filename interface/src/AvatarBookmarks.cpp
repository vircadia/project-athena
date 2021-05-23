//
//  AvatarBookmarks.cpp
//  interface/src
//
//  Created by Triplelexx on 23/03/17.
//  Copyright 2017 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AvatarBookmarks.h"

#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QQmlContext>
#include <QList>
#include <QtCore/QThread>

#include <shared/QtHelpers.h>
#include <Application.h>
#include <OffscreenUi.h>
#include <avatar/AvatarManager.h>
#include <EntityItemID.h>
#include <EntityTree.h>
#include <ModelEntityItem.h>
#include <PhysicalEntitySimulation.h>
#include <EntityEditPacketSender.h>
#include <VariantMapToScriptValue.h>

#include "MainWindow.h"
#include "InterfaceLogging.h"

#include "QVariantGLM.h"

#include <QtQuick/QQuickWindow>
#include <memory>

void addAvatarEntities(const QVariantList& avatarEntities) {
    auto nodeList = DependencyManager::get<NodeList>();
    const QUuid myNodeID = nodeList->getSessionUUID();
    EntityTreePointer entityTree = DependencyManager::get<EntityTreeRenderer>()->getTree();
    if (!entityTree) {
        return;
    }
    EntitySimulationPointer entitySimulation = entityTree->getSimulation();
    PhysicalEntitySimulationPointer physicalEntitySimulation = std::static_pointer_cast<PhysicalEntitySimulation>(entitySimulation);
    EntityEditPacketSender* entityPacketSender = physicalEntitySimulation->getPacketSender();
    ScriptEnginePointer scriptEngine = newScriptEngine();
    for (int index = 0; index < avatarEntities.count(); index++) {
        const QVariantMap& avatarEntityProperties = avatarEntities.at(index).toMap();
        QVariant variantProperties = avatarEntityProperties["properties"];
        QVariantMap asMap = variantProperties.toMap();
        ScriptValuePointer scriptProperties = variantMapToScriptValue(asMap, *scriptEngine);
        EntityItemProperties entityProperties;
        EntityItemPropertiesFromScriptValueIgnoreReadOnly(scriptProperties, entityProperties);

        entityProperties.setParentID(myNodeID);
        entityProperties.setEntityHostType(entity::HostType::AVATAR);
        entityProperties.setOwningAvatarID(myNodeID);
        entityProperties.markAllChanged();

        EntityItemID id = EntityItemID(QUuid::createUuid());
        bool success = true;
        entityTree->withWriteLock([&entityTree, id, &entityProperties, &success] {
            EntityItemPointer entity = entityTree->addEntity(id, entityProperties);
            if (entity) {
                if (entityProperties.queryAACubeRelatedPropertyChanged()) {
                    // due to parenting, the server may not know where something is in world-space, so include the bounding cube.
                    bool success;
                    AACube queryAACube = entity->getQueryAACube(success);
                    if (success) {
                        entityProperties.setQueryAACube(queryAACube);
                    }
                }

                entity->setLastBroadcast(usecTimestampNow());
                if (entityProperties.getDynamic()) {
                    // since we're creating a dynamic object we volunteer immediately to own its simulation
                    entity->upgradeScriptSimulationPriority(VOLUNTEER_SIMULATION_PRIORITY);
                }
                entityProperties.setLastEdited(entity->getLastEdited());
            } else {
                qCDebug(entities) << "AvatarEntitiesBookmark failed to add new Entity to local Octree";
                success = false;
            }
        });

        if (success) {
            entityPacketSender->queueEditEntityMessage(PacketType::EntityAdd, entityTree, id, entityProperties);
        }
    }
}

AvatarBookmarks::AvatarBookmarks() {
    QDir directory(PathUtils::getAppDataPath());
    if (!directory.exists()) {
        directory.mkpath(".");
    }

    _bookmarksFilename = PathUtils::getAppDataPath() + "/" + AVATARBOOKMARKS_FILENAME;
    if(!QFile::exists(_bookmarksFilename)) {
        auto defaultBookmarksFilename = PathUtils::resourcesPath() + QString("avatar/bookmarks") + "/" + AVATARBOOKMARKS_FILENAME;
        if (!QFile::exists(defaultBookmarksFilename)) {
            qDebug() << defaultBookmarksFilename << "doesn't exist!";
        }

        if (!QFile::copy(defaultBookmarksFilename, _bookmarksFilename)) {
            qDebug() << "failed to copy" << defaultBookmarksFilename << "to" << _bookmarksFilename;
        } else {
            QFile bookmarksFile(_bookmarksFilename);
            bookmarksFile.setPermissions(bookmarksFile.permissions() | QFile::WriteUser);
        }
    }
    readFromFile();
}

void AvatarBookmarks::addBookmark(const QString& bookmarkName) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "addBookmark", Q_ARG(QString, bookmarkName));
        return;
    }
    QVariantMap bookmark = getAvatarDataToBookmark();
    insert(bookmarkName, bookmark);

    emit bookmarkAdded(bookmarkName);
}

void AvatarBookmarks::saveBookmark(const QString& bookmarkName) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "saveBookmark", Q_ARG(QString, bookmarkName));
        return;
    }
    if (contains(bookmarkName)) {
        QVariantMap bookmark = getAvatarDataToBookmark();
        insert(bookmarkName, bookmark);
    }
}

void AvatarBookmarks::removeBookmark(const QString& bookmarkName) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "removeBookmark", Q_ARG(QString, bookmarkName));
        return;
    }

    remove(bookmarkName);
    emit bookmarkDeleted(bookmarkName);
}

void AvatarBookmarks::deleteBookmark() {
}

void AvatarBookmarks::updateAvatarEntities(const QVariantList &avatarEntities) {
    auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();
    auto currentAvatarEntities = myAvatar->getAvatarEntityDataNonDefault();
    std::set<QUuid> newAvatarEntities;

    // Update or add all the new avatar entities
    for (auto& avatarEntityVariant : avatarEntities) {
        auto avatarEntityVariantMap = avatarEntityVariant.toMap();
        auto idItr = avatarEntityVariantMap.find("id");
        if (idItr != avatarEntityVariantMap.end()) {
            auto propertiesItr = avatarEntityVariantMap.find("properties");
            if (propertiesItr != avatarEntityVariantMap.end()) {
                EntityItemID id = idItr.value().toUuid();
                newAvatarEntities.insert(id);
                myAvatar->updateAvatarEntity(id, QJsonDocument::fromVariant(propertiesItr.value()).toBinaryData());
            }
        }
    }

    // Remove any old entities not in the new list
    for (auto& avatarEntityID : currentAvatarEntities.keys()) {
        if (newAvatarEntities.find(avatarEntityID) == newAvatarEntities.end()) {
            myAvatar->removeWornAvatarEntity(avatarEntityID);
        }
    }
}

/*@jsdoc
 * Details of an avatar bookmark.
 * @typedef {object} AvatarBookmarks.BookmarkData
 * @property {number} version - The version of the bookmark data format.
 * @property {string} avatarUrl - The URL of the avatar model.
 * @property {number} avatarScale - The target scale of the avatar.
 * @property {Array<Object<"properties",Entities.EntityProperties>>} [avatarEntites] - The avatar entities included with the 
 *     bookmark.
 * @property {AttachmentData[]} [attachments] - The attachments included with the bookmark.
 *     <p class="important">Deprecated: Use avatar entities instead.
 */

void AvatarBookmarks::loadBookmark(const QString& bookmarkName) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "loadBookmark", Q_ARG(QString, bookmarkName));
        return;
    }

    auto bookmarkEntry = _bookmarks.find(bookmarkName);

    if (bookmarkEntry != _bookmarks.end()) {
        QVariantMap bookmark = bookmarkEntry.value().toMap();
        if (bookmark.empty()) { // compatibility with bookmarks like this: "Wooden Doll": "http://mpassets.highfidelity.com/7fe80a1e-f445-4800-9e89-40e677b03bee-v1/mannequin.fst?noDownload=false",
            auto avatarUrl = bookmarkEntry.value().toString();
            if (!avatarUrl.isEmpty()) {
                bookmark.insert(ENTRY_AVATAR_URL, avatarUrl);
            }
        }

        if (!bookmark.empty()) {
            auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();
            auto treeRenderer = DependencyManager::get<EntityTreeRenderer>();
            EntityTreePointer entityTree = treeRenderer ? treeRenderer->getTree() : nullptr;

            // Once the skeleton URL has been loaded, add the Avatar Entities.
            // We have to wait, because otherwise the avatar entities will try to get attached to the joints
            // of the *current* avatar at first. But the current avatar might have a different joints scheme
            // from the new avatar, and that would cause the entities to be attached to the wrong joints.

            std::shared_ptr<QMetaObject::Connection> connection1 = std::make_shared<QMetaObject::Connection>();
            *connection1 = connect(myAvatar.get(), &MyAvatar::onLoadComplete, [this, bookmark, bookmarkName, myAvatar, connection1]() {
                qCDebug(interfaceapp) << "Finish loading avatar bookmark" << bookmarkName;
                QObject::disconnect(*connection1);
                myAvatar->clearWornAvatarEntities();
                const float& qScale = bookmark.value(ENTRY_AVATAR_SCALE, 1.0f).toFloat();
                myAvatar->setAvatarScale(qScale);
                QList<QVariant> attachments = bookmark.value(ENTRY_AVATAR_ATTACHMENTS, QList<QVariant>()).toList();
                myAvatar->setAttachmentsVariant(attachments);
                QVariantList avatarEntities = bookmark.value(ENTRY_AVATAR_ENTITIES, QVariantList()).toList();
                addAvatarEntities(avatarEntities);
                emit bookmarkLoaded(bookmarkName);
            });

            qCDebug(interfaceapp) << "Start loading avatar bookmark" << bookmarkName;

            const QString& avatarUrl = bookmark.value(ENTRY_AVATAR_URL, "").toString();
            myAvatar->useFullAvatarURL(avatarUrl);
        }
    }
}

void AvatarBookmarks::readFromFile() {
    // migrate old avatarbookmarks.json, used to be in 'local' folder on windows
    QString oldConfigPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" + AVATARBOOKMARKS_FILENAME;
    QFile oldConfig(oldConfigPath);

    // I imagine that in a year from now, this code for migrating (as well as the two lines above)
    // may be removed since all bookmarks should have been migrated by then
    // - Robbie Uvanni (6.8.2017)
    if (oldConfig.exists()) {
        if (QDir().rename(oldConfigPath, _bookmarksFilename)) {
            qCDebug(interfaceapp) << "Successfully migrated" << AVATARBOOKMARKS_FILENAME;
        } else {
            qCDebug(interfaceapp) << "Failed to migrate" << AVATARBOOKMARKS_FILENAME;
        }
    }

    Bookmarks::readFromFile();
}

QVariantMap AvatarBookmarks::getBookmark(const QString &bookmarkName)
{
    if (QThread::currentThread() != thread()) {
        QVariantMap result;
        BLOCKING_INVOKE_METHOD(this, "getBookmark", Q_RETURN_ARG(QVariantMap, result), Q_ARG(QString, bookmarkName));
        return result;
    }

    QVariantMap bookmark;

    auto bookmarkEntry = _bookmarks.find(bookmarkName);

    if (bookmarkEntry != _bookmarks.end()) {
        bookmark = bookmarkEntry.value().toMap();
    }

    return bookmark;
}

QVariantMap AvatarBookmarks::getAvatarDataToBookmark() {
    auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();

    const QString& avatarUrl = myAvatar->getSkeletonModelURL().toString();
    const QString& avatarIcon = QString("");
    const QVariant& avatarScale = myAvatar->getAvatarScale();

    // If Avatar attachments ever change, this is where to update them, when saving remember to also append to AVATAR_BOOKMARK_VERSION
    QVariantMap bookmark;
    bookmark.insert(ENTRY_VERSION, AVATAR_BOOKMARK_VERSION);
    bookmark.insert(ENTRY_AVATAR_URL, avatarUrl);
    bookmark.insert(ENTRY_AVATAR_ICON, avatarIcon);
    bookmark.insert(ENTRY_AVATAR_SCALE, avatarScale);

    QVariantList wearableEntities;
    auto treeRenderer = DependencyManager::get<EntityTreeRenderer>();
    EntityTreePointer entityTree = treeRenderer ? treeRenderer->getTree() : nullptr;

    if (entityTree) {
        ScriptEnginePointer scriptEngine = newScriptEngine();
        auto avatarEntities = myAvatar->getAvatarEntityDataNonDefault();
        for (auto entityID : avatarEntities.keys()) {
            auto entity = entityTree->findEntityByID(entityID);
            if (!entity || !isWearableEntity(entity)) {
                continue;
            }

            QVariantMap avatarEntityData;

            EncodeBitstreamParams params;
            auto desiredProperties = entity->getEntityProperties(params);
            desiredProperties += PROP_LOCAL_POSITION;
            desiredProperties += PROP_LOCAL_ROTATION;
            desiredProperties -= PROP_JOINT_ROTATIONS_SET;
            desiredProperties -= PROP_JOINT_ROTATIONS;
            desiredProperties -= PROP_JOINT_TRANSLATIONS_SET;
            desiredProperties -= PROP_JOINT_TRANSLATIONS;

            EntityItemProperties entityProperties = entity->getProperties(desiredProperties);
            ScriptValuePointer scriptProperties = EntityItemPropertiesToScriptValue(scriptEngine.get(), entityProperties);
            avatarEntityData["properties"] = scriptProperties->toVariant();
            wearableEntities.append(QVariant(avatarEntityData));
        }
    }
    bookmark.insert(ENTRY_AVATAR_ENTITIES, wearableEntities);
    return bookmark;
}
