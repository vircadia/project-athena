//
//  AvatarMixer.cpp
//  assignment-client/src/avatars
//
//  Created by Stephen Birarda on 9/5/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AvatarMixer.h"

#include <cfloat>
#include <chrono>
#include <memory>
#include <random>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QJsonDocument>
#include <shared/QtHelpers.h>

#include <AABox.h>
#include <AvatarLogging.h>
#include <LogHandler.h>
#include <NodeList.h>
#include <udt/PacketHeaders.h>
#include <SharedUtil.h>
#include <UUID.h>
#include <TryLocker.h>
#include "../AssignmentDynamicFactory.h"
#include "../entities/AssignmentParentFinder.h"
#include <model-networking/ModelCache.h>
#include <hfm/ModelFormatRegistry.h>

const QString AVATAR_MIXER_LOGGING_NAME = "avatar-mixer";

// FIXME - what we'd actually like to do is send to users at ~50% of their present rate down to 30hz. Assume 90 for now.
const int AVATAR_MIXER_BROADCAST_FRAMES_PER_SECOND = 45;

const QRegularExpression AvatarMixer::suffixedNamePattern { R"(^\s*(.+)\s*_(\d)+\s*$)" };

// Lexicographic comparison:
bool AvatarMixer::SessionDisplayName::operator<(const SessionDisplayName& rhs) const {
    if (_baseName < rhs._baseName) {
        return true;
    } else if (rhs._baseName < _baseName) {
        return false;
    } else {
        return _suffix < rhs._suffix;
    }
}

AvatarMixer::AvatarMixer(ReceivedMessage& message) :
    ThreadedAssignment(message),
    _slavePool(&_slaveSharedData)
{
    DependencyManager::registerInheritance<EntityDynamicFactoryInterface, AssignmentDynamicFactory>();
    DependencyManager::set<AssignmentDynamicFactory>();
    DependencyManager::set<ModelFormatRegistry>();
    DependencyManager::set<ModelCache>();
    DependencyManager::set<ResourceCacheSharedItems>();
    DependencyManager::set<ResourceManager>();
    // make sure we hear about node kills so we can tell the other nodes
    connect(DependencyManager::get<NodeList>().data(), &NodeList::nodeKilled, this, &AvatarMixer::handleAvatarKilled);

    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();
    packetReceiver.registerListener(PacketType::AvatarData,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::queueIncomingPacket));
    packetReceiver.registerListener(PacketType::AdjustAvatarSorting,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleAdjustAvatarSorting));
    packetReceiver.registerListener(PacketType::AvatarQuery,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleAvatarQueryPacket));
    packetReceiver.registerListener(PacketType::AvatarIdentity,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleAvatarIdentityPacket));
    packetReceiver.registerListener(PacketType::KillAvatar,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleKillAvatarPacket));
    packetReceiver.registerListener(PacketType::NodeIgnoreRequest,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleNodeIgnoreRequestPacket));
    packetReceiver.registerListener(PacketType::RadiusIgnoreRequest,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleRadiusIgnoreRequestPacket));
    packetReceiver.registerListener(PacketType::RequestsDomainListData,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleRequestsDomainListDataPacket));
    packetReceiver.registerListener(PacketType::SetAvatarTraits,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::queueIncomingPacket));
    packetReceiver.registerListener(PacketType::BulkAvatarTraitsAck,
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::queueIncomingPacket));
    packetReceiver.registerListenerForTypes({ PacketType::OctreeStats, PacketType::EntityData, PacketType::EntityErase },
        PacketReceiver::makeSourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleOctreePacket));

    packetReceiver.registerListenerForTypes({
        PacketType::ReplicatedAvatarIdentity,
        PacketType::ReplicatedKillAvatar
    }, PacketReceiver::makeUnsourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleReplicatedPacket));

    packetReceiver.registerListener(PacketType::ReplicatedBulkAvatarData,
        PacketReceiver::makeUnsourcedListenerReference<AvatarMixer>(this, &AvatarMixer::handleReplicatedBulkAvatarPacket));

    auto nodeList = DependencyManager::get<NodeList>();
    connect(nodeList.data(), &NodeList::packetVersionMismatch, this, &AvatarMixer::handlePacketVersionMismatch);
    connect(nodeList.data(), &NodeList::nodeAdded, this, [this](const SharedNodePointer& node) {
        if (node->getType() == NodeType::DownstreamAvatarMixer) {
            getOrCreateClientData(node);
        }
    });
}

SharedNodePointer addOrUpdateReplicatedNode(const QUuid& nodeID, const HifiSockAddr& senderSockAddr) {
    auto replicatedNode = DependencyManager::get<NodeList>()->addOrUpdateNode(nodeID, NodeType::Agent,
                                                                              senderSockAddr,
                                                                              senderSockAddr,
                                                                              Node::NULL_LOCAL_ID, true, true);

    replicatedNode->setLastHeardMicrostamp(usecTimestampNow());

    return replicatedNode;
}

void AvatarMixer::handleReplicatedPacket(QSharedPointer<ReceivedMessage> message) {
    auto nodeList = DependencyManager::get<NodeList>();
    auto nodeID = QUuid::fromRfc4122(message->peek(NUM_BYTES_RFC4122_UUID));

    SharedNodePointer replicatedNode;

    if (message->getType() == PacketType::ReplicatedKillAvatar) {
        // this is a kill packet, which we should only process if we already have the node in our list
        // since it of course does not make sense to add a node just to remove it an instant later
        replicatedNode = nodeList->nodeWithUUID(nodeID);

        if (!replicatedNode) {
            return;
        }
    } else {
        replicatedNode = addOrUpdateReplicatedNode(nodeID, message->getSenderSockAddr());
    }

    // we better have a node to work with at this point
    assert(replicatedNode);

    if (message->getType() == PacketType::ReplicatedAvatarIdentity) {
        handleAvatarIdentityPacket(message, replicatedNode);
    } else if (message->getType() == PacketType::ReplicatedKillAvatar) {
        handleKillAvatarPacket(message, replicatedNode);
    }
}

void AvatarMixer::handleReplicatedBulkAvatarPacket(QSharedPointer<ReceivedMessage> message) {
    while (message->getBytesLeftToRead()) {
        // first, grab the node ID for this replicated avatar
        // Node ID is now part of user data, since ReplicatedBulkAvatarPacket is non-sourced.
        auto nodeID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));
        // make sure we have an upstream replicated node that matches
        auto replicatedNode = addOrUpdateReplicatedNode(nodeID, message->getSenderSockAddr());

        // grab the size of the avatar byte array so we know how much to read
        quint16 avatarByteArraySize;
        message->readPrimitive(&avatarByteArraySize);

        // read the avatar byte array
        auto avatarByteArray = message->read(avatarByteArraySize);

        // construct a "fake" avatar data received message from the byte array and packet list information
        auto replicatedMessage = QSharedPointer<ReceivedMessage>::create(avatarByteArray, PacketType::AvatarData,
                                                                         versionForPacketType(PacketType::AvatarData),
                                                                         message->getSenderSockAddr(), Node::NULL_LOCAL_ID);

        // queue up the replicated avatar data with the client data for the replicated node
        auto start = usecTimestampNow();
        getOrCreateClientData(replicatedNode)->queuePacket(replicatedMessage, replicatedNode);
        auto end = usecTimestampNow();
        _queueIncomingPacketElapsedTime += (end - start);
    }
}

