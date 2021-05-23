//
//  Agent.h
//  assignment-client/src
//
//  Created by Stephen Birarda on 7/1/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Agent_h
#define hifi_Agent_h

#include <memory>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <QUuid>
#include <QtCore/QSharedPointer>

#include <EntityEditPacketSender.h>
#include <EntityTree.h>
#include <ScriptEngine.h>
#include <ThreadedAssignment.h>

#include <plugins/CodecPlugin.h>

#include <Sound.h>
#include "AudioGate.h"
#include "MixedAudioStream.h"
#include "entities/EntityTreeHeadlessViewer.h"
#include "avatars/ScriptableAvatar.h"

class ScriptEngine;
class ScriptManager;
using ScriptEnginePointer = QSharedPointer<ScriptEngine>;
using ScriptManagerPointer = QSharedPointer<ScriptManager>;

class Agent : public ThreadedAssignment {
    Q_OBJECT

    Q_PROPERTY(bool isAvatar READ isAvatar WRITE setIsAvatar)
    Q_PROPERTY(bool isPlayingAvatarSound READ isPlayingAvatarSound)
    Q_PROPERTY(bool isListeningToAudioStream READ isListeningToAudioStream WRITE setIsListeningToAudioStream)
    Q_PROPERTY(bool isNoiseGateEnabled READ isNoiseGateEnabled WRITE setIsNoiseGateEnabled)
    Q_PROPERTY(float lastReceivedAudioLoudness READ getLastReceivedAudioLoudness)
    Q_PROPERTY(QUuid sessionUUID READ getSessionUUID)

public:
    Agent(ReceivedMessage& message);

    bool isPlayingAvatarSound() const { return _avatarSound != NULL; }

    bool isListeningToAudioStream() const { return _isListeningToAudioStream; }
    void setIsListeningToAudioStream(bool isListeningToAudioStream);

    bool isNoiseGateEnabled() const { return _isNoiseGateEnabled; }
    void setIsNoiseGateEnabled(bool isNoiseGateEnabled);

    float getLastReceivedAudioLoudness() const { return _lastReceivedAudioLoudness; }
    QUuid getSessionUUID() const;

    virtual void aboutToFinish() override;

public slots:
    void run() override;

    void playAvatarSound(SharedSoundPointer avatarSound);

    void setIsAvatar(bool isAvatar);
    bool isAvatar() const { return _isAvatar; }

    Q_INVOKABLE virtual void stop() override;

private slots:
    void requestScript();
    void scriptRequestFinished();
    void executeScript();

    void handleAudioPacket(QSharedPointer<ReceivedMessage> message);
    void handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleSelectedAudioFormat(QSharedPointer<ReceivedMessage> message);

    void nodeActivated(SharedNodePointer activatedNode);
    void nodeKilled(SharedNodePointer killedNode);

    void processAgentAvatarAudio();

private:
    void negotiateAudioFormat();
    void selectAudioFormat(const QString& selectedCodecName);
    void encodeFrameOfZeros(QByteArray& encodedZeros);
    void computeLoudness(const QByteArray* decodedBuffer, QSharedPointer<ScriptableAvatar>);

    ScriptManagerPointer _scriptManager;
    EntityEditPacketSender _entityEditSender;
    EntityTreeHeadlessViewer _entityViewer;

    MixedAudioStream _receivedAudioStream;
    float _lastReceivedAudioLoudness;

    void setAvatarSound(SharedSoundPointer avatarSound) { _avatarSound = avatarSound; }

    void queryAvatars();

    QString _scriptContents;
    QTimer* _scriptRequestTimeout { nullptr };
    ResourceRequest* _pendingScriptRequest { nullptr };
    bool _isListeningToAudioStream = false;
    SharedSoundPointer _avatarSound;
    bool _shouldMuteRecordingAudio { false };
    int _numAvatarSoundSentBytes = 0;
    bool _isAvatar = false;
    QTimer* _avatarQueryTimer = nullptr;
    QHash<QUuid, quint16> _outgoingScriptAudioSequenceNumbers;

    AudioGate _audioGate;
    bool _audioGateOpen { true };
    bool _isNoiseGateEnabled { false };

    CodecPluginPointer _codec;
    QString _selectedCodecName;
    Encoder* _encoder { nullptr };
    QTimer _avatarAudioTimer;
    bool _flushEncoder { false };
};

#endif // hifi_Agent_h
