//
//  AssetClient.cpp
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/21
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AssetClient.h"

#include <cstdint>

#include <QtCore/QBuffer>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtScript/QScriptEngine>
#include <QtNetwork/QNetworkDiskCache>

#include <shared/MiniPromises.h>

#include "AssetRequest.h"
#include "AssetUpload.h"
#include "AssetUtils.h"
#include "MappingRequest.h"
#include "NetworkAccessManager.h"
#include "NetworkLogging.h"
#include "NodeList.h"
#include "PacketReceiver.h"
#include "ResourceCache.h"

MessageID AssetClient::_currentID = 0;

AssetClient::AssetClient() {
    setCustomDeleter([](Dependency* dependency){
        static_cast<AssetClient*>(dependency)->deleteLater();
    });

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    auto& packetReceiver = nodeList->getPacketReceiver();

    packetReceiver.registerListener(PacketType::AssetMappingOperationReply,
        PacketReceiver::makeSourcedListenerReference<AssetClient>(this, &AssetClient::handleAssetMappingOperationReply));
    packetReceiver.registerListener(PacketType::AssetGetInfoReply,
        PacketReceiver::makeSourcedListenerReference<AssetClient>(this, &AssetClient::handleAssetGetInfoReply));
    packetReceiver.registerListener(PacketType::AssetGetReply,
        PacketReceiver::makeSourcedListenerReference<AssetClient>(this, &AssetClient::handleAssetGetReply), true);
    packetReceiver.registerListener(PacketType::AssetUploadReply,
        PacketReceiver::makeSourcedListenerReference<AssetClient>(this, &AssetClient::handleAssetUploadReply));

    connect(nodeList.data(), &LimitedNodeList::nodeKilled, this, &AssetClient::handleNodeKilled);
    connect(nodeList.data(), &LimitedNodeList::clientConnectionToNodeReset,
            this, &AssetClient::handleNodeClientConnectionReset);
}

namespace {
    const QString& CACHE_ERROR_MESSAGE{ "AssetClient::Error: %1 %2" };
}

/**jsdoc
 * Cache status value returned by {@link Assets.getCacheStatus}.
 * @typedef {object} Assets.GetCacheStatusResult
 * @property {string} cacheDirectory - The path of the cache directory.
 * @property {number} cacheSize - The current cache size, in bytes.
 * @property {number} maximumCacheSize - The maximum cache size, in bytes.
 */
MiniPromise::Promise AssetClient::cacheInfoRequestAsync(MiniPromise::Promise deferred) {
    if (!deferred) {
        deferred = makePromise(__FUNCTION__); // create on caller's thread
    }
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "cacheInfoRequestAsync", Q_ARG(MiniPromise::Promise, deferred));
    } else {
        auto cache = qobject_cast<QNetworkDiskCache*>(NetworkAccessManager::getInstance().cache());
        if (cache) {
            deferred->resolve({
                { "cacheDirectory", cache->cacheDirectory() },
                { "cacheSize", cache->cacheSize() },
                { "maximumCacheSize", cache->maximumCacheSize() },
            });
        } else {
            deferred->reject(CACHE_ERROR_MESSAGE.arg(__FUNCTION__).arg("cache unavailable"));
        }
    }
    return deferred;
}

/**jsdoc
 * Information on an asset in the cache. Value returned by {@link Assets.queryCacheMeta} and included in the data returned by 
 * {@link Assets.loadFromCache}.
 * @typedef {object} Assets.CacheItemMetaData
 * @property {object} [attributes] - The attributes that are stored with this cache item. <em>Not used.</em>
 * @property {Date} [expirationDate] - The date and time when the meta data expires. An invalid date means "never expires".
 * @property {boolean} isValid - <code>true</code> if the item specified in the URL is in the cache, <code>false</code> if
 *     it isn't.
 * @property {Date} [lastModified] - The date and time when the meta data was last modified.
 * @property {object} [rawHeaders] - The raw headers that are set in the meta data. <em>Not used.</em>
 * @property {boolean} [saveToDisk] - <code>true</code> if the cache item is allowed to be store on disk,
 *     <code>false</code> if it isn't.
 * @property {string} [url|metaDataURL] - The ATP URL of the cached item.
 */
