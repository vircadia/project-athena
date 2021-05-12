//
//  AssetClient.h
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/21
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssetClient_h
#define hifi_AssetClient_h

#include <QStandardItemModel>
#include <QtQml/QJSEngine>
#include <QString>

#include <map>

#include <DependencyManager.h>
#include <shared/MiniPromises.h>

#include "AssetUtils.h"
#include "ByteRange.h"
#include "ClientServerUtils.h"
#include "LimitedNodeList.h"
#include "Node.h"
#include "ReceivedMessage.h"

class GetMappingRequest;
class SetMappingRequest;
class GetAllMappingsRequest;
class DeleteMappingsRequest;
class RenameMappingRequest;
class SetBakingEnabledRequest;
class AssetRequest;
class AssetUpload;

struct AssetInfo {
    QString hash;
    int64_t size;
};

using MappingOperationCallback = std::function<void(bool responseReceived, AssetUtils::AssetServerError serverError, QSharedPointer<ReceivedMessage> message)>;
using ReceivedAssetCallback = std::function<void(bool responseReceived, AssetUtils::AssetServerError serverError, const QByteArray& data)>;
using GetInfoCallback = std::function<void(bool responseReceived, AssetUtils::AssetServerError serverError, AssetInfo info)>;
using UploadResultCallback = std::function<void(bool responseReceived, AssetUtils::AssetServerError serverError, const QString& hash)>;
using ProgressCallback = std::function<void(qint64 totalReceived, qint64 total)>;

class AssetClient : public QObject, public Dependency {
    Q_OBJECT
public:
    AssetClient();

    Q_INVOKABLE GetMappingRequest* createGetMappingRequest(const AssetUtils::AssetPath& path);
    Q_INVOKABLE GetAllMappingsRequest* createGetAllMappingsRequest();
    Q_INVOKABLE DeleteMappingsRequest* createDeleteMappingsRequest(const AssetUtils::AssetPathList& paths);
    Q_INVOKABLE SetMappingRequest* createSetMappingRequest(const AssetUtils::AssetPath& path, const AssetUtils::AssetHash& hash);
    Q_INVOKABLE RenameMappingRequest* createRenameMappingRequest(const AssetUtils::AssetPath& oldPath, const AssetUtils::AssetPath& newPath);
    Q_INVOKABLE SetBakingEnabledRequest* createSetBakingEnabledRequest(const AssetUtils::AssetPathList& path, bool enabled);
    Q_INVOKABLE AssetRequest* createRequest(const AssetUtils::AssetHash& hash, const ByteRange& byteRange = ByteRange());
    Q_INVOKABLE AssetUpload* createUpload(const QString& filename);
    Q_INVOKABLE AssetUpload* createUpload(const QByteArray& data);

public slots:
    void cacheInfoRequest(QObject* reciever, QString slot);
    MiniPromise::Promise cacheInfoRequestAsync(MiniPromise::Promise deferred = nullptr);
    MiniPromise::Promise queryCacheMetaAsync(const QUrl& url, MiniPromise::Promise deferred = nullptr);
    MiniPromise::Promise loadFromCacheAsync(const QUrl& url, MiniPromise::Promise deferred = nullptr);
    MiniPromise::Promise saveToCacheAsync(const QUrl& url, const QByteArray& data, const QVariantMap& metadata = QVariantMap(), MiniPromise::Promise deferred = nullptr);
    void clearCache();

private slots:
    void handleAssetMappingOperationReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleAssetGetInfoReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleAssetGetReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleAssetUploadReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);

    void handleNodeKilled(SharedNodePointer node);
    void handleNodeClientConnectionReset(SharedNodePointer node);

private:
    MessageID getAssetMapping(const AssetUtils::AssetHash& hash, MappingOperationCallback callback);
    MessageID getAllAssetMappings(MappingOperationCallback callback);
    MessageID setAssetMapping(const QString& path, const AssetUtils::AssetHash& hash, MappingOperationCallback callback);
    MessageID deleteAssetMappings(const AssetUtils::AssetPathList& paths, MappingOperationCallback callback);
    MessageID renameAssetMapping(const AssetUtils::AssetPath& oldPath, const AssetUtils::AssetPath& newPath, MappingOperationCallback callback);
    MessageID setBakingEnabled(const AssetUtils::AssetPathList& paths, bool enabled, MappingOperationCallback callback);

    MessageID getAssetInfo(const QString& hash, GetInfoCallback callback);
    MessageID getAsset(const QString& hash, AssetUtils::DataOffset start, AssetUtils::DataOffset end,
                  ReceivedAssetCallback callback, ProgressCallback progressCallback);
    MessageID uploadAsset(const QByteArray& data, UploadResultCallback callback);

    bool cancelMappingRequest(MessageID id);
    bool cancelGetAssetInfoRequest(MessageID id);
    bool cancelGetAssetRequest(MessageID id);
    bool cancelUploadAssetRequest(MessageID id);

    void handleProgressCallback(const QWeakPointer<Node>& node, MessageID messageID, qint64 size, AssetUtils::DataOffset length);
    void handleCompleteCallback(const QWeakPointer<Node>& node, MessageID messageID, AssetUtils::DataOffset length);

    void forceFailureOfPendingRequests(SharedNodePointer node);

    struct GetAssetRequestData {
        QSharedPointer<ReceivedMessage> message;
        ReceivedAssetCallback completeCallback;
        ProgressCallback progressCallback;
    };

    static MessageID _currentID;
    std::unordered_map<SharedNodePointer, std::unordered_map<MessageID, MappingOperationCallback>> _pendingMappingRequests;
    std::unordered_map<SharedNodePointer, std::unordered_map<MessageID, GetAssetRequestData>> _pendingRequests;
    std::unordered_map<SharedNodePointer, std::unordered_map<MessageID, GetInfoCallback>> _pendingInfoRequests;
    std::unordered_map<SharedNodePointer, std::unordered_map<MessageID, UploadResultCallback>> _pendingUploads;

    friend class AssetRequest;
    friend class AssetUpload;
    friend class MappingRequest;
    friend class GetMappingRequest;
    friend class GetAllMappingsRequest;
    friend class SetMappingRequest;
    friend class DeleteMappingsRequest;
    friend class RenameMappingRequest;
    friend class SetBakingEnabledRequest;
};

#endif
