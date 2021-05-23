//
//  BaseAssetScriptingInterface.h
//  libraries/networking/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// BaseAssetScriptingInterface contains the engine-agnostic support code that can be used from
// both QML JS and ScriptEngine JS engine implementations

#ifndef hifi_BaseAssetScriptingInterface_h
#define hifi_BaseAssetScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "AssetClient.h"
#include <shared/MiniPromises.h>
#include "NetworkAccessManager.h"
#include <QtNetwork/QNetworkDiskCache>

class BaseAssetScriptingInterface : public QObject {
    Q_OBJECT
public:

    /*@jsdoc
     * <p>Types of response that {@link Assets.decompressData}, {@link Assets.getAsset}, or {@link Assets.loadFromCache} may 
     * provide.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>"arraybuffer"</code></td><td>A binary <code>ArrayBuffer</code> object.</td></tr>
     *     <tr><td><code>"json"</code></td><td>A parsed <code>JSON</code> object.</td></tr>
     *     <tr><td><code>"text"</code></td><td>UTF-8 decoded <code>string</code> value.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {string} Assets.ResponseType
     */
    const QStringList RESPONSE_TYPES{ "text", "arraybuffer", "json" };
    using Promise = MiniPromise::Promise;
    QSharedPointer<AssetClient> assetClient();

    BaseAssetScriptingInterface(QObject* parent = nullptr);

public slots:

    /*@jsdoc
     * Checks whether a string is a valid path. Note: A valid path must start with a <code>"/"</code>.
     * @function Assets.isValidPath
     * @param {string} path - The path to check.
     * @returns {boolean} <code>true</code> if the path is a valid path, <code>false</code> if it isn't.
     */
    bool isValidPath(QString input) { return AssetUtils::isValidPath(input); }

    /*@jsdoc
     * Checks whether a string is a valid path and filename. Note: A valid path and filename must start with a <code>"/"</code> 
     * but must not end with a <code>"/"</code>.
     * @function Assets.isValidFilePath
     * @param {string} path - The path to check.
     * @returns {boolean} <code>true</code> if the path is a valid file path, <code>false</code> if it isn't.
     */
    bool isValidFilePath(QString input) { return AssetUtils::isValidFilePath(input); }

    /*@jsdoc
     * Gets the normalized ATP URL for a path or hash: ensures that it has <code>"atp:"</code> at the start.
     * @function Assets.getATPUrl
     * @param {string} url - The URL to normalize.
     * @returns {string} The normalized ATP URL.
     */
    QUrl getATPUrl(QString input) { return AssetUtils::getATPUrl(input); }

    /*@jsdoc
     * Gets the SHA256 hexadecimal hash portion of an asset server URL.
     * @function Assets.extractAssetHash
     * @param {string} url - The URL to get the SHA256 hexadecimal hash from.
     * @returns {string} The SHA256 hexadecimal hash portion of the URL if present and valid, <code>""</code> otherwise.
     */
    QString extractAssetHash(QString input) { return AssetUtils::extractAssetHash(input); }

    /*@jsdoc
     * Checks whether a string is a valid SHA256 hexadecimal hash, i.e., 64 hexadecimal characters.
     * @function Assets.isValidHash
     * @param {string} hash - The hash to check.
     * @returns {boolean} <code>true</code> if the hash is a valid SHA256 hexadecimal string, <code>false</code> if it isn't.
     */
    bool isValidHash(QString input) { return AssetUtils::isValidHash(input); }

    /*@jsdoc
     * Calculates the SHA256 hash of given data.
     * @function Assets.hashData
     * @param {string|ArrayBuffer} data - The data to calculate the hash of.
     * @returns {ArrayBuffer} The SHA256 hash of the <code>data</code>.
     */
    QByteArray hashData(const QByteArray& data) { return AssetUtils::hashData(data); }

    /*@jsdoc
     * Calculates the SHA256 hash of given data, in hexadecimal format.
     * @function Assets.hashDataHex
     * @param {string|ArrayBuffer} data - The data to calculate the hash of.
     * @returns {string} The SHA256 hash of the <code>data</code>, in hexadecimal format.
     * @example <caption>Calculate the hash of some text.</caption>
     * var text = "Hello world!";
     * print("Hash: " + Assets.hashDataHex(text));
     */
    QString hashDataHex(const QByteArray& data) { return hashData(data).toHex(); }

protected:
    bool _cacheReady{ false };
    bool initializeCache();
    Promise getCacheStatus();
    Promise queryCacheMeta(const QUrl& url);
    Promise loadFromCache(const QUrl& url, bool decompress = false, const QString& responseType = "arraybuffer");
    Promise saveToCache(const QUrl& url, const QByteArray& data, const QVariantMap& metadata = QVariantMap());

    Promise loadAsset(QString asset, bool decompress, QString responseType);
    Promise getAssetInfo(QString asset);
    Promise downloadBytes(QString hash);
    Promise uploadBytes(const QByteArray& bytes);
    Promise compressBytes(const QByteArray& bytes, int level = -1);
    Promise convertBytes(const QByteArray& dataByteArray, const QString& responseType);
    Promise decompressBytes(const QByteArray& bytes);
    Promise symlinkAsset(QString hash, QString path);
};
#endif // hifi_BaseAssetScriptingInterface_h
