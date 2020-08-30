//
//  Assignment.cpp
//  libraries/networking/src
//
//  Created by Stephen Birarda on 8/22/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Assignment.h"

#include <QtCore/QDataStream>

#include <BuildInfo.h>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

#include "udt/PacketHeaders.h"
#include "SharedUtil.h"
#include "UUID.h"

Assignment::Type Assignment::typeForNodeType(NodeType_t nodeType) {
    switch (nodeType) {
        case NodeType::AudioMixer:
            return Assignment::AudioMixerType;
        case NodeType::AvatarMixer:
            return Assignment::AvatarMixerType;
        case NodeType::Agent:
            return Assignment::AgentType;
        case NodeType::EntityServer:
            return Assignment::EntityServerType;
        case NodeType::AssetServer:
            return Assignment::AssetServerType;
        case NodeType::MessagesMixer:
            return Assignment::MessagesMixerType;
        case NodeType::EntityScriptServer:
            return Assignment::EntityScriptServerType;
        default:
            return Assignment::AllTypes;
    }
}

Assignment::Assignment() :
    _uuid(),
    _command(Assignment::RequestCommand),
    _type(Assignment::AllTypes),
    _pool(),
    _location(Assignment::LocalLocation),
    _payload(),
    _isStatic(false)
{
    
}

Assignment::Assignment(Assignment::Command command, Assignment::Type type, const QString& pool, Assignment::Location location, QString dataDirectory) :
    _uuid(),
    _command(command),
    _type(type),
    _pool(pool),
    _location(location),
    _payload(),
    _isStatic(false),
    _nodeVersion(),
    _dataDirectory(dataDirectory)
{
    if (_command == Assignment::CreateCommand) {
        // this is a newly created assignment, generate a random UUID
        _uuid = QUuid::createUuid();
    } else if (_command == Assignment::RequestCommand) {
        _nodeVersion = BuildInfo::VERSION;
    }
}

Assignment::Assignment(ReceivedMessage& message) :
    _pool(),
    _location(GlobalLocation),
    _payload(),
    _nodeVersion()
{
    if (message.getType() == PacketType::RequestAssignment) {
        _command = Assignment::RequestCommand;
    } else if (message.getType() == PacketType::CreateAssignment) {
        _command = Assignment::CreateCommand;
    }
    
    QDataStream packetStream(message.getMessage());
    
    packetStream >> *this;
}

#ifdef WIN32
#pragma warning(default:4351)
#endif


Assignment::Assignment(const Assignment& otherAssignment) : QObject() {
    _uuid = otherAssignment._uuid;
    _command = otherAssignment._command;
    _type = otherAssignment._type;
    _location = otherAssignment._location;
    _pool = otherAssignment._pool;
    _payload = otherAssignment._payload;
    _nodeVersion = otherAssignment._nodeVersion;
}

Assignment& Assignment::operator=(const Assignment& rhsAssignment) {
    Assignment temp(rhsAssignment);
    swap(temp);
    return *this;
}

void Assignment::swap(Assignment& otherAssignment) {
    using std::swap;
    
    swap(_uuid, otherAssignment._uuid);
    swap(_command, otherAssignment._command);
    swap(_type, otherAssignment._type);
    swap(_location, otherAssignment._location);
    swap(_pool, otherAssignment._pool);
    swap(_payload, otherAssignment._payload);
    swap(_nodeVersion, otherAssignment._nodeVersion);
}

const char* Assignment::getTypeName() const {
    return typeToString(_type);
}

const char* Assignment::typeToString(Assignment::Type type) {
    switch (type) {
        case Assignment::AudioMixerType:
            return "audio-mixer";
        case Assignment::AvatarMixerType:
            return "avatar-mixer";
        case Assignment::AgentType:
            return "agent";
        case Assignment::AssetServerType:
            return "asset-server";
        case Assignment::EntityServerType:
            return "entity-server";
        case Assignment::MessagesMixerType:
            return "messages-mixer";
        case Assignment::EntityScriptServerType:
            return "entity-script-server";
        default:
            return "unknown";
    }
}

QDebug operator<<(QDebug debug, const Assignment &assignment) {
    debug.nospace() << "UUID: " << qPrintable(assignment.getUUID().toString()) <<
        ", Type: " << assignment.getTypeName() << " (" << assignment.getType() << ")";
    
    if (!assignment.getPool().isEmpty()) {
        debug << ", Pool: " << assignment.getPool();
    }

    return debug.space();
}

QDataStream& operator<<(QDataStream &out, const Assignment& assignment) {
    out << (quint8) assignment._type << assignment._uuid << assignment._pool << assignment._payload;
    
    if (assignment._command == Assignment::RequestCommand) {
        out << assignment._nodeVersion;
    }
    
    return out;
}

QDataStream& operator>>(QDataStream &in, Assignment& assignment) {
    quint8 packedType;
    in >> packedType >> assignment._uuid >> assignment._pool >> assignment._payload;
    assignment._type = (Assignment::Type) packedType;
    
    if (assignment._command == Assignment::RequestCommand) {
        in >> assignment._nodeVersion;
    }
    
    return in;
}


uint qHash(const Assignment::Type& key, uint seed) {
    // seems odd that Qt couldn't figure out this cast itself, but this fixes a compile error after switch to
    // strongly typed enum for PacketType
    return qHash((uint8_t) key, seed);
}
