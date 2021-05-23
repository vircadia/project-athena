//
//  EntityScriptServer.cpp
//  assignment-client/src/scripts
//
//  Created by Clément Brisset on 1/5/17.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityScriptServer.h"

#include <mutex>

#include <AudioConstants.h>
#include <AudioScriptingInterface.h>
#include <AudioInjectorManager.h>
#include <AvatarHashMap.h>
#include <ClientServerUtils.h>
#include <DebugDraw.h>
#include <EntityNodeData.h>
#include <EntityScriptingInterface.h>
#include <LogHandler.h>
#include <MessagesClient.h>
#include <plugins/CodecPlugin.h>
#include <plugins/PluginManager.h>
#include <ResourceManager.h>
#include <ResourceScriptingInterface.h>
#include <ScriptCache.h>
#include <ScriptEngines.h>
#include <SoundCacheScriptingInterface.h>
#include <UUID.h>
#include <WebSocketServerClass.h>

#include <EntityScriptClient.h> // for EntityScriptServerServices

#include "../AssignmentDynamicFactory.h"
#include "EntityScriptServerLogging.h"
#include "../entities/AssignmentParentFinder.h"

using Mutex = std::mutex;
using Lock = std::lock_guard<Mutex>;

static std::mutex logBufferMutex;
static std::string logBuffer;

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    auto logMessage = LogHandler::getInstance().printMessage((LogMsgType) type, context, message);

    if (!logMessage.isEmpty()) {
        Lock lock(logBufferMutex);
        logBuffer.append(logMessage.toStdString() + '\n');
    }
}

int EntityScriptServer::_entitiesScriptEngineCount = 0;

EntityScriptServer::EntityScriptServer(ReceivedMessage& message) : ThreadedAssignment(message) {
    qInstallMessageHandler(messageHandler);

    DependencyManager::registerInheritance<EntityDynamicFactoryInterface, AssignmentDynamicFactory>();
    DependencyManager::set<AssignmentDynamicFactory>();

    DependencyManager::set<EntityScriptingInterface>(false)->setPacketSender(&_entityEditSender);
    DependencyManager::set<ResourceScriptingInterface>();

    DependencyManager::set<ResourceManager>();
    DependencyManager::set<PluginManager>()->instantiate();

    DependencyManager::registerInheritance<SpatialParentFinder, AssignmentParentFinder>();

    DependencyManager::set<AudioScriptingInterface>();

    DependencyManager::set<ResourceCacheSharedItems>();
    DependencyManager::set<SoundCache>();
    DependencyManager::set<SoundCacheScriptingInterface>();
    DependencyManager::set<AudioInjectorManager>();

    DependencyManager::set<ScriptCache>();


    // Needed to ensure the creation of the DebugDraw instance on the main thread
    DebugDraw::getInstance();

    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();
    packetReceiver.registerListenerForTypes({ PacketType::OctreeStats, PacketType::EntityData, PacketType::EntityErase },
                                            PacketReceiver::makeSourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleOctreePacket));
    packetReceiver.registerListener(PacketType::SelectedAudioFormat,
        PacketReceiver::makeUnsourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleSelectedAudioFormat));

    packetReceiver.registerListener(PacketType::ReloadEntityServerScript,
        PacketReceiver::makeSourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleReloadEntityServerScriptPacket));
    packetReceiver.registerListener(PacketType::EntityScriptGetStatus,
        PacketReceiver::makeSourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleEntityScriptGetStatusPacket));
    packetReceiver.registerListener(PacketType::EntityServerScriptLog,
        PacketReceiver::makeSourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleEntityServerScriptLogPacket));
    packetReceiver.registerListener(PacketType::EntityScriptCallMethod,
        PacketReceiver::makeSourcedListenerReference<EntityScriptServer>(this, &EntityScriptServer::handleEntityScriptCallMethodPacket));

    static const int LOG_INTERVAL = MSECS_PER_SECOND / 10;
    auto timer = new QTimer(this);
    timer->setInterval(LOG_INTERVAL);
    connect(timer, &QTimer::timeout, this, &EntityScriptServer::pushLogs);
    timer->start();
}

