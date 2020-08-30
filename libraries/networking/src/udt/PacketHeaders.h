//
//  PacketHeaders.h
//  libraries/networking/src
//
//  Created by Stephen Birarda on 4/8/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PacketHeaders_h
#define hifi_PacketHeaders_h

#pragma once

#include <cstdint>
#include <map>

#include <QtCore/QCryptographicHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QUuid>

// The enums are inside this PacketTypeEnum for run-time conversion of enum value to string via
// Q_ENUMS, without requiring a macro that is called for each enum value.
class PacketTypeEnum {
    Q_GADGET
    Q_ENUMS(Value)
public:
    // If adding a new packet packetType, you can replace one marked usable or add at the end.
    // This enum must hold 256 or fewer packet types (so the value is <= 255) since it is statically typed as a uint8_t
    enum class Value : uint8_t {
        Unknown,
        StunResponse,
        DomainList,
        Ping,
        PingReply,
        KillAvatar,
        AvatarData,
        InjectAudio,
        MixedAudio,
        MicrophoneAudioNoEcho,
        MicrophoneAudioWithEcho,
        BulkAvatarData,
        SilentAudioFrame,
        DomainListRequest,
        RequestAssignment,
        CreateAssignment,
        DomainConnectionDenied,
        MuteEnvironment,
        AudioStreamStats,
        DomainServerPathQuery,
        DomainServerPathResponse,
        DomainServerAddedNode,
        ICEServerPeerInformation,
        ICEServerQuery,
        OctreeStats,
        SetAvatarTraits,
        InjectorGainSet,
        AssignmentClientStatus,
        NoisyMute,
        AvatarIdentity,
        NodeIgnoreRequest,
        DomainConnectRequest,
        DomainServerRequireDTLS,
        NodeJsonStats,
        OctreeDataNack,
        StopNode,
        AudioEnvironment,
        EntityEditNack,
        ICEServerHeartbeat,
        ICEPing,
        ICEPingReply,
        EntityData,
        EntityQuery,
        EntityAdd,
        EntityErase,
        EntityEdit,
        DomainServerConnectionToken,
        DomainSettingsRequest,
        DomainSettings,
        AssetGet,
        AssetGetReply,
        AssetUpload,
        AssetUploadReply,
        AssetGetInfo,
        AssetGetInfoReply,
        DomainDisconnectRequest,
        DomainServerRemovedNode,
        MessagesData,
        MessagesSubscribe,
        MessagesUnsubscribe,
        ICEServerHeartbeatDenied,
        AssetMappingOperation,
        AssetMappingOperationReply,
        ICEServerHeartbeatACK,
        NegotiateAudioFormat,
        SelectedAudioFormat,
        MoreEntityShapes,
        NodeKickRequest,
        NodeMuteRequest,
        RadiusIgnoreRequest,
        UsernameFromIDRequest,
        UsernameFromIDReply,
        AvatarQuery,
        RequestsDomainListData,
        PerAvatarGainSet,
        EntityScriptGetStatus,
        EntityScriptGetStatusReply,
        ReloadEntityServerScript,
        EntityPhysics,
        EntityServerScriptLog,
        AdjustAvatarSorting,
        OctreeFileReplacement,
        CollisionEventChanges,
        ReplicatedMicrophoneAudioNoEcho,
        ReplicatedMicrophoneAudioWithEcho,
        ReplicatedInjectAudio,
        ReplicatedSilentAudioFrame,
        ReplicatedAvatarIdentity,
        ReplicatedKillAvatar,
        ReplicatedBulkAvatarData,
        DomainContentReplacementFromUrl,
        DropOnNextProtocolChange_1,
        EntityScriptCallMethod,
        DropOnNextProtocolChange_2,
        DropOnNextProtocolChange_3,
        OctreeDataFileRequest,
        OctreeDataFileReply,
        OctreeDataPersist,
        EntityClone,
        EntityQueryInitialResultsComplete,
        BulkAvatarTraits,
        AudioSoloRequest,
        BulkAvatarTraitsAck,
        StopInjector,
        AvatarZonePresence,
        NUM_PACKET_TYPE
    };

