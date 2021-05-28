//
//  EntityServer.h
//  assignment-client/src/entities
//
//  Created by Brad Hefta-Gaub on 4/29/14
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityServer_h
#define hifi_EntityServer_h

#include "../octree/OctreeServer.h"

#include <memory>

#include <EntityItem.h>
#include <EntityTree.h>
#include <SimpleEntitySimulation.h>

#include "EntityServerConsts.h"

/// Handles assignments of type EntityServer - sending entities to various clients.

struct ViewerSendingStats {
    quint64 lastSent;
    quint64 lastEdited;
};

class EntityServer : public OctreeServer, public NewlyCreatedEntityHook {
    Q_OBJECT
public:
    EntityServer(ReceivedMessage& message);
    ~EntityServer();

    // Subclasses must implement these methods
    virtual std::unique_ptr<OctreeQueryNode> createOctreeQueryNode() override;
    virtual char getMyNodeType() const override { return NodeType::EntityServer; }
    virtual PacketType getMyQueryMessageType() const override { return PacketType::EntityQuery; }
    virtual const char* getMyServerName() const override { return MODEL_SERVER_NAME; }
    virtual const char* getMyLoggingServerTargetName() const override { return MODEL_SERVER_LOGGING_TARGET_NAME; }
    virtual const char* getMyDefaultPersistFilename() const override { return LOCAL_MODELS_PERSIST_FILE; }
    virtual PacketType getMyEditNackType() const override { return PacketType::EntityEditNack; }
    virtual QString getMyDomainSettingsKey() const override { return QString("entity_server_settings"); }

    // subclass may implement these methods
    virtual void beforeRun() override;
    virtual bool hasSpecialPacketsToSend(const SharedNodePointer& node) override;
    virtual int sendSpecialPackets(const SharedNodePointer& node, OctreeQueryNode* queryNode, int& packetsSent) override;

    virtual void entityCreated(const EntityItem& newEntity, const SharedNodePointer& senderNode) override;
    virtual void readAdditionalConfiguration(const QJsonObject& settingsSectionObject) override;
    virtual QString serverSubclassStats() override;

    virtual void trackSend(const QUuid& dataID, quint64 dataLastEdited, const QUuid& sessionID) override;
    virtual void trackViewerGone(const QUuid& sessionID) override;

    virtual void aboutToFinish() override;

public slots:
    virtual void nodeAdded(SharedNodePointer node) override;
    virtual void nodeKilled(SharedNodePointer node) override;
    void pruneDeletedEntities();
    void entityFilterAdded(EntityItemID id, bool success);

protected:
    virtual OctreePointer createTree() override;
    virtual UniqueSendThread newSendThread(const SharedNodePointer& node) override;

private slots:
    void handleEntityPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void domainSettingsRequestFailed();

private:
    SimpleEntitySimulationPointer _entitySimulation;
    QTimer* _pruneDeletedEntitiesTimer = nullptr;

    QReadWriteLock _viewerSendingStatsLock;
    QMap<QUuid, QMap<QUuid, ViewerSendingStats>> _viewerSendingStats;

    static const int DEFAULT_MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = 45 * 60 * 1000;                    // 45m
    static const int DEFAULT_MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = 60 * 60 * 1000;                    // 1h
    int _MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = DEFAULT_MINIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS;  // 45m
    int _MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS = DEFAULT_MAXIMUM_DYNAMIC_DOMAIN_VERIFICATION_TIMER_MS;  // 1h
    QTimer _dynamicDomainVerificationTimer;
    void startDynamicDomainVerification();
};

#endif  // hifi_EntityServer_h