EntityScriptServer::~EntityScriptServer() {
    qInstallMessageHandler(LogHandler::verboseMessageHandler);
}

static const QString ENTITY_SCRIPT_SERVER_LOGGING_NAME = "entity-script-server";

void EntityScriptServer::handleReloadEntityServerScriptPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (senderNode->getCanRez() || senderNode->getCanRezTmp() || senderNode->getCanRezCertified() || senderNode->getCanRezTmpCertified()) {
        auto entityID = QUuid::fromRfc4122(message->read(NUM_BYTES_RFC4122_UUID));

        if (_entityViewer.getTree() && !_shuttingDown) {
            qCDebug(entity_script_server) << "Reloading: " << entityID;
            checkAndCallPreload(entityID, true);
        }
    }
}

void EntityScriptServer::handleEntityScriptGetStatusPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (senderNode->getCanRez() || senderNode->getCanRezTmp() || senderNode->getCanRezCertified() || senderNode->getCanRezTmpCertified()) {
        MessageID messageID;
        message->readPrimitive(&messageID);
        auto entityID = QUuid::fromRfc4122(message->read(NUM_BYTES_RFC4122_UUID));

        auto replyPacketList = NLPacketList::create(PacketType::EntityScriptGetStatusReply, QByteArray(), true, true);
        replyPacketList->writePrimitive(messageID);

        EntityScriptDetails details;
        if (_entitiesScriptManager->getEntityScriptDetails(entityID, details)) {
            replyPacketList->writePrimitive(true);
            replyPacketList->writePrimitive(details.status);
            replyPacketList->writeString(details.errorInfo);
        } else {
            replyPacketList->writePrimitive(false);
        }

        auto nodeList = DependencyManager::get<NodeList>();
        nodeList->sendPacketList(std::move(replyPacketList), *senderNode);
    }
}

void EntityScriptServer::handleSettings() {

    auto nodeList = DependencyManager::get<NodeList>();

    auto& domainHandler = nodeList->getDomainHandler();
    const QJsonObject& settingsObject = domainHandler.getSettingsObject();

    static const QString ENTITY_SCRIPT_SERVER_SETTINGS_KEY = "entity_script_server";

    if (!settingsObject.contains(ENTITY_SCRIPT_SERVER_SETTINGS_KEY)) {
        qWarning() << "Received settings from the domain-server with no entity_script_server section.";
        return;
    }

    auto entityScriptServerSettings = settingsObject[ENTITY_SCRIPT_SERVER_SETTINGS_KEY].toObject();

    static const QString MAX_ENTITY_PPS_OPTION = "max_total_entity_pps";
    static const QString ENTITY_PPS_PER_SCRIPT = "entity_pps_per_script";

    if (!entityScriptServerSettings.contains(MAX_ENTITY_PPS_OPTION) || !entityScriptServerSettings.contains(ENTITY_PPS_PER_SCRIPT)) {
        qWarning() << "Received settings from the domain-server with no max_total_entity_pps or entity_pps_per_script properties.";
        return;
    }

    _maxEntityPPS = std::max(0, entityScriptServerSettings[MAX_ENTITY_PPS_OPTION].toInt());
    _entityPPSPerScript = std::max(0, entityScriptServerSettings[ENTITY_PPS_PER_SCRIPT].toInt());

    qDebug() << QString("Received entity script server settings, Max Entity PPS: %1, Entity PPS Per Entity Script: %2")
                .arg(_maxEntityPPS).arg(_entityPPSPerScript);
}

void EntityScriptServer::updateEntityPPS() {
    int numRunningScripts = _entitiesScriptManager->getNumRunningEntityScripts();
    int pps;
    if (std::numeric_limits<int>::max() / _entityPPSPerScript < numRunningScripts) {
        qWarning() << QString("Integer multiplication would overflow, clamping to maxint: %1 * %2").arg(numRunningScripts).arg(_entityPPSPerScript);
        pps = std::numeric_limits<int>::max();
        pps = std::min(_maxEntityPPS, pps);
    } else {
        pps = _entityPPSPerScript * numRunningScripts;
        pps = std::min(_maxEntityPPS, pps);
    }
    _entityEditSender.setPacketsPerSecond(pps);
}

