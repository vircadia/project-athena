//
//  NodePermissions.h
//  libraries/networking/src/
//
//  Created by Seth Alves on 2016-6-1.
//  Copyright 2016 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_NodePermissions_h
#define hifi_NodePermissions_h

#include <memory>
#include <unordered_map>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QUuid>
#include <QHash>
#include <utility>
#include "GroupRank.h"

class NodePermissions;
using NodePermissionsPointer = std::shared_ptr<NodePermissions>;
using NodePermissionsKey = std::pair<QString, QUuid>; // name, rankID
using NodePermissionsKeyList = QList<QPair<QString, QUuid>>;

namespace std {
    template<>
    struct hash<NodePermissionsKey> {    
        size_t operator()(const NodePermissionsKey& key) const;
    };
}

class NodePermissions {
public:
    NodePermissions() { _id = QUuid::createUuid().toString(); _rankID = QUuid(); }
    NodePermissions(const QString& name) { _id = name.toLower(); _rankID = QUuid(); }
    NodePermissions(const NodePermissionsKey& key) { _id = key.first.toLower(); _rankID = key.second; }
    NodePermissions(QMap<QString, QVariant> perms);

    QString getID() const { return _id; } // a user-name or a group-name, not verified
    void setID(const QString& id) { _id = id; }
    void setRankID(QUuid& rankID) { _rankID = rankID; }
    QUuid getRankID() const { return _rankID; }
    NodePermissionsKey getKey() const { return NodePermissionsKey(_id, _rankID); }

    // the _id member isn't authenticated/verified and _username is.
    void setVerifiedUserName(QString userName) { _verifiedUserName = userName.toLower(); }
    const QString& getVerifiedUserName() const { return _verifiedUserName; }

    void setVerifiedDomainUserName(QString userName) { _verifiedDomainUserName = userName.toLower(); }
    const QString& getVerifiedDomainUserName() const { return _verifiedDomainUserName; }

    void setGroupID(QUuid groupID) { _groupID = groupID; if (!groupID.isNull()) { _groupIDSet = true; }}
    QUuid getGroupID() const { return _groupID; }
    bool isGroup() const { return _groupIDSet; }

    bool isAssignment { false };

    // these 3 names have special meaning.
    static NodePermissionsKey standardNameLocalhost;
    static NodePermissionsKey standardNameLoggedIn;
    static NodePermissionsKey standardNameAnonymous;
    static NodePermissionsKey standardNameFriends;
    static QStringList standardNames;

    enum class Permission {
        none = 0,
        canConnectToDomain = 1,
        canAdjustLocks = 2,
        canRezPermanentEntities = 4,
        canRezTemporaryEntities = 8,
        canWriteToAssetServer = 16,
        canConnectPastMaxCapacity = 32,
        canKick = 64,
        canReplaceDomainContent = 128,
        canGetAndSetPrivateUserData = 1024,
        canRezAvatarEntities = 2048
    };
    Q_DECLARE_FLAGS(Permissions, Permission)
    Permissions permissions;

    QVariant toVariant(QHash<QUuid, GroupRank> groupRanks = QHash<QUuid, GroupRank>());

    void setAll(bool value);

    NodePermissions& operator|=(const NodePermissions& rhs);
    NodePermissions& operator&=(const NodePermissions& rhs);
    NodePermissions operator~();
    friend QDataStream& operator<<(QDataStream& out, const NodePermissions& perms);
    friend QDataStream& operator>>(QDataStream& in, NodePermissions& perms);

    void clear(Permission p) { permissions &= (Permission) (~(uint)p); }
    void set(Permission p) { permissions |= p; }
    bool can(Permission p) const { return permissions.testFlag(p); }

protected:
    QString _id;
    QUuid _rankID { QUuid() }; // 0 unless this is for a group
    QString _verifiedUserName;
    QString _verifiedDomainUserName;

    bool _groupIDSet { false };
    QUuid _groupID;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(NodePermissions::Permissions)


// wrap QHash in a class that forces all keys to be lowercase
class NodePermissionsMap {
public:
    NodePermissionsMap() { }
    NodePermissionsPointer& operator[](const NodePermissionsKey& key) {
        NodePermissionsKey dataKey(key.first.toLower(), key.second);
        if (0 == _data.count(dataKey)) {
            _data[dataKey] = NodePermissionsPointer(new NodePermissions(key));
        }
        return _data[dataKey];
    }
    NodePermissionsPointer operator[](const NodePermissionsKey& key) const {
        NodePermissionsPointer result;
        auto itr =  _data.find(NodePermissionsKey(key.first.toLower(), key.second));
        if (_data.end() != itr) {
            result = itr->second;
        }
        return result;
    }
    bool contains(const NodePermissionsKey& key) const {
        return 0 != _data.count(NodePermissionsKey(key.first.toLower(), key.second));
    }
    bool contains(const QString& keyFirst, const QUuid& keySecond) const {
        return 0 != _data.count(NodePermissionsKey(keyFirst.toLower(), keySecond));
    }

    QList<NodePermissionsKey> keys() const { 
        QList<NodePermissionsKey> result;
        for (const auto& entry : _data) {
            result.push_back(entry.first);
        }
        return result;
    }

    const std::unordered_map<NodePermissionsKey, NodePermissionsPointer>& get() { return _data; }
    void clear() { _data.clear(); }
    void remove(const NodePermissionsKey& key) { _data.erase(key); }

private:
    std::unordered_map<NodePermissionsKey, NodePermissionsPointer> _data;
};


const NodePermissions DEFAULT_AGENT_PERMISSIONS;

QDebug operator<<(QDebug debug, const NodePermissions& perms);
QDebug operator<<(QDebug debug, const NodePermissionsPointer& perms);
NodePermissionsPointer& operator&=(NodePermissionsPointer& lhs, const NodePermissionsPointer& rhs);
NodePermissionsPointer& operator&=(NodePermissionsPointer& lhs, NodePermissions::Permission rhs);
NodePermissionsPointer operator~(NodePermissionsPointer& lhs);

#endif // hifi_NodePermissions_h
