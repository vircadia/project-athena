//
//  OctreeInboundPacketProcessor.cpp
//  assignment-client/src/octree
//
//  Created by Brad Hefta-Gaub on 8/21/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "OctreeInboundPacketProcessor.h"

#include <limits>

#include <NumericalConstants.h>
#include <udt/PacketHeaders.h>
#include <PerfStat.h>

#include "OctreeServer.h"
#include "OctreeServerConsts.h"

static QUuid DEFAULT_NODE_ID_REF;
const quint64 TOO_LONG_SINCE_LAST_NACK = 1 * USECS_PER_SECOND;

OctreeInboundPacketProcessor::OctreeInboundPacketProcessor(OctreeServer* myServer) :
    _myServer(myServer),
    _receivedPacketCount(0),
    _totalTransitTime(0),
    _totalProcessTime(0),
    _totalLockWaitTime(0),
    _totalElementsInPacket(0),
    _totalPackets(0),
    _lastNackTime(usecTimestampNow()),
    _shuttingDown(false)
{
}

void OctreeInboundPacketProcessor::resetStats() {
    _totalTransitTime = 0;
    _totalProcessTime = 0;
    _totalLockWaitTime = 0;
    _totalElementsInPacket = 0;
    _totalPackets = 0;
    _lastNackTime = usecTimestampNow();

    QWriteLocker locker(&_senderStatsLock);
    _singleSenderStats.clear();
}

uint32_t OctreeInboundPacketProcessor::getMaxWait() const {
    // calculate time until next sendNackPackets()
    quint64 nextNackTime = _lastNackTime + TOO_LONG_SINCE_LAST_NACK;
    quint64 now = usecTimestampNow();
    if (now >= nextNackTime) {
        return 0;
    }
    return (nextNackTime - now) / USECS_PER_MSEC + 1;
}

void OctreeInboundPacketProcessor::preProcess() {
    // check if it's time to send a nack. If yes, do so
    quint64 now = usecTimestampNow();
    if (now - _lastNackTime >= TOO_LONG_SINCE_LAST_NACK) {
        _lastNackTime = now;
        sendNackPackets();
    }
}

void OctreeInboundPacketProcessor::midProcess() {
    // check if it's time to send a nack. If yes, do so
    quint64 now = usecTimestampNow();
    if (now - _lastNackTime >= TOO_LONG_SINCE_LAST_NACK) {
        _lastNackTime = now;
        sendNackPackets();
    }
}

