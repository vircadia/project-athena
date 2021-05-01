//
//  EntityItem.cpp
//  libraries/models/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityItem.h"

#include <QtCore/QObject>
#include <QtEndian>
#include <QJsonDocument>
#include <NetworkingConstants.h>
#include <MetaverseAPI.h>
#include <NetworkAccessManager.h>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/component_wise.hpp>

#include <BufferParser.h>
#include <ByteCountCoding.h>
#include <GLMHelpers.h>
#include <Octree.h>
#include <PhysicsHelpers.h>
#include <Profile.h>
#include <RegisteredMetaTypes.h>
#include <SharedUtil.h> // usecTimestampNow()
#include <LogHandler.h>
#include <Extents.h>
#include <QVariantGLM.h>
#include <Grab.h>

#include "EntityScriptingInterface.h"
#include "EntitiesLogging.h"
#include "EntityTree.h"
#include "EntitySimulation.h"
#include "EntityDynamicFactoryInterface.h"

//#define WANT_DEBUG

Q_DECLARE_METATYPE(EntityItemPointer);
int entityItemPointernMetaTypeId = qRegisterMetaType<EntityItemPointer>();

int EntityItem::_maxActionsDataSize = 800;
quint64 EntityItem::_rememberDeletedActionTime = 20 * USECS_PER_SECOND;

EntityItem::EntityItem(const EntityItemID& entityItemID) :
    SpatiallyNestable(NestableType::Entity, entityItemID)
{
    setLocalVelocity(ENTITY_ITEM_DEFAULT_VELOCITY);
    setLocalAngularVelocity(ENTITY_ITEM_DEFAULT_ANGULAR_VELOCITY);
    setUnscaledDimensions(ENTITY_ITEM_DEFAULT_DIMENSIONS);
    // explicitly set transform parts to set dirty flags used by batch rendering
    locationChanged();
    dimensionsChanged();
    quint64 now = usecTimestampNow();
    _lastSimulated = now;
    _lastUpdated = now;
}

EntityItem::~EntityItem() {
    // these pointers MUST be correct at delete, else we probably have a dangling backpointer
    // to this EntityItem in the corresponding data structure.
    assert(!_simulated || (!_element && !_physicsInfo));
    assert(!_element);
    assert(!_physicsInfo);
}

EntityPropertyFlags EntityItem::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    // Core
    requestedProperties += PROP_SIMULATION_OWNER;
    requestedProperties += PROP_PARENT_ID;
    requestedProperties += PROP_PARENT_JOINT_INDEX;
    requestedProperties += PROP_VISIBLE;
    requestedProperties += PROP_NAME;
    requestedProperties += PROP_LOCKED;
    requestedProperties += PROP_USER_DATA;
    requestedProperties += PROP_PRIVATE_USER_DATA;
    requestedProperties += PROP_HREF;
    requestedProperties += PROP_DESCRIPTION;
    requestedProperties += PROP_POSITION;
    requestedProperties += PROP_DIMENSIONS;
    requestedProperties += PROP_ROTATION;
    requestedProperties += PROP_REGISTRATION_POINT;
    requestedProperties += PROP_CREATED;
    requestedProperties += PROP_LAST_EDITED_BY;
    requestedProperties += PROP_ENTITY_HOST_TYPE;
    requestedProperties += PROP_OWNING_AVATAR_ID;
    requestedProperties += PROP_PARENT_ID;
    requestedProperties += PROP_PARENT_JOINT_INDEX;
    requestedProperties += PROP_QUERY_AA_CUBE;
    requestedProperties += PROP_CAN_CAST_SHADOW;
    requestedProperties += PROP_VISIBLE_IN_SECONDARY_CAMERA;
    requestedProperties += PROP_RENDER_LAYER;
    requestedProperties += PROP_PRIMITIVE_MODE;
    requestedProperties += PROP_IGNORE_PICK_INTERSECTION;
    requestedProperties += PROP_RENDER_WITH_ZONES;
    requestedProperties += PROP_BILLBOARD_MODE;
    requestedProperties += _grabProperties.getEntityProperties(params);

    // Physics
    requestedProperties += PROP_DENSITY;
    requestedProperties += PROP_VELOCITY;
    requestedProperties += PROP_ANGULAR_VELOCITY;
    requestedProperties += PROP_GRAVITY;
    requestedProperties += PROP_ACCELERATION;
    requestedProperties += PROP_DAMPING;
    requestedProperties += PROP_ANGULAR_DAMPING;
    requestedProperties += PROP_RESTITUTION;
    requestedProperties += PROP_FRICTION;
    requestedProperties += PROP_LIFETIME;
    requestedProperties += PROP_COLLISIONLESS;
    requestedProperties += PROP_COLLISION_MASK;
    requestedProperties += PROP_DYNAMIC;
    requestedProperties += PROP_COLLISION_SOUND_URL;
    requestedProperties += PROP_ACTION_DATA;

    // Cloning
    requestedProperties += PROP_CLONEABLE;
    requestedProperties += PROP_CLONE_LIFETIME;
    requestedProperties += PROP_CLONE_LIMIT;
    requestedProperties += PROP_CLONE_DYNAMIC;
    requestedProperties += PROP_CLONE_AVATAR_ENTITY;
    requestedProperties += PROP_CLONE_ORIGIN_ID;

    // Scripts
    requestedProperties += PROP_SCRIPT;
    requestedProperties += PROP_SCRIPT_TIMESTAMP;
    requestedProperties += PROP_SERVER_SCRIPTS;

    // Certifiable properties
    requestedProperties += PROP_ITEM_NAME;
    requestedProperties += PROP_ITEM_DESCRIPTION;
    requestedProperties += PROP_ITEM_CATEGORIES;
    requestedProperties += PROP_ITEM_ARTIST;
    requestedProperties += PROP_ITEM_LICENSE;
    requestedProperties += PROP_LIMITED_RUN;
    requestedProperties += PROP_MARKETPLACE_ID;
    requestedProperties += PROP_EDITION_NUMBER;
    requestedProperties += PROP_ENTITY_INSTANCE_NUMBER;
    requestedProperties += PROP_CERTIFICATE_ID;
    requestedProperties += PROP_CERTIFICATE_TYPE;
    requestedProperties += PROP_STATIC_CERTIFICATE_VERSION;

    return requestedProperties;
}

OctreeElement::AppendState EntityItem::appendEntityData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                            EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                            const bool destinationNodeCanGetAndSetPrivateUserData) const {

    // ALL this fits...
    //    object ID [16 bytes]
    //    ByteCountCoded(type code) [~1 byte]
    //    last edited [8 bytes]
    //    ByteCountCoded(last_edited to last_updated delta) [~1-8 bytes]
    //    PropertyFlags<>( everything ) [1-2 bytes]
    // ~27-35 bytes...

    OctreeElement::AppendState appendState = OctreeElement::COMPLETED; // assume the best

    // encode our ID as a byte count coded byte stream
    QByteArray encodedID = getID().toRfc4122();

    // encode our type as a byte count coded byte stream
    ByteCountCoded<quint32> typeCoder = getType();
    QByteArray encodedType = typeCoder;

    // last updated (animations, non-physics changes)
    quint64 updateDelta = getLastUpdated() <= getLastEdited() ? 0 : getLastUpdated() - getLastEdited();
    ByteCountCoded<quint64> updateDeltaCoder = updateDelta;
    QByteArray encodedUpdateDelta = updateDeltaCoder;

    // last simulated (velocity, angular velocity, physics changes)
    quint64 simulatedDelta = getLastSimulated() <= getLastEdited() ? 0 : getLastSimulated() - getLastEdited();
    ByteCountCoded<quint64> simulatedDeltaCoder = simulatedDelta;
    QByteArray encodedSimulatedDelta = simulatedDeltaCoder;


    EntityPropertyFlags propertyFlags(PROP_LAST_ITEM);
    EntityPropertyFlags requestedProperties = getEntityProperties(params);

    // these properties are not sent over the wire
    requestedProperties -= PROP_ENTITY_HOST_TYPE;
    requestedProperties -= PROP_OWNING_AVATAR_ID;
    requestedProperties -= PROP_VISIBLE_IN_SECONDARY_CAMERA;

    // If we are being called for a subsequent pass at appendEntityData() that failed to completely encode this item,
    // then our entityTreeElementExtraEncodeData should include data about which properties we need to append.
    if (entityTreeElementExtraEncodeData && entityTreeElementExtraEncodeData->entities.contains(getEntityItemID())) {
        requestedProperties = entityTreeElementExtraEncodeData->entities.value(getEntityItemID());
    }

    QString privateUserData = "";
    if (destinationNodeCanGetAndSetPrivateUserData) {
        privateUserData = getPrivateUserData();
    }

    EntityPropertyFlags propertiesDidntFit = requestedProperties;

    LevelDetails entityLevel = packetData->startLevel();

    quint64 lastEdited = getLastEdited();

    #ifdef WANT_DEBUG
        float editedAgo = getEditedAgo();
        QString agoAsString = formatSecondsElapsed(editedAgo);
        qCDebug(entities) << "Writing entity " << getEntityItemID() << " to buffer, lastEdited =" << lastEdited
                        << " ago=" << editedAgo << "seconds - " << agoAsString;
    #endif

    bool successIDFits = false;
    bool successTypeFits = false;
    bool successCreatedFits = false;
    bool successLastEditedFits = false;
    bool successLastUpdatedFits = false;
    bool successLastSimulatedFits = false;
    bool successPropertyFlagsFits = false;
    int propertyFlagsOffset = 0;
    int oldPropertyFlagsLength = 0;
    QByteArray encodedPropertyFlags;
    int propertyCount = 0;

    successIDFits = packetData->appendRawData(encodedID);
    if (successIDFits) {
        successTypeFits = packetData->appendRawData(encodedType);
    }
    if (successTypeFits) {
        successCreatedFits = packetData->appendValue(_created);
    }
    if (successCreatedFits) {
        successLastEditedFits = packetData->appendValue(lastEdited);
    }
    if (successLastEditedFits) {
        successLastUpdatedFits = packetData->appendRawData(encodedUpdateDelta);
    }
    if (successLastUpdatedFits) {
        successLastSimulatedFits = packetData->appendRawData(encodedSimulatedDelta);
    }

    if (successLastSimulatedFits) {
        propertyFlagsOffset = packetData->getUncompressedByteOffset();
        encodedPropertyFlags = propertyFlags;
        oldPropertyFlagsLength = encodedPropertyFlags.length();
        successPropertyFlagsFits = packetData->appendRawData(encodedPropertyFlags);
    }

    bool headerFits = successIDFits && successTypeFits && successCreatedFits && successLastEditedFits
                              && successLastUpdatedFits && successPropertyFlagsFits;

    int startOfEntityItemData = packetData->getUncompressedByteOffset();

    if (headerFits) {
        bool successPropertyFits;

        propertyFlags -= PROP_LAST_ITEM; // clear the last item for now, we may or may not set it as the actual item

        // NOTE: When we enable partial packing of entity properties, we'll want to pack simulationOwner, transform, and velocity properties near each other
        // since they will commonly be transmitted together.  simulationOwner must always go first, to avoid race conditions of simulation ownership bids
        // These items would go here once supported....
        //      PROP_PAGED_PROPERTY,
        //      PROP_CUSTOM_PROPERTIES_INCLUDED,

        APPEND_ENTITY_PROPERTY(PROP_SIMULATION_OWNER, _simulationOwner.toByteArray());
        // convert AVATAR_SELF_ID to actual sessionUUID.
        QUuid actualParentID = getParentID();
        auto nodeList = DependencyManager::get<NodeList>();
        if (actualParentID == AVATAR_SELF_ID) {
            actualParentID = nodeList->getSessionUUID();
        }
        APPEND_ENTITY_PROPERTY(PROP_PARENT_ID, actualParentID);
        APPEND_ENTITY_PROPERTY(PROP_PARENT_JOINT_INDEX, getParentJointIndex());
        APPEND_ENTITY_PROPERTY(PROP_VISIBLE, getVisible());
        APPEND_ENTITY_PROPERTY(PROP_NAME, getName());
        APPEND_ENTITY_PROPERTY(PROP_LOCKED, getLocked());
        APPEND_ENTITY_PROPERTY(PROP_USER_DATA, getUserData());
        APPEND_ENTITY_PROPERTY(PROP_PRIVATE_USER_DATA, privateUserData);
        APPEND_ENTITY_PROPERTY(PROP_HREF, getHref());
        APPEND_ENTITY_PROPERTY(PROP_DESCRIPTION, getDescription());
        APPEND_ENTITY_PROPERTY(PROP_POSITION, getLocalPosition());
        APPEND_ENTITY_PROPERTY(PROP_DIMENSIONS, getScaledDimensions());
        APPEND_ENTITY_PROPERTY(PROP_ROTATION, getLocalOrientation());
        APPEND_ENTITY_PROPERTY(PROP_REGISTRATION_POINT, getRegistrationPoint());
        APPEND_ENTITY_PROPERTY(PROP_CREATED, getCreated());
        APPEND_ENTITY_PROPERTY(PROP_LAST_EDITED_BY, getLastEditedBy());
        // APPEND_ENTITY_PROPERTY(PROP_ENTITY_HOST_TYPE, (uint32_t)getEntityHostType());  // not sent over the wire
        // APPEND_ENTITY_PROPERTY(PROP_OWNING_AVATAR_ID, getOwningAvatarID());            // not sent over the wire
        APPEND_ENTITY_PROPERTY(PROP_QUERY_AA_CUBE, getQueryAACube());
        APPEND_ENTITY_PROPERTY(PROP_CAN_CAST_SHADOW, getCanCastShadow());
        // APPEND_ENTITY_PROPERTY(PROP_VISIBLE_IN_SECONDARY_CAMERA, getIsVisibleInSecondaryCamera()); // not sent over the wire
        APPEND_ENTITY_PROPERTY(PROP_RENDER_LAYER, (uint32_t)getRenderLayer());
        APPEND_ENTITY_PROPERTY(PROP_PRIMITIVE_MODE, (uint32_t)getPrimitiveMode());
        APPEND_ENTITY_PROPERTY(PROP_IGNORE_PICK_INTERSECTION, getIgnorePickIntersection());
        APPEND_ENTITY_PROPERTY(PROP_RENDER_WITH_ZONES, getRenderWithZones());
        APPEND_ENTITY_PROPERTY(PROP_BILLBOARD_MODE, (uint32_t)getBillboardMode());
        withReadLock([&] {
            _grabProperties.appendSubclassData(packetData, params, entityTreeElementExtraEncodeData, requestedProperties,
                propertyFlags, propertiesDidntFit, propertyCount, appendState);
        });

        // Physics
        APPEND_ENTITY_PROPERTY(PROP_DENSITY, getDensity());
        APPEND_ENTITY_PROPERTY(PROP_VELOCITY, getLocalVelocity());
        APPEND_ENTITY_PROPERTY(PROP_ANGULAR_VELOCITY, getLocalAngularVelocity());
        APPEND_ENTITY_PROPERTY(PROP_GRAVITY, getGravity());
        APPEND_ENTITY_PROPERTY(PROP_ACCELERATION, getAcceleration());
        APPEND_ENTITY_PROPERTY(PROP_DAMPING, getDamping());
        APPEND_ENTITY_PROPERTY(PROP_ANGULAR_DAMPING, getAngularDamping());
        APPEND_ENTITY_PROPERTY(PROP_RESTITUTION, getRestitution());
        APPEND_ENTITY_PROPERTY(PROP_FRICTION, getFriction());
        APPEND_ENTITY_PROPERTY(PROP_LIFETIME, getLifetime());
        APPEND_ENTITY_PROPERTY(PROP_COLLISIONLESS, getCollisionless());
        APPEND_ENTITY_PROPERTY(PROP_COLLISION_MASK, getCollisionMask());
        APPEND_ENTITY_PROPERTY(PROP_DYNAMIC, getDynamic());
        APPEND_ENTITY_PROPERTY(PROP_COLLISION_SOUND_URL, getCollisionSoundURL());
        APPEND_ENTITY_PROPERTY(PROP_ACTION_DATA, getDynamicData());

        // Cloning
        APPEND_ENTITY_PROPERTY(PROP_CLONEABLE, getCloneable());
        APPEND_ENTITY_PROPERTY(PROP_CLONE_LIFETIME, getCloneLifetime());
        APPEND_ENTITY_PROPERTY(PROP_CLONE_LIMIT, getCloneLimit());
        APPEND_ENTITY_PROPERTY(PROP_CLONE_DYNAMIC, getCloneDynamic());
        APPEND_ENTITY_PROPERTY(PROP_CLONE_AVATAR_ENTITY, getCloneAvatarEntity());
        APPEND_ENTITY_PROPERTY(PROP_CLONE_ORIGIN_ID, getCloneOriginID());

        // Scripts
        APPEND_ENTITY_PROPERTY(PROP_SCRIPT, getScript());
        APPEND_ENTITY_PROPERTY(PROP_SCRIPT_TIMESTAMP, getScriptTimestamp());
        APPEND_ENTITY_PROPERTY(PROP_SERVER_SCRIPTS, getServerScripts());

        // Certifiable Properties
        APPEND_ENTITY_PROPERTY(PROP_ITEM_NAME, QString());
        APPEND_ENTITY_PROPERTY(PROP_ITEM_DESCRIPTION, QString());
        APPEND_ENTITY_PROPERTY(PROP_ITEM_CATEGORIES, QString());
        APPEND_ENTITY_PROPERTY(PROP_ITEM_ARTIST, QString());
        APPEND_ENTITY_PROPERTY(PROP_ITEM_LICENSE, QString());
        APPEND_ENTITY_PROPERTY(PROP_LIMITED_RUN, quint32(-1));
        APPEND_ENTITY_PROPERTY(PROP_MARKETPLACE_ID, QString());
        APPEND_ENTITY_PROPERTY(PROP_EDITION_NUMBER, 0U);
        APPEND_ENTITY_PROPERTY(PROP_ENTITY_INSTANCE_NUMBER, 0U);
        APPEND_ENTITY_PROPERTY(PROP_CERTIFICATE_ID, QString());
        APPEND_ENTITY_PROPERTY(PROP_CERTIFICATE_TYPE, QString());
        APPEND_ENTITY_PROPERTY(PROP_STATIC_CERTIFICATE_VERSION, 0U);

        appendSubclassData(packetData, params, entityTreeElementExtraEncodeData,
                                requestedProperties,
                                propertyFlags,
                                propertiesDidntFit,
                                propertyCount,
                                appendState);
    }

    if (propertyCount > 0) {
        int endOfEntityItemData = packetData->getUncompressedByteOffset();
        encodedPropertyFlags = propertyFlags;
        int newPropertyFlagsLength = encodedPropertyFlags.length();
        packetData->updatePriorBytes(propertyFlagsOffset,
                (const unsigned char*)encodedPropertyFlags.constData(), encodedPropertyFlags.length());

        // if the size of the PropertyFlags shrunk, we need to shift everything down to front of packet.
        if (newPropertyFlagsLength < oldPropertyFlagsLength) {
            int oldSize = packetData->getUncompressedSize();
            const unsigned char* modelItemData = packetData->getUncompressedData(propertyFlagsOffset + oldPropertyFlagsLength);
            int modelItemDataLength = endOfEntityItemData - startOfEntityItemData;
            int newEntityItemDataStart = propertyFlagsOffset + newPropertyFlagsLength;
            packetData->updatePriorBytes(newEntityItemDataStart, modelItemData, modelItemDataLength);
            int newSize = oldSize - (oldPropertyFlagsLength - newPropertyFlagsLength);
            packetData->setUncompressedSize(newSize);

        } else {
            assert(newPropertyFlagsLength == oldPropertyFlagsLength); // should not have grown
        }

        packetData->endLevel(entityLevel);
    } else {
        packetData->discardLevel(entityLevel);
        appendState = OctreeElement::NONE; // if we got here, then we didn't include the item
    }

    // If any part of the model items didn't fit, then the element is considered partial
    if (appendState != OctreeElement::COMPLETED) {
        // add this item into our list for the next appendElementData() pass
        entityTreeElementExtraEncodeData->entities.insert(getEntityItemID(), propertiesDidntFit);
    }

    // if any part of our entity was sent, call trackSend
    if (appendState != OctreeElement::NONE) {
        params.trackSend(getID(), getLastEdited());
    }

    return appendState;
}

