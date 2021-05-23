//
//  Agent.cpp
//  assignment-client/src
//
//  Created by Stephen Birarda on 7/1/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Agent.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QStandardPaths>
#include <QtNetwork/QNetworkDiskCache>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QThread>

#include <AnimationCacheScriptingInterface.h>
#include <AssetClient.h>
#include <AvatarHashMap.h>
#include <AudioInjectorManager.h>
#include <AssetClient.h>
#include <DebugDraw.h>
#include <EntityScriptingInterface.h>
#include <LocationScriptingInterface.h>
#include <AudioScriptingInterface.h>
#include <MessagesClient.h>
#include <NetworkAccessManager.h>
#include <NodeList.h>
#include <udt/PacketHeaders.h>
#include <ResourceCache.h>
#include <ResourceScriptingInterface.h>
#include <ScriptCache.h>
#include <ScriptEngine.h>
#include <ScriptEngines.h>
#include <ScriptManager.h>
#include <SoundCacheScriptingInterface.h>
#include <SoundCache.h>
#include <UserActivityLoggerScriptingInterface.h>
#include <UsersScriptingInterface.h>
#include <UUID.h>

#include <recording/ClipCache.h>
#include <recording/Deck.h>
#include <recording/Recorder.h>
#include <recording/Frame.h>

#include <plugins/CodecPlugin.h>
#include <plugins/PluginManager.h>

#include <WebSocketServerClass.h>
#include <EntityScriptingInterface.h> // TODO: consider moving to scriptengine.h

#include <hfm/ModelFormatRegistry.h>

#include "entities/AssignmentParentFinder.h"
#include "AssignmentDynamicFactory.h"
#include "RecordingScriptingInterface.h"
#include "AbstractAudioInterface.h"
#include "AgentScriptingInterface.h"

static const int RECEIVED_AUDIO_STREAM_CAPACITY_FRAMES = 10;

Agent::Agent(ReceivedMessage& message) :
    ThreadedAssignment(message),
    _receivedAudioStream(RECEIVED_AUDIO_STREAM_CAPACITY_FRAMES, RECEIVED_AUDIO_STREAM_CAPACITY_FRAMES),
    _audioGate(AudioConstants::SAMPLE_RATE, AudioConstants::MONO),
    _avatarAudioTimer(this)
{
    DependencyManager::set<ScriptableAvatar>();

    DependencyManager::registerInheritance<EntityDynamicFactoryInterface, AssignmentDynamicFactory>();
    DependencyManager::set<AssignmentDynamicFactory>();

    DependencyManager::set<AnimationCache>();
    DependencyManager::set<AnimationCacheScriptingInterface>();
    DependencyManager::set<EntityScriptingInterface>(false);

    DependencyManager::set<ResourceScriptingInterface>();
    DependencyManager::set<UserActivityLoggerScriptingInterface>();

    _entityEditSender.setPacketsPerSecond(DEFAULT_ENTITY_PPS_PER_SCRIPT);
    DependencyManager::get<EntityScriptingInterface>()->setPacketSender(&_entityEditSender);

    DependencyManager::set<ResourceManager>();
    DependencyManager::set<PluginManager>()->instantiate();

    DependencyManager::registerInheritance<SpatialParentFinder, AssignmentParentFinder>();

    DependencyManager::set<ResourceCacheSharedItems>();
    DependencyManager::set<SoundCache>();
    DependencyManager::set<SoundCacheScriptingInterface>();
    DependencyManager::set<AudioScriptingInterface>();
    DependencyManager::set<AudioInjectorManager>();

    DependencyManager::set<recording::Deck>();
    DependencyManager::set<recording::Recorder>();
    DependencyManager::set<recording::ClipCache>();

    DependencyManager::set<RecordingScriptingInterface>();
    DependencyManager::set<UsersScriptingInterface>();

    DependencyManager::set<ModelFormatRegistry>();
    DependencyManager::set<ModelCache>();

    // Needed to ensure the creation of the DebugDraw instance on the main thread
    DebugDraw::getInstance();


    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();

    packetReceiver.registerListenerForTypes(
        { PacketType::MixedAudio, PacketType::SilentAudioFrame },
        PacketReceiver::makeUnsourcedListenerReference<Agent>(this, &Agent::handleAudioPacket));
    packetReceiver.registerListenerForTypes(
        { PacketType::OctreeStats, PacketType::EntityData, PacketType::EntityErase },
        PacketReceiver::makeSourcedListenerReference<Agent>(this, &Agent::handleOctreePacket));
    packetReceiver.registerListener(PacketType::SelectedAudioFormat,
        PacketReceiver::makeUnsourcedListenerReference<Agent>(this, &Agent::handleSelectedAudioFormat));

    // 100Hz timer for audio
    const int TARGET_INTERVAL_MSEC = 10; // 10ms
    connect(&_avatarAudioTimer, &QTimer::timeout, this, &Agent::processAgentAvatarAudio);
    _avatarAudioTimer.setSingleShot(false);
    _avatarAudioTimer.setInterval(TARGET_INTERVAL_MSEC);
    _avatarAudioTimer.setTimerType(Qt::PreciseTimer);
}