void AvatarMixer::optionallyReplicatePacket(ReceivedMessage& message, const Node& node) {
    // first, make sure that this is a packet from a node we are supposed to replicate
    if (node.isReplicated()) {

        // check if this is a packet type we replicate
        // which means it must be a packet type present in REPLICATED_PACKET_MAPPING or must be the
        // replicated version of one of those packet types
        PacketType replicatedType = PacketTypeEnum::getReplicatedPacketMapping().value(message.getType());

        if (replicatedType == PacketType::Unknown) {
            if (PacketTypeEnum::getReplicatedPacketMapping().key(message.getType()) != PacketType::Unknown) {
                replicatedType = message.getType();
            } else {
                qDebug() << __FUNCTION__ << "called without replicatable packet type - returning";
                return;
            }
        }

        std::unique_ptr<NLPacket> packet;

        auto nodeList = DependencyManager::get<NodeList>();
        nodeList->eachMatchingNode([&](const SharedNodePointer& downstreamNode) {
            return shouldReplicateTo(node, *downstreamNode);
        }, [&](const SharedNodePointer& node) {
            if (!packet) {
                // construct an NLPacket to send to the replicant that has the contents of the received packet
                packet = NLPacket::create(replicatedType, message.getSize());
                packet->write(message.getMessage());
            }

            nodeList->sendUnreliablePacket(*packet, *node);
        });
    }
}

void AvatarMixer::queueIncomingPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer node) {
    auto start = usecTimestampNow();
    getOrCreateClientData(node)->queuePacket(message, node);
    auto end = usecTimestampNow();
    _queueIncomingPacketElapsedTime += (end - start);
}

void AvatarMixer::sendIdentityPacket(AvatarMixerClientData* nodeData, const SharedNodePointer& destinationNode) {
    if (destinationNode->getType() == NodeType::Agent && !destinationNode->isUpstream()) {
        QByteArray individualData = nodeData->getAvatar().identityByteArray();
        individualData.replace(0, NUM_BYTES_RFC4122_UUID, nodeData->getNodeID().toRfc4122());
        auto identityPackets = NLPacketList::create(PacketType::AvatarIdentity, QByteArray(), true, true);
        identityPackets->write(individualData);
        DependencyManager::get<NodeList>()->sendPacketList(std::move(identityPackets), *destinationNode);
        ++_sumIdentityPackets;
    }
}

std::chrono::microseconds AvatarMixer::timeFrame(p_high_resolution_clock::time_point& timestamp) {
    // advance the next frame
    auto nextTimestamp = timestamp + std::chrono::microseconds((int)((float)USECS_PER_SECOND / (float)AVATAR_MIXER_BROADCAST_FRAMES_PER_SECOND));
    auto now = p_high_resolution_clock::now();

    // compute how long the last frame took
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - timestamp);

    // set the new frame timestamp
    timestamp = std::max(now, nextTimestamp);

    // sleep until the next frame should start
    // WIN32 sleep_until is broken until VS2015 Update 2
    // instead, std::max (above) guarantees that timestamp >= now, so we can sleep_for
    std::this_thread::sleep_for(timestamp - now);

    return duration;
}


void AvatarMixer::start() {

    auto nodeList = DependencyManager::get<NodeList>();

    unsigned int frame = 1;
    auto frameTimestamp = p_high_resolution_clock::now();

    while (!_isFinished) {

        auto frameDuration = timeFrame(frameTimestamp); // calculates last frame duration and sleeps remainder of target amount
        throttle(frameDuration, frame); // determines _throttlingRatio for upcoming mix frame

        int lockWait, nodeTransform, functor;

        // Set our query each frame
        {
            _entityViewer.queryOctree();
        }

        // Dirty the hero status if there's been an entity change.
        {
            if (_dirtyHeroStatus) {
                _dirtyHeroStatus = false;
                nodeList->nestedEach([](NodeList::const_iterator cbegin, NodeList::const_iterator cend) {
                    std::for_each(cbegin, cend, [](const SharedNodePointer& node) {
                        if (node->getType() == NodeType::Agent) {
                            NodeData* nodeData = node->getLinkedData();
                            if (nodeData) {
                                auto& avatar = static_cast<AvatarMixerClientData*>(nodeData)->getAvatar();
                                avatar.setNeedsHeroCheck();
                            }
                        }
                    });
                });
            }
        }

        // Allow nodes to process any pending/queued packets across our worker threads
        {
            auto start = usecTimestampNow();

            nodeList->nestedEach([&](NodeList::const_iterator cbegin, NodeList::const_iterator cend) {
                auto end = usecTimestampNow();
                _processQueuedAvatarDataPacketsLockWaitElapsedTime += (end - start);

                _slavePool.processIncomingPackets(cbegin, cend);
            }, &lockWait, &nodeTransform, &functor);
            auto end = usecTimestampNow();
            _processQueuedAvatarDataPacketsElapsedTime += (end - start);

            _broadcastAvatarDataLockWait += lockWait;
            _broadcastAvatarDataNodeTransform += nodeTransform;
            _broadcastAvatarDataNodeFunctor += functor;
        }

        // process pending display names... this doesn't currently run on multiple threads, because it
        // side-effects the mixer's data, which is fine because it's a very low cost operation
        {
            auto start = usecTimestampNow();
            nodeList->nestedEach([&](NodeList::const_iterator cbegin, NodeList::const_iterator cend) {
                std::for_each(cbegin, cend, [&](const SharedNodePointer& node) {
                    if (node->getType() == NodeType::Agent) {
                        manageIdentityData(node);
                    }

                    ++_sumListeners;
                });
            }, &lockWait, &nodeTransform, &functor);
            auto end = usecTimestampNow();
            _displayNameManagementElapsedTime += (end - start);

            _broadcastAvatarDataLockWait += lockWait;
            _broadcastAvatarDataNodeTransform += nodeTransform;
            _broadcastAvatarDataNodeFunctor += functor;
        }

        // this is where we need to put the real work...
        {
            auto start = usecTimestampNow();
            nodeList->nestedEach([&](NodeList::const_iterator cbegin, NodeList::const_iterator cend) {
                auto start = usecTimestampNow();
                _slavePool.broadcastAvatarData(cbegin, cend, _lastFrameTimestamp, _maxKbpsPerNode, _throttlingRatio);
                auto end = usecTimestampNow();
                _broadcastAvatarDataInner += (end - start);
            }, &lockWait, &nodeTransform, &functor);
            auto end = usecTimestampNow();
            _broadcastAvatarDataElapsedTime += (end - start);

            _broadcastAvatarDataLockWait += lockWait;
            _broadcastAvatarDataNodeTransform += nodeTransform;
            _broadcastAvatarDataNodeFunctor += functor;
        }

        ++frame;
        ++_numTightLoopFrames;
        _loopRate.increment();

        // play nice with qt event-looping
        {
            // since we're a while loop we need to yield to qt's event processing
            auto start = usecTimestampNow();
            QCoreApplication::processEvents();
            if (_isFinished) {
                // alert qt eventing that this is finished
                QCoreApplication::sendPostedEvents(this, QEvent::DeferredDelete);
                break;
            }
            auto end = usecTimestampNow();
            _processEventsElapsedTime += (end - start);
        }

        _lastFrameTimestamp = frameTimestamp;

    }
}