// TODO: My goal is to get rid of this concept completely. The old code (and some of the current code) used this
// result to calculate if a packet being sent to it was potentially bad or corrupt. I've adjusted this to now
// only consider the minimum header bytes as being required. But it would be preferable to completely eliminate
// this logic from the callers.
int EntityItem::expectedBytes() {
    // Header bytes
    //    object ID [16 bytes]
    //    ByteCountCoded(type code) [~1 byte]
    //    last edited [8 bytes]
    //    ByteCountCoded(last_edited to last_updated delta) [~1-8 bytes]
    //    PropertyFlags<>( everything ) [1-2 bytes]
    // ~27-35 bytes...
    const int MINIMUM_HEADER_BYTES = 27;
    return MINIMUM_HEADER_BYTES;
}

const uint8_t PENDING_STATE_NOTHING = 0;
const uint8_t PENDING_STATE_TAKE = 1;
const uint8_t PENDING_STATE_RELEASE = 2;

// clients use this method to unpack FULL updates from entity-server
int EntityItem::readEntityDataFromBuffer(const unsigned char* data, int bytesLeftToRead, ReadBitstreamToTreeParams& args) {
    setSourceUUID(args.sourceUUID);

    args.entitiesPerPacket++;

    // Header bytes
    //    object ID [16 bytes]
    //    ByteCountCoded(type code) [~1 byte]
    //    last edited [8 bytes]
    //    ByteCountCoded(last_edited to last_updated delta) [~1-8 bytes]
    //    PropertyFlags<>( everything ) [1-2 bytes]
    // ~27-35 bytes...
    const int MINIMUM_HEADER_BYTES = 27;

    if (bytesLeftToRead < MINIMUM_HEADER_BYTES) {
        return 0;
    }

    int64_t clockSkew = 0;
    uint64_t maxPingRoundTrip = 33333; // two frames periods at 60 fps
    if (args.sourceNode) {
        clockSkew = args.sourceNode->getClockSkewUsec();
        const float MSECS_PER_USEC = 1000;
        maxPingRoundTrip += args.sourceNode->getPingMs() * MSECS_PER_USEC;
    }

    BufferParser parser(data, bytesLeftToRead);

#ifdef DEBUG
#define VALIDATE_ENTITY_ITEM_PARSER 1
#endif

#ifdef VALIDATE_ENTITY_ITEM_PARSER
    int bytesRead = 0;
    int originalLength = bytesLeftToRead;
    // TODO: figure out a way to avoid the big deep copy below.
    QByteArray originalDataBuffer((const char*)data, originalLength); // big deep copy!
    const unsigned char* dataAt = data;
#endif

    // id
    parser.readUuid(_id);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    {
        QByteArray encodedID = originalDataBuffer.mid(bytesRead, NUM_BYTES_RFC4122_UUID); // maximum possible size
        QUuid id = QUuid::fromRfc4122(encodedID);
        dataAt += encodedID.size();
        bytesRead += encodedID.size();
        Q_ASSERT(id == _id);
        Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
    }
#endif

    // type
    parser.readCompressedCount<quint32>((quint32&)_type);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    QByteArray encodedType = originalDataBuffer.mid(bytesRead); // maximum possible size
    ByteCountCoded<quint32> typeCoder = encodedType;
    encodedType = typeCoder; // determine true length
    dataAt += encodedType.size();
    bytesRead += encodedType.size();
    quint32 type = typeCoder;
    EntityTypes::EntityType oldType = (EntityTypes::EntityType)type;
    Q_ASSERT(oldType == _type);
    Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
#endif

    bool overwriteLocalData = true; // assume the new content overwrites our local data
    quint64 now = usecTimestampNow();
    bool somethingChanged = false;

    // _created
    {
        quint64 createdFromBuffer = 0;
        parser.readValue(createdFromBuffer);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
        {
            quint64 createdFromBuffer2 = 0;
            memcpy(&createdFromBuffer2, dataAt, sizeof(createdFromBuffer2));
            dataAt += sizeof(createdFromBuffer2);
            bytesRead += sizeof(createdFromBuffer2);
            Q_ASSERT(createdFromBuffer2 == createdFromBuffer);
            Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
        }
#endif
        if (_created == UNKNOWN_CREATED_TIME) {
            // we don't yet have a _created timestamp, so we accept this one
            createdFromBuffer -= clockSkew;
            if (createdFromBuffer > now || createdFromBuffer == UNKNOWN_CREATED_TIME) {
                createdFromBuffer = now;
            }
            _created = createdFromBuffer;
        }
    }

    #ifdef WANT_DEBUG
    {
        quint64 lastEdited = getLastEdited();
        float editedAgo = getEditedAgo();
        QString agoAsString = formatSecondsElapsed(editedAgo);
        QString ageAsString = formatSecondsElapsed(getAge());
        qCDebug(entities) << "------------------------------------------";
        qCDebug(entities) << "Loading entity " << getEntityItemID() << " from buffer...";
        qCDebug(entities) << "------------------------------------------";
        debugDump();
        qCDebug(entities) << "------------------------------------------";
        qCDebug(entities) << "    _created =" << _created;
        qCDebug(entities) << "    age=" << getAge() << "seconds - " << ageAsString;
        qCDebug(entities) << "    lastEdited =" << lastEdited;
        qCDebug(entities) << "    ago=" << editedAgo << "seconds - " << agoAsString;
    }
    #endif

    quint64 lastEditedFromBuffer = 0;

    // TODO: we could make this encoded as a delta from _created
    // _lastEdited
    parser.readValue(lastEditedFromBuffer);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    {
        quint64 lastEditedFromBuffer2 = 0;
        memcpy(&lastEditedFromBuffer2, dataAt, sizeof(lastEditedFromBuffer2));
        dataAt += sizeof(lastEditedFromBuffer2);
        bytesRead += sizeof(lastEditedFromBuffer2);
        Q_ASSERT(lastEditedFromBuffer2 == lastEditedFromBuffer);
        Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
    }
#endif
    quint64 lastEditedFromBufferAdjusted = lastEditedFromBuffer == 0 ? 0 : lastEditedFromBuffer - clockSkew;
    if (lastEditedFromBufferAdjusted > now) {
        lastEditedFromBufferAdjusted = now;
    }

    bool fromSameServerEdit = (lastEditedFromBuffer == _lastEditedFromRemoteInRemoteTime);

    #ifdef WANT_DEBUG
        qCDebug(entities) << "data from server **************** ";
        qCDebug(entities) << "                           entityItemID:" << getEntityItemID();
        qCDebug(entities) << "                                    now:" << now;
        qCDebug(entities) << "                          getLastEdited:" << debugTime(getLastEdited(), now);
        qCDebug(entities) << "                   lastEditedFromBuffer:" << debugTime(lastEditedFromBuffer, now);
        qCDebug(entities) << "                              clockSkew:" << clockSkew;
        qCDebug(entities) << "           lastEditedFromBufferAdjusted:" << debugTime(lastEditedFromBufferAdjusted, now);
        qCDebug(entities) << "                  _lastEditedFromRemote:" << debugTime(_lastEditedFromRemote, now);
        qCDebug(entities) << "      _lastEditedFromRemoteInRemoteTime:" << debugTime(_lastEditedFromRemoteInRemoteTime, now);
        qCDebug(entities) << "                     fromSameServerEdit:" << fromSameServerEdit;
    #endif

    bool ignoreServerPacket = false; // assume we'll use this server packet

    // If this packet is from the same server edit as the last packet we accepted from the server
    // we probably want to use it.
    if (fromSameServerEdit) {
        // If this is from the same sever packet, then check against any local changes since we got
        // the most recent packet from this server time
        if (_lastEdited > _lastEditedFromRemote) {
            ignoreServerPacket = true;
        }
    } else {
        // If this isn't from the same sever packet, then honor our skew adjusted times...
        // If we've changed our local tree more recently than the new data from this packet
        // then we will not be changing our values, instead we just read and skip the data
        if (_lastEdited > lastEditedFromBufferAdjusted) {
            ignoreServerPacket = true;
        }
    }

    // before proceeding, check to see if this is an entity that we know has been deleted, which
    // might happen in the case of out-of-order and/or recorvered packets, if we've deleted the entity
    // we can confidently ignore this packet
    EntityTreePointer tree = getTree();
    if (tree && tree->isDeletedEntity(_id)) {
        #ifdef WANT_DEBUG
            qCDebug(entities) << "Received packet for previously deleted entity [" << _id << "] ignoring. "
                "(inside " << __FUNCTION__ << ")";
        #endif
        ignoreServerPacket = true;
    }

    if (ignoreServerPacket) {
        overwriteLocalData = false;
        #ifdef WANT_DEBUG
            qCDebug(entities) << "IGNORING old data from server!!! ****************";
            debugDump();
        #endif
    } else {
        #ifdef WANT_DEBUG
            qCDebug(entities) << "USING NEW data from server!!! ****************";
            debugDump();
        #endif

        // don't allow _lastEdited to be in the future
        _lastEdited = lastEditedFromBufferAdjusted;
        _lastEditedFromRemote = now;
        _lastEditedFromRemoteInRemoteTime = lastEditedFromBuffer;
    }

    // last updated is stored as ByteCountCoded delta from lastEdited
    quint64 updateDelta;
    parser.readCompressedCount(updateDelta);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    {
        QByteArray encodedUpdateDelta = originalDataBuffer.mid(bytesRead); // maximum possible size
        ByteCountCoded<quint64> updateDeltaCoder = encodedUpdateDelta;
        quint64 updateDelta2 = updateDeltaCoder;
        Q_ASSERT(updateDelta == updateDelta2);
        encodedUpdateDelta = updateDeltaCoder; // determine true length
        dataAt += encodedUpdateDelta.size();
        bytesRead += encodedUpdateDelta.size();
        Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
    }
#endif

    if (overwriteLocalData) {
        _lastUpdated = lastEditedFromBufferAdjusted + updateDelta; // don't adjust for clock skew since we already did that
        #ifdef WANT_DEBUG
            qCDebug(entities) << "                           _lastUpdated:" << debugTime(_lastUpdated, now);
            qCDebug(entities) << "                            _lastEdited:" << debugTime(_lastEdited, now);
            qCDebug(entities) << "           lastEditedFromBufferAdjusted:" << debugTime(lastEditedFromBufferAdjusted, now);
        #endif
    }

    // Newer bitstreams will have a last simulated and a last updated value
    quint64 lastSimulatedFromBufferAdjusted = now;
    // last simulated is stored as ByteCountCoded delta from lastEdited
    quint64 simulatedDelta;
    parser.readCompressedCount(simulatedDelta);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    {
        QByteArray encodedSimulatedDelta = originalDataBuffer.mid(bytesRead); // maximum possible size
        ByteCountCoded<quint64> simulatedDeltaCoder = encodedSimulatedDelta;
        quint64 simulatedDelta2 = simulatedDeltaCoder;
        Q_ASSERT(simulatedDelta2 == simulatedDelta);
        encodedSimulatedDelta = simulatedDeltaCoder; // determine true length
        dataAt += encodedSimulatedDelta.size();
        bytesRead += encodedSimulatedDelta.size();
        Q_ASSERT(parser.offset() == (unsigned int) bytesRead);
    }
#endif

    if (overwriteLocalData) {
        lastSimulatedFromBufferAdjusted = lastEditedFromBufferAdjusted + simulatedDelta; // don't adjust for clock skew since we already did that
        if (lastSimulatedFromBufferAdjusted > now) {
            lastSimulatedFromBufferAdjusted = now;
        }
        #ifdef WANT_DEBUG
            qCDebug(entities) << "                            _lastEdited:" << debugTime(_lastEdited, now);
            qCDebug(entities) << "           lastEditedFromBufferAdjusted:" << debugTime(lastEditedFromBufferAdjusted, now);
            qCDebug(entities) << "        lastSimulatedFromBufferAdjusted:" << debugTime(lastSimulatedFromBufferAdjusted, now);
        #endif
    }

    #ifdef WANT_DEBUG
        if (overwriteLocalData) {
            qCDebug(entities) << "EntityItem::readEntityDataFromBuffer()... changed entity:" << getEntityItemID();
            qCDebug(entities) << "                          getLastEdited:" << debugTime(getLastEdited(), now);
            qCDebug(entities) << "                       getLastSimulated:" << debugTime(getLastSimulated(), now);
            qCDebug(entities) << "                         getLastUpdated:" << debugTime(getLastUpdated(), now);
        }
    #endif


    // Property Flags
    EntityPropertyFlags propertyFlags;
    parser.readFlags(propertyFlags);
#ifdef VALIDATE_ENTITY_ITEM_PARSER
    {
        QByteArray encodedPropertyFlags = originalDataBuffer.mid(bytesRead); // maximum possible size
        EntityPropertyFlags propertyFlags2 = encodedPropertyFlags;
        dataAt += propertyFlags.getEncodedLength();
        bytesRead += propertyFlags.getEncodedLength();
        Q_ASSERT(propertyFlags2 == propertyFlags);
        Q_ASSERT(parser.offset() == (unsigned int)bytesRead);
    }
#endif

#ifdef VALIDATE_ENTITY_ITEM_PARSER
    Q_ASSERT(parser.data() + parser.offset() == dataAt);
#else
    const unsigned char* dataAt = parser.data() + parser.offset();
    int bytesRead = (int)parser.offset();
#endif

    auto nodeList = DependencyManager::get<NodeList>();
    const QUuid& myNodeID = nodeList->getSessionUUID();
    bool weOwnSimulation = _simulationOwner.matchesValidID(myNodeID);

    // NOTE: the server is authoritative for changes to simOwnerID so we always unpack ownership data
    // even when we would otherwise ignore the rest of the packet.

    bool filterRejection = false;
    if (propertyFlags.getHasProperty(PROP_SIMULATION_OWNER)) {
        QByteArray simOwnerData;
        int bytes = OctreePacketData::unpackDataFromBytes(dataAt, simOwnerData);
        SimulationOwner newSimOwner;
        newSimOwner.fromByteArray(simOwnerData);
        dataAt += bytes;
        bytesRead += bytes;

        if (wantTerseEditLogging() && _simulationOwner != newSimOwner) {
            qCDebug(entities) << "sim ownership for" << getDebugName() << "is now" << newSimOwner;
        }
        // This is used in the custom physics setters, below. When an entity-server filter alters
        // or rejects a set of properties, it clears this. In such cases, we don't want those custom
        // setters to ignore what the server says.
        filterRejection = newSimOwner.getID().isNull();
        if (weOwnSimulation) {
            if (newSimOwner.getID().isNull() && !pendingRelease(lastEditedFromBufferAdjusted)) {
                // entity-server is trying to clear our ownership (probably at our own request)
                // but we actually want to own it, therefore we ignore this clear event
                // and pretend that we own it (e.g. we assume we'll receive ownership soon)

                // However, for now, when the server uses a newer time than what we sent, listen to what we're told.
                if (overwriteLocalData) {
                    weOwnSimulation = false;
                }
            } else if (_simulationOwner.set(newSimOwner)) {
                markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);
                somethingChanged = true;
                // recompute weOwnSimulation for later
                weOwnSimulation = _simulationOwner.matchesValidID(myNodeID);
            }
        } else if (_pendingOwnershipState == PENDING_STATE_TAKE) {
            // we're waiting to receive acceptance of a bid
            // this ownership data either satisifies our bid or does not
            bool bidIsSatisfied = newSimOwner.getID() == myNodeID &&
                (newSimOwner.getPriority() == _pendingOwnershipPriority ||
                 (_pendingOwnershipPriority == VOLUNTEER_SIMULATION_PRIORITY &&
                  newSimOwner.getPriority() == RECRUIT_SIMULATION_PRIORITY));

            if (newSimOwner.getID().isNull()) {
                // the entity-server is clearing someone else's ownership
                if (!_simulationOwner.isNull()) {
                    markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);
                    somethingChanged = true;
                    _simulationOwner.clearCurrentOwner();
                }
            } else {
                if (newSimOwner.getID() != _simulationOwner.getID()) {
                    markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);
                }
                if (_simulationOwner.set(newSimOwner)) {
                    // the entity-server changed ownership
                    somethingChanged = true;
                }
            }
            if (bidIsSatisfied || (somethingChanged && _pendingOwnershipTimestamp < now - maxPingRoundTrip)) {
                // the bid has been satisfied, or it has been invalidated by data sent AFTER the bid should have been received
                // in either case: accept our fate and clear pending state
                _pendingOwnershipState = PENDING_STATE_NOTHING;
                _pendingOwnershipPriority = 0;
            }
            weOwnSimulation = bidIsSatisfied || (_simulationOwner.getID() == myNodeID);
        } else {
            // we are not waiting to take ownership
            if (newSimOwner.getID() != _simulationOwner.getID()) {
                markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);
            }
            if (_simulationOwner.set(newSimOwner)) {
                // the entity-server changed ownership...
                somethingChanged = true;
                if (newSimOwner.getID() == myNodeID) {
                    // we have recieved ownership
                    weOwnSimulation = true;
                    // accept our fate and clear pendingState (just in case)
                    _pendingOwnershipState = PENDING_STATE_NOTHING;
                    _pendingOwnershipPriority = 0;
                }
            }
        }
    }

    auto lastEdited = lastEditedFromBufferAdjusted;
    bool otherOverwrites = overwriteLocalData && !weOwnSimulation;
    // calculate hasGrab once outside the lambda rather than calling it every time inside
    bool hasGrab = stillHasGrab();
    auto shouldUpdate = [lastEdited, otherOverwrites, filterRejection, hasGrab](quint64 updatedTimestamp, bool valueChanged) {
        if (hasGrab) {
            return false;
        }
        bool simulationChanged = lastEdited > updatedTimestamp;
        return otherOverwrites && simulationChanged && (valueChanged || filterRejection);
    };

    // Core
    // PROP_SIMULATION_OWNER handled above
    {   // parentID and parentJointIndex are protected by simulation ownership
        bool oldOverwrite = overwriteLocalData;
        overwriteLocalData = overwriteLocalData && !weOwnSimulation;
        READ_ENTITY_PROPERTY(PROP_PARENT_ID, QUuid, setParentID);
        READ_ENTITY_PROPERTY(PROP_PARENT_JOINT_INDEX, quint16, setParentJointIndex);
        overwriteLocalData = oldOverwrite;
    }
    READ_ENTITY_PROPERTY(PROP_VISIBLE, bool, setVisible);
    READ_ENTITY_PROPERTY(PROP_NAME, QString, setName);
    READ_ENTITY_PROPERTY(PROP_LOCKED, bool, setLocked);
    READ_ENTITY_PROPERTY(PROP_USER_DATA, QString, setUserData);
    READ_ENTITY_PROPERTY(PROP_PRIVATE_USER_DATA, QString, setPrivateUserData);
    READ_ENTITY_PROPERTY(PROP_HREF, QString, setHref);
    READ_ENTITY_PROPERTY(PROP_DESCRIPTION, QString, setDescription);
    {   // When we own the simulation we don't accept updates to the entity's transform/velocities
        // we also want to ignore any duplicate packets that have the same "recently updated" values
        // as a packet we've already recieved. This is because we want multiple edits of the same
        // information to be idempotent, but if we applied new physics properties we'd resimulation
        // with small differences in results.

        // Because the regular streaming property "setters" only have access to the new value, we've
        // made these lambdas that can access other details about the previous updates to suppress
        // any duplicates.

        // Note: duplicate packets are expected and not wrong. They may be sent for any number of
        // reasons and the contract is that the client handles them in an idempotent manner.
        auto customUpdatePositionFromNetwork = [this, shouldUpdate, lastEdited](glm::vec3 value) {
            if (shouldUpdate(_lastUpdatedPositionTimestamp, value != _lastUpdatedPositionValue)) {
                setPosition(value);
                _lastUpdatedPositionTimestamp = lastEdited;
                _lastUpdatedPositionValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_POSITION, glm::vec3, customUpdatePositionFromNetwork);
    }
    READ_ENTITY_PROPERTY(PROP_DIMENSIONS, glm::vec3, setScaledDimensions);
    {   // See comment above
        auto customUpdateRotationFromNetwork = [this, shouldUpdate, lastEdited](glm::quat value) {
            if (shouldUpdate(_lastUpdatedRotationTimestamp, value != _lastUpdatedRotationValue)) {
                setRotation(value);
                _lastUpdatedRotationTimestamp = lastEdited;
                _lastUpdatedRotationValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_ROTATION, glm::quat, customUpdateRotationFromNetwork);
    }
    READ_ENTITY_PROPERTY(PROP_REGISTRATION_POINT, glm::vec3, setRegistrationPoint);
    READ_ENTITY_PROPERTY(PROP_CREATED, quint64, setCreated);
    READ_ENTITY_PROPERTY(PROP_LAST_EDITED_BY, QUuid, setLastEditedBy);
    // READ_ENTITY_PROPERTY(PROP_ENTITY_HOST_TYPE, entity::HostType, setEntityHostType); // not sent over the wire
    // READ_ENTITY_PROPERTY(PROP_OWNING_AVATAR_ID, QUuuid, setOwningAvatarID);           // not sent over the wire
    {   // See comment above
        auto customUpdateQueryAACubeFromNetwork = [this, shouldUpdate, lastEdited](AACube value) {
            if (shouldUpdate(_lastUpdatedQueryAACubeTimestamp, value != _lastUpdatedQueryAACubeValue)) {
                setQueryAACube(value);
                _lastUpdatedQueryAACubeTimestamp = lastEdited;
                _lastUpdatedQueryAACubeValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_QUERY_AA_CUBE, AACube, customUpdateQueryAACubeFromNetwork);
    }
    READ_ENTITY_PROPERTY(PROP_CAN_CAST_SHADOW, bool, setCanCastShadow);
    // READ_ENTITY_PROPERTY(PROP_VISIBLE_IN_SECONDARY_CAMERA, bool, setIsVisibleInSecondaryCamera);  // not sent over the wire
    READ_ENTITY_PROPERTY(PROP_RENDER_LAYER, RenderLayer, setRenderLayer);
    READ_ENTITY_PROPERTY(PROP_PRIMITIVE_MODE, PrimitiveMode, setPrimitiveMode);
    READ_ENTITY_PROPERTY(PROP_IGNORE_PICK_INTERSECTION, bool, setIgnorePickIntersection);
    READ_ENTITY_PROPERTY(PROP_RENDER_WITH_ZONES, QVector<QUuid>, setRenderWithZones);
    READ_ENTITY_PROPERTY(PROP_BILLBOARD_MODE, BillboardMode, setBillboardMode);
    withWriteLock([&] {
        int bytesFromGrab = _grabProperties.readEntitySubclassDataFromBuffer(dataAt, (bytesLeftToRead - bytesRead), args,
            propertyFlags, overwriteLocalData,
            somethingChanged);
        bytesRead += bytesFromGrab;
        dataAt += bytesFromGrab;
    });

    READ_ENTITY_PROPERTY(PROP_DENSITY, float, setDensity);
    {
        auto customUpdateVelocityFromNetwork = [this, shouldUpdate, lastEdited](glm::vec3 value) {
            if (shouldUpdate(_lastUpdatedVelocityTimestamp, value != _lastUpdatedVelocityValue)) {
                setVelocity(value);
                _lastUpdatedVelocityTimestamp = lastEdited;
                _lastUpdatedVelocityValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_VELOCITY, glm::vec3, customUpdateVelocityFromNetwork);
        auto customUpdateAngularVelocityFromNetwork = [this, shouldUpdate, lastEdited](glm::vec3 value){
            if (shouldUpdate(_lastUpdatedAngularVelocityTimestamp, value != _lastUpdatedAngularVelocityValue)) {
                setAngularVelocity(value);
                _lastUpdatedAngularVelocityTimestamp = lastEdited;
                _lastUpdatedAngularVelocityValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_ANGULAR_VELOCITY, glm::vec3, customUpdateAngularVelocityFromNetwork);
        READ_ENTITY_PROPERTY(PROP_GRAVITY, glm::vec3, setGravity);
        auto customSetAcceleration = [this, shouldUpdate, lastEdited](glm::vec3 value){
            if (shouldUpdate(_lastUpdatedAccelerationTimestamp, value != _lastUpdatedAccelerationValue)) {
                setAcceleration(value);
                _lastUpdatedAccelerationTimestamp = lastEdited;
                _lastUpdatedAccelerationValue = value;
            }
        };
        READ_ENTITY_PROPERTY(PROP_ACCELERATION, glm::vec3, customSetAcceleration);
    }
    READ_ENTITY_PROPERTY(PROP_DAMPING, float, setDamping);
    READ_ENTITY_PROPERTY(PROP_ANGULAR_DAMPING, float, setAngularDamping);
    READ_ENTITY_PROPERTY(PROP_RESTITUTION, float, setRestitution);
    READ_ENTITY_PROPERTY(PROP_FRICTION, float, setFriction);
    READ_ENTITY_PROPERTY(PROP_LIFETIME, float, setLifetime);
    READ_ENTITY_PROPERTY(PROP_COLLISIONLESS, bool, setCollisionless);
    READ_ENTITY_PROPERTY(PROP_COLLISION_MASK, uint16_t, setCollisionMask);
    READ_ENTITY_PROPERTY(PROP_DYNAMIC, bool, setDynamic);
    READ_ENTITY_PROPERTY(PROP_COLLISION_SOUND_URL, QString, setCollisionSoundURL);
    READ_ENTITY_PROPERTY(PROP_ACTION_DATA, QByteArray, setDynamicData);

    // Cloning
    READ_ENTITY_PROPERTY(PROP_CLONEABLE, bool, setCloneable);
    READ_ENTITY_PROPERTY(PROP_CLONE_LIFETIME, float, setCloneLifetime);
    READ_ENTITY_PROPERTY(PROP_CLONE_LIMIT, float, setCloneLimit);
    READ_ENTITY_PROPERTY(PROP_CLONE_DYNAMIC, bool, setCloneDynamic);
    READ_ENTITY_PROPERTY(PROP_CLONE_AVATAR_ENTITY, bool, setCloneAvatarEntity);
    READ_ENTITY_PROPERTY(PROP_CLONE_ORIGIN_ID, QUuid, setCloneOriginID);

    // Scripts
    READ_ENTITY_PROPERTY(PROP_SCRIPT, QString, setScript);
    READ_ENTITY_PROPERTY(PROP_SCRIPT_TIMESTAMP, quint64, setScriptTimestamp);
    {
        // We use this scope to work around an issue stopping server script changes
        // from being received by an entity script server running a script that continously updates an entity.
        // Basically, we'll allow recent changes to the server scripts even if there are local changes to other properties
        // that have been made more recently.
        bool oldOverwrite = overwriteLocalData;
        overwriteLocalData = !ignoreServerPacket || (lastEditedFromBufferAdjusted > _serverScriptsChangedTimestamp);
        READ_ENTITY_PROPERTY(PROP_SERVER_SCRIPTS, QString, setServerScripts);
        overwriteLocalData = oldOverwrite;
    }

    // Certifiable props
    SKIP_ENTITY_PROPERTY(PROP_ITEM_NAME, QString);
    SKIP_ENTITY_PROPERTY(PROP_ITEM_DESCRIPTION, QString);
    SKIP_ENTITY_PROPERTY(PROP_ITEM_CATEGORIES, QString);
    SKIP_ENTITY_PROPERTY(PROP_ITEM_ARTIST, QString);
    SKIP_ENTITY_PROPERTY(PROP_ITEM_LICENSE, QString);
    SKIP_ENTITY_PROPERTY(PROP_LIMITED_RUN, quint32);
    SKIP_ENTITY_PROPERTY(PROP_MARKETPLACE_ID, QString);
    SKIP_ENTITY_PROPERTY(PROP_EDITION_NUMBER, quint32);
    SKIP_ENTITY_PROPERTY(PROP_ENTITY_INSTANCE_NUMBER, quint32);
    SKIP_ENTITY_PROPERTY(PROP_CERTIFICATE_ID, QString);
    SKIP_ENTITY_PROPERTY(PROP_CERTIFICATE_TYPE, QString);
    SKIP_ENTITY_PROPERTY(PROP_STATIC_CERTIFICATE_VERSION, quint32);

    bytesRead += readEntitySubclassDataFromBuffer(dataAt, (bytesLeftToRead - bytesRead), args,
                                                  propertyFlags, overwriteLocalData, somethingChanged);

    ////////////////////////////////////
    // WARNING: Do not add stream content here after the subclass. Always add it before the subclass
    //
    // NOTE: we had a bad version of the stream that we added stream data after the subclass. We can attempt to recover
    // by doing this parsing here... but it's not likely going to fully recover the content.
    //

    if (overwriteLocalData &&
            !hasGrab &&
            (getDirtyFlags() & (Simulation::DIRTY_TRANSFORM | Simulation::DIRTY_VELOCITIES))) {
        // NOTE: This code is attempting to "repair" the old data we just got from the server to make it more
        // closely match where the entities should be if they'd stepped forward in time to "now". The server
        // is sending us data with a known "last simulated" time. That time is likely in the past, and therefore
        // this "new" data is actually slightly out of date. We calculate the time we need to skip forward and
        // use our simulation helper routine to get a best estimate of where the entity should be.
        //
        // NOTE: We don't want to do this in the hasGrab case because grabs "know best"
        // (e.g. grabs will prevent drift between distributed physics simulations).
        //
        float skipTimeForward = (float)(now - lastSimulatedFromBufferAdjusted) / (float)(USECS_PER_SECOND);

        // we want to extrapolate the motion forward to compensate for packet travel time, but
        // we don't want the side effect of flag setting.
        stepKinematicMotion(skipTimeForward);
    }

    if (overwriteLocalData) {
        if (!_simulationOwner.matchesValidID(myNodeID)) {
            _lastSimulated = now;
        }
    }

    // Tracking for editing roundtrips here. We will tell our EntityTree that we just got incoming data about
    // and entity that was edited at some time in the past. The tree will determine how it wants to track this
    // information.
    EntityTreeElementPointer element = _element; // use local copy of _element for logic below
    if (element && element->getTree()) {
        element->getTree()->trackIncomingEntityLastEdited(lastEditedFromBufferAdjusted, bytesRead);
    }

    if (somethingChanged) {
        somethingChangedNotification();
    }

    return bytesRead;
}

void EntityItem::debugDump() const {
    auto position = getWorldPosition();
    qCDebug(entities) << "EntityItem id:" << getEntityItemID();
    qCDebug(entities, " edited ago:%f", (double)getEditedAgo());
    qCDebug(entities, " position:%f,%f,%f", (double)position.x, (double)position.y, (double)position.z);
    qCDebug(entities) << " dimensions:" << getScaledDimensions();
}

// adjust any internal timestamps to fix clock skew for this server
void EntityItem::adjustEditPacketForClockSkew(QByteArray& buffer, qint64 clockSkew) {
    unsigned char* dataAt = reinterpret_cast<unsigned char*>(buffer.data());
    int octets = numberOfThreeBitSectionsInCode(dataAt);
    int lengthOfOctcode = (int)bytesRequiredForCodeLength(octets);
    dataAt += lengthOfOctcode;

    // lastEdited
    quint64 lastEditedInLocalTime;
    memcpy(&lastEditedInLocalTime, dataAt, sizeof(lastEditedInLocalTime));
    quint64 lastEditedInServerTime = lastEditedInLocalTime > 0 ? lastEditedInLocalTime + clockSkew : 0;
    memcpy(dataAt, &lastEditedInServerTime, sizeof(lastEditedInServerTime));
    #ifdef WANT_DEBUG
        qCDebug(entities, "EntityItem::adjustEditPacketForClockSkew()...");
        qCDebug(entities) << "     lastEditedInLocalTime: " << lastEditedInLocalTime;
        qCDebug(entities) << "                 clockSkew: " << clockSkew;
        qCDebug(entities) << "    lastEditedInServerTime: " << lastEditedInServerTime;
    #endif
    //assert(lastEditedInLocalTime > (quint64)0);
}

float EntityItem::computeMass() const {
    glm::vec3 dimensions = getScaledDimensions();
    return getDensity() * _volumeMultiplier * dimensions.x * dimensions.y * dimensions.z;
}

void EntityItem::setDensity(float density) {
    float clampedDensity = glm::max(glm::min(density, ENTITY_ITEM_MAX_DENSITY), ENTITY_ITEM_MIN_DENSITY);
    withWriteLock([&] {
        if (_density != clampedDensity) {
            _density = clampedDensity;
            _flags |= Simulation::DIRTY_MASS;
        }
    });
}

void EntityItem::setMass(float mass) {
    // Setting the mass actually changes the _density (at fixed volume), however
    // we must protect the density range to help maintain stability of physics simulation
    // therefore this method might not accept the mass that is supplied.

    glm::vec3 dimensions = getScaledDimensions();
    float volume = _volumeMultiplier * dimensions.x * dimensions.y * dimensions.z;

    // compute new density
    float newDensity = 1.0f;
    if (volume < ENTITY_ITEM_MIN_VOLUME) {
        // avoid divide by zero
        newDensity = glm::min(mass / ENTITY_ITEM_MIN_VOLUME, ENTITY_ITEM_MAX_DENSITY);
    } else {
        newDensity = glm::max(glm::min(mass / volume, ENTITY_ITEM_MAX_DENSITY), ENTITY_ITEM_MIN_DENSITY);
    }
    withWriteLock([&] {
        if (_density != newDensity) {
            _density = newDensity;
            _flags |= Simulation::DIRTY_MASS;
        }
    });
}

void EntityItem::setHref(QString value) {
    auto href = value.toLower();
    // Let's let the user set the value of this property to anything, then let consumers of the property
    // decide what to do with it. Currently, the only in-engine consumers are `EntityTreeRenderer::mousePressEvent()`
    // and `OtherAvatar::handleChangedAvatarEntityData()` (to remove the href property from others' avatar entities).
    //
    // We want this property to be as flexible as possible. The value of this property _should_ only be values that can
    // be handled by `AddressManager::handleLookupString()`. That function will return `false` and not do
    // anything if the value of this property isn't something that function can handle.
    withWriteLock([&] {
        _href = value;
    });
}

void EntityItem::setCollisionSoundURL(const QString& value) {
    bool modified = false;
    withWriteLock([&] {
        if (_collisionSoundURL != value) {
            _collisionSoundURL = value;
            modified = true;
        }
    });
    if (modified) {
        if (auto myTree = getTree()) {
            myTree->notifyNewCollisionSoundURL(value, getEntityItemID());
        }
    }
}

void EntityItem::simulate(const quint64& now) {
    DETAILED_PROFILE_RANGE(simulation_physics, "Simulate");
    if (getLastSimulated() == 0) {
        setLastSimulated(now);
    }

    float timeElapsed = (float)(now - getLastSimulated()) / (float)(USECS_PER_SECOND);

    #ifdef WANT_DEBUG
        qCDebug(entities) << "********** EntityItem::simulate()";
        qCDebug(entities) << "    entity ID=" << getEntityItemID();
        qCDebug(entities) << "    simulator ID=" << getSimulatorID();
        qCDebug(entities) << "    now=" << now;
        qCDebug(entities) << "    _lastSimulated=" << _lastSimulated;
        qCDebug(entities) << "    timeElapsed=" << timeElapsed;
        qCDebug(entities) << "    hasVelocity=" << hasVelocity();
        qCDebug(entities) << "    hasGravity=" << hasGravity();
        qCDebug(entities) << "    hasAcceleration=" << hasAcceleration();
        qCDebug(entities) << "    hasAngularVelocity=" << hasAngularVelocity();
        qCDebug(entities) << "    getAngularVelocity=" << getLocalAngularVelocity();
        qCDebug(entities) << "    isMortal=" << isMortal();
        qCDebug(entities) << "    getAge()=" << getAge();
        qCDebug(entities) << "    getLifetime()=" << getLifetime();


        if (hasVelocity() || hasGravity()) {
            qCDebug(entities) << "    MOVING...=";
            qCDebug(entities) << "        hasVelocity=" << hasVelocity();
            qCDebug(entities) << "        hasGravity=" << hasGravity();
            qCDebug(entities) << "        hasAcceleration=" << hasAcceleration();
            qCDebug(entities) << "        hasAngularVelocity=" << hasAngularVelocity();
            qCDebug(entities) << "        getAngularVelocity=" << getLocalAngularVelocity();
        }
        if (hasAngularVelocity()) {
            qCDebug(entities) << "    CHANGING...=";
            qCDebug(entities) << "        hasAngularVelocity=" << hasAngularVelocity();
            qCDebug(entities) << "        getAngularVelocity=" << getLocalAngularVelocity();
        }
        if (isMortal()) {
            qCDebug(entities) << "    MORTAL...=";
            qCDebug(entities) << "        isMortal=" << isMortal();
            qCDebug(entities) << "        getAge()=" << getAge();
            qCDebug(entities) << "        getLifetime()=" << getLifetime();
        }
        qCDebug(entities) << "     ********** EntityItem::simulate() .... SETTING _lastSimulated=" << _lastSimulated;
    #endif

    if (!stepKinematicMotion(timeElapsed)) {
        // this entity is no longer moving
        // flag it to transition from KINEMATIC to STATIC
        markDirtyFlags(Simulation::DIRTY_MOTION_TYPE);
        setAcceleration(Vectors::ZERO);
    }
    setLastSimulated(now);
}

bool EntityItem::stepKinematicMotion(float timeElapsed) {
    DETAILED_PROFILE_RANGE(simulation_physics, "StepKinematicMotion");
    // get all the data
    Transform transform;
    glm::vec3 linearVelocity;
    glm::vec3 angularVelocity;
    getLocalTransformAndVelocities(transform, linearVelocity, angularVelocity);

    // find out if it is moving
    bool isSpinning = (glm::length2(angularVelocity) > 0.0f);
    float linearSpeedSquared = glm::length2(linearVelocity);
    bool isTranslating = linearSpeedSquared > 0.0f;
    bool moving = isTranslating || isSpinning;
    if (!moving) {
        return false;
    }

    if (timeElapsed <= 0.0f) {
        // someone gave us a useless time value so bail early
        // but return 'true' because it is moving
        return true;
    }

    const float MAX_TIME_ELAPSED = 1.0f; // seconds
    if (timeElapsed > MAX_TIME_ELAPSED) {
        qCDebug(entities) << "kinematic timestep = " << timeElapsed << " truncated to " << MAX_TIME_ELAPSED;
    }
    timeElapsed = glm::min(timeElapsed, MAX_TIME_ELAPSED);

    if (isSpinning) {
        float angularDamping = getAngularDamping();
        // angular damping
        if (angularDamping > 0.0f) {
            angularVelocity *= powf(1.0f - angularDamping, timeElapsed);
        }

        const float MIN_KINEMATIC_ANGULAR_SPEED_SQUARED =
            KINEMATIC_ANGULAR_SPEED_THRESHOLD * KINEMATIC_ANGULAR_SPEED_THRESHOLD;
        if (glm::length2(angularVelocity) < MIN_KINEMATIC_ANGULAR_SPEED_SQUARED) {
            angularVelocity = Vectors::ZERO;
        } else {
            // for improved agreement with the way Bullet integrates rotations we use an approximation
            // and break the integration into bullet-sized substeps
            glm::quat rotation = transform.getRotation();
            float dt = timeElapsed;
            while (dt > 0.0f) {
                glm::quat  dQ = computeBulletRotationStep(angularVelocity, glm::min(dt, PHYSICS_ENGINE_FIXED_SUBSTEP));
                rotation = glm::normalize(dQ * rotation);
                dt -= PHYSICS_ENGINE_FIXED_SUBSTEP;
            }
            transform.setRotation(rotation);
        }
    }

    glm::vec3 position = transform.getTranslation();
    const float MIN_KINEMATIC_LINEAR_SPEED_SQUARED =
        KINEMATIC_LINEAR_SPEED_THRESHOLD * KINEMATIC_LINEAR_SPEED_THRESHOLD;
    if (isTranslating) {
        glm::vec3 deltaVelocity = Vectors::ZERO;

        // linear damping
        float damping = getDamping();
        if (damping > 0.0f) {
            deltaVelocity = (powf(1.0f - damping, timeElapsed) - 1.0f) * linearVelocity;
        }

        const float MIN_KINEMATIC_LINEAR_ACCELERATION_SQUARED = 1.0e-4f; // 0.01 m/sec^2
        vec3 acceleration = getAcceleration();
        if (glm::length2(acceleration) > MIN_KINEMATIC_LINEAR_ACCELERATION_SQUARED) {
            // yes acceleration
            // acceleration is in world-frame but we need it in local-frame
            glm::vec3 linearAcceleration = acceleration;
            bool success;
            Transform parentTransform = getParentTransform(success);
            if (success) {
                linearAcceleration = glm::inverse(parentTransform.getRotation()) * linearAcceleration;
            }
            deltaVelocity += linearAcceleration * timeElapsed;

            if (linearSpeedSquared < MIN_KINEMATIC_LINEAR_SPEED_SQUARED
                    && glm::length2(deltaVelocity) < MIN_KINEMATIC_LINEAR_SPEED_SQUARED
                    && glm::length2(linearVelocity + deltaVelocity) < MIN_KINEMATIC_LINEAR_SPEED_SQUARED) {
                linearVelocity = Vectors::ZERO;
            } else {
                // NOTE: we do NOT include the second-order acceleration term (0.5 * a * dt^2)
                // when computing the displacement because Bullet also ignores that term.  Yes,
                // this is an approximation and it works best when dt is small.
                position += timeElapsed * linearVelocity;
                linearVelocity += deltaVelocity;
            }
        } else {
            // no acceleration
            if (linearSpeedSquared < MIN_KINEMATIC_LINEAR_SPEED_SQUARED) {
                linearVelocity = Vectors::ZERO;
            } else {
                // NOTE: we don't use second-order acceleration term for linear displacement
                // because Bullet doesn't use it.
                position += timeElapsed * linearVelocity;
                linearVelocity += deltaVelocity;
            }
        }
    }

    transform.setTranslation(position);
    setLocalTransformAndVelocities(transform, linearVelocity, angularVelocity);

    return true;
}

bool EntityItem::isMoving() const {
    return hasVelocity() || hasAngularVelocity();
}

bool EntityItem::isMovingRelativeToParent() const {
    return hasLocalVelocity() || hasLocalAngularVelocity();
}

EntityTreePointer EntityItem::getTree() const {
    EntityTreeElementPointer containingElement = getElement();
    EntityTreePointer tree = containingElement ? containingElement->getTree() : nullptr;
    return tree;
}

SpatialParentTree* EntityItem::getParentTree() const {
    return getTree().get();
}

bool EntityItem::wantTerseEditLogging() const {
    EntityTreePointer tree = getTree();
    return tree ? tree->wantTerseEditLogging() : false;
}

glm::mat4 EntityItem::getEntityToWorldMatrix() const {
    glm::mat4 translation = glm::translate(getWorldPosition());
    glm::mat4 rotation = glm::mat4_cast(getWorldOrientation());
    glm::mat4 scale = glm::scale(getScaledDimensions());
    glm::mat4 registration = glm::translate(ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - getRegistrationPoint());
    return translation * rotation * scale * registration;
}

glm::mat4 EntityItem::getWorldToEntityMatrix() const {
    return glm::inverse(getEntityToWorldMatrix());
}

glm::vec3 EntityItem::entityToWorld(const glm::vec3& point) const {
    return glm::vec3(getEntityToWorldMatrix() * glm::vec4(point, 1.0f));
}

glm::vec3 EntityItem::worldToEntity(const glm::vec3& point) const {
    return glm::vec3(getWorldToEntityMatrix() * glm::vec4(point, 1.0f));
}

bool EntityItem::lifetimeHasExpired() const {
    return isMortal() && (getAge() > getLifetime());
}

quint64 EntityItem::getExpiry() const {
    return getCreated() + (quint64)(getLifetime() * (float)USECS_PER_SECOND);
}

EntityItemProperties EntityItem::getProperties(const EntityPropertyFlags& desiredProperties, bool allowEmptyDesiredProperties) const {
    EncodeBitstreamParams params; // unknown
    const EntityPropertyFlags propertyFlags = !allowEmptyDesiredProperties && desiredProperties.isEmpty() ?
        getEntityProperties(params) : desiredProperties;
    EntityItemProperties properties(propertyFlags);
    properties._id = getID();
    properties._idSet = true;
    properties._lastEdited = getLastEdited();

    properties._type = getType();

    // Core
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(simulationOwner, getSimulationOwner);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(parentID, getParentID);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(parentJointIndex, getParentJointIndex);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(visible, getVisible);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(name, getName);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(locked, getLocked);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(userData, getUserData);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(privateUserData, getPrivateUserData);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(href, getHref);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(description, getDescription);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(position, getLocalPosition);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(dimensions, getScaledDimensions);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(rotation, getLocalOrientation);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(registrationPoint, getRegistrationPoint);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(created, getCreated);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(lastEditedBy, getLastEditedBy);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(entityHostType, getEntityHostType);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(owningAvatarID, getOwningAvatarIDForProperties);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(queryAACube, getQueryAACube);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(canCastShadow, getCanCastShadow);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(isVisibleInSecondaryCamera, isVisibleInSecondaryCamera);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(renderLayer, getRenderLayer);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(primitiveMode, getPrimitiveMode);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(ignorePickIntersection, getIgnorePickIntersection);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(renderWithZones, getRenderWithZones);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(billboardMode, getBillboardMode);
    withReadLock([&] {
        _grabProperties.getProperties(properties);
    });

    // Physics
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(density, getDensity);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(velocity, getLocalVelocity);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(angularVelocity, getLocalAngularVelocity);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(gravity, getGravity);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(acceleration, getAcceleration);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(damping, getDamping);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(angularDamping, getAngularDamping);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(restitution, getRestitution);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(friction, getFriction);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(lifetime, getLifetime);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(collisionless, getCollisionless);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(collisionMask, getCollisionMask);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(dynamic, getDynamic);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(collisionSoundURL, getCollisionSoundURL);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(actionData, getDynamicData);

    // Cloning
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneable, getCloneable);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneLifetime, getCloneLifetime);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneLimit, getCloneLimit);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneDynamic, getCloneDynamic);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneAvatarEntity, getCloneAvatarEntity);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(cloneOriginID, getCloneOriginID);

    // Scripts
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(script, getScript);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(scriptTimestamp, getScriptTimestamp);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(serverScripts, getServerScripts);

    // Script local data
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(localPosition, getLocalPosition);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(localRotation, getLocalOrientation);
    // FIXME: are these needed?
    //COPY_ENTITY_PROPERTY_TO_PROPERTIES(localVelocity, getLocalVelocity);
    //COPY_ENTITY_PROPERTY_TO_PROPERTIES(localAngularVelocity, getLocalAngularVelocity);
    //COPY_ENTITY_PROPERTY_TO_PROPERTIES(localDimensions, getLocalDimensions);

    properties._defaultSettings = false;

    return properties;
}

