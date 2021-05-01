//
//  DomainGatekeeper.h
//  domain-server/src
//
//  Created by Stephen Birarda on 2015-08-24.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_DomainGatekeeper_h
#define hifi_DomainGatekeeper_h

#include <unordered_map>
#include <unordered_set>

#include <QtCore/QObject>
#include <QtNetwork/QNetworkReply>

#include <DomainHandler.h>

#include <NLPacket.h>
#include <Node.h>
#include <UUIDHasher.h>

#include "NodeConnectionData.h"
#include "PendingAssignedNodeData.h"

const QString DOMAIN_GROUP_CHAR = "@";

class DomainServer;

class DomainGatekeeper : public QObject {
    Q_OBJECT
public:
    DomainGatekeeper(DomainServer* server);
    
    void addPendingAssignedNode(const QUuid& nodeUUID, const QUuid& assignmentUUID, const QString& nodeVersion);
    QUuid assignmentUUIDForPendingAssignment(const QUuid& tempUUID);

    void cleanupICEPeerForNode(const QUuid& nodeID);

    Node::LocalID findOrCreateLocalID(const QUuid& uuid);

    static void sendProtocolMismatchConnectionDenial(const HifiSockAddr& senderSockAddr);
public slots:
    void processConnectRequestPacket(QSharedPointer<ReceivedMessage> message);
    void processICEPingPacket(QSharedPointer<ReceivedMessage> message);
    void processICEPingReplyPacket(QSharedPointer<ReceivedMessage> message);
    void processICEPeerInformationPacket(QSharedPointer<ReceivedMessage> message);

    void publicKeyJSONCallback(QNetworkReply* requestReply);
    void publicKeyJSONErrorCallback(QNetworkReply* requestReply);

    void getIsGroupMemberJSONCallback(QNetworkReply* requestReply);
    void getIsGroupMemberErrorCallback(QNetworkReply* requestReply);

    void getDomainOwnerFriendsListJSONCallback(QNetworkReply* requestReply);
    void getDomainOwnerFriendsListErrorCallback(QNetworkReply* requestReply);

    void refreshGroupsCache();

signals:
    void killNode(SharedNodePointer node);
    void connectedNode(SharedNodePointer node, quint64 requestReceiveTime);

public slots:
    void updateNodePermissions();

private slots:
    void handlePeerPingTimeout();

    // Login and groups for domain, separate from metaverse.
    void requestDomainUserFinished();

private:
    SharedNodePointer processAssignmentConnectRequest(const NodeConnectionData& nodeConnection,
                                                      const PendingAssignedNodeData& pendingAssignment);
    SharedNodePointer processAgentConnectRequest(const NodeConnectionData& nodeConnection,
                                                 const QString& username,
                                                 const QByteArray& usernameSignature,
                                                 const QString& domainUsername,
                                                 const QString& domainAccessToken,
                                                 const QString& domainRefreshToken);
    SharedNodePointer addVerifiedNodeFromConnectRequest(const NodeConnectionData& nodeConnection);
    
    bool verifyUserSignature(const QString& username, const QByteArray& usernameSignature,
                             const HifiSockAddr& senderSockAddr);
    
    bool needToVerifyDomainUserIdentity(const QString& username, const QString& accessToken, const QString& refreshToken);
    bool verifyDomainUserIdentity(const QString& username, const QString& accessToken, const QString& refreshToken,
                                  const HifiSockAddr& senderSockAddr);

    bool isWithinMaxCapacity();
    
    bool shouldAllowConnectionFromNode(const QString& username, const QByteArray& usernameSignature,
                                       const HifiSockAddr& senderSockAddr);
    
    void sendConnectionTokenPacket(const QString& username, const HifiSockAddr& senderSockAddr);
    static void sendConnectionDeniedPacket(const QString& reason, const HifiSockAddr& senderSockAddr,
            DomainHandler::ConnectionRefusedReason reasonCode = DomainHandler::ConnectionRefusedReason::Unknown,
            QString extraInfo = QString());
    
    void pingPunchForConnectingPeer(const SharedNetworkPeer& peer);
    
    void requestUserPublicKey(const QString& username, bool isOptimistic = false);
    
    DomainServer* _server;
    
    std::unordered_map<QUuid, PendingAssignedNodeData> _pendingAssignedNodes;
    
    QHash<QUuid, SharedNetworkPeer> _icePeers;

    using ConnectingNodeID = QUuid;
    using ICEPeerID = QUuid;
    QHash<ConnectingNodeID, ICEPeerID> _nodeToICEPeerIDs;
    
    QHash<QString, QUuid> _connectionTokenHash;

    // the word "optimistic" below is used for keys that we request during user connection before the user has
    // had a chance to upload a new public key

    // we don't send back user signature decryption errors for those keys so that there isn't a thrasing of key re-generation
    // and connection refusal

    using KeyFlagPair = QPair<QByteArray, bool>;

    QHash<QString, KeyFlagPair> _userPublicKeys; // keep track of keys and flag them as optimistic or not
    QHash<QString, bool> _inFlightPublicKeyRequests; // keep track of keys we've asked for (and if it was optimistic)
    QSet<QString> _domainOwnerFriends; // keep track of friends of the domain owner
    QSet<QString> _inFlightGroupMembershipsRequests; // keep track of which we've already asked for

    NodePermissions setPermissionsForUser(bool isLocalUser, QString verifiedUsername, QString verifiedDomainUsername,
                                          const QHostAddress& senderAddress, const QString& hardwareAddress, 
                                          const QUuid& machineFingerprint);

    void getGroupMemberships(const QString& username);
    // void getIsGroupMember(const QString& username, const QUuid groupID);
    void getDomainOwnerFriendsList();

    // Local ID management.
    void initLocalIDManagement();
    using UUIDToLocalID = std::unordered_map<QUuid, Node::LocalID> ;
    using LocalIDs = std::unordered_set<Node::LocalID>;
    LocalIDs _localIDs;
    UUIDToLocalID _uuidToLocalID;
    Node::LocalID _currentLocalID;
    Node::LocalID _idIncrement;

    // Login and groups for domain, separate from metaverse.
    bool domainHasLogin();
    void requestDomainUser(const QString& username, const QString& accessToken, const QString& refreshToken);
    
    typedef QHash<QString, QPair<QString, QString>> DomainUserIdentities; // <domainUserName, <access_token, refresh_token>>
    DomainUserIdentities _inFlightDomainUserIdentityRequests;  // Domain user identity requests currently in progress.
    DomainUserIdentities _verifiedDomainUserIdentities;  // Verified domain users.

    QHash<QString, QStringList> _domainGroupMemberships;  // <domainUserName, [domainGroupName]>
};


#endif // hifi_DomainGatekeeper_h
