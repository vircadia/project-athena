//
//  AssetScriptingInterface.h
//  libraries/script-engine/src
//
//  Created by Stephen Birarda on 2016-03-08.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_AssetScriptingInterface_h
#define hifi_AssetScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <AssetClient.h>
#include <NetworkAccessManager.h>
#include <BaseAssetScriptingInterface.h>
#include <QtNetwork/QNetworkDiskCache>
#include <QtCore/QSharedPointer>

#include "Scriptable.h"

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Assets</code> API provides facilities for interacting with the domain's asset server and the client cache. 
 * <p>Assets are stored in the asset server in files with SHA256 names. These files are mapped to user-friendly URLs of the 
 * format: <code>atp:/path/filename</code>. The assets may optionally be baked, in which case a request for the original 
 * unbaked version of the asset is automatically redirected to the baked version. The asset data may optionally be stored as 
 * compressed.</p>
 * <p>The client cache can be accessed directly, using <code>"atp:"</code> or <code>"cache:"</code> URLs. Interface, avatar, 
 * and assignment client scripts can write to the cache. All script types can read from the cache.</p>
 *
 * @namespace Assets
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 */
class AssetScriptingInterface : public BaseAssetScriptingInterface, Scriptable {
    Q_OBJECT
public:
    using Parent = BaseAssetScriptingInterface;
    AssetScriptingInterface(QObject* parent = nullptr);

    /*@jsdoc
     * Called when an {@link Assets.uploadData} call is complete.
     * @callback Assets~uploadDataCallback
     * @param {string} url - The raw URL of the file that the content is stored in, with <code>atp:</code> as the scheme and 
     *     the SHA256 hash as the filename (with no extension).
     * @param {string} hash - The SHA256 hash of the content.
     */
    /*@jsdoc
     * Uploads content to the asset server, storing it in a SHA256-named file.
     * <p>Note: The asset server destroys any unmapped SHA256-named file at server restart. Use {@link Assets.setMapping} to 
     * set a path-to-hash mapping for the new file.</p>
     * @function Assets.uploadData
     * @param {string} data - The content to upload.
     * @param {Assets~uploadDataCallback} callback - The function to call upon completion.
     * @example <caption>Store a string in the asset server.</caption>
     * Assets.uploadData("Hello world!", function (url, hash) {
     *     print("URL: " + url);  // atp:0a1b...9g
     *     Assets.setMapping("/assetsExamples/helloWorld.txt", hash, function (error) {
     *         if (error) {
     *             print("ERROR: Could not set mapping!");
     *             return;
     *         }
     *     });
     * });
     */
    Q_INVOKABLE void uploadData(QString data, ScriptValuePointer callback);

    /*@jsdoc
     * Called when an {@link Assets.downloadData} call is complete.
     * @callback Assets~downloadDataCallback
     * @param {string} data - The content that was downloaded.
     * @param {Assets.DownloadDataError} error - The success or failure of the download.
     */
    /*@jsdoc
     * Downloads content from the asset server, from a SHA256-named file.
     * @function Assets.downloadData
     * @param {string} url - The raw URL of asset to download: <code>atp:</code> followed by the assets's SHA256 hash.
     * @param {Assets~downloadDataCallback} callback - The function to call upon completion.
     * @example <caption>Store and retrieve a string from the asset server.</caption>
     * var assetURL;
     * 
     * // Store the string.
     * Assets.uploadData("Hello world!", function (url, hash) {
     *     assetURL = url;
     *     print("url: " + assetURL);  // atp:a0g89...
     *     Assets.setMapping("/assetsExamples/helloWorld.txt", hash, function (error) {
     *         if (error) {
     *             print("ERROR: Could not set mapping!");
     *             return;
     *         }
     *     });
     * });
     * 
     * // Retrieve the string.
     * Script.setTimeout(function () {
     *     Assets.downloadData(assetURL, function (data, error) {
     *         print("Downloaded data: " + data);
     *         print("Error: " + JSON.stringify(error));
     *     });
     * }, 1000);
     */
    Q_INVOKABLE void downloadData(QString url, ScriptValuePointer callback);