// NOTE: nodeData->getAvatar() might be side effected, must be called when access to node/nodeData
// is guaranteed to not be accessed by other thread
void AvatarMixer::manageIdentityData(const SharedNodePointer& node) {
    AvatarMixerClientData* nodeData = reinterpret_cast<AvatarMixerClientData*>(node->getLinkedData());

    // there is no need to manage identity data we haven't received yet
    // so bail early if we've never received an identity packet for this avatar
    if (!nodeData || !nodeData->getAvatar().hasProcessedFirstIdentity() || !node->getActiveSocket()) {
        return;
    }

    MixerAvatar& avatar = nodeData->getAvatar();
    bool sendIdentity = avatar.needsIdentityUpdate();
    if (sendIdentity) {
        nodeData->flagIdentityChange();
    }
    if (nodeData->getAvatarSessionDisplayNameMustChange()) {
        const QString& existingBaseDisplayName = avatar.getSessionDisplayName();
        if (!existingBaseDisplayName.isEmpty()) {
            SessionDisplayName existingDisplayName { existingBaseDisplayName };

            auto suffixMatch = suffixedNamePattern.match(existingBaseDisplayName);
            if (suffixMatch.hasMatch()) {
                existingDisplayName._baseName = suffixMatch.captured(1);
                existingDisplayName._suffix = suffixMatch.captured(2).toInt();
            }
            _sessionDisplayNames.erase(existingDisplayName);
        }

        QString baseName = avatar.getDisplayName().trimmed();
        const QRegularExpression curses { "fuck|shit|damn|cock|cunt" }; // POC. We may eventually want something much more elaborate (subscription?).
        baseName = baseName.replace(curses, "*"); // Replace rather than remove, so that people have a clue that the person's a jerk.
        static const QRegularExpression trailingDigits { R"(\s*(_\d+\s*)?(\s*\n[^$]*)?$)" }; // trailing whitespace "_123" and any subsequent lines
        baseName = baseName.remove(trailingDigits);
        if (baseName.isEmpty()) {
            baseName = "anonymous";
        }

        SessionDisplayName newDisplayName { baseName };
        auto nameIter = _sessionDisplayNames.lower_bound(newDisplayName);
        if (nameIter != _sessionDisplayNames.end() && nameIter->_baseName == baseName) {
            // Existing instance(s) of name; find first free suffix
            while (*nameIter == newDisplayName && ++newDisplayName._suffix && ++nameIter != _sessionDisplayNames.end())
                ;
        }

        _sessionDisplayNames.insert(newDisplayName);
        QString sessionDisplayName = (newDisplayName._suffix > 0) ? baseName + "_" + QString::number(newDisplayName._suffix) : baseName;
        avatar.setSessionDisplayName(sessionDisplayName);
        nodeData->setBaseDisplayName(baseName);

        nodeData->flagIdentityChange();
        nodeData->setAvatarSessionDisplayNameMustChange(false);
        sendIdentity = true;
        qCDebug(avatars) << "Giving session display name" << sessionDisplayName << "to node with ID" << node->getUUID();
    }

    if (sendIdentity && !node->isUpstream()) {
        sendIdentityPacket(nodeData, node); // Tell node whose name changed about its new session display name or avatar.
        // since this packet includes a change to either the skeleton model URL or the display name
        // it needs a new sequence number
        avatar.pushIdentitySequenceNumber();

        // tell node whose name changed about its new session display name or avatar.
        sendIdentityPacket(nodeData, node);
        avatar.setNeedsIdentityUpdate(false);
    }
}

void AvatarMixer::throttle(std::chrono::microseconds duration, int frame) {
    // throttle using a modified proportional-integral controller
    const float FRAME_TIME = USECS_PER_SECOND / AVATAR_MIXER_BROADCAST_FRAMES_PER_SECOND;
    float mixRatio = duration.count() / FRAME_TIME;

    // constants are determined based on a "regular" 16-CPU EC2 server

    // target different mix and backoff ratios (they also have different backoff rates)
    // this is to prevent oscillation, and encourage throttling to find a steady state
    const float TARGET = 0.9f;
    // on a "regular" machine with 100 avatars, this is the largest value where
    // - overthrottling can be recovered
    // - oscillations will not occur after the recovery
    const float BACKOFF_TARGET = 0.44f;

    // the mixer is known to struggle at about 150 on a "regular" machine
    // so throttle 2/150 the streams to ensure smooth mixing (throttling is linear)
    const float STRUGGLES_AT = 150.0f;
    const float THROTTLE_RATE = 2 / STRUGGLES_AT;
    const float BACKOFF_RATE = THROTTLE_RATE / 4;

    // recovery should be bounded so that large changes in user count is a tolerable experience
    // throttling is linear, so most cases will not need a full recovery
    const int RECOVERY_TIME = 180;

    // weight more recent frames to determine if throttling is necessary,
    const int TRAILING_FRAMES = (int)(100 * RECOVERY_TIME * BACKOFF_RATE);
    const float CURRENT_FRAME_RATIO = 1.0f / TRAILING_FRAMES;
    const float PREVIOUS_FRAMES_RATIO = 1.0f - CURRENT_FRAME_RATIO;
    _trailingMixRatio = PREVIOUS_FRAMES_RATIO * _trailingMixRatio + CURRENT_FRAME_RATIO * mixRatio;

    if (frame % TRAILING_FRAMES == 0) {
        if (_trailingMixRatio > TARGET) {
            int proportionalTerm = 1 + (_trailingMixRatio - TARGET) / 0.1f;
            _throttlingRatio += THROTTLE_RATE * proportionalTerm;
            _throttlingRatio = std::min(_throttlingRatio, 1.0f);
            qDebug("avatar-mixer is struggling (%f mix/sleep) - throttling %f of streams",
                (double)_trailingMixRatio, (double)_throttlingRatio);
        }
        else if (_throttlingRatio > 0.0f && _trailingMixRatio <= BACKOFF_TARGET) {
            int proportionalTerm = 1 + (TARGET - _trailingMixRatio) / 0.2f;
            _throttlingRatio -= BACKOFF_RATE * proportionalTerm;
            _throttlingRatio = std::max(_throttlingRatio, 0.0f);
            qDebug("avatar-mixer is recovering (%f mix/sleep) - throttling %f of streams",
                (double)_trailingMixRatio, (double)_throttlingRatio);
        }
    }
}


