//
//  NodeList.h
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_NodeList_h
#define hifi_NodeList_h

#include <stdint.h>
#include <iterator>
#include <assert.h>

#ifndef _WIN32
#include <unistd.h> // not on windows, not needed for mac or windows
#endif

#include <TBBHelpers.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>

#include <DependencyManager.h>
#include <SettingHandle.h>

#include "DomainHandler.h"
#include "LimitedNodeList.h"
#include "Node.h"

const quint64 DOMAIN_SERVER_CHECK_IN_MSECS = 1 * 1000;

using PacketOrPacketList = std::pair<std::unique_ptr<NLPacket>, std::unique_ptr<NLPacketList>>;
using NodePacketOrPacketListPair = std::pair<SharedNodePointer, PacketOrPacketList>;

using NodePacketPair = std::pair<SharedNodePointer, std::unique_ptr<NLPacket>>;
using NodeSharedPacketPair = std::pair<SharedNodePointer, QSharedPointer<NLPacket>>;
using NodeSharedReceivedMessagePair = std::pair<SharedNodePointer, QSharedPointer<ReceivedMessage>>;

class Application;
class Assignment;

class NodeList : public LimitedNodeList {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    void startThread();
    NodeType_t getOwnerType() const { return _ownerType.load(); }
    void setOwnerType(NodeType_t ownerType) { _ownerType.store(ownerType); }

    Q_INVOKABLE qint64 sendStats(QJsonObject statsObject, SockAddr destination);
    Q_INVOKABLE qint64 sendStatsToDomainServer(QJsonObject statsObject);

    DomainHandler& getDomainHandler() { return _domainHandler; }

    const NodeSet& getNodeInterestSet() const { return _nodeTypesOfInterest; }
    void addNodeTypeToInterestSet(NodeType_t nodeTypeToAdd);
    void addSetOfNodeTypesToNodeInterestSet(const NodeSet& setOfNodeTypes);
    void resetNodeInterestSet() { _nodeTypesOfInterest.clear(); }

    void setAssignmentServerSocket(const SockAddr& serverSocket) { _assignmentServerSocket = serverSocket; }
    void sendAssignment(Assignment& assignment);

    void disableDomainPortAutoDiscovery(bool disabled = false) { _domainPortAutoDiscovery = disabled; };

    void setIsShuttingDown(bool isShuttingDown) { _isShuttingDown = isShuttingDown; }

    void ignoreNodesInRadius(bool enabled = true);
    bool getIgnoreRadiusEnabled() const { return _ignoreRadiusEnabled.get(); }
    void toggleIgnoreRadius() { ignoreNodesInRadius(!getIgnoreRadiusEnabled()); }
    void enableIgnoreRadius() { ignoreNodesInRadius(true); }
    void disableIgnoreRadius() { ignoreNodesInRadius(false); }
    void ignoreNodeBySessionID(const QUuid& nodeID, bool ignoreEnabled);
    bool isIgnoringNode(const QUuid& nodeID) const;
    void personalMuteNodeBySessionID(const QUuid& nodeID, bool muteEnabled);
    bool isPersonalMutingNode(const QUuid& nodeID) const;
    void setAvatarGain(const QUuid& nodeID, float gain);
    float getAvatarGain(const QUuid& nodeID);
    void setInjectorGain(float gain);
    float getInjectorGain();

    void kickNodeBySessionID(const QUuid& nodeID, unsigned int banFlags);
    void muteNodeBySessionID(const QUuid& nodeID);
    void requestUsernameFromSessionID(const QUuid& nodeID);
    bool getRequestsDomainListData() { return _requestsDomainListData; }
    void setRequestsDomainListData(bool isRequesting);

    bool getSendDomainServerCheckInEnabled() { return _sendDomainServerCheckInEnabled; }
    void setSendDomainServerCheckInEnabled(bool enabled) { _sendDomainServerCheckInEnabled = enabled; }

    void removeFromIgnoreMuteSets(const QUuid& nodeID);

    virtual bool isDomainServer() const override { return false; }
    virtual QUuid getDomainUUID() const override { return _domainHandler.getUUID(); }
    virtual Node::LocalID getDomainLocalID() const override { return _domainHandler.getLocalID(); }
    virtual SockAddr getDomainSockAddr() const override { return _domainHandler.getSockAddr(); }

public slots:
    void reset(QString reason, bool skipDomainHandlerReset = false);
    void resetFromDomainHandler() { reset("Reset from Domain Handler", true); }

    void sendDomainServerCheckIn();
    void handleDSPathQuery(const QString& newPath);