MiniPromise::Promise AssetClient::queryCacheMetaAsync(const QUrl& url, MiniPromise::Promise deferred) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "queryCacheMetaAsync", Q_ARG(const QUrl&, url), Q_ARG(MiniPromise::Promise, deferred));
    } else {
        auto cache = NetworkAccessManager::getInstance().cache();
        if (cache) {
            QNetworkCacheMetaData metaData = cache->metaData(url);
            QVariantMap attributes, rawHeaders;
            if (!metaData.isValid()) {
                deferred->reject("invalid cache entry", {
                    { "_url", url },
                    { "isValid", metaData.isValid() },
                    { "metaDataURL", metaData.url() },
                });
            } else {
                auto metaAttributes = metaData.attributes();
                foreach(QNetworkRequest::Attribute k, metaAttributes.keys()) {
                    attributes[QString::number(k)] = metaAttributes[k];
                }
                for (const auto& i : metaData.rawHeaders()) {
                    rawHeaders[i.first] = i.second;
                }
                deferred->resolve({
                    { "_url", url },
                    { "isValid", metaData.isValid() },
                    { "url", metaData.url() },
                    { "expirationDate", metaData.expirationDate() },
                    { "lastModified", metaData.lastModified().toString().isEmpty() ? QDateTime() : metaData.lastModified() },
                    { "saveToDisk", metaData.saveToDisk() },
                    { "attributes", attributes },
                    { "rawHeaders", rawHeaders },
                });
            }
        } else {
            deferred->reject(CACHE_ERROR_MESSAGE.arg(__FUNCTION__).arg("cache unavailable"));
        }
    }
    return deferred;
}

MiniPromise::Promise AssetClient::loadFromCacheAsync(const QUrl& url, MiniPromise::Promise deferred) {
    auto errorMessage = CACHE_ERROR_MESSAGE.arg(__FUNCTION__);
    if (!deferred) {
        deferred = makePromise(__FUNCTION__); // create on caller's thread
    }
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "loadFromCacheAsync", Q_ARG(const QUrl&, url), Q_ARG(MiniPromise::Promise, deferred));
    } else {
        auto cache = NetworkAccessManager::getInstance().cache();
        if (cache) {
            MiniPromise::Promise metaRequest = makePromise(__FUNCTION__);
            queryCacheMetaAsync(url, metaRequest);
            metaRequest->finally([&](QString error, QVariantMap metadata) {
                QVariantMap result = {
                    { "url", url },
                    { "metadata", metadata },
                    { "data", QByteArray() },
                };
                if (!error.isEmpty()) {
                    deferred->reject(error, result);
                    return;
                }
                // caller is responsible for the deletion of the ioDevice, hence the unique_ptr
                auto ioDevice = std::unique_ptr<QIODevice>(cache->data(url));
                if (ioDevice) {
                    result["data"] = ioDevice->readAll();
                } else {
                    error = errorMessage.arg("error reading data");
                }
                deferred->handle(error, result);
            });
        } else {
           deferred->reject(errorMessage.arg("cache unavailable"));
        }
    }
    return deferred;
}

namespace {
    // parse RFC 1123 HTTP date format
    QDateTime parseHttpDate(const QString& dateString) {
        QDateTime dt = QDateTime::fromString(dateString.left(25), "ddd, dd MMM yyyy HH:mm:ss");
        if (!dt.isValid()) {
            dt = QDateTime::fromString(dateString, Qt::ISODateWithMs);
        }
        if (!dt.isValid()) {
            qDebug() << __FUNCTION__ << "unrecognized date format:" << dateString;
        }
        dt.setTimeSpec(Qt::UTC);
        return dt;
    }
    QDateTime getHttpDateValue(const QVariantMap& headers, const QString& keyName, const QDateTime& defaultValue) {
        return headers.contains(keyName) ? parseHttpDate(headers[keyName].toString()) : defaultValue;
    }
}

/**jsdoc
 * Last-modified and expiry times for a cache item.
 * @typedef {object} Assets.SaveToCacheHeaders
 * @property {string} [expires] - The date and time the cache value expires, in the format:
 *     <code>"ddd, dd MMM yyyy HH:mm:ss"</code>. The default value is an invalid date, representing "never expires".
 * @property {string} [last-modified] - The date and time the cache value was last modified, in the format:
 *     <code>"ddd, dd MMM yyyy HH:mm:ss"</code>. The default value is the current date and time.
 */
