//
//  NodeList.cpp
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "NodeList.h"

#include <chrono>

#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QJsonDocument>
#include <QtCore/QMetaEnum>
#include <QtCore/QUrl>
#include <QtCore/QThread>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include <shared/QtHelpers.h>
#include <ThreadHelpers.h>
#include <LogHandler.h>
#include <UUID.h>
#include <platform/Platform.h>
#include <platform/PlatformKeys.h>

#include "AccountManager.h"
#include "AddressManager.h"
#include "Assignment.h"
#include "AudioHelpers.h"
#include "DomainAccountManager.h"
#include "SockAddr.h"
#include "FingerprintUtils.h"

#include "NetworkLogging.h"
#include "udt/PacketHeaders.h"
#include "SharedUtil.h"
#include <Trace.h>
#include <ModerationFlags.h>

using namespace std::chrono;

const int KEEPALIVE_PING_INTERVAL_MS = 1000;
const int MAX_SYSTEM_INFO_SIZE = 1000;

NodeList::NodeList(char newOwnerType, int socketListenPort, int dtlsListenPort) :
    LimitedNodeList(socketListenPort, dtlsListenPort),
    _ownerType(newOwnerType),
    _nodeTypesOfInterest(),
    _domainHandler(this),
    _assignmentServerSocket(),
    _keepAlivePingTimer(this)
{
    setCustomDeleter([](Dependency* dependency){
        static_cast<NodeList*>(dependency)->deleteLater();
    });

    auto addressManager = DependencyManager::get<AddressManager>();

    // handle domain change signals from AddressManager
    connect(addressManager.data(), &AddressManager::possibleDomainChangeRequired,
            &_domainHandler, &DomainHandler::setURLAndID);

    connect(addressManager.data(), &AddressManager::possibleDomainChangeRequiredViaICEForID,
            &_domainHandler, &DomainHandler::setIceServerHostnameAndID);

    // handle a request for a path change from the AddressManager
    connect(addressManager.data(), &AddressManager::pathChangeRequired, this, &NodeList::handleDSPathQuery);

    // in case we don't know how to talk to DS when a path change is requested
    // fire off any pending DS path query when we get socket information
    connect(&_domainHandler, &DomainHandler::connectedToDomain, this, &NodeList::sendPendingDSPathQuery);

    // send a domain server check in immediately once the DS socket is known
    connect(&_domainHandler, &DomainHandler::completedSocketDiscovery, this, &NodeList::sendDomainServerCheckIn);

    // send a domain server check in immediately if there is a public socket change
    connect(this, &LimitedNodeList::publicSockAddrChanged, this, &NodeList::sendDomainServerCheckIn);

    // clear our NodeList when the domain changes
    connect(&_domainHandler, SIGNAL(disconnectedFromDomain()), this, SLOT(resetFromDomainHandler()));

    // send an ICE heartbeat as soon as we get ice server information
    connect(&_domainHandler, &DomainHandler::iceSocketAndIDReceived, this, &NodeList::handleICEConnectionToDomainServer);

    // handle ping timeout from DomainHandler to establish a connection with auto networked domain-server
    connect(&_domainHandler.getICEPeer(), &NetworkPeer::pingTimerTimeout, this, &NodeList::pingPunchForDomainServer);

    // send a ping punch immediately
    connect(&_domainHandler, &DomainHandler::icePeerSocketsReceived, this, &NodeList::pingPunchForDomainServer);

    // FIXME: Can remove this temporary work-around in version 2021.2.0. (New protocol version implies a domain server upgrade.)
    // Adjust our canRezAvatarEntities permissions on older domains that do not have this setting.
    // DomainServerList and DomainSettings packets can come in either order so need to adjust with both occurrences.
    auto nodeList = DependencyManager::get<NodeList>();
    connect(&_domainHandler, &DomainHandler::settingsReceived, this, &NodeList::adjustCanRezAvatarEntitiesPerSettings);

    auto accountManager = DependencyManager::get<AccountManager>();

    // assume that we may need to send a new DS check in anytime a new keypair is generated
    connect(accountManager.data(), &AccountManager::newKeypair, this, &NodeList::sendDomainServerCheckIn);

    // clear out NodeList when login is finished and we know our new username
    connect(accountManager.data(), &AccountManager::usernameChanged , this, [this]{ reset("Username changed"); });

    // clear our NodeList when logout is requested
    connect(accountManager.data(), &AccountManager::logoutComplete , this, [this]{ reset("Logged out"); });

    // Only used in Interface.
    auto domainAccountManager = DependencyManager::get<DomainAccountManager>();
    if (domainAccountManager) {
        _hasDomainAccountManager = true;
        connect(domainAccountManager.data(), &DomainAccountManager::newTokens, this, &NodeList::sendDomainServerCheckIn);
    }

    // anytime we get a new node we will want to attempt to punch to it
    connect(this, &LimitedNodeList::nodeAdded, this, &NodeList::startNodeHolePunch);
    connect(this, &LimitedNodeList::nodeSocketUpdated, this, &NodeList::startNodeHolePunch);

    // anytime we get a new node we may need to re-send our set of ignored node IDs to it
    connect(this, &LimitedNodeList::nodeActivated, this, &NodeList::maybeSendIgnoreSetToNode);

    // setup our timer to send keepalive pings (it's started and stopped on domain connect/disconnect)
    _keepAlivePingTimer.setInterval(KEEPALIVE_PING_INTERVAL_MS); // 1s, Qt::CoarseTimer acceptable
    connect(&_keepAlivePingTimer, &QTimer::timeout, this, &NodeList::sendKeepAlivePings);
    connect(&_domainHandler, SIGNAL(connectedToDomain(QUrl)), &_keepAlivePingTimer, SLOT(start()));
    connect(&_domainHandler, &DomainHandler::disconnectedFromDomain, &_keepAlivePingTimer, &QTimer::stop);

    connect(&_domainHandler, &DomainHandler::limitOfSilentDomainCheckInsReached, this, [this]() {
        if (_connectReason != Awake) {
            _connectReason = SilentDomainDisconnect;
        }
    });

    // set our sockAddrBelongsToDomainOrNode method as the connection creation filter for the udt::Socket
    using std::placeholders::_1;
    _nodeSocket.setConnectionCreationFilterOperator(std::bind(&NodeList::sockAddrBelongsToDomainOrNode, this, _1));

    // we definitely want STUN to update our public socket, so call the LNL to kick that off
    startSTUNPublicSocketUpdate();

    auto& packetReceiver = getPacketReceiver();
    packetReceiver.registerListener(PacketType::DomainList,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processDomainServerList));
    packetReceiver.registerListener(PacketType::Ping,
        PacketReceiver::makeSourcedListenerReference<NodeList>(this, &NodeList::processPingPacket));
    packetReceiver.registerListener(PacketType::PingReply,
        PacketReceiver::makeSourcedListenerReference<NodeList>(this, &NodeList::processPingReplyPacket));
    packetReceiver.registerListener(PacketType::ICEPing,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processICEPingPacket));
    packetReceiver.registerListener(PacketType::DomainServerAddedNode,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processDomainServerAddedNode));
    packetReceiver.registerListener(PacketType::DomainServerConnectionToken,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processDomainServerConnectionTokenPacket));
    packetReceiver.registerListener(PacketType::DomainConnectionDenied,
        PacketReceiver::makeUnsourcedListenerReference<DomainHandler>(&_domainHandler, &DomainHandler::processDomainServerConnectionDeniedPacket));
    packetReceiver.registerListener(PacketType::DomainSettings,
        PacketReceiver::makeUnsourcedListenerReference<DomainHandler>(&_domainHandler, &DomainHandler::processSettingsPacketList));
    packetReceiver.registerListener(PacketType::ICEServerPeerInformation,
        PacketReceiver::makeUnsourcedListenerReference<DomainHandler>(&_domainHandler, &DomainHandler::processICEResponsePacket));
    packetReceiver.registerListener(PacketType::DomainServerRequireDTLS,
        PacketReceiver::makeUnsourcedListenerReference<DomainHandler>(&_domainHandler, &DomainHandler::processDTLSRequirementPacket));
    packetReceiver.registerListener(PacketType::ICEPingReply,
        PacketReceiver::makeUnsourcedListenerReference<DomainHandler>(&_domainHandler, &DomainHandler::processICEPingReplyPacket));
    packetReceiver.registerListener(PacketType::DomainServerPathResponse,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processDomainServerPathResponse));
    packetReceiver.registerListener(PacketType::DomainServerRemovedNode,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processDomainServerRemovedNode));
    packetReceiver.registerListener(PacketType::UsernameFromIDReply,
        PacketReceiver::makeUnsourcedListenerReference<NodeList>(this, &NodeList::processUsernameFromIDReply));
}