void Agent::playAvatarSound(SharedSoundPointer sound) {
    // this must happen on Agent's main thread
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "playAvatarSound", Q_ARG(SharedSoundPointer, sound));
        return;
    } else {
        // TODO: seems to add occasional artifact in tests.  I believe it is
        // correct to do this, but need to figure out for sure, so commenting this
        // out until I verify.
        // _numAvatarSoundSentBytes = 0;
        setAvatarSound(sound);
    }
}

void Agent::handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
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

void Agent::handleAudioPacket(QSharedPointer<ReceivedMessage> message) {
    _receivedAudioStream.parseData(*message);
    _lastReceivedAudioLoudness = _receivedAudioStream.getNextOutputFrameLoudness();
    _receivedAudioStream.clearBuffer();
}

static const QString AGENT_LOGGING_NAME = "agent";

void Agent::run() {
    // Create ScriptEngines on threaded-assignment thread then move to main thread.
    DependencyManager::set<ScriptEngines>(ScriptManager::AGENT_SCRIPT)->moveToThread(qApp->thread());

    DependencyManager::set<ScriptCache>();

    // make sure we request our script once the agent connects to the domain
    auto nodeList = DependencyManager::get<NodeList>();

    connect(&nodeList->getDomainHandler(), &DomainHandler::connectedToDomain, this, &Agent::requestScript);

    ThreadedAssignment::commonInit(AGENT_LOGGING_NAME, NodeType::Agent);

    // Setup MessagesClient
    auto messagesClient = DependencyManager::set<MessagesClient>();
    messagesClient->startThread();

    // make sure we hear about connected nodes so we can grab an ATP script if a request is pending
    connect(nodeList.data(), &LimitedNodeList::nodeActivated, this, &Agent::nodeActivated);

    // make sure we hear about dissappearing nodes so we can clear the entity tree if an entity server goes away
    connect(nodeList.data(), &LimitedNodeList::nodeKilled, this,  &Agent::nodeKilled);

    nodeList->addSetOfNodeTypesToNodeInterestSet({
        NodeType::AudioMixer, NodeType::AvatarMixer, NodeType::EntityServer,
        NodeType::MessagesMixer, NodeType::AssetServer, NodeType::EntityScriptServer
    });
}

void Agent::requestScript() {
    auto nodeList = DependencyManager::get<NodeList>();
    disconnect(&nodeList->getDomainHandler(), &DomainHandler::connectedToDomain, this, &Agent::requestScript);

    // figure out the URL for the script for this agent assignment
    QUrl scriptURL;
    if (_payload.isEmpty())  {
        scriptURL = QUrl(QString("http://%1:%2/assignment/%3/")
                         .arg(nodeList->getDomainHandler().getIP().toString())
                         .arg(DOMAIN_SERVER_HTTP_PORT)
                         .arg(uuidStringWithoutCurlyBraces(nodeList->getSessionUUID())));
    } else {
        scriptURL = QUrl(_payload);
    }

    // make sure this is not a script request for the file scheme
    if (scriptURL.scheme() == HIFI_URL_SCHEME_FILE) {
        qWarning() << "Cannot load script for Agent from local filesystem.";
        scriptRequestFinished();
        return;
    }

    auto request = DependencyManager::get<ResourceManager>()->createResourceRequest(
        this, scriptURL, true, -1, "Agent::requestScript");

    if (!request) {
        qWarning() << "Could not create ResourceRequest for Agent script at" << scriptURL.toString();
        scriptRequestFinished();
        return;
    }

    // setup a timeout for script request
    static const int SCRIPT_TIMEOUT_MS = 10000;
    _scriptRequestTimeout = new QTimer;
    connect(_scriptRequestTimeout, &QTimer::timeout, this, &Agent::scriptRequestFinished);
    _scriptRequestTimeout->start(SCRIPT_TIMEOUT_MS);

    connect(request, &ResourceRequest::finished, this, &Agent::scriptRequestFinished);

    if (scriptURL.scheme() == URL_SCHEME_ATP) {
        // we have an ATP URL for the script - if we're not currently connected to the AssetServer
        // then wait for the nodeConnected signal to fire off the request

        auto assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

        if (!assetServer || !assetServer->getActiveSocket()) {
            qDebug() << "Waiting to connect to Asset Server for ATP script download.";
            _pendingScriptRequest = request;

            return;
        }
    }

    qInfo() << "Requesting script at URL" << qPrintable(request->getUrl().toString());

    request->send();
}

