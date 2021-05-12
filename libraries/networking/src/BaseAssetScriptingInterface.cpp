//
//  BaseAssetScriptingInterface.cpp
//  libraries/networking/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

#include "BaseAssetScriptingInterface.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QThread>

#include "AssetRequest.h"
#include "AssetUpload.h"
#include "AssetUtils.h"
#include "MappingRequest.h"
#include "NetworkLogging.h"

#include <RegisteredMetaTypes.h>

#include <shared/QtHelpers.h>
#include "Gzip.h"

using Promise = MiniPromise::Promise;

QSharedPointer<AssetClient> BaseAssetScriptingInterface::assetClient() {
    auto client = DependencyManager::get<AssetClient>();
    Q_ASSERT(client);
    if (!client) {
        qDebug() << "BaseAssetScriptingInterface::assetClient unavailable";
    }
    return client;
}

BaseAssetScriptingInterface::BaseAssetScriptingInterface(QObject* parent) : QObject(parent) {}

bool BaseAssetScriptingInterface::initializeCache() {
    if (!assetClient()) {
        return false; // not yet possible to initialize the cache
    }
    if (_cacheReady) {
        return true; // cache is ready
    }

    Promise deferred = makePromise("BaseAssetScriptingInterface--queryCacheStatus");
    deferred->then([this](QVariantMap result) {
        _cacheReady = !result.value("cacheDirectory").toString().isEmpty();
    });
    deferred->fail([](QString error) {
        qDebug() << "BaseAssetScriptingInterface::queryCacheStatus ERROR" << QThread::currentThread() << error;
    });
    assetClient()->cacheInfoRequestAsync(deferred);
    return false; // cache is not ready yet
}

Promise BaseAssetScriptingInterface::getCacheStatus() {
    return assetClient()->cacheInfoRequestAsync(makePromise(__FUNCTION__));
}

Promise BaseAssetScriptingInterface::queryCacheMeta(const QUrl& url) {
    return assetClient()->queryCacheMetaAsync(url, makePromise(__FUNCTION__));
}

/**jsdoc
 * Data and information returned by {@link Assets.loadFromCache}.
 * @typedef {object} Assets.LoadFromCacheResult
 * @property {number} [byteLength] - The number of bytes in the retrieved data.
 * @property {string} [contentType] - The automatically detected MIME type of the content. 
 * @property {ArrayBuffer} data - The data bytes.
 * @property {Assets.CacheItemMetaData} metadata - Information on the cache item.
 * @property {string|object|ArrayBuffer} [response] - The content of the response.
 * @property {Assets.ResponseType} responseType - The type of the content in <code>response</code>.
 * @property {string} url - The URL of the cache item.
 */
Promise BaseAssetScriptingInterface::loadFromCache(const QUrl& url, bool decompress, const QString& responseType) {
    QVariantMap metaData = {
        { "_type", "cache" },
        { "url", url },
        { "responseType", responseType },
    };

    Promise completed = makePromise("loadFromCache::completed");
    Promise fetched = makePromise("loadFromCache::fetched");

    Promise downloaded = assetClient()->loadFromCacheAsync(url, makePromise("loadFromCache-retrieval"));
    downloaded->mixin(metaData);
    downloaded->fail(fetched);

    if (decompress) {
        downloaded->then([=](QVariantMap result) {
            fetched->mixin(result);
            Promise decompressed = decompressBytes(result.value("data").toByteArray());
            decompressed->mixin(result);
            decompressed->ready(fetched);
        });
    } else {
        downloaded->then(fetched);
    }

    fetched->fail(completed);
    fetched->then([=](QVariantMap result) {
        Promise converted = convertBytes(result.value("data").toByteArray(), responseType);
        converted->mixin(result);
        converted->ready(completed);
    });

    return completed;
}

Promise BaseAssetScriptingInterface::saveToCache(const QUrl& url, const QByteArray& data, const QVariantMap& headers) {
    return assetClient()->saveToCacheAsync(url, data, headers, makePromise(__FUNCTION__));
}

