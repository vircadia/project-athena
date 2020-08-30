//
//  NodePermissions.cpp
//  libraries/networking/src/
//
//  Created by Seth Alves on 2016-6-1.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "NodePermissions.h"

#include <QtCore/QtGlobal>
#include <QDataStream>
#include <QtCore/QDebug>



size_t std::hash<NodePermissionsKey>::operator()(const NodePermissionsKey& key) const {
    size_t result = qHash(key.first);
    result <<= sizeof(size_t) / 2;

#if (QT_POINTER_SIZE == 8)
    const uint MASK = 0x00FF;
#else
    const uint MASK = 0xFFFF;
#endif

    result |= (qHash(key.second) & MASK);
    return result;
}


NodePermissionsKey NodePermissions::standardNameLocalhost = NodePermissionsKey("localhost", 0);
NodePermissionsKey NodePermissions::standardNameLoggedIn = NodePermissionsKey("logged-in", 0);
NodePermissionsKey NodePermissions::standardNameAnonymous = NodePermissionsKey("anonymous", 0);
NodePermissionsKey NodePermissions::standardNameFriends = NodePermissionsKey("friends", 0);

QStringList NodePermissions::standardNames = QList<QString>()
    << NodePermissions::standardNameLocalhost.first
    << NodePermissions::standardNameLoggedIn.first
    << NodePermissions::standardNameAnonymous.first
    << NodePermissions::standardNameFriends.first;

NodePermissions::NodePermissions(QMap<QString, QVariant> perms) {
    _id = perms["permissions_id"].toString().toLower();
    if (perms.contains("group_id")) {
        _groupID = perms["group_id"].toUuid();
        if (!_groupID.isNull()) {
            _groupIDSet = true;
        }
    }
    if (perms.contains("rank_id")) {
        _rankID = QUuid(perms["rank_id"].toString());
    }

    permissions = NodePermissions::Permissions();
    permissions |= perms["id_can_connect"].toBool() ? Permission::canConnectToDomain : Permission::none;
    permissions |= perms["id_can_rez_avatar_entities"].toBool() ? Permission::canRezAvatarEntities : Permission::none;
    permissions |= perms["id_can_adjust_locks"].toBool() ? Permission::canAdjustLocks : Permission::none;
    permissions |= perms["id_can_rez"].toBool() ? Permission::canRezPermanentEntities : Permission::none;
    permissions |= perms["id_can_rez_tmp"].toBool() ? Permission::canRezTemporaryEntities : Permission::none;
    permissions |= perms["id_can_write_to_asset_server"].toBool() ? Permission::canWriteToAssetServer : Permission::none;
    permissions |= perms["id_can_connect_past_max_capacity"].toBool() ?
        Permission::canConnectPastMaxCapacity : Permission::none;
    permissions |= perms["id_can_kick"].toBool() ? Permission::canKick : Permission::none;
    permissions |= perms["id_can_replace_content"].toBool() ? Permission::canReplaceDomainContent : Permission::none;
    permissions |= perms["id_can_get_and_set_private_user_data"].toBool() ? 
        Permission::canGetAndSetPrivateUserData : Permission::none;
}

QVariant NodePermissions::toVariant(QHash<QUuid, GroupRank> groupRanks) {
    QMap<QString, QVariant> values;
    values["permissions_id"] = _id;
    if (_groupIDSet) {
        values["group_id"] = _groupID;

        if (!_rankID.isNull()) {
            values["rank_id"] = _rankID;
        }

        if (groupRanks.contains(_rankID)) {
            values["rank_name"] = groupRanks[_rankID].name;
            values["rank_order"] = groupRanks[_rankID].order;
        }
    }
    values["id_can_connect"] = can(Permission::canConnectToDomain);
    values["id_can_rez_avatar_entities"] = can(Permission::canRezAvatarEntities);
    values["id_can_adjust_locks"] = can(Permission::canAdjustLocks);
    values["id_can_rez"] = can(Permission::canRezPermanentEntities);
    values["id_can_rez_tmp"] = can(Permission::canRezTemporaryEntities);
    values["id_can_write_to_asset_server"] = can(Permission::canWriteToAssetServer);
    values["id_can_connect_past_max_capacity"] = can(Permission::canConnectPastMaxCapacity);
    values["id_can_kick"] = can(Permission::canKick);
    values["id_can_replace_content"] = can(Permission::canReplaceDomainContent);
    values["id_can_get_and_set_private_user_data"] = can(Permission::canGetAndSetPrivateUserData);
    return QVariant(values);
}

void NodePermissions::setAll(bool value) {
    permissions = NodePermissions::Permissions();
    if (value) {
        permissions = ~permissions;
    }
}

NodePermissions& NodePermissions::operator|=(const NodePermissions& rhs) {
    permissions |= rhs.permissions;
    return *this;
}

NodePermissions& NodePermissions::operator&=(const NodePermissions& rhs) {
    permissions &= rhs.permissions;
    return *this;
}

NodePermissions NodePermissions::operator~() {
    NodePermissions result = *this;
    result.permissions = ~permissions;
    return result;
}

QDataStream& operator<<(QDataStream& out, const NodePermissions& perms) {
    out << (uint)perms.permissions;
    return out;
}

QDataStream& operator>>(QDataStream& in, NodePermissions& perms) {
    uint permissionsInt;
    in >> permissionsInt;
    perms.permissions = (NodePermissions::Permissions)permissionsInt;
    return in;
}

QDebug operator<<(QDebug debug, const NodePermissions& perms) {
    debug.nospace() << "[permissions: " << perms.getID() << "/" << perms.getVerifiedUserName() << " -- ";
    debug.nospace() << "rank=" << perms.getRankID()
                    << ", groupID=" << perms.getGroupID() << "/" << (perms.isGroup() ? "y" : "n");
    if (perms.can(NodePermissions::Permission::canConnectToDomain)) {
        debug << " connect";
    }
    if (perms.can(NodePermissions::Permission::canRezAvatarEntities)) {
        debug << " rez-avatar-entities";
    }
    if (perms.can(NodePermissions::Permission::canAdjustLocks)) {
        debug << " locks";
    }
    if (perms.can(NodePermissions::Permission::canRezPermanentEntities)) {
        debug << " rez";
    }
    if (perms.can(NodePermissions::Permission::canRezTemporaryEntities)) {
        debug << " rez-tmp";
    }
    if (perms.can(NodePermissions::Permission::canWriteToAssetServer)) {
        debug << " asset-server";
    }
    if (perms.can(NodePermissions::Permission::canConnectPastMaxCapacity)) {
        debug << " ignore-max-cap";
    }
    if (perms.can(NodePermissions::Permission::canKick)) {
        debug << " kick";
    }
    if (perms.can(NodePermissions::Permission::canReplaceDomainContent)) {
        debug << " can_replace_content";
    }
    if (perms.can(NodePermissions::Permission::canGetAndSetPrivateUserData)) {
        debug << " get-and-set-private-user-data";
    }
    debug.nospace() << "]";
    return debug.nospace();
}

QDebug operator<<(QDebug debug, const NodePermissionsPointer& perms) {
    if (perms) {
        return operator<<(debug, *perms.get());
    }
    debug.nospace() << "[permissions: null]";
    return debug.nospace();
}