    /*@jsdoc
     * Called when an {@link Assets.setMapping} call is complete.
     * @callback Assets~setMappingCallback
     * @param {string} error - <code>null</code> if the path-to-hash mapping was set, otherwise a description of the error.
     */
    /*@jsdoc
     * Sets a path-to-hash mapping within the asset server.
     * @function Assets.setMapping
     * @param {string} path - A user-friendly path for the file in the asset server, without leading <code>"atp:"</code>.
     * @param {string} hash - The hash in the asset server.
     * @param {Assets~setMappingCallback} callback - The function to call upon completion.
     */
    Q_INVOKABLE void setMapping(QString path, QString hash, ScriptValuePointer callback);

    /*@jsdoc
     * Called when an {@link Assets.getMapping} call is complete.
     * @callback Assets~getMappingCallback
     * @param {string} error - <code>null</code> if the path was found, otherwise a description of the error.
     * @param {string} hash - The hash value if the path was found, <code>""</code> if it wasn't.
     */
    /*@jsdoc
     * Gets the hash for a path within the asset server. The hash is for the unbaked or baked version of the
     * asset, according to the asset server setting for the particular path.
     * @function Assets.getMapping
     * @param {string} path - The path to a file in the asset server to get the hash of.
     * @param {Assets~getMappingCallback} callback - The function to call upon completion.
     * @example <caption>Report the hash of an asset server item.</caption>
     * var assetPath = Window.browseAssets();
     * if (assetPath) {
     *     var mapping = Assets.getMapping(assetPath, function (error, hash) {
     *         print("Asset: " + assetPath);
     *         print("- hash:  " + hash);
     *         print("- error: " + error);
     *     });
     * }
     */
    Q_INVOKABLE void getMapping(QString path, ScriptValuePointer callback);

    /*@jsdoc
     * Called when an {@link Assets.setBakingEnabled} call is complete.
     * @callback Assets~setBakingEnabledCallback
     * @param {string} error - <code>null</code> if baking was successfully enabled or disabled, otherwise a description of the 
     * error.
     */
    /*@jsdoc
     * Sets whether or not to bake an asset in the asset server.
     * @function Assets.setBakingEnabled
     * @param {string} path - The path to a file in the asset server.
     * @param {boolean} enabled - <code>true</code> to enable baking of the asset, <code>false</code> to disable.
     * @param {Assets~setBakingEnabledCallback} callback - The function to call upon completion.
     */
    // Note: Second callback parameter not documented because it's always {}.
    Q_INVOKABLE void setBakingEnabled(QString path, bool enabled, ScriptValuePointer callback);

#if (PR_BUILD || DEV_BUILD)
    /**
     * This function is purely for development purposes, and not meant for use in a
     * production context. It is not a public-facing API, so it should not have JSDoc.
     */
    Q_INVOKABLE void sendFakedHandshake();
#endif

    /*@jsdoc
     * Details of a callback function.
     * @typedef {object} Assets.CallbackDetails
     * @property {object} scope - The scope that the <code>callback</code> function is defined in. This object is bound to 
     *     <code>this</code> when the function is called.
     * @property {Assets~compressDataCallback|Assets~decompressDataCallback|Assets~getAssetCallback
     *     |Assets~getCacheStatusCallback|Assets~loadFromCacheCallback|Assets~putAssetCallback|Assets~queryCacheMetaCallback
     *     |Assets~resolveAssetCallback|Assets~saveToCacheCallback} 
     *     callback -  The function to call upon completion. May be an inline function or a function identifier. If a function 
     *     identifier, it must be a member of <code>scope</code>.
     */

