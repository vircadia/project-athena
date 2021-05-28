//
//  EntityServer.cpp
//  assignment-client/src/entities
//
//  Created by Brad Hefta-Gaub on 4/29/14
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityServer.h"

#include <QtCore/QEventLoop>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>

#include <EntityTree.h>
#include <ResourceCache.h>
#include <ScriptCache.h>
#include <plugins/PluginManager.h>
#include <EntityEditFilters.h>
#include <NetworkingConstants.h>
#include <MetaverseAPI.h>
#include <hfm/ModelFormatRegistry.h>

#include "../AssignmentDynamicFactory.h"
#include "AssignmentParentFinder.h"
#include "EntityNodeData.h"
#include "EntityServerConsts.h"
#include "EntityTreeSendThread.h"

const char* MODEL_SERVER_NAME = "Entity";
const char* MODEL_SERVER_LOGGING_TARGET_NAME = "entity-server";
const char* LOCAL_MODELS_PERSIST_FILE = "resources/models.svo";

EntityServer::EntityServer(ReceivedMessage& message) :
    OctreeServer(message),
    _entitySimulation(nullptr),
    _dynamicDomainVerificationTimer(this)
{
    DependencyManager::set<ResourceManager>();
    DependencyManager::set<ResourceCacheSharedItems>();
    DependencyManager::set<ScriptCache>();
    DependencyManager::set<PluginManager>()->instantiate();

    DependencyManager::registerInheritance<EntityDynamicFactoryInterface, AssignmentDynamicFactory>();
    DependencyManager::set<AssignmentDynamicFactory>();
    DependencyManager::set<ModelFormatRegistry>(); // ModelFormatRegistry must be defined before ModelCache. See the ModelCache ctor
    DependencyManager::set<ModelCache>();

    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();
    packetReceiver.registerListenerForTypes({ PacketType::EntityAdd,
        PacketType::EntityClone,
        PacketType::EntityEdit,
        PacketType::EntityErase,
        PacketType::EntityPhysics,
        PacketType::ChallengeOwnership,
        PacketType::ChallengeOwnershipRequest,
        PacketType::ChallengeOwnershipReply },
        PacketReceiver::makeSourcedListenerReference<EntityServer>(this, &EntityServer::handleEntityPacket));

    connect(&_dynamicDomainVerificationTimer, &QTimer::timeout, this, &EntityServer::startDynamicDomainVerification);
    _dynamicDomainVerificationTimer.setSingleShot(true);
}

EntityServer::~EntityServer() {
    if (_pruneDeletedEntitiesTimer) {
        _pruneDeletedEntitiesTimer->stop();
        _pruneDeletedEntitiesTimer->deleteLater();
    }

    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    tree->removeNewlyCreatedHook(this);
}

void EntityServer::aboutToFinish() {
    DependencyManager::get<ResourceManager>()->cleanup();

    DependencyManager::destroy<AssignmentDynamicFactory>();

    OctreeServer::aboutToFinish();
}

void EntityServer::handleEntityPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (_octreeInboundPacketProcessor) {
        _octreeInboundPacketProcessor->queueReceivedPacket(message, senderNode);
    }
}

std::unique_ptr<OctreeQueryNode> EntityServer::createOctreeQueryNode() {
    return std::unique_ptr<OctreeQueryNode> { new EntityNodeData() };
}

OctreePointer EntityServer::createTree() {
    EntityTreePointer tree = EntityTreePointer(new EntityTree(true));
    tree->createRootElement();
    tree->addNewlyCreatedHook(this);
    if (!_entitySimulation) {
        SimpleEntitySimulationPointer simpleSimulation { new SimpleEntitySimulation() };
        simpleSimulation->setEntityTree(tree);
        tree->setSimulation(simpleSimulation);
        _entitySimulation = simpleSimulation;
    }

    DependencyManager::registerInheritance<SpatialParentFinder, AssignmentParentFinder>();
    DependencyManager::set<AssignmentParentFinder>(tree);
    DependencyManager::set<EntityEditFilters>(std::static_pointer_cast<EntityTree>(tree));

    return tree;
}