qint64 NodeList::sendStats(QJsonObject statsObject, SockAddr destination) {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "sendStats", Qt::QueuedConnection,
                                  Q_ARG(QJsonObject, statsObject),
                                  Q_ARG(SockAddr, destination));
        return 0;
    }

    auto statsPacketList = NLPacketList::create(PacketType::NodeJsonStats, QByteArray(), true, true);

    QJsonDocument jsonDocument(statsObject);
    statsPacketList->write(jsonDocument.toBinaryData());

    sendPacketList(std::move(statsPacketList), destination);
    return 0;
}

qint64 NodeList::sendStatsToDomainServer(QJsonObject statsObject) {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "sendStatsToDomainServer", Qt::QueuedConnection,
                                  Q_ARG(QJsonObject, statsObject));
        return 0;
    }

    return sendStats(statsObject, _domainHandler.getSockAddr());
}

void NodeList::timePingReply(ReceivedMessage& message, const SharedNodePointer& sendingNode) {
    PingType_t pingType;

    quint64 ourOriginalTime, othersReplyTime;

    message.seek(0);

    message.readPrimitive(&pingType);
    message.readPrimitive(&ourOriginalTime);
    message.readPrimitive(&othersReplyTime);

    quint64 now = usecTimestampNow();
    qint64 pingTime = now - ourOriginalTime;
    qint64 oneWayFlightTime = pingTime / 2; // half of the ping is our one way flight

    // The other node's expected time should be our original time plus the one way flight time
    // anything other than that is clock skew
    quint64 othersExpectedReply = ourOriginalTime + oneWayFlightTime;
    qint64 clockSkew = othersReplyTime - othersExpectedReply;

    sendingNode->setPingMs(pingTime / 1000);
    sendingNode->updateClockSkewUsec(clockSkew);

    const bool wantDebug = false;

    if (wantDebug) {
        auto averageClockSkew = sendingNode->getClockSkewUsec();
        qCDebug(networking) << "PING_REPLY from node " << *sendingNode << "\n" <<
        "                     now: " << now << "\n" <<
        "                 ourTime: " << ourOriginalTime << "\n" <<
        "                pingTime: " << pingTime << "\n" <<
        "        oneWayFlightTime: " << oneWayFlightTime << "\n" <<
        "         othersReplyTime: " << othersReplyTime << "\n" <<
        "    othersExprectedReply: " << othersExpectedReply << "\n" <<
        "               clockSkew: " << clockSkew << "[" << formatUsecTime(clockSkew) << "]" << "\n" <<
        "       average clockSkew: " << averageClockSkew << "[" << formatUsecTime(averageClockSkew) << "]";
    }
}

void NodeList::processPingPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    // send back a reply
    auto replyPacket = constructPingReplyPacket(*message);
    const SockAddr& senderSockAddr = message->getSenderSockAddr();
    sendPacket(std::move(replyPacket), *sendingNode, senderSockAddr);

    // If we don't have a symmetric socket for this node and this socket doesn't match
    // what we have for public and local then set it as the symmetric.
    // This allows a server on a reachable port to communicate with nodes on symmetric NATs
    if (sendingNode->getSymmetricSocket().isNull()) {
        if (senderSockAddr != sendingNode->getLocalSocket() && senderSockAddr != sendingNode->getPublicSocket()) {
            sendingNode->setSymmetricSocket(senderSockAddr);
        }
    }

    int64_t connectionID;

    message->readPrimitive(&connectionID);

    auto it = _connectionIDs.find(sendingNode->getUUID());
    if (it != _connectionIDs.end()) {
        if (connectionID > it->second) {
            qDebug() << "Received a ping packet with a larger connection id (" << connectionID << ">" << it->second << ") from "
                     << sendingNode->getUUID();
            killNodeWithUUID(sendingNode->getUUID(), connectionID);
        }
    }

}

void NodeList::processPingReplyPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    // activate the appropriate socket for this node, if not yet updated
    activateSocketFromNodeCommunication(*message, sendingNode);

    // set the ping time for this node for stat collection
    timePingReply(*message, sendingNode);
}

void NodeList::processICEPingPacket(QSharedPointer<ReceivedMessage> message) {
    // send back a reply
    auto replyPacket = constructICEPingReplyPacket(*message, _domainHandler.getICEClientID());
    sendPacket(std::move(replyPacket), message->getSenderSockAddr());
}

void NodeList::reset(QString reason, bool skipDomainHandlerReset) {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "reset",
                                  Q_ARG(QString, reason),
                                  Q_ARG(bool, skipDomainHandlerReset));
        return;
    }
    LimitedNodeList::reset(reason);

    // lock and clear our set of ignored IDs
    _ignoredSetLock.lockForWrite();
    _ignoredNodeIDs.clear();
    _ignoredSetLock.unlock();
    // lock and clear our set of personally muted IDs
    _personalMutedSetLock.lockForWrite();
    _personalMutedNodeIDs.clear();
    _personalMutedSetLock.unlock();

    // lock and clear out set of avatarGains
    _avatarGainMapLock.lockForWrite();
    _avatarGainMap.clear();
    _avatarGainMapLock.unlock();

    if (!skipDomainHandlerReset) {
        // clear the domain connection information, unless they're the ones that asked us to reset
        _domainHandler.softReset(reason);
    }

    // refresh the owner UUID to the NULL UUID
    setSessionUUID(QUuid());
    setSessionLocalID(Node::NULL_LOCAL_ID);

    // if we setup the DTLS socket, also disconnect from the DTLS socket readyRead() so it can handle handshaking
    if (_dtlsSocket) {
        disconnect(_dtlsSocket, 0, this, 0);
    }
}

void NodeList::addNodeTypeToInterestSet(NodeType_t nodeTypeToAdd) {
    _nodeTypesOfInterest << nodeTypeToAdd;
}

void NodeList::addSetOfNodeTypesToNodeInterestSet(const NodeSet& setOfNodeTypes) {
    _nodeTypesOfInterest.unite(setOfNodeTypes);
}