    /*@jsdoc
     * Called when an {@link Assets.getAsset} call is complete.
     * @callback Assets~getAssetCallback
     * @param {string} error - <code>null</code> if the content was downloaded, otherwise a description of the error.
     * @param {Assets.GetResult} result - Information on and the content downloaded.
     */
    /*@jsdoc
     * Downloads content from the asset server.
     * @function Assets.getAsset
     * @param {string|Assets.GetOptions} source - What to download and download options. If a string, the mapped path or hash 
     *     to download, optionally including a leading <code>"atp:"</code>.
     * @param {object|Assets.CallbackDetails|Assets~getAssetCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~getAssetCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Retrieve a string from the asset server.</caption>
     * Assets.getAsset(
     *     {
     *         url: "/assetsExamples/helloWorld.txt",
     *         responseType: "text"
     *     },
     *     function (error, result) {
     *         if (error) {
     *             print("ERROR: Data not downloaded");
     *         } else {
     *             print("Data: " + result.response);
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void getAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());

    /*@jsdoc
     * Called when an {@link Assets.putAsset} call is complete.
     * @callback Assets~putAssetCallback
     * @param {string} error - <code>null</code> if the content was uploaded and any path-to-hash mapping set, otherwise a 
     *     description of the error.
     * @param {Assets.PutResult} result - Information on the content uploaded.
     */
    /*@jsdoc
     * Uploads content to the asset server and sets a path-to-hash mapping.
     * @function Assets.putAsset
     * @param {string|Assets.PutOptions} options - The content to upload and upload options. If a string, the value of the
     *     string is uploaded but a path-to-hash mapping is not set.
     * @param {object|Assets.CallbackDetails|Assets~putAssetCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~putAssetCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Store a string in the asset server.</caption>
     * Assets.putAsset(
     *     {
     *         data: "Hello world!",
     *         path: "/assetsExamples/helloWorld.txt"
     *     },
     *     function (error, result) {
     *         if (error) {
     *             print("ERROR: Data not uploaded or mapping not set");
     *         } else {
     *             print("URL: " + result.url);  // atp:/assetsExamples/helloWorld.txt
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void putAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());

    /*@jsdoc
     * Called when an {@link Assets.deleteAsset} call is complete.
     * <p class="important">Not implemented: This type is not implemented yet.</p>
     * @callback Assets~deleteAssetCallback
     * @param {string} error - <code>null</code> if the content was deleted, otherwise a description of the error.
     * @param {Assets.DeleteResult} result - Information on the content deleted.
     */
    /*@jsdoc
     * Deletes content from the asset server.
     * <p class="important">Not implemented: This method is not implemented yet.</p>
     * @function Assets.deleteAsset
     * @param {Assets.DeleteOptions} options - The content to delete and delete options.
     * @param {object} scope - The scope that the <code>callback</code> function is defined in.
     * @param {Assets~deleteAssetCallback} callback - The function to call upon completion.
     */
    Q_INVOKABLE void deleteAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());

    /*@jsdoc
     * Called when an {@link Assets.resolveAsset} call is complete.
     * @callback Assets~resolveAssetCallback
     * @param {string} error - <code>null</code> if the asset hash or path was resolved, otherwise a description of the error. 
     * @param {Assets.ResolveResult} result - Information on the hash or path resolved.
     */
    /*@jsdoc
     * Resolves and returns information on a hash or a path in the asset server.
     * @function Assets.resolveAsset
     * @param {string|Assets.ResolveOptions} source - The hash or path to resolve if a string, otherwise an object specifying 
     *     what to resolve. If a string, it may have a leading <code>"atp:"</code>.
     * @param {object|Assets.CallbackDetails|Assets~resolveAssetCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~resolveAssetCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Get the hash and URL for a path.</caption>
     * Assets.resolveAsset(
     *     "/assetsExamples/helloWorld.txt",
     *     function (error, result) {
     *         if (error) {
     *             print("ERROR: " + error);
     *         } else {
     *             print("Hash: " + result.hash);
     *             print("URL: " + result.url);
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void resolveAsset(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Called when an {@link Assets.decompressData} call is complete.
     * @callback Assets~decompressDataCallback
     * @param {string} error - <code>null</code> if the data was successfully compressed, otherwise a description of the error.
     * @param {Assets.DecompressResult} result - Information on and the decompressed data.
     */
    /*@jsdoc
     * Decompresses data in memory using gunzip.
     * @function Assets.decompressData
     * @param {Assets.DecompressOptions} source - What to decompress and decompression options.
     * @param {object|Assets.CallbackDetails|Assets~decompressDataCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~decompressDataCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     */
    Q_INVOKABLE void decompressData(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Called when an {@link Assets.compressData} call is complete.
     * @callback Assets~compressDataCallback
     * @param {string} error - <code>null</code> if the data was successfully compressed, otherwise a description of the error.
     * @param {Assets.CompressResult} result - Information on and the compressed data.
     */
    /*@jsdoc
     * Compresses data in memory using gzip.
     * @function Assets.compressData
     * @param {string|ArrayBuffer|Assets.CompressOptions} source - What to compress and compression options. If a string or 
     *     ArrayBuffer, the data to compress.
     * @param {object|Assets.CallbackDetails|Assets~compressDataCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~compressDataCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     */
    Q_INVOKABLE void compressData(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Initializes the cache if it isn't already initialized.
     * @function Assets.initializeCache
     * @returns {boolean} <code>true</code> if the cache is initialized, <code>false</code> if it isn't.
     */
    Q_INVOKABLE bool initializeCache();
    
    /*@jsdoc
     * Checks whether the script can write to the cache.
     * @function Assets.canWriteCacheValue
     * @param {string} url - <em>Not used.</em>
     * @returns {boolean} <code>true</code> if the script is an Interface, avatar, or assignment client script, 
     *     <code>false</code> if the script is a client entity or server entity script.
     * @example <caption>Report whether the script can write to the cache.</caption>
     * print("Can write to cache: " + Assets.canWriteCacheValue(null));
     */
    Q_INVOKABLE bool canWriteCacheValue(const QUrl& url);
    
    /*@jsdoc
     * Called when a {@link Assets.getCacheStatus} call is complete.
     * @callback Assets~getCacheStatusCallback
     * @param {string} error - <code>null</code> if the cache status was retrieved without error, otherwise a description of 
     *     the error.
     * @param {Assets.GetCacheStatusResult} result - Details of the current cache status.
     */
    /*@jsdoc
     * Gets the current cache status.
     * @function Assets.getCacheStatus
     * @param {object|Assets.CallbackDetails|Assets~getCacheStatusCallback} scopeOrCallback - If an object, then the scope that 
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is 
     *     called. 
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~getCacheStatusCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function 
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by 
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Report the cache status.</caption>
     * Assets.getCacheStatus(function (error, status) {
     *     print("Cache status");
     *     print("- Error: " + error);
     *     print("- Status: " + JSON.stringify(status));
     * });
     */
    Q_INVOKABLE void getCacheStatus(ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer()) {
        jsPromiseReady(Parent::getCacheStatus(), scope, callback);
    }

    /*@jsdoc
     * Called when {@link Assets.queryCacheMeta} is complete.
     * @callback Assets~queryCacheMetaCallback
     * @param {string} error - <code>null</code> if the URL has a valid cache entry, otherwise a description of the error.
     * @param {Assets.CacheItemMetaData} result - Information on an asset in the cache.
     */
    /*@jsdoc
     * Gets information about the status of an asset in the cache.
     * @function Assets.queryCacheMeta
     * @param {string|Assets.QueryCacheMetaOptions} path - The URL of the cached asset to get information on if a string, 
     *     otherwise an object specifying the cached asset to get information on. The URL must start with <code>"atp:"</code> 
     *     or <code>"cache:"</code>.
     * @param {object|Assets.CallbackDetails|Assets~queryCacheMetaCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~queryCacheMetaCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Report details of a string store in the cache.</caption>
     * Assets.queryCacheMeta(
     *     "cache:/cacheExample/helloCache.txt",
     *     function (error, result) {
     *         if (error) {
     *             print("Error: " + error);
     *         } else {
     *             print("Success:");
     *             print("- URL: " + result.url);
     *             print("- isValid: " + result.isValid);
     *             print("- saveToDisk: " + result.saveToDisk);
     *             print("- expirationDate: " + result.expirationDate);
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void queryCacheMeta(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Called when an {@link Assets.loadFromCache} call is complete.
     * @callback Assets~loadFromCacheCallback
     * @param {string} error - <code>null</code> if the cache item was successfully retrieved, otherwise a description of the 
     *     error.
     * @param {Assets.LoadFromCacheResult} result - Information on and the retrieved data.
     */
    /*@jsdoc
     * Retrieves data from the cache directly, without downloading it. 
     * @function Assets.loadFromCache
     * @param {string|Assets.LoadFromCacheOptions} options - The URL of the asset to load from the cache if a string, otherwise 
     *     an object specifying the asset to load from the cache and load options. The URL must start with <code>"atp:"</code> 
     *     or <code>"cache:"</code>.
     * @param {object|Assets.CallbackDetails|Assets~loadFromCacheCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~loadFromCacheCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Retrieve a string from the cache.</caption>
     * Assets.loadFromCache(
     *     "cache:/cacheExample/helloCache.txt",
     *     function (error, result) {
     *         if (error) {
     *             print("Error: " + error);
     *         } else {
     *             print("Success:");
     *             print("- Response: " + result.response);
     *             print("- Content type: " + result.contentType);
     *             print("- Number of bytes: " + result.byteLength);
     *             print("- Bytes: " + [].slice.call(new Uint8Array(result.data), 0, result.byteLength));
     *             print("- URL: " + result.url);
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void loadFromCache(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Called when an {@link Assets.saveToCache} call is complete.
     * @callback Assets~saveToCacheCallback
     * @param {string} error - <code>null</code> if the asset data was successfully saved to the cache, otherwise a description 
     *     of the error.
     * @param {Assets.SaveToCacheResult} result - Information on the cached data.
     */
    /*@jsdoc
     * Saves asset data to the cache directly, without downloading it from a URL.
     * <p>Note: Can only be used in Interface, avatar, and assignment client scripts.</p>
     * @function Assets.saveToCache
     * @param {Assets.SaveToCacheOptions} options - The data to save to the cache and cache options.
     * @param {object|Assets.CallbackDetails|Assets~saveToCacheCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~saveToCacheCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     * @example <caption>Save a string in the cache.</caption>
     * Assets.saveToCache(
     *     {
     *         url: "cache:/cacheExample/helloCache.txt",
     *         data: "Hello cache"
     *     },
     *     function (error, result) {
     *         if (error) {
     *             print("Error: " + error);
     *         } else {
     *             print("Success:");
     *             print("- Bytes: " + result.byteLength);
     *             print("- URL: " + result.url);
     *         }
     *     }
     * );
     */
    Q_INVOKABLE void saveToCache(ScriptValuePointer options, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    
    /*@jsdoc
     * Saves asset data to the cache directly, without downloading it from a URL.
     * <p>Note: Can only be used in Interface, avatar, and assignment client scripts.</p>
     * @function Assets.saveToCache
     * @param {string} url - The URL to associate with the cache item. Must start with <code>"atp:"</code> or 
     *     <code>"cache:"</code>.
     * @param {string|ArrayBuffer} data - The data to save to the cache.
     * @param {Assets.SaveToCacheHeaders} headers - The last-modified and expiry times for the cache item.
     * @param {object|Assets.CallbackDetails|Assets~saveToCacheCallback} scopeOrCallback - If an object, then the scope that
     *     the <code>callback</code> function is defined in. This object is bound to <code>this</code> when the function is
     *     called.
     *     <p>Otherwise, the function to call upon completion. This may be an inline function or a function identifier.</p>
     * @param {Assets~saveToCacheCallback} [callback] - Used if <code>scopeOrCallback</code> specifies the scope.
     *     <p>The function to call upon completion. May be an inline function, a function identifier, or the name of a function
     *     in a string. If the name of a function or a function identifier, it must be a member of the scope specified by
     *     <code>scopeOrCallback</code>.</p>
     */
    Q_INVOKABLE void saveToCache(const QUrl& url, const QByteArray& data, const QVariantMap& metadata,
                                 ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
protected:
    ScriptValuePointer jsBindCallback(ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());
    Promise jsPromiseReady(Promise promise, ScriptValuePointer scope, ScriptValuePointer callback = ScriptValuePointer());

    void jsCallback(const ScriptValuePointer& handler, const ScriptValuePointer& error, const QVariantMap& result);
    void jsCallback(const ScriptValuePointer& handler, const ScriptValuePointer& error, const ScriptValuePointer& result);
    bool jsVerify(bool condition, const QString& error);
};

#endif // hifi_AssetScriptingInterface_h