    const static QHash<PacketTypeEnum::Value, PacketTypeEnum::Value> getReplicatedPacketMapping() {
        const static QHash<PacketTypeEnum::Value, PacketTypeEnum::Value> REPLICATED_PACKET_MAPPING {
            { PacketTypeEnum::Value::MicrophoneAudioNoEcho, PacketTypeEnum::Value::ReplicatedMicrophoneAudioNoEcho },
            { PacketTypeEnum::Value::MicrophoneAudioWithEcho, PacketTypeEnum::Value::ReplicatedMicrophoneAudioWithEcho },
            { PacketTypeEnum::Value::InjectAudio, PacketTypeEnum::Value::ReplicatedInjectAudio },
            { PacketTypeEnum::Value::SilentAudioFrame, PacketTypeEnum::Value::ReplicatedSilentAudioFrame },
            { PacketTypeEnum::Value::AvatarIdentity, PacketTypeEnum::Value::ReplicatedAvatarIdentity },
            { PacketTypeEnum::Value::KillAvatar, PacketTypeEnum::Value::ReplicatedKillAvatar },
            { PacketTypeEnum::Value::BulkAvatarData, PacketTypeEnum::Value::ReplicatedBulkAvatarData }
        };
        return REPLICATED_PACKET_MAPPING;
    }

    const static QSet<PacketTypeEnum::Value> getNonVerifiedPackets() {
        const static QSet<PacketTypeEnum::Value> NON_VERIFIED_PACKETS = QSet<PacketTypeEnum::Value>()
            << PacketTypeEnum::Value::NodeJsonStats
            << PacketTypeEnum::Value::EntityQuery
            << PacketTypeEnum::Value::OctreeDataNack
            << PacketTypeEnum::Value::EntityEditNack
            << PacketTypeEnum::Value::DomainListRequest
            << PacketTypeEnum::Value::StopNode
            << PacketTypeEnum::Value::DomainDisconnectRequest
            << PacketTypeEnum::Value::UsernameFromIDRequest
            << PacketTypeEnum::Value::NodeKickRequest
            << PacketTypeEnum::Value::NodeMuteRequest;
        return NON_VERIFIED_PACKETS;
    }

    const static QSet<PacketTypeEnum::Value> getNonSourcedPackets() {
        const static QSet<PacketTypeEnum::Value> NON_SOURCED_PACKETS = QSet<PacketTypeEnum::Value>()
            << PacketTypeEnum::Value::StunResponse << PacketTypeEnum::Value::CreateAssignment
            << PacketTypeEnum::Value::RequestAssignment << PacketTypeEnum::Value::DomainServerRequireDTLS
            << PacketTypeEnum::Value::DomainConnectRequest << PacketTypeEnum::Value::DomainList
            << PacketTypeEnum::Value::DomainConnectionDenied << PacketTypeEnum::Value::DomainServerPathQuery
            << PacketTypeEnum::Value::DomainServerPathResponse << PacketTypeEnum::Value::DomainServerAddedNode
            << PacketTypeEnum::Value::DomainServerConnectionToken << PacketTypeEnum::Value::DomainSettingsRequest
            << PacketTypeEnum::Value::OctreeDataFileRequest << PacketTypeEnum::Value::OctreeDataFileReply
            << PacketTypeEnum::Value::OctreeDataPersist << PacketTypeEnum::Value::DomainContentReplacementFromUrl
            << PacketTypeEnum::Value::DomainSettings << PacketTypeEnum::Value::ICEServerPeerInformation
            << PacketTypeEnum::Value::ICEServerQuery << PacketTypeEnum::Value::ICEServerHeartbeat
            << PacketTypeEnum::Value::ICEServerHeartbeatACK << PacketTypeEnum::Value::ICEPing
            << PacketTypeEnum::Value::ICEPingReply << PacketTypeEnum::Value::ICEServerHeartbeatDenied
            << PacketTypeEnum::Value::AssignmentClientStatus << PacketTypeEnum::Value::StopNode
            << PacketTypeEnum::Value::DomainServerRemovedNode << PacketTypeEnum::Value::UsernameFromIDReply
            << PacketTypeEnum::Value::OctreeFileReplacement << PacketTypeEnum::Value::ReplicatedMicrophoneAudioNoEcho
            << PacketTypeEnum::Value::ReplicatedMicrophoneAudioWithEcho << PacketTypeEnum::Value::ReplicatedInjectAudio
            << PacketTypeEnum::Value::ReplicatedSilentAudioFrame << PacketTypeEnum::Value::ReplicatedAvatarIdentity
            << PacketTypeEnum::Value::ReplicatedKillAvatar << PacketTypeEnum::Value::ReplicatedBulkAvatarData
            << PacketTypeEnum::Value::AvatarZonePresence;
        return NON_SOURCED_PACKETS;
    }