void AvatarMixer::handleAvatarKilled(SharedNodePointer avatarNode) {
    if (avatarNode->getType() == NodeType::Agent
        && avatarNode->getLinkedData()) {
        auto nodeList = DependencyManager::get<NodeList>();

        {  // decrement sessionDisplayNames table and possibly remove
           QMutexLocker nodeDataLocker(&avatarNode->getLinkedData()->getMutex());
           AvatarMixerClientData* nodeData = dynamic_cast<AvatarMixerClientData*>(avatarNode->getLinkedData());
           const QString& displayName = nodeData->getAvatar().getSessionDisplayName();
           SessionDisplayName exitingDisplayName { displayName };

           auto suffixMatch = suffixedNamePattern.match(displayName);
           if (suffixMatch.hasMatch()) {
               exitingDisplayName._baseName = suffixMatch.captured(1);
               exitingDisplayName._suffix = suffixMatch.captured(2).toInt();
           }
           auto displayNameIter = _sessionDisplayNames.find(exitingDisplayName);
           if (displayNameIter == _sessionDisplayNames.end()) {
               qCDebug(avatars) << "Exiting avatar displayname" << displayName << "not found";
           } else {
               _sessionDisplayNames.erase(displayNameIter);
           }
        }

        std::unique_ptr<NLPacket> killPacket;
        std::unique_ptr<NLPacket> replicatedKillPacket;

        // this was an avatar we were sending to other people
        // send a kill packet for it to our other nodes
        nodeList->eachMatchingNode([&](const SharedNodePointer& node) {
            // we relay avatar kill packets to agents that are not upstream
            // and downstream avatar mixers, if the node that was just killed was being replicatedConnectedAgent
            return node->getActiveSocket() &&
                (((node->getType() == NodeType::Agent || node->getType() == NodeType::EntityScriptServer) && !node->isUpstream()) ||
                 (avatarNode->isReplicated() && shouldReplicateTo(*avatarNode, *node)));
        }, [&](const SharedNodePointer& node) {
            if (node->getType() == NodeType::Agent || node->getType() == NodeType::EntityScriptServer) {
                if (!killPacket) {
                    killPacket = NLPacket::create(PacketType::KillAvatar, NUM_BYTES_RFC4122_UUID + sizeof(KillAvatarReason), true);
                    killPacket->write(avatarNode->getUUID().toRfc4122());
                    killPacket->writePrimitive(KillAvatarReason::AvatarDisconnected);
                }

                auto killPacketCopy = NLPacket::createCopy(*killPacket);

                nodeList->sendPacket(std::move(killPacketCopy), *node);
            } else {
                // send a replicated kill packet to the downstream avatar mixer
                if (!replicatedKillPacket) {
                    replicatedKillPacket = NLPacket::create(PacketType::ReplicatedKillAvatar,
                                                  NUM_BYTES_RFC4122_UUID + sizeof(KillAvatarReason));
                    replicatedKillPacket->write(avatarNode->getUUID().toRfc4122());
                    replicatedKillPacket->writePrimitive(KillAvatarReason::AvatarDisconnected);
                }

                nodeList->sendUnreliablePacket(*replicatedKillPacket, *node);
            }
        });


        // we also want to remove sequence number data for this avatar on our other avatars
        // so invoke the appropriate method on the AvatarMixerClientData for other avatars
        nodeList->eachMatchingNode(
            [&](const SharedNodePointer& node)->bool {
                if (!node->getLinkedData()) {
                    return false;
                }

                if (node->getUUID() == avatarNode->getUUID()) {
                    return false;
                }

                return true;
            },
            [&](const SharedNodePointer& node) {
                QMetaObject::invokeMethod(node->getLinkedData(),
                                         "cleanupKilledNode",
                                          Qt::AutoConnection,
                                          Q_ARG(const QUuid&, QUuid(avatarNode->getUUID())),
                                          Q_ARG(Node::LocalID, avatarNode->getLocalID()));
            }
        );
    }
}


void AvatarMixer::handleAdjustAvatarSorting(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto start = usecTimestampNow();

    // only allow admins with kick rights to change this value...
    if (senderNode->getCanKick()) {
        message->readPrimitive(&AvatarData::_avatarSortCoefficientSize);
        message->readPrimitive(&AvatarData::_avatarSortCoefficientCenter);
        message->readPrimitive(&AvatarData::_avatarSortCoefficientAge);

        qCDebug(avatars) << "New avatar sorting... "
                            << "size:" << AvatarData::_avatarSortCoefficientSize
                            << "center:" << AvatarData::_avatarSortCoefficientCenter
                            << "age:" << AvatarData::_avatarSortCoefficientAge;
    }

    auto end = usecTimestampNow();
    _handleAdjustAvatarSortingElapsedTime += (end - start);
}


void AvatarMixer::handleAvatarQueryPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto start = usecTimestampNow();
    getOrCreateClientData(senderNode);

    AvatarMixerClientData* nodeData = dynamic_cast<AvatarMixerClientData*>(senderNode->getLinkedData());
    if (nodeData) {
        nodeData->readViewFrustumPacket(message->getMessage());
    }

    auto end = usecTimestampNow();
    _handleViewFrustumPacketElapsedTime += (end - start);
}

void AvatarMixer::handleRequestsDomainListDataPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto start = usecTimestampNow();

    getOrCreateClientData(senderNode);

    if (senderNode->getLinkedData()) {
        AvatarMixerClientData* nodeData = dynamic_cast<AvatarMixerClientData*>(senderNode->getLinkedData());
        if (nodeData != nullptr) {
            bool isRequesting;
            message->readPrimitive(&isRequesting);
            nodeData->setRequestsDomainListData(isRequesting);
            qCDebug(avatars) << "node" << nodeData->getNodeID() << "requestsDomainListData" << isRequesting;

            // If we just opened the PAL...
            if (isRequesting) {
                // For each node in the NodeList...
                auto nodeList = DependencyManager::get<NodeList>();
                nodeList->eachMatchingNode(
                    // Discover the valid nodes we're ignoring...
                    [&](const SharedNodePointer& node)->bool {
                    if (node->getUUID() != senderNode->getUUID() &&
                        (nodeData->isRadiusIgnoring(node->getUUID()) ||
                        senderNode->isIgnoringNodeWithID(node->getUUID()))) {
                        return true;
                    }
                    return false;
                },
                    // ...For those nodes, reset the lastBroadcastTime to 0
                    // so that the AvatarMixer will send Identity data to us
                    [&](const SharedNodePointer& node) {
                        nodeData->setLastBroadcastTime(node->getLocalID(), 0);
                        nodeData->resetSentTraitData(node->getLocalID());
                }
                );
            }
        }
    }
    auto end = usecTimestampNow();
    _handleRequestsDomainListDataPacketElapsedTime += (end - start);
}

