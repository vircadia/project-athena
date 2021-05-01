//
//  LimitedNodeList.h
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_LimitedNodeList_h
#define hifi_LimitedNodeList_h

#include <assert.h>
#include <stdint.h>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>

#ifndef _WIN32
#include <unistd.h> // not on windows, not needed for mac or windows
#endif

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSet>
#include <QtCore/QSharedMemory>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>

#include <TBBHelpers.h>

#include <DependencyManager.h>
#include <SharedUtil.h>

#include "DomainHandler.h"
#include "NetworkingConstants.h"
#include "Node.h"
#include "NLPacket.h"
#include "NLPacketList.h"
#include "PacketReceiver.h"
#include "ReceivedMessage.h"
#include "udt/ControlPacket.h"
#include "udt/PacketHeaders.h"
#include "udt/Socket.h"
#include "UUIDHasher.h"

const int INVALID_PORT = -1;

const quint64 NODE_SILENCE_THRESHOLD_MSECS = 10 * 1000;

static const size_t DEFAULT_MAX_CONNECTION_RATE { std::numeric_limits<size_t>::max() };

const char DEFAULT_ASSIGNMENT_SERVER_HOSTNAME[] = "localhost";

const char STUN_SERVER_HOSTNAME[] = "stun1.l.google.com";
const unsigned short STUN_SERVER_PORT = NetworkingConstants::STUN_SERVER_DEFAULT_PORT;

const QString DOMAIN_SERVER_LOCAL_PORT_SMEM_KEY = "domain-server.local-port";
const QString DOMAIN_SERVER_LOCAL_HTTP_PORT_SMEM_KEY = "domain-server.local-http-port";
const QString DOMAIN_SERVER_LOCAL_HTTPS_PORT_SMEM_KEY = "domain-server.local-https-port";

const QHostAddress DEFAULT_ASSIGNMENT_CLIENT_MONITOR_HOSTNAME = QHostAddress::LocalHost;

const QString USERNAME_UUID_REPLACEMENT_STATS_KEY = "$username";

using ConnectionID = int64_t;
const ConnectionID NULL_CONNECTION_ID { -1 };
const ConnectionID INITIAL_CONNECTION_ID { 0 };

typedef std::pair<QUuid, SharedNodePointer> UUIDNodePair;
typedef tbb::concurrent_unordered_map<QUuid, SharedNodePointer, UUIDHasher> NodeHash;

typedef quint8 PingType_t;
namespace PingType {
    const PingType_t Agnostic = 0;
    const PingType_t Local = 1;
    const PingType_t Public = 2;
    const PingType_t Symmetric = 3;
}