OctreeServer::UniqueSendThread EntityServer::newSendThread(const SharedNodePointer& node) {
    return std::unique_ptr<EntityTreeSendThread>(new EntityTreeSendThread(this, node));
}

void EntityServer::beforeRun() {
    _pruneDeletedEntitiesTimer = new QTimer();
    connect(_pruneDeletedEntitiesTimer, SIGNAL(timeout()), this, SLOT(pruneDeletedEntities()));
    const int PRUNE_DELETED_MODELS_INTERVAL_MSECS = 1 * 1000; // once every second
    _pruneDeletedEntitiesTimer->start(PRUNE_DELETED_MODELS_INTERVAL_MSECS);

    DomainHandler& domainHandler = DependencyManager::get<NodeList>()->getDomainHandler();
    connect(&domainHandler, &DomainHandler::settingsReceiveFail, this, &EntityServer::domainSettingsRequestFailed);
}

void EntityServer::entityCreated(const EntityItem& newEntity, const SharedNodePointer& senderNode) {
}

// EntityServer will use the "special packets" to send list of recently deleted entities
bool EntityServer::hasSpecialPacketsToSend(const SharedNodePointer& node) {
    bool shouldSendDeletedEntities = false;

    // check to see if any new entities have been added since we last sent to this node...
    EntityNodeData* nodeData = static_cast<EntityNodeData*>(node->getLinkedData());
    if (nodeData) {
        quint64 deletedEntitiesSentAt = nodeData->getLastDeletedEntitiesSentAt();
        EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
        shouldSendDeletedEntities = tree->hasEntitiesDeletedSince(deletedEntitiesSentAt);

        #ifdef EXTRA_ERASE_DEBUGGING
            if (shouldSendDeletedEntities) {
                int elapsed = usecTimestampNow() - deletedEntitiesSentAt;
                qDebug() << "shouldSendDeletedEntities to node:" << node->getUUID() << "deletedEntitiesSentAt:" << deletedEntitiesSentAt << "elapsed:" << elapsed;
            }
        #endif
    }

    return shouldSendDeletedEntities;
}

