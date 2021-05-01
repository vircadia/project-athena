//
//  DomainServerSettingsManager.cpp
//  domain-server/src
//
//  Created by Stephen Birarda on 2014-06-24.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DomainServerSettingsManager.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QSslKey>
#include <QSaveFile>
#include <QPair>

#include <AccountManager.h>
#include <Assignment.h>
#include <AvatarData.h>
#include <HifiConfigVariantMap.h>
#include <HTTPConnection.h>
#include <NLPacketList.h>
#include <NumericalConstants.h>
#include <SettingHandle.h>
#include <SettingHelpers.h>
#include <FingerprintUtils.h>
#include <ModerationFlags.h>

#include "DomainServerNodeData.h"

const QString SETTINGS_DESCRIPTION_RELATIVE_PATH = "/resources/describe-settings.json";
const QString SETTINGS_PATH = "/settings";
const QString SETTINGS_PATH_JSON = SETTINGS_PATH + ".json";
const QString CONTENT_SETTINGS_PATH_JSON = "/content-settings.json";

const QString DESCRIPTION_SETTINGS_KEY = "settings";
const QString SETTING_DEFAULT_KEY = "default";
const QString DESCRIPTION_NAME_KEY = "name";
const QString DESCRIPTION_GROUP_LABEL_KEY = "label";
const QString DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY = "show_on_enable";
const QString DESCRIPTION_ENABLE_KEY = "enable";
const QString DESCRIPTION_BACKUP_FLAG_KEY = "backup";
const QString SETTING_DESCRIPTION_TYPE_KEY = "type";
const QString DESCRIPTION_COLUMNS_KEY = "columns";
const QString CONTENT_SETTING_FLAG_KEY = "content_setting";
static const QString SPLIT_MENU_GROUPS_DOMAIN_SETTINGS_KEY = "domain_settings";
static const QString SPLIT_MENU_GROUPS_CONTENT_SETTINGS_KEY = "content_settings";

const QString SETTINGS_VIEWPOINT_KEY = "viewpoint";

DomainServerSettingsManager::DomainServerSettingsManager() {
    // load the description object from the settings description
    QFile descriptionFile(QCoreApplication::applicationDirPath() + SETTINGS_DESCRIPTION_RELATIVE_PATH);
    descriptionFile.open(QIODevice::ReadOnly);

    QJsonParseError parseError;
    QJsonDocument descriptionDocument = QJsonDocument::fromJson(descriptionFile.readAll(), &parseError);

    if (descriptionDocument.isObject()) {
        QJsonObject descriptionObject = descriptionDocument.object();

        const QString DESCRIPTION_VERSION_KEY = "version";

        if (descriptionObject.contains(DESCRIPTION_VERSION_KEY)) {
            // read the version from the settings description
            _descriptionVersion = descriptionObject[DESCRIPTION_VERSION_KEY].toDouble();

            if (descriptionObject.contains(DESCRIPTION_SETTINGS_KEY)) {
                _descriptionArray = descriptionDocument.object()[DESCRIPTION_SETTINGS_KEY].toArray();
                splitSettingsDescription();

                return;
            }
        }
    }

    static const QString MISSING_SETTINGS_DESC_MSG =
        QString("Did not find settings description in JSON at %1 - Unable to continue. domain-server will quit.\n%2 at %3")
        .arg(SETTINGS_DESCRIPTION_RELATIVE_PATH).arg(parseError.errorString()).arg(parseError.offset);
    static const int MISSING_SETTINGS_DESC_ERROR_CODE = 6;

    QMetaObject::invokeMethod(QCoreApplication::instance(), "queuedQuit", Qt::QueuedConnection,
                              Q_ARG(QString, MISSING_SETTINGS_DESC_MSG),
                              Q_ARG(int, MISSING_SETTINGS_DESC_ERROR_CODE));
}

void DomainServerSettingsManager::splitSettingsDescription() {
    // construct separate description arrays for domain settings and content settings
    // since they are displayed on different pages

    // along the way we also construct one object that holds the groups separated by domain settings
    // and content settings, so that the DS can setup dropdown menus below "Content" and "Settings"
    // headers to jump directly to a settings group on the page of either
    QJsonArray domainSettingsMenuGroups;
    QJsonArray contentSettingsMenuGroups;

    foreach(const QJsonValue& group, _descriptionArray) {
        QJsonObject groupObject = group.toObject();

        static const QString HIDDEN_GROUP_KEY = "hidden";
        bool groupHidden = groupObject.contains(HIDDEN_GROUP_KEY) && groupObject[HIDDEN_GROUP_KEY].toBool();

        QJsonArray domainSettingArray;
        QJsonArray contentSettingArray;

        foreach(const QJsonValue& settingDescription, groupObject[DESCRIPTION_SETTINGS_KEY].toArray()) {
            QJsonObject settingDescriptionObject = settingDescription.toObject();

            bool isContentSetting = settingDescriptionObject.contains(CONTENT_SETTING_FLAG_KEY)
                && settingDescriptionObject[CONTENT_SETTING_FLAG_KEY].toBool();

            if (isContentSetting) {
                // push the setting description to the pending content setting array
                contentSettingArray.push_back(settingDescriptionObject);
            } else {
                // push the setting description to the pending domain setting array
                domainSettingArray.push_back(settingDescriptionObject);
            }
        }

        if (!domainSettingArray.isEmpty() || !contentSettingArray.isEmpty()) {

            // we know for sure we'll have something to add to our settings menu groups
            // so setup that object for the group now, as long as the group isn't hidden alltogether
            QJsonObject settingsDropdownGroup;

            if (!groupHidden) {
                if (groupObject.contains(DESCRIPTION_NAME_KEY)) {
                    settingsDropdownGroup[DESCRIPTION_NAME_KEY] = groupObject[DESCRIPTION_NAME_KEY];
                }

                settingsDropdownGroup[DESCRIPTION_GROUP_LABEL_KEY] = groupObject[DESCRIPTION_GROUP_LABEL_KEY];

                if (groupObject.contains(DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY)) {
                    settingsDropdownGroup[DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY] = groupObject[DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY];
                }

                static const QString DESCRIPTION_GROUP_HTML_ID_KEY = "html_id";
                if (groupObject.contains(DESCRIPTION_GROUP_HTML_ID_KEY)) {
                    settingsDropdownGroup[DESCRIPTION_GROUP_HTML_ID_KEY] = groupObject[DESCRIPTION_GROUP_HTML_ID_KEY];
                }
            }

            if (!domainSettingArray.isEmpty()) {
                // we have some domain settings from this group, add the group with the filtered settings
                QJsonObject filteredGroupObject = groupObject;
                filteredGroupObject[DESCRIPTION_SETTINGS_KEY] = domainSettingArray;
                _domainSettingsDescription.push_back(filteredGroupObject);

                // if the group isn't hidden, add its information to the domain settings menu groups
                if (!groupHidden) {
                    domainSettingsMenuGroups.push_back(settingsDropdownGroup);
                }
            }

            if (!contentSettingArray.isEmpty()) {
                // we have some content settings from this group, add the group with the filtered settings
                QJsonObject filteredGroupObject = groupObject;
                filteredGroupObject[DESCRIPTION_SETTINGS_KEY] = contentSettingArray;
                _contentSettingsDescription.push_back(filteredGroupObject);

                // if the group isn't hidden, add its information to the content settings menu groups
                if (!groupHidden) {
                    contentSettingsMenuGroups.push_back(settingsDropdownGroup);
                }
            }
        }
    }

    // populate the settings menu groups with what we've collected

    _settingsMenuGroups[SPLIT_MENU_GROUPS_DOMAIN_SETTINGS_KEY] = domainSettingsMenuGroups;
    _settingsMenuGroups[SPLIT_MENU_GROUPS_CONTENT_SETTINGS_KEY] = contentSettingsMenuGroups;
}

void DomainServerSettingsManager::processSettingsRequestPacket(QSharedPointer<ReceivedMessage> message) {
    Assignment::Type type;
    message->readPrimitive(&type);

    QJsonObject responseObject = settingsResponseObjectForType(QString::number(type));
    auto json = QJsonDocument(responseObject).toJson();

    auto packetList = NLPacketList::create(PacketType::DomainSettings, QByteArray(), true, true);

    packetList->write(json);

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    nodeList->sendPacketList(std::move(packetList), message->getSenderSockAddr());
}