/**jsdoc
 * Information on saving asset data to the cache with {@link Assets.saveToCache}.
 * @typedef {object} Assets.SaveToCacheResult
 * @property {number} [byteLength] - The size of the cached data, in bytes.
 * @property {Date} [expirationDate] - The date and time that the cache item expires. An invalid date means "never expires".
 * @property {Date} [lastModified] - The date and time that the cache item was last modified.
 * @property {string} [metaDataURL] - The URL associated with the cache item.
 * @property {boolean} [success] - <code>true</code> if the save to cache request was successful.
 * @property {string} [url] - The URL associated with the cache item.
 */
MiniPromise::Promise AssetClient::saveToCacheAsync(const QUrl& url, const QByteArray& data, const QVariantMap& headers, MiniPromise::Promise deferred) {
    if (!deferred) {
        deferred = makePromise(__FUNCTION__); // create on caller's thread
    }
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, "saveToCacheAsync", Qt::QueuedConnection,
            Q_ARG(const QUrl&, url),
            Q_ARG(const QByteArray&, data),
            Q_ARG(const QVariantMap&, headers),
            Q_ARG(MiniPromise::Promise, deferred));
    } else {
        auto cache = NetworkAccessManager::getInstance().cache();
        if (cache) {
            QNetworkCacheMetaData metaData;
            metaData.setUrl(url);
            metaData.setSaveToDisk(true);
            metaData.setLastModified(getHttpDateValue(headers, "last-modified", QDateTime::currentDateTimeUtc()));
            metaData.setExpirationDate(getHttpDateValue(headers, "expires", QDateTime())); // nil defaultValue == never expires
            auto ioDevice = cache->prepare(metaData);
            if (ioDevice) {
                ioDevice->write(data);
                cache->insert(ioDevice);
                deferred->resolve({
                    { "url", url },
                    { "success", true },
                    { "metaDataURL", metaData.url() },
                    { "byteLength", data.size() },
                    { "expirationDate", metaData.expirationDate() },
                    { "lastModified", metaData.lastModified().toString().isEmpty() ? QDateTime() : metaData.lastModified() },
                });
            } else {
                auto error = QString("Could not save to disk cache");
                qCWarning(asset_client) << error;
                deferred->reject(CACHE_ERROR_MESSAGE.arg(__FUNCTION__).arg(error));
            }
        } else {
            deferred->reject(CACHE_ERROR_MESSAGE.arg(__FUNCTION__).arg("unavailable"));
        }
    }
    return deferred;
}

void AssetClient::cacheInfoRequest(QObject* reciever, QString slot) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "cacheInfoRequest", Qt::QueuedConnection,
                                  Q_ARG(QObject*, reciever), Q_ARG(QString, slot));
        return;
    }


    if (auto* cache = qobject_cast<QNetworkDiskCache*>(NetworkAccessManager::getInstance().cache())) {
        QMetaObject::invokeMethod(reciever, slot.toStdString().data(), Qt::QueuedConnection,
                                  Q_ARG(QString, cache->cacheDirectory()),
                                  Q_ARG(qint64, cache->cacheSize()),
                                  Q_ARG(qint64, cache->maximumCacheSize()));
    } else {
        qCWarning(asset_client) << "No disk cache to get info from.";
    }
}

void AssetClient::clearCache() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "clearCache", Qt::QueuedConnection);
        return;
    }

    if (auto cache = NetworkAccessManager::getInstance().cache()) {
        qInfo() << "AssetClient::clearCache(): Clearing disk cache.";
        cache->clear();
    } else {
        qCWarning(asset_client) << "No disk cache to clear.";
    }
}

void AssetClient::handleAssetMappingOperationReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    Q_ASSERT(QThread::currentThread() == thread());

    MessageID messageID;
    message->readPrimitive(&messageID);

    AssetUtils::AssetServerError error;
    message->readPrimitive(&error);

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingMappingRequests.find(senderNode);
    if (messageMapIt != _pendingMappingRequests.end()) {

        // Found the node, get the MessageID -> Callback map
        auto& messageCallbackMap = messageMapIt->second;

        // Check if we have this pending request
        auto requestIt = messageCallbackMap.find(messageID);
        if (requestIt != messageCallbackMap.end()) {
            auto callback = requestIt->second;
            callback(true, error, message);
            messageCallbackMap.erase(requestIt);
        }

        // Although the messageCallbackMap may now be empty, we won't delete the node until we have disconnected from
        // it to avoid constantly creating/deleting the map on subsequent requests.
    }
}

