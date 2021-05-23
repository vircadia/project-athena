//
//  AssetScriptingInterface.cpp
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2016-03-08.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AssetScriptingInterface.h"

#include <QMimeDatabase>
#include <QThread>

#include <AssetRequest.h>
#include <AssetUpload.h>
#include <AssetUtils.h>
#include <MappingRequest.h>
#include <NodeList.h>

#include <RegisteredMetaTypes.h>
#include "ScriptEngine.h"
#include "ScriptEngineCast.h"
#include "ScriptEngineLogging.h"
#include "ScriptManager.h"
#include "ScriptValue.h"
#include "ScriptValueUtils.h"

#include <shared/QtHelpers.h>
#include <Gzip.h>

#include <future>

using Promise = MiniPromise::Promise;

AssetScriptingInterface::AssetScriptingInterface(QObject* parent) : BaseAssetScriptingInterface(parent) {
    qCDebug(scriptengine) << "AssetScriptingInterface::AssetScriptingInterface" << parent;

    auto scriptManager = qobject_cast<ScriptManager*>(parent);
    scriptRegisterMetaType(scriptManager->engine().get(), promiseToScriptValue, promiseFromScriptValue);
}

#define JS_VERIFY(cond, error) { if (!this->jsVerify(cond, error)) { return; } }

bool AssetScriptingInterface::initializeCache() {
    if (!Parent::initializeCache()) {
        if (assetClient()) {
            std::promise<bool> cacheStatusResult;
            Promise assetClientPromise(makePromise(__func__));
            assetClientPromise->moveToThread(qApp->thread());  // To ensure the finally() is processed.

            assetClient()->cacheInfoRequestAsync(assetClientPromise);
            assetClientPromise->finally([&](QString, QVariantMap result)
                { cacheStatusResult.set_value(!result.isEmpty()); });
            return cacheStatusResult.get_future().get();
        } else {
            return false;
        }
    } else {
        return true;
    }
}

void AssetScriptingInterface::uploadData(QString data, ScriptValuePointer callback) {
    auto handler = jsBindCallback(thisObject(), callback);
    QByteArray dataByteArray = data.toUtf8();
    auto upload = DependencyManager::get<AssetClient>()->createUpload(dataByteArray);

    Promise deferred = makePromise(__FUNCTION__);
    auto scriptEngine = engine();
    deferred->ready([=](QString error, QVariantMap result) {
        auto url = result.value("url").toString();
        auto hash = result.value("hash").toString();
        jsCallback(handler, scriptEngine->newValue(url), scriptEngine->newValue(hash));
    });

    connect(upload, &AssetUpload::finished, upload, [deferred](AssetUpload* upload, const QString& hash) {
        // we are now on the "Resource Manager" thread (and "hash" being a *reference* makes it unsafe to use directly)
        Q_ASSERT(QThread::currentThread() == upload->thread());
        deferred->resolve({
            { "url", "atp:" + hash },
            { "hash", hash },
        });
        upload->deleteLater();
    });
    upload->start();
}

void AssetScriptingInterface::setMapping(QString path, QString hash, ScriptValuePointer callback) {
    auto handler = jsBindCallback(thisObject(), callback);
    auto setMappingRequest = assetClient()->createSetMappingRequest(path, hash);
    Promise deferred = makePromise(__FUNCTION__);
    auto scriptEngine = engine();
    deferred->ready([=](QString error, QVariantMap result) {
        jsCallback(handler, scriptEngine->newValue(error), result);
    });

    connect(setMappingRequest, &SetMappingRequest::finished, setMappingRequest, [deferred](SetMappingRequest* request) {
        Q_ASSERT(QThread::currentThread() == request->thread());
        // we are now on the "Resource Manager" thread
        QString error = request->getErrorString();
        // forward a thread-safe values back to our thread
        deferred->handle(error, { { "error", request->getError() } });
        request->deleteLater();
    });
    setMappingRequest->start();
}

/*@jsdoc
 * The success or failure of an {@link Assets.downloadData} call.
 * @typedef {object} Assets.DownloadDataError
 * @property {string} errorMessage - <code>""</code> if the download was successful, otherwise a description of the error.
 */