void AvatarMixer::handleAvatarIdentityPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto start = usecTimestampNow();
    auto nodeList = DependencyManager::get<NodeList>();
    getOrCreateClientData(senderNode);

    if (senderNode->getLinkedData()) {
        AvatarMixerClientData* nodeData = dynamic_cast<AvatarMixerClientData*>(senderNode->getLinkedData());
        if (nodeData != nullptr) {
            AvatarData& avatar = nodeData->getAvatar();

            // parse the identity packet and update the change timestamp if appropriate
            bool identityChanged = false;
            bool displayNameChanged = false;
            QDataStream avatarIdentityStream(message->getMessage());
            avatar.processAvatarIdentity(avatarIdentityStream, identityChanged, displayNameChanged);

            if (identityChanged) {
                QMutexLocker nodeDataLocker(&nodeData->getMutex());
                nodeData->flagIdentityChange();
                if (displayNameChanged) {
                    nodeData->setAvatarSessionDisplayNameMustChange(true);
                }
            }
        }
    }
    auto end = usecTimestampNow();
    _handleAvatarIdentityPacketElapsedTime += (end - start);
}

void AvatarMixer::handleKillAvatarPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer node) {
    auto start = usecTimestampNow();
    handleAvatarKilled(node);

    node->setLinkedData(nullptr);
    auto end = usecTimestampNow();
    _handleKillAvatarPacketElapsedTime += (end - start);

    optionallyReplicatePacket(*message, *node);
}

void AvatarMixer::handleNodeIgnoreRequestPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto start = usecTimestampNow();
    auto nodeList = DependencyManager::get<NodeList>();
    AvatarMixerClientData* nodeData = reinterpret_cast<AvatarMixerClientData*>(senderNode->getLinkedData());

    bool addToIgnore;
    message->readPrimitive(&addToIgnore);
    while (message->getBytesLeftToRead()) {
        // parse out the UUID being ignored from the packet
        QUuid ignoredUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));
        auto ignoredNode = nodeList->nodeWithUUID(ignoredUUID);
        if (ignoredNode) {
            if (nodeData) {
                // Reset the lastBroadcastTime for the ignored avatar to 0
                // so the AvatarMixer knows it'll have to send identity data about the ignored avatar
                // to the ignorer if the ignorer unignores.
                nodeData->setLastBroadcastTime(ignoredNode->getLocalID(), 0);
                nodeData->resetSentTraitData(ignoredNode->getLocalID());
            }


            // Reset the lastBroadcastTime for the ignorer (FROM THE PERSPECTIVE OF THE IGNORED) to 0
            // so the AvatarMixer knows it'll have to send identity data about the ignorer
            // to the ignored if the ignorer unignores.
            AvatarMixerClientData* ignoredNodeData = reinterpret_cast<AvatarMixerClientData*>(ignoredNode->getLinkedData());
            if (ignoredNodeData) {
                ignoredNodeData->setLastBroadcastTime(senderNode->getLocalID(), 0);
                ignoredNodeData->resetSentTraitData(senderNode->getLocalID());
            }
        }

        if (addToIgnore) {
            senderNode->addIgnoredNode(ignoredUUID);

            if (ignoredNode) {
                // send a reliable kill packet to remove the sending avatar for the ignored avatar
                auto killPacket = NLPacket::create(PacketType::KillAvatar,
                                                   NUM_BYTES_RFC4122_UUID + sizeof(KillAvatarReason), true);
                killPacket->write(senderNode->getUUID().toRfc4122());
                killPacket->writePrimitive(KillAvatarReason::AvatarDisconnected);
                nodeList->sendPacket(std::move(killPacket), *ignoredNode);
            }
        } else {
            senderNode->removeIgnoredNode(ignoredUUID);
        }
    }
    auto end = usecTimestampNow();
    _handleNodeIgnoreRequestPacketElapsedTime += (end - start);
}

void AvatarMixer::handleRadiusIgnoreRequestPacket(QSharedPointer<ReceivedMessage> packet, SharedNodePointer sendingNode) {
    auto start = usecTimestampNow();

    bool enabled;
    packet->readPrimitive(&enabled);

    auto avatarData = getOrCreateClientData(sendingNode);
    avatarData->setIsIgnoreRadiusEnabled(enabled);

    auto end = usecTimestampNow();
    _handleRadiusIgnoreRequestPacketElapsedTime += (end - start);
}