void EntityItem::getTransformAndVelocityProperties(EntityItemProperties& properties) const {
    if (!properties._positionChanged) {
        properties._position = getLocalPosition();
    }
    if (!properties._velocityChanged) {
        properties._velocity = getLocalVelocity();
    }
    if (!properties._rotationChanged) {
        properties._rotation = getLocalOrientation();
    }
    if (!properties._angularVelocityChanged) {
        properties._angularVelocity = getLocalAngularVelocity();
    }
    if (!properties._accelerationChanged) {
        properties._acceleration = getAcceleration();
    }

    properties._positionChanged = true;
    properties._velocityChanged = true;
    properties._rotationChanged = true;
    properties._angularVelocityChanged = true;
    properties._accelerationChanged = true;
}

void EntityItem::upgradeScriptSimulationPriority(uint8_t priority) {
    uint8_t newPriority = glm::max(priority, _scriptSimulationPriority);
    if (newPriority < SCRIPT_GRAB_SIMULATION_PRIORITY && stillHasMyGrab()) {
        newPriority = SCRIPT_GRAB_SIMULATION_PRIORITY;
    }
    if (newPriority != _scriptSimulationPriority) {
        // set the dirty flag to trigger a bid or ownership update
        markDirtyFlags(Simulation::DIRTY_SIMULATION_OWNERSHIP_PRIORITY);
        _scriptSimulationPriority = newPriority;
    }
}