void NodeList::sendDomainServerCheckIn() {

    // On ThreadedAssignments (assignment clients), this function
    // is called by the server check-in timer thread
    // not the NodeList thread.  Calling it on the NodeList thread
    // resulted in starvation of the server check-in function.
    // be VERY CAREFUL modifying this code as members of NodeList
    // may be called by multiple threads.

    if (!_sendDomainServerCheckInEnabled) {
        static const QString DISABLED_CHECKIN_DEBUG{ "Refusing to send a domain-server check in while it is disabled." };
        HIFI_FCDEBUG(networking_ice(), DISABLED_CHECKIN_DEBUG);
        return;
    }

    if (_isShuttingDown) {
        qCDebug(networking_ice) << "Refusing to send a domain-server check in while shutting down.";
        return;
    }

    auto publicSockAddr = _publicSockAddr;
    auto domainHandlerIp = _domainHandler.getIP();

    if (publicSockAddr.isNull()) {
        // we don't know our public socket and we need to send it to the domain server
        qCDebug(networking_ice) << "Waiting for inital public socket from STUN. Will not send domain-server check in.";
    } else if (domainHandlerIp.isNull() && _domainHandler.requiresICE()) {
        qCDebug(networking_ice) << "Waiting for ICE discovered domain-server socket. Will not send domain-server check in.";
        handleICEConnectionToDomainServer();
        // let the domain handler know we are due to send a checkin packet
    } else if (!domainHandlerIp.isNull() && !_domainHandler.checkInPacketTimeout()) {
        bool domainIsConnected = _domainHandler.isConnected();
        SockAddr domainSockAddr = _domainHandler.getSockAddr();
        PacketType domainPacketType = !domainIsConnected
            ? PacketType::DomainConnectRequest : PacketType::DomainListRequest;

        if (!domainIsConnected) {
            auto hostname = _domainHandler.getHostname();
            QMetaEnum metaEnum = QMetaEnum::fromType<LimitedNodeList::ConnectReason>();
            qCDebug(networking_ice) << "Sending connect request ( REASON:" << QString(metaEnum.valueToKey(_connectReason)) << ") to domain-server at" << hostname;

            // is this our localhost domain-server?
            // if so we need to make sure we have an up-to-date local port in case it restarted

            if ((domainSockAddr.getAddress() == QHostAddress::LocalHost || hostname == "localhost")
                && _domainPortAutoDiscovery == false) {

                quint16 domainPort = DEFAULT_DOMAIN_SERVER_PORT;
                getLocalServerPortFromSharedMemory(DOMAIN_SERVER_LOCAL_PORT_SMEM_KEY, domainPort);
                qCDebug(networking_ice) << "Local domain-server port read from shared memory (or default) is" << domainPort;
                _domainHandler.setPort(domainPort);
            }
        }

        // check if we're missing a keypair we need to verify ourselves with the domain-server
        auto accountManager = DependencyManager::get<AccountManager>();
        const QUuid& connectionToken = _domainHandler.getConnectionToken();

        bool requiresUsernameSignature = !domainIsConnected && !connectionToken.isNull();

        if (requiresUsernameSignature && !accountManager->getAccountInfo().hasPrivateKey()) {
            qCWarning(networking_ice) << "A keypair is required to present a username signature to the domain-server"
                << "but no keypair is present. Waiting for keypair generation to complete.";
            accountManager->generateNewUserKeypair();

            // don't send the check in packet - wait for the new public key to be available to the domain-server first
            return;
        }

        auto domainPacket = NLPacket::create(domainPacketType);

        QDataStream packetStream(domainPacket.get());

        SockAddr localSockAddr = _localSockAddr;
        if (domainPacketType == PacketType::DomainConnectRequest) {

#if (PR_BUILD || DEV_BUILD)
            // #######
            if (_shouldSendNewerVersion) {
                domainPacket->setVersion(versionForPacketType(domainPacketType) + 1);
            }
#endif

            QUuid connectUUID = _domainHandler.getAssignmentUUID();

            if (connectUUID.isNull() && _domainHandler.requiresICE()) {
                // this is a connect request and we're an interface client
                // that used ice to discover the DS
                // so send our ICE client UUID with the connect request
                connectUUID = _domainHandler.getICEClientID();
            }

            // pack the connect UUID for this connect request
            packetStream << connectUUID;

            // include the protocol version signature in our connect request
            QByteArray protocolVersionSig = protocolVersionsSignature();
            packetStream.writeBytes(protocolVersionSig.constData(), protocolVersionSig.size());

            // if possible, include the MAC address for the current interface in our connect request
            QString hardwareAddress;
            for (auto networkInterface : QNetworkInterface::allInterfaces()) {
                for (auto interfaceAddress : networkInterface.addressEntries()) {
                    if (interfaceAddress.ip() == localSockAddr.getAddress()) {
                        // this is the interface whose local IP matches what we've detected the current IP to be
                        hardwareAddress = networkInterface.hardwareAddress();

                        // stop checking interfaces and addresses
                        break;
                    }
                }

                // stop looping if this was the current interface
                if (!hardwareAddress.isEmpty()) {
                    break;
                }
            }

            packetStream << hardwareAddress;

            // now add the machine fingerprint
            auto accountManager = DependencyManager::get<AccountManager>();
            packetStream << FingerprintUtils::getMachineFingerprint();

            platform::json all = platform::getAll();
            platform::json desc;
            // only pull out those items that will fit within a packet
            desc[platform::keys::COMPUTER] = all[platform::keys::COMPUTER];
            desc[platform::keys::MEMORY] = all[platform::keys::MEMORY];
            desc[platform::keys::CPUS] = all[platform::keys::CPUS];
            desc[platform::keys::GPUS] = all[platform::keys::GPUS];
            desc[platform::keys::DISPLAYS] = all[platform::keys::DISPLAYS];
            desc[platform::keys::NICS] = all[platform::keys::NICS];

            QByteArray systemInfo(desc.dump().c_str());
            QByteArray compressedSystemInfo = qCompress(systemInfo);

            if (compressedSystemInfo.size() > MAX_SYSTEM_INFO_SIZE) {
                // Highly unlikely, as not even unreasonable machines will
                // overflow the max size, but prevent MTU overflow anyway.
                // We could do something sophisticated like clearing specific
                // values if they're too big, but we'll save that for later.
                compressedSystemInfo.clear();
            }

            packetStream << compressedSystemInfo;

            packetStream << _connectReason;

            if (_nodeDisconnectTimestamp < _nodeConnectTimestamp) {
                _nodeDisconnectTimestamp = usecTimestampNow();
            }
            quint64 previousConnectionUptime = _nodeConnectTimestamp ? _nodeDisconnectTimestamp - _nodeConnectTimestamp : 0;

            packetStream << previousConnectionUptime;

        }

        packetStream << quint64(duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

        // pack our data to send to the domain-server including
        // the hostname information (so the domain-server can see which place name we came in on)
        packetStream << _ownerType.load() << publicSockAddr << localSockAddr << _nodeTypesOfInterest.toList();
        packetStream << DependencyManager::get<AddressManager>()->getPlaceName();

        if (!domainIsConnected) {

            // Metaverse account.
            DataServerAccountInfo& accountInfo = accountManager->getAccountInfo();
            packetStream << accountInfo.getUsername();
            // if this is a connect request, and we can present a username signature, send it along
            if (requiresUsernameSignature && accountManager->getAccountInfo().hasPrivateKey()) {
                const QByteArray& usernameSignature = accountManager->getAccountInfo().getUsernameSignature(connectionToken);
                packetStream << usernameSignature;
            } else {
                packetStream << QString("");  // Placeholder in case have domain username.
            }

            // Domain account.
            if (_hasDomainAccountManager) {
                auto domainAccountManager = DependencyManager::get<DomainAccountManager>();
                if (!domainAccountManager->getUsername().isEmpty() && !domainAccountManager->getAccessToken().isEmpty()) {
                    packetStream << domainAccountManager->getUsername();
                    packetStream << (domainAccountManager->getAccessToken() + ":" + domainAccountManager->getRefreshToken());
                }
            }

        }

        flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::SendDSCheckIn);

        // Send duplicate check-ins in the exponentially increasing sequence 1, 1, 2, 4, ...
        static const int MAX_CHECKINS_TOGETHER = 20;
        int outstandingCheckins = _domainHandler.getCheckInPacketsSinceLastReply();

        int checkinCount = outstandingCheckins > 1 ? std::pow(2, outstandingCheckins - 2) : 1;
        checkinCount = std::min(checkinCount, MAX_CHECKINS_TOGETHER);
        for (int i = 1; i < checkinCount; ++i) {
            auto packetCopy = domainPacket->createCopy(*domainPacket);
            sendPacket(std::move(packetCopy), domainSockAddr);
        }
        sendPacket(std::move(domainPacket), domainSockAddr);

    }
}