void AvatarMixer::sendStatsPacket() {
    if (!_numTightLoopFrames) {
        return;
    }

    auto start = usecTimestampNow();

    QJsonObject statsObject;

    statsObject["broadcast_loop_rate"] = _loopRate.rate();
    statsObject["threads"] = _slavePool.numThreads();
    statsObject["trailing_mix_ratio"] = _trailingMixRatio;
    statsObject["throttling_ratio"] = _throttlingRatio;

#ifdef DEBUG_EVENT_QUEUE
    QJsonObject qtStats;

    _slavePool.queueStats(qtStats);
    statsObject["avatar_thread_event_queue"] = qtStats;
#endif

    // this things all occur on the frequency of the tight loop
    int tightLoopFrames = _numTightLoopFrames;
    int tenTimesPerFrame = tightLoopFrames * 10;
    #define TIGHT_LOOP_STAT(x) (x > tenTimesPerFrame) ? x / tightLoopFrames : ((float)x / (float)tightLoopFrames);
    #define TIGHT_LOOP_STAT_UINT64(x) (x > (quint64)tenTimesPerFrame) ? x / tightLoopFrames : ((float)x / (float)tightLoopFrames);

    statsObject["average_listeners_last_second"] = TIGHT_LOOP_STAT(_sumListeners);

    QJsonObject singleCoreTasks;
    singleCoreTasks["processEvents"] = TIGHT_LOOP_STAT_UINT64(_processEventsElapsedTime);
    singleCoreTasks["queueIncomingPacket"] = TIGHT_LOOP_STAT_UINT64(_queueIncomingPacketElapsedTime);

    QJsonObject incomingPacketStats;
    incomingPacketStats["handleAvatarIdentityPacket"] = TIGHT_LOOP_STAT_UINT64(_handleAvatarIdentityPacketElapsedTime);
    incomingPacketStats["handleKillAvatarPacket"] = TIGHT_LOOP_STAT_UINT64(_handleKillAvatarPacketElapsedTime);
    incomingPacketStats["handleNodeIgnoreRequestPacket"] = TIGHT_LOOP_STAT_UINT64(_handleNodeIgnoreRequestPacketElapsedTime);
    incomingPacketStats["handleRadiusIgnoreRequestPacket"] = TIGHT_LOOP_STAT_UINT64(_handleRadiusIgnoreRequestPacketElapsedTime);
    incomingPacketStats["handleRequestsDomainListDataPacket"] = TIGHT_LOOP_STAT_UINT64(_handleRequestsDomainListDataPacketElapsedTime);
    incomingPacketStats["handleAvatarQueryPacket"] = TIGHT_LOOP_STAT_UINT64(_handleViewFrustumPacketElapsedTime);

    singleCoreTasks["incoming_packets"] = incomingPacketStats;
    singleCoreTasks["sendStats"] = (float)_sendStatsElapsedTime;

    statsObject["singleCoreTasks"] = singleCoreTasks;

    QJsonObject parallelTasks;

    QJsonObject processQueuedAvatarDataPacketsStats;
    processQueuedAvatarDataPacketsStats["1_total"] = TIGHT_LOOP_STAT_UINT64(_processQueuedAvatarDataPacketsElapsedTime);
    processQueuedAvatarDataPacketsStats["2_lockWait"] = TIGHT_LOOP_STAT_UINT64(_processQueuedAvatarDataPacketsLockWaitElapsedTime);
    parallelTasks["processQueuedAvatarDataPackets"] = processQueuedAvatarDataPacketsStats;

    QJsonObject broadcastAvatarDataStats;

    broadcastAvatarDataStats["1_total"] = TIGHT_LOOP_STAT_UINT64(_broadcastAvatarDataElapsedTime);
    broadcastAvatarDataStats["2_innner"] = TIGHT_LOOP_STAT_UINT64(_broadcastAvatarDataInner);
    broadcastAvatarDataStats["3_lockWait"] = TIGHT_LOOP_STAT_UINT64(_broadcastAvatarDataLockWait);
    broadcastAvatarDataStats["4_NodeTransform"] = TIGHT_LOOP_STAT_UINT64(_broadcastAvatarDataNodeTransform);
    broadcastAvatarDataStats["5_Functor"] = TIGHT_LOOP_STAT_UINT64(_broadcastAvatarDataNodeFunctor);

    parallelTasks["broadcastAvatarData"] = broadcastAvatarDataStats;

    QJsonObject displayNameManagementStats;
    displayNameManagementStats["1_total"] = TIGHT_LOOP_STAT_UINT64(_displayNameManagementElapsedTime);
    parallelTasks["displayNameManagement"] = displayNameManagementStats;

    statsObject["parallelTasks"] = parallelTasks;


    AvatarMixerSlaveStats aggregateStats;

    // gather stats
    _slavePool.each([&](AvatarMixerSlave& slave) {
        AvatarMixerSlaveStats stats;
        slave.harvestStats(stats);
        aggregateStats += stats;
    });

    QJsonObject slavesAggregatObject;

    slavesAggregatObject["received_1_nodesProcessed"] = TIGHT_LOOP_STAT(aggregateStats.nodesProcessed);

    slavesAggregatObject["sent_1_nodesBroadcastedTo"] = TIGHT_LOOP_STAT(aggregateStats.nodesBroadcastedTo);

    float averageNodes = ((float)aggregateStats.nodesBroadcastedTo / (float)tightLoopFrames);

    float averageOthersIncluded = averageNodes ? aggregateStats.numOthersIncluded / averageNodes : 0.0f;
    slavesAggregatObject["sent_2_averageOthersIncluded"] = TIGHT_LOOP_STAT(averageOthersIncluded);

    float averageOverBudgetAvatars = averageNodes ? aggregateStats.overBudgetAvatars / averageNodes : 0.0f;
    slavesAggregatObject["sent_3_averageOverBudgetAvatars"] = TIGHT_LOOP_STAT(averageOverBudgetAvatars);
    slavesAggregatObject["sent_4_averageDataBytes"] = TIGHT_LOOP_STAT(aggregateStats.numDataBytesSent);
    slavesAggregatObject["sent_5_averageTraitsBytes"] = TIGHT_LOOP_STAT(aggregateStats.numTraitsBytesSent);
    slavesAggregatObject["sent_6_averageIdentityBytes"] = TIGHT_LOOP_STAT(aggregateStats.numIdentityBytesSent);
    slavesAggregatObject["sent_7_averageHeroAvatars"] = TIGHT_LOOP_STAT(aggregateStats.numHeroesIncluded);

    slavesAggregatObject["timing_1_processIncomingPackets"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.processIncomingPacketsElapsedTime);
    slavesAggregatObject["timing_2_ignoreCalculation"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.ignoreCalculationElapsedTime);
    slavesAggregatObject["timing_3_toByteArray"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.toByteArrayElapsedTime);
    slavesAggregatObject["timing_4_avatarDataPacking"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.avatarDataPackingElapsedTime);
    slavesAggregatObject["timing_5_packetSending"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.packetSendingElapsedTime);
    slavesAggregatObject["timing_6_jobElapsedTime"] = TIGHT_LOOP_STAT_UINT64(aggregateStats.jobElapsedTime);

    statsObject["slaves_aggregate (per frame)"] = slavesAggregatObject;

    _handleViewFrustumPacketElapsedTime = 0;
    _handleAvatarIdentityPacketElapsedTime = 0;
    _handleKillAvatarPacketElapsedTime = 0;
    _handleNodeIgnoreRequestPacketElapsedTime = 0;
    _handleRadiusIgnoreRequestPacketElapsedTime = 0;
    _handleRequestsDomainListDataPacketElapsedTime = 0;
    _processEventsElapsedTime = 0;
    _queueIncomingPacketElapsedTime = 0;
    _processQueuedAvatarDataPacketsElapsedTime = 0;
    _processQueuedAvatarDataPacketsLockWaitElapsedTime = 0;

    QJsonObject avatarsObject;
    auto nodeList = DependencyManager::get<NodeList>();
    // add stats for each listener
    nodeList->eachNode([&](const SharedNodePointer& node) {
        QJsonObject avatarStats;

        const QString NODE_OUTBOUND_KBPS_STAT_KEY = "outbound_kbps";
        const QString NODE_INBOUND_KBPS_STAT_KEY = "inbound_kbps";

        // add the key to ask the domain-server for a username replacement, if it has it
        avatarStats[USERNAME_UUID_REPLACEMENT_STATS_KEY] = uuidStringWithoutCurlyBraces(node->getUUID());

        float outboundAvatarDataKbps = node->getOutboundKbps();
        avatarStats[NODE_OUTBOUND_KBPS_STAT_KEY] = outboundAvatarDataKbps;
        avatarStats[NODE_INBOUND_KBPS_STAT_KEY] = node->getInboundKbps();

        AvatarMixerClientData* clientData = static_cast<AvatarMixerClientData*>(node->getLinkedData());
        if (clientData) {
            MutexTryLocker lock(clientData->getMutex());
            if (lock.isLocked()) {
                clientData->loadJSONStats(avatarStats);

                // add the diff between the full outbound bandwidth and the measured bandwidth for AvatarData send only
                avatarStats["delta_full_vs_avatar_data_kbps"] =
                    (double)outboundAvatarDataKbps - avatarStats[OUTBOUND_AVATAR_DATA_STATS_KEY].toDouble();
            }

            if (node->getType() != NodeType::Agent) {  // Nodes that aren't avatars
                const QString displayName
                    { node->getType() == NodeType::EntityScriptServer ? "ENTITY SCRIPT SERVER" : "ENTITY SERVER" };
                avatarStats["display_name"] = displayName;
            }
        }

        avatarsObject[uuidStringWithoutCurlyBraces(node->getUUID())] = avatarStats;
    });

    statsObject["z_avatars"] = avatarsObject;

    ThreadedAssignment::addPacketStatsAndSendStatsPacket(statsObject);

    _sumListeners = 0;
    _sumIdentityPackets = 0;
    _numTightLoopFrames = 0;

    _broadcastAvatarDataElapsedTime = 0;
    _broadcastAvatarDataInner = 0;
    _broadcastAvatarDataLockWait = 0;
    _broadcastAvatarDataNodeTransform = 0;
    _broadcastAvatarDataNodeFunctor = 0;

    _displayNameManagementElapsedTime = 0;
    _ignoreCalculationElapsedTime = 0;
    _avatarDataPackingElapsedTime = 0;
    _packetSendingElapsedTime = 0;


    auto end = usecTimestampNow();
    _sendStatsElapsedTime = (end - start);

    _lastStatsTime = start;

}