bool haveAssetServer() {
    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (!assetServer) {
        qCWarning(asset_client) << "Could not complete AssetClient operation "
            << "since you are not currently connected to an asset-server.";
        return false;
    }

    return true;
}

GetMappingRequest* AssetClient::createGetMappingRequest(const AssetUtils::AssetPath& path) {
    auto request = new GetMappingRequest(path);

    request->moveToThread(thread());

    return request;
}

GetAllMappingsRequest* AssetClient::createGetAllMappingsRequest() {
    auto request = new GetAllMappingsRequest();

    request->moveToThread(thread());

    return request;
}

DeleteMappingsRequest* AssetClient::createDeleteMappingsRequest(const AssetUtils::AssetPathList& paths) {
    auto request = new DeleteMappingsRequest(paths);

    request->moveToThread(thread());

    return request;
}

SetMappingRequest* AssetClient::createSetMappingRequest(const AssetUtils::AssetPath& path, const AssetUtils::AssetHash& hash) {
    auto request = new SetMappingRequest(path, hash);

    request->moveToThread(thread());

    return request;
}

RenameMappingRequest* AssetClient::createRenameMappingRequest(const AssetUtils::AssetPath& oldPath, const AssetUtils::AssetPath& newPath) {
    auto request = new RenameMappingRequest(oldPath, newPath);

    request->moveToThread(thread());

    return request;
}

SetBakingEnabledRequest* AssetClient::createSetBakingEnabledRequest(const AssetUtils::AssetPathList& path, bool enabled) {
    auto bakingEnabledRequest = new SetBakingEnabledRequest(path, enabled);

    bakingEnabledRequest->moveToThread(thread());

    return bakingEnabledRequest;
}

AssetRequest* AssetClient::createRequest(const AssetUtils::AssetHash& hash, const ByteRange& byteRange) {
    auto request = new AssetRequest(hash, byteRange);

    // Move to the AssetClient thread in case we are not currently on that thread (which will usually be the case)
    request->moveToThread(thread());

    return request;
}

AssetUpload* AssetClient::createUpload(const QString& filename) {
    auto upload = new AssetUpload(filename);

    upload->moveToThread(thread());

    return upload;
}

AssetUpload* AssetClient::createUpload(const QByteArray& data) {
    auto upload = new AssetUpload(data);

    upload->moveToThread(thread());

    return upload;
}

MessageID AssetClient::getAsset(const QString& hash, AssetUtils::DataOffset start, AssetUtils::DataOffset end,
                                ReceivedAssetCallback callback, ProgressCallback progressCallback) {
    Q_ASSERT(QThread::currentThread() == thread());

    if (hash.length() != AssetUtils::SHA256_HASH_HEX_LENGTH) {
        qCWarning(asset_client) << "Invalid hash size";
        return false;
    }

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {

        auto messageID = ++_currentID;

        auto payloadSize = sizeof(messageID) + AssetUtils::SHA256_HASH_LENGTH + sizeof(start) + sizeof(end);
        auto packet = NLPacket::create(PacketType::AssetGet, payloadSize, true);

        qCDebug(asset_client) << "Requesting data from" << start << "to" << end << "of" << hash << "from asset-server.";

        packet->writePrimitive(messageID);

        packet->write(QByteArray::fromHex(hash.toLatin1()));

        packet->writePrimitive(start);
        packet->writePrimitive(end);

        if (nodeList->sendPacket(std::move(packet), *assetServer) != -1) {
            _pendingRequests[assetServer][messageID] = { QSharedPointer<ReceivedMessage>(), callback, progressCallback };

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QByteArray());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::getAssetInfo(const QString& hash, GetInfoCallback callback) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto messageID = ++_currentID;

        auto payloadSize = sizeof(messageID) + AssetUtils::SHA256_HASH_LENGTH;
        auto packet = NLPacket::create(PacketType::AssetGetInfo, payloadSize, true);

        packet->writePrimitive(messageID);
        packet->write(QByteArray::fromHex(hash.toLatin1()));

        if (nodeList->sendPacket(std::move(packet), *assetServer) != -1) {
            _pendingInfoRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, { "", 0 });
    return INVALID_MESSAGE_ID;
}

void AssetClient::handleAssetGetInfoReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    Q_ASSERT(QThread::currentThread() == thread());

    MessageID messageID;
    message->readPrimitive(&messageID);
    auto assetHash = message->read(AssetUtils::SHA256_HASH_LENGTH);

    AssetUtils::AssetServerError error;
    message->readPrimitive(&error);

    AssetInfo info { assetHash.toHex(), 0 };

    if (error == AssetUtils::AssetServerError::NoError) {
        message->readPrimitive(&info.size);
    }

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingInfoRequests.find(senderNode);
    if (messageMapIt != _pendingInfoRequests.end()) {

        // Found the node, get the MessageID -> Callback map
        auto& messageCallbackMap = messageMapIt->second;

        // Check if we have this pending request
        auto requestIt = messageCallbackMap.find(messageID);
        if (requestIt != messageCallbackMap.end()) {
            auto callback = requestIt->second;
            callback(true, error, info);
            messageCallbackMap.erase(requestIt);
        }

        // Although the messageCallbackMap may now be empty, we won't delete the node until we have disconnected from
        // it to avoid constantly creating/deleting the map on subsequent requests.
    }
}