void NodeList::handleDSPathQuery(const QString& newPath) {
    if (_domainHandler.isServerless()) {
        if (_domainHandler.isConnected()) {
            auto viewpoint = _domainHandler.getViewPointFromNamedPath(newPath);
            if (!newPath.isEmpty()) {
                DependencyManager::get<AddressManager>()->goToViewpointForPath(viewpoint, newPath);
            }
        } else {
            _domainHandler.setPendingPath(newPath);
        }
    } else if (_domainHandler.isSocketKnown()) {
        // if we have a DS socket we assume it will get this packet and send if off right away
        sendDSPathQuery(newPath);
    } else {
        // otherwise we make it pending so that it can be sent once a connection is established
        _domainHandler.setPendingPath(newPath);
    }
}

void NodeList::sendPendingDSPathQuery() {

    QString pendingPath = _domainHandler.getPendingPath();

    if (!pendingPath.isEmpty()) {

        if (_domainHandler.isServerless()) {
            auto viewpoint = _domainHandler.getViewPointFromNamedPath(pendingPath);
            if (!pendingPath.isEmpty()) {
                DependencyManager::get<AddressManager>()->goToViewpointForPath(viewpoint, pendingPath);
            }
        } else {
            qCDebug(networking) << "Attempting to send pending query to DS for path" << pendingPath;
            // this is a slot triggered if we just established a network link with a DS and want to send a path query
            sendDSPathQuery(_domainHandler.getPendingPath());
        }

        // clear whatever the pending path was
        _domainHandler.clearPendingPath();
    }
}

void NodeList::sendDSPathQuery(const QString& newPath) {
    // only send a path query if we know who our DS is or is going to be
    if (_domainHandler.isSocketKnown()) {
        // construct the path query packet
        auto pathQueryPacket = NLPacket::create(PacketType::DomainServerPathQuery, -1, true);

        // get the UTF8 representation of path query
        QByteArray pathQueryUTF8 = newPath.toUtf8();

        // get the size of the UTF8 representation of the desired path
        quint16 numPathBytes = pathQueryUTF8.size();

        if (numPathBytes + ((qint16) sizeof(numPathBytes)) < pathQueryPacket->bytesAvailableForWrite()) {
            // append the size of the path to the query packet
            pathQueryPacket->writePrimitive(numPathBytes);

            // append the path itself to the query packet
            pathQueryPacket->write(pathQueryUTF8);

            qCDebug(networking) << "Sending a path query packet for path" << newPath << "to domain-server at"
                << _domainHandler.getSockAddr();

            // send off the path query
            sendPacket(std::move(pathQueryPacket), _domainHandler.getSockAddr());
        } else {
            qCDebug(networking) << "Path" << newPath << "would make PacketType::DomainServerPathQuery packet > MAX_PACKET_SIZE." <<
                "Will not send query.";
        }
    }
}

void NodeList::processDomainServerPathResponse(QSharedPointer<ReceivedMessage> message) {
    // This is a response to a path query we theoretically made.
    // In the future we may want to check that this was actually from our DS and for a query we actually made.

    // figure out how many bytes the path query is
    quint16 numPathBytes;
    message->readPrimitive(&numPathBytes);

    // pull the path from the packet
    if (message->getBytesLeftToRead() < numPathBytes) {
        qCDebug(networking) << "Could not read query path from DomainServerPathQueryResponse. Bailing.";
        return;
    }

    QString pathQuery = QString::fromUtf8(message->getRawMessage() + message->getPosition(), numPathBytes);
    message->seek(message->getPosition() + numPathBytes);

    // figure out how many bytes the viewpoint is
    quint16 numViewpointBytes;
    message->readPrimitive(&numViewpointBytes);

    if (message->getBytesLeftToRead() < numViewpointBytes) {
        qCDebug(networking) << "Could not read resulting viewpoint from DomainServerPathQueryReponse. Bailing";
        return;
    }

    // pull the viewpoint from the packet
    QString viewpoint = QString::fromUtf8(message->getRawMessage() + message->getPosition(), numViewpointBytes);

    // Hand it off to the AddressManager so it can handle it as a relative viewpoint
    if (!pathQuery.isEmpty() && DependencyManager::get<AddressManager>()->goToViewpointForPath(viewpoint, pathQuery)) {
        qCDebug(networking) << "Going to viewpoint" << viewpoint << "which was the lookup result for path" << pathQuery;
    } else {
        qCDebug(networking) << "Could not go to viewpoint" << viewpoint
            << "which was the lookup result for path" << pathQuery;
    }
}

void NodeList::handleICEConnectionToDomainServer() {
    // if we're still waiting to get sockets we want to ping for the domain-server
    // then send another heartbeat now
    if (!_domainHandler.getICEPeer().hasSockets()) {

        _domainHandler.getICEPeer().resetConnectionAttempts();

        flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::SendICEServerQuery);

        LimitedNodeList::sendPeerQueryToIceServer(_domainHandler.getICEServerSockAddr(),
                                                  _domainHandler.getICEClientID(),
                                                  _domainHandler.getPendingDomainID());
    }
}

void NodeList::pingPunchForDomainServer() {
    // make sure if we're here that we actually still need to ping the domain-server
    if (_domainHandler.getIP().isNull() && _domainHandler.getICEPeer().hasSockets()) {

        // check if we've hit the number of pings we'll send to the DS before we consider it a fail
        const int NUM_DOMAIN_SERVER_PINGS_BEFORE_RESET = 2000 / UDP_PUNCH_PING_INTERVAL_MS;

        if (_domainHandler.getICEPeer().getConnectionAttempts() == 0) {
            qCDebug(networking_ice) << "Sending ping packets to establish connectivity with domain-server with ID"
                << uuidStringWithoutCurlyBraces(_domainHandler.getPendingDomainID());
        } else {
            if (_domainHandler.getICEPeer().getConnectionAttempts() % NUM_DOMAIN_SERVER_PINGS_BEFORE_RESET == 0) {
                // if we have then nullify the domain handler's network peer and send a fresh ICE heartbeat
                qCDebug(networking_ice) << "No ping replies received from domain-server with ID"
                    << uuidStringWithoutCurlyBraces(_domainHandler.getICEClientID()) << "-" << "re-sending ICE query.";

                _domainHandler.getICEPeer().softReset();
                handleICEConnectionToDomainServer();

                return;
            }
        }

        flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::SendPingsToDS);

        // send the ping packet to the local and public sockets for this node
        auto localPingPacket = constructICEPingPacket(PingType::Local, _domainHandler.getICEClientID());
        sendPacket(std::move(localPingPacket), _domainHandler.getICEPeer().getLocalSocket());

        auto publicPingPacket = constructICEPingPacket(PingType::Public, _domainHandler.getICEClientID());
        sendPacket(std::move(publicPingPacket), _domainHandler.getICEPeer().getPublicSocket());

        _domainHandler.getICEPeer().incrementConnectionAttempts();
    }
}

void NodeList::processDomainServerConnectionTokenPacket(QSharedPointer<ReceivedMessage> message) {
    if (_domainHandler.getSockAddr().isNull()) {
        // refuse to process this packet if we aren't currently connected to the DS
        return;
    }
    // read in the connection token from the packet, then send domain-server checkin
    _domainHandler.setConnectionToken(QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID)));

    _domainHandler.clearPendingCheckins();
    sendDomainServerCheckIn();
}