class LimitedNodeList : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY
public:

    enum ConnectionStep {
        LookupAddress = 1,
        HandleAddress,
        SendSTUNRequest,
        SetPublicSocketFromSTUN,
        SetICEServerHostname,
        SetICEServerSocket,
        SendICEServerQuery,
        ReceiveDSPeerInformation,
        SendPingsToDS,
        SetDomainHostname,
        SetDomainSocket,
        SendDSCheckIn,
        ReceiveDSList,
        AddedAudioMixer,
        SendAudioPing,
        SetAudioMixerSocket,
        SendAudioPacket,
        ReceiveFirstAudioPacket
    };

    Q_ENUM(ConnectionStep);

    enum ConnectReason : quint32 {
        Connect = 0,
        SilentDomainDisconnect,
        Awake
    };
    Q_ENUM(ConnectReason);

    QUuid getSessionUUID() const;
    void setSessionUUID(const QUuid& sessionUUID);
    Node::LocalID getSessionLocalID() const;
    void setSessionLocalID(Node::LocalID localID);

    void setPermissions(const NodePermissions& newPermissions);
    bool isAllowedEditor() const { return _permissions.can(NodePermissions::Permission::canAdjustLocks); }
    bool getThisNodeCanRez() const { return _permissions.can(NodePermissions::Permission::canRezPermanentEntities); }
    bool getThisNodeCanRezTmp() const { return _permissions.can(NodePermissions::Permission::canRezTemporaryEntities); }
    bool getThisNodeCanWriteAssets() const { return _permissions.can(NodePermissions::Permission::canWriteToAssetServer); }
    bool getThisNodeCanKick() const { return _permissions.can(NodePermissions::Permission::canKick); }
    bool getThisNodeCanReplaceContent() const { return _permissions.can(NodePermissions::Permission::canReplaceDomainContent); }
    bool getThisNodeCanGetAndSetPrivateUserData() const { return _permissions.can(NodePermissions::Permission::canGetAndSetPrivateUserData); }
    bool getThisNodeCanRezAvatarEntities() const { return _permissions.can(NodePermissions::Permission::canRezAvatarEntities); }

    quint16 getSocketLocalPort() const { return _nodeSocket.localPort(); }
    Q_INVOKABLE void setSocketLocalPort(quint16 socketLocalPort);

    QUdpSocket& getDTLSSocket();

    PacketReceiver& getPacketReceiver() { return *_packetReceiver; }

    virtual bool isDomainServer() const { return true; }
    virtual QUuid getDomainUUID() const { assert(false); return QUuid(); }
    virtual Node::LocalID getDomainLocalID() const { assert(false); return Node::NULL_LOCAL_ID; }
    virtual HifiSockAddr getDomainSockAddr() const { assert(false); return HifiSockAddr(); }

    // use sendUnreliablePacket to send an unreliable packet (that you do not need to move)
    // either to a node (via its active socket) or to a manual sockaddr
    qint64 sendUnreliablePacket(const NLPacket& packet, const Node& destinationNode);
    qint64 sendUnreliablePacket(const NLPacket& packet, const HifiSockAddr& sockAddr, HMACAuth* hmacAuth = nullptr);

    // use sendPacket to send a moved unreliable or reliable NL packet to a node's active socket or manual sockaddr
    qint64 sendPacket(std::unique_ptr<NLPacket> packet, const Node& destinationNode);
    qint64 sendPacket(std::unique_ptr<NLPacket> packet, const HifiSockAddr& sockAddr, HMACAuth* hmacAuth = nullptr);

    // use sendUnreliableUnorderedPacketList to unreliably send separate packets from the packet list
    // either to a node's active socket or to a manual sockaddr
    qint64 sendUnreliableUnorderedPacketList(NLPacketList& packetList, const Node& destinationNode);
    qint64 sendUnreliableUnorderedPacketList(NLPacketList& packetList, const HifiSockAddr& sockAddr,
        HMACAuth* hmacAuth = nullptr);

    // use sendPacketList to send reliable packet lists (ordered or unordered) to a node's active socket
    // or to a manual sock addr
    qint64 sendPacketList(std::unique_ptr<NLPacketList> packetList, const HifiSockAddr& sockAddr);
    qint64 sendPacketList(std::unique_ptr<NLPacketList> packetList, const Node& destinationNode);

    std::function<void(Node*)> linkedDataCreateCallback;

    size_t size() const { QReadLocker readLock(&_nodeMutex); return _nodeHash.size(); }

    SharedNodePointer nodeWithUUID(const QUuid& nodeUUID);
    SharedNodePointer nodeWithLocalID(Node::LocalID localID) const;

    SharedNodePointer addOrUpdateNode(const QUuid& uuid, NodeType_t nodeType,
                                      const HifiSockAddr& publicSocket, const HifiSockAddr& localSocket,
                                      Node::LocalID localID = Node::NULL_LOCAL_ID, bool isReplicated = false,
                                      bool isUpstream = false, const QUuid& connectionSecret = QUuid(),
                                      const NodePermissions& permissions = DEFAULT_AGENT_PERMISSIONS);

    static bool parseSTUNResponse(udt::BasePacket* packet, QHostAddress& newPublicAddress, uint16_t& newPublicPort);
    bool hasCompletedInitialSTUN() const { return _hasCompletedInitialSTUN; }

    const HifiSockAddr& getLocalSockAddr() const { return _localSockAddr; }
    const HifiSockAddr& getPublicSockAddr() const { return _publicSockAddr; }
    const HifiSockAddr& getSTUNSockAddr() const { return _stunSockAddr; }

    void processKillNode(ReceivedMessage& message);

    int updateNodeWithDataFromPacket(QSharedPointer<ReceivedMessage> packet, SharedNodePointer matchingNode);
    NodeData* getOrCreateLinkedData(SharedNodePointer node);

    unsigned int broadcastToNodes(std::unique_ptr<NLPacket> packet, const NodeSet& destinationNodeTypes);
    SharedNodePointer soloNodeOfType(NodeType_t nodeType);

    std::unique_ptr<NLPacket> constructPingPacket(const QUuid& nodeId, PingType_t pingType = PingType::Agnostic);
    std::unique_ptr<NLPacket> constructPingReplyPacket(ReceivedMessage& message);

    static std::unique_ptr<NLPacket> constructICEPingPacket(PingType_t pingType, const QUuid& iceID);
    static std::unique_ptr<NLPacket> constructICEPingReplyPacket(ReceivedMessage& message, const QUuid& iceID);

    void sendPeerQueryToIceServer(const HifiSockAddr& iceServerSockAddr, const QUuid& clientID, const QUuid& peerID);

    SharedNodePointer findNodeWithAddr(const HifiSockAddr& addr);

    using value_type = SharedNodePointer;
    using const_iterator = std::vector<value_type>::const_iterator;

    // Cede control of iteration under a single read lock (e.g. for use by thread pools)
    // Use this for nested loops instead of taking nested read locks!
    //   This allows multiple threads (i.e. a thread pool) to share a lock
    //   without deadlocking when a dying node attempts to acquire a write lock
    template<typename NestedNodeLambda>
    void nestedEach(NestedNodeLambda functor,
                    int* lockWaitOut = nullptr,
                    int* nodeTransformOut = nullptr,
                    int* functorOut = nullptr) {
        quint64 start, endTransform, endFunctor;

        start = usecTimestampNow();
        std::vector<SharedNodePointer> nodes;
        {
            QReadLocker readLock(&_nodeMutex);
            auto endLock = usecTimestampNow();
            if (lockWaitOut) {
                *lockWaitOut = (endLock - start);
            }

            // Size of _nodeHash could change at any time,
            // so reserve enough memory for the current size
            // and then back insert all the nodes found
            nodes.reserve(_nodeHash.size());
            std::transform(_nodeHash.cbegin(), _nodeHash.cend(), std::back_inserter(nodes), [&](const NodeHash::value_type& it) {
                return it.second;
            });

            endTransform = usecTimestampNow();
            if (nodeTransformOut) {
                *nodeTransformOut = (endTransform - endLock);
            }
        }

        functor(nodes.cbegin(), nodes.cend());
        endFunctor = usecTimestampNow();
        if (functorOut) {
            *functorOut = (endFunctor - endTransform);
        }
    }

    template<typename NodeLambda>
    void eachNode(NodeLambda functor) {
        QReadLocker readLock(&_nodeMutex);

        for (NodeHash::const_iterator it = _nodeHash.cbegin(); it != _nodeHash.cend(); ++it) {
            functor(it->second);
        }
    }

    template<typename PredLambda, typename NodeLambda>
    void eachMatchingNode(PredLambda predicate, NodeLambda functor) {
        QReadLocker readLock(&_nodeMutex);

        for (NodeHash::const_iterator it = _nodeHash.cbegin(); it != _nodeHash.cend(); ++it) {
            if (predicate(it->second)) {
                functor(it->second);
            }
        }
    }

    template<typename BreakableNodeLambda>
    void eachNodeBreakable(BreakableNodeLambda functor) {
        QReadLocker readLock(&_nodeMutex);

        for (NodeHash::const_iterator it = _nodeHash.cbegin(); it != _nodeHash.cend(); ++it) {
            if (!functor(it->second)) {
                break;
            }
        }
    }

    template<typename PredLambda>
    SharedNodePointer nodeMatchingPredicate(const PredLambda predicate) {
        QReadLocker readLock(&_nodeMutex);

        for (NodeHash::const_iterator it = _nodeHash.cbegin(); it != _nodeHash.cend(); ++it) {
            if (predicate(it->second)) {
                return it->second;
            }
        }

        return SharedNodePointer();
    }

    // This is unsafe because it does not take a lock
    // Must only be called when you know that a read lock on the node mutex is held
    // and will be held for the duration of your iteration
    template<typename NodeLambda>
    void unsafeEachNode(NodeLambda functor) {
        for (NodeHash::const_iterator it = _nodeHash.cbegin(); it != _nodeHash.cend(); ++it) {
            functor(it->second);
        }
    }

    void putLocalPortIntoSharedMemory(const QString key, QObject* parent, quint16 localPort);
    bool getLocalServerPortFromSharedMemory(const QString key, quint16& localPort);

    const QMap<quint64, ConnectionStep> getLastConnectionTimes() const
        { QReadLocker readLock(&_connectionTimeLock); return _lastConnectionTimes; }
    void flagTimeForConnectionStep(ConnectionStep connectionStep);

    udt::Socket::StatsVector sampleStatsForAllConnections() { return _nodeSocket.sampleStatsForAllConnections(); }

    void setConnectionMaxBandwidth(int maxBandwidth) { _nodeSocket.setConnectionMaxBandwidth(maxBandwidth); }

    void setPacketFilterOperator(udt::PacketFilterOperator filterOperator) { _nodeSocket.setPacketFilterOperator(filterOperator); }
    bool packetVersionMatch(const udt::Packet& packet);

    bool isPacketVerifiedWithSource(const udt::Packet& packet, Node* sourceNode = nullptr);
    bool isPacketVerified(const udt::Packet& packet) { return isPacketVerifiedWithSource(packet); }
    void setAuthenticatePackets(bool useAuthentication) { _useAuthentication = useAuthentication; }
    bool getAuthenticatePackets() const { return _useAuthentication; }

    void setFlagTimeForConnectionStep(bool flag) { _flagTimeForConnectionStep = flag; }
    bool isFlagTimeForConnectionStep() { return _flagTimeForConnectionStep; }

    static void makeSTUNRequestPacket(char* stunRequestPacket);