void AssetScriptingInterface::downloadData(QString urlString, ScriptValuePointer callback) {
    // FIXME: historically this API method failed silently when given a non-atp prefixed
    //   urlString (or if the AssetRequest failed).
    // .. is that by design or could we update without breaking things to provide better feedback to scripts?

    if (!urlString.startsWith(ATP_SCHEME)) {
        // ... for now at least log a message so user can check logs
        qCDebug(scriptengine) << "AssetScriptingInterface::downloadData url must be of form atp:<hash-value>";
        return;
    }
    QString hash = AssetUtils::extractAssetHash(urlString);
    auto handler = jsBindCallback(thisObject(), callback);
    auto assetClient = DependencyManager::get<AssetClient>();
    auto assetRequest = assetClient->createRequest(hash);

    Promise deferred = makePromise(__FUNCTION__);
    auto scriptEngine = engine();
    deferred->ready([=](QString error, QVariantMap result) {
        // FIXME: to remain backwards-compatible the signature here is "callback(data, n/a)"
        jsCallback(handler, scriptEngine->newValue(result.value("data").toString()), { { "errorMessage", error } });
    });

    connect(assetRequest, &AssetRequest::finished, assetRequest, [deferred](AssetRequest* request) {
        Q_ASSERT(QThread::currentThread() == request->thread());
        // we are now on the "Resource Manager" thread
        Q_ASSERT(request->getState() == AssetRequest::Finished);

        if (request->getError() == AssetRequest::Error::NoError) {
            QString data = QString::fromUtf8(request->getData());
            // forward a thread-safe values back to our thread
            deferred->resolve({ { "data", data } });
        } else {
            // FIXME: propagate error to scripts? (requires changing signature or inverting param order above..)
            //deferred->resolve(request->getErrorString(), { { "error", requet->getError() } });
            qCDebug(scriptengine) << "AssetScriptingInterface::downloadData ERROR: " << request->getErrorString();
        }

        request->deleteLater();
    });

    assetRequest->start();
}

void AssetScriptingInterface::setBakingEnabled(QString path, bool enabled, ScriptValuePointer callback) {
    auto setBakingEnabledRequest = DependencyManager::get<AssetClient>()->createSetBakingEnabledRequest({ path }, enabled);

    Promise deferred = jsPromiseReady(makePromise(__FUNCTION__), thisObject(), callback);
    if (!deferred) {
        return;
    }

    connect(setBakingEnabledRequest, &SetBakingEnabledRequest::finished, setBakingEnabledRequest, [deferred](SetBakingEnabledRequest* request) {
        Q_ASSERT(QThread::currentThread() == request->thread());
        // we are now on the "Resource Manager" thread

        QString error = request->getErrorString();
        // forward thread-safe values back to our thread
        deferred->handle(error, {});
        request->deleteLater();
    });
    setBakingEnabledRequest->start();
}

#if (PR_BUILD || DEV_BUILD)
void AssetScriptingInterface::sendFakedHandshake() {
    auto nodeList = DependencyManager::get<NodeList>();
    SharedNodePointer assetServer = nodeList->soloNodeOfType(NodeType::AssetServer);

    nodeList->sendFakedHandshakeRequestToNode(assetServer);
}

#endif

void AssetScriptingInterface::getMapping(QString asset, ScriptValuePointer callback) {
    auto path = AssetUtils::getATPUrl(asset).path();
    auto handler = jsBindCallback(thisObject(), callback);
    JS_VERIFY(AssetUtils::isValidFilePath(path), "invalid ATP file path: " + asset + "(path:"+path+")");
    JS_VERIFY(callback->isFunction(), "expected second parameter to be a callback function");
    Promise promise = getAssetInfo(path);
    auto scriptEngine = engine();
    promise->ready([=](QString error, QVariantMap result) {
        jsCallback(handler, scriptEngine->newValue(error), scriptEngine->newValue(result.value("hash").toString()));
    });
}

bool AssetScriptingInterface::jsVerify(bool condition, const QString& error) {
    if (condition) {
        return true;
    }
    if (context()) {
        context()->throwError(error);
    } else {
        qCDebug(scriptengine) << "WARNING -- jsVerify failed outside of a valid JS context: " + error;
    }
    return false;
}

ScriptValuePointer AssetScriptingInterface::jsBindCallback(ScriptValuePointer scope, ScriptValuePointer callback) {
    ScriptValuePointer handler = ::makeScopedHandlerObject(scope, callback);
    ScriptValuePointer value = handler->property("callback");
    if (!jsVerify(handler->isObject() && value->isFunction(),
                 QString("jsBindCallback -- .callback is not a function (%1)").arg(value->toVariant().typeName()))) {
        return ScriptValuePointer();
    }
    return handler;
}