void Agent::nodeActivated(SharedNodePointer activatedNode) {
    if (_pendingScriptRequest && activatedNode->getType() == NodeType::AssetServer) {
        qInfo() << "Requesting script at URL" << qPrintable(_pendingScriptRequest->getUrl().toString());

        _pendingScriptRequest->send();

        _pendingScriptRequest = nullptr;
    }
    if (activatedNode->getType() == NodeType::AudioMixer) {
        negotiateAudioFormat();
    }
}

void Agent::nodeKilled(SharedNodePointer killedNode) {
    if (killedNode->getType() == NodeType::EntityServer) {
        // an entity server has gone away, ask the headless viewer to clear its tree
        _entityViewer.clear();
    }
}

void Agent::negotiateAudioFormat() {
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

void Agent::handleSelectedAudioFormat(QSharedPointer<ReceivedMessage> message) {
    QString selectedCodecName = message->readString();
    selectAudioFormat(selectedCodecName);
}

void Agent::selectAudioFormat(const QString& selectedCodecName) {
    if (_selectedCodecName == selectedCodecName) {
        return;
    }
    _selectedCodecName = selectedCodecName;

    qDebug() << "Selected Codec:" << _selectedCodecName;

    // release any old codec encoder/decoder first...
    if (_codec && _encoder) {
        _codec->releaseEncoder(_encoder);
        _encoder = nullptr;
        _codec = nullptr;
    }
    _receivedAudioStream.cleanupCodec();

    auto codecPlugins = PluginManager::getInstance()->getCodecPlugins();
    for (auto& plugin : codecPlugins) {
        if (_selectedCodecName == plugin->getName()) {
            _codec = plugin;
            _receivedAudioStream.setupCodec(plugin, _selectedCodecName, AudioConstants::STEREO);
            _encoder = plugin->createEncoder(AudioConstants::SAMPLE_RATE, AudioConstants::MONO);
            qDebug() << "Selected Codec Plugin:" << _codec.get();
            break;
        }
    }
}

void Agent::scriptRequestFinished() {
    auto request = qobject_cast<ResourceRequest*>(sender());

    // stop the script request timeout, if it's running
    if (_scriptRequestTimeout) {
        QMetaObject::invokeMethod(_scriptRequestTimeout, "stop");
        _scriptRequestTimeout->deleteLater();
    }

    if (request && request->getResult() == ResourceRequest::Success) {
        _scriptContents = request->getData();
        qInfo() << "Downloaded script:" << _scriptContents;

        // we could just call executeScript directly - we use a QueuedConnection to allow scriptRequestFinished
        // to return before calling executeScript
        QMetaObject::invokeMethod(this, "executeScript", Qt::QueuedConnection);
    } else {
        if (request) {
            qWarning() << "Failed to download script at" << request->getUrl().toString() << " - bailing on assignment.";
            qWarning() << "ResourceRequest error was" << request->getResult();
        } else {
            qWarning() << "Failed to download script - request timed out. Bailing on assignment.";
        }

        setFinished(true);
    }

    request->deleteLater();
}

void Agent::executeScript() {
    // the following block is scoped so that any shared pointers we take here
    // are cleared before we call setFinished at the end of the function
    {
        _scriptManager = scriptManagerFactory(ScriptManager::AGENT_SCRIPT, _scriptContents, _payload);

        // setup an Avatar for the script to use
        auto scriptedAvatar = DependencyManager::get<ScriptableAvatar>();
        scriptedAvatar->setID(getSessionUUID());

        // call model URL setters with empty URLs so our avatar, if user, will have the default models
        scriptedAvatar->setSkeletonModelURL(QUrl());

        // force lazy initialization of the head data for the scripted avatar
        // since it is referenced below by computeLoudness and getAudioLoudness
        scriptedAvatar->getHeadOrientation();

        // give this AvatarData object to the script engine
        auto scriptEngine = _scriptManager->engine();
        scriptEngine->registerGlobalObject("Avatar", scriptedAvatar.data());

        // give scripts access to the Users object
        scriptEngine->registerGlobalObject("Users", DependencyManager::get<UsersScriptingInterface>().data());

        auto player = DependencyManager::get<recording::Deck>();
        connect(player.data(), &recording::Deck::playbackStateChanged, [&player, &scriptedAvatar] {
            if (player->isPlaying()) {
                auto recordingInterface = DependencyManager::get<RecordingScriptingInterface>();
                if (recordingInterface->getPlayFromCurrentLocation()) {
                    scriptedAvatar->setRecordingBasis();
                }

                // these procedural movements are included in the recordings
                scriptedAvatar->setHasScriptedBlendshapes(true);
                scriptedAvatar->setHasProceduralEyeFaceMovement(false);
                scriptedAvatar->setHasProceduralBlinkFaceMovement(false);
                scriptedAvatar->setHasAudioEnabledFaceMovement(false);
            } else {
                scriptedAvatar->clearRecordingBasis();

                // restore procedural blendshape movement
                scriptedAvatar->setHasScriptedBlendshapes(false);
                scriptedAvatar->setHasProceduralEyeFaceMovement(true);
                scriptedAvatar->setHasProceduralBlinkFaceMovement(true);
                scriptedAvatar->setHasAudioEnabledFaceMovement(true);
            }
        });

        using namespace recording;
        static const FrameType AVATAR_FRAME_TYPE = Frame::registerFrameType(AvatarData::FRAME_NAME);
        Frame::registerFrameHandler(AVATAR_FRAME_TYPE, [scriptedAvatar](Frame::ConstPointer frame) {

            auto recordingInterface = DependencyManager::get<RecordingScriptingInterface>();
            bool useFrameSkeleton = recordingInterface->getPlayerUseSkeletonModel();

            // FIXME - the ability to switch the avatar URL is not actually supported when playing back from a recording
            if (!useFrameSkeleton) {
                static std::once_flag warning;
                std::call_once(warning, [] {
                    qWarning() << "Recording.setPlayerUseSkeletonModel(false) is not currently supported.";
                });
            }

            AvatarData::fromFrame(frame->data, *scriptedAvatar);
        });

        using namespace recording;
        static const FrameType AUDIO_FRAME_TYPE = Frame::registerFrameType(AudioConstants::getAudioFrameName());
        Frame::registerFrameHandler(AUDIO_FRAME_TYPE, [this, &player, &scriptedAvatar](Frame::ConstPointer frame) {
            if (_shouldMuteRecordingAudio) {
                return;
            }

            static quint16 audioSequenceNumber{ 0 };

            QByteArray audio(frame->data);

            int16_t* samples = reinterpret_cast<int16_t*>(audio.data());
            int numSamples = AudioConstants::NETWORK_FRAME_SAMPLES_PER_CHANNEL;

            auto volume = player->getVolume();
            if (volume >= 0.0f && volume < 1.0f) {
                int32_t fract = (int32_t)(volume * (float)(1 << 16));   // Q16
                for (int i = 0; i < numSamples; i++) {
                    samples[i] = (fract * (int32_t)samples[i]) >> 16;
                }
            }

            if (_isNoiseGateEnabled) {
                _audioGate.render(samples, samples, numSamples);
            }

            computeLoudness(&audio, scriptedAvatar);

            // state machine to detect gate opening and closing
            bool audioGateOpen = (scriptedAvatar->getAudioLoudness() != 0.0f);
            bool openedInLastBlock = !_audioGateOpen && audioGateOpen;  // the gate just opened
            bool closedInLastBlock = _audioGateOpen && !audioGateOpen;  // the gate just closed
            _audioGateOpen = audioGateOpen;
            Q_UNUSED(openedInLastBlock);

            // the codec must be flushed to silence before sending silent packets,
            // so delay the transition to silent packets by one packet after becoming silent.
            auto packetType = PacketType::MicrophoneAudioNoEcho;
            if (!audioGateOpen && !closedInLastBlock) {
                packetType = PacketType::SilentAudioFrame;
            }

            Transform audioTransform;
            auto headOrientation = scriptedAvatar->getHeadOrientation();
            audioTransform.setTranslation(scriptedAvatar->getWorldPosition());
            audioTransform.setRotation(headOrientation);

            QByteArray encodedBuffer;
            if (_encoder) {
                _encoder->encode(audio, encodedBuffer);
            } else {
                encodedBuffer = audio;
            }

            AbstractAudioInterface::emitAudioPacket(encodedBuffer.data(), encodedBuffer.size(), audioSequenceNumber, false,
                                                    audioTransform, scriptedAvatar->getWorldPosition(), glm::vec3(0),
                                                    packetType, _selectedCodecName);
        });

        auto avatarHashMap = DependencyManager::set<AvatarHashMap>();
        scriptEngine->registerGlobalObject("AvatarList", avatarHashMap.data());

        // register ourselves to the script engine
        scriptEngine->registerGlobalObject("Agent", new AgentScriptingInterface(this));

        scriptEngine->registerGlobalObject("AnimationCache", DependencyManager::get<AnimationCacheScriptingInterface>().data());
        scriptEngine->registerGlobalObject("SoundCache", DependencyManager::get<SoundCacheScriptingInterface>().data());

        ScriptValuePointer webSocketServerConstructorValue = scriptEngine->newFunction(WebSocketServerClass::constructor);
        scriptEngine->globalObject()->setProperty("WebSocketServer", webSocketServerConstructorValue);

        auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();

        scriptEngine->registerGlobalObject("EntityViewer", &_entityViewer);

        scriptEngine->registerGetterSetter("location", LocationScriptingInterface::locationGetter,
                                            LocationScriptingInterface::locationSetter);

        auto recordingInterface = DependencyManager::get<RecordingScriptingInterface>();
        scriptEngine->registerGlobalObject("Recording", recordingInterface.data());

        entityScriptingInterface->init();

        _entityViewer.init();

        entityScriptingInterface->setEntityTree(_entityViewer.getTree());

        DependencyManager::set<AssignmentParentFinder>(_entityViewer.getTree());

        DependencyManager::get<ScriptEngines>()->runScriptInitializers(_scriptManager);
        _scriptManager->run();

        Frame::clearFrameHandler(AUDIO_FRAME_TYPE);
        Frame::clearFrameHandler(AVATAR_FRAME_TYPE);

        if (recordingInterface->isPlaying()) {
            recordingInterface->stopPlaying();
        }

        if (recordingInterface->isRecording()) {
            recordingInterface->stopRecording();
        }

        setIsAvatar(false); // will stop timers for sending identity packets
    }

    setFinished(true);
}

QUuid Agent::getSessionUUID() const {
    return DependencyManager::get<NodeList>()->getSessionUUID();
}

void Agent::setIsListeningToAudioStream(bool isListeningToAudioStream) {
    // this must happen on Agent's main thread
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setIsListeningToAudioStream", Q_ARG(bool, isListeningToAudioStream));
        return;
    }
    if (_isListeningToAudioStream) {
        // have to tell just the audio mixer to KillAvatar.

        auto nodeList = DependencyManager::get<NodeList>();
        nodeList->eachMatchingNode(
            [](const SharedNodePointer& node)->bool {
            return (node->getType() == NodeType::AudioMixer) && node->getActiveSocket();
        },
            [&](const SharedNodePointer& node) {
            qDebug() << "sending KillAvatar message to Audio Mixers";
            auto packet = NLPacket::create(PacketType::KillAvatar, NUM_BYTES_RFC4122_UUID + sizeof(KillAvatarReason), true);
            packet->write(getSessionUUID().toRfc4122());
            packet->writePrimitive(KillAvatarReason::NoReason);
            nodeList->sendPacket(std::move(packet), *node);
        });

    }
    _isListeningToAudioStream = isListeningToAudioStream;
}