void EntityItem::clearScriptSimulationPriority() {
    // DO NOT markDirtyFlags(Simulation::DIRTY_SIMULATION_OWNERSHIP_PRIORITY) here, because this
    // is only ever called from the code that actually handles the dirty flags, and it knows best.
    _scriptSimulationPriority = stillHasMyGrab() ? SCRIPT_GRAB_SIMULATION_PRIORITY : 0;
}

void EntityItem::setPendingOwnershipPriority(uint8_t priority) {
    _pendingOwnershipTimestamp = usecTimestampNow();
    _pendingOwnershipPriority = priority;
    _pendingOwnershipState = (_pendingOwnershipPriority == 0) ? PENDING_STATE_RELEASE : PENDING_STATE_TAKE;
}

bool EntityItem::pendingRelease(uint64_t timestamp) const {
    return _pendingOwnershipPriority == 0 &&
        _pendingOwnershipState == PENDING_STATE_RELEASE &&
        _pendingOwnershipTimestamp >= timestamp;
}

bool EntityItem::stillWaitingToTakeOwnership(uint64_t timestamp) const {
    return _pendingOwnershipPriority > 0 &&
        _pendingOwnershipState == PENDING_STATE_TAKE &&
        _pendingOwnershipTimestamp >= timestamp;
}