    const static QSet<PacketTypeEnum::Value> getDomainSourcedPackets() {
        const static QSet<PacketTypeEnum::Value> DOMAIN_SOURCED_PACKETS = QSet<PacketTypeEnum::Value>()
            << PacketTypeEnum::Value::AssetMappingOperation
            << PacketTypeEnum::Value::AssetGet
            << PacketTypeEnum::Value::AssetUpload;
        return DOMAIN_SOURCED_PACKETS;
    }

    const static QSet<PacketTypeEnum::Value> getDomainIgnoredVerificationPackets() {
        const static QSet<PacketTypeEnum::Value> DOMAIN_IGNORED_VERIFICATION_PACKETS = QSet<PacketTypeEnum::Value>()
            << PacketTypeEnum::Value::AssetMappingOperationReply
            << PacketTypeEnum::Value::AssetGetReply
            << PacketTypeEnum::Value::AssetUploadReply;
        return DOMAIN_IGNORED_VERIFICATION_PACKETS;
    }
};

using PacketType = PacketTypeEnum::Value;

const int NUM_BYTES_MD5_HASH = 16;

// NOTE: There is a max limit of 255, hopefully we have a better way to manage this by then.
typedef uint8_t PacketVersion;

PacketVersion versionForPacketType(PacketType packetType);
QByteArray protocolVersionsSignature(); /// returns a unique signature for all the current protocols
QString protocolVersionsSignatureBase64();

#if (PR_BUILD || DEV_BUILD)
void sendWrongProtocolVersionsSignature(bool sendWrongVersion); /// for debugging version negotiation
#endif

uint qHash(const PacketType& key, uint seed);
QDebug operator<<(QDebug debug, const PacketType& type);

// Due to the different legacy behaviour, we need special processing for domains that were created before
// the zone inheritance modes were added. These have version numbers up to 80.
enum class EntityVersion : PacketVersion {
    StrokeColorProperty = 0,
    HasDynamicOwnershipTests,
    HazeEffect,
    StaticCertJsonVersionOne,
    OwnershipChallengeFix,
    ZoneLightInheritModes = 82,
    ZoneStageRemoved,
    SoftEntities,
    MaterialEntities,
    ShadowControl,
    MaterialData,
    CloneableData,
    CollisionMask16Bytes,
    YieldSimulationOwnership,
    ParticleEntityFix,
    ParticleSpin,
    BloomEffect,
    GrabProperties,
    ScriptGlmVectors,
    FixedLightSerialization,
    MaterialRepeat,
    EntityHostTypes,
    CleanupProperties,
    ImageEntities,
    GridEntities,
    MissingTextProperties,
    GrabTraits,
    MorePropertiesCleanup,
    FixPropertiesFromCleanup,
    UpdatedPolyLines,
    FixProtocolVersionBumpMismatch,
    MigrateOverlayRenderProperties,
    MissingWebEntityProperties,
    PulseProperties,
    RingGizmoEntities,
    AvatarPriorityZone,
    ShowKeyboardFocusHighlight,
    WebBillboardMode,
    ModelScale,
    ReOrderParentIDProperties,
    CertificateTypeProperty,
    DisableWebMedia,
    ParticleShapeType,
    ParticleShapeTypeDeadlockFix,
    PrivateUserData,
    TextUnlit,
    ShadowBiasAndDistance,
    TextEntityFonts,
    ScriptServerKinematicMotion,
    ScreenshareZone,
    ZoneOcclusion,
    ModelBlendshapes,
    TransparentWeb,
    UseOriginalPivot,
    UserAgent,
    AllBillboardMode,
    TextAlignment,