// FIXME - most of the old code for this was encapsulated in EntityTree, I liked that design from a data
// hiding and object oriented perspective. But that didn't really allow us to handle the case of lots
// of entities being deleted at the same time. I'd like to look to move this back into EntityTree but
// for now this works and addresses the bug.
int EntityServer::sendSpecialPackets(const SharedNodePointer& node, OctreeQueryNode* queryNode, int& packetsSent) {
    int totalBytes = 0;

    EntityNodeData* nodeData = static_cast<EntityNodeData*>(node->getLinkedData());
    if (nodeData) {

        quint64 deletedEntitiesSentAt = nodeData->getLastDeletedEntitiesSentAt();
        quint64 considerEntitiesSince = EntityTree::getAdjustedConsiderSince(deletedEntitiesSentAt);

        quint64 deletePacketSentAt = usecTimestampNow();
        EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
        auto recentlyDeleted = tree->getRecentlyDeletedEntityIDs();

        packetsSent = 0;

        // create a new special packet
        std::unique_ptr<NLPacket> deletesPacket = NLPacket::create(PacketType::EntityErase);

        // pack in flags
        OCTREE_PACKET_FLAGS flags = 0;
        deletesPacket->writePrimitive(flags);

        // pack in sequence number
        auto sequenceNumber = queryNode->getSequenceNumber();
        deletesPacket->writePrimitive(sequenceNumber);

        // pack in timestamp
        OCTREE_PACKET_SENT_TIME now = usecTimestampNow();
        deletesPacket->writePrimitive(now);

        // figure out where we are now and pack a temporary number of IDs
        uint16_t numberOfIDs = 0;
        qint64 numberOfIDsPos = deletesPacket->pos();
        deletesPacket->writePrimitive(numberOfIDs);

        // we keep a multi map of entity IDs to timestamps, we only want to include the entity IDs that have been
        // deleted since we last sent to this node
        auto it = recentlyDeleted.constBegin();
        while (it != recentlyDeleted.constEnd()) {

            // if the timestamp is more recent then out last sent time, include it
            if (it.key() > considerEntitiesSince) {

                // get all the IDs for this timestamp
                const auto& entityIDsFromTime = recentlyDeleted.values(it.key());

                for (const auto& entityID : entityIDsFromTime) {

                    // check to make sure we have room for one more ID, if we don't have more
                    // room, then send out this packet and create another one
                    if (NUM_BYTES_RFC4122_UUID > deletesPacket->bytesAvailableForWrite()) {

                        // replace the count for the number of included IDs
                        deletesPacket->seek(numberOfIDsPos);
                        deletesPacket->writePrimitive(numberOfIDs);

                        // Send the current packet
                        queryNode->packetSent(*deletesPacket);
                        auto thisPacketSize = deletesPacket->getDataSize();
                        totalBytes += thisPacketSize;
                        packetsSent++;
                        DependencyManager::get<NodeList>()->sendPacket(std::move(deletesPacket), *node);

                        #ifdef EXTRA_ERASE_DEBUGGING
                            qDebug() << "EntityServer::sendSpecialPackets() sending packet packetsSent[" << packetsSent << "] size:" << thisPacketSize;
                        #endif


                        // create another packet
                        deletesPacket = NLPacket::create(PacketType::EntityErase);

                        // pack in flags
                        deletesPacket->writePrimitive(flags);

                        // pack in sequence number
                        sequenceNumber = queryNode->getSequenceNumber();
                        deletesPacket->writePrimitive(sequenceNumber);

                        // pack in timestamp
                        deletesPacket->writePrimitive(now);

                        // figure out where we are now and pack a temporary number of IDs
                        numberOfIDs = 0;
                        numberOfIDsPos = deletesPacket->pos();
                        deletesPacket->writePrimitive(numberOfIDs);
                    }

                    // FIXME - we still seem to see cases where incorrect EntityIDs get sent from the server
                    // to the client. These were causing "lost" entities like flashlights and laser pointers
                    // now that we keep around some additional history of the erased entities and resend that
                    // history for a longer time window, these entities are not "lost". But we haven't yet
                    // found/fixed the underlying issue that caused bad UUIDs to be sent to some users.
                    deletesPacket->write(entityID.toRfc4122());
                    ++numberOfIDs;

                    #ifdef EXTRA_ERASE_DEBUGGING
                        qDebug() << "EntityTree::encodeEntitiesDeletedSince() including:" << entityID;
                    #endif
                } // end for (ids)

            } // end if (it.val > sinceLast)


            ++it;
        } // end while

        // replace the count for the number of included IDs
        deletesPacket->seek(numberOfIDsPos);
        deletesPacket->writePrimitive(numberOfIDs);

        // Send the current packet
        queryNode->packetSent(*deletesPacket);
        auto thisPacketSize = deletesPacket->getDataSize();
        totalBytes += thisPacketSize;
        packetsSent++;
        DependencyManager::get<NodeList>()->sendPacket(std::move(deletesPacket), *node);
        #ifdef EXTRA_ERASE_DEBUGGING
            qDebug() << "EntityServer::sendSpecialPackets() sending packet packetsSent[" << packetsSent << "] size:" << thisPacketSize;
        #endif

        nodeData->setLastDeletedEntitiesSentAt(deletePacketSentAt);
    }

    #ifdef EXTRA_ERASE_DEBUGGING
        if (packetsSent > 0) {
            qDebug() << "EntityServer::sendSpecialPackets() sent " << packetsSent << "special packets of " 
                        << totalBytes << " total bytes to node:" << node->getUUID();
        }
    #endif

    // TODO: caller is expecting a packetLength, what if we send more than one packet??
    return totalBytes;
}

void EntityServer::pruneDeletedEntities() {
    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    if (tree->hasAnyDeletedEntities()) {

        quint64 earliestLastDeletedEntitiesSent = usecTimestampNow() + 1; // in the future
        DependencyManager::get<NodeList>()->eachNode([&earliestLastDeletedEntitiesSent](const SharedNodePointer& node) {
            if (node->getLinkedData()) {
                EntityNodeData* nodeData = static_cast<EntityNodeData*>(node->getLinkedData());
                quint64 nodeLastDeletedEntitiesSentAt = nodeData->getLastDeletedEntitiesSentAt();
                if (nodeLastDeletedEntitiesSentAt < earliestLastDeletedEntitiesSent) {
                    earliestLastDeletedEntitiesSent = nodeLastDeletedEntitiesSentAt;
                }
            }
        });
        tree->forgetEntitiesDeletedBefore(earliestLastDeletedEntitiesSent);
    }
}