void NodeList::processDomainServerList(QSharedPointer<ReceivedMessage> message) {

    // parse header information
    QDataStream packetStream(message->getMessage());

    // grab the domain's ID from the beginning of the packet
    QUuid domainUUID;
    packetStream >> domainUUID;

    Node::LocalID domainLocalID;
    packetStream >> domainLocalID;

    // pull our owner (ie. session) UUID from the packet, it's always the first thing
    // The short (16 bit) ID comes next.
    QUuid newUUID;
    Node::LocalID newLocalID;
    packetStream >> newUUID;
    packetStream >> newLocalID;

    // pull the permissions/right/privileges for this node out of the stream
    NodePermissions newPermissions;
    packetStream >> newPermissions;
    // FIXME: Can remove this temporary work-around in version 2021.2.0. (New protocol version implies a domain server upgrade.)
    // Adjust our canRezAvatarEntities permissions on older domains that do not have this setting.
    // DomainServerList and DomainSettings packets can come in either order so need to adjust with both occurrences.
    bool adjustedPermissions = adjustCanRezAvatarEntitiesPermissions(_domainHandler.getSettingsObject(), newPermissions, false);

    // Is packet authentication enabled?
    bool isAuthenticated;
    packetStream >> isAuthenticated;

    qint64 now = qint64(duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

    quint64 connectRequestTimestamp;
    packetStream >> connectRequestTimestamp;

    quint64 domainServerPingSendTime;
    packetStream >> domainServerPingSendTime;

    quint64 domainServerCheckinProcessingTime;
    packetStream >> domainServerCheckinProcessingTime;

    bool newConnection;
    packetStream >> newConnection;

    if (newConnection) {
        _nodeConnectTimestamp = usecTimestampNow();
        _connectReason = Connect;
    }

    qint64 pingLagTime = (now - qint64(connectRequestTimestamp)) / qint64(USECS_PER_MSEC);

    qint64 domainServerRequestLag = (qint64(domainServerPingSendTime - domainServerCheckinProcessingTime) - qint64(connectRequestTimestamp)) / qint64(USECS_PER_MSEC);;
    qint64 domainServerResponseLag = (now - qint64(domainServerPingSendTime)) / qint64(USECS_PER_MSEC);

    if (_domainHandler.getSockAddr().isNull()) {
        qWarning(networking) << "IGNORING DomainList packet while not connected to a Domain Server: sent " << pingLagTime << " msec ago.";
        qWarning(networking) << "DomainList request lag (interface->ds): " << domainServerRequestLag << "msec";
        qWarning(networking) << "DomainList server processing time: " << domainServerCheckinProcessingTime << "usec";
        qWarning(networking) << "DomainList response lag (ds->interface): " << domainServerResponseLag << "msec";
        // refuse to process this packet if we aren't currently connected to the DS
        return;
    }

    // warn if ping lag is getting long
    if (pingLagTime > qint64(MSECS_PER_SECOND)) {
        qCDebug(networking) << "DomainList ping is lagging: " << pingLagTime << "msec";
        qCDebug(networking) << "DomainList request lag (interface->ds): " << domainServerRequestLag << "msec";
        qCDebug(networking) << "DomainList server processing time: " << domainServerCheckinProcessingTime << "usec";
        qCDebug(networking) << "DomainList response lag (ds->interface): " << domainServerResponseLag << "msec";
    }

    // this is a packet from the domain server, reset the count of un-replied check-ins
    _domainHandler.clearPendingCheckins();
    setDropOutgoingNodeTraffic(false);

    // emit our signal so listeners know we just heard from the DS
    emit receivedDomainServerList();

    DependencyManager::get<NodeList>()->flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::ReceiveDSList);

    if (_domainHandler.isConnected() && _domainHandler.getUUID() != domainUUID) {
        // Received packet from different domain.
        qWarning() << "IGNORING DomainList packet from" << domainUUID << "while connected to"
                   << _domainHandler.getUUID() << ": sent " << pingLagTime << " msec ago.";
        qWarning(networking) << "DomainList request lag (interface->ds): " << domainServerRequestLag << "msec";
        qWarning(networking) << "DomainList server processing time: " << domainServerCheckinProcessingTime << "usec";
        qWarning(networking) << "DomainList response lag (ds->interface): " << domainServerResponseLag << "msec";
        return;
    }

    // when connected, if the session ID or local ID were not null and changed, we should reset
    auto currentLocalID = getSessionLocalID();
    auto currentSessionID = getSessionUUID();
    if (_domainHandler.isConnected() &&
        ((currentLocalID != Node::NULL_LOCAL_ID && newLocalID != currentLocalID) ||
        (!currentSessionID.isNull() && newUUID != currentSessionID))) {
            // reset the nodelist, but don't do a domain handler reset since we're about to process a good domain list
            reset("Local ID or Session ID changed while connected to domain - forcing NodeList reset", true);

            // tell the domain handler that we're no longer connected so that below
            // it can re-perform actions as if we just connected
            _domainHandler.setIsConnected(false);
            // Clear any reliable connections using old ID.
            _nodeSocket.clearConnections();
    }

    setSessionLocalID(newLocalID);
    setSessionUUID(newUUID);

    // FIXME: Remove this call to requestDomainSettings() and reinstate the one in DomainHandler::setIsConnected(), in version
    // 2021.2.0. (New protocol version implies a domain server upgrade.)
    if (!_domainHandler.isConnected() 
            && _domainHandler.getScheme() == URL_SCHEME_VIRCADIA && !_domainHandler.getHostname().isEmpty()) {
        // We're about to connect but we need the domain settings (in particular, the node permissions) in order to adjust the
        // canRezAvatarEntities permission above before using the permissions in determining whether or not to connect without
        // avatar entities rezzing below.
        _domainHandler.requestDomainSettings();
    }

    // Don't continue login to the domain if have avatar entities and don't have permissions to rez them, unless user has OKed
    // continuing login.
    if (!newPermissions.can(NodePermissions::Permission::canRezAvatarEntities)
            && (!adjustedPermissions || !_domainHandler.canConnectWithoutAvatarEntities())) {
        return;
    }

    // if this was the first domain-server list from this domain, we've now connected
    if (!_domainHandler.isConnected()) {
        _domainHandler.setLocalID(domainLocalID);
        _domainHandler.setUUID(domainUUID);
        _domainHandler.setIsConnected(true);

        // in case we didn't use a place name to get to this domain,
        // give the address manager a chance to lookup a default one now
        DependencyManager::get<AddressManager>()->lookupShareableNameForDomainID(domainUUID);
    }

    setPermissions(newPermissions);
    setAuthenticatePackets(isAuthenticated);

    // pull each node in the packet
    while (packetStream.device()->pos() < message->getSize()) {
        parseNodeFromPacketStream(packetStream);
    }
}

void NodeList::processDomainServerAddedNode(QSharedPointer<ReceivedMessage> message) {
    // setup a QDataStream
    QDataStream packetStream(message->getMessage());

    // use our shared method to pull out the new node
    parseNodeFromPacketStream(packetStream);
}

void NodeList::processDomainServerRemovedNode(QSharedPointer<ReceivedMessage> message) {
    // read the UUID from the packet, remove it if it exists
    QUuid nodeUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));
    qCDebug(networking) << "Received packet from domain-server to remove node with UUID" << uuidStringWithoutCurlyBraces(nodeUUID);
    killNodeWithUUID(nodeUUID);
    removeDelayedAdd(nodeUUID);
}

void NodeList::parseNodeFromPacketStream(QDataStream& packetStream) {
    NewNodeInfo info;

    packetStream >> info.type
                 >> info.uuid
                 >> info.publicSocket
                 >> info.localSocket
                 >> info.permissions
                 >> info.isReplicated
                 >> info.sessionLocalID
                 >> info.connectionSecretUUID;

    // if the public socket address is 0 then it's reachable at the same IP
    // as the domain server
    if (info.publicSocket.getAddress().isNull()) {
        info.publicSocket.setAddress(_domainHandler.getIP());
    }

    addNewNode(info);
}