    void processDomainServerList(QSharedPointer<ReceivedMessage> message);
    void processDomainServerAddedNode(QSharedPointer<ReceivedMessage> message);
    void processDomainServerRemovedNode(QSharedPointer<ReceivedMessage> message);
    void processDomainServerPathResponse(QSharedPointer<ReceivedMessage> message);

    void processDomainServerConnectionTokenPacket(QSharedPointer<ReceivedMessage> message);

    void processPingPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode);
    void processPingReplyPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode);

    void processICEPingPacket(QSharedPointer<ReceivedMessage> message);

    void processUsernameFromIDReply(QSharedPointer<ReceivedMessage> message);

    // FIXME: Can remove these work-arounds in version 2021.2.0. (New protocol version implies a domain server upgrade.)
    bool adjustCanRezAvatarEntitiesPermissions(const QJsonObject& domainSettingsObject, NodePermissions& permissions,
        bool notify);
    void adjustCanRezAvatarEntitiesPerSettings(const QJsonObject& domainSettingsObject);

#if (PR_BUILD || DEV_BUILD)
    void toggleSendNewerDSConnectVersion(bool shouldSendNewerVersion) { _shouldSendNewerVersion = shouldSendNewerVersion; }
#endif

signals:
    void receivedDomainServerList();
    void ignoredNode(const QUuid& nodeID, bool enabled);
    void ignoreRadiusEnabledChanged(bool isIgnored);
    void usernameFromIDReply(const QString& nodeID, const QString& username, const QString& machineFingerprint, bool isAdmin);

private slots:
    void stopKeepalivePingTimer();
    void sendPendingDSPathQuery();
    void handleICEConnectionToDomainServer();

    void startNodeHolePunch(const SharedNodePointer& node);
    void handleNodePingTimeout();

    void pingPunchForDomainServer();

    void sendKeepAlivePings();

    void maybeSendIgnoreSetToNode(SharedNodePointer node);

private:
    NodeList() : LimitedNodeList(INVALID_PORT, INVALID_PORT) { assert(false); } // Not implemented, needed for DependencyManager templates compile
    NodeList(char ownerType, int socketListenPort = INVALID_PORT, int dtlsListenPort = INVALID_PORT);
    NodeList(NodeList const&) = delete; // Don't implement, needed to avoid copies of singleton
    void operator=(NodeList const&) = delete; // Don't implement, needed to avoid copies of singleton

    void processDomainServerAuthRequest(const QByteArray& packet);
    void requestAuthForDomainServer();
    void activateSocketFromNodeCommunication(ReceivedMessage& message, const SharedNodePointer& sendingNode);
    void timePingReply(ReceivedMessage& message, const SharedNodePointer& sendingNode);

    void sendDSPathQuery(const QString& newPath);

    void parseNodeFromPacketStream(QDataStream& packetStream);

    void pingPunchForInactiveNode(const SharedNodePointer& node);

    bool sockAddrBelongsToDomainOrNode(const SockAddr& sockAddr);

    std::atomic<NodeType_t> _ownerType;
    NodeSet _nodeTypesOfInterest;
    DomainHandler _domainHandler;
    SockAddr _assignmentServerSocket;
    bool _isShuttingDown { false };
    QTimer _keepAlivePingTimer;
    bool _requestsDomainListData { false };

    bool _sendDomainServerCheckInEnabled { true };
    bool _domainPortAutoDiscovery { false };

    mutable QReadWriteLock _ignoredSetLock;
    tbb::concurrent_unordered_set<QUuid, UUIDHasher> _ignoredNodeIDs;
    mutable QReadWriteLock _personalMutedSetLock;
    tbb::concurrent_unordered_set<QUuid, UUIDHasher> _personalMutedNodeIDs;
    mutable QReadWriteLock _avatarGainMapLock;
    tbb::concurrent_unordered_map<QUuid, float, UUIDHasher> _avatarGainMap;

    std::atomic<float> _avatarGain { 0.0f };    // in dB
    std::atomic<float> _injectorGain { 0.0f };  // in dB

    void sendIgnoreRadiusStateToNode(const SharedNodePointer& destinationNode);
#if defined(Q_OS_ANDROID)
    Setting::Handle<bool> _ignoreRadiusEnabled { "IgnoreRadiusEnabled", false };
#else
    Setting::Handle<bool> _ignoreRadiusEnabled { "IgnoreRadiusEnabled", false }; // False, until such time as it is made to work better.
#endif

#if (PR_BUILD || DEV_BUILD)
    bool _shouldSendNewerVersion { false };
#endif

    bool _hasDomainAccountManager { false };
};

#endif // hifi_NodeList_h