void AvatarMixer::run() {
    qCDebug(avatars) << "Waiting for connection to domain to request settings from domain-server.";

    // wait until we have the domain-server settings, otherwise we bail
    DomainHandler& domainHandler = DependencyManager::get<NodeList>()->getDomainHandler();
    connect(&domainHandler, &DomainHandler::settingsReceived, this, &AvatarMixer::domainSettingsRequestComplete);
    connect(&domainHandler, &DomainHandler::settingsReceiveFail, this, &AvatarMixer::domainSettingsRequestFailed);

    ThreadedAssignment::commonInit(AVATAR_MIXER_LOGGING_NAME, NodeType::AvatarMixer);
}

AvatarMixerClientData* AvatarMixer::getOrCreateClientData(SharedNodePointer node) {
    auto clientData = dynamic_cast<AvatarMixerClientData*>(node->getLinkedData());

    if (!clientData) {
        node->setLinkedData(std::unique_ptr<NodeData> { new AvatarMixerClientData(node->getUUID(), node->getLocalID()) });
        clientData = dynamic_cast<AvatarMixerClientData*>(node->getLinkedData());
        auto& avatar = clientData->getAvatar();
        avatar.setDomainMinimumHeight(_domainMinimumHeight);
        avatar.setDomainMaximumHeight(_domainMaximumHeight);
    }

    return clientData;
}

void AvatarMixer::domainSettingsRequestComplete() {
    auto nodeList = DependencyManager::get<NodeList>();
    nodeList->addSetOfNodeTypesToNodeInterestSet({
        NodeType::Agent, NodeType::EntityScriptServer, NodeType::EntityServer,
        NodeType::UpstreamAvatarMixer, NodeType::DownstreamAvatarMixer
    });

    // parse the settings to pull out the values we need
    parseDomainServerSettings(nodeList->getDomainHandler().getSettingsObject());

    setupEntityQuery();

    // start our tight loop...
    start();
}

void AvatarMixer::handlePacketVersionMismatch(PacketType type, const HifiSockAddr& senderSockAddr, const QUuid& senderUUID) {
    // if this client is using packet versions we don't expect.
    if ((type == PacketTypeEnum::Value::AvatarIdentity || type == PacketTypeEnum::Value::AvatarData) && !senderUUID.isNull()) {
        // Echo an empty AvatarData packet back to that client.
        // This should trigger a version mismatch dialog on their side.
        auto nodeList = DependencyManager::get<NodeList>();
        auto node = nodeList->nodeWithUUID(senderUUID);
        if (node) {
            auto emptyPacket = NLPacket::create(PacketType::AvatarData, 0);
            nodeList->sendPacket(std::move(emptyPacket), *node);
        }
    }
}