#if (PR_BUILD || DEV_BUILD)
    void sendFakedHandshakeRequestToNode(SharedNodePointer node);
#endif

    size_t getMaxConnectionRate() const { return _maxConnectionRate; }
    void setMaxConnectionRate(size_t rate) { _maxConnectionRate = rate; }

    int getInboundPPS() const { return _inboundPPS; }
    int getOutboundPPS() const { return _outboundPPS; }
    float getInboundKbps() const { return _inboundKbps; }
    float getOutboundKbps() const { return _outboundKbps; }

    void setDropOutgoingNodeTraffic(bool squelchOutgoingNodeTraffic) { _dropOutgoingNodeTraffic = squelchOutgoingNodeTraffic; }

    const std::set<NodeType_t> SOLO_NODE_TYPES = {
        NodeType::AvatarMixer,
        NodeType::AudioMixer,
        NodeType::AssetServer,
        NodeType::EntityServer,
        NodeType::MessagesMixer,
        NodeType::EntityScriptServer
    };

public slots:
    void reset(QString reason);
    void eraseAllNodes(QString reason);

    void removeSilentNodes();

    void updateLocalSocket();

    void startSTUNPublicSocketUpdate();
    virtual void sendSTUNRequest();

    bool killNodeWithUUID(const QUuid& nodeUUID, ConnectionID newConnectionID = NULL_CONNECTION_ID);
    void noteAwakening() { _connectReason = Awake; }