void Agent::setIsNoiseGateEnabled(bool isNoiseGateEnabled) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setIsNoiseGateEnabled", Q_ARG(bool, isNoiseGateEnabled));
        return;
    }
    _isNoiseGateEnabled = isNoiseGateEnabled;
}

void Agent::setIsAvatar(bool isAvatar) {
    // this must happen on Agent's main thread
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setIsAvatar", Q_ARG(bool, isAvatar));
        return;
    }
    _isAvatar = isAvatar;

    auto scriptableAvatar = DependencyManager::get<ScriptableAvatar>();
    if (_isAvatar) {
        if (!_avatarQueryTimer) {
            // set up the avatar timers
            _avatarQueryTimer = new QTimer(this);

            // connect our slot
            connect(_avatarQueryTimer, &QTimer::timeout, this, &Agent::queryAvatars);

            static const int AVATAR_VIEW_PACKET_SEND_INTERVAL_MSECS = 1000;

            // start the timer
            _avatarQueryTimer->start(AVATAR_VIEW_PACKET_SEND_INTERVAL_MSECS);

            connect(_scriptManager.data(), &ScriptManager::update,
                    scriptableAvatar.data(), &ScriptableAvatar::update, Qt::QueuedConnection);

            // tell the avatarAudioTimer to start ticking
            QMetaObject::invokeMethod(&_avatarAudioTimer, "start");
        }

        _entityEditSender.setMyAvatar(scriptableAvatar.data());
    } else {
        if (_avatarQueryTimer) {
            _avatarQueryTimer->stop();
            delete _avatarQueryTimer;
            _avatarQueryTimer = nullptr;

            // Clear the skeleton model so that if agent is set to an avatar again the skeleton model is (re)loaded.
            auto scriptedAvatar = DependencyManager::get<ScriptableAvatar>();
            scriptedAvatar->setSkeletonModelURL(QUrl());

            // The avatar mixer never times out a connection (e.g., based on identity or data packets)
            // but rather keeps avatars in its list as long as "connected". As a result, clients timeout
            // when we stop sending identity, but then get woken up again by the mixer itself, which sends
            // identity packets to everyone. Here we explicitly tell the mixer to kill the entry for us.
            auto nodeList = DependencyManager::get<NodeList>();
            nodeList->eachMatchingNode(
                [&](const SharedNodePointer& node)->bool {
                return (node->getType() == NodeType::AvatarMixer || node->getType() == NodeType::AudioMixer)
                        && node->getActiveSocket();
            },
                [&](const SharedNodePointer& node) {
                qDebug() << "sending KillAvatar message to Avatar and Audio Mixers";
                auto packet = NLPacket::create(PacketType::KillAvatar, NUM_BYTES_RFC4122_UUID + sizeof(KillAvatarReason), true);
                packet->write(getSessionUUID().toRfc4122());
                packet->writePrimitive(KillAvatarReason::NoReason);
                nodeList->sendPacket(std::move(packet), *node);
            });

            disconnect(_scriptManager.data(), &ScriptManager::update,
                       scriptableAvatar.data(), &ScriptableAvatar::update);

            QMetaObject::invokeMethod(&_avatarAudioTimer, "stop");
        }

        _entityEditSender.setMyAvatar(nullptr);
    }
}