void AvatarMixer::parseDomainServerSettings(const QJsonObject& domainSettings) {
    const QString AVATAR_MIXER_SETTINGS_KEY = "avatar_mixer";
    QJsonObject avatarMixerGroupObject = domainSettings[AVATAR_MIXER_SETTINGS_KEY].toObject();


    const QString NODE_SEND_BANDWIDTH_KEY = "max_node_send_bandwidth";

    const float DEFAULT_NODE_SEND_BANDWIDTH = 5.0f;
    QJsonValue nodeBandwidthValue = avatarMixerGroupObject[NODE_SEND_BANDWIDTH_KEY];
    if (!nodeBandwidthValue.isDouble()) {
        qCDebug(avatars) << NODE_SEND_BANDWIDTH_KEY << "is not a double - will continue with default value";
    }

    _maxKbpsPerNode = nodeBandwidthValue.toDouble(DEFAULT_NODE_SEND_BANDWIDTH) * KILO_PER_MEGA;
    qCDebug(avatars) << "The maximum send bandwidth per node is" << _maxKbpsPerNode << "kbps.";

    const QString AUTO_THREADS = "auto_threads";
    bool autoThreads = avatarMixerGroupObject[AUTO_THREADS].toBool();
    if (!autoThreads) {
        bool ok;
        const QString NUM_THREADS = "num_threads";
        int numThreads = avatarMixerGroupObject[NUM_THREADS].toString().toInt(&ok);
        if (!ok) {
            qCWarning(avatars) << "Avatar mixer: Error reading thread count. Using 1 thread.";
            numThreads = 1;
        }
        qCDebug(avatars) << "Avatar mixer will use specified number of threads:" << numThreads;
        _slavePool.setNumThreads(numThreads);
    } else {
        qCDebug(avatars) << "Avatar mixer will automatically determine number of threads to use. Using:" << _slavePool.numThreads() << "threads.";
    }

    {
        const QString CONNECTION_RATE = "connection_rate";
        auto nodeList = DependencyManager::get<NodeList>();
        bool success;
        int connectionRate = avatarMixerGroupObject[CONNECTION_RATE].toString().toInt(&success);
        if (success) {
            nodeList->setMaxConnectionRate(connectionRate);
        }
    }

    {   // Fraction of downstream bandwidth reserved for 'hero' avatars:
        static const QString PRIORITY_FRACTION_KEY = "priority_fraction";
        if (avatarMixerGroupObject.contains(PRIORITY_FRACTION_KEY)) {
            float priorityFraction = float(avatarMixerGroupObject[PRIORITY_FRACTION_KEY].toDouble());
            _slavePool.setPriorityReservedFraction(std::min(std::max(0.0f, priorityFraction), 1.0f));
            qCDebug(avatars) << "Avatar mixer reserving" << priorityFraction << "of bandwidth for priority avatars";
        }
    }

    const QString AVATARS_SETTINGS_KEY = "avatars";

    static const QString MIN_HEIGHT_OPTION = "min_avatar_height";
    float settingMinHeight = avatarMixerGroupObject[MIN_HEIGHT_OPTION].toDouble(MIN_AVATAR_HEIGHT);
    _domainMinimumHeight = glm::clamp(settingMinHeight, MIN_AVATAR_HEIGHT, MAX_AVATAR_HEIGHT);

    static const QString MAX_HEIGHT_OPTION = "max_avatar_height";
    float settingMaxHeight = avatarMixerGroupObject[MAX_HEIGHT_OPTION].toDouble(MAX_AVATAR_HEIGHT);
    _domainMaximumHeight = glm::clamp(settingMaxHeight, MIN_AVATAR_HEIGHT, MAX_AVATAR_HEIGHT);

    // make sure that the domain owner didn't flip min and max
    if (_domainMinimumHeight > _domainMaximumHeight) {
        std::swap(_domainMinimumHeight, _domainMaximumHeight);
    }

    qCDebug(avatars) << "This domain requires a minimum avatar height of" << _domainMinimumHeight
                     << "and a maximum avatar height of" << _domainMaximumHeight;

    static const QString AVATAR_WHITELIST_OPTION = "avatar_whitelist";
    _slaveSharedData.skeletonURLWhitelist = avatarMixerGroupObject[AVATAR_WHITELIST_OPTION]
        .toString().split(',', QString::KeepEmptyParts);

    static const QString REPLACEMENT_AVATAR_OPTION = "replacement_avatar";
    _slaveSharedData.skeletonReplacementURL = avatarMixerGroupObject[REPLACEMENT_AVATAR_OPTION]
        .toString();

    if (_slaveSharedData.skeletonURLWhitelist.count() == 1 && _slaveSharedData.skeletonURLWhitelist[0].isEmpty()) {
        // KeepEmptyParts above will parse "," as ["", ""] (which is ok), but "" as [""] (which is not ok).
        _slaveSharedData.skeletonURLWhitelist.clear();
    }

    if (_slaveSharedData.skeletonURLWhitelist.isEmpty()) {
        qCDebug(avatars) << "All avatars are allowed.";
    } else {
        qCDebug(avatars) << "Avatars other than" << _slaveSharedData.skeletonURLWhitelist << "will be replaced by" << (_slaveSharedData.skeletonReplacementURL.isEmpty() ? "default" : _slaveSharedData.skeletonReplacementURL.toString());
    }
}

void AvatarMixer::setupEntityQuery() {
    _entityViewer.init();
    EntityTreePointer entityTree = _entityViewer.getTree();
    DependencyManager::registerInheritance<SpatialParentFinder, AssignmentParentFinder>();
    DependencyManager::set<AssignmentParentFinder>(entityTree);

    connect(entityTree.get(), &EntityTree::addingEntityPointer, this, &AvatarMixer::entityAdded);
    connect(entityTree.get(), &EntityTree::deletingEntityPointer, this, &AvatarMixer::entityChange);

    // ES query: {"avatarPriority": true, "type": "Zone"}
    QJsonObject priorityZoneQuery;
    priorityZoneQuery["avatarPriority"] = true;
    priorityZoneQuery["type"] = "Zone";

    QJsonObject queryFlags;
    queryFlags["includeAncestors"] = true;
    queryFlags["includeDescendants"] = true;
    priorityZoneQuery["flags"] = queryFlags;
    priorityZoneQuery["name"] = true; // Handy for debugging.

    _entityViewer.getOctreeQuery().setJSONParameters(priorityZoneQuery);
    _slaveSharedData.entityTree = entityTree;
}

void AvatarMixer::handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    PacketType packetType = message->getType();

    switch (packetType) {
    case PacketType::OctreeStats:
    {   // Ignore stats, but may have a different Entity packet appended.
        OctreeHeadlessViewer::parseOctreeStats(message, senderNode);
        const auto piggyBackedSizeWithHeader = message->getBytesLeftToRead();
        if (piggyBackedSizeWithHeader > 0) {
            // pull out the piggybacked packet and create a new QSharedPointer<NLPacket> for it
            auto buffer = std::unique_ptr<char[]>(new char[piggyBackedSizeWithHeader]);
            memcpy(buffer.get(), message->getRawMessage() + message->getPosition(), piggyBackedSizeWithHeader);

            auto newPacket = NLPacket::fromReceivedPacket(std::move(buffer), piggyBackedSizeWithHeader, message->getSenderSockAddr());
            auto newMessage = QSharedPointer<ReceivedMessage>::create(*newPacket);
            handleOctreePacket(newMessage, senderNode);
        }
        break;
    }

    case PacketType::EntityData:
        _entityViewer.processDatagram(*message, senderNode);
        break;

    case PacketType::EntityErase:
        _entityViewer.processEraseMessage(*message, senderNode);
        break;

    default:
        qCDebug(avatars) << "Unexpected packet type:" << packetType;
        break;
    }
}

void AvatarMixer::entityAdded(EntityItem* entity) {
    if (entity->getType() == EntityTypes::Zone) {
        _dirtyHeroStatus = true;
        entity->registerChangeHandler([this](const EntityItemID& entityItemID) {
            entityChange();
        });
    }
}

void AvatarMixer::entityRemoved(EntityItem * entity) {
    if (entity->getType() == EntityTypes::Zone) {
        _dirtyHeroStatus = true;
    }
}

void AvatarMixer::entityChange() {
    _dirtyHeroStatus = true;
}

void AvatarMixer::aboutToFinish() {
    DependencyManager::destroy<ResourceManager>();
    DependencyManager::destroy<ResourceCacheSharedItems>();
    DependencyManager::destroy<ModelCache>();
    DependencyManager::destroy<ModelFormatRegistry>();
    DependencyManager::destroy<AssignmentDynamicFactory>();
    DependencyManager::destroy<AssignmentParentFinder>();

    ThreadedAssignment::aboutToFinish();
}