private slots:
    void sampleConnectionStats();

signals:
    // QUuid might be zero for non-sourced packet types.
    void packetVersionMismatch(PacketType type, const HifiSockAddr& senderSockAddr, const QUuid& senderUUID);

    void uuidChanged(const QUuid& ownerUUID, const QUuid& oldUUID);
    void nodeAdded(SharedNodePointer);
    void nodeSocketUpdated(SharedNodePointer);
    void nodeKilled(SharedNodePointer);
    void nodeActivated(SharedNodePointer);

    void clientConnectionToNodeReset(SharedNodePointer);

    void localSockAddrChanged(const HifiSockAddr& localSockAddr);
    void publicSockAddrChanged(const HifiSockAddr& publicSockAddr);

    void isAllowedEditorChanged(bool isAllowedEditor);
    void canRezChanged(bool canRez);
    void canRezTmpChanged(bool canRezTmp);
    void canWriteAssetsChanged(bool canWriteAssets);
    void canKickChanged(bool canKick);
    void canReplaceContentChanged(bool canReplaceContent);
    void canGetAndSetPrivateUserDataChanged(bool canGetAndSetPrivateUserData);
    void canRezAvatarEntitiesChanged(bool canRezAvatarEntities);

protected slots:
    void connectedForLocalSocketTest();
    void errorTestingLocalSocket();

    void clientConnectionToSockAddrReset(const HifiSockAddr& sockAddr);

    void processDelayedAdds();