void NodeList::sendAssignment(Assignment& assignment) {

    PacketType assignmentPacketType = assignment.getCommand() == Assignment::CreateCommand
        ? PacketType::CreateAssignment
        : PacketType::RequestAssignment;

    auto assignmentPacket = NLPacket::create(assignmentPacketType);

    QDataStream packetStream(assignmentPacket.get());
    packetStream << assignment;

    sendPacket(std::move(assignmentPacket), _assignmentServerSocket);
}

void NodeList::pingPunchForInactiveNode(const SharedNodePointer& node) {
    if (node->getType() == NodeType::AudioMixer) {
        flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::SendAudioPing);
    }

    // every two seconds we're trying to ping this node and we're not getting anywhere - debug that out
    const int NUM_DEBUG_CONNECTION_ATTEMPTS = 2000 / (UDP_PUNCH_PING_INTERVAL_MS);

    if (node->getConnectionAttempts() > 0 && node->getConnectionAttempts() % NUM_DEBUG_CONNECTION_ATTEMPTS == 0) {
        qCDebug(networking) << "No response to UDP hole punch pings for node" << node->getUUID() << "in last 2 s.";
    }

    auto nodeID = node->getUUID();

    // send the ping packet to the local and public sockets for this node
    auto localPingPacket = constructPingPacket(nodeID, PingType::Local);
    sendPacket(std::move(localPingPacket), *node, node->getLocalSocket());

    auto publicPingPacket = constructPingPacket(nodeID, PingType::Public);
    sendPacket(std::move(publicPingPacket), *node, node->getPublicSocket());

    if (!node->getSymmetricSocket().isNull()) {
        auto symmetricPingPacket = constructPingPacket(nodeID, PingType::Symmetric);
        sendPacket(std::move(symmetricPingPacket), *node, node->getSymmetricSocket());
    }

    node->incrementConnectionAttempts();
}

void NodeList::startNodeHolePunch(const SharedNodePointer& node) {
    // we don't hole punch to downstream servers, since it is assumed that we have a direct line to them
    // we also don't hole punch to relayed upstream nodes, since we do not communicate directly with them

    if (!NodeType::isDownstream(node->getType()) && !node->isUpstream()) {
        // connect to the correct signal on this node so we know when to ping it
        connect(node.data(), &Node::pingTimerTimeout, this, &NodeList::handleNodePingTimeout);

        // start the ping timer for this node
        node->startPingTimer();

        // ping this node immediately
        pingPunchForInactiveNode(node);
    }

    // nodes that are downstream or upstream of our own type are kept alive when we hear about them from the domain server
    // and always have their public socket as their active socket
    if (node->getType() == NodeType::downstreamType(_ownerType) || node->getType() == NodeType::upstreamType(_ownerType)) {
        node->setLastHeardMicrostamp(usecTimestampNow());
        node->activatePublicSocket();
    }

}

void NodeList::handleNodePingTimeout() {
    Node* senderNode = qobject_cast<Node*>(sender());
    if (senderNode) {
        SharedNodePointer sharedNode = nodeWithUUID(senderNode->getUUID());

        if (sharedNode && !sharedNode->getActiveSocket()) {
            pingPunchForInactiveNode(sharedNode);
        }
    }
}

void NodeList::activateSocketFromNodeCommunication(ReceivedMessage& message, const SharedNodePointer& sendingNode) {
    // deconstruct this ping packet to see if it is a public or local reply
    QDataStream packetStream(message.getMessage());

    quint8 pingType;
    packetStream >> pingType;

    // if this is a local or public ping then we can activate a socket
    // we do nothing with agnostic pings, those are simply for timing
    if (pingType == PingType::Local && sendingNode->getActiveSocket() != &sendingNode->getLocalSocket()) {
        sendingNode->activateLocalSocket();
    } else if (pingType == PingType::Public && !sendingNode->getActiveSocket()) {
        sendingNode->activatePublicSocket();
    } else if (pingType == PingType::Symmetric && !sendingNode->getActiveSocket()) {
        sendingNode->activateSymmetricSocket();
    }

    if (sendingNode->getType() == NodeType::AudioMixer) {
       flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::SetAudioMixerSocket);
    }
}

void NodeList::stopKeepalivePingTimer() {
    if (_keepAlivePingTimer.isActive()) {
        _keepAlivePingTimer.stop();
    }
}

void NodeList::sendKeepAlivePings() {
    // send keep-alive ping packets to nodes of types we care about that are not relayed to us from an upstream node

    eachMatchingNode([this](const SharedNodePointer& node)->bool {
        auto type = node->getType();
        return !node->isUpstream() && _nodeTypesOfInterest.contains(type) && !NodeType::isDownstream(type);
    }, [&](const SharedNodePointer& node) {
        sendPacket(constructPingPacket(node->getUUID()), *node);
    });
}

bool NodeList::sockAddrBelongsToDomainOrNode(const SockAddr& sockAddr) {
    return _domainHandler.getSockAddr() == sockAddr || LimitedNodeList::sockAddrBelongsToNode(sockAddr);
}

void NodeList::ignoreNodesInRadius(bool enabled) {
    bool isEnabledChange = _ignoreRadiusEnabled.get() != enabled;
    _ignoreRadiusEnabled.set(enabled);

    eachMatchingNode([](const SharedNodePointer& node)->bool {
        return (node->getType() == NodeType::AudioMixer || node->getType() == NodeType::AvatarMixer);
    }, [this](const SharedNodePointer& destinationNode) {
        sendIgnoreRadiusStateToNode(destinationNode);
    });
    if (isEnabledChange) {
        emit ignoreRadiusEnabledChanged(enabled);
    }
}

void NodeList::sendIgnoreRadiusStateToNode(const SharedNodePointer& destinationNode) {
    auto ignorePacket = NLPacket::create(PacketType::RadiusIgnoreRequest, sizeof(bool) + sizeof(float), true);
    ignorePacket->writePrimitive(_ignoreRadiusEnabled.get());
    sendPacket(std::move(ignorePacket), *destinationNode);
}

void NodeList::ignoreNodeBySessionID(const QUuid& nodeID, bool ignoreEnabled) {
    // enumerate the nodes to send a reliable ignore packet to each that can leverage it
    if (!nodeID.isNull() && getSessionUUID() != nodeID) {
        eachMatchingNode([](const SharedNodePointer& node)->bool {
            if (node->getType() == NodeType::AudioMixer || node->getType() == NodeType::AvatarMixer) {
                return true;
            } else {
                return false;
            }
        }, [&nodeID, ignoreEnabled, this](const SharedNodePointer& destinationNode) {
            // create a reliable NLPacket with space for the ignore UUID
            auto ignorePacket = NLPacket::create(PacketType::NodeIgnoreRequest, NUM_BYTES_RFC4122_UUID + sizeof(bool), true);

            ignorePacket->writePrimitive(ignoreEnabled);
            // write the node ID to the packet
            ignorePacket->write(nodeID.toRfc4122());

            qCDebug(networking) << "Sending packet to" << (destinationNode->getType() == NodeType::AudioMixer ? "AudioMixer" : "AvatarMixer") << "to"
                << (ignoreEnabled ? "ignore" : "unignore") << "node" << uuidStringWithoutCurlyBraces(nodeID);

            // send off this ignore packet reliably to the matching node
            sendPacket(std::move(ignorePacket), *destinationNode);
        });

        if (ignoreEnabled) {
            {
                QReadLocker ignoredSetLocker{ &_ignoredSetLock }; // read lock for insert
                // add this nodeID to our set of ignored IDs
                _ignoredNodeIDs.insert(nodeID);
            }
            {
                QReadLocker personalMutedSetLocker{ &_personalMutedSetLock }; // read lock for insert
                // add this nodeID to our set of personal muted IDs
                _personalMutedNodeIDs.insert(nodeID);
            }
            emit ignoredNode(nodeID, true);
        } else {
            {
                QWriteLocker ignoredSetLocker{ &_ignoredSetLock }; // write lock for unsafe_erase
                _ignoredNodeIDs.unsafe_erase(nodeID);
            }
            {
                QWriteLocker personalMutedSetLocker{ &_personalMutedSetLock }; // write lock for unsafe_erase
                _personalMutedNodeIDs.unsafe_erase(nodeID);
            }
            emit ignoredNode(nodeID, false);
        }

    } else {
        qWarning() << "NodeList::ignoreNodeBySessionID called with an invalid ID or an ID which matches the current session ID.";
    }
}