void OctreeInboundPacketProcessor::processPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    if (_shuttingDown) {
        qDebug() << "OctreeInboundPacketProcessor::processPacket() while shutting down... ignoring incoming packet";
        return;
    }

    bool debugProcessPacket = _myServer->wantsVerboseDebug();

    if (debugProcessPacket) {
        qDebug("OctreeInboundPacketProcessor::processPacket() payload=%p payloadLength=%lld",
               message->getRawMessage(),
               message->getSize());
    }

    // Ask our tree subclass if it can handle the incoming packet...
    PacketType packetType = message->getType();
    
    if (_myServer->getOctree()->handlesEditPacketType(packetType)) {
        PerformanceWarning warn(debugProcessPacket, "processPacket KNOWN TYPE", debugProcessPacket);
        _receivedPacketCount++;

        unsigned short int sequence;
        message->readPrimitive(&sequence);

        quint64 sentAt;
        message->readPrimitive(&sentAt);
        
        quint64 arrivedAt = usecTimestampNow();
        if (sentAt > arrivedAt) {
            if (debugProcessPacket || _myServer->wantsDebugReceiving()) {
                qDebug() << "unreasonable sentAt=" << sentAt << " usecs";
                qDebug() << "setting sentAt to arrivedAt=" << arrivedAt << " usecs";
            }
            sentAt = arrivedAt;
        }

        quint64 transitTime = arrivedAt - sentAt;
        int editsInPacket = 0;
        quint64 processTime = 0;
        quint64 lockWaitTime = 0;

        if (debugProcessPacket || _myServer->wantsDebugReceiving()) {
            qDebug() << "PROCESSING THREAD: got '" << packetType << "' packet - " << _receivedPacketCount << " command from client";
            qDebug() << "    receivedBytes=" << message->getSize();
            qDebug() << "         sequence=" << sequence;
            qDebug() << "           sentAt=" << sentAt << " usecs";
            qDebug() << "        arrivedAt=" << arrivedAt << " usecs";
            qDebug() << "      transitTime=" << transitTime << " usecs";
            qDebug() << "      sendingNode->getClockSkewUsec()=" << sendingNode->getClockSkewUsec() << " usecs";


        }

        if (debugProcessPacket) {
            qDebug() << "    numBytesPacketHeader=" << NLPacket::totalHeaderSize(packetType);
            qDebug() << "    sizeof(sequence)=" << sizeof(sequence);
            qDebug() << "    sizeof(sentAt)=" << sizeof(sentAt);
            qDebug() << "    atByte (in payload)=" << message->getPosition();
            qDebug() << "    payload size=" << message->getSize();

            if (!message->getBytesLeftToRead()) {
                qDebug() << "    ----- UNEXPECTED ---- got a packet without any edit details!!!! --------";
            }
        }
        
        const unsigned char* editData = nullptr;
        
        while (message->getBytesLeftToRead() > 0) {

            editData = reinterpret_cast<const unsigned char*>(message->getRawMessage() + message->getPosition());

            int maxSize = message->getBytesLeftToRead();

            if (debugProcessPacket) {
                qDebug() << " --- inside while loop ---";
                qDebug() << "    maxSize=" << maxSize;
                qDebug("OctreeInboundPacketProcessor::processPacket() %hhu "
                       "payload=%p payloadLength=%lld editData=%p payloadPosition=%lld maxSize=%d",
                       (unsigned char)packetType, message->getRawMessage(), message->getSize(), editData,
                        message->getPosition(), maxSize);
            }

            quint64 startProcess, startLock = usecTimestampNow();
            int editDataBytesRead;
            _myServer->getOctree()->withWriteLock([&] {
                startProcess = usecTimestampNow();
                editDataBytesRead =
                    _myServer->getOctree()->processEditPacketData(*message, editData, maxSize, sendingNode);
            });
            quint64 endProcess = usecTimestampNow();

            if (debugProcessPacket) {
                qDebug() << "OctreeInboundPacketProcessor::processPacket() after processEditPacketData()..."
                    << "editDataBytesRead=" << editDataBytesRead;
            }

            editsInPacket++;
            quint64 thisProcessTime = endProcess - startProcess;
            quint64 thisLockWaitTime = startProcess - startLock;
            processTime += thisProcessTime;
            lockWaitTime += thisLockWaitTime;

            // skip to next edit record in the packet
            message->seek(message->getPosition() + editDataBytesRead);

            if (debugProcessPacket) {
                qDebug() << "    editDataBytesRead=" << editDataBytesRead;
                qDebug() << "    AFTER processEditPacketData payload position=" << message->getPosition();
                qDebug() << "    AFTER processEditPacketData payload size=" << message->getSize();
            }

        }

        if (debugProcessPacket) {
            qDebug("OctreeInboundPacketProcessor::processPacket() DONE LOOPING FOR %hhu "
                   "payload=%p payloadLength=%lld editData=%p payloadPosition=%lld",
                   (unsigned char)packetType, message->getRawMessage(), message->getSize(), editData, message->getPosition());
        }

        // Make sure our Node and NodeList knows we've heard from this node.
        QUuid& nodeUUID = DEFAULT_NODE_ID_REF;
        if (sendingNode) {
            nodeUUID = sendingNode->getUUID();
            if (debugProcessPacket) {
                qDebug() << "sender has uuid=" << nodeUUID;
            }
        } else {
            if (debugProcessPacket) {
                qDebug() << "sender has no known nodeUUID.";
            }
        }
        trackInboundPacket(nodeUUID, sequence, transitTime, editsInPacket, processTime, lockWaitTime);
    } else {
        qDebug("unknown packet ignored... packetType=%hhu", (unsigned char)packetType);
    }
}