void DomainServerSettingsManager::setupConfigMap(const QString& userConfigFilename) {
    // since we're called from the DomainServerSettingsManager constructor, we don't take a write lock here
    // even though we change the underlying config map

    _configMap.setUserConfigFilename(userConfigFilename);
    _configMap.loadConfig();

    static const auto VERSION_SETTINGS_KEYPATH = "version";
    QVariant* versionVariant = _configMap.valueForKeyPath(VERSION_SETTINGS_KEYPATH);

    if (!versionVariant) {
        versionVariant = _configMap.valueForKeyPath(VERSION_SETTINGS_KEYPATH, true);
        *versionVariant = _descriptionVersion;
        persistToFile();
        qDebug() << "No version in config file, setting to current version" << _descriptionVersion;
    }

    {
        // Backward compatibility migration code
        // The config version used to be stored in a different file
        // This moves it to the actual config file.
        Setting::Handle<double> JSON_SETTING_VERSION("json-settings/version", 0.0);
        if (JSON_SETTING_VERSION.isSet()) {
            auto version = JSON_SETTING_VERSION.get();
            *versionVariant = version;
            persistToFile();
            QFile::remove(settingsFilename());
        }
    }

    // What settings version were we before and what are we using now?
    // Do we need to do any re-mapping?
    double oldVersion = versionVariant->toDouble();

    if (oldVersion != _descriptionVersion) {
        const QString ALLOWED_USERS_SETTINGS_KEYPATH = "security.allowed_users";
        const QString RESTRICTED_ACCESS_SETTINGS_KEYPATH = "security.restricted_access";
        const QString ALLOWED_EDITORS_SETTINGS_KEYPATH = "security.allowed_editors";
        const QString EDITORS_ARE_REZZERS_KEYPATH = "security.editors_are_rezzers";
        const QString EDITORS_CAN_REPLACE_CONTENT_KEYPATH = "security.editors_can_replace_content";

        qDebug() << "Previous domain-server settings version was"
            << QString::number(oldVersion, 'g', 8) << "and the new version is"
            << QString::number(_descriptionVersion, 'g', 8) << "- checking if any re-mapping is required";

        // we have a version mismatch - for now handle custom behaviour here since there are not many remappings
        if (oldVersion < 1.0) {
            // This was prior to the introduction of security.restricted_access
            // If the user has a list of allowed users then set their value for security.restricted_access to true

            QVariant* allowedUsers = _configMap.valueForKeyPath(ALLOWED_USERS_SETTINGS_KEYPATH);

            if (allowedUsers
                && allowedUsers->canConvert(QMetaType::QVariantList)
                && reinterpret_cast<QVariantList*>(allowedUsers)->size() > 0) {

                qDebug() << "Forcing security.restricted_access to TRUE since there was an"
                    << "existing list of allowed users.";

                // In the pre-toggle system the user had a list of allowed users, so
                // we need to set security.restricted_access to true
                QVariant* restrictedAccess = _configMap.valueForKeyPath(RESTRICTED_ACCESS_SETTINGS_KEYPATH, true);

                *restrictedAccess = QVariant(true);
            }
        }

        if (oldVersion < 1.1) {
            static const QString ENTITY_SERVER_SETTINGS_KEY = "entity_server_settings";
            static const QString ENTITY_FILE_NAME_KEY = "persistFilename";
            static const QString ENTITY_FILE_PATH_KEYPATH = ENTITY_SERVER_SETTINGS_KEY + ".persistFilePath";

            // this was prior to change of poorly named entitiesFileName to entitiesFilePath
            QVariant* persistFileNameVariant = _configMap.valueForKeyPath(ENTITY_SERVER_SETTINGS_KEY + "." + ENTITY_FILE_NAME_KEY);
            if (persistFileNameVariant && persistFileNameVariant->canConvert(QMetaType::QString)) {
                QString persistFileName = persistFileNameVariant->toString();

                qDebug() << "Migrating persistFilename to persistFilePath for entity-server settings";

                // grab the persistFilePath option, create it if it doesn't exist
                QVariant* persistFilePath = _configMap.valueForKeyPath(ENTITY_FILE_PATH_KEYPATH, true);

                // write the migrated value
                *persistFilePath = persistFileName;

                // remove the old setting
                QVariant* entityServerVariant = _configMap.valueForKeyPath(ENTITY_SERVER_SETTINGS_KEY);
                if (entityServerVariant && entityServerVariant->canConvert(QMetaType::QVariantMap)) {
                    QVariantMap entityServerMap = entityServerVariant->toMap();
                    entityServerMap.remove(ENTITY_FILE_NAME_KEY);

                    *entityServerVariant = entityServerMap;
                }
            }

        }

        if (oldVersion < 1.2) {
            // This was prior to the base64 encoding of password for HTTP Basic Authentication.
            // If we have a password in the previous settings file, make it base 64
            static const QString BASIC_AUTH_PASSWORD_KEY_PATH { "security.http_password" };

            QVariant* passwordVariant = _configMap.valueForKeyPath(BASIC_AUTH_PASSWORD_KEY_PATH);

            if (passwordVariant && passwordVariant->canConvert(QMetaType::QString)) {
                QString plaintextPassword = passwordVariant->toString();

                qDebug() << "Migrating plaintext password to SHA256 hash in domain-server settings.";

                *passwordVariant = QCryptographicHash::hash(plaintextPassword.toUtf8(), QCryptographicHash::Sha256).toHex();
            }
        }

        if (oldVersion < 1.4) {
            // This was prior to the permissions-grid in the domain-server settings page
            bool isRestrictedAccess = valueOrDefaultValueForKeyPath(RESTRICTED_ACCESS_SETTINGS_KEYPATH).toBool();
            QStringList allowedUsers = valueOrDefaultValueForKeyPath(ALLOWED_USERS_SETTINGS_KEYPATH).toStringList();
            QStringList allowedEditors = valueOrDefaultValueForKeyPath(ALLOWED_EDITORS_SETTINGS_KEYPATH).toStringList();
            bool onlyEditorsAreRezzers = valueOrDefaultValueForKeyPath(EDITORS_ARE_REZZERS_KEYPATH).toBool();

            _standardAgentPermissions[NodePermissions::standardNameLocalhost].reset(
                new NodePermissions(NodePermissions::standardNameLocalhost));
            _standardAgentPermissions[NodePermissions::standardNameLocalhost]->setAll(true);
            _standardAgentPermissions[NodePermissions::standardNameAnonymous].reset(
                new NodePermissions(NodePermissions::standardNameAnonymous));
            _standardAgentPermissions[NodePermissions::standardNameLoggedIn].reset(
                new NodePermissions(NodePermissions::standardNameLoggedIn));
            _standardAgentPermissions[NodePermissions::standardNameFriends].reset(
                new NodePermissions(NodePermissions::standardNameFriends));

            if (isRestrictedAccess) {
                // only users in allow-users list can connect
                _standardAgentPermissions[NodePermissions::standardNameAnonymous]->clear(
                    NodePermissions::Permission::canConnectToDomain);
                _standardAgentPermissions[NodePermissions::standardNameLoggedIn]->clear(
                    NodePermissions::Permission::canConnectToDomain);
            } // else anonymous and logged-in retain default of canConnectToDomain = true

            foreach (QString allowedUser, allowedUsers) {
                // even if isRestrictedAccess is false, we have to add explicit rows for these users.
                _agentPermissions[NodePermissionsKey(allowedUser, 0)].reset(new NodePermissions(allowedUser));
                _agentPermissions[NodePermissionsKey(allowedUser, 0)]->set(NodePermissions::Permission::canConnectToDomain);
            }

            foreach (QString allowedEditor, allowedEditors) {
                NodePermissionsKey editorKey(allowedEditor, 0);
                if (!_agentPermissions.contains(editorKey)) {
                    _agentPermissions[editorKey].reset(new NodePermissions(allowedEditor));
                    if (isRestrictedAccess) {
                        // they can change locks, but can't connect.
                        _agentPermissions[editorKey]->clear(NodePermissions::Permission::canConnectToDomain);
                    }
                }
                _agentPermissions[editorKey]->set(NodePermissions::Permission::canAdjustLocks);
            }

            std::list<std::unordered_map<NodePermissionsKey, NodePermissionsPointer>> permissionsSets{
                _standardAgentPermissions.get(),
                _agentPermissions.get()
            };
            foreach (auto permissionsSet, permissionsSets) {
                for (auto entry : permissionsSet) {
                    const auto& userKey = entry.first;
                    if (onlyEditorsAreRezzers) {
                        if (permissionsSet[userKey]->can(NodePermissions::Permission::canAdjustLocks)) {
                            permissionsSet[userKey]->set(NodePermissions::Permission::canRezPermanentEntities);
                            permissionsSet[userKey]->set(NodePermissions::Permission::canRezTemporaryEntities);
                        } else {
                            permissionsSet[userKey]->clear(NodePermissions::Permission::canRezPermanentEntities);
                            permissionsSet[userKey]->clear(NodePermissions::Permission::canRezTemporaryEntities);
                        }
                    } else {
                        permissionsSet[userKey]->set(NodePermissions::Permission::canRezPermanentEntities);
                        permissionsSet[userKey]->set(NodePermissions::Permission::canRezTemporaryEntities);
                    }
                }
            }

            packPermissions();
            _standardAgentPermissions.clear();
            _agentPermissions.clear();
        }

        if (oldVersion < 1.6) {
            unpackPermissions();

            // This was prior to addition of kick permissions, add that to localhost permissions by default
            _standardAgentPermissions[NodePermissions::standardNameLocalhost]->set(NodePermissions::Permission::canKick);

            packPermissions();
        }

        if (oldVersion < 1.8) {
            unpackPermissions();
            // This was prior to addition of domain content replacement, add that to localhost permissions by default
            _standardAgentPermissions[NodePermissions::standardNameLocalhost]->set(NodePermissions::Permission::canReplaceDomainContent);
            packPermissions();
        }

        if (oldVersion < 2.0) {
            const QString WIZARD_COMPLETED_ONCE = "wizard.completed_once";

            QVariant* wizardCompletedOnce = _configMap.valueForKeyPath(WIZARD_COMPLETED_ONCE, true);

            *wizardCompletedOnce = QVariant(true);
        }

        if (oldVersion < 2.1) {
            // convert old avatar scale settings into avatar height.

            const QString AVATAR_MIN_SCALE_KEYPATH = "avatars.min_avatar_scale";
            const QString AVATAR_MAX_SCALE_KEYPATH = "avatars.max_avatar_scale";
            const QString AVATAR_MIN_HEIGHT_KEYPATH = "avatars.min_avatar_height";
            const QString AVATAR_MAX_HEIGHT_KEYPATH = "avatars.max_avatar_height";

            QVariant* avatarMinScale = _configMap.valueForKeyPath(AVATAR_MIN_SCALE_KEYPATH);
            if (avatarMinScale) {
                auto newMinScaleVariant = _configMap.valueForKeyPath(AVATAR_MIN_HEIGHT_KEYPATH, true);
                *newMinScaleVariant = avatarMinScale->toFloat() * DEFAULT_AVATAR_HEIGHT;
            }

            QVariant* avatarMaxScale = _configMap.valueForKeyPath(AVATAR_MAX_SCALE_KEYPATH);
            if (avatarMaxScale) {
                auto newMaxScaleVariant = _configMap.valueForKeyPath(AVATAR_MAX_HEIGHT_KEYPATH, true);
                *newMaxScaleVariant = avatarMaxScale->toFloat() * DEFAULT_AVATAR_HEIGHT;
            }
        }

        if (oldVersion < 2.2) {
            // migrate entity server rolling backup intervals to new location for automatic content archive intervals

            const QString ENTITY_SERVER_BACKUPS_KEYPATH = "entity_server_settings.backups";
            const QString AUTO_CONTENT_ARCHIVES_RULES_KEYPATH = AUTOMATIC_CONTENT_ARCHIVES_GROUP + ".backup_rules";

            QVariant* previousBackupsVariant = _configMap.valueForKeyPath(ENTITY_SERVER_BACKUPS_KEYPATH);

            if (previousBackupsVariant) {
                auto migratedBackupsVariant = _configMap.valueForKeyPath(AUTO_CONTENT_ARCHIVES_RULES_KEYPATH, true);
                *migratedBackupsVariant = *previousBackupsVariant;
            }
        }

        if (oldVersion < 2.3) {
            unpackPermissions();
            _standardAgentPermissions[NodePermissions::standardNameLocalhost]->set(NodePermissions::Permission::canGetAndSetPrivateUserData);
            packPermissions();
        }

        if (oldVersion < 2.4) {
            // migrate oauth settings to their own group
            const QString ADMIN_USERS = "admin-users";
            const QString OAUTH_ADMIN_USERS = "oauth.admin-users";
            const QString OAUTH_CLIENT_ID = "oauth.client-id";
            const QString ALT_ADMIN_USERS = "admin.users";
            const QString ADMIN_ROLES = "admin-roles";
            const QString OAUTH_ADMIN_ROLES = "oauth.admin-roles";
            const QString OAUTH_ENABLE = "oauth.enable";

            QVector<QPair<const char*, const char*> > conversionMap = {
                {"key", "oauth.key"},
                {"cert", "oauth.cert"},
                {"hostname", "oauth.hostname"},
                {"oauth-client-id", "oauth.client-id"},
                {"oauth-provider", "oauth.provider"}
            };

            for (auto & conversion : conversionMap) {
                QVariant* prevValue = _configMap.valueForKeyPath(conversion.first);
                if (prevValue) {
                    auto newValue = _configMap.valueForKeyPath(conversion.second, true);
                    *newValue = *prevValue;
                }
            }

            QVariant* client_id = _configMap.valueForKeyPath(OAUTH_CLIENT_ID);
            if (client_id) {
                QVariant* oauthEnable = _configMap.valueForKeyPath(OAUTH_ENABLE, true);
                
                *oauthEnable = QVariant(true);
            }

            QVariant* oldAdminUsers = _configMap.valueForKeyPath(ADMIN_USERS);
            QVariant* newAdminUsers = _configMap.valueForKeyPath(OAUTH_ADMIN_USERS, true);
            QVariantList adminUsers(newAdminUsers->toList());
            if (oldAdminUsers) {
                QStringList adminUsersList = oldAdminUsers->toStringList();
                for (auto & user : adminUsersList) {
                    if (!adminUsers.contains(user)) {
                        adminUsers.append(user);
                    }
                }
            }
            QVariant* altAdminUsers = _configMap.valueForKeyPath(ALT_ADMIN_USERS);
            if (altAdminUsers) {
                QStringList adminUsersList = altAdminUsers->toStringList();
                for (auto & user : adminUsersList) {
                    if (!adminUsers.contains(user)) {
                        adminUsers.append(user);
                    }
                }
            }

            *newAdminUsers = adminUsers;

            QVariant* oldAdminRoles = _configMap.valueForKeyPath(ADMIN_ROLES);
            QVariant* newAdminRoles = _configMap.valueForKeyPath(OAUTH_ADMIN_ROLES, true);
            QVariantList adminRoles(newAdminRoles->toList());
            if (oldAdminRoles) {
                QStringList adminRoleList = oldAdminRoles->toStringList();
                for (auto & role : adminRoleList) {
                    if (!adminRoles.contains(role)) {
                        adminRoles.append(role);
                    }
                }
            }

            *newAdminRoles = adminRoles;
        }

        if (oldVersion < 2.5) {
            // Default values for new canRezAvatarEntities permission.
            unpackPermissions();
            std::list<std::unordered_map<NodePermissionsKey, NodePermissionsPointer>> permissionsSets{
                _standardAgentPermissions.get(),
                _agentPermissions.get(),
                _ipPermissions.get(),
                _macPermissions.get(),
                _machineFingerprintPermissions.get(),
                _groupPermissions.get(),
                _groupForbiddens.get()
            };
            foreach (auto permissionsSet, permissionsSets) {
                for (auto entry : permissionsSet) {
                    const auto& userKey = entry.first;
                    if (permissionsSet[userKey]->can(NodePermissions::Permission::canConnectToDomain)) {
                        permissionsSet[userKey]->set(NodePermissions::Permission::canRezAvatarEntities);
                    }
                }
            }
            packPermissions();
        }

        // write the current description version to our settings
        *versionVariant = _descriptionVersion;

        // write the new settings to the json file
        persistToFile();
    }

    unpackPermissions();
}