Promise AssetScriptingInterface::jsPromiseReady(Promise promise, ScriptValuePointer scope, ScriptValuePointer callback) {
    auto handler = jsBindCallback(scope, callback);
    if (!jsVerify(handler->isValid(), "jsPromiseReady -- invalid callback handler")) {
        return nullptr;
    }
    auto scriptEngine = engine();
    return promise->ready([this, handler, scriptEngine](QString error, QVariantMap result) {
        jsCallback(handler, scriptEngine->newValue(error), result);
    });
}

void AssetScriptingInterface::jsCallback(const ScriptValuePointer& handler,
                                         const ScriptValuePointer& error, const ScriptValuePointer& result) {
    Q_ASSERT(thread() == QThread::currentThread());
    auto errorValue = !error->toBool() ? engine()->nullValue() : error;
    JS_VERIFY(handler->isObject() && handler->property("callback")->isFunction(),
              QString("jsCallback -- .callback is not a function (%1)")
              .arg(handler->property("callback")->toVariant().typeName()));
    ::callScopedHandlerObject(handler, errorValue, result);
}

void AssetScriptingInterface::jsCallback(const ScriptValuePointer& handler,
                                         const ScriptValuePointer& error, const QVariantMap& result) {
    Q_ASSERT(thread() == QThread::currentThread());
    Q_ASSERT(handler->engine());
    auto engine = handler->engine();
    jsCallback(handler, error, engine->toScriptValue(result));
}

void AssetScriptingInterface::deleteAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    jsVerify(false, "TODO: deleteAsset API");
}

/*@jsdoc
 * Source and download options for {@link Assets.getAsset}.
 * @typedef {object} Assets.GetOptions
 * @property {boolean} [decompress=false] - <code>true</code> to gunzip decompress the downloaded data. Synonym:
 *     <code>compressed</code>.
 * @property {Assets.ResponseType} [responseType="text"] - The desired result type.
 * @property {string} url - The mapped path or hash to download. May have a leading <code>"atp:"</code>.
 */
/*@jsdoc
 * Result value returned by {@link Assets.getAsset}. 
 * @typedef {object} Assets.GetResult
 * @property {number} [byteLength] - The number of bytes in the downloaded content in <code>response</code>.
 * @property {boolean} cached - <code>true</code> if the item was retrieved from the cache, <code>false</code> if it was 
 *     downloaded.
 * @property {string} [contentType] - The automatically detected MIME type of the content.
 * @property {boolean} [decompressed] - <code>true</code> if the content was decompressed, <code>false</code> if it wasn't.
 * @property {string} [hash] - The hash for the downloaded asset.
 * @property {string} [hashURL] - The ATP URL of the hash file.
 * @property {string} [path] - The path for the asset, if a path was requested. Otherwise, <code>undefined</code>.
 * @property {string|object|ArrayBuffer} [response] - The downloaded content.
 * @property {Assets.ResponseType} [responseType] - The type of the downloaded content in <code>response</code>.
 * @property {string} [url] - The URL of the asset requested: the path with leading <code>"atp:"</code> if a path was 
 *     requested, otherwise the requested URL.
 * @property {boolean} [wasRedirected] - <code>true</code> if the downloaded data is the baked version of the asset, 
 *      <code>false</code> if it isn't baked.
 */
void AssetScriptingInterface::getAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    JS_VERIFY(options->isObject() || options->isString(), "expected request options Object or URL as first parameter");

    auto decompress = options->property("decompress")->toBool() || options->property("compressed")->toBool();
    auto responseType = options->property("responseType")->toString().toLower();
    auto url = options->property("url")->toString();
    if (options->isString()) {
        url = options->toString();
    }
    if (responseType.isEmpty()) {
        responseType = "text";
    }
    auto asset = AssetUtils::getATPUrl(url).path();
    JS_VERIFY(AssetUtils::isValidHash(asset) || AssetUtils::isValidFilePath(asset),
              QString("Invalid ATP url '%1'").arg(url));
    JS_VERIFY(RESPONSE_TYPES.contains(responseType),
              QString("Invalid responseType: '%1' (expected: %2)").arg(responseType).arg(RESPONSE_TYPES.join(" | ")));

    Promise fetched = jsPromiseReady(makePromise("fetched"), scope, callback);
    if (!fetched) {
        return;
    }

    Promise mapped = makePromise("mapped");
    mapped->fail(fetched);
    mapped->then([=](QVariantMap result) {
        QString hash = result.value("hash").toString();
        QString url = result.value("url").toString();
        if (!AssetUtils::isValidHash(hash)) {
            fetched->reject("internal hash error: " + hash, result);
        } else {
            Promise promise = loadAsset(hash, decompress, responseType);
            promise->mixin(result);
            promise->ready([=](QString error, QVariantMap loadResult) {
                loadResult["url"] = url; // maintain mapped .url in results (vs. atp:hash returned by loadAsset)
                fetched->handle(error, loadResult);
            });
        }
    });

    if (AssetUtils::isValidHash(asset)) {
        mapped->resolve({
            { "hash", asset },
            { "url", url },
        });
    } else {
        getAssetInfo(asset)->ready(mapped);
    }
}