void AssetClient::handleAssetGetReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto assetHash = message->readHead(AssetUtils::SHA256_HASH_LENGTH);
    qCDebug(asset_client) << "Got reply for asset: " << assetHash.toHex();

    MessageID messageID;
    message->readHeadPrimitive(&messageID);

    AssetUtils::AssetServerError error;
    message->readHeadPrimitive(&error);

    AssetUtils::DataOffset length = 0;
    if (!error) {
        message->readHeadPrimitive(&length);
    } else {
        qCWarning(asset_client) << "Failure getting asset: " << error;
    }

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingRequests.find(senderNode);
    if (messageMapIt == _pendingRequests.end()) {
        return;
    }

    // Found the node, get the MessageID -> Callback map
    auto& messageCallbackMap = messageMapIt->second;

    // Check if we have this pending request
    auto requestIt = messageCallbackMap.find(messageID);
    if (requestIt == messageCallbackMap.end()) {
        // Although the messageCallbackMap may now be empty, we won't delete the node until we have disconnected from
        // it to avoid constantly creating/deleting the map on subsequent requests.
        return;
    }

    auto& callbacks = requestIt->second;

    // Store message in case we need to disconnect from it later.
    callbacks.message = message;


    auto weakNode = senderNode.toWeakRef();
    connect(message.data(), &ReceivedMessage::progress, this, [this, weakNode, messageID, length](qint64 size) {
        handleProgressCallback(weakNode, messageID, size, length);
    });
    connect(message.data(), &ReceivedMessage::completed, this, [this, weakNode, messageID, length]() {
        handleCompleteCallback(weakNode, messageID, length);
    });

    if (message->isComplete()) {
        disconnect(message.data(), nullptr, this, nullptr);

        if (length != message->getBytesLeftToRead()) {
            callbacks.completeCallback(false, error, QByteArray());
        } else {
            callbacks.completeCallback(true, error, message->readAll());
        }


        messageCallbackMap.erase(requestIt);
    }
}

void AssetClient::handleProgressCallback(const QWeakPointer<Node>& node, MessageID messageID,
                                         qint64 size, AssetUtils::DataOffset length) {
    auto senderNode = node.toStrongRef();

    if (!senderNode) {
        return;
    }

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingRequests.find(senderNode);
    if (messageMapIt == _pendingRequests.end()) {
        return;
    }

    // Found the node, get the MessageID -> Callback map
    auto& messageCallbackMap = messageMapIt->second;

    // Check if we have this pending request
    auto requestIt = messageCallbackMap.find(messageID);
    if (requestIt == messageCallbackMap.end()) {
        return;
    }

    auto& callbacks = requestIt->second;
    callbacks.progressCallback(size, length);
}