void DomainServerSettingsManager::initializeGroupPermissions(NodePermissionsMap& permissionsRows,
                                                             QString groupName, NodePermissionsPointer perms) {
    // this is called when someone has used the domain-settings webpage to add a group.  They type the group's name
    // and give it some permissions.  The domain-server asks api for the group's ranks and populates the map
    // with them.  Here, that initial user-entered row is removed and it's permissions are copied to all the ranks
    // except owner.

    QString groupNameLower = groupName.toLower();

    foreach (NodePermissionsKey nameKey, permissionsRows.keys()) {
        if (nameKey.first.toLower() != groupNameLower) {
            continue;
        }
        QUuid groupID = _groupIDs[groupNameLower.toLower()];
        QUuid rankID = nameKey.second;
        GroupRank rank = _groupRanks[groupID][rankID];
        if (rank.order == 0) {
            // we don't copy the initial permissions to the owner.
            continue;
        }
        permissionsRows[nameKey]->setAll(false);
        *(permissionsRows[nameKey]) |= *perms;
    }
}

void DomainServerSettingsManager::packPermissionsForMap(QString mapName,
                                                        NodePermissionsMap& permissionsRows,
                                                        QString keyPath) {
    // grab a write lock on the settings mutex since we're about to change the config map
    QWriteLocker locker(&_settingsLock);

    // find (or create) the "security" section of the settings map
    QVariant* security = _configMap.valueForKeyPath("security", true);
    if (!security->canConvert(QMetaType::QVariantMap)) {
        (*security) = QVariantMap();
    }

    // find (or create) whichever subsection of "security" we are packing
    QVariant* permissions = _configMap.valueForKeyPath(keyPath, true);
    if (!permissions->canConvert(QMetaType::QVariantList)) {
        (*permissions) = QVariantList();
    }

    // convert details for each member of the subsection
    QVariantList* permissionsList = reinterpret_cast<QVariantList*>(permissions);
    (*permissionsList).clear();
    QList<NodePermissionsKey> permissionsKeys = permissionsRows.keys();

    // when a group is added from the domain-server settings page, the config map has a group-name with
    // no ID or rank.  We need to leave that there until we get a valid response back from the api.
    // once we have the ranks and IDs, we need to delete the original entry so that it doesn't show
    // up in the settings-page with undefined's after it.
    QHash<QString, bool> groupNamesWithRanks;
    // note which groups have rank/ID information
    foreach (NodePermissionsKey userKey, permissionsKeys) {
        NodePermissionsPointer perms = permissionsRows[userKey];
        if (perms->getRankID() != QUuid()) {
            groupNamesWithRanks[userKey.first] = true;
        }
    }
    foreach (NodePermissionsKey userKey, permissionsKeys) {
        NodePermissionsPointer perms = permissionsRows[userKey];
        if (perms->isGroup()) {
            QString groupName = userKey.first;
            if (perms->getRankID() == QUuid() && groupNamesWithRanks.contains(groupName)) {
                // copy the values from this user-added entry to the other (non-owner) ranks and remove it.
                permissionsRows.remove(userKey);
                initializeGroupPermissions(permissionsRows, groupName, perms);
            }
        }
    }

    // convert each group-name / rank-id pair to a variant-map
    foreach (NodePermissionsKey userKey, permissionsKeys) {
        if (!permissionsRows.contains(userKey)) {
            continue;
        }
        NodePermissionsPointer perms = permissionsRows[userKey];
        if (perms->isGroup()) {
            QHash<QUuid, GroupRank>& groupRanks = _groupRanks[perms->getGroupID()];
            *permissionsList += perms->toVariant(groupRanks);
        } else {
            *permissionsList += perms->toVariant();
        }
    }
}

void DomainServerSettingsManager::packPermissions() {
    // transfer details from _agentPermissions to _configMap

    // save settings for anonymous / logged-in / localhost
    packPermissionsForMap("standard_permissions", _standardAgentPermissions, AGENT_STANDARD_PERMISSIONS_KEYPATH);

    // save settings for specific users
    packPermissionsForMap("permissions", _agentPermissions, AGENT_PERMISSIONS_KEYPATH);

    // save settings for IP addresses
    packPermissionsForMap("permissions", _ipPermissions, IP_PERMISSIONS_KEYPATH);

    // save settings for MAC addresses
    packPermissionsForMap("permissions", _macPermissions, MAC_PERMISSIONS_KEYPATH);

    // save settings for Machine Fingerprint
    packPermissionsForMap("permissions", _machineFingerprintPermissions, MACHINE_FINGERPRINT_PERMISSIONS_KEYPATH);

    // save settings for groups
    packPermissionsForMap("permissions", _groupPermissions, GROUP_PERMISSIONS_KEYPATH);

    // save settings for blacklist groups
    packPermissionsForMap("permissions", _groupForbiddens, GROUP_FORBIDDENS_KEYPATH);

    persistToFile();
}

bool DomainServerSettingsManager::unpackPermissionsForKeypath(const QString& keyPath,
                                                              NodePermissionsMap* mapPointer,
                                                              std::function<void(NodePermissionsPointer)> customUnpacker) {

    mapPointer->clear();

    QVariant permissions = valueOrDefaultValueForKeyPath(keyPath);

    if (!permissions.isValid()) {
        // we don't have a permissions object to unpack for this keypath, bail
        return false;
    }

    if (!permissions.canConvert(QMetaType::QVariantList)) {
        qDebug() << "Failed to extract permissions for key path" << keyPath << "from settings.";
    }

    bool needPack = false;

    QList<QVariant> permissionsList = permissions.toList();
    foreach (QVariant permsHash, permissionsList) {
        NodePermissionsPointer perms { new NodePermissions(permsHash.toMap()) };
        QString id = perms->getID();

        NodePermissionsKey idKey = perms->getKey();

        if (mapPointer->contains(idKey)) {
            qDebug() << "Duplicate name in permissions table for" << keyPath << " - " << id;
            *((*mapPointer)[idKey]) |= *perms;
            needPack = true;
        } else {
            (*mapPointer)[idKey] = perms;
        }

        if (customUnpacker) {
            customUnpacker(perms);
        }
    }

    return needPack;

}

void DomainServerSettingsManager::unpackPermissions() {
    // transfer details from _configMap to _agentPermissions

    // NOTE: Defaults for standard permissions (anonymous, friends, localhost, logged-in) used
    // to be set here and then immediately persisted to the config JSON file.
    // They have since been moved to describe-settings.json as the default value for AGENT_STANDARD_PERMISSIONS_KEYPATH.
    // In order to change the default standard permissions you must change the default value in describe-settings.json.

    bool needPack = false;

    needPack |= unpackPermissionsForKeypath(AGENT_STANDARD_PERMISSIONS_KEYPATH, &_standardAgentPermissions);

    needPack |= unpackPermissionsForKeypath(AGENT_PERMISSIONS_KEYPATH, &_agentPermissions);

    needPack |= unpackPermissionsForKeypath(IP_PERMISSIONS_KEYPATH, &_ipPermissions,
        [&](NodePermissionsPointer perms){
            // make sure that this permission row is for a valid IP address
            if (QHostAddress(perms->getKey().first).isNull()) {
                _ipPermissions.remove(perms->getKey());

                // we removed a row from the IP permissions, we'll need a re-pack
                needPack = true;
            }
    });

    needPack |= unpackPermissionsForKeypath(MAC_PERMISSIONS_KEYPATH, &_macPermissions,
        [&](NodePermissionsPointer perms){
            // make sure that this permission row is for a non-empty hardware
            if (perms->getKey().first.isEmpty()) {
                _macPermissions.remove(perms->getKey());

                // we removed a row from the MAC permissions, we'll need a re-pack
                needPack = true;
            }
    });

    needPack |= unpackPermissionsForKeypath(MACHINE_FINGERPRINT_PERMISSIONS_KEYPATH, &_machineFingerprintPermissions,
        [&](NodePermissionsPointer perms){
            // make sure that this permission row has valid machine fingerprint
            if (QUuid(perms->getKey().first) == QUuid()) {
                _machineFingerprintPermissions.remove(perms->getKey());

                // we removed a row, so we'll need a re-pack
                needPack = true;
            }

    });


    needPack |= unpackPermissionsForKeypath(GROUP_PERMISSIONS_KEYPATH, &_groupPermissions,
        [&](NodePermissionsPointer perms){
            if (perms->isGroup()) {
                // the group-id was cached.  hook-up the uuid in the uuid->group hash
                _groupPermissionsByUUID[GroupByUUIDKey(perms->getGroupID(), perms->getRankID())] = _groupPermissions[perms->getKey()];
                needPack |= setGroupID(perms->getID(), perms->getGroupID());
            }
    });

    needPack |= unpackPermissionsForKeypath(GROUP_FORBIDDENS_KEYPATH, &_groupForbiddens,
        [&](NodePermissionsPointer perms) {
            if (perms->isGroup()) {
                // the group-id was cached.  hook-up the uuid in the uuid->group hash
                _groupForbiddensByUUID[GroupByUUIDKey(perms->getGroupID(), perms->getRankID())] = _groupForbiddens[perms->getKey()];
                needPack |= setGroupID(perms->getID(), perms->getGroupID());
            }
    });

    needPack |= ensurePermissionsForGroupRanks();

    if (needPack) {
        packPermissions();
    }

#ifdef WANT_DEBUG
    qDebug() << "--------------- permissions ---------------------";
    std::array<NodePermissionsMap*, 7> permissionsSets {{
        &_standardAgentPermissions, &_agentPermissions,
        &_groupPermissions, &_groupForbiddens,
        &_ipPermissions, &_macPermissions,
        &_machineFingerprintPermissions
    }};

    foreach (auto permissionSet, permissionsSets) {
        auto& permissionKeyMap = permissionSet->get();
        auto it = permissionKeyMap.begin();

        while (it != permissionKeyMap.end()) {

            NodePermissionsPointer perms = it->second;
            if (perms->isGroup()) {
                qDebug() << it->first << perms->getGroupID() << perms;
            } else {
                qDebug() << it->first << perms;
            }

            ++it;
        }
    }
#endif

}