void EntityScriptServer::handleEntityServerScriptLogPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    bool canRezAny = senderNode->getCanRez() || senderNode->getCanRezTmp() || senderNode->getCanRezCertified() || senderNode->getCanRezTmpCertified();
    bool enable = false;
    message->readPrimitive(&enable);

    auto senderUUID = senderNode->getUUID();
    auto it = _logListeners.find(senderUUID);

    if (enable && canRezAny) {
        if (it == std::end(_logListeners)) {
            _logListeners.insert(senderUUID);
            qCInfo(entity_script_server) << "Node" << senderUUID << "subscribed to log stream";
        }
    } else {
        if (it != std::end(_logListeners)) {
            _logListeners.erase(it);
            qCInfo(entity_script_server) << "Node" << senderUUID << "unsubscribed from log stream";
        }
    }
}

void EntityScriptServer::pushLogs() {
    std::string buffer;
    {
        Lock lock(logBufferMutex);
        std::swap(logBuffer, buffer);
    }

    if (buffer.empty()) {
        return;
    }
    if (_logListeners.empty()) {
        return;
    }

    auto nodeList = DependencyManager::get<NodeList>();
    for (auto uuid : _logListeners) {
        auto node = nodeList->nodeWithUUID(uuid);
        if (node && node->getActiveSocket()) {
            auto packet = NLPacketList::create(PacketType::EntityServerScriptLog, QByteArray(), true, true);
            packet->write(buffer.data(), buffer.size());
            nodeList->sendPacketList(std::move(packet), *node);
        }
    }
}

void EntityScriptServer::handleEntityScriptCallMethodPacket(QSharedPointer<ReceivedMessage> receivedMessage, SharedNodePointer senderNode) {

    if (_entitiesScriptManager && _entityViewer.getTree() && !_shuttingDown) {
        auto entityID = QUuid::fromRfc4122(receivedMessage->read(NUM_BYTES_RFC4122_UUID));

        auto method = receivedMessage->readString();

        quint16 paramCount;
        receivedMessage->readPrimitive(&paramCount);

        QStringList params;
        for (int param = 0; param < paramCount; param++) {
            auto paramString = receivedMessage->readString();
            params << paramString;
        }

        _entitiesScriptManager->callEntityScriptMethod(entityID, method, params, senderNode->getUUID());
    }
}


void EntityScriptServer::run() {
    DependencyManager::set<ScriptEngines>(ScriptManager::ENTITY_SERVER_SCRIPT);
    DependencyManager::set<EntityScriptServerServices>();
    DependencyManager::set<AvatarHashMap>();

    // make sure we request our script once the agent connects to the domain
    auto nodeList = DependencyManager::get<NodeList>();

    ThreadedAssignment::commonInit(ENTITY_SCRIPT_SERVER_LOGGING_NAME, NodeType::EntityScriptServer);

    // Setup MessagesClient
    auto messagesClient = DependencyManager::set<MessagesClient>();
    messagesClient->startThread();

    DomainHandler& domainHandler = DependencyManager::get<NodeList>()->getDomainHandler();
    connect(&domainHandler, &DomainHandler::settingsReceived, this, &EntityScriptServer::handleSettings);

    // make sure we hear about connected nodes so we can grab an ATP script if a request is pending
    connect(nodeList.data(), &LimitedNodeList::nodeActivated, this, &EntityScriptServer::nodeActivated);
    connect(nodeList.data(), &LimitedNodeList::nodeKilled, this, &EntityScriptServer::nodeKilled);

    nodeList->addSetOfNodeTypesToNodeInterestSet({
        NodeType::Agent, NodeType::AudioMixer, NodeType::AvatarMixer,
        NodeType::EntityServer, NodeType::MessagesMixer, NodeType::AssetServer
    });

    // Setup Script Engine
    resetEntitiesScriptEngine();

    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    entityScriptingInterface->init();

    _entityViewer.init();
    
    // setup the JSON filter that asks for entities with a non-default serverScripts property
    QJsonObject queryJSONParameters;
    queryJSONParameters[EntityJSONQueryProperties::SERVER_SCRIPTS_PROPERTY] = EntityQueryFilterSymbol::NonDefault;

    QJsonObject queryFlags;

    queryFlags[EntityJSONQueryProperties::INCLUDE_ANCESTORS_PROPERTY] = true;
    queryFlags[EntityJSONQueryProperties::INCLUDE_DESCENDANTS_PROPERTY] = true;

    queryJSONParameters[EntityJSONQueryProperties::FLAGS_PROPERTY] = queryFlags;
    
    // setup the JSON parameters so that OctreeQuery does not use a frustum and uses our JSON filter
    _entityViewer.getOctreeQuery().setJSONParameters(queryJSONParameters);

    entityScriptingInterface->setEntityTree(_entityViewer.getTree());

    auto treePtr = _entityViewer.getTree();
    DependencyManager::set<AssignmentParentFinder>(treePtr);

    if (!_entitySimulation) {
        SimpleEntitySimulationPointer simpleSimulation { new SimpleEntitySimulation() };
        simpleSimulation->setEntityTree(treePtr);
        treePtr->setSimulation(simpleSimulation);
        _entitySimulation = simpleSimulation;
    }

    auto tree = treePtr.get();
    connect(tree, &EntityTree::deletingEntity, this, &EntityScriptServer::deletingEntity, Qt::QueuedConnection);
    connect(tree, &EntityTree::addingEntity, this, &EntityScriptServer::addingEntity, Qt::QueuedConnection);
    connect(tree, &EntityTree::entityServerScriptChanging, this, &EntityScriptServer::entityServerScriptChanging, Qt::QueuedConnection);
}