bool EntityItem::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    // Core
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(simulationOwner, setSimulationOwner);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(parentID, setParentID);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(parentJointIndex, setParentJointIndex);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(visible, setVisible);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(name, setName);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(locked, setLocked);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(userData, setUserData);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(privateUserData, setPrivateUserData);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(href, setHref);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(description, setDescription);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(position, setPosition);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(dimensions, setScaledDimensions);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(rotation, setRotation);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(registrationPoint, setRegistrationPoint);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(created, setCreated);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(lastEditedBy, setLastEditedBy);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(entityHostType, setEntityHostType);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(owningAvatarID, setOwningAvatarID);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(queryAACube, setQueryAACube);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(canCastShadow, setCanCastShadow);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(isVisibleInSecondaryCamera, setIsVisibleInSecondaryCamera);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(renderLayer, setRenderLayer);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(primitiveMode, setPrimitiveMode);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(ignorePickIntersection, setIgnorePickIntersection);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(renderWithZones, setRenderWithZones);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(billboardMode, setBillboardMode);
    withWriteLock([&] {
        bool grabPropertiesChanged = _grabProperties.setProperties(properties);
        somethingChanged |= grabPropertiesChanged;
    });

    // Physics
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(density, setDensity);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(velocity, setVelocity);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(angularVelocity, setAngularVelocity);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(gravity, setGravity);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(acceleration, setAcceleration);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(damping, setDamping);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(angularDamping, setAngularDamping);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(restitution, setRestitution);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(friction, setFriction);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(lifetime, setLifetime);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(collisionless, setCollisionless);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(collisionMask, setCollisionMask);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(dynamic, setDynamic);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(collisionSoundURL, setCollisionSoundURL);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(actionData, setDynamicData);

    // Cloning
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneable, setCloneable);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneLifetime, setCloneLifetime);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneLimit, setCloneLimit);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneDynamic, setCloneDynamic);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneAvatarEntity, setCloneAvatarEntity);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(cloneOriginID, setCloneOriginID);

    // Scripts
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(script, setScript);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(scriptTimestamp, setScriptTimestamp);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(serverScripts, setServerScripts);

    if (updateQueryAACube()) {
        somethingChanged = true;
    }

    // Now check the sub classes
    somethingChanged |= setSubClassProperties(properties);

    // Finally notify if change detected
    if (somethingChanged) {
        uint64_t now = usecTimestampNow();
        #ifdef WANT_DEBUG
            int elapsed = now - getLastEdited();
            qCDebug(entities) << "EntityItem::setProperties() AFTER update... edited AGO=" << elapsed <<
                    "now=" << now << " getLastEdited()=" << getLastEdited();
        #endif
        setLastEdited(properties._lastEdited);
        if (getDirtyFlags() & (Simulation::DIRTY_TRANSFORM | Simulation::DIRTY_VELOCITIES)) {
            // anything that sets the transform or velocity must update _lastSimulated which is used
            // for kinematic extrapolation (e.g. we want to extrapolate forward from this moment
            // when position and/or velocity was changed).
            _lastSimulated = now;
        }
        somethingChangedNotification();
    }

    // timestamps
    quint64 timestamp = properties.getCreated();
    if (_created == UNKNOWN_CREATED_TIME && timestamp != UNKNOWN_CREATED_TIME) {
        quint64 now = usecTimestampNow();
        if (timestamp > now) {
            timestamp = now;
        }
        _created = timestamp;
    }

    return somethingChanged;
}

void EntityItem::recordCreationTime() {
    if (_created == UNKNOWN_CREATED_TIME) {
        _created = usecTimestampNow();
    }
    auto now = usecTimestampNow();
    _lastEdited = _created;
    _lastUpdated = now;
    _lastSimulated = now;
}

const Transform EntityItem::getTransformToCenter(bool& success) const {
    Transform result = getTransform(success);
    glm::vec3 pivot = getPivot();
    if (pivot != ENTITY_ITEM_ZERO_VEC3) {
        result.postTranslate(pivot);
    }
    glm::vec3 registrationPoint = getRegistrationPoint();
    if (registrationPoint != ENTITY_ITEM_HALF_VEC3) { // If it is not already centered, translate to center
        result.postTranslate((ENTITY_ITEM_HALF_VEC3 - registrationPoint) * getScaledDimensions()); // Position to center
    }
    return result;
}

const Transform EntityItem::getTransformToCenterWithOnlyLocalRotation(bool& success) const {
    Transform result = getTransformWithOnlyLocalRotation(success);
    glm::vec3 pivot = getPivot();
    if (pivot != ENTITY_ITEM_ZERO_VEC3) {
        result.postTranslate(pivot);
    }
    glm::vec3 registrationPoint = getRegistrationPoint();
    if (registrationPoint != ENTITY_ITEM_HALF_VEC3) { // If it is not already centered, translate to center
        result.postTranslate((ENTITY_ITEM_HALF_VEC3 - registrationPoint) * getScaledDimensions()); // Position to center
    }
    return result;
}

/// The maximum bounding cube for the entity, independent of it's rotation.
/// This accounts for the registration point (upon which rotation occurs around).
///
AACube EntityItem::getMaximumAACube(bool& success) const {
    if (_recalcMaxAACube) {
        glm::vec3 centerOfRotation = getWorldPosition(success); // also where _registration point is
        if (success) {
            _recalcMaxAACube = false;
            // we want to compute the furthestExtent that an entity can extend out from its "position"
            // to do this we compute the max of these two vec3s: registration and 1-registration
            // and then scale by dimensions and add the absolute value of the pivot
            glm::vec3 registrationPoint = getRegistrationPoint();
            glm::vec3 maxExtents = getScaledDimensions() * glm::max(registrationPoint, glm::vec3(1.0f) - registrationPoint);

            // there exists a sphere that contains maxExtents for all rotations
            float radius = glm::length(maxExtents);

            // put a cube around the sphere
            // TODO? replace _maxAACube with _boundingSphereRadius
            glm::vec3 minimumCorner = (centerOfRotation + getWorldOrientation() * getPivot()) - glm::vec3(radius, radius, radius);
            _maxAACube = AACube(minimumCorner, radius * 2.0f);
        }
    } else {
        success = true;
    }
    return _maxAACube;
}

/// The minimum bounding cube for the entity accounting for it's rotation.
/// This accounts for the registration point (upon which rotation occurs around).
///
AACube EntityItem::getMinimumAACube(bool& success) const {
    if (_recalcMinAACube) {
        // position represents the position of the registration point.
        glm::vec3 position = getWorldPosition(success);
        if (success) {
            _recalcMinAACube = false;
            glm::vec3 dimensions = getScaledDimensions();
            glm::vec3 registrationPoint = getRegistrationPoint();
            glm::vec3 pivot = getPivot();
            glm::vec3 unrotatedMinRelativeToEntity = -(dimensions * registrationPoint);
            glm::vec3 unrotatedMaxRelativeToEntity = dimensions * (ENTITY_ITEM_ONE_VEC3 - registrationPoint);
            Extents extents = { unrotatedMinRelativeToEntity, unrotatedMaxRelativeToEntity };
            extents.shiftBy(pivot);
            extents.rotate(getWorldOrientation());

            // shift the extents to be relative to the position/registration point
            extents.shiftBy(position);

            // the cube that best encompasses extents is...
            AABox box(extents);
            glm::vec3 centerOfBox = box.calcCenter();
            float longestSide = box.getLargestDimension();
            float halfLongestSide = longestSide / 2.0f;
            glm::vec3 cornerOfCube = centerOfBox - glm::vec3(halfLongestSide, halfLongestSide, halfLongestSide);

            _minAACube = AACube(cornerOfCube, longestSide);
        }
    } else {
        success = true;
    }
    return _minAACube;
}

AABox EntityItem::getAABox(bool& success) const {
    if (_recalcAABox) {
        // position represents the position of the registration point.
        glm::vec3 position = getWorldPosition(success);
        if (success) {
            _recalcAABox = false;
            glm::vec3 dimensions = getScaledDimensions();
            glm::vec3 registrationPoint = getRegistrationPoint();
            glm::vec3 pivot = getPivot();
            glm::vec3 unrotatedMinRelativeToEntity = -(dimensions * registrationPoint);
            glm::vec3 unrotatedMaxRelativeToEntity = dimensions * (ENTITY_ITEM_ONE_VEC3 - registrationPoint);
            Extents extents = { unrotatedMinRelativeToEntity, unrotatedMaxRelativeToEntity };
            extents.shiftBy(pivot);
            extents.rotate(getWorldOrientation());

            // shift the extents to be relative to the position/registration point
            extents.shiftBy(position);

            _cachedAABox = AABox(extents);
        }
    } else {
        success = true;
    }
    return _cachedAABox;
}

AACube EntityItem::getQueryAACube(bool& success) const {
    AACube result = SpatiallyNestable::getQueryAACube(success);
    if (success) {
        return result;
    }
    // this is for when we've loaded an older json file that didn't have queryAACube properties.
    result = getMaximumAACube(success);
    if (success) {
        _queryAACube = result;
        _queryAACubeSet = true;
    }
    return result;
}

bool EntityItem::shouldPuffQueryAACube() const {
    return hasActions() || isChildOfMyAvatar() || isMovingRelativeToParent();
}

// TODO: get rid of all users of this function...
//    ... radius = cornerToCornerLength / 2.0f
//    ... cornerToCornerLength = sqrt(3 x maxDimension ^ 2)
//    ... radius = sqrt(3 x maxDimension ^ 2) / 2.0f;
float EntityItem::getRadius() const {
    return 0.5f * glm::length(getScaledDimensions());
}

void EntityItem::adjustShapeInfoByRegistration(ShapeInfo& info, bool includePivot) const {
    glm::vec3 offset;
    glm::vec3 registrationPoint = getRegistrationPoint();
    if (registrationPoint != ENTITY_ITEM_DEFAULT_REGISTRATION_POINT) {
        offset += (ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - registrationPoint) * getScaledDimensions();
    }

    if (includePivot) {
        glm::vec3 pivot = getPivot();
        if (pivot != ENTITY_ITEM_ZERO_VEC3) {
            offset += pivot;
        }
    }

    if (offset != ENTITY_ITEM_ZERO_VEC3) {
        info.setOffset(offset);
    }
}

bool EntityItem::contains(const glm::vec3& point) const {
    ShapeType shapeType = getShapeType();

    if (shapeType == SHAPE_TYPE_SPHERE) {
        glm::vec3 dimensions = getScaledDimensions();
        if (dimensions.x == dimensions.y && dimensions.y == dimensions.z) {
            glm::vec3 localPoint = point - (getWorldPosition() + getWorldOrientation() * (dimensions * (ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - getRegistrationPoint()) + getPivot()));
            return glm::length2(localPoint) < glm::length2(0.5f * dimensions.x);
        }
    }

    // we transform into the "normalized entity-frame" where the bounding box is centered on the origin
    // and has dimensions <1,1,1>
    glm::vec3 localPoint = glm::vec3(glm::inverse(getEntityToWorldMatrix()) * glm::vec4(point, 1.0f));

    const float NORMALIZED_HALF_SIDE = 0.5f;
    const float NORMALIZED_RADIUS_SQUARED = NORMALIZED_HALF_SIDE * NORMALIZED_HALF_SIDE;
    switch(shapeType) {
        case SHAPE_TYPE_NONE:
            return false;
        case SHAPE_TYPE_CAPSULE_X:
        case SHAPE_TYPE_CAPSULE_Y:
        case SHAPE_TYPE_CAPSULE_Z:
        case SHAPE_TYPE_HULL:
        case SHAPE_TYPE_PLANE:
        case SHAPE_TYPE_COMPOUND:
        case SHAPE_TYPE_SIMPLE_HULL:
        case SHAPE_TYPE_SIMPLE_COMPOUND:
        case SHAPE_TYPE_STATIC_MESH:
        case SHAPE_TYPE_CIRCLE:
        // the above cases not yet supported --> fall through to BOX case
        case SHAPE_TYPE_BOX: {
            localPoint = glm::abs(localPoint);
            return glm::all(glm::lessThanEqual(localPoint, glm::vec3(NORMALIZED_HALF_SIDE)));
        }
        case SHAPE_TYPE_SPHERE:
        case SHAPE_TYPE_ELLIPSOID: {
            // since we've transformed into the normalized space this is just a sphere-point intersection test
            return glm::length2(localPoint) <= NORMALIZED_RADIUS_SQUARED;
        }
        case SHAPE_TYPE_CYLINDER_X:
            return fabsf(localPoint.x) <= NORMALIZED_HALF_SIDE &&
                localPoint.y * localPoint.y + localPoint.z * localPoint.z <= NORMALIZED_RADIUS_SQUARED;
        case SHAPE_TYPE_CYLINDER_Y:
            return fabsf(localPoint.y) <= NORMALIZED_HALF_SIDE &&
                localPoint.z * localPoint.z + localPoint.x * localPoint.x <= NORMALIZED_RADIUS_SQUARED;
        case SHAPE_TYPE_CYLINDER_Z:
            return fabsf(localPoint.z) <= NORMALIZED_HALF_SIDE &&
                localPoint.x * localPoint.x + localPoint.y * localPoint.y <= NORMALIZED_RADIUS_SQUARED;
        default:
            return false;
    }
}

void EntityItem::computeShapeInfo(ShapeInfo& info) {
    info.setParams(getShapeType(), 0.5f * getScaledDimensions());
    adjustShapeInfoByRegistration(info);
}

float EntityItem::getVolumeEstimate() const {
    glm::vec3 dimensions = getScaledDimensions();
    return dimensions.x * dimensions.y * dimensions.z;
}

void EntityItem::setRegistrationPoint(const glm::vec3& value) {
    bool changed = false;
    withWriteLock([&] {
        if (value != _registrationPoint) {
            _registrationPoint = glm::clamp(value, glm::vec3(ENTITY_ITEM_MIN_REGISTRATION_POINT), 
                                                   glm::vec3(ENTITY_ITEM_MAX_REGISTRATION_POINT));
            changed = true;
        }
    });

    if (changed) {
        dimensionsChanged(); // Registration Point affects the bounding box
        markDirtyFlags(Simulation::DIRTY_SHAPE);
    }
}

void EntityItem::setPosition(const glm::vec3& value) {
    if (getLocalPosition() != value) {
        setLocalPosition(value);

        EntityTreePointer tree = getTree();
        markDirtyFlags(Simulation::DIRTY_POSITION);
        if (tree) {
            tree->entityChanged(getThisPointer());
        }
        forEachDescendant([&](SpatiallyNestablePointer object) {
            if (object->getNestableType() == NestableType::Entity) {
                EntityItemPointer entity = std::static_pointer_cast<EntityItem>(object);
                entity->markDirtyFlags(Simulation::DIRTY_POSITION);
                if (tree) {
                    tree->entityChanged(entity);
                }
            }
        });
    }
}