void Agent::queryAvatars() {
    auto scriptedAvatar = DependencyManager::get<ScriptableAvatar>();

    ViewFrustum view;
    view.setPosition(scriptedAvatar->getWorldPosition());
    view.setOrientation(scriptedAvatar->getHeadOrientation());
    view.setProjection(DEFAULT_FIELD_OF_VIEW_DEGREES, DEFAULT_ASPECT_RATIO,
                       DEFAULT_NEAR_CLIP, DEFAULT_FAR_CLIP);
    view.calculate();
    ConicalViewFrustum conicalView { view };

    auto avatarPacket = NLPacket::create(PacketType::AvatarQuery);
    auto destinationBuffer = reinterpret_cast<unsigned char*>(avatarPacket->getPayload());
    auto bufferStart = destinationBuffer;

    uint8_t numFrustums = 1;
    memcpy(destinationBuffer, &numFrustums, sizeof(numFrustums));
    destinationBuffer += sizeof(numFrustums);

    destinationBuffer += conicalView.serialize(destinationBuffer);

    avatarPacket->setPayloadSize(destinationBuffer - bufferStart);

    DependencyManager::get<NodeList>()->broadcastToNodes(std::move(avatarPacket),
                                                         { NodeType::AvatarMixer });
}

void Agent::encodeFrameOfZeros(QByteArray& encodedZeros) {
    _flushEncoder = false;
    static const QByteArray zeros(AudioConstants::NETWORK_FRAME_BYTES_PER_CHANNEL, 0);
    if (_encoder) {
        _encoder->encode(zeros, encodedZeros);
    } else {
        encodedZeros = zeros;
    }
}