void EntityScriptServer::cleanupOldKilledListeners() {
    auto threshold = usecTimestampNow() - 5 * USECS_PER_SECOND;
    using ValueType = std::pair<QUuid, quint64>;
    auto it = std::remove_if(std::begin(_killedListeners), std::end(_killedListeners), [&](ValueType value) {
        return value.second < threshold;
    });
    _killedListeners.erase(it, std::end(_killedListeners));
}

void EntityScriptServer::nodeActivated(SharedNodePointer activatedNode) {
    switch (activatedNode->getType()) {
        case NodeType::AudioMixer:
            negotiateAudioFormat();
            break;
        case NodeType::Agent: {
            auto activatedNodeUUID = activatedNode->getUUID();
            using ValueType = std::pair<QUuid, quint64>;
            auto it = std::find_if(std::begin(_killedListeners), std::end(_killedListeners), [&](ValueType value) {
                return value.first == activatedNodeUUID;
            });
            if (it != std::end(_killedListeners)) {
                _killedListeners.erase(it);
                _logListeners.insert(activatedNodeUUID);
            }
            break;
        }
        default:
            // Do nothing
            break;
    }
}

void EntityScriptServer::nodeKilled(SharedNodePointer killedNode) {
    switch (killedNode->getType()) {
        case NodeType::EntityServer: {
            // Before we clear, make sure this was our only entity server.
            // Otherwise we're assuming that we have "trading" entity servers
            // (an old one going away and a new one coming onboard)
            // and that we shouldn't clear here because we're still doing work.
            bool hasAnotherEntityServer = false;
            auto nodeList = DependencyManager::get<NodeList>();

            nodeList->eachNodeBreakable([&hasAnotherEntityServer, &killedNode](const SharedNodePointer& node){
                if (node->getType() == NodeType::EntityServer && node->getUUID() != killedNode->getUUID()) {
                    // we're talking to > 1 entity servers, we know we won't clear
                    hasAnotherEntityServer = true;
                    return false;
                }

                return true;
            });

            if (!hasAnotherEntityServer) {
                clear();
            }
            
            break;
        }
        case NodeType::Agent: {
            cleanupOldKilledListeners();

            auto killedNodeUUID = killedNode->getUUID();
            auto it = _logListeners.find(killedNodeUUID);
            if (it != std::end(_logListeners)) {
                _logListeners.erase(killedNodeUUID);
                _killedListeners.emplace_back(killedNodeUUID, usecTimestampNow());
            }
            break;
        }
        default:
            // Do nothing
            break;
    }
}