Promise BaseAssetScriptingInterface::loadAsset(QString asset, bool decompress, QString responseType) {
    auto hash = AssetUtils::extractAssetHash(asset);
    auto url = AssetUtils::getATPUrl(hash).toString();

    QVariantMap metaData = {
        { "_asset", asset },
        { "_type", "download" },
        { "hash", hash },
        { "url", url },
        { "responseType", responseType },
    };

    Promise completed = makePromise("loadAsset::completed");
    Promise fetched = makePromise("loadAsset::fetched");

    Promise downloaded = downloadBytes(hash);
    downloaded->mixin(metaData);
    downloaded->fail(fetched);

    if (decompress) {
        downloaded->then([=](QVariantMap result) {
            Q_ASSERT(thread() == QThread::currentThread());
            fetched->mixin(result);
            Promise decompressed = decompressBytes(result.value("data").toByteArray());
            decompressed->mixin(result);
            decompressed->ready(fetched);
        });
    } else {
        downloaded->then(fetched);
    }

    fetched->fail(completed);
    fetched->then([=](QVariantMap result) {
        Promise converted = convertBytes(result.value("data").toByteArray(), responseType);
        converted->mixin(result);
        converted->ready(completed);
    });

    return completed;
}

Promise BaseAssetScriptingInterface::convertBytes(const QByteArray& dataByteArray, const QString& responseType) {
    QVariantMap result = {
        { "_contentType", QMimeDatabase().mimeTypeForData(dataByteArray).name() },
        { "_byteLength", dataByteArray.size() },
        { "_responseType", responseType },
    };
    QString error;
    Promise conversion = makePromise(__FUNCTION__);
    if (!RESPONSE_TYPES.contains(responseType)) {
        error = QString("convertBytes: invalid responseType: '%1' (expected: %2)").arg(responseType).arg(RESPONSE_TYPES.join(" | "));
    } else if (responseType == "arraybuffer") {
        // interpret as bytes
        result["response"] = dataByteArray;
    } else if (responseType == "text") {
        // interpret as utf-8 text
        result["response"] = QString::fromUtf8(dataByteArray);
    } else if (responseType == "json") {
        // interpret as JSON
        QJsonParseError status;
        auto parsed = QJsonDocument::fromJson(dataByteArray, &status);
        if (status.error == QJsonParseError::NoError) {
            result["response"] = parsed.isArray() ? QVariant(parsed.array().toVariantList()) : QVariant(parsed.object().toVariantMap());
        } else {
            result = {
                { "error", status.error },
                { "offset", status.offset },
            };
            error = "JSON Parse Error: " + status.errorString();
        }
    }
    if (result.value("response").canConvert<QByteArray>()) {
        auto data = result.value("response").toByteArray();
        result["contentType"] = QMimeDatabase().mimeTypeForData(data).name();
        result["byteLength"] = data.size();
        result["responseType"] = responseType;
    }
    return conversion->handle(error, result);
}

Promise BaseAssetScriptingInterface::decompressBytes(const QByteArray& dataByteArray) {
    QByteArray inflated;
    Promise decompressed = makePromise(__FUNCTION__);
    auto start = usecTimestampNow();
    if (gunzip(dataByteArray, inflated)) {
        auto end = usecTimestampNow();
        decompressed->resolve({
            { "_compressedByteLength", dataByteArray.size() },
            { "_compressedContentType", QMimeDatabase().mimeTypeForData(dataByteArray).name() },
            { "_compressMS", (double)(end - start) / 1000.0 },
            { "decompressed", true },
            { "byteLength", inflated.size() },
            { "contentType", QMimeDatabase().mimeTypeForData(inflated).name() },
            { "data", inflated },
       });
    } else {
        decompressed->reject("gunzip error");
    }
    return decompressed;
}

Promise BaseAssetScriptingInterface::compressBytes(const QByteArray& dataByteArray, int level) {
    QByteArray deflated;
    auto start = usecTimestampNow();
    Promise compressed = makePromise(__FUNCTION__);
    if (gzip(dataByteArray, deflated, level)) {
        auto end = usecTimestampNow();
        compressed->resolve({
            { "_uncompressedByteLength", dataByteArray.size() },
            { "_uncompressedContentType", QMimeDatabase().mimeTypeForData(dataByteArray).name() },
            { "_compressMS", (double)(end - start) / 1000.0 },
            { "compressed", true },
            { "byteLength", deflated.size() },
            { "contentType", QMimeDatabase().mimeTypeForData(deflated).name() },
            { "data", deflated },
       });
    } else {
        compressed->reject("gzip error", {});
    }
    return compressed;
}