protected:
    struct NewNodeInfo {
        qint8 type;
        QUuid uuid;
        HifiSockAddr publicSocket;
        HifiSockAddr localSocket;
        NodePermissions permissions;
        bool isReplicated;
        Node::LocalID sessionLocalID;
        QUuid connectionSecretUUID;
    };

    LimitedNodeList(int socketListenPort = INVALID_PORT, int dtlsListenPort = INVALID_PORT);
    LimitedNodeList(LimitedNodeList const&) = delete; // Don't implement, needed to avoid copies of singleton
    void operator=(LimitedNodeList const&) = delete; // Don't implement, needed to avoid copies of singleton

    qint64 sendPacket(std::unique_ptr<NLPacket> packet, const Node& destinationNode,
                      const HifiSockAddr& overridenSockAddr);
    void fillPacketHeader(const NLPacket& packet, HMACAuth* hmacAuth = nullptr);

    void setLocalSocket(const HifiSockAddr& sockAddr);

    bool packetSourceAndHashMatchAndTrackBandwidth(const udt::Packet& packet, Node* sourceNode = nullptr);
    void processSTUNResponse(std::unique_ptr<udt::BasePacket> packet);

    void handleNodeKill(const SharedNodePointer& node, ConnectionID newConnectionID = NULL_CONNECTION_ID);

    void stopInitialSTUNUpdate(bool success);

    void sendPacketToIceServer(PacketType packetType, const HifiSockAddr& iceServerSockAddr, const QUuid& clientID,
                               const QUuid& peerRequestID = QUuid());

    bool sockAddrBelongsToNode(const HifiSockAddr& sockAddr);

    void addNewNode(NewNodeInfo info);
    void delayNodeAdd(NewNodeInfo info);
    void removeDelayedAdd(QUuid nodeUUID);
    bool isDelayedNode(QUuid nodeUUID);

    NodeHash _nodeHash;
    mutable QReadWriteLock _nodeMutex { QReadWriteLock::Recursive };
    udt::Socket _nodeSocket;
    QUdpSocket* _dtlsSocket { nullptr };
    HifiSockAddr _localSockAddr;
    HifiSockAddr _publicSockAddr;
    HifiSockAddr _stunSockAddr { STUN_SERVER_HOSTNAME, STUN_SERVER_PORT };
    bool _hasTCPCheckedLocalSocket { false };
    bool _useAuthentication { true };

    PacketReceiver* _packetReceiver;

    NodePermissions _permissions;

    QPointer<QTimer> _initialSTUNTimer;

    int _numInitialSTUNRequests = 0;
    bool _hasCompletedInitialSTUN = false;
    quint64 _firstSTUNTime = 0;
    quint64 _publicSocketUpdateTime = 0;

    mutable QReadWriteLock _connectionTimeLock { };
    QMap<quint64, ConnectionStep> _lastConnectionTimes;
    bool _areConnectionTimesComplete = false;

    template<typename IteratorLambda>
    void eachNodeHashIterator(IteratorLambda functor) {
        QWriteLocker writeLock(&_nodeMutex);
        NodeHash::iterator it = _nodeHash.begin();

        while (it != _nodeHash.end()) {
            functor(it);
        }
    }

    std::unordered_map<QUuid, ConnectionID> _connectionIDs;
    quint64 _nodeConnectTimestamp{ 0 };
    quint64 _nodeDisconnectTimestamp{ 0 };
    ConnectReason _connectReason { Connect };

private slots:
    void flagTimeForConnectionStep(ConnectionStep connectionStep, quint64 timestamp);
    void possiblyTimeoutSTUNAddressLookup();
    void addSTUNHandlerToUnfiltered(); // called once STUN socket known

private:
    mutable QReadWriteLock _sessionUUIDLock;
    QUuid _sessionUUID;
    using LocalIDMapping = tbb::concurrent_unordered_map<Node::LocalID, SharedNodePointer>;
    LocalIDMapping _localIDMap;
    Node::LocalID _sessionLocalID { 0 };
    bool _flagTimeForConnectionStep { false }; // only keep track in interface

    size_t _maxConnectionRate { DEFAULT_MAX_CONNECTION_RATE };
    size_t _nodesAddedInCurrentTimeSlice { 0 };
    std::vector<NewNodeInfo> _delayedNodeAdds;

    int _inboundPPS { 0 };
    int _outboundPPS { 0 };
    float _inboundKbps { 0.0f };
    float _outboundKbps { 0.0f };

    bool _dropOutgoingNodeTraffic { false };

    quint64 _sendErrorStatsTime { (quint64)0 };
    static const quint64 ERROR_STATS_PERIOD_US { 1 * USECS_PER_SECOND };
};

#endif // hifi_LimitedNodeList_h