void EntityItem::setParentID(const QUuid& value) {
    QUuid oldParentID = getParentID();
    if (oldParentID != value) {
        _needsRenderUpdate = true;
        EntityTreePointer tree = getTree();
        if (tree && !oldParentID.isNull()) {
            tree->removeFromChildrenOfAvatars(getThisPointer());
        }

        uint32_t oldParentNoBootstrapping = 0;
        uint32_t newParentNoBootstrapping = 0;
        if (!value.isNull() && tree) {
            EntityItemPointer entity = tree->findEntityByEntityItemID(value);
            if (entity) {
                newParentNoBootstrapping = entity->getSpecialFlags() & Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
            }
        }

        if (!oldParentID.isNull() && tree) {
            EntityItemPointer entity = tree->findEntityByEntityItemID(oldParentID);
            if (entity) {
                oldParentNoBootstrapping = entity->getDirtyFlags() & Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
            }
        }

        if (!value.isNull() && (value == Physics::getSessionUUID() || value == AVATAR_SELF_ID)) {
            newParentNoBootstrapping |= Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
        }

        if (!oldParentID.isNull() && (oldParentID == Physics::getSessionUUID() || oldParentID == AVATAR_SELF_ID)) {
            oldParentNoBootstrapping |= Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
        }

        if ((bool)(oldParentNoBootstrapping ^ newParentNoBootstrapping)) {
            if ((bool)(newParentNoBootstrapping & Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING)) {
                markSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
                forEachDescendant([&](SpatiallyNestablePointer object) {
                        if (object->getNestableType() == NestableType::Entity) {
                            EntityItemPointer entity = std::static_pointer_cast<EntityItem>(object);
                            entity->markDirtyFlags(Simulation::DIRTY_COLLISION_GROUP);
                            entity->markSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
                        }
                });
            } else {
                clearSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
                forEachDescendant([&](SpatiallyNestablePointer object) {
                        if (object->getNestableType() == NestableType::Entity) {
                            EntityItemPointer entity = std::static_pointer_cast<EntityItem>(object);
                            entity->markDirtyFlags(Simulation::DIRTY_COLLISION_GROUP);
                            entity->clearSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
                        }
                });
            }
        }

        SpatiallyNestable::setParentID(value);
        // children are forced to be kinematic
        // may need to not collide with own avatar
        markDirtyFlags(Simulation::DIRTY_MOTION_TYPE | Simulation::DIRTY_COLLISION_GROUP);

        if (tree) {
            tree->addToNeedsParentFixupList(getThisPointer());
        }
        updateQueryAACube();
    }
}

glm::vec3 EntityItem::getScaledDimensions() const {
    glm::vec3 scale = getSNScale();
    return getUnscaledDimensions() * scale;
}

void EntityItem::setScaledDimensions(const glm::vec3& value) {
    glm::vec3 parentScale = getSNScale();
    setUnscaledDimensions(value / parentScale);
}

void EntityItem::setUnscaledDimensions(const glm::vec3& value) {
    glm::vec3 newDimensions = glm::max(value, glm::vec3(ENTITY_ITEM_MIN_DIMENSION));
    const float MIN_SCALE_CHANGE_SQUARED = 1.0e-6f;
    if (glm::length2(getUnscaledDimensions() - newDimensions) > MIN_SCALE_CHANGE_SQUARED) {
        withWriteLock([&] {
            _unscaledDimensions = newDimensions;
            _flags |= (Simulation::DIRTY_SHAPE | Simulation::DIRTY_MASS);
            _queryAACubeSet = false;
        });
        locationChanged();
        dimensionsChanged();
    }
}

glm::vec3 EntityItem::getUnscaledDimensions() const {
   return resultWithReadLock<glm::vec3>([&] {
        return _unscaledDimensions;
    });
}

void EntityItem::setRotation(glm::quat rotation) {
    if (getLocalOrientation() != rotation) {
        setLocalOrientation(rotation);
        _flags |= Simulation::DIRTY_ROTATION;
        forEachDescendant([&](SpatiallyNestablePointer object) {
            if (object->getNestableType() == NestableType::Entity) {
                EntityItemPointer entity = std::static_pointer_cast<EntityItem>(object);
                entity->markDirtyFlags(Simulation::DIRTY_ROTATION | Simulation::DIRTY_POSITION);
            }
        });
    }
}

void EntityItem::setVelocity(const glm::vec3& value) {
    glm::vec3 velocity = getLocalVelocity();
    if (velocity != value) {
        float speed = glm::length(value);
        if (!glm::isnan(speed)) {
            const float MIN_LINEAR_SPEED = 0.001f;
            const float MAX_LINEAR_SPEED = 270.0f; // 3m per step at 90Hz
            if (speed < MIN_LINEAR_SPEED) {
                velocity = ENTITY_ITEM_ZERO_VEC3;
            } else if (speed > MAX_LINEAR_SPEED) {
                velocity = (MAX_LINEAR_SPEED / speed) * value;
            } else {
                velocity = value;
            }
            setLocalVelocity(velocity);
            _flags |= Simulation::DIRTY_LINEAR_VELOCITY;
        }
    }
}

void EntityItem::setDamping(float value) {
    auto clampedDamping = glm::clamp(value, ENTITY_ITEM_MIN_DAMPING, ENTITY_ITEM_MAX_DAMPING);
    withWriteLock([&] {
        if (_damping != clampedDamping) {
            _damping = clampedDamping;
            _flags |= Simulation::DIRTY_MATERIAL;
        }
    });
}

void EntityItem::setGravity(const glm::vec3& value) {
    withWriteLock([&] {
        if (_gravity != value) {
            float magnitude = glm::length(value);
            if (!glm::isnan(magnitude)) {
                const float MAX_ACCELERATION_OF_GRAVITY = 10.0f * 9.8f; // 10g
                if (magnitude > MAX_ACCELERATION_OF_GRAVITY) {
                    _gravity = (MAX_ACCELERATION_OF_GRAVITY / magnitude) * value;
                } else {
                    _gravity = value;
                }
                _flags |= Simulation::DIRTY_LINEAR_VELOCITY;
            }
        }
    });
}

void EntityItem::setAngularVelocity(const glm::vec3& value) {
    glm::vec3 angularVelocity = getLocalAngularVelocity();
    if (angularVelocity != value) {
        float speed = glm::length(value);
        if (!glm::isnan(speed)) {
            const float MIN_ANGULAR_SPEED = 0.0002f;
            const float MAX_ANGULAR_SPEED = 9.0f * TWO_PI; // 1/10 rotation per step at 90Hz
            if (speed < MIN_ANGULAR_SPEED) {
                angularVelocity = ENTITY_ITEM_ZERO_VEC3;
            } else if (speed > MAX_ANGULAR_SPEED) {
                angularVelocity = (MAX_ANGULAR_SPEED / speed) * value;
            } else {
                angularVelocity = value;
            }
            setLocalAngularVelocity(angularVelocity);
            _flags |= Simulation::DIRTY_ANGULAR_VELOCITY;
        }
    }
}

void EntityItem::setAngularDamping(float value) {
    auto clampedDamping = glm::clamp(value, ENTITY_ITEM_MIN_DAMPING, ENTITY_ITEM_MAX_DAMPING);
    withWriteLock([&] {
        if (_angularDamping != clampedDamping) {
            _angularDamping = clampedDamping;
            _flags |= Simulation::DIRTY_MATERIAL;
        }
    });
}

void EntityItem::setCollisionless(bool value) {
    withWriteLock([&] {
        if (_collisionless != value) {
            _collisionless = value;
            _flags |= Simulation::DIRTY_COLLISION_GROUP;
        }
    });
}

void EntityItem::setCollisionMask(uint16_t value) {
    withWriteLock([&] {
        if ((_collisionMask & ENTITY_COLLISION_MASK_DEFAULT) != (value & ENTITY_COLLISION_MASK_DEFAULT)) {
            _collisionMask = (value & ENTITY_COLLISION_MASK_DEFAULT);
            _flags |= Simulation::DIRTY_COLLISION_GROUP;
        }
    });
}

void EntityItem::setDynamic(bool value) {
    if (getDynamic() != value) {
        auto shapeType = getShapeType();
        withWriteLock([&] {
            // dynamic and STATIC_MESH are incompatible so we check for that case
            if (value && shapeType == SHAPE_TYPE_STATIC_MESH) {
                if (_dynamic) {
                    _dynamic = false;
                    _flags |= Simulation::DIRTY_MOTION_TYPE;
                }
            } else {
                _dynamic = value;
                _flags |= Simulation::DIRTY_MOTION_TYPE;
            }
        });
    }
}

void EntityItem::setRestitution(float value) {
    float clampedValue = glm::max(glm::min(ENTITY_ITEM_MAX_RESTITUTION, value), ENTITY_ITEM_MIN_RESTITUTION);
    withWriteLock([&] {
        if (_restitution != clampedValue) {
            _restitution = clampedValue;
            _flags |= Simulation::DIRTY_MATERIAL;
        }
    });

}

void EntityItem::setFriction(float value) {
    float clampedValue = glm::max(glm::min(ENTITY_ITEM_MAX_FRICTION, value), ENTITY_ITEM_MIN_FRICTION);
    withWriteLock([&] {
        if (_friction != clampedValue) {
            _friction = clampedValue;
            _flags |= Simulation::DIRTY_MATERIAL;
        }
    });
}

void EntityItem::setLifetime(float value) {
    withWriteLock([&] {
        if (_lifetime != value) {
            _lifetime = value;
            _flags |= Simulation::DIRTY_LIFETIME;
        }
    });
}

void EntityItem::setCreated(quint64 value) {
    withWriteLock([&] {
        if (_created != value) {
            _created = value;
            _flags |= Simulation::DIRTY_LIFETIME;
        }
    });
}

void EntityItem::computeCollisionGroupAndFinalMask(int32_t& group, int32_t& mask) const {
    if (_collisionless) {
        group = BULLET_COLLISION_GROUP_COLLISIONLESS;
        mask = 0;
    } else {
        if (getDynamic()) {
            group = BULLET_COLLISION_GROUP_DYNAMIC;
        } else if (hasActions() || isMovingRelativeToParent()) {
            group = BULLET_COLLISION_GROUP_KINEMATIC;
        } else {
            group = BULLET_COLLISION_GROUP_STATIC;
        }

        uint16_t userMask = getCollisionMask();

        if ((bool)(userMask & USER_COLLISION_GROUP_MY_AVATAR) !=
                (bool)(userMask & USER_COLLISION_GROUP_OTHER_AVATAR)) {
            // asymmetric avatar collision mask bits
            if (!getSimulatorID().isNull() && getSimulatorID() != Physics::getSessionUUID()) {
                // someone else owns the simulation, so we toggle the avatar bits (swap interpretation)
                userMask ^= USER_COLLISION_MASK_AVATARS | ~userMask;
            }
        }

        if ((bool)(_flags & Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING)) {
            userMask &= ~USER_COLLISION_GROUP_MY_AVATAR;
        }
        mask = Physics::getDefaultCollisionMask(group) & (int32_t)(userMask);
    }
}

void EntityItem::setSimulationOwner(const QUuid& id, uint8_t priority) {
    if (wantTerseEditLogging() && (id != _simulationOwner.getID() || priority != _simulationOwner.getPriority())) {
        qCDebug(entities) << "sim ownership for" << getDebugName() << "is now" << id << priority;
    }
    _simulationOwner.set(id, priority);
}

void EntityItem::setSimulationOwner(const SimulationOwner& owner) {
    // NOTE: this method only used by EntityServer.  The Interface uses special code in readEntityDataFromBuffer().
    if (wantTerseEditLogging() && _simulationOwner != owner) {
        qCDebug(entities) << "sim ownership for" << getDebugName() << "is now" << owner;
    }

    if (_simulationOwner.set(owner)) {
        markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);
    }
}

void EntityItem::clearSimulationOwnership() {
    if (wantTerseEditLogging() && !_simulationOwner.isNull()) {
        qCDebug(entities) << "sim ownership for" << getDebugName() << "is now null";
    }

    _simulationOwner.clear();
    // don't bother setting the DIRTY_SIMULATOR_ID flag because:
    // (a) when entity-server calls clearSimulationOwnership() the dirty-flags are meaningless (only used by interface)
    // (b) the interface only calls clearSimulationOwnership() in a context that already knows best about dirty flags
    //markDirtyFlags(Simulation::DIRTY_SIMULATOR_ID);

}

QString EntityItem::actionsToDebugString() {
    QString result;
    QVector<QByteArray> serializedActions;
    QHash<QUuid, EntityDynamicPointer>::const_iterator i = _objectActions.begin();
    while (i != _objectActions.end()) {
        const QUuid id = i.key();
        EntityDynamicPointer action = _objectActions[id];
        EntityDynamicType actionType = action->getType();
        result += QString("") + actionType + ":" + action->getID().toString() + " ";
        i++;
    }
    return result;
}

bool EntityItem::addAction(EntitySimulationPointer simulation, EntityDynamicPointer action) {
    bool result;
    withWriteLock([&] {
        checkWaitingToRemove(simulation);

        result = addActionInternal(simulation, action);
        if (result) {
            action->setIsMine(true);
            _dynamicDataDirty = true;
        } else {
            removeActionInternal(action->getID());
        }
    });
    updateQueryAACube();

    return result;
}

void EntityItem::enableNoBootstrap() {
    if (!(bool)(_flags & Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING)) {
        _flags |= Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
        _flags |= Simulation::DIRTY_COLLISION_GROUP; // may need to not collide with own avatar

        // NOTE: unlike disableNoBootstrap() below, we do not call simulation->changeEntity() here
        // because most enableNoBootstrap() cases are already correctly handled outside this scope
        // and I didn't want to add redundant work.
        // TODO: cleanup Grabs & dirtySimulationFlags to be more efficient and make more sense.

        forEachDescendant([&](SpatiallyNestablePointer child) {
            if (child->getNestableType() == NestableType::Entity) {
                EntityItemPointer entity = std::static_pointer_cast<EntityItem>(child);
                entity->markDirtyFlags(Simulation::DIRTY_COLLISION_GROUP);
                entity->markSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
            }
        });
    }
}

void EntityItem::disableNoBootstrap() {
    if (!stillHasMyGrab()) {
        _flags &= ~Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING;
        _flags |= Simulation::DIRTY_COLLISION_GROUP; // may need to not collide with own avatar

        EntityTreePointer entityTree = getTree();
        assert(entityTree);
        EntitySimulationPointer simulation = entityTree->getSimulation();
        assert(simulation);
        simulation->changeEntity(getThisPointer());

        forEachDescendant([&](SpatiallyNestablePointer child) {
            if (child->getNestableType() == NestableType::Entity) {
                EntityItemPointer entity = std::static_pointer_cast<EntityItem>(child);
                entity->markDirtyFlags(Simulation::DIRTY_COLLISION_GROUP);
                entity->clearSpecialFlags(Simulation::SPECIAL_FLAG_NO_BOOTSTRAPPING);
                simulation->changeEntity(entity);
            }
        });
    }
}


bool EntityItem::addActionInternal(EntitySimulationPointer simulation, EntityDynamicPointer action) {
    assert(action);
    assert(simulation);
    auto actionOwnerEntity = action->getOwnerEntity().lock();
    assert(actionOwnerEntity);
    assert(actionOwnerEntity.get() == this);

    const QUuid& actionID = action->getID();
    assert(!_objectActions.contains(actionID) || _objectActions[actionID] == action);
    _objectActions[actionID] = action;
    simulation->addDynamic(action);

    bool success;
    QByteArray newDataCache;
    serializeActions(success, newDataCache);
    if (success) {
        _allActionsDataCache = newDataCache;
        _flags |= Simulation::DIRTY_PHYSICS_ACTIVATION;

        auto actionType = action->getType();
        if (actionType == DYNAMIC_TYPE_HOLD || actionType == DYNAMIC_TYPE_FAR_GRAB) {
            enableNoBootstrap();
        }
    } else {
        qCDebug(entities) << "EntityItem::addActionInternal -- serializeActions failed";
    }
    return success;
}

bool EntityItem::updateAction(EntitySimulationPointer simulation, const QUuid& actionID, const QVariantMap& arguments) {
    bool success = false;
    withWriteLock([&] {
        checkWaitingToRemove(simulation);

        if (!_objectActions.contains(actionID)) {
            return;
        }

        EntityDynamicPointer action = _objectActions[actionID];

        success = action->updateArguments(arguments);
        if (success) {
            action->setIsMine(true);
            serializeActions(success, _allActionsDataCache);
            _flags |= Simulation::DIRTY_PHYSICS_ACTIVATION;
        } else {
            qCDebug(entities) << "EntityItem::updateAction failed";
        }
    });
    return success;
}

bool EntityItem::removeAction(EntitySimulationPointer simulation, const QUuid& actionID) {
    // TODO: some action
    bool success = false;
    withWriteLock([&] {
        checkWaitingToRemove(simulation);
        success = removeActionInternal(actionID);
    });
    updateQueryAACube();

    return success;
}

bool EntityItem::stillHasGrab() const {
    return !(_grabs.empty());
}

// returns 'true' if there exists an action that returns 'true' for EntityActionInterface::isMine()
// (e.g. the action belongs to the MyAvatar instance)
bool EntityItem::stillHasMyGrab() const {
    bool foundGrab = false;
    if (!_grabs.empty()) {
        _grabsLock.withReadLock([&] {
            foreach (const GrabPointer &grab, _grabs) {
                if (grab->getOwnerID() == Physics::getSessionUUID()) {
                    foundGrab = true;
                    break;
                }
            }
        });
    }
    return foundGrab;
}