Promise BaseAssetScriptingInterface::downloadBytes(QString hash) {
    QPointer<AssetRequest> assetRequest = assetClient()->createRequest(hash);
    Promise deferred = makePromise(__FUNCTION__);

    QObject::connect(assetRequest, &AssetRequest::finished, assetRequest, [deferred](AssetRequest* request) {
        // note: we are now on the "Resource Manager" thread
        Q_ASSERT(QThread::currentThread() == request->thread());
        Q_ASSERT(request->getState() == AssetRequest::Finished);
        QString error;
        QVariantMap result;
        if (request->getError() == AssetRequest::Error::NoError) {
            QByteArray data = request->getData();
            result = {
                { "url", request->getUrl() },
                { "hash", request->getHash() },
                { "cached", request->loadedFromCache() },
                { "contentType", QMimeDatabase().mimeTypeForData(data).name() },
                { "data", data },
            };
        } else {
            error = request->getError();
            result = { { "error", request->getError() } };
        }
        // forward thread-safe copies back to our thread
        deferred->handle(error, result);
        request->deleteLater();
    });
    assetRequest->start();
    return deferred;
}

Promise BaseAssetScriptingInterface::uploadBytes(const QByteArray& bytes) {
    Promise deferred = makePromise(__FUNCTION__);
    QPointer<AssetUpload> upload = assetClient()->createUpload(bytes);

    const auto byteLength = bytes.size();
    QObject::connect(upload, &AssetUpload::finished, upload, [=](AssetUpload* upload, const QString& hash) {
        Q_ASSERT(QThread::currentThread() == upload->thread());
        // note: we are now on the "Resource Manager" thread
        QString error;
        QVariantMap result;
        if (upload->getError() == AssetUpload::NoError) {
            result = {
                { "hash", hash },
                { "url", AssetUtils::getATPUrl(hash).toString() },
                { "filename", upload->getFilename() },
                { "byteLength", byteLength },
            };
        } else {
            error = upload->getErrorString();
            result = { { "error", upload->getError() } };
        }
        // forward thread-safe copies back to our thread
        deferred->handle(error, result);
        upload->deleteLater();
    });
    upload->start();
    return deferred;
}

Promise BaseAssetScriptingInterface::getAssetInfo(QString asset) {
    Promise deferred = makePromise(__FUNCTION__);
    auto url = AssetUtils::getATPUrl(asset);
    auto path = url.path();
    auto hash = AssetUtils::extractAssetHash(asset);
    if (AssetUtils::isValidHash(hash)) {
        // already a valid ATP hash -- nothing to do
        deferred->resolve({
            { "hash", hash },
            { "path", path },
            { "url", url },
        });
    } else if (AssetUtils::isValidFilePath(path)) {
        QPointer<GetMappingRequest> request = assetClient()->createGetMappingRequest(path);

        QObject::connect(request, &GetMappingRequest::finished, request, [=]() {
            Q_ASSERT(QThread::currentThread() == request->thread());
            // note: we are now on the "Resource Manager" thread
            QString error;
            QVariantMap result;
            if (request->getError() == GetMappingRequest::NoError) {
                result = {
                    { "_hash", hash },
                    { "_path", path },
                    { "_url", url },
                    { "url", url },
                    { "hash", request->getHash() },
                    { "hashURL", AssetUtils::getATPUrl(request->getHash()).toString() },
                    { "wasRedirected", request->wasRedirected() },
                    { "path", request->wasRedirected() ? request->getRedirectedPath() : path },
                };
            } else {
                error = request->getErrorString();
                result = { { "error", request->getError() } };
            }
            // forward thread-safe copies back to our thread
            deferred->handle(error, result);
            request->deleteLater();
        });
        request->start();
    } else {
        deferred->reject("invalid ATP file path: " + asset + "("+path+")", {});
    }
    return deferred;
}

Promise BaseAssetScriptingInterface::symlinkAsset(QString hash, QString path) {
    auto deferred = makePromise(__FUNCTION__);
    QPointer<SetMappingRequest> setMappingRequest = assetClient()->createSetMappingRequest(path, hash);

    connect(setMappingRequest, &SetMappingRequest::finished, setMappingRequest, [=](SetMappingRequest* request) {
        Q_ASSERT(QThread::currentThread() == request->thread());
        // we are now on the "Resource Manager" thread
        QString error;
        QVariantMap result;
        if (request->getError() == SetMappingRequest::NoError) {
            result = {
                { "_hash", hash },
                { "_path", path },
                { "hash", request->getHash() },
                { "path", request->getPath() },
                { "url", AssetUtils::getATPUrl(request->getPath()).toString() },
            };
        } else {
            error = request->getErrorString();
            result = { { "error", request->getError() } };
        }
        // forward results back to our thread
        deferred->handle(error, result);
        request->deleteLater();
    });
    setMappingRequest->start();
    return deferred;
}