void EntityScriptServer::negotiateAudioFormat() {
    auto nodeList = DependencyManager::get<NodeList>();
    auto negotiateFormatPacket = NLPacket::create(PacketType::NegotiateAudioFormat);
    auto codecPlugins = PluginManager::getInstance()->getCodecPlugins();
    quint8 numberOfCodecs = (quint8)codecPlugins.size();
    negotiateFormatPacket->writePrimitive(numberOfCodecs);
    for (auto& plugin : codecPlugins) {
        auto codecName = plugin->getName();
        negotiateFormatPacket->writeString(codecName);
    }

    // grab our audio mixer from the NodeList, if it exists
    SharedNodePointer audioMixer = nodeList->soloNodeOfType(NodeType::AudioMixer);

    if (audioMixer) {
        // send off this mute packet
        nodeList->sendPacket(std::move(negotiateFormatPacket), *audioMixer);
    }
}

void EntityScriptServer::handleSelectedAudioFormat(QSharedPointer<ReceivedMessage> message) {
    QString selectedCodecName = message->readString();
    selectAudioFormat(selectedCodecName);
}

void EntityScriptServer::selectAudioFormat(const QString& selectedCodecName) {
    _selectedCodecName = selectedCodecName;

    qCDebug(entity_script_server) << "Selected Codec:" << _selectedCodecName;

    // release any old codec encoder/decoder first...
    if (_codec && _encoder) {
        _codec->releaseEncoder(_encoder);
        _encoder = nullptr;
        _codec = nullptr;
    }

    auto codecPlugins = PluginManager::getInstance()->getCodecPlugins();
    for (auto& plugin : codecPlugins) {
        if (_selectedCodecName == plugin->getName()) {
            _codec = plugin;
            _encoder = plugin->createEncoder(AudioConstants::SAMPLE_RATE, AudioConstants::MONO);
            qCDebug(entity_script_server) << "Selected Codec Plugin:" << _codec.get();
            break;
        }
    }
}

void EntityScriptServer::resetEntitiesScriptEngine() {
    auto engineName = QString("about:Entities %1").arg(++_entitiesScriptEngineCount);
    auto newManager = scriptManagerFactory(ScriptManager::ENTITY_SERVER_SCRIPT, NO_SCRIPT, engineName);
    auto newEngine = newManager->engine();

    auto webSocketServerConstructorValue = newEngine->newFunction(WebSocketServerClass::constructor);
    newEngine->globalObject()->setProperty("WebSocketServer", webSocketServerConstructorValue);

    newEngine->registerGlobalObject("SoundCache", DependencyManager::get<SoundCacheScriptingInterface>().data());
    newEngine->registerGlobalObject("AvatarList", DependencyManager::get<AvatarHashMap>().data());

    // connect this script engines printedMessage signal to the global ScriptEngines these various messages
    auto scriptEngines = DependencyManager::get<ScriptEngines>().data();
    connect(newManager.data(), &ScriptManager::printedMessage, scriptEngines, &ScriptEngines::onPrintedMessage);
    connect(newManager.data(), &ScriptManager::errorMessage, scriptEngines, &ScriptEngines::onErrorMessage);
    connect(newManager.data(), &ScriptManager::warningMessage, scriptEngines, &ScriptEngines::onWarningMessage);
    connect(newManager.data(), &ScriptManager::infoMessage, scriptEngines, &ScriptEngines::onInfoMessage);

    connect(newManager.data(), &ScriptManager::update, this, [this] {
        _entityViewer.queryOctree();
        _entityViewer.getTree()->preUpdate();
        _entityViewer.getTree()->update();
    });

    scriptEngines->runScriptInitializers(newManager);
    newManager->runInThread();
    auto newEngineSP = qSharedPointerCast<EntitiesScriptEngineProvider>(newManager);
    // On the entity script server, these are the same
    DependencyManager::get<EntityScriptingInterface>()->setPersistentEntitiesScriptEngine(newEngineSP);
    DependencyManager::get<EntityScriptingInterface>()->setNonPersistentEntitiesScriptEngine(newEngineSP);

    if (_entitiesScriptManager) {
        disconnect(_entitiesScriptManager.data(), &ScriptManager::entityScriptDetailsUpdated,
                   this, &EntityScriptServer::updateEntityPPS);
    }

    _entitiesScriptManager.swap(newManager);
    connect(_entitiesScriptManager.data(), &ScriptManager::entityScriptDetailsUpdated,
            this, &EntityScriptServer::updateEntityPPS);
}