/*@jsdoc
 * Source options for {@link Assets.resolveAsset}.
 * @typedef {object} Assets.ResolveOptions
 * @property {string} url - The hash or path to resolve. May have a leading <code>"atp:"</code>.
 */
/*@jsdoc
 * Result value returned by {@link Assets.resolveAsset}.
 * <p>Note: If resolving a hash, a file of that hash need not be present on the asset server for the hash to resolve.</p>
 * @typedef {object} Assets.ResolveResult
 * @property {string} [hash] - The hash of the asset.
 * @property {string} [hashURL] - The url of the asset's hash file, with leading <code>atp:</code>. 
 * @property {string} [path] - The path to the asset.
 * @property {string} [url] - The URL of the asset.
 * @property {boolean} [wasRedirected] - <code>true</code> if the resolved data is for the baked version of the asset,
 *      <code>false</code> if it isn't.
 */
void AssetScriptingInterface::resolveAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    const QString& URL{ "url" };

    auto url = (options->isString() ? options : options->property(URL))->toString();
    auto asset = AssetUtils::getATPUrl(url).path();

    JS_VERIFY(AssetUtils::isValidFilePath(asset) || AssetUtils::isValidHash(asset),
             "expected options to be an asset URL or request options containing .url property");

    jsPromiseReady(getAssetInfo(asset), scope, callback);
}

/*@jsdoc
 * Content and decompression options for {@link Assets.decompressData}.
 * @typedef {object} Assets.DecompressOptions
 * @property {ArrayBuffer} data - The data to decompress.
 * @property {Assets.ResponseType} [responseType=text] - The type of decompressed data to return.
 */
/*@jsdoc
 * Result value returned by {@link Assets.decompressData}.
 * @typedef {object} Assets.DecompressResult
 * @property {number} [byteLength] - The number of bytes in the decompressed data.
 * @property {string} [contentType] - The MIME type of the decompressed data.
 * @property {boolean} [decompressed] - <code>true</code> if the data is decompressed.
 * @property {string|object|ArrayBuffer} [response] - The decompressed data.
 * @property {Assets.ResponseType} [responseType] - The type of the decompressed data in <code>response</code>.
 */
void AssetScriptingInterface::decompressData(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    auto data = options->property("data");
    QByteArray dataByteArray = scriptvalue_cast<QByteArray>(data);
    auto responseType = options->property("responseType")->toString().toLower();
    if (responseType.isEmpty()) {
        responseType = "text";
    }
    Promise completed = jsPromiseReady(makePromise(__FUNCTION__), scope, callback);
    Promise decompressed = decompressBytes(dataByteArray);
    if (responseType == "arraybuffer") {
        decompressed->ready(completed);
    } else {
        decompressed->ready([=](QString error, QVariantMap result) {
            Promise converted = convertBytes(result.value("data").toByteArray(), responseType);
            converted->mixin(result);
            converted->ready(completed);
        });
    }
}

namespace {
    const int32_t DEFAULT_GZIP_COMPRESSION_LEVEL = -1;
    const int32_t MAX_GZIP_COMPRESSION_LEVEL = 9;
}

/*@jsdoc
 * Content and compression options for {@link Assets.compressData}.
 * @typedef {object} Assets.CompressOptions
 * @property {string|ArrayBuffer} data - The data to compress.
 * @property {number} level - The compression level, range <code>-1</code> &ndash; <code>9</code>. <code>-1</code> means 
 *     use the default gzip compression level, <code>0</code> means no compression, and <code>9</code> means maximum 
 *     compression.
 */
/*@jsdoc
 * Result value returned by {@link Assets.compressData}.
 * @typedef {object} Assets.CompressResult
 * @property {number} [byteLength] - The number of bytes in the compressed data.
 * @property {boolean} [compressed] - <code>true</code> if the data is compressed.
 * @property {string} [contentType] - The MIME type of the compressed data, i.e., <code>"application/gzip"</code>.
 * @property {ArrayBuffer} [data] - The compressed data.
 */