void AssetClient::handleCompleteCallback(const QWeakPointer<Node>& node, MessageID messageID, AssetUtils::DataOffset length) {
    auto senderNode = node.toStrongRef();

    if (!senderNode) {
        qCWarning(asset_client) << "Got completed asset for node that no longer exists";
        return;
    }

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingRequests.find(senderNode);
    if (messageMapIt == _pendingRequests.end()) {
        qCWarning(asset_client) << "Got completed asset for a node that doesn't have any pending requests";
        return;
    }

    // Found the node, get the MessageID -> Callback map
    auto& messageCallbackMap = messageMapIt->second;

    // Check if we have this pending request
    auto requestIt = messageCallbackMap.find(messageID);
    if (requestIt == messageCallbackMap.end()) {
        qCWarning(asset_client) << "Got completed asset for a request that doesn't exist";
        return;
    }

    auto& callbacks = requestIt->second;
    auto& message = callbacks.message;

    if (!message) {
        qCWarning(asset_client) << "Got completed asset for a message that doesn't exist";
        return;
    }

    if (message->failed() || length != message->getBytesLeftToRead()) {
        callbacks.completeCallback(false, AssetUtils::AssetServerError::NoError, QByteArray());
    } else {
        callbacks.completeCallback(true, AssetUtils::AssetServerError::NoError, message->readAll());
    }

    // We should never get to this point without the associated senderNode and messageID
    // in our list of pending requests. If the senderNode had disconnected or the message
    // had been canceled, we should have been disconnected from the ReceivedMessage
    // signals and thus never had this lambda called.
    messageCallbackMap.erase(messageID);
}


MessageID AssetClient::getAssetMapping(const AssetUtils::AssetPath& path, MappingOperationCallback callback) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::Get);

        packetList->writeString(path);

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::getAllAssetMappings(MappingOperationCallback callback) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::GetAll);

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::deleteAssetMappings(const AssetUtils::AssetPathList& paths, MappingOperationCallback callback) {
    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::Delete);

        packetList->writePrimitive(int(paths.size()));

        for (auto& path: paths) {
            packetList->writeString(path);
        }

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::setAssetMapping(const QString& path, const AssetUtils::AssetHash& hash, MappingOperationCallback callback) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::Set);

        packetList->writeString(path);
        packetList->write(QByteArray::fromHex(hash.toUtf8()));

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::renameAssetMapping(const AssetUtils::AssetPath& oldPath, const AssetUtils::AssetPath& newPath, MappingOperationCallback callback) {
    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::Rename);

        packetList->writeString(oldPath);
        packetList->writeString(newPath);

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;

        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