    // Add new versions above here
    NUM_PACKET_TYPE,
    LAST_PACKET_TYPE = NUM_PACKET_TYPE - 1
};

enum class EntityScriptCallMethodVersion : PacketVersion {
    ServerCallable = 18,
    ClientCallable = 19
};

enum class EntityQueryPacketVersion: PacketVersion {
    JSONFilter = 18,
    JSONFilterWithFamilyTree = 19,
    ConnectionIdentifier = 20,
    RemovedJurisdictions = 21,
    MultiFrustumQuery = 22,
    ConicalFrustums = 23
};

enum class AssetServerPacketVersion: PacketVersion {
    VegasCongestionControl = 19,
    RangeRequestSupport,
    RedirectedMappings,
    BakingTextureMeta
};

enum class AvatarMixerPacketVersion : PacketVersion {
    TranslationSupport = 17,
    SoftAttachmentSupport,
    AvatarEntities,
    AbsoluteSixByteRotations,
    SensorToWorldMat,
    HandControllerJoints,
    HasKillAvatarReason,
    SessionDisplayName,
    Unignore,
    ImmediateSessionDisplayNameUpdates,
    VariableAvatarData,
    AvatarAsChildFixes,
    StickAndBallDefaultAvatar,
    IdentityPacketsIncludeUpdateTime,
    AvatarIdentitySequenceId,
    MannequinDefaultAvatar,
    AvatarIdentitySequenceFront,
    IsReplicatedInAvatarIdentity,
    AvatarIdentityLookAtSnapping,
    UpdatedMannequinDefaultAvatar,
    AvatarJointDefaultPoseFlags,
    FBXReaderNodeReparenting,
    FixMannequinDefaultAvatarFeet,
    ProceduralFaceMovementFlagsAndBlendshapes,
    FarGrabJoints,
    MigrateSkeletonURLToTraits,
    MigrateAvatarEntitiesToTraits,
    FarGrabJointsRedux,
    JointTransScaled,
    GrabTraits,
    CollisionFlag,
    AvatarTraitsAck,
    FasterAvatarEntities,
    SendMaxTranslationDimension,
    FBXJointOrderChange,
    HandControllerSection,
    SendVerificationFailed,
    ARKitBlendshapes
};

enum class DomainConnectRequestVersion : PacketVersion {
    NoHostname = 17,
    HasHostname,
    HasProtocolVersions,
    HasMACAddress,
    HasMachineFingerprint,
    AlwaysHasMachineFingerprint,
    HasTimestamp,
    HasReason,
    HasSystemInfo,
    HasCompressedSystemInfo
};

enum class DomainConnectionDeniedVersion : PacketVersion {
    ReasonMessageOnly = 17,
    IncludesReasonCode,
    IncludesExtraInfo
};

enum class DomainServerAddedNodeVersion : PacketVersion {
    PrePermissionsGrid = 17,
    PermissionsGrid
};

enum class DomainListVersion : PacketVersion {
    PrePermissionsGrid = 18,
    PermissionsGrid,
    GetUsernameFromUUIDSupport,
    GetMachineFingerprintFromUUIDSupport,
    AuthenticationOptional,
    HasTimestamp,
    HasConnectReason
};

enum class AudioVersion : PacketVersion {
    HasCompressedAudio = 17,
    CodecNameInAudioPackets,
    Exactly10msAudioPackets,
    TerminatingStreamStats,
    SpaceBubbleChanges,
    HasPersonalMute,
    HighDynamicRangeVolume,
    StopInjectors
};

enum class MessageDataVersion : PacketVersion {
    TextOrBinaryData = 18
};

enum class IcePingVersion : PacketVersion {
    SendICEPeerID = 18
};

enum class PingVersion : PacketVersion {
    IncludeConnectionID = 18
};

enum class AvatarQueryVersion : PacketVersion {
    SendMultipleFrustums = 21,
    ConicalFrustums = 22
};

#endif // hifi_PacketHeaders_h