bool DomainServerSettingsManager::ensurePermissionsForGroupRanks() {
    // make sure each rank in each group has its own set of permissions
    bool changed = false;
    QList<QUuid> permissionGroupIDs = getGroupIDs();
    foreach (QUuid groupID, permissionGroupIDs) {
        QString groupName = _groupNames[groupID];
        QHash<QUuid, GroupRank>& ranksForGroup = _groupRanks[groupID];
        foreach (QUuid rankID, ranksForGroup.keys()) {
            NodePermissionsKey nameKey = NodePermissionsKey(groupName, rankID);
            GroupByUUIDKey idKey = GroupByUUIDKey(groupID, rankID);
            NodePermissionsPointer perms;
            if (_groupPermissions.contains(nameKey)) {
                perms = _groupPermissions[nameKey];
            } else {
                perms = NodePermissionsPointer(new NodePermissions(nameKey));
                _groupPermissions[nameKey] = perms;
                changed = true;
            }
            if (perms->getGroupID() != groupID) {
                perms->setGroupID(groupID);
                changed = true;
            }
            if (perms->getRankID() != rankID) {
                perms->setRankID(rankID);
                changed = true;
            }
            _groupPermissionsByUUID[idKey] = perms;
        }
    }

    QList<QUuid> forbiddenGroupIDs = getBlacklistGroupIDs();
    foreach (QUuid groupID, forbiddenGroupIDs) {
        QString groupName = _groupNames[groupID];
        QHash<QUuid, GroupRank>& ranksForGroup = _groupRanks[groupID];
        foreach (QUuid rankID, ranksForGroup.keys()) {
            NodePermissionsKey nameKey = NodePermissionsKey(groupName, rankID);
            GroupByUUIDKey idKey = GroupByUUIDKey(groupID, rankID);
            NodePermissionsPointer perms;
            if (_groupForbiddens.contains(nameKey)) {
                perms = _groupForbiddens[nameKey];
            } else {
                perms = NodePermissionsPointer(new NodePermissions(nameKey));
                _groupForbiddens[nameKey] = perms;
                changed = true;
            }
            if (perms->getGroupID() != groupID) {
                perms->setGroupID(groupID);
                changed = true;
            }
            if (perms->getRankID() != rankID) {
                perms->setRankID(rankID);
                changed = true;
            }
            _groupForbiddensByUUID[idKey] = perms;
        }
    }

    return changed;
}

void DomainServerSettingsManager::processNodeKickRequestPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    // before we do any processing on this packet make sure it comes from a node that is allowed to kick
    if (sendingNode->getCanKick()) {
        // pull the UUID being kicked from the packet
        QUuid nodeUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));

        bool hasOptionalBanParameters = false;
        int banParameters;
        bool banByUsername;
        bool banByFingerprint;
        bool banByIP;
        // pull optional ban parameters from the packet
        if (message.data()->getSize() == (NUM_BYTES_RFC4122_UUID + sizeof(int))) {
            hasOptionalBanParameters = true;
            message->readPrimitive(&banParameters);
            banByUsername = banParameters & ModerationFlags::BanFlags::BAN_BY_USERNAME;
            banByFingerprint = banParameters & ModerationFlags::BanFlags::BAN_BY_FINGERPRINT;
            banByIP = banParameters & ModerationFlags::BanFlags::BAN_BY_IP;
        }

        if (!nodeUUID.isNull() && nodeUUID != sendingNode->getUUID()) {
            // make sure we actually have a node with this UUID
            auto limitedNodeList = DependencyManager::get<LimitedNodeList>();

            auto matchingNode = limitedNodeList->nodeWithUUID(nodeUUID);

            if (matchingNode) {
                // we have a matching node, time to decide how to store updated permissions for this node

                NodePermissionsPointer destinationPermissions;

                auto verifiedUsername = matchingNode->getPermissions().getVerifiedUserName();

                bool newPermissions = false;

                if (!verifiedUsername.isEmpty()) {
                    // if we have a verified user name for this user, we first apply the kick to the username

                    // if we have optional ban parameters, we should ban the username based on the parameter
                    if (!hasOptionalBanParameters || banByUsername) {
                        // check if there were already permissions
                        bool hadPermissions = havePermissionsForName(verifiedUsername);

                        // grab or create permissions for the given username
                        auto userPermissions = _agentPermissions[matchingNode->getPermissions().getKey()];

                        newPermissions =
                            !hadPermissions || userPermissions->can(NodePermissions::Permission::canConnectToDomain);

                        // ensure that the connect permission is clear
                        userPermissions->clear(NodePermissions::Permission::canConnectToDomain);
                    }
                }

                // if we didn't have a username, or this domain-server uses the "multi-kick" setting to
                // kick logged in users via username AND machine fingerprint (or IP as fallback)
                // then we remove connect permissions for the machine fingerprint (or IP as fallback)
                const QString MULTI_KICK_SETTINGS_KEYPATH = "security.multi_kick_logged_in";

                if (banByFingerprint || verifiedUsername.isEmpty() || valueOrDefaultValueForKeyPath(MULTI_KICK_SETTINGS_KEYPATH).toBool()) {
                    // remove connect permissions for the machine fingerprint
                    DomainServerNodeData* nodeData = static_cast<DomainServerNodeData*>(matchingNode->getLinkedData());
                    if (nodeData) {
                        // get this machine's fingerprint
                        auto domainServerFingerprint = FingerprintUtils::getMachineFingerprint();

                        if (nodeData->getMachineFingerprint() == domainServerFingerprint) {
                            qWarning() << "attempt to kick node running on same machine as domain server (by fingerprint), ignoring KickRequest";
                            return;
                        }
                        NodePermissionsKey machineFingerprintKey(nodeData->getMachineFingerprint().toString(), 0);

                        // check if there were already permissions for the fingerprint
                        bool hadFingerprintPermissions = hasPermissionsForMachineFingerprint(nodeData->getMachineFingerprint());

                        // grab or create permissions for the given fingerprint
                        auto fingerprintPermissions = _machineFingerprintPermissions[machineFingerprintKey];

                        // write them
                        if (!hadFingerprintPermissions || fingerprintPermissions->can(NodePermissions::Permission::canConnectToDomain)) {
                            newPermissions = true;
                            fingerprintPermissions->clear(NodePermissions::Permission::canConnectToDomain);
                        }
                    } else {
                        // if no node data, all we can do is ban by IP address
                        banByIP = true;
                    }
                }
                
                if (banByIP) {
                    auto& kickAddress = matchingNode->getActiveSocket()
                        ? matchingNode->getActiveSocket()->getAddress()
                        : matchingNode->getPublicSocket().getAddress();

                    // probably isLoopback covers it, as whenever I try to ban an agent on same machine as the domain-server
                    // it is always 127.0.0.1, but looking at the public and local addresses just to be sure
                    // TODO: soon we will have feedback (in the form of a message to the client) after we kick.  When we
                    // do, we will have a success flag, and perhaps a reason for failure.  For now, just don't do it.
                    if (kickAddress == limitedNodeList->getPublicSockAddr().getAddress() ||
                        kickAddress == limitedNodeList->getLocalSockAddr().getAddress() ||
                        kickAddress.isLoopback() ) {
                        qWarning() << "attempt to kick node running on same machine as domain server, ignoring KickRequest";
                        return;
                    }

                    NodePermissionsKey ipAddressKey(kickAddress.toString(), QUuid());

                    // check if there were already permissions for the IP
                    bool hadIPPermissions = hasPermissionsForIP(kickAddress);

                    // grab or create permissions for the given IP address
                    auto ipPermissions = _ipPermissions[ipAddressKey];

                    if (!hadIPPermissions || ipPermissions->can(NodePermissions::Permission::canConnectToDomain)) {
                        newPermissions = true;

                        ipPermissions->clear(NodePermissions::Permission::canConnectToDomain);
                    }
                }

                if (newPermissions) {
                    qDebug() << "Removing connect permission for node" << uuidStringWithoutCurlyBraces(matchingNode->getUUID())
                        << "after kick request from" << uuidStringWithoutCurlyBraces(sendingNode->getUUID());

                    // we've changed permissions, time to store them to disk and emit our signal to say they have changed
                    packPermissions();
                }

                // we emit this no matter what -- though if this isn't a new permission probably 2 people are racing to kick and this
                // person lost the race.  No matter, just be sure this is called as otherwise it takes like 10s for the person being banned
                // to go away
                emit updateNodePermissions();

            } else {
                qWarning() << "Node kick request received for unknown node. Refusing to process.";
            }
        } else {
            // this isn't a UUID we can use
            qWarning() << "Node kick request received for invalid node ID or from node being kicked. Refusing to process.";
        }

    } else {
        qWarning() << "Refusing to process a kick packet from node" << uuidStringWithoutCurlyBraces(sendingNode->getUUID())
        << "that does not have kick permissions.";
    }
}

// This function processes the "Get Username from ID" request.
void DomainServerSettingsManager::processUsernameFromIDRequestPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    // From the packet, pull the UUID we're identifying
    QUuid nodeUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));

    if (!nodeUUID.isNull()) {
        // First, make sure we actually have a node with this UUID
        auto limitedNodeList = DependencyManager::get<LimitedNodeList>();
        auto matchingNode = limitedNodeList->nodeWithUUID(nodeUUID);

        // If we do have a matching node...
        if (matchingNode) {
            // Setup the packet
            auto usernameFromIDReplyPacket = NLPacket::create(PacketType::UsernameFromIDReply);

            QString verifiedUsername;
            QUuid machineFingerprint;

            // Write the UUID to the packet
            usernameFromIDReplyPacket->write(nodeUUID.toRfc4122());

            // Check if the sending node has permission to kick (is an admin)
            // OR if the message is from a node whose UUID matches the one in the packet
            if (sendingNode->getCanKick() || nodeUUID == sendingNode->getUUID()) {
                // It's time to figure out the username
                verifiedUsername = matchingNode->getPermissions().getVerifiedUserName();
                usernameFromIDReplyPacket->writeString(verifiedUsername);

                // now put in the machine fingerprint
                DomainServerNodeData* nodeData = static_cast<DomainServerNodeData*>(matchingNode->getLinkedData());
                machineFingerprint = nodeData ? nodeData->getMachineFingerprint() : QUuid();
                usernameFromIDReplyPacket->write(machineFingerprint.toRfc4122());
            } else {
                usernameFromIDReplyPacket->writeString(verifiedUsername);
                usernameFromIDReplyPacket->write(machineFingerprint.toRfc4122());
            }
            // Write whether or not the user is an admin
            bool isAdmin = matchingNode->getCanKick();
            usernameFromIDReplyPacket->writePrimitive(isAdmin);

            qDebug() << "Sending username" << verifiedUsername << "and machine fingerprint" << machineFingerprint << "associated with node" << nodeUUID << ". Node admin status: " << isAdmin;
            // Ship it!
            limitedNodeList->sendPacket(std::move(usernameFromIDReplyPacket), *sendingNode);
        } else {
            qWarning() << "Node username request received for unknown node. Refusing to process.";
        }
    } else {
        qWarning() << "Node username request received for invalid node ID. Refusing to process.";
    }
}

QStringList DomainServerSettingsManager::getAllNames() const {
    QStringList result;
    foreach (auto key, _agentPermissions.keys()) {
        result << key.first.toLower();
    }
    return result;
}