void NodeList::removeFromIgnoreMuteSets(const QUuid& nodeID) {
    // don't remove yourself, or nobody
    if (!nodeID.isNull() && getSessionUUID() != nodeID) {
        {
            QWriteLocker ignoredSetLocker{ &_ignoredSetLock };
            _ignoredNodeIDs.unsafe_erase(nodeID);
        }
        {
            QWriteLocker personalMutedSetLocker{ &_personalMutedSetLock };
            _personalMutedNodeIDs.unsafe_erase(nodeID);
        }
    }
}

bool NodeList::isIgnoringNode(const QUuid& nodeID) const {
    QReadLocker ignoredSetLocker{ &_ignoredSetLock };
    return _ignoredNodeIDs.find(nodeID) != _ignoredNodeIDs.cend();
}

void NodeList::personalMuteNodeBySessionID(const QUuid& nodeID, bool muteEnabled) {
    // cannot personal mute yourself, or nobody
    if (!nodeID.isNull() && getSessionUUID() != nodeID) {
        auto audioMixer = soloNodeOfType(NodeType::AudioMixer);
        if (audioMixer) {
            if (isIgnoringNode(nodeID)) {
                qCDebug(networking) << "You can't personally mute or unmute a node you're already ignoring.";
            }
            else {
                // setup the packet
                auto personalMutePacket = NLPacket::create(PacketType::NodeIgnoreRequest, NUM_BYTES_RFC4122_UUID + sizeof(bool), true);

                personalMutePacket->writePrimitive(muteEnabled);
                // write the node ID to the packet
                personalMutePacket->write(nodeID.toRfc4122());

                qCDebug(networking) << "Sending Personal Mute Packet to" << (muteEnabled ? "mute" : "unmute") << "node" << uuidStringWithoutCurlyBraces(nodeID);

                sendPacket(std::move(personalMutePacket), *audioMixer);


                if (muteEnabled) {
                    QReadLocker personalMutedSetLocker{ &_personalMutedSetLock }; // read lock for insert
                    // add this nodeID to our set of personal muted IDs
                    _personalMutedNodeIDs.insert(nodeID);
                } else {
                    QWriteLocker personalMutedSetLocker{ &_personalMutedSetLock }; // write lock for unsafe_erase
                    _personalMutedNodeIDs.unsafe_erase(nodeID);
                }
            }
        } else {
            qWarning() << "Couldn't find audio mixer to send node personal mute request";
        }
    } else {
        qWarning() << "NodeList::personalMuteNodeBySessionID called with an invalid ID or an ID which matches the current session ID.";
    }
}

bool NodeList::isPersonalMutingNode(const QUuid& nodeID) const {
    QReadLocker personalMutedSetLocker{ &_personalMutedSetLock };
    return _personalMutedNodeIDs.find(nodeID) != _personalMutedNodeIDs.cend();
}

void NodeList::maybeSendIgnoreSetToNode(SharedNodePointer newNode) {
    if (newNode->getType() == NodeType::AudioMixer) {
        // this is a mixer that we just added - it's unlikely it knows who we were previously ignoring in this session,
        // so send that list along now (assuming it isn't empty)

        QReadLocker personalMutedSetLocker{ &_personalMutedSetLock };

        if (_personalMutedNodeIDs.size() > 0) {
            // setup a packet list so we can send the stream of ignore IDs
            auto personalMutePacketList = NLPacketList::create(PacketType::NodeIgnoreRequest, QByteArray(), true, true);

            // Force the "enabled" flag in this packet to true
            personalMutePacketList->writePrimitive(true);

            // enumerate the ignored IDs and write them to the packet list
            auto it = _personalMutedNodeIDs.cbegin();
            while (it != _personalMutedNodeIDs.end()) {
                personalMutePacketList->write(it->toRfc4122());
                ++it;
            }

            // send this NLPacketList to the new node
            sendPacketList(std::move(personalMutePacketList), *newNode);
        }

        // also send them the current ignore radius state.
        sendIgnoreRadiusStateToNode(newNode);

        // also send the current avatar and injector gains
        if (_avatarGain != 0.0f) {
            setAvatarGain(QUuid(), _avatarGain);
        }
        if (_injectorGain != 0.0f) {
            setInjectorGain(_injectorGain);
        }
    }
    if (newNode->getType() == NodeType::AvatarMixer) {
        // this is a mixer that we just added - it's unlikely it knows who we were previously ignoring in this session,
        // so send that list along now (assuming it isn't empty)

        QReadLocker ignoredSetLocker{ &_ignoredSetLock };

        if (_ignoredNodeIDs.size() > 0) {
            // setup a packet list so we can send the stream of ignore IDs
            auto ignorePacketList = NLPacketList::create(PacketType::NodeIgnoreRequest, QByteArray(), true, true);

            // Force the "enabled" flag in this packet to true
            ignorePacketList->writePrimitive(true);

            // enumerate the ignored IDs and write them to the packet list
            auto it = _ignoredNodeIDs.cbegin();
            while (it != _ignoredNodeIDs.end()) {
                ignorePacketList->write(it->toRfc4122());
                ++it;
            }

            // send this NLPacketList to the new node
            sendPacketList(std::move(ignorePacketList), *newNode);
        }

        // also send them the current ignore radius state.
        sendIgnoreRadiusStateToNode(newNode);
    }
}

void NodeList::setAvatarGain(const QUuid& nodeID, float gain) {
    if (nodeID.isNull()) {
        _avatarGain = gain;
    }

    // cannot set gain of yourself
    if (getSessionUUID() != nodeID) {
        auto audioMixer = soloNodeOfType(NodeType::AudioMixer);
        if (audioMixer) {
            // setup the packet
            auto setAvatarGainPacket = NLPacket::create(PacketType::PerAvatarGainSet, NUM_BYTES_RFC4122_UUID + sizeof(float), true);

            // write the node ID to the packet
            setAvatarGainPacket->write(nodeID.toRfc4122());

            // We need to convert the gain in dB (from the script) to an amplitude before packing it.
            setAvatarGainPacket->writePrimitive(packFloatGainToByte(fastExp2f(gain / 6.02059991f)));

            if (nodeID.isNull()) {
                qCDebug(networking) << "Sending Set MASTER Avatar Gain packet with Gain:" << gain;

                sendPacket(std::move(setAvatarGainPacket), *audioMixer);

            } else {
                qCDebug(networking) << "Sending Set Avatar Gain packet with UUID:" << uuidStringWithoutCurlyBraces(nodeID) << "Gain:" << gain;

                sendPacket(std::move(setAvatarGainPacket), *audioMixer);
                QWriteLocker lock{ &_avatarGainMapLock };
                _avatarGainMap[nodeID] = gain;
            }

        } else {
            qWarning() << "Couldn't find audio mixer to send set gain request";
        }
    } else {
        qWarning() << "NodeList::setAvatarGain called with an ID which matches the current session ID:" << nodeID;
    }
}