void EntityServer::readAdditionalConfiguration(const QJsonObject& settingsSectionObject) {
    bool wantEditLogging = false;
    readOptionBool(QString("wantEditLogging"), settingsSectionObject, wantEditLogging);
    qDebug("wantEditLogging=%s", debug::valueOf(wantEditLogging));
    
    bool wantAuditEditLogging = false;
    readOptionBool(QString("wantAuditEditLogging"), settingsSectionObject, wantAuditEditLogging);
    qDebug("wantAuditEditLogging=%s", debug::valueOf(wantAuditEditLogging));

    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    
    int auditEditLoggingInterval;
    if (readOptionInt("auditEditLoggingInterval", settingsSectionObject, auditEditLoggingInterval)) {
        tree->setAuditEditLoggingInterval(auditEditLoggingInterval);
    } else {
        tree->setAuditEditLoggingInterval(EntityTree::DEFAULT_AUDIT_EDIT_INTERVAL);
    }

    bool wantTerseEditLogging = false;
    readOptionBool(QString("wantTerseEditLogging"), settingsSectionObject, wantTerseEditLogging);
    qDebug("wantTerseEditLogging=%s", debug::valueOf(wantTerseEditLogging));

    int maxTmpEntityLifetime;
    if (readOptionInt("maxTmpLifetime", settingsSectionObject, maxTmpEntityLifetime)) {
        tree->setEntityMaxTmpLifetime(maxTmpEntityLifetime);
    } else {
        tree->setEntityMaxTmpLifetime(EntityTree::DEFAULT_MAX_TMP_ENTITY_LIFETIME);
    }

    int minTime;
    if (readOptionInt("dynamicDomainVerificationTimeMin", settingsSectionObject, minTime)) {
        _MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = minTime * 1000;
    }

    int maxTime;
    if (readOptionInt("dynamicDomainVerificationTimeMax", settingsSectionObject, maxTime)) {
        _MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = maxTime * 1000;
    }

    startDynamicDomainVerification();

    tree->setWantEditLogging(wantEditLogging);
    tree->setWantAuditEditLogging(wantAuditEditLogging);
    tree->setWantTerseEditLogging(wantTerseEditLogging);

    QString entityScriptSourceWhitelist;
    if (readOptionString("entityScriptSourceWhitelist", settingsSectionObject, entityScriptSourceWhitelist)) {
        tree->setEntityScriptSourceWhitelist(entityScriptSourceWhitelist);
    } else {
        tree->setEntityScriptSourceWhitelist("");
    }
    
    auto entityEditFilters = DependencyManager::get<EntityEditFilters>();
    
    QString filterURL;
    if (readOptionString("entityEditFilter", settingsSectionObject, filterURL) && !filterURL.isEmpty()) {
        // connect the filterAdded signal, and block edits until you hear back
        connect(entityEditFilters.data(), &EntityEditFilters::filterAdded, this, &EntityServer::entityFilterAdded);
        
        entityEditFilters->addFilter(EntityItemID(), filterURL);
    }
}

void EntityServer::entityFilterAdded(EntityItemID id, bool success) {
    if (id.isInvalidID()) {
        if (success) {
            qDebug() << "entity edit filter for " << id << "added successfully";
        } else {
            qDebug() << "entity edit filter unsuccessfully added, all edits will be rejected for those without lock rights.";
        }
    }
}

void EntityServer::nodeAdded(SharedNodePointer node) {
    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    if (tree) {
        tree->knowAvatarID(node->getUUID());
    }
    OctreeServer::nodeAdded(node);
}

void EntityServer::nodeKilled(SharedNodePointer node) {
    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    if (tree) {
        tree->deleteDescendantsOfAvatar(node->getUUID());
        tree->forgetAvatarID(node->getUUID());
    }
    OctreeServer::nodeKilled(node);
}

