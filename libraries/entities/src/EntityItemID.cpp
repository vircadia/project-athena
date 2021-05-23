//
//  EntityItemID.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityItemID.h"
#include <QtCore/QObject>
#include <QDebug>

#include <BufferParser.h>
#include <udt/PacketHeaders.h>
#include <UUID.h>

#include "RegisteredMetaTypes.h"
#include <ScriptValue.h>
#include <ScriptValueUtils.h>

int entityItemIDTypeID = qRegisterMetaType<EntityItemID>();

EntityItemID::EntityItemID() : QUuid()
{
}


EntityItemID::EntityItemID(const QUuid& id) : QUuid(id)
{
}

// EntityItemID::EntityItemID(const EntityItemID& other) : QUuid(other)
// {
// }

EntityItemID EntityItemID::readEntityItemIDFromBuffer(const unsigned char* data, int bytesLeftToRead) {
    EntityItemID result;
    if (bytesLeftToRead >= NUM_BYTES_RFC4122_UUID) {
        BufferParser(data, bytesLeftToRead).readUuid(result);
    }
    return result;
}

ScriptValuePointer EntityItemID::toScriptValue(ScriptEngine* engine) const { 
    return EntityItemIDtoScriptValue(engine, *this); 
}

ScriptValuePointer EntityItemIDtoScriptValue(ScriptEngine* engine, const EntityItemID& id) {
    return quuidToScriptValue(engine, id);
}

void EntityItemIDfromScriptValue(const ScriptValuePointer &object, EntityItemID& id) {
    quuidFromScriptValue(object, id);
}

QVector<EntityItemID> qVectorEntityItemIDFromScriptValue(const ScriptValuePointer& array) {
    if (!array->isArray()) {
        return QVector<EntityItemID>();
    }
    QVector<EntityItemID> newVector;
    int length = array->property("length")->toInteger();
    newVector.reserve(length);
    for (int i = 0; i < length; i++) {
        QString uuidAsString = array->property(i)->toString();
        EntityItemID fromString(uuidAsString);
        newVector << fromString;
    }
    return newVector;
}

size_t std::hash<EntityItemID>::operator()(const EntityItemID& id) const { return qHash(id); }
