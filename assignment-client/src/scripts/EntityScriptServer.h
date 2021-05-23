//
//  EntityScriptServer.h
//  assignment-client/src/scripts
//
//  Created by Clément Brisset on 1/5/17.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityScriptServer_h
#define hifi_EntityScriptServer_h

#include <set>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtCore/QSharedPointer>

#include <EntityEditPacketSender.h>
#include <plugins/CodecPlugin.h>
#include <SimpleEntitySimulation.h>
#include <ThreadedAssignment.h>
#include <ScriptManager.h>

#include "../entities/EntityTreeHeadlessViewer.h"

class EntityScriptServer : public ThreadedAssignment {
    Q_OBJECT

public:
    EntityScriptServer(ReceivedMessage& message);
    ~EntityScriptServer();

    virtual void aboutToFinish() override;

public slots:
    void run() override;
    void nodeActivated(SharedNodePointer activatedNode);
    void nodeKilled(SharedNodePointer killedNode);
    void sendStatsPacket() override;

private slots:
    void handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleSelectedAudioFormat(QSharedPointer<ReceivedMessage> message);

    void handleReloadEntityServerScriptPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleEntityScriptGetStatusPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);

    void handleSettings();
    void updateEntityPPS();

    void handleEntityServerScriptLogPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);

    void pushLogs();

    void handleEntityScriptCallMethodPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);


private:
    void negotiateAudioFormat();
    void selectAudioFormat(const QString& selectedCodecName);

    void resetEntitiesScriptEngine();
    void clear();
    void shutdownScriptEngine();

    void addingEntity(const EntityItemID& entityID);
    void deletingEntity(const EntityItemID& entityID);
    void entityServerScriptChanging(const EntityItemID& entityID, bool reload);
    void checkAndCallPreload(const EntityItemID& entityID, bool forceRedownload = false);

    void cleanupOldKilledListeners();

    bool _shuttingDown { false };

    static int _entitiesScriptEngineCount;
    ScriptManagerPointer _entitiesScriptManager;
    SimpleEntitySimulationPointer _entitySimulation;
    EntityEditPacketSender _entityEditSender;
    EntityTreeHeadlessViewer _entityViewer;

    int _maxEntityPPS { DEFAULT_MAX_ENTITY_PPS };
    int _entityPPSPerScript { DEFAULT_ENTITY_PPS_PER_SCRIPT };

    std::set<QUuid> _logListeners;
    std::vector<std::pair<QUuid, quint64>> _killedListeners;

    QString _selectedCodecName;
    CodecPluginPointer _codec;
    Encoder* _encoder { nullptr };
};

#endif // hifi_EntityScriptServer_h