// FIXME - this stats tracking is somewhat temporary to debug the Whiteboard issues. It's not a bad
// set of stats to have, but we'd probably want a different data structure if we keep it very long.
// Since this version uses a single shared QMap for all senders, there could be some lock contention 
// on this QWriteLocker
void EntityServer::trackSend(const QUuid& dataID, quint64 dataLastEdited, const QUuid& sessionID) {
    QWriteLocker locker(&_viewerSendingStatsLock);
    _viewerSendingStats[sessionID][dataID] = { usecTimestampNow(), dataLastEdited };
}

void EntityServer::trackViewerGone(const QUuid& sessionID) {
    {
        QWriteLocker locker(&_viewerSendingStatsLock);
        _viewerSendingStats.remove(sessionID);
    }

    if (_entitySimulation) {
        _tree->withReadLock([&] {
            _entitySimulation->clearOwnership(sessionID);
        });
    }
}

QString EntityServer::serverSubclassStats() {
    QLocale locale(QLocale::English);
    QString statsString;

    // display memory usage stats
    statsString += "<b>Entity Server Memory Statistics</b>\r\n";
    statsString += QString().sprintf("EntityTreeElement size... %ld bytes\r\n", sizeof(EntityTreeElement));
    statsString += QString().sprintf("       EntityItem size... %ld bytes\r\n", sizeof(EntityItem));
    statsString += "\r\n\r\n";

    statsString += "<b>Entity Server Sending to Viewer Statistics</b>\r\n";
    statsString += "----- Viewer Node ID -----------------    ----- Entity ID ----------------------    "
                   "---------- Last Sent To ----------    ---------- Last Edited -----------\r\n";

    int viewers = 0;
    const int COLUMN_WIDTH = 24;

    {
        QReadLocker locker(&_viewerSendingStatsLock);
        quint64 now = usecTimestampNow();

        for (auto viewerID : _viewerSendingStats.keys()) {
            statsString += viewerID.toString() + "\r\n";

            auto viewerData = _viewerSendingStats[viewerID];
            for (auto entityID : viewerData.keys()) {
                ViewerSendingStats stats = viewerData[entityID];

                quint64 elapsedSinceSent = now - stats.lastSent;
                double sentMsecsAgo = (double)(elapsedSinceSent / USECS_PER_MSEC);

                quint64 elapsedSinceEdit = now - stats.lastEdited;
                double editMsecsAgo = (double)(elapsedSinceEdit / USECS_PER_MSEC);

                statsString += "                                          "; // the viewerID spacing
                statsString += entityID.toString();
                statsString += "    ";
                statsString += QString("%1 msecs ago")
                    .arg(locale.toString((double)sentMsecsAgo).rightJustified(COLUMN_WIDTH, ' '));
                statsString += QString("%1 msecs ago")
                    .arg(locale.toString((double)editMsecsAgo).rightJustified(COLUMN_WIDTH, ' '));
                statsString += "\r\n";
            }
            viewers++;
        }
    }
    if (viewers < 1) {
        statsString += "    no viewers... \r\n";
    }
    statsString += "\r\n\r\n";

    return statsString;
}

void EntityServer::domainSettingsRequestFailed() {
    auto nodeList = DependencyManager::get<NodeList>();
    qCDebug(entities) << "The EntityServer couldn't get the Domain Settings. Starting dynamic domain verification with default values...";

    _MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = DEFAULT_MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS;
    _MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = DEFAULT_MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS;
    startDynamicDomainVerification();
}

void EntityServer::startDynamicDomainVerification() {
    qCDebug(entities) << "Starting Dynamic Domain Verification...";

    EntityTreePointer tree = std::static_pointer_cast<EntityTree>(_tree);
    tree->startDynamicDomainVerificationOnServer((float) _MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS / MSECS_PER_SECOND);

    int nextInterval = qrand() % ((_MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS + 1) - _MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS) + _MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS;
    qCDebug(entities) << "Restarting Dynamic Domain Verification timer for" << nextInterval / 1000 << "seconds";
    _dynamicDomainVerificationTimer.start(nextInterval);
}