MessageID AssetClient::setBakingEnabled(const AssetUtils::AssetPathList& paths, bool enabled, MappingOperationCallback callback) {
    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetMappingOperation, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        packetList->writePrimitive(AssetUtils::AssetMappingOperationType::SetBakingEnabled);

        packetList->writePrimitive(enabled);

        packetList->writePrimitive(int(paths.size()));

        for (auto& path : paths) {
            packetList->writeString(path);
        }

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingMappingRequests[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
    return INVALID_MESSAGE_ID;
}

bool AssetClient::cancelMappingRequest(MessageID id) {
    Q_ASSERT(QThread::currentThread() == thread());

    for (auto& kv : _pendingMappingRequests) {
        if (kv.second.erase(id)) {
            return true;
        }
    }
    return false;
}

bool AssetClient::cancelGetAssetInfoRequest(MessageID id) {
    Q_ASSERT(QThread::currentThread() == thread());

    for (auto& kv : _pendingInfoRequests) {
        if (kv.second.erase(id)) {
            return true;
        }
    }
    return false;
}

bool AssetClient::cancelGetAssetRequest(MessageID id) {
    Q_ASSERT(QThread::currentThread() == thread());

    // Search through each pending mapping request for id `id`
    for (auto& kv : _pendingRequests) {
        auto& messageCallbackMap = kv.second;
        auto requestIt = messageCallbackMap.find(id);
        if (requestIt != messageCallbackMap.end()) {

            auto& message = requestIt->second.message;
            if (message) {
                // disconnect from all signals emitting from the pending message
                disconnect(message.data(), nullptr, this, nullptr);
            }

            messageCallbackMap.erase(requestIt);

            return true;
        }
    }
    return false;
}

bool AssetClient::cancelUploadAssetRequest(MessageID id) {
    Q_ASSERT(QThread::currentThread() == thread());

    // Search through each pending mapping request for id `id`
    for (auto& kv : _pendingUploads) {
        if (kv.second.erase(id)) {
            return true;
        }
    }
    return false;
}

MessageID AssetClient::uploadAsset(const QByteArray& data, UploadResultCallback callback) {
    Q_ASSERT(QThread::currentThread() == thread());

    auto nodeList = DependencyManager::get<LimitedNodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    if (assetServer) {
        auto packetList = NLPacketList::create(PacketType::AssetUpload, QByteArray(), true, true);

        auto messageID = ++_currentID;
        packetList->writePrimitive(messageID);

        uint64_t size = data.length();
        packetList->writePrimitive(size);
        packetList->write(data.constData(), size);

        if (nodeList->sendPacketList(std::move(packetList), *assetServer) != -1) {
            _pendingUploads[assetServer][messageID] = callback;

            return messageID;
        }
    }

    callback(false, AssetUtils::AssetServerError::NoError, QString());
    return INVALID_MESSAGE_ID;
}

void AssetClient::handleAssetUploadReply(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    Q_ASSERT(QThread::currentThread() == thread());

    MessageID messageID;
    message->readPrimitive(&messageID);

    AssetUtils::AssetServerError error;
    message->readPrimitive(&error);

    QString hashString;

    if (error) {
        qCWarning(asset_client) << "Error uploading file to asset server";
    } else {
        auto hash = message->read(AssetUtils::SHA256_HASH_LENGTH);
        hashString = hash.toHex();

        qCDebug(asset_client) << "Successfully uploaded asset to asset-server - SHA256 hash is " << hashString;
    }

    // Check if we have any pending requests for this node
    auto messageMapIt = _pendingUploads.find(senderNode);
    if (messageMapIt != _pendingUploads.end()) {

        // Found the node, get the MessageID -> Callback map
        auto& messageCallbackMap = messageMapIt->second;

        // Check if we have this pending request
        auto requestIt = messageCallbackMap.find(messageID);
        if (requestIt != messageCallbackMap.end()) {
            auto callback = requestIt->second;
            callback(true, error, hashString);
            messageCallbackMap.erase(requestIt);
        }

        // Although the messageCallbackMap may now be empty, we won't delete the node until we have disconnected from
        // it to avoid constantly creating/deleting the map on subsequent requests.
    }
}

void AssetClient::handleNodeKilled(SharedNodePointer node) {
    Q_ASSERT(QThread::currentThread() == thread());

    if (node->getType() != NodeType::AssetServer) {
        return;
    }

    forceFailureOfPendingRequests(node);

    {
        auto messageMapIt = _pendingUploads.find(node);
        if (messageMapIt != _pendingUploads.end()) {
            for (const auto& value : messageMapIt->second) {
                value.second(false, AssetUtils::AssetServerError::NoError, "");
            }
            messageMapIt->second.clear();
        }
    }
}

void AssetClient::handleNodeClientConnectionReset(SharedNodePointer node) {
    // a client connection to a Node was reset
    // if it was an AssetServer we need to cause anything pending to fail so it is re-attempted

    if (node->getType() != NodeType::AssetServer) {
        return;
    }

    qCDebug(asset_client) << "AssetClient detected client connection reset handshake with Asset Server - failing any pending requests";

    forceFailureOfPendingRequests(node);
}

void AssetClient::forceFailureOfPendingRequests(SharedNodePointer node) {

    {
        auto messageMapIt = _pendingRequests.find(node);
        if (messageMapIt != _pendingRequests.end()) {
            for (const auto& value : messageMapIt->second) {
                auto& message = value.second.message;
                if (message) {
                    // Disconnect from all signals emitting from the pending message
                    disconnect(message.data(), nullptr, this, nullptr);
                }

                value.second.completeCallback(false, AssetUtils::AssetServerError::NoError, QByteArray());
            }
            messageMapIt->second.clear();
        }
    }

    {
        auto messageMapIt = _pendingInfoRequests.find(node);
        if (messageMapIt != _pendingInfoRequests.end()) {
            AssetInfo info { "", 0 };
            for (const auto& value : messageMapIt->second) {
                value.second(false, AssetUtils::AssetServerError::NoError, info);
            }
            messageMapIt->second.clear();
        }
    }

    {
        auto messageMapIt = _pendingMappingRequests.find(node);
        if (messageMapIt != _pendingMappingRequests.end()) {
            for (const auto& value : messageMapIt->second) {
                value.second(false, AssetUtils::AssetServerError::NoError, QSharedPointer<ReceivedMessage>());
            }
            messageMapIt->second.clear();
        }
    }
}