void Agent::computeLoudness(const QByteArray* decodedBuffer, QSharedPointer<ScriptableAvatar> scriptableAvatar) {
    float lastInputLoudness = 0.0f;
    if (decodedBuffer) {
        auto samples = reinterpret_cast<const int16_t*>(decodedBuffer->constData());
        int numSamples = decodedBuffer->size() / AudioConstants::SAMPLE_SIZE;

        assert(numSamples < 65536); // int32_t loudness cannot overflow
        if (numSamples > 0) {
            int32_t loudness = 0;
            for (int i = 0; i < numSamples; ++i) {
                loudness += std::abs((int32_t)samples[i]);
            }
            lastInputLoudness = (float)loudness / numSamples;
        }
    }
    scriptableAvatar->setAudioLoudness(lastInputLoudness);
}

void Agent::processAgentAvatarAudio() {
    auto recordingInterface = DependencyManager::get<RecordingScriptingInterface>();
    bool isPlayingRecording = recordingInterface->isPlaying();

    if (_isAvatar && ((_isListeningToAudioStream && !isPlayingRecording) || _avatarSound)) {
        // if we have an avatar audio stream then send it out to our audio-mixer
        auto scriptedAvatar = DependencyManager::get<ScriptableAvatar>();
        bool silentFrame = true;

        int16_t numAvailableSamples = AudioConstants::NETWORK_FRAME_SAMPLES_PER_CHANNEL;
        const int16_t* nextSoundOutput = NULL;

        if (_avatarSound && _avatarSound->isReady()) {
            if (isPlayingRecording && !_shouldMuteRecordingAudio) {
                _shouldMuteRecordingAudio = true;
            }
            
            auto audioData = _avatarSound->getAudioData();
            nextSoundOutput = reinterpret_cast<const int16_t*>(audioData->rawData()
                    + _numAvatarSoundSentBytes);

            int numAvailableBytes = (audioData->getNumBytes() - _numAvatarSoundSentBytes) > AudioConstants::NETWORK_FRAME_BYTES_PER_CHANNEL
                ? AudioConstants::NETWORK_FRAME_BYTES_PER_CHANNEL
                : audioData->getNumBytes() - _numAvatarSoundSentBytes;
            numAvailableSamples = (int16_t)numAvailableBytes / sizeof(int16_t);


            // check if the all of the _numAvatarAudioBufferSamples to be sent are silence
            for (int i = 0; i < numAvailableSamples; ++i) {
                if (nextSoundOutput[i] != 0) {
                    silentFrame = false;
                    break;
                }
            }

            _numAvatarSoundSentBytes += numAvailableBytes;
            if (_numAvatarSoundSentBytes == (int)audioData->getNumBytes()) {
                // we're done with this sound object - so set our pointer back to NULL
                // and our sent bytes back to zero
                _avatarSound.clear();
                _numAvatarSoundSentBytes = 0;
                _flushEncoder = true;

                if (_shouldMuteRecordingAudio) {
                    _shouldMuteRecordingAudio = false;
                }
            }
        }

        auto audioPacket = NLPacket::create(silentFrame && !_flushEncoder
                ? PacketType::SilentAudioFrame
                : PacketType::MicrophoneAudioNoEcho);

        // seek past the sequence number, will be packed when destination node is known
        audioPacket->seek(sizeof(quint16));

        if (silentFrame && !_flushEncoder) {

            if (!_isListeningToAudioStream) {
                // if we have a silent frame and we're not listening then just send nothing and break out of here
                return;
            }

            // write the codec
            audioPacket->writeString(_selectedCodecName);

            // write the number of silent samples so the audio-mixer can uphold timing
            audioPacket->writePrimitive(numAvailableSamples);

            // use the orientation and position of this avatar for the source of this audio
            audioPacket->writePrimitive(scriptedAvatar->getWorldPosition());
            glm::quat headOrientation = scriptedAvatar->getHeadOrientation();
            audioPacket->writePrimitive(headOrientation);
            audioPacket->writePrimitive(scriptedAvatar->getWorldPosition());
            audioPacket->writePrimitive(glm::vec3(0));

            // no matter what, the loudness should be set to 0
            computeLoudness(nullptr, scriptedAvatar);
        } else if (nextSoundOutput || _flushEncoder) {

            // write the codec
            audioPacket->writeString(_selectedCodecName);

            // assume scripted avatar audio is mono and set channel flag to zero
            audioPacket->writePrimitive((quint8)0);

            // use the orientation and position of this avatar for the source of this audio
            audioPacket->writePrimitive(scriptedAvatar->getWorldPosition());
            glm::quat headOrientation = scriptedAvatar->getHeadOrientation();
            audioPacket->writePrimitive(headOrientation);
            audioPacket->writePrimitive(scriptedAvatar->getWorldPosition()); // HUH? why do we write this twice??
            audioPacket->writePrimitive(glm::vec3(0));

            QByteArray encodedBuffer;
            if (_flushEncoder) {
                encodeFrameOfZeros(encodedBuffer);
                // loudness is 0
                computeLoudness(nullptr, scriptedAvatar);
            } else {
                QByteArray decodedBuffer(reinterpret_cast<const char*>(nextSoundOutput), numAvailableSamples*sizeof(int16_t));
                if (_encoder) {
                    // encode it
                    _encoder->encode(decodedBuffer, encodedBuffer);
                } else {
                    encodedBuffer = decodedBuffer;
                }
                computeLoudness(&decodedBuffer, scriptedAvatar);
            }
            audioPacket->write(encodedBuffer.constData(), encodedBuffer.size());
        }

        // we should never have both nextSoundOutput being null and silentFrame being false, but lets
        // assert on it in case things above change in a bad way
        assert(nextSoundOutput || silentFrame);

        // write audio packet to AudioMixer nodes
        auto nodeList = DependencyManager::get<NodeList>();
        nodeList->eachNode([this, &nodeList, &audioPacket](const SharedNodePointer& node) {
            // only send to nodes of type AudioMixer
            if (node->getType() == NodeType::AudioMixer) {
                // pack sequence number
                quint16 sequence = _outgoingScriptAudioSequenceNumbers[node->getUUID()]++;
                audioPacket->seek(0);
                audioPacket->writePrimitive(sequence);
                // send audio packet
                nodeList->sendUnreliablePacket(*audioPacket, *node);
            }
        });
    }
}