void AssetScriptingInterface::compressData(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    auto data = options->property("data")->isValid() ? options->property("data") : options;
    QByteArray dataByteArray = data->isString() ? data->toString().toUtf8() : scriptvalue_cast<QByteArray>(data);
    int level = options->property("level")->isNumber() ? options->property("level")->toInt32() : DEFAULT_GZIP_COMPRESSION_LEVEL;
    JS_VERIFY(level >= DEFAULT_GZIP_COMPRESSION_LEVEL || level <= MAX_GZIP_COMPRESSION_LEVEL, QString("invalid .level %1").arg(level));
    jsPromiseReady(compressBytes(dataByteArray, level), scope, callback);
}

/*@jsdoc
 * Content and upload options for {@link Assets.putAsset}.
 * @typedef {object} Assets.PutOptions
 * @property {boolean} [compress=false] - <code>true</code> to gzip compress the content for upload and storage,
 *     <code>false</code> to upload and store the data without gzip compression. Synonym: <code>compressed</code>.
 * @property {string|ArrayBuffer} data - The content to upload.
 * @property {string} [path] - A user-friendly path for the file in the asset server. May have a leading 
 *     <code>"atp:"</code>. If not specified, no path-to-hash mapping is set.
 *     <p>Note: The asset server destroys any unmapped SHA256-named file at server restart. Either set the mapping path 
 *     with this property or use {@link Assets.setMapping} to set a path-to-hash mapping for the uploaded file.</p>
 */
/*@jsdoc
 * Result value returned by {@link Assets.putAsset}.
 * @typedef {object} Assets.PutResult
 * @property {number} [byteLength] - The number of bytes in the hash file stored on the asset server.
 * @property {boolean} [compressed] - <code>true</code> if the content stored is gzip compressed.
 * @property {string} [contentType] - <code>"application/gzip"</code> if the content stored is gzip compressed.
 * @property {string} [hash] - The SHA256 hash of the content.
 * @property {string} [url] - The <code>atp:</code> URL of the content: using the path if specified, otherwise the hash.
 * @property {string} [path] - The uploaded content's mapped path, if specified.
 */
void AssetScriptingInterface::putAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    auto compress = options->property("compress")->toBool() || options->property("compressed")->toBool();
    auto data = options->isObject() ? options->property("data") : options;
    auto rawPath = options->property("path")->toString();
    auto path = AssetUtils::getATPUrl(rawPath).path();

    QByteArray dataByteArray = data->isString() ? data->toString().toUtf8() : scriptvalue_cast<QByteArray>(data);

    JS_VERIFY(path.isEmpty() || AssetUtils::isValidFilePath(path),
              QString("expected valid ATP file path '%1' ('%2')").arg(rawPath).arg(path));
    JS_VERIFY(dataByteArray.size() > 0,
              QString("expected non-zero .data (got %1 / #%2 bytes)").arg(data->toVariant().typeName()).arg(dataByteArray.size()));

    // [compressed] => uploaded to server => [mapped to path]
    Promise prepared = makePromise("putAsset::prepared");
    Promise uploaded = makePromise("putAsset::uploaded");
    Promise completed = makePromise("putAsset::completed");
    jsPromiseReady(completed, scope, callback);

    if (compress) {
        Promise compress = compressBytes(dataByteArray, DEFAULT_GZIP_COMPRESSION_LEVEL);
        compress->ready(prepared);
    } else {
        prepared->resolve({{ "data", dataByteArray }});
    }

    prepared->fail(completed);
    prepared->then([=](QVariantMap result) {
        Promise upload = uploadBytes(result.value("data").toByteArray());
        upload->mixin(result);
        upload->ready(uploaded);
    });

    uploaded->fail(completed);
    if (path.isEmpty()) {
        uploaded->then(completed);
    } else {
        uploaded->then([=](QVariantMap result) {
            QString hash = result.value("hash").toString();
            if (!AssetUtils::isValidHash(hash)) {
                completed->reject("path mapping requested, but did not receive valid hash", result);
            } else {
                Promise link = symlinkAsset(hash, path);
                link->mixin(result);
                link->ready(completed);
            }
        });
    }
}

/*@jsdoc
 * Source for {@link Assets.queryCacheMeta}.
 * @typedef {object} Assets.QueryCacheMetaOptions
 * @property {string} url - The URL of the cached asset to get information on. Must start with <code>"atp:"</code> or 
 *     <code>"cache:"</code>.
 */