bool EntityItem::removeActionInternal(const QUuid& actionID, EntitySimulationPointer simulation) {
    _previouslyDeletedActions.insert(actionID, usecTimestampNow());
    if (_objectActions.contains(actionID)) {
        if (!simulation) {
            EntityTreeElementPointer element = _element; // use local copy of _element for logic below
            EntityTreePointer entityTree = element ? element->getTree() : nullptr;
            simulation = entityTree ? entityTree->getSimulation() : nullptr;
        }

        EntityDynamicPointer action = _objectActions[actionID];
        auto removedActionType = action->getType();
        action->setOwnerEntity(nullptr);
        action->setIsMine(false);
        _objectActions.remove(actionID);

        if (removedActionType == DYNAMIC_TYPE_HOLD || removedActionType == DYNAMIC_TYPE_FAR_GRAB) {
            disableNoBootstrap();
        } else {
            // NO-OP: we assume SPECIAL_FLAG_NO_BOOTSTRAPPING bits and collision group are correct
            // because they should have been set correctly when the action was added
            // and/or when children were linked
        }

        if (simulation) {
            action->removeFromSimulation(simulation);
        }

        bool success = true;
        serializeActions(success, _allActionsDataCache);
        _flags |= Simulation::DIRTY_PHYSICS_ACTIVATION;
        setDynamicDataNeedsTransmit(true);
        return success;
    }
    return false;
}

bool EntityItem::clearActions(EntitySimulationPointer simulation) {
    withWriteLock([&] {
        QHash<QUuid, EntityDynamicPointer>::iterator i = _objectActions.begin();
        while (i != _objectActions.end()) {
            const QUuid id = i.key();
            EntityDynamicPointer action = _objectActions[id];
            i = _objectActions.erase(i);
            action->setOwnerEntity(nullptr);
            action->removeFromSimulation(simulation);
        }
        // empty _serializedActions means no actions for the EntityItem
        _actionsToRemove.clear();
        _allActionsDataCache.clear();
        _flags |= Simulation::DIRTY_PHYSICS_ACTIVATION;
        _flags |= Simulation::DIRTY_COLLISION_GROUP; // may need to not collide with own avatar
    });
    return true;
}


void EntityItem::deserializeActions() {
    withWriteLock([&] {
        deserializeActionsInternal();
    });
}


void EntityItem::deserializeActionsInternal() {
    quint64 now = usecTimestampNow();

    if (!_element) {
        qCDebug(entities) << "EntityItem::deserializeActionsInternal -- no _element";
        return;
    }

    EntityTreePointer entityTree = getTree();
    assert(entityTree);
    EntitySimulationPointer simulation = entityTree ? entityTree->getSimulation() : nullptr;
    assert(simulation);

    QVector<QByteArray> serializedActions;
    if (_allActionsDataCache.size() > 0) {
        QDataStream serializedActionsStream(_allActionsDataCache);
        serializedActionsStream >> serializedActions;
    }

    // Keep track of which actions got added or updated by the new dynamicData
    QSet<QUuid> updated;

    foreach(QByteArray serializedAction, serializedActions) {
        QDataStream serializedActionStream(serializedAction);
        EntityDynamicType actionType;
        QUuid actionID;
        serializedActionStream >> actionType;
        serializedActionStream >> actionID;
        if (_previouslyDeletedActions.contains(actionID)) {
            continue;
        }

        if (_objectActions.contains(actionID)) {
            EntityDynamicPointer action = _objectActions[actionID];
            // TODO: make sure types match?  there isn't currently a way to
            // change the type of an existing action.
            if (!action->isMine()) {
                action->deserialize(serializedAction);
            }
            updated << actionID;
        } else {
            auto actionFactory = DependencyManager::get<EntityDynamicFactoryInterface>();
            EntityItemPointer entity = getThisPointer();
            EntityDynamicPointer action = actionFactory->factoryBA(entity, serializedAction);
            if (action) {
                entity->addActionInternal(simulation, action);
                updated << actionID;
            } else {
                HIFI_FCDEBUG(entities(), "EntityItem::deserializeActionsInternal -- action creation failed for"
                        << getID() << _name); // getName();
                removeActionInternal(actionID, nullptr);
            }
        }
    }

    // remove any actions that weren't included in the new data.
    QHash<QUuid, EntityDynamicPointer>::const_iterator i = _objectActions.begin();
    while (i != _objectActions.end()) {
        QUuid id = i.key();
        if (!updated.contains(id)) {
            EntityDynamicPointer action = i.value();

            if (action->isMine()) {
                // we just received an update that didn't include one of our actions.  tell the server about it (again).
                setDynamicDataNeedsTransmit(true);
            } else {
                // don't let someone else delete my action.
                _actionsToRemove << id;
                _previouslyDeletedActions.insert(id, now);
            }
        }
        i++;
    }

    // trim down _previouslyDeletedActions
    QMutableHashIterator<QUuid, quint64> _previouslyDeletedIter(_previouslyDeletedActions);
    while (_previouslyDeletedIter.hasNext()) {
        _previouslyDeletedIter.next();
        if (now - _previouslyDeletedIter.value() > _rememberDeletedActionTime) {
            _previouslyDeletedActions.remove(_previouslyDeletedIter.key());
        }
    }

    _dynamicDataDirty = true;

    return;
}

void EntityItem::checkWaitingToRemove(EntitySimulationPointer simulation) {
    foreach(QUuid actionID, _actionsToRemove) {
        removeActionInternal(actionID, simulation);
    }
    _actionsToRemove.clear();
}

void EntityItem::setDynamicData(QByteArray dynamicData) {
    withWriteLock([&] {
        setDynamicDataInternal(dynamicData);
    });
}

void EntityItem::setDynamicDataInternal(QByteArray dynamicData) {
    if (_allActionsDataCache != dynamicData) {
        _allActionsDataCache = dynamicData;
        deserializeActionsInternal();
    }
    checkWaitingToRemove();
}

void EntityItem::serializeActions(bool& success, QByteArray& result) const {
    if (_objectActions.size() == 0) {
        success = true;
        result.clear();
        return;
    }

    QVector<QByteArray> serializedActions;
    QHash<QUuid, EntityDynamicPointer>::const_iterator i = _objectActions.begin();
    while (i != _objectActions.end()) {
        const QUuid id = i.key();
        EntityDynamicPointer action = _objectActions[id];
        QByteArray bytesForAction = action->serialize();
        serializedActions << bytesForAction;
        i++;
    }

    QDataStream serializedActionsStream(&result, QIODevice::WriteOnly);
    serializedActionsStream << serializedActions;

    if (result.size() >= _maxActionsDataSize) {
        qCDebug(entities) << "EntityItem::serializeActions size is too large -- "
                          << result.size() << ">=" << _maxActionsDataSize;
        success = false;
        return;
    }

    success = true;
    return;
}

const QByteArray EntityItem::getDynamicDataInternal() const {
    if (_dynamicDataDirty) {
        bool success;
        serializeActions(success, _allActionsDataCache);
        if (success) {
            _dynamicDataDirty = false;
        }
    }
    return _allActionsDataCache;
}

const QByteArray EntityItem::getDynamicData() const {
    QByteArray result;

    if (_dynamicDataDirty) {
        withWriteLock([&] {
            getDynamicDataInternal();
            result = _allActionsDataCache;
        });
    } else {
        withReadLock([&] {
            result = _allActionsDataCache;
        });
    }
    return result;
}

QVariantMap EntityItem::getActionArguments(const QUuid& actionID) const {
    QVariantMap result;
    withReadLock([&] {
        if (_objectActions.contains(actionID)) {
            EntityDynamicPointer action = _objectActions[actionID];
            result = action->getArguments();
            result["type"] = EntityDynamicInterface::dynamicTypeToString(action->getType());
        }
    });

    return result;
}

bool EntityItem::shouldSuppressLocationEdits() const {
    // if any of the actions indicate they'd like suppression, suppress
    QHash<QUuid, EntityDynamicPointer>::const_iterator i = _objectActions.begin();
    while (i != _objectActions.end()) {
        if (i.value()->shouldSuppressLocationEdits()) {
            return true;
        }
        i++;
    }

    i = _grabActions.begin();
    while (i != _grabActions.end()) {
        if (i.value()->shouldSuppressLocationEdits()) {
            return true;
        }
        i++;
    }

    // if any of the ancestors are MyAvatar, suppress
    return isChildOfMyAvatar();
}

QList<EntityDynamicPointer> EntityItem::getActionsOfType(EntityDynamicType typeToGet) const {
    QList<EntityDynamicPointer> result;

    for (QHash<QUuid, EntityDynamicPointer>::const_iterator i = _objectActions.begin();
         i != _objectActions.end();
         i++) {
        EntityDynamicPointer action = i.value();
        if (action->getType() == typeToGet && action->isActive()) {
            result += action;
        }
    }

    for (QHash<QUuid, EntityDynamicPointer>::const_iterator i = _grabActions.begin();
         i != _grabActions.end();
         i++) {
        EntityDynamicPointer action = i.value();
        if (action->getType() == typeToGet && action->isActive()) {
            result += action;
        }
    }

    return result;
}

void EntityItem::locationChanged(bool tellPhysics, bool tellChildren) {
    requiresRecalcBoxes();
    if (tellPhysics) {
        _flags |= Simulation::DIRTY_TRANSFORM;
        EntityTreePointer tree = getTree();
        if (tree) {
            tree->entityChanged(getThisPointer());
        }
    }
    SpatiallyNestable::locationChanged(tellPhysics, tellChildren);
    std::pair<int32_t, glm::vec4> data(_spaceIndex, glm::vec4(getWorldPosition(), _boundingRadius));
    emit spaceUpdate(data);
    somethingChangedNotification();
}

void EntityItem::dimensionsChanged() {
    requiresRecalcBoxes();
    SpatiallyNestable::dimensionsChanged(); // Do what you have to do
    _boundingRadius = 0.5f * glm::length(getScaledDimensions());
    std::pair<int32_t, glm::vec4> data(_spaceIndex, glm::vec4(getWorldPosition(), _boundingRadius));
    emit spaceUpdate(data);
    somethingChangedNotification();
}

bool EntityItem::getScalesWithParent() const {
    // keep this logic the same as in EntityItemProperties::getScalesWithParent
    if (isAvatarEntity()) {
        QUuid ancestorID = findAncestorOfType(NestableType::Avatar);
        return !ancestorID.isNull();
    } else {
        return false;
    }
}

void EntityItem::globalizeProperties(EntityItemProperties& properties, const QString& messageTemplate, const glm::vec3& offset) const {
    // TODO -- combine this with convertLocationToScriptSemantics
    bool success;
    auto globalPosition = getWorldPosition(success);
    if (success) {
        properties.setPosition(globalPosition + offset);
        properties.setRotation(getWorldOrientation());
        properties.setDimensions(getScaledDimensions());
        // Should we do velocities and accelerations, too? This could end up being quite involved, which is why the method exists.
    } else {
        properties.setPosition(getQueryAACube().calcCenter() + offset); // best we can do
    }
    if (!messageTemplate.isEmpty()) {
        QString name = properties.getName();
        if (name.isEmpty()) {
            name = EntityTypes::getEntityTypeName(properties.getType());
        }
        qCWarning(entities) << messageTemplate.arg(getEntityItemID().toString()).arg(name).arg(properties.getParentID().toString());
    }
    QUuid empty;
    properties.setParentID(empty);
}


bool EntityItem::matchesJSONFilters(const QJsonObject& jsonFilters) const {

    // The intention for the query JSON filter and this method is to be flexible to handle a variety of filters for
    // ALL entity properties. Some work will need to be done to the property system so that it can be more flexible
    // (to grab the value and default value of a property given the string representation of that property, for example)

    // currently the only property filter we handle in EntityItem is '+' for serverScripts
    // which means that we only handle a filtered query asking for entities where the serverScripts property is non-default

    static const QString SERVER_SCRIPTS_PROPERTY = "serverScripts";
    static const QString ENTITY_TYPE_PROPERTY = "type";

    foreach(const auto& property, jsonFilters.keys()) {
        if (property == SERVER_SCRIPTS_PROPERTY && jsonFilters[property] == EntityQueryFilterSymbol::NonDefault) {
            // check if this entity has a non-default value for serverScripts
            if (_serverScripts != ENTITY_ITEM_DEFAULT_SERVER_SCRIPTS) {
                return true;
            } else {
                return false;
            }
        } else if (property == ENTITY_TYPE_PROPERTY) {
            return (jsonFilters[property] == EntityTypes::getEntityTypeName(getType()) );
        }
    }

    // the json filter syntax did not match what we expected, return a match
    return true;
}

quint64 EntityItem::getLastSimulated() const {
    quint64 result;
    withReadLock([&] {
        result = _lastSimulated;
    });
    return result;
}

void EntityItem::setLastSimulated(quint64 now) {
    withWriteLock([&] {
        _lastSimulated = now;
    });
}

quint64 EntityItem::getLastEdited() const {
    quint64 result;
    withReadLock([&] {
        result = _lastEdited;
    });
    return result;
}

void EntityItem::setLastEdited(quint64 lastEdited) {
    if (lastEdited == 0 || lastEdited > _lastEdited) {
        withWriteLock([&] {
            _lastEdited = _lastUpdated = lastEdited;
            _changedOnServer = glm::max(lastEdited, _changedOnServer);
        });
    }
}

void EntityItem::markAsChangedOnServer() {
    withWriteLock([&] {
        _changedOnServer = usecTimestampNow();
    });
}

quint64 EntityItem::getLastChangedOnServer() const {
    quint64 result;
    withReadLock([&] {
        result = _changedOnServer;
    });
    return result;
}

void EntityItem::update(const quint64& now) {
    withWriteLock([&] {
        _lastUpdated = now;
    });
}

quint64 EntityItem::getLastUpdated() const {
    quint64 result;
    withReadLock([&] {
        result = _lastUpdated;
    });
    return result;
}

void EntityItem::requiresRecalcBoxes() {
    withWriteLock([&] {
        _recalcAABox = true;
        _recalcMinAACube = true;
        _recalcMaxAACube = true;
    });
}

QString EntityItem::getHref() const {
    QString result;
    withReadLock([&] {
        result = _href;
    });
    return result;
}

QString EntityItem::getDescription() const {
    QString result;
    withReadLock([&] {
        result = _description;
    });
    return result;
}

void EntityItem::setDescription(const QString& value) {
    withWriteLock([&] {
        _description = value;
    });
}

glm::vec3 EntityItem::getGravity() const {
    glm::vec3 result;
    withReadLock([&] {
        result = _gravity;
    });
    return result;
}

glm::vec3 EntityItem::getAcceleration() const {
    glm::vec3 result;
    withReadLock([&] {
        result = _acceleration;
    });
    return result;
}

void EntityItem::setAcceleration(const glm::vec3& value) {
    withWriteLock([&] {
        _acceleration = value;
    });
}

float EntityItem::getDamping() const {
    float result;
    withReadLock([&] {
        result = _damping;
    });
    return result;
}

float EntityItem::getRestitution() const {
    float result;
    withReadLock([&] {
        result = _restitution;
    });
    return result;
}

float EntityItem::getFriction() const {
    float result;
    withReadLock([&] {
        result = _friction;
    });
    return result;
}

// lifetime related properties.
float EntityItem::getLifetime() const {
    float result;
    withReadLock([&] {
        result = _lifetime;
    });
    return result;
}

quint64 EntityItem::getCreated() const {
    quint64 result;
    withReadLock([&] {
        result = _created;
    });
    return result;
}

QString EntityItem::getScript() const {
    QString result;
    withReadLock([&] {
        result = _script;
    });
    return result;
}

void EntityItem::setScript(const QString& value) {
    withWriteLock([&] {
        _script = value;
    });
}

quint64 EntityItem::getScriptTimestamp() const {
    quint64 result;
    withReadLock([&] {
        result = _scriptTimestamp;
    });
    return result;
}

void EntityItem::setScriptTimestamp(const quint64 value) {
    withWriteLock([&] {
        _scriptTimestamp = value;
    });
}

QString EntityItem::getServerScripts() const {
    QString result;
    withReadLock([&] {
        result = _serverScripts;
    });
    return result;
}

void EntityItem::setServerScripts(const QString& serverScripts) {
    withWriteLock([&] {
        _serverScripts = serverScripts;
        _serverScriptsChangedTimestamp = usecTimestampNow();
    });
}

QString EntityItem::getCollisionSoundURL() const {
    QString result;
    withReadLock([&] {
        result = _collisionSoundURL;
    });
    return result;
}

glm::vec3 EntityItem::getRegistrationPoint() const {
    return resultWithReadLock<glm::vec3>([&] {
        return _registrationPoint;
    });
}

float EntityItem::getAngularDamping() const {
    float result;
    withReadLock([&] {
        result = _angularDamping;
    });
    return result;
}

QString EntityItem::getName() const {
    QString result;
    withReadLock([&] {
        result = _name;
    });
    return result;
}

void EntityItem::setName(const QString& value) {
    withWriteLock([&] {
        _name = value;
    });
}

QString EntityItem::getDebugName() {
    QString result = getName();
    if (result.isEmpty()) {
        result = getID().toString();
    }
    return result;
}

bool EntityItem::getVisible() const {
    bool result;
    withReadLock([&] {
        result = _visible;
    });
    return result;
}

void EntityItem::setVisible(bool value) {
    bool changed;
    withWriteLock([&] {
        changed = _visible != value;
        _needsRenderUpdate |= changed;
        _visible = value;
    });

    if (changed) {
        bumpAncestorChainRenderableVersion();
    }
}

bool EntityItem::isVisibleInSecondaryCamera() const {
    bool result;
    withReadLock([&] {
        result = _isVisibleInSecondaryCamera;
    });
    return result;
}