void EntityScriptServer::clear() {
    // unload and stop the engine
    if (_entitiesScriptManager) {
        // do this here (instead of in deleter) to avoid marshalling unload signals back to this thread
        _entitiesScriptManager->unloadAllEntityScripts();
        _entitiesScriptManager->stop();
        _entitiesScriptManager->waitTillDoneRunning();
    }

    _entityViewer.clear();

    // reset the engine
    if (!_shuttingDown) {
        resetEntitiesScriptEngine();
    }
}

void EntityScriptServer::shutdownScriptEngine() {
    if (_entitiesScriptManager) {
        _entitiesScriptManager->disconnectNonEssentialSignals(); // disconnect all slots/signals from the script engine, except essential
    }
    _shuttingDown = true;

    clear(); // always clear() on shutdown

    auto scriptEngines = DependencyManager::get<ScriptEngines>();
    scriptEngines->shutdownScripting();

    _entitiesScriptManager.clear();

    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    // our entity tree is going to go away so tell that to the EntityScriptingInterface
    entityScriptingInterface->setEntityTree(nullptr);

    // Should always be true as they are singletons.
    if (entityScriptingInterface->getPacketSender() == &_entityEditSender) {
        // The packet sender is about to go away.
        entityScriptingInterface->setPacketSender(nullptr);
    }
}

void EntityScriptServer::addingEntity(const EntityItemID& entityID) {
    checkAndCallPreload(entityID);
}

void EntityScriptServer::deletingEntity(const EntityItemID& entityID) {
    if (_entityViewer.getTree() && !_shuttingDown && _entitiesScriptManager) {
        _entitiesScriptManager->unloadEntityScript(entityID, true);
    }
}

void EntityScriptServer::entityServerScriptChanging(const EntityItemID& entityID, bool reload) {
    if (_entityViewer.getTree() && !_shuttingDown) {
        checkAndCallPreload(entityID, reload);
    }
}

void EntityScriptServer::checkAndCallPreload(const EntityItemID& entityID, bool forceRedownload) {
    if (_entityViewer.getTree() && !_shuttingDown && _entitiesScriptManager) {

        EntityItemPointer entity = _entityViewer.getTree()->findEntityByEntityItemID(entityID);
        EntityScriptDetails details;
        bool isRunning = _entitiesScriptManager->getEntityScriptDetails(entityID, details);
        if (entity && (forceRedownload || !isRunning || details.scriptText != entity->getServerScripts())) {
            if (isRunning) {
                _entitiesScriptManager->unloadEntityScript(entityID, true);
            }

            QString scriptUrl = entity->getServerScripts();
            if (!scriptUrl.isEmpty()) {
                scriptUrl = DependencyManager::get<ResourceManager>()->normalizeURL(scriptUrl);
                _entitiesScriptManager->loadEntityScript(entityID, scriptUrl, forceRedownload);
            }
        }
    }
}