void AssetScriptingInterface::queryCacheMeta(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    QString url = options->isString() ? options->toString() : options->property("url")->toString();
    JS_VERIFY(QUrl(url).isValid(), QString("Invalid URL '%1'").arg(url));
    jsPromiseReady(Parent::queryCacheMeta(url), scope, callback);
}

/*@jsdoc
 * Source and retrieval options for {@link Assets.loadFromCache}.
 * @typedef {object} Assets.LoadFromCacheOptions
 * @property {boolean} [decompress=false] - <code>true</code> to gunzip decompress the cached data. Synonym:
 *     <code>compressed</code>.
 * @property {Assets.ResponseType} [responseType=text] - The desired result type.
 * @property {string} url - The URL of the asset to load from cache. Must start with <code>"atp:"</code> or
 *     <code>"cache:"</code>.
 */
void AssetScriptingInterface::loadFromCache(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    QString url, responseType;
    bool decompress = false;
    if (options->isString()) {
        url = options->toString();
        responseType = "text";
    } else {
        url = options->property("url")->toString();
        responseType = options->property("responseType")->isValid() ? options->property("responseType")->toString() : "text";
        decompress = options->property("decompress")->toBool() || options->property("compressed")->toBool();
    }
    JS_VERIFY(QUrl(url).isValid(), QString("Invalid URL '%1'").arg(url));
    JS_VERIFY(RESPONSE_TYPES.contains(responseType),
              QString("Invalid responseType: '%1' (expected: %2)").arg(responseType).arg(RESPONSE_TYPES.join(" | ")));

    jsPromiseReady(Parent::loadFromCache(url, decompress, responseType), scope, callback);
}

bool AssetScriptingInterface::canWriteCacheValue(const QUrl& url) {
    auto scriptManager = engine()->manager();
    if (!scriptManager) {
        return false;
    }
    // allow cache writes only from Client, EntityServer and Agent scripts
    bool isAllowedContext = (
        scriptManager->isClientScript() ||
        scriptManager->isAgentScript()
    );
    if (!isAllowedContext) {
        return false;
    }
    return true;
}

/*@jsdoc
 * The data to save to the cache and cache options for {@link Assets.saveToCache}.
 * @typedef {object} Assets.SaveToCacheOptions
 * @property {string|ArrayBuffer} data - The data to save to the cache.
 * @property {Assets.SaveToCacheHeaders} [headers] - The last-modified and expiry times for the cache item.
 * @property {string} [url] - The URL to associate with the cache item. Must start with <code>"atp:"</code> or
 *     <code>"cache:"</code>. If not specified, the URL is <code>"atp:"</code> followed by the SHA256 hash of the content.
 */
void AssetScriptingInterface::saveToCache(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback) {
    JS_VERIFY(options->isObject(), QString("expected options object as first parameter not: %1").arg(options->toVariant().typeName()));

    QString url = options->property("url")->toString();
    QByteArray data = scriptvalue_cast<QByteArray>(options->property("data"));
    QVariantMap headers = scriptvalue_cast<QVariantMap>(options->property("headers"));

    saveToCache(url, data, headers, scope, callback);
}

void AssetScriptingInterface::saveToCache(const QUrl& rawURL, const QByteArray& data, const QVariantMap& metadata, ScriptValuePointer scope, ScriptValuePointer callback) {
    QUrl url = rawURL;
    if (url.path().isEmpty() && !data.isEmpty()) {
        // generate a valid ATP URL from the data  -- appending any existing fragment or querystring values
        auto atpURL = AssetUtils::getATPUrl(hashDataHex(data));
        atpURL.setQuery(url.query());
        atpURL.setFragment(url.fragment());
        url = atpURL;
    }
    auto hash = AssetUtils::extractAssetHash(url.toDisplayString());

    JS_VERIFY(url.isValid(), QString("Invalid URL '%1'").arg(url.toString()));
    JS_VERIFY(canWriteCacheValue(url), "Invalid cache write URL: " + url.toString());
    JS_VERIFY(url.scheme() == "atp" || url.scheme() == "cache", "only 'atp' and 'cache' URL schemes supported");
    JS_VERIFY(hash.isEmpty() || hash == hashDataHex(data), QString("invalid checksum hash for atp:HASH style URL (%1 != %2)").arg(hash, hashDataHex(data)));


    jsPromiseReady(Parent::saveToCache(url, data, metadata), scope, callback);
}