void EntityItem::setIsVisibleInSecondaryCamera(bool value) {
    withWriteLock([&] {
        _needsRenderUpdate |= _isVisibleInSecondaryCamera != value;
        _isVisibleInSecondaryCamera = value;
    });
}

RenderLayer EntityItem::getRenderLayer() const {
    return resultWithReadLock<RenderLayer>([&] {
        return _renderLayer;
    });
}

void EntityItem::setRenderLayer(RenderLayer value) {
    withWriteLock([&] {
        _needsRenderUpdate |= _renderLayer != value;
        _renderLayer = value;
    });
}

PrimitiveMode EntityItem::getPrimitiveMode() const {
    return resultWithReadLock<PrimitiveMode>([&] {
        return _primitiveMode;
    });
}

void EntityItem::setPrimitiveMode(PrimitiveMode value) {
    withWriteLock([&] {
        _needsRenderUpdate |= _primitiveMode != value;
        _primitiveMode = value;
    });
}

bool EntityItem::getCauterized() const {
    return resultWithReadLock<bool>([&] {
        return _cauterized;
    });
}

void EntityItem::setCauterized(bool value) {
    bool needsRenderUpdate = false;
    withWriteLock([&] {
        needsRenderUpdate = _cauterized != value;
        _needsRenderUpdate |= needsRenderUpdate;
        _cauterized = value;
    });
    if (needsRenderUpdate) {
        somethingChangedNotification();
    }
}

bool EntityItem::getIgnorePickIntersection() const {
    return resultWithReadLock<bool>([&] {
        return _ignorePickIntersection;
    });
}

void EntityItem::setIgnorePickIntersection(bool value) {
    withWriteLock([&] {
        _ignorePickIntersection = value;
    });
}

bool EntityItem::getCanCastShadow() const {
    bool result;
    withReadLock([&] {
        result = _canCastShadow;
    });
    return result;
}

void EntityItem::setCanCastShadow(bool value) {
    withWriteLock([&] {
        _needsRenderUpdate |= _canCastShadow != value;
        _canCastShadow = value;
    });
}

bool EntityItem::getCullWithParent() const {
    bool result;
    withReadLock([&] {
        result = _cullWithParent;
    });
    return result;
}

void EntityItem::setCullWithParent(bool value) {
    bool needsRenderUpdate = false;
    withWriteLock([&] {
        needsRenderUpdate = _cullWithParent != value;
        _needsRenderUpdate |= needsRenderUpdate;
        _cullWithParent = value;
    });
    if (needsRenderUpdate) {
        somethingChangedNotification();
    }
}

bool EntityItem::isChildOfMyAvatar() const {
    QUuid ancestorID = findAncestorOfType(NestableType::Avatar);
    return !ancestorID.isNull() && (ancestorID == Physics::getSessionUUID() || ancestorID == AVATAR_SELF_ID);
}

bool EntityItem::getCollisionless() const {
    bool result;
    withReadLock([&] {
        result = _collisionless;
    });
    return result;
}

uint16_t EntityItem::getCollisionMask() const {
    return _collisionMask;
}

bool EntityItem::getDynamic() const {
    if (SHAPE_TYPE_STATIC_MESH == getShapeType()) {
        return false;
    }
    return _dynamic;
}

bool EntityItem::getLocked() const {
    return _locked;
}

void EntityItem::setLocked(bool value) {
    bool changed { false };
    withWriteLock([&] {
        if (_locked != value) {
            _locked = value;
            changed = true;
        }
    });
    if (changed) {
        markDirtyFlags(Simulation::DIRTY_MOTION_TYPE);
        EntityTreePointer tree = getTree();
        if (tree) {
            tree->entityChanged(getThisPointer());
        }
    }
}

QString EntityItem::getUserData() const {
    QString result;
    withReadLock([&] {
        result = _userData;
    });
    return result;
}

void EntityItem::setUserData(const QString& value) {
    withWriteLock([&] {
        _userData = value;
    });
}

QString EntityItem::getPrivateUserData() const {
    QString result;
    withReadLock([&] {
        result = _privateUserData;
    });
    return result;
}

void EntityItem::setPrivateUserData(const QString& value) {
    withWriteLock([&] {
        _privateUserData = value;
    });
}

uint32_t EntityItem::getDirtyFlags() const {
    uint32_t result;
    withReadLock([&] {
        result = _flags & Simulation::DIRTY_FLAGS_MASK;
    });
    return result;
}

void EntityItem::markDirtyFlags(uint32_t mask) {
    withWriteLock([&] {
        mask &= Simulation::DIRTY_FLAGS_MASK;
        _flags |= mask;
    });
}

void EntityItem::clearDirtyFlags(uint32_t mask) {
    withWriteLock([&] {
        mask &= Simulation::DIRTY_FLAGS_MASK;
        _flags &= ~mask;
    });
}

uint32_t EntityItem::getSpecialFlags() const {
    uint32_t result;
    withReadLock([&] {
        result = _flags & Simulation::SPECIAL_FLAGS_MASK;
    });
    return result;
}

void EntityItem::markSpecialFlags(uint32_t mask) {
    withWriteLock([&] {
        mask &= Simulation::SPECIAL_FLAGS_MASK;
        _flags |= mask;
    });
}

void EntityItem::clearSpecialFlags(uint32_t mask) {
    withWriteLock([&] {
        mask &= Simulation::SPECIAL_FLAGS_MASK;
        _flags &= ~mask;
    });
}

float EntityItem::getDensity() const {
    float result;
    withReadLock([&] {
        result = _density;
    });
    return result;
}

EntityItem::ChangeHandlerId EntityItem::registerChangeHandler(const ChangeHandlerCallback& handler) {
    ChangeHandlerId result = QUuid::createUuid();
    withWriteLock([&] {
        _changeHandlers[result] = handler;
    });
    return result;
}

void EntityItem::deregisterChangeHandler(const ChangeHandlerId& changeHandlerId) {
    withWriteLock([&] {
        _changeHandlers.remove(changeHandlerId);
    });
}

void EntityItem::somethingChangedNotification() {
    auto id = getEntityItemID();
    withReadLock([&] {
        for (const auto& handler : _changeHandlers.values()) {
            handler(id);
        }
    });
}

void EntityItem::collectChildrenForDelete(std::vector<EntityItemPointer>& entitiesToDelete, const QUuid& sessionID) const {
    // Deleting an entity has consequences for its children, however there are rules dictating what can be deleted.
    // This method helps enforce those rules: not for this entity, but for its children.
    for (SpatiallyNestablePointer child : getChildren()) {
        if (child && child->getNestableType() == NestableType::Entity) {
            EntityItemPointer childEntity = std::static_pointer_cast<EntityItem>(child);
            // NOTE: null sessionID means "collect ALL known children", else we only collect: local-entities and myAvatar-entities
            if (sessionID.isNull() || childEntity->isLocalEntity() || childEntity->isMyAvatarEntity()) {
                if (std::find(entitiesToDelete.begin(), entitiesToDelete.end(), childEntity) == entitiesToDelete.end()) {
                    entitiesToDelete.push_back(childEntity);
                    childEntity->collectChildrenForDelete(entitiesToDelete, sessionID);
                }
            }
        }
    }
}

void EntityItem::setSpaceIndex(int32_t index) {
    assert(_spaceIndex == -1);
    _spaceIndex = index;
}

void EntityItem::preDelete() {
}

bool EntityItem::getCloneable() const {
    bool result;
    withReadLock([&] {
        result = _cloneable;
    });
    return result;
}

void EntityItem::setCloneable(bool value) {
    withWriteLock([&] {
        _cloneable = value;
    });
}

float EntityItem::getCloneLifetime() const {
    float result;
    withReadLock([&] {
        result = _cloneLifetime;
    });
    return result;
}

void EntityItem::setCloneLifetime(float value) {
    withWriteLock([&] {
        _cloneLifetime = value;
    });
}

float EntityItem::getCloneLimit() const {
    float result;
    withReadLock([&] {
        result = _cloneLimit;
    });
    return result;
}

void EntityItem::setCloneLimit(float value) {
    withWriteLock([&] {
        _cloneLimit = value;
    });
}

bool EntityItem::getCloneDynamic() const {
    bool result;
    withReadLock([&] {
        result = _cloneDynamic;
    });
    return result;
}

void EntityItem::setCloneDynamic(bool value) {
    withWriteLock([&] {
        _cloneDynamic = value;
    });
}

bool EntityItem::getCloneAvatarEntity() const {
    bool result;
    withReadLock([&] {
        result = _cloneAvatarEntity;
    });
    return result;
}

void EntityItem::setCloneAvatarEntity(bool value) {
    withWriteLock([&] {
        _cloneAvatarEntity = value;
    });
}

const QUuid EntityItem::getCloneOriginID() const {
    QUuid result;
    withReadLock([&] {
        result = _cloneOriginID;
    });
    return result;
}

void EntityItem::setCloneOriginID(const QUuid& value) {
    withWriteLock([&] {
        _cloneOriginID = value;
    });
}

void EntityItem::addCloneID(const QUuid& cloneID) {
    withWriteLock([&] {
        if (!_cloneIDs.contains(cloneID)) {
            _cloneIDs.append(cloneID);
        }
    });
}

void EntityItem::removeCloneID(const QUuid& cloneID) {
    withWriteLock([&] {
        int index = _cloneIDs.indexOf(cloneID);
        if (index >= 0) {
            _cloneIDs.removeAt(index);
        }
    });
}

const QVector<QUuid> EntityItem::getCloneIDs() const {
    QVector<QUuid> result;
    withReadLock([&] {
        result = _cloneIDs;
    });
    return result;
}

void EntityItem::setCloneIDs(const QVector<QUuid>& cloneIDs) {
    withWriteLock([&] {
        _cloneIDs = cloneIDs;
    });
}

bool EntityItem::shouldPreloadScript() const {
    return !_script.isEmpty() && ((_loadedScript != _script) || (_loadedScriptTimestamp != _scriptTimestamp));
}

void EntityItem::scriptHasPreloaded() {
    _loadedScript = _script;
    _loadedScriptTimestamp = _scriptTimestamp;
}

void EntityItem::scriptHasUnloaded() {
    _loadedScript = "";
    _loadedScriptTimestamp = 0;
    _scriptPreloadFinished = false;
}

void EntityItem::setScriptHasFinishedPreload(bool value) {
    _scriptPreloadFinished = value;
}

bool EntityItem::isScriptPreloadFinished() {
    return _scriptPreloadFinished;
}

void EntityItem::prepareForSimulationOwnershipBid(EntityItemProperties& properties, uint64_t now, uint8_t priority) {
    if (dynamicDataNeedsTransmit()) {
        setDynamicDataNeedsTransmit(false);
        properties.setActionData(getDynamicData());
    }

    if (updateQueryAACube()) {
        // due to parenting, the server may not know where something is in world-space, so include the bounding cube.
        properties.setQueryAACube(getQueryAACube());
    }

    // set the LastEdited of the properties but NOT the entity itself
    properties.setLastEdited(now);

    clearScriptSimulationPriority();
    properties.setSimulationOwner(Physics::getSessionUUID(), priority);
    setPendingOwnershipPriority(priority);

    // TODO: figure out if it would be OK to NOT bother set these properties here
    properties.setEntityHostType(getEntityHostType());
    properties.setOwningAvatarID(getOwningAvatarID());
    setLastBroadcast(now); // for debug/physics status icons
}

bool EntityItem::isWearable() const {
    return isVisible() &&
        (getParentID() == DependencyManager::get<NodeList>()->getSessionUUID() || getParentID() == AVATAR_SELF_ID);
}

bool EntityItem::isMyAvatarEntity() const {
    return _hostType == entity::HostType::AVATAR && AVATAR_SELF_ID == _owningAvatarID;
};

QUuid EntityItem::getOwningAvatarIDForProperties() const {
    if (isMyAvatarEntity()) {
        // NOTE: we always store AVATAR_SELF_ID for MyAvatar's avatar entities,
        // however for EntityItemProperties to be consumed by outside contexts (e.g. JS)
        // we use the actual "sessionUUID" which is conveniently cached in the Physics namespace
        return Physics::getSessionUUID();
    }
    return _owningAvatarID;
}

void EntityItem::setOwningAvatarID(const QUuid& owningAvatarID) {
    if (!owningAvatarID.isNull() && owningAvatarID == Physics::getSessionUUID()) {
        _owningAvatarID = AVATAR_SELF_ID;
    } else {
        _owningAvatarID = owningAvatarID;
    }
}

void EntityItem::addGrab(GrabPointer grab) {
    enableNoBootstrap();
    SpatiallyNestable::addGrab(grab);

    if (!getParentID().isNull()) {
        return;
    }

    int jointIndex = grab->getParentJointIndex();
    bool isFarGrab = jointIndex == FARGRAB_RIGHTHAND_INDEX
        || jointIndex == FARGRAB_LEFTHAND_INDEX
        || jointIndex == FARGRAB_MOUSE_INDEX;

    // GRAB HACK: FarGrab doesn't work on non-dynamic things yet, but we really really want NearGrab
    // (aka Hold) to work for such ojects, hence we filter the useAction case like so:
    bool useAction = getDynamic() || (_physicsInfo && !isFarGrab);
    if (useAction) {
        EntityTreePointer entityTree = getTree();
        assert(entityTree);
        EntitySimulationPointer simulation = entityTree->getSimulation();
        assert(simulation);

        auto actionFactory = DependencyManager::get<EntityDynamicFactoryInterface>();
        QUuid actionID = QUuid::createUuid();

        EntityDynamicType dynamicType;
        QVariantMap arguments;
        if (isFarGrab) {
            // add a far-grab action
            dynamicType = DYNAMIC_TYPE_FAR_GRAB;
            arguments["otherID"] = grab->getOwnerID();
            arguments["otherJointIndex"] = jointIndex;
            arguments["targetPosition"] = vec3ToQMap(grab->getPositionalOffset());
            arguments["targetRotation"] = quatToQMap(grab->getRotationalOffset());
            arguments["linearTimeScale"] = 0.05;
            arguments["angularTimeScale"] = 0.05;
        } else {
            // add a near-grab action
            dynamicType = DYNAMIC_TYPE_HOLD;
            arguments["holderID"] = grab->getOwnerID();
            arguments["hand"] = grab->getHand();
            arguments["timeScale"] = 0.05;
            arguments["relativePosition"] = vec3ToQMap(grab->getPositionalOffset());
            arguments["relativeRotation"] = quatToQMap(grab->getRotationalOffset());
            arguments["kinematic"] = _grabProperties.getGrabKinematic();
            arguments["kinematicSetVelocity"] = true;
            arguments["ignoreIK"] = _grabProperties.getGrabFollowsController();
        }
        EntityDynamicPointer action = actionFactory->factory(dynamicType, actionID, getThisPointer(), arguments);
        grab->setActionID(actionID);
        _grabActions[actionID] = action;
        simulation->addDynamic(action);
        markDirtyFlags(Simulation::DIRTY_MOTION_TYPE);
        simulation->changeEntity(getThisPointer());

        // don't forget to set isMine() for locally-created grabs
        action->setIsMine(grab->getOwnerID() == Physics::getSessionUUID());
    }
}

void EntityItem::removeGrab(GrabPointer grab) {
    int oldNumGrabs = _grabs.size();
    SpatiallyNestable::removeGrab(grab);
    if (!getDynamic() && _grabs.size() != oldNumGrabs) {
        // GRAB HACK: the expected behavior is for non-dynamic grabbed things to NOT be throwable
        // so we slam the velocities to zero here whenever the number of grabs change.
        // NOTE: if there is still another grab in effect this shouldn't interfere with the object's motion
        // because that grab will slam the position+velocities next frame.
        setLocalVelocity(glm::vec3(0.0f));
        setAngularVelocity(glm::vec3(0.0f));
    }

    QUuid actionID = grab->getActionID();
    if (!actionID.isNull()) {
        EntityDynamicPointer action = _grabActions.value(actionID);
        if (action) {
            _grabActions.remove(actionID);
            EntityTreePointer entityTree = getTree();
            EntitySimulationPointer simulation = entityTree ? entityTree->getSimulation() : nullptr;
            if (simulation) {
                action->removeFromSimulation(simulation);
                action->removeFromOwner();
            }
        }
    }
    disableNoBootstrap();
}

void EntityItem::disableGrab(GrabPointer grab) {
    QUuid actionID = grab->getActionID();
    if (!actionID.isNull()) {
        EntityDynamicPointer action = _grabActions.value(actionID);
        if (action) {
            action->deactivate();
        }
    }
}

void EntityItem::setRenderWithZones(const QVector<QUuid>& renderWithZones) {
    withWriteLock([&] {
        if (_renderWithZones != renderWithZones) {
            _needsZoneOcclusionUpdate = true;
            _renderWithZones = renderWithZones;
        }
    });
}

QVector<QUuid> EntityItem::getRenderWithZones() const {
    return resultWithReadLock<QVector<QUuid>>([&] {
        return _renderWithZones;
    });
}

BillboardMode EntityItem::getBillboardMode() const {
    return resultWithReadLock<BillboardMode>([&] {
        return _billboardMode;
    });
}

void EntityItem::setBillboardMode(BillboardMode value) {
    withWriteLock([&] {
        _needsRenderUpdate |= _billboardMode != value;
        _billboardMode = value;
    });
}