void OctreeInboundPacketProcessor::trackInboundPacket(const QUuid& nodeUUID, unsigned short int sequence, quint64 transitTime,
            int editsInPacket, quint64 processTime, quint64 lockWaitTime) {

    _totalTransitTime += transitTime;
    _totalProcessTime += processTime;
    _totalLockWaitTime += lockWaitTime;
    _totalElementsInPacket += editsInPacket;
    _totalPackets++;

    QWriteLocker locker(&_senderStatsLock);

    // find the individual senders stats and track them there too...
    // see if this is the first we've heard of this node...
    if (_singleSenderStats.find(nodeUUID) == _singleSenderStats.end()) {
        SingleSenderStats stats;
        stats.trackInboundPacket(sequence, transitTime, editsInPacket, processTime, lockWaitTime);
        _singleSenderStats[nodeUUID] = stats;
    } else {
        SingleSenderStats& stats = _singleSenderStats[nodeUUID];
        stats.trackInboundPacket(sequence, transitTime, editsInPacket, processTime, lockWaitTime);
    }
}

int OctreeInboundPacketProcessor::sendNackPackets() {
    if (_shuttingDown) {
        qDebug() << "OctreeInboundPacketProcessor::sendNackPackets() while shutting down... ignore";
        return 0;
    }

    auto nodeList = DependencyManager::get<NodeList>();
    int packetsSent = 0;
    int totalBytesSent = 0;

    QWriteLocker locker(&_senderStatsLock);

    NodeToSenderStatsMapIterator i = _singleSenderStats.begin();
    while (i != _singleSenderStats.end()) {

        QUuid nodeUUID = i.key();
        SingleSenderStats nodeStats = i.value();
        
        // check if this node is still alive.  Remove its stats if it's dead.
        if (!isAlive(nodeUUID)) {
            i = _singleSenderStats.erase(i);
            continue;
        }

        // if there are packets from _node that are waiting to be processed,
        // don't send a NACK since the missing packets may be among those waiting packets.
        if (hasPacketsToProcessFrom(nodeUUID)) {
            ++i;
            continue;
        }

        const SharedNodePointer& destinationNode = DependencyManager::get<NodeList>()->nodeWithUUID(nodeUUID);
        // if the node no longer exists, remove its stats
        if (!destinationNode) {
            i = _singleSenderStats.erase(i);
            continue;
        }

        // retrieve sequence number stats of node, prune its missing set
        SequenceNumberStats& sequenceNumberStats = nodeStats.getIncomingEditSequenceNumberStats();
        sequenceNumberStats.pruneMissingSet();

        // construct nack packet(s) for this node
        const QSet<unsigned short int>& missingSequenceNumbers = sequenceNumberStats.getMissingSet();

        auto it = missingSequenceNumbers.constBegin();

        if (it != missingSequenceNumbers.constEnd()) {
            auto nackPacketList = NLPacketList::create(_myServer->getMyEditNackType());

            while (it != missingSequenceNumbers.constEnd()) {
                unsigned short int sequenceNumber = *it;
                nackPacketList->writePrimitive(sequenceNumber);
                ++it;
            }

            qDebug() << "NACK Sent back to editor/client... destinationNode=" << nodeUUID;

            packetsSent += (int)nackPacketList->getNumPackets();

            // send the list of nack packets
            totalBytesSent += nodeList->sendPacketList(std::move(nackPacketList), *destinationNode);
        }
        
        ++i;
    }

    OctreeSendThread::_totalPackets += packetsSent;
    OctreeSendThread::_totalBytes += totalBytesSent;

    return packetsSent;
}


SingleSenderStats::SingleSenderStats()
    : _totalTransitTime(0),
    _totalProcessTime(0),
    _totalLockWaitTime(0),
    _totalElementsInPacket(0),
    _totalPackets(0),
    _incomingEditSequenceNumberStats()
{

}

void SingleSenderStats::trackInboundPacket(unsigned short int incomingSequence, quint64 transitTime,
    int editsInPacket, quint64 processTime, quint64 lockWaitTime) {

    // track sequence number
    _incomingEditSequenceNumberStats.sequenceNumberReceived(incomingSequence);

    // update other stats
    _totalTransitTime += transitTime;
    _totalProcessTime += processTime;
    _totalLockWaitTime += lockWaitTime;
    _totalElementsInPacket += editsInPacket;
    _totalPackets++;
}