NodePermissions DomainServerSettingsManager::getStandardPermissionsForName(const NodePermissionsKey& name) const {
    if (_standardAgentPermissions.contains(name)) {
        return *(_standardAgentPermissions[name].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForName(const QString& name) const {
    NodePermissionsKey nameKey = NodePermissionsKey(name, 0);
    if (_agentPermissions.contains(nameKey)) {
        return *(_agentPermissions[nameKey].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForIP(const QHostAddress& address) const {
    NodePermissionsKey ipKey = NodePermissionsKey(address.toString(), 0);
    if (_ipPermissions.contains(ipKey)) {
        return *(_ipPermissions[ipKey].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForMAC(const QString& macAddress) const {
    NodePermissionsKey macKey = NodePermissionsKey(macAddress, 0);
    if (_macPermissions.contains(macKey)) {
        return *(_macPermissions[macKey].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForMachineFingerprint(const QUuid& machineFingerprint) const {
    NodePermissionsKey fingerprintKey = NodePermissionsKey(machineFingerprint.toString(), 0);
    if (_machineFingerprintPermissions.contains(fingerprintKey)) {
        return *(_machineFingerprintPermissions[fingerprintKey].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForGroup(const QString& groupName, QUuid rankID) const {
    NodePermissionsKey groupRankKey = NodePermissionsKey(groupName, rankID);
    if (_groupPermissions.contains(groupRankKey)) {
        return *(_groupPermissions[groupRankKey].get());
    }
    NodePermissions nullPermissions;
    nullPermissions.setAll(false);
    return nullPermissions;
}

NodePermissions DomainServerSettingsManager::getPermissionsForGroup(const QUuid& groupID, QUuid rankID) const {
    GroupByUUIDKey byUUIDKey = GroupByUUIDKey(groupID, rankID);
    if (!_groupPermissionsByUUID.contains(byUUIDKey)) {
        NodePermissions nullPermissions;
        nullPermissions.setAll(false);
        return nullPermissions;
    }
    NodePermissionsKey groupKey = _groupPermissionsByUUID[byUUIDKey]->getKey();
    return getPermissionsForGroup(groupKey.first, groupKey.second);
}

NodePermissions DomainServerSettingsManager::getForbiddensForGroup(const QString& groupName, QUuid rankID) const {
    NodePermissionsKey groupRankKey = NodePermissionsKey(groupName, rankID);
    if (_groupForbiddens.contains(groupRankKey)) {
        return *(_groupForbiddens[groupRankKey].get());
    }
    NodePermissions allForbiddens;
    allForbiddens.setAll(true);
    return allForbiddens;
}

NodePermissions DomainServerSettingsManager::getForbiddensForGroup(const QUuid& groupID, QUuid rankID) const {
    GroupByUUIDKey byUUIDKey = GroupByUUIDKey(groupID, rankID);
    if (!_groupForbiddensByUUID.contains(byUUIDKey)) {
        NodePermissions allForbiddens;
        allForbiddens.setAll(true);
        return allForbiddens;
    }

    NodePermissionsKey groupKey = _groupForbiddensByUUID[byUUIDKey]->getKey();
    return getForbiddensForGroup(groupKey.first, groupKey.second);
}

QVariant DomainServerSettingsManager::valueForKeyPath(const QString& keyPath) {
    QReadLocker locker(&_settingsLock);
    auto foundValue = _configMap.valueForKeyPath(keyPath);
    return foundValue ? *foundValue : QVariant();
}

QVariant DomainServerSettingsManager::valueOrDefaultValueForKeyPath(const QString& keyPath) {
    QReadLocker locker(&_settingsLock);
    const QVariant* foundValue = _configMap.valueForKeyPath(keyPath);

    if (foundValue) {
        return *foundValue;
    } else {
        // we don't need the settings lock anymore since we're done reading from the config map
        locker.unlock();

        int dotIndex = keyPath.indexOf('.');

        QString groupKey = keyPath.mid(0, dotIndex);
        QString settingKey = keyPath.mid(dotIndex + 1);

        foreach(const QVariant& group, _descriptionArray.toVariantList()) {
            QVariantMap groupMap = group.toMap();

            if (groupMap[DESCRIPTION_NAME_KEY].toString() == groupKey) {
                foreach(const QVariant& setting, groupMap[DESCRIPTION_SETTINGS_KEY].toList()) {
                    QVariantMap settingMap = setting.toMap();
                    if (settingMap[DESCRIPTION_NAME_KEY].toString() == settingKey) {
                        return settingMap[SETTING_DEFAULT_KEY];
                    }
                }

                return QVariant();
            }
        }
    }

    return QVariant();
}

bool DomainServerSettingsManager::handleAuthenticatedHTTPRequest(HTTPConnection *connection, const QUrl &url) {
    if (connection->requestOperation() == QNetworkAccessManager::PostOperation) {
        static const QString SETTINGS_RESTORE_PATH = "/settings/restore";

        if (url.path() == SETTINGS_PATH_JSON || url.path() == CONTENT_SETTINGS_PATH_JSON) {
            // this is a POST operation to change one or more settings
            QJsonDocument postedDocument = QJsonDocument::fromJson(connection->requestContent());
            QJsonObject postedObject = postedDocument.object();

            SettingsType endpointType = url.path() == SETTINGS_PATH_JSON ? DomainSettings : ContentSettings;

            // we recurse one level deep below each group for the appropriate setting
            bool restartRequired = recurseJSONObjectAndOverwriteSettings(postedObject, endpointType);

            // return success to the caller
            QString jsonSuccess = "{\"status\": \"success\"}";
            connection->respond(HTTPConnection::StatusCode200, jsonSuccess.toUtf8(), "application/json");

            // defer a restart to the domain-server, this gives our HTTPConnection enough time to respond
            if (restartRequired) {
                const int DOMAIN_SERVER_RESTART_TIMER_MSECS = 1000;
                QTimer::singleShot(DOMAIN_SERVER_RESTART_TIMER_MSECS, qApp, SLOT(restart()));
            } else {
                unpackPermissions();
                apiRefreshGroupInformation();
                emit updateNodePermissions();
                emit settingsUpdated();
            }

            return true;
        } else if (url.path() == SETTINGS_RESTORE_PATH) {
            // this is an JSON settings file restore, ask the HTTPConnection to parse the data
            QList<FormData> formData = connection->parseFormData();

            bool wasRestoreSuccessful = false;

            if (formData.size() > 0 && formData[0].second.size() > 0) {
                // take the posted file and convert it to a QJsonObject
                auto postedDocument = QJsonDocument::fromJson(formData[0].second);
                if (postedDocument.isObject()) {
                    wasRestoreSuccessful = restoreSettingsFromObject(postedDocument.object(), DomainSettings);
                }
            }

            if (wasRestoreSuccessful) {
                // respond with a 200 for success
                QString jsonSuccess = "{\"status\": \"success\"}";
                connection->respond(HTTPConnection::StatusCode200, jsonSuccess.toUtf8(), "application/json");

                // defer a restart to the domain-server, this gives our HTTPConnection enough time to respond
                const int DOMAIN_SERVER_RESTART_TIMER_MSECS = 1000;
                QTimer::singleShot(DOMAIN_SERVER_RESTART_TIMER_MSECS, qApp, SLOT(restart()));
            } else {
                // respond with a 400 for failure
                connection->respond(HTTPConnection::StatusCode400);
            }

            return true;
        }
    }   else if (connection->requestOperation() == QNetworkAccessManager::GetOperation) {
        static const QString SETTINGS_MENU_GROUPS_PATH = "/settings-menu-groups.json";
        static const QString SETTINGS_BACKUP_PATH = "/settings/backup.json";

        if (url.path() == SETTINGS_PATH_JSON || url.path() == CONTENT_SETTINGS_PATH_JSON) {

            // setup a JSON Object with descriptions and non-omitted settings
            const QString SETTINGS_RESPONSE_DESCRIPTION_KEY = "descriptions";
            const QString SETTINGS_RESPONSE_VALUE_KEY = "values";

            QJsonObject rootObject;

            DomainSettingsInclusion domainSettingsInclusion = (url.path() == SETTINGS_PATH_JSON)
                ? IncludeDomainSettings : NoDomainSettings;
            ContentSettingsInclusion contentSettingsInclusion = (url.path() == CONTENT_SETTINGS_PATH_JSON)
                ? IncludeContentSettings : NoContentSettings;

            rootObject[SETTINGS_RESPONSE_DESCRIPTION_KEY] = (url.path() == SETTINGS_PATH_JSON)
                ? _domainSettingsDescription : _contentSettingsDescription;

            // grab a domain settings object for all types, filtered for the right class of settings
            // and exclude default values
            rootObject[SETTINGS_RESPONSE_VALUE_KEY] = settingsResponseObjectForType("", Authenticated,
                                                                                    domainSettingsInclusion,
                                                                                    contentSettingsInclusion,
                                                                                    IncludeDefaultSettings);

            connection->respond(HTTPConnection::StatusCode200, QJsonDocument(rootObject).toJson(), "application/json");

            return true;
        } else if (url.path() == SETTINGS_MENU_GROUPS_PATH) {

            QJsonObject settings;
            for (auto & key : _settingsMenuGroups.keys()) {
                const QJsonArray& settingGroups = _settingsMenuGroups[key].toArray();
                QJsonArray groups;
                foreach (const QJsonValue& group, settingGroups) {
                    QJsonObject groupObject = group.toObject();
                    QVariant* enableKey = _configMap.valueForKeyPath(groupObject[DESCRIPTION_NAME_KEY].toString() + "." + DESCRIPTION_ENABLE_KEY);

                    if (!groupObject.contains(DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY)
                        || (groupObject[DESCRIPTION_GROUP_SHOW_ON_ENABLE_KEY].toBool() && enableKey && enableKey->toBool() )) {
                        groups.append(groupObject);
                    }
                }
                settings[key] = groups;
            }

            connection->respond(HTTPConnection::StatusCode200, QJsonDocument(settings).toJson(), "application/json");

            return true;
        } else if (url.path() == SETTINGS_BACKUP_PATH) {
            // grab the settings backup as an authenticated user
            // for the domain settings type only, excluding hidden and default values
            auto currentDomainSettingsJSON = settingsResponseObjectForType("", Authenticated, IncludeDomainSettings,
                                                                           NoContentSettings, NoDefaultSettings, ForBackup);

            // setup headers that tell the client to download the file wth a special name
            Headers downloadHeaders;
            downloadHeaders.insert("Content-Transfer-Encoding", "binary");

            // create a timestamped filename for the backup
            const QString DATETIME_FORMAT { "yyyy-MM-dd_HH-mm-ss" };
            auto backupFilename = "domain-settings_" + QDateTime::currentDateTime().toString(DATETIME_FORMAT) + ".json";

            downloadHeaders.insert("Content-Disposition",
                                   QString("attachment; filename=\"%1\"").arg(backupFilename).toLocal8Bit());

            connection->respond(HTTPConnection::StatusCode200, QJsonDocument(currentDomainSettingsJSON).toJson(),
                                "application/force-download", downloadHeaders);
        }
    }

    return false;
}

bool DomainServerSettingsManager::restoreSettingsFromObject(QJsonObject settingsToRestore, SettingsType settingsType) {

    // grab a write lock since we're about to change the settings map
    QWriteLocker locker(&_settingsLock);

    QJsonArray* filteredDescriptionArray = settingsType == DomainSettings
        ? &_domainSettingsDescription : &_contentSettingsDescription;

    // grab a copy of the current config before restore, so that we can back out if something bad happens during
    QVariantMap preRestoreConfig = _configMap.getConfig();

    bool shouldCancelRestore = false;

    // enumerate through the settings in the description
    // if we have one in the restore then use it, otherwise clear it from current settings
    foreach(const QJsonValue& descriptionGroupValue, *filteredDescriptionArray) {
        QJsonObject descriptionGroupObject = descriptionGroupValue.toObject();
        QString groupKey = descriptionGroupObject[DESCRIPTION_NAME_KEY].toString();
        QJsonArray descriptionGroupSettings = descriptionGroupObject[DESCRIPTION_SETTINGS_KEY].toArray();

        // grab the matching group from the restore so we can look at its settings
        QJsonObject restoreGroup;
        QVariantMap* configGroupMap = nullptr;

        if (groupKey.isEmpty()) {
            // this is for a setting at the root, use the full object as our restore group
            restoreGroup = settingsToRestore;

            // the variant map for this "group" is just the config map since there's no group
            configGroupMap = &_configMap.getConfig();
        } else {
            if (settingsToRestore.contains(groupKey)) {
                restoreGroup = settingsToRestore[groupKey].toObject();
            }

            // grab the variant for the group
            auto groupMapVariant = _configMap.valueForKeyPath(groupKey);

            // if it existed, double check that it is a map - any other value is unexpected and should cancel a restore
            if (groupMapVariant) {
                if (groupMapVariant->canConvert<QVariantMap>()) {
                    configGroupMap = static_cast<QVariantMap*>(groupMapVariant->data());
                } else {
                    shouldCancelRestore = true;
                    break;
                }
            }
        }

        foreach(const QJsonValue& descriptionSettingValue, descriptionGroupSettings) {

            QJsonObject descriptionSettingObject = descriptionSettingValue.toObject();

            // we'll override this setting with the default or what is in the restore as long as
            // it isn't specifically excluded from backups
            bool isBackedUpSetting = !descriptionSettingObject.contains(DESCRIPTION_BACKUP_FLAG_KEY)
                || descriptionSettingObject[DESCRIPTION_BACKUP_FLAG_KEY].toBool();

            if (isBackedUpSetting) {
                QString settingName = descriptionSettingObject[DESCRIPTION_NAME_KEY].toString();

                // check if we have a matching setting for this in the restore
                QJsonValue restoreValue;
                if (restoreGroup.contains(settingName)) {
                    restoreValue = restoreGroup[settingName];
                }

                // we should create the value for this key path in our current config map
                // if we had value in the restore file
                bool shouldCreateIfMissing = !restoreValue.isNull();

                // get a QVariant pointer to this setting in our config map
                QString fullSettingKey = !groupKey.isEmpty()
                    ? groupKey + "." + settingName : settingName;

                QVariant* variantValue = _configMap.valueForKeyPath(fullSettingKey, shouldCreateIfMissing);

                if (restoreValue.isNull()) {
                    if (variantValue && !variantValue->isNull() && configGroupMap) {
                        // we didn't have a value to restore, but there might be a value in the config map
                        // so we need to remove the value in the config map which will set it back to the default
                        qDebug() << "Removing" << fullSettingKey << "from settings since it is not in the restored JSON";
                        configGroupMap->remove(settingName);
                    }
                } else {
                    // we have a value to restore, use update setting to set it
                    // but clear the existing value first so that no merging between the restored settings
                    // and existing settings occurs
                    variantValue->clear();

                    // we might need to re-grab config group map in case it didn't exist when we looked for it before
                    // but was created by the call to valueForKeyPath before
                    if (!configGroupMap) {
                        auto groupMapVariant = _configMap.valueForKeyPath(groupKey);
                        if (groupMapVariant && groupMapVariant->canConvert<QVariantMap>()) {
                            configGroupMap = static_cast<QVariantMap*>(groupMapVariant->data());
                        } else {
                            shouldCancelRestore = true;
                            break;
                        }
                    }

                    qDebug() << "Updating setting" << fullSettingKey << "from restored JSON";

                    updateSetting(settingName, restoreValue, *configGroupMap, descriptionSettingObject);
                }
            }
        }

        if (shouldCancelRestore) {
            break;
        }
    }

    if (shouldCancelRestore) {
        // if we cancelled the restore, go back to our state before and return false
        qDebug() << "Restore cancelled, settings have not been changed";
        _configMap.getConfig() = preRestoreConfig;
        return false;
    } else {
        // restore completed, persist the new settings
        qDebug() << "Restore completed, persisting restored settings to file";

        // let go of the write lock since we're done making changes to the config map
        locker.unlock();

        persistToFile();
        return true;
    }
}

QJsonObject DomainServerSettingsManager::settingsResponseObjectForType(const QString& typeValue,
                                                                       SettingsRequestAuthentication authentication,
                                                                       DomainSettingsInclusion domainSettingsInclusion,
                                                                       ContentSettingsInclusion contentSettingsInclusion,
                                                                       DefaultSettingsInclusion defaultSettingsInclusion,
                                                                       SettingsBackupFlag settingsBackupFlag) {
    QJsonObject responseObject;

    responseObject["version"] = _descriptionVersion;  // Domain settings version number.

    if (!typeValue.isEmpty() || authentication == Authenticated) {
        // convert the string type value to a QJsonValue
        QJsonValue queryType = typeValue.isEmpty() ? QJsonValue() : QJsonValue(typeValue.toInt());

        const QString AFFECTED_TYPES_JSON_KEY = "assignment-types";

        // only enumerate the requested settings type (domain setting or content setting)
        QJsonArray* filteredDescriptionArray = &_descriptionArray;

        if (domainSettingsInclusion == IncludeDomainSettings && contentSettingsInclusion != IncludeContentSettings) {
            filteredDescriptionArray = &_domainSettingsDescription;
        } else if (contentSettingsInclusion == IncludeContentSettings && domainSettingsInclusion != IncludeDomainSettings) {
            filteredDescriptionArray = &_contentSettingsDescription;
        }

        // enumerate the groups in the potentially filtered object to find which settings to pass
        foreach(const QJsonValue& groupValue, *filteredDescriptionArray) {
            QJsonObject groupObject = groupValue.toObject();
            QString groupKey = groupObject[DESCRIPTION_NAME_KEY].toString();
            QJsonArray groupSettingsArray = groupObject[DESCRIPTION_SETTINGS_KEY].toArray();

            QJsonObject groupResponseObject;

            foreach(const QJsonValue& settingValue, groupSettingsArray) {

                const QString VALUE_HIDDEN_FLAG_KEY = "value-hidden";

                QJsonObject settingObject = settingValue.toObject();

                // consider this setting as long as it isn't hidden
                // and either this isn't for a backup or it's a value included in backups
                bool includedInBackups = !settingObject.contains(DESCRIPTION_BACKUP_FLAG_KEY)
                    || settingObject[DESCRIPTION_BACKUP_FLAG_KEY].toBool();

                if (!settingObject[VALUE_HIDDEN_FLAG_KEY].toBool() && (settingsBackupFlag != ForBackup || includedInBackups)) {
                    QJsonArray affectedTypesArray = settingObject[AFFECTED_TYPES_JSON_KEY].toArray();
                    if (affectedTypesArray.isEmpty()) {
                        affectedTypesArray = groupObject[AFFECTED_TYPES_JSON_KEY].toArray();
                    }

                    if (affectedTypesArray.contains(queryType) ||
                        (queryType.isNull() && authentication == Authenticated)) {
                        QString settingName = settingObject[DESCRIPTION_NAME_KEY].toString();

                        // we need to check if the settings map has a value for this setting
                        QVariant variantValue;

                        if (!groupKey.isEmpty()) {
                             QVariant settingsMapGroupValue = valueForKeyPath(groupKey);

                            if (!settingsMapGroupValue.isNull()) {
                                variantValue = settingsMapGroupValue.toMap().value(settingName);
                            }
                        } else {
                            variantValue = valueForKeyPath(settingName);
                        }

                        // final check for inclusion
                        // either we include default values or we don't but this isn't a default value
                        if ((defaultSettingsInclusion == IncludeDefaultSettings) || variantValue.isValid()) {
                            QJsonValue result;

                            if (!variantValue.isValid()) {
                                // no value for this setting, pass the default
                                if (settingObject.contains(SETTING_DEFAULT_KEY)) {
                                    result = settingObject[SETTING_DEFAULT_KEY];
                                } else {
                                    // users are allowed not to provide a default for string values
                                    // if so we set to the empty string
                                    result = QString("");
                                }
                            } else {
                                result = QJsonValue::fromVariant(variantValue);
                            }

                            if (!groupKey.isEmpty()) {
                                // this belongs in the group object
                                groupResponseObject[settingName] = result;
                            } else {
                                // this is a value that should be at the root
                                responseObject[settingName] = result;
                            }
                        }
                    }
                }
            }

            if (!groupKey.isEmpty() && !groupResponseObject.isEmpty()) {
                // set this group's object to the constructed object
                responseObject[groupKey] = groupResponseObject;
            }
        }
    }

    // add 'derived' values used primarily for UI

    const QString X509_CERTIFICATE_OPTION = "oauth.cert";

    QString certPath = valueForKeyPath(X509_CERTIFICATE_OPTION).toString();
    if (!certPath.isEmpty()) {
        // the user wants to use the following cert and key for HTTPS
        // this is used for Oauth callbacks when authorizing users against a data server
        // let's make sure we can load the key and certificate

        qDebug() << "Reading certificate file at" << certPath << "for HTTPS.";

        QFile certFile(certPath);
        certFile.open(QIODevice::ReadOnly);

        QSslCertificate sslCertificate(&certFile);
        QString digest = sslCertificate.digest().toHex(':');
        auto groupObject = responseObject["oauth"].toObject();
        groupObject["cert-fingerprint"] = digest;
        responseObject["oauth"] = groupObject;
    }

    return responseObject;
}

void DomainServerSettingsManager::updateSetting(const QString& key, const QJsonValue& newValue, QVariantMap& settingMap,
                                                const QJsonObject& settingDescription) {
    if (newValue.isString()) {
        if (newValue.toString().isEmpty()) {
            // this is an empty value, clear it in settings variant so the default is sent
            settingMap.remove(key);
        } else {
            // make sure the resulting json value has the right type
            QString settingType = settingDescription[SETTING_DESCRIPTION_TYPE_KEY].toString();
            const QString INPUT_DOUBLE_TYPE = "double";
            const QString INPUT_INTEGER_TYPE = "int";

            if (settingType == INPUT_DOUBLE_TYPE) {
                settingMap[key] = newValue.toString().toDouble();
            } else if (settingType == INPUT_INTEGER_TYPE) {
                settingMap[key] = newValue.toString().toInt();
            } else {
                QString sanitizedValue = newValue.toString();

                // we perform special handling for viewpoints here
                // we do not want them to be prepended with a slash
                if (key == SETTINGS_VIEWPOINT_KEY && !sanitizedValue.startsWith('/')) {
                    sanitizedValue.prepend('/');
                }

                settingMap[key] = sanitizedValue;
            }
        }
    } else if (newValue.isDouble()) {
        settingMap[key] = newValue.toDouble();
    } else if (newValue.isBool()) {
        settingMap[key] = newValue.toBool();
    } else if (newValue.isObject()) {
        if (!settingMap.contains(key)) {
            // we don't have a map below this key yet, so set it up now
            settingMap[key] = QVariantMap();
        }

        QVariant& possibleMap = settingMap[key];

        if (!possibleMap.canConvert(QMetaType::QVariantMap)) {
            // if this isn't a map then we need to make it one, otherwise we're about to crash
            qDebug() << "Value at" << key << "was not the expected QVariantMap while updating DS settings"
                << "- removing existing value and making it a QVariantMap";
            possibleMap = QVariantMap();
        }

        QVariantMap& thisMap = *reinterpret_cast<QVariantMap*>(possibleMap.data());
        foreach(const QString childKey, newValue.toObject().keys()) {

            QJsonObject childDescriptionObject = settingDescription;

            // is this the key? if so we have the description already
            if (key != settingDescription[DESCRIPTION_NAME_KEY].toString()) {
                // otherwise find the description object for this childKey under columns
                foreach(const QJsonValue& column, settingDescription[DESCRIPTION_COLUMNS_KEY].toArray()) {
                    if (column.isObject()) {
                        QJsonObject thisDescription = column.toObject();
                        if (thisDescription[DESCRIPTION_NAME_KEY] == childKey) {
                            childDescriptionObject = column.toObject();
                            break;
                        }
                    }
                }
            }

            QString sanitizedKey = childKey;

            if (key == SETTINGS_PATHS_KEY && !sanitizedKey.startsWith('/')) {
                // We perform special handling for paths here.
                // If we got sent a path without a leading slash then we add it.
                sanitizedKey.prepend("/");
            }

            updateSetting(sanitizedKey, newValue.toObject()[childKey], thisMap, childDescriptionObject);
        }

        if (settingMap[key].toMap().isEmpty()) {
            // we've cleared all of the settings below this value, so remove this one too
            settingMap.remove(key);
        }
    } else if (newValue.isArray()) {
        // we just assume array is replacement
        // TODO: we still need to recurse here with the description in case values in the array have special types
        settingMap[key] = newValue.toArray().toVariantList();
    }

    sortPermissions();
}

QJsonObject DomainServerSettingsManager::settingDescriptionFromGroup(const QJsonObject& groupObject,
                                                                     const QString& settingName) {
    foreach(const QJsonValue& settingValue, groupObject[DESCRIPTION_SETTINGS_KEY].toArray()) {
        QJsonObject settingObject = settingValue.toObject();
        if (settingObject[DESCRIPTION_NAME_KEY].toString() == settingName) {
            return settingObject;
        }
    }

    return QJsonObject();
}

bool DomainServerSettingsManager::recurseJSONObjectAndOverwriteSettings(const QJsonObject& postedSettingsObject,
                                                                        SettingsType settingsType) {

    // take a write lock since we're about to overwrite settings in the config map
    QWriteLocker locker(&_settingsLock);

    QJsonObject postedObject(postedSettingsObject);

    static const QString SECURITY_ROOT_KEY = "security";
    static const QString AC_SUBNET_WHITELIST_KEY = "ac_subnet_whitelist";
    static const QString BROADCASTING_KEY = "broadcasting";
    static const QString WIZARD_KEY = "wizard";
    static const QString DESCRIPTION_ROOT_KEY = "descriptors";
    static const QString OAUTH_ROOT_KEY = "oauth";
    static const QString OAUTH_KEY_CONTENTS = "key-contents";
    static const QString OAUTH_CERT_CONTENTS = "cert-contents";
    static const QString OAUTH_CERT_PATH = "cert";
    static const QString OAUTH_KEY_PASSPHRASE = "key-passphrase";
    static const QString OAUTH_KEY_PATH = "key";

    auto& settingsVariant = _configMap.getConfig();
    bool needRestart = false;

    auto& filteredDescriptionArray = settingsType == DomainSettings ? _domainSettingsDescription : _contentSettingsDescription;

    auto oauthObject = postedObject[OAUTH_ROOT_KEY].toObject();
    if (oauthObject.contains(OAUTH_CERT_CONTENTS)) {
        QSslCertificate cert(oauthObject[OAUTH_CERT_CONTENTS].toString().toUtf8());
        if (!cert.isNull()) {
            static const QString CERT_FILE_NAME = "certificate.crt";
            auto certPath = PathUtils::getAppDataFilePath(CERT_FILE_NAME);
            QFile file(certPath);
            if (file.open(QFile::WriteOnly)) {
                file.write(cert.toPem());
                file.close();
            }
            oauthObject[OAUTH_CERT_PATH] = certPath;
        }
        oauthObject.remove(OAUTH_CERT_CONTENTS);
    }
    if (oauthObject.contains(OAUTH_KEY_CONTENTS)) {
        QString keyPassphraseString = oauthObject[OAUTH_KEY_PASSPHRASE].toString();
        QSslKey key(oauthObject[OAUTH_KEY_CONTENTS].toString().toUtf8(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, keyPassphraseString.toUtf8());
        if (!key.isNull()) {
            static const QString KEY_FILE_NAME = "certificate.key";
            auto keyPath = PathUtils::getAppDataFilePath(KEY_FILE_NAME);
            QFile file(keyPath);
            if (file.open(QFile::WriteOnly)) {
                file.write(key.toPem());
                file.close();
                file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            }
            oauthObject[OAUTH_KEY_PATH] = keyPath;
        }
        oauthObject.remove(OAUTH_KEY_CONTENTS);
    }

    postedObject[OAUTH_ROOT_KEY] = oauthObject;

    // Iterate on the setting groups
    foreach(const QString& rootKey, postedObject.keys()) {
        const QJsonValue& rootValue = postedObject[rootKey];

        if (!settingsVariant.contains(rootKey)) {
            // we don't have a map below this key yet, so set it up now
            settingsVariant[rootKey] = QVariantMap();
        }

        QVariantMap* thisMap = &settingsVariant;

        QJsonObject groupDescriptionObject;

        // we need to check the description array to see if this is a root setting or a group setting
        foreach(const QJsonValue& groupValue, filteredDescriptionArray) {
            if (groupValue.toObject()[DESCRIPTION_NAME_KEY] == rootKey) {
                // we matched a group - keep this since we'll use it below to update the settings
                groupDescriptionObject = groupValue.toObject();

                // change the map we will update to be the map for this group
                thisMap = reinterpret_cast<QVariantMap*>(settingsVariant[rootKey].data());

                break;
            }
        }

        if (groupDescriptionObject.isEmpty()) {
            // this is a root value, so we can call updateSetting for it directly
            // first we need to find our description value for it

            QJsonObject matchingDescriptionObject;

            foreach(const QJsonValue& groupValue, _descriptionArray) {
                // find groups with root values (they don't have a group name)
                QJsonObject groupObject = groupValue.toObject();

                if (!groupObject.contains(DESCRIPTION_NAME_KEY)) {

                    // this is a group with root values - check if our setting is in here
                    matchingDescriptionObject = settingDescriptionFromGroup(groupObject, rootKey);

                    if (!matchingDescriptionObject.isEmpty()) {
                        break;
                    }
                }
            }

            if (!matchingDescriptionObject.isEmpty()) {
                updateSetting(rootKey, rootValue, *thisMap, matchingDescriptionObject);

                if (rootKey != SECURITY_ROOT_KEY && rootKey != BROADCASTING_KEY &&
                    rootKey != SETTINGS_PATHS_KEY && rootKey != WIZARD_KEY) {
                    needRestart = true;
                }
            } else {
                qDebug() << "Setting for root key" << rootKey << "does not exist - cannot update setting.";
            }
        } else {
            // this is a group - iterate on the settings in the group
            foreach(const QString& settingKey, rootValue.toObject().keys()) {
                // make sure this particular setting exists and we have a description object for it
                QJsonObject matchingDescriptionObject = settingDescriptionFromGroup(groupDescriptionObject, settingKey);

                // if we matched the setting then update the value
                if (!matchingDescriptionObject.isEmpty()) {
                    const QJsonValue& settingValue = rootValue.toObject()[settingKey];
                    updateSetting(settingKey, settingValue, *thisMap, matchingDescriptionObject);

                    if ((rootKey != SECURITY_ROOT_KEY && rootKey != BROADCASTING_KEY &&
                         rootKey != DESCRIPTION_ROOT_KEY && rootKey != WIZARD_KEY) ||
                        settingKey == AC_SUBNET_WHITELIST_KEY) {
                        needRestart = true;
                    }
                } else {
                    qDebug() << "Could not find description for setting" << settingKey << "in group" << rootKey <<
                        "- cannot update setting.";
                }
            }
        }

        if (settingsVariant[rootKey].toMap().empty()) {
            // we've cleared all of the settings below this value, so remove this one too
            settingsVariant.remove(rootKey);
        }
    }

    // we're done making changes to the config map, let go of our read lock
    locker.unlock();

    // store whatever the current config map is to file
    persistToFile();

    return needRestart;
}

// Compare two members of a permissions list
bool permissionVariantLessThan(const QVariant &v1, const QVariant &v2) {
    if (!v1.canConvert(QMetaType::QVariantMap) ||
        !v2.canConvert(QMetaType::QVariantMap)) {
        return v1.toString() < v2.toString();
    }
    QVariantMap m1 = v1.toMap();
    QVariantMap m2 = v2.toMap();

    if (!m1.contains("permissions_id") ||
        !m2.contains("permissions_id")) {
        return v1.toString() < v2.toString();
    }

    if (m1.contains("rank_order") && m2.contains("rank_order") &&
        m1["permissions_id"].toString() == m2["permissions_id"].toString()) {
        return m1["rank_order"].toInt() < m2["rank_order"].toInt();
    }

    return m1["permissions_id"].toString() < m2["permissions_id"].toString();
}

void DomainServerSettingsManager::sortPermissions() {
    // take a write lock since we're about to change the config map data
    QWriteLocker locker(&_settingsLock);

    // sort the permission-names
    QVariant* standardPermissions = _configMap.valueForKeyPath(AGENT_STANDARD_PERMISSIONS_KEYPATH);
    if (standardPermissions && standardPermissions->canConvert(QMetaType::QVariantList)) {
        QList<QVariant>* standardPermissionsList = reinterpret_cast<QVariantList*>(standardPermissions);
        std::sort((*standardPermissionsList).begin(), (*standardPermissionsList).end(), permissionVariantLessThan);
    }
    QVariant* permissions = _configMap.valueForKeyPath(AGENT_PERMISSIONS_KEYPATH);
    if (permissions && permissions->canConvert(QMetaType::QVariantList)) {
        QList<QVariant>* permissionsList = reinterpret_cast<QVariantList*>(permissions);
        std::sort((*permissionsList).begin(), (*permissionsList).end(), permissionVariantLessThan);
    }
    QVariant* groupPermissions = _configMap.valueForKeyPath(GROUP_PERMISSIONS_KEYPATH);
    if (groupPermissions && groupPermissions->canConvert(QMetaType::QVariantList)) {
        QList<QVariant>* permissionsList = reinterpret_cast<QVariantList*>(groupPermissions);
        std::sort((*permissionsList).begin(), (*permissionsList).end(), permissionVariantLessThan);
    }
    QVariant* forbiddenPermissions = _configMap.valueForKeyPath(GROUP_FORBIDDENS_KEYPATH);
    if (forbiddenPermissions && forbiddenPermissions->canConvert(QMetaType::QVariantList)) {
        QList<QVariant>* permissionsList = reinterpret_cast<QVariantList*>(forbiddenPermissions);
        std::sort((*permissionsList).begin(), (*permissionsList).end(), permissionVariantLessThan);
    }
}

void DomainServerSettingsManager::persistToFile() {
    QString settingsFilename = _configMap.getUserConfigFilename();
    QDir settingsDir = QFileInfo(settingsFilename).dir();
    if (!settingsDir.exists() && !settingsDir.mkpath(".")) {
        // If the path already exists when the `mkpath` method is
        // called, it will return true. It will only return false if the
        // path doesn't exist after the call returns.
        qCritical("Could not create the settings file parent directory. Unable to persist settings.");
        QWriteLocker locker(&_settingsLock);
        _configMap.loadConfig();
        return;
    }
    QSaveFile settingsFile(settingsFilename);
    if (!settingsFile.open(QIODevice::WriteOnly)) {
        qCritical("Could not open the JSON settings file. Unable to persist settings.");
        QWriteLocker locker(&_settingsLock);
        _configMap.loadConfig();
        return;
    }

    sortPermissions();

    QVariantMap conf;
    {
        QReadLocker locker(&_settingsLock);
        conf = _configMap.getConfig();
    }
    QByteArray json = QJsonDocument::fromVariant(conf).toJson();
    if (settingsFile.write(json) == -1) {
        qCritical("Could not write to JSON settings file. Unable to persist settings.");
        QWriteLocker locker(&_settingsLock);
        _configMap.loadConfig();
        return;
    }
    if (!settingsFile.commit()) {
        qCritical("Could not commit writes to JSON settings file. Unable to persist settings.");
        QWriteLocker locker(&_settingsLock);
        _configMap.loadConfig();
        return; // defend against future code
    }

    QFile(settingsFilename).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QStringList DomainServerSettingsManager::getAllKnownGroupNames() {
    // extract all the group names from the group-permissions and group-forbiddens settings
    QSet<QString> result;

    for (const auto& entry : _groupPermissions.get()) {
        result += entry.first.first;
    }

    for (const auto& entry : _groupForbiddens.get()) {
        result += entry.first.first;
    }

    return result.toList();
}

bool DomainServerSettingsManager::setGroupID(const QString& groupName, const QUuid& groupID) {
    bool changed = false;
    _groupIDs[groupName.toLower()] = groupID;
    _groupNames[groupID] = groupName;


    for (const auto& entry : _groupPermissions.get()) {
        auto& perms = entry.second;
        if (perms->getID().toLower() == groupName.toLower() && !perms->isGroup()) {
            changed = true;
            perms->setGroupID(groupID);
        }
    }

    for (const auto& entry : _groupForbiddens.get()) {
        auto& perms = entry.second;
        if (perms->getID().toLower() == groupName.toLower() && !perms->isGroup()) {
            changed = true;
            perms->setGroupID(groupID);
        }
    }

    return changed;
}

void DomainServerSettingsManager::apiRefreshGroupInformation() {
    if (!DependencyManager::get<AccountManager>()->hasAuthEndpoint()) {
        // can't yet.
        return;
    }

    bool changed = false;

    QStringList groupNames = getAllKnownGroupNames();
    foreach (QString groupName, groupNames) {
        QString lowerGroupName = groupName.toLower();
        if (lowerGroupName.startsWith(DOMAIN_GROUP_CHAR)) {
            // Ignore domain groups. (Assumption: metaverse group names can't start with a "@".)
            return;
        }
        if (_groupIDs.contains(lowerGroupName)) {
            // we already know about this one.  recall setGroupID in case the group has been
            // added to another section (the same group is found in both groups and blacklists).
            changed = setGroupID(groupName, _groupIDs[lowerGroupName]);
            continue;
        }
        apiGetGroupID(groupName);
    }

    foreach (QUuid groupID, _groupNames.keys()) {
        apiGetGroupRanks(groupID);
    }

    changed |= ensurePermissionsForGroupRanks();

    if (changed) {
        packPermissions();
    }

    unpackPermissions();
}

void DomainServerSettingsManager::apiGetGroupID(const QString& groupName) {
    JSONCallbackParameters callbackParams;
    callbackParams.callbackReceiver = this;
    callbackParams.jsonCallbackMethod = "apiGetGroupIDJSONCallback";
    callbackParams.errorCallbackMethod = "apiGetGroupIDErrorCallback";

    const QString GET_GROUP_ID_PATH = "/api/v1/groups/names/%1";
    DependencyManager::get<AccountManager>()->sendRequest(GET_GROUP_ID_PATH.arg(groupName),
                                                          AccountManagerAuth::Required,
                                                          QNetworkAccessManager::GetOperation, callbackParams);
}

void DomainServerSettingsManager::apiGetGroupIDJSONCallback(QNetworkReply* requestReply) {
    // {
    //     "data":{
    //         "groups":[{
    //             "description":null,
    //             "id":"fd55479a-265d-4990-854e-3d04214ad1b0",
    //             "is_list":false,
    //             "membership":{
    //                 "permissions":{
    //                     "custom_1=":false,
    //                     "custom_2=":false,
    //                     "custom_3=":false,
    //                     "custom_4=":false,
    //                     "del_group=":true,
    //                     "invite_member=":true,
    //                     "kick_member=":true,
    //                     "list_members=":true,
    //                     "mv_group=":true,
    //                     "query_members=":true,
    //                     "rank_member=":true
    //                 },
    //                 "rank":{
    //                     "name=":"owner",
    //                     "order=":0
    //                 }
    //             },
    //             "name":"Blerg Blah"
    //         }]
    //     },
    //     "status":"success"
    // }
    QJsonObject jsonObject = QJsonDocument::fromJson(requestReply->readAll()).object();
    if (jsonObject["status"].toString() == "success") {
        QJsonArray groups = jsonObject["data"].toObject()["groups"].toArray();
        for (int i = 0; i < groups.size(); i++) {
            QJsonObject group = groups.at(i).toObject();
            QString groupName = group["name"].toString();
            QUuid groupID = QUuid(group["id"].toString());

            bool changed = setGroupID(groupName, groupID);
            if (changed) {
                packPermissions();
                apiGetGroupRanks(groupID);
            }
        }
    } else {
        qDebug() << "getGroupID api call returned:" << QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    }
}

void DomainServerSettingsManager::apiGetGroupIDErrorCallback(QNetworkReply* requestReply) {
    qDebug() << "******************** getGroupID api call failed:" << requestReply->error();
}

void DomainServerSettingsManager::apiGetGroupRanks(const QUuid& groupID) {
    JSONCallbackParameters callbackParams;
    callbackParams.callbackReceiver = this;
    callbackParams.jsonCallbackMethod = "apiGetGroupRanksJSONCallback";
    callbackParams.errorCallbackMethod = "apiGetGroupRanksErrorCallback";

    const QString GET_GROUP_RANKS_PATH = "/api/v1/groups/%1/ranks";
    DependencyManager::get<AccountManager>()->sendRequest(GET_GROUP_RANKS_PATH.arg(groupID.toString().mid(1,36)),
                                                          AccountManagerAuth::Required,
                                                          QNetworkAccessManager::GetOperation, callbackParams);
}

void DomainServerSettingsManager::apiGetGroupRanksJSONCallback(QNetworkReply* requestReply) {
    // {
    //     "data":{
    //         "groups":{
    //             "d3500f49-0655-4b1b-9846-ff8dd1b03351":{
    //                 "members_count":1,
    //                 "ranks":[
    //                     {
    //                         "id":"7979b774-e7f8-436c-9df1-912f1019f32f",
    //                         "members_count":1,
    //                         "name":"owner",
    //                         "order":0,
    //                         "permissions":{
    //                             "custom_1":false,
    //                             "custom_2":false,
    //                             "custom_3":false,
    //                             "custom_4":false,
    //                             "edit_group":true,
    //                             "edit_member":true,
    //                             "edit_rank":true,
    //                             "list_members":true,
    //                             "list_permissions":true,
    //                             "list_ranks":true,
    //                             "query_member":true
    //                         }
    //                     }
    //                 ]
    //             }
    //         }
    //     },"status":"success"
    // }

    bool changed = false;
    QJsonObject jsonObject = QJsonDocument::fromJson(requestReply->readAll()).object();

    if (jsonObject["status"].toString() == "success") {
        QJsonObject groups = jsonObject["data"].toObject()["groups"].toObject();
        foreach (auto groupID, groups.keys()) {
            QJsonObject group = groups[groupID].toObject();
            QJsonArray ranks = group["ranks"].toArray();

            QHash<QUuid, GroupRank>& ranksForGroup = _groupRanks[groupID];
            QHash<QUuid, bool> idsFromThisUpdate;

            for (int rankIndex = 0; rankIndex < ranks.size(); rankIndex++) {
                QJsonObject rank = ranks[rankIndex].toObject();

                QUuid rankID = QUuid(rank["id"].toString());
                int rankOrder = rank["order"].toInt();
                QString rankName = rank["name"].toString();
                int rankMembersCount = rank["members_count"].toInt();

                GroupRank groupRank(rankID, rankOrder, rankName, rankMembersCount);

                if (ranksForGroup[rankID] != groupRank) {
                    ranksForGroup[rankID] = groupRank;
                    changed = true;
                }

                idsFromThisUpdate[rankID] = true;
            }

            // clean up any that went away
            foreach (QUuid rankID, ranksForGroup.keys()) {
                if (!idsFromThisUpdate.contains(rankID)) {
                    ranksForGroup.remove(rankID);
                }
            }
        }

        changed |= ensurePermissionsForGroupRanks();
        if (changed) {
            packPermissions();
        }
    } else {
        qDebug() << "getGroupRanks api call returned:" << QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    }
}

void DomainServerSettingsManager::apiGetGroupRanksErrorCallback(QNetworkReply* requestReply) {
    qDebug() << "******************** getGroupRanks api call failed:" << requestReply->error();
}

void DomainServerSettingsManager::recordGroupMembership(const QString& name, const QUuid groupID, QUuid rankID) {
    if (rankID != QUuid()) {
        _groupMembership[name.toLower()][groupID] = rankID;
    } else {
        _groupMembership[name.toLower()].remove(groupID);
    }
}

QUuid DomainServerSettingsManager::isGroupMember(const QString& name, const QUuid& groupID) {
    const QHash<QUuid, QUuid>& groupsForName = _groupMembership[name.toLower()];
    if (groupsForName.contains(groupID)) {
        return groupsForName[groupID];
    }
    return QUuid();
}

QList<QUuid> DomainServerSettingsManager::getGroupIDs() {
    QSet<QUuid> result;
    foreach (NodePermissionsKey groupKey, _groupPermissions.keys()) {
        if (_groupPermissions[groupKey]->isGroup()) {
            result += _groupPermissions[groupKey]->getGroupID();
        }
    }
    return result.toList();
}

QList<QUuid> DomainServerSettingsManager::getBlacklistGroupIDs() {
    QSet<QUuid> result;
    foreach (NodePermissionsKey groupKey, _groupForbiddens.keys()) {
        if (_groupForbiddens[groupKey]->isGroup()) {
            result += _groupForbiddens[groupKey]->getGroupID();
        }
    }
    return result.toList();
}

QStringList DomainServerSettingsManager::getDomainServerGroupNames() {
    // All names as listed in the domain server settings; both metaverse groups and domain groups
    QSet<QString> result;
    foreach(NodePermissionsKey groupKey, _groupPermissions.keys()) {
        result += _groupPermissions[groupKey]->getID();
    }
    return result.toList();
}

QStringList DomainServerSettingsManager::getDomainServerBlacklistGroupNames() {
    // All names as listed in the domain server settings; not necessarily mnetaverse groups.
    QSet<QString> result;
    foreach (NodePermissionsKey groupKey, _groupForbiddens.keys()) {
        result += _groupForbiddens[groupKey]->getID();
    }
    return result.toList();
}

void DomainServerSettingsManager::debugDumpGroupsState() {
    qDebug() << "--------- GROUPS ---------";

    qDebug() << "_groupPermissions:";
    foreach (NodePermissionsKey groupKey, _groupPermissions.keys()) {
        NodePermissionsPointer perms = _groupPermissions[groupKey];
        qDebug() << "|  " << groupKey << perms;
    }

    qDebug() << "_groupForbiddens:";
    foreach (NodePermissionsKey groupKey, _groupForbiddens.keys()) {
        NodePermissionsPointer perms = _groupForbiddens[groupKey];
        qDebug() << "|  " << groupKey << perms;
    }

    qDebug() << "_groupIDs:";
    foreach (QString groupName, _groupIDs.keys()) {
        qDebug() << "|  " << groupName << "==>" << _groupIDs[groupName.toLower()];
    }

    qDebug() << "_groupNames:";
    foreach (QUuid groupID, _groupNames.keys()) {
        qDebug() << "|  " << groupID << "==>" << _groupNames[groupID];
    }

    qDebug() << "_groupRanks:";
    foreach (QUuid groupID, _groupRanks.keys()) {
        QHash<QUuid, GroupRank>& ranksForGroup = _groupRanks[groupID];
        qDebug() << "|  " << groupID;
        foreach (QUuid rankID, ranksForGroup.keys()) {
            QString rankName = ranksForGroup[rankID].name;
            qDebug() << "|      " << rankID << rankName;
        }
    }

    qDebug() << "_groupMembership";
    foreach (QString userName, _groupMembership.keys()) {
        QHash<QUuid, QUuid>& groupsForUser = _groupMembership[userName.toLower()];
        QString line = "";
        foreach (QUuid groupID, groupsForUser.keys()) {
            line += " g=" + groupID.toString() + ",r=" + groupsForUser[groupID].toString();
        }
        qDebug() << "|  " << userName << line;
    }
}