void Agent::aboutToFinish() {
    // our entity tree is going to go away so tell that to the EntityScriptingInterface
    DependencyManager::get<EntityScriptingInterface>()->setEntityTree(nullptr);

    DependencyManager::get<ResourceManager>()->cleanup();

    DependencyManager::destroy<ModelFormatRegistry>();
    DependencyManager::destroy<ModelCache>();

    DependencyManager::destroy<PluginManager>();

    // cleanup the AudioInjectorManager (and any still running injectors)
    DependencyManager::destroy<AudioInjectorManager>();

    // destroy all other created dependencies

    DependencyManager::destroy<SoundCacheScriptingInterface>();
    DependencyManager::destroy<AudioScriptingInterface>();
    DependencyManager::destroy<RecordingScriptingInterface>();
    DependencyManager::destroy<AnimationCacheScriptingInterface>();
    DependencyManager::destroy<EntityScriptingInterface>();
    DependencyManager::destroy<ResourceScriptingInterface>();
    DependencyManager::destroy<UserActivityLoggerScriptingInterface>();

    DependencyManager::destroy<ScriptCache>();
    DependencyManager::destroy<SoundCache>();
    DependencyManager::destroy<AnimationCache>();

    DependencyManager::destroy<recording::Deck>();
    DependencyManager::destroy<recording::Recorder>();
    DependencyManager::destroy<recording::ClipCache>();

    DependencyManager::destroy<AvatarHashMap>();
    DependencyManager::destroy<AssignmentParentFinder>();
    DependencyManager::destroy<MessagesClient>();
    DependencyManager::destroy<ResourceManager>();

    DependencyManager::destroy<ResourceCacheSharedItems>();

    // drop our shared pointer to the script engine, then ask ScriptEngines to shutdown scripting
    // this ensures that the ScriptEngine goes down before ScriptEngines
    _scriptManager.clear();

    {
        DependencyManager::get<ScriptEngines>()->shutdownScripting();
    }
    
    DependencyManager::destroy<ScriptEngines>();

    DependencyManager::destroy<AssignmentDynamicFactory>();

    DependencyManager::destroy<ScriptableAvatar>();

    // cleanup codec & encoder
    if (_codec && _encoder) {
        _codec->releaseEncoder(_encoder);
        _encoder = nullptr;
    }
}

void Agent::stop() {
    if (_scriptManager) {
        _scriptManager->stop();
    } else {
        setFinished(true);
    }
}
