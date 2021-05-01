//
//  EntityEditPacketSender.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 8/12/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityEditPacketSender_h
#define hifi_EntityEditPacketSender_h

#include <OctreeEditPacketSender.h>

#include <mutex>

#include "EntityItem.h"
#include "AvatarData.h"

/// Utility for processing, packing, queueing and sending of outbound edit voxel messages.
class EntityEditPacketSender :  public OctreeEditPacketSender {
    Q_OBJECT
public:
    EntityEditPacketSender();

    void setMyAvatar(AvatarData* myAvatar) { _myAvatar = myAvatar; }
    AvatarData* getMyAvatar() { return _myAvatar; }

    /// Queues an array of several voxel edit messages. Will potentially send a pending multi-command packet. Determines
    /// which voxel-server node or nodes the packet should be sent to. Can be called even before voxel servers are known, in
    /// which case up to MaxPendingMessages will be buffered and processed when voxel servers are known.
    /// NOTE: EntityItemProperties assumes that all distances are in meter units
    void queueEditEntityMessage(PacketType type, EntityTreePointer entityTree,
                                EntityItemID entityItemID, const EntityItemProperties& properties);


    void queueEraseEntityMessage(const EntityItemID& entityItemID);
    void queueCloneEntityMessage(const EntityItemID& entityIDToClone, const EntityItemID& newEntityID);

    // My server type is the model server
    virtual char getMyNodeType() const override { return NodeType::EntityServer; }
    virtual void adjustEditPacketForClockSkew(PacketType type, QByteArray& buffer, qint64 clockSkew) override;

public slots:
    void processEntityEditNackPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode);

private:
    friend class MyAvatar;
    void queueEditAvatarEntityMessage(EntityTreePointer entityTree, EntityItemID entityItemID);

private:
    std::mutex _mutex;
    AvatarData* _myAvatar { nullptr };
};
#endif // hifi_EntityEditPacketSender_h