void EntityScriptServer::sendStatsPacket() {
    QJsonObject statsObject;

    QJsonObject octreeStats;
    octreeStats["elementCount"] = (double)OctreeElement::getNodeCount();
    octreeStats["internalElementCount"] = (double)OctreeElement::getInternalNodeCount();
    octreeStats["leafElementCount"] = (double)OctreeElement::getLeafNodeCount();
    statsObject["octree_stats"] = octreeStats;

    QJsonObject scriptEngineStats;
    int numberRunningScripts = 0;
    const auto scriptManager = _entitiesScriptManager;
    if (scriptManager) {
        numberRunningScripts = scriptManager->getNumRunningEntityScripts();
    }
    scriptEngineStats["number_running_scripts"] = numberRunningScripts;
    statsObject["script_engine_stats"] = scriptEngineStats;
    

    auto nodeList = DependencyManager::get<NodeList>();
    QJsonObject nodesObject;
    nodeList->eachNode([&](const SharedNodePointer& node) {
        QJsonObject clientStats;
        const QString uuidString(uuidStringWithoutCurlyBraces(node->getUUID()));
        clientStats["node_type"] = NodeType::getNodeTypeName(node->getType());
        auto& nodeStats = node->getConnectionStats();

        static const QString NODE_OUTBOUND_KBPS_STAT_KEY("outbound_kbit/s");
        static const QString NODE_INBOUND_KBPS_STAT_KEY("inbound_kbit/s");

        // add the key to ask the domain-server for a username replacement, if it has it
        clientStats[USERNAME_UUID_REPLACEMENT_STATS_KEY] = uuidString;

        clientStats[NODE_OUTBOUND_KBPS_STAT_KEY] = node->getOutboundKbps();
        clientStats[NODE_INBOUND_KBPS_STAT_KEY] = node->getInboundKbps();

        using namespace std::chrono;
        const float statsPeriod = duration<float, seconds::period>(nodeStats.endTime - nodeStats.startTime).count();
        clientStats["unreliable_packet/s"] = (nodeStats.sentUnreliablePackets + nodeStats.receivedUnreliablePackets) / statsPeriod;
        clientStats["reliable_packet/s"] = (nodeStats.sentPackets + nodeStats.receivedPackets) / statsPeriod;

        nodesObject[uuidString] = clientStats;
    });

    statsObject["nodes"] = nodesObject;
    addPacketStatsAndSendStatsPacket(statsObject);
}

void EntityScriptServer::handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    auto packetType = message->getType();

    if (packetType == PacketType::OctreeStats) {

        int statsMessageLength = OctreeHeadlessViewer::parseOctreeStats(message, senderNode);
        if (message->getSize() > statsMessageLength) {
            // pull out the piggybacked packet and create a new QSharedPointer<NLPacket> for it
            int piggyBackedSizeWithHeader = message->getSize() - statsMessageLength;

            auto buffer = std::unique_ptr<char[]>(new char[piggyBackedSizeWithHeader]);
            memcpy(buffer.get(), message->getRawMessage() + statsMessageLength, piggyBackedSizeWithHeader);

            auto newPacket = NLPacket::fromReceivedPacket(std::move(buffer), piggyBackedSizeWithHeader, message->getSenderSockAddr());
            message = QSharedPointer<ReceivedMessage>::create(*newPacket);
        } else {
            return; // bail since no piggyback data
        }

        packetType = message->getType();
    } // fall through to piggyback message

    if (packetType == PacketType::EntityData) {
        _entityViewer.processDatagram(*message, senderNode);
    } else if (packetType == PacketType::EntityErase) {
        _entityViewer.processEraseMessage(*message, senderNode);
    }
}

void EntityScriptServer::aboutToFinish() {
    shutdownScriptEngine();

    DependencyManager::get<EntityScriptingInterface>()->setEntityTree(nullptr);
    DependencyManager::get<ResourceManager>()->cleanup();

    DependencyManager::destroy<AudioScriptingInterface>();
    DependencyManager::destroy<SoundCacheScriptingInterface>();
    DependencyManager::destroy<ResourceScriptingInterface>();
    DependencyManager::destroy<EntityScriptingInterface>();

    DependencyManager::destroy<SoundCache>();
    DependencyManager::destroy<ScriptCache>();

    DependencyManager::destroy<ResourceManager>();
    DependencyManager::destroy<ResourceCacheSharedItems>();

    DependencyManager::destroy<MessagesClient>();

    DependencyManager::destroy<AssignmentDynamicFactory>();
    DependencyManager::destroy<AssignmentParentFinder>();
    DependencyManager::destroy<AvatarHashMap>();


    DependencyManager::destroy<PluginManager>();


    // cleanup the AudioInjectorManager (and any still running injectors)
    DependencyManager::destroy<AudioInjectorManager>();

    DependencyManager::destroy<ScriptEngines>();
    DependencyManager::destroy<EntityScriptServerServices>();

    // cleanup codec & encoder
    if (_codec && _encoder) {
        _codec->releaseEncoder(_encoder);
        _encoder = nullptr;
    }
}