float NodeList::getAvatarGain(const QUuid& nodeID) {
    if (nodeID.isNull()) {
        return _avatarGain;
    } else {
        QReadLocker lock{ &_avatarGainMapLock };
        auto it = _avatarGainMap.find(nodeID);
        if (it != _avatarGainMap.cend()) {
            return it->second;
        }
    }
    return 0.0f;
}

void NodeList::setInjectorGain(float gain) {
    _injectorGain = gain;

    auto audioMixer = soloNodeOfType(NodeType::AudioMixer);
    if (audioMixer) {
        // setup the packet
        auto setInjectorGainPacket = NLPacket::create(PacketType::InjectorGainSet, sizeof(float), true);

        // We need to convert the gain in dB (from the script) to an amplitude before packing it.
        setInjectorGainPacket->writePrimitive(packFloatGainToByte(fastExp2f(gain / 6.02059991f)));

        qCDebug(networking) << "Sending Set Injector Gain packet with Gain:" << gain;

        sendPacket(std::move(setInjectorGainPacket), *audioMixer);

    } else {
        qWarning() << "Couldn't find audio mixer to send set gain request";
    }
}

float NodeList::getInjectorGain() {
    return _injectorGain;
}

void NodeList::kickNodeBySessionID(const QUuid& nodeID, unsigned int banFlags) {
    // send a request to domain-server to kick the node with the given session ID
    // the domain-server will handle the persistence of the kick (via username or IP)

    if (!nodeID.isNull() && getSessionUUID() != nodeID ) {
        if (getThisNodeCanKick()) {
            // setup the packet
            auto kickPacket = NLPacket::create(PacketType::NodeKickRequest, NUM_BYTES_RFC4122_UUID + sizeof(int), true);

            // write the node ID to the packet
            kickPacket->write(nodeID.toRfc4122());
            // write the ban parameters to the packet
            kickPacket->writePrimitive(banFlags);

            qCDebug(networking) << "Sending packet to kick node" << uuidStringWithoutCurlyBraces(nodeID);

            sendPacket(std::move(kickPacket), _domainHandler.getSockAddr());
        } else {
            qWarning() << "You do not have permissions to kick in this domain."
                << "Request to kick node" << uuidStringWithoutCurlyBraces(nodeID) << "will not be sent";
        }
    } else {
        qWarning() << "NodeList::kickNodeBySessionID called with an invalid ID or an ID which matches the current session ID.";

    }
}

void NodeList::muteNodeBySessionID(const QUuid& nodeID) {
    // cannot mute yourself, or nobody
    if (!nodeID.isNull() && getSessionUUID() != nodeID ) {
        if (getThisNodeCanKick()) {
            auto audioMixer = soloNodeOfType(NodeType::AudioMixer);
            if (audioMixer) {
                // setup the packet
                auto mutePacket = NLPacket::create(PacketType::NodeMuteRequest, NUM_BYTES_RFC4122_UUID, true);

                // write the node ID to the packet
                mutePacket->write(nodeID.toRfc4122());

                qCDebug(networking) << "Sending packet to mute node" << uuidStringWithoutCurlyBraces(nodeID);

                sendPacket(std::move(mutePacket), *audioMixer);
            } else {
                qWarning() << "Couldn't find audio mixer to send node mute request";
            }
        } else {
            qWarning() << "You do not have permissions to mute in this domain."
                << "Request to mute node" << uuidStringWithoutCurlyBraces(nodeID) << "will not be sent";
        }
    } else {
        qWarning() << "NodeList::muteNodeBySessionID called with an invalid ID or an ID which matches the current session ID.";

    }
}

void NodeList::requestUsernameFromSessionID(const QUuid& nodeID) {
    // send a request to domain-server to get the username/machine fingerprint/admin status associated with the given session ID
    // If the requesting user isn't an admin, the username and machine fingerprint will return "".
    auto usernameFromIDRequestPacket = NLPacket::create(PacketType::UsernameFromIDRequest, NUM_BYTES_RFC4122_UUID, true);

    // write the node ID to the packet
    if (nodeID.isNull()) {
        usernameFromIDRequestPacket->write(getSessionUUID().toRfc4122());
    } else {
        usernameFromIDRequestPacket->write(nodeID.toRfc4122());
    }

    qCDebug(networking) << "Sending packet to get username/fingerprint/admin status of node" << uuidStringWithoutCurlyBraces(nodeID);

    sendPacket(std::move(usernameFromIDRequestPacket), _domainHandler.getSockAddr());
}

void NodeList::processUsernameFromIDReply(QSharedPointer<ReceivedMessage> message) {
    // read the UUID from the packet
    QString nodeUUIDString = (QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID))).toString();
    // read the username from the packet
    QString username = message->readString();
    // read the machine fingerprint from the packet
    QString machineFingerprintString = (QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID))).toString();
    bool isAdmin;
    message->readPrimitive(&isAdmin);

    qCDebug(networking) << "Got username" << username << "and machine fingerprint"
        << machineFingerprintString << "for node" << nodeUUIDString << ". isAdmin:" << isAdmin;

    emit usernameFromIDReply(nodeUUIDString, username, machineFingerprintString, isAdmin);
}

void NodeList::setRequestsDomainListData(bool isRequesting) {
    // Tell the avatar mixer and audio mixer whether I want to receive any additional data to which I might be entitled
    if (_requestsDomainListData == isRequesting) {
        return;
    }
    eachMatchingNode([](const SharedNodePointer& node)->bool {
        return (node->getType() == NodeType::AudioMixer || node->getType() == NodeType::AvatarMixer);
    }, [this, isRequesting](const SharedNodePointer& destinationNode) {
        auto packet = NLPacket::create(PacketType::RequestsDomainListData, sizeof(bool), true); // reliable
        packet->writePrimitive(isRequesting);
        sendPacket(std::move(packet), *destinationNode);
    });
    _requestsDomainListData = isRequesting;
}


void NodeList::startThread() {
    moveToNewNamedThread(this, "NodeList Thread", QThread::TimeCriticalPriority);
}


// FIXME: Can remove this work-around in version 2021.2.0. (New protocol version implies a domain server upgrade.)
bool NodeList::adjustCanRezAvatarEntitiesPermissions(const QJsonObject& domainSettingsObject,
        NodePermissions& permissions, bool notify) {

    if (domainSettingsObject.isEmpty()) {
        // Don't have enough information to adjust yet.
        return false;  // Failed to adjust.
    }

    const double CANREZAVATARENTITIES_INTRODUCED_VERSION = 2.5;
    auto version = domainSettingsObject.value("version");
    if (version.isUndefined() || version.isDouble() && version.toDouble() < CANREZAVATARENTITIES_INTRODUCED_VERSION) {
        // On domains without the canRezAvatarEntities permission available, set it to the same as canConnectToDomain.
        if (permissions.can(NodePermissions::Permission::canConnectToDomain)) {
            if (!permissions.can(NodePermissions::Permission::canRezAvatarEntities)) {
                permissions.set(NodePermissions::Permission::canRezAvatarEntities);
                if (notify) {
                    emit canRezAvatarEntitiesChanged(permissions.can(NodePermissions::Permission::canRezAvatarEntities));
                }
            }
        }
    }

    return true;  // Successfully adjusted.
}

// FIXME: Can remove this work-around in version 2021.2.0. (New protocol version implies a domain server upgrade.)
void NodeList::adjustCanRezAvatarEntitiesPerSettings(const QJsonObject& domainSettingsObject) {
    adjustCanRezAvatarEntitiesPermissions(domainSettingsObject, _permissions, true);
}
