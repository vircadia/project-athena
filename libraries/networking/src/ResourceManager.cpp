//
//  ResourceManager.cpp
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ResourceManager.h"

#include <mutex>

#include <QFileInfo>

#include <SharedUtil.h>
#include <ThreadHelpers.h>

#include "AssetResourceRequest.h"
#include "FileResourceRequest.h"
#include "HTTPResourceRequest.h"
#include "NetworkAccessManager.h"
#include "NetworkLogging.h"
#include "NetworkingConstants.h"

ResourceManager::ResourceManager(bool atpSupportEnabled) : _atpSupportEnabled(atpSupportEnabled) {
    QString name = "Resource Manager Thread";
    _thread.setObjectName(name);

    if (_atpSupportEnabled) {
        auto assetClient = DependencyManager::set<AssetClient>();
        assetClient->moveToThread(&_thread);
        QObject::connect(&_thread, &QThread::started, assetClient.data(), [assetClient, name] {
            setThreadName(name.toStdString());
        });
    }

    _thread.start();
}

ResourceManager::~ResourceManager() {
    if (_thread.isRunning()) {
        _thread.quit();
        static const auto MAX_RESOURCE_MANAGER_THREAD_QUITTING_TIME = MSECS_PER_SECOND / 2;
        if (!_thread.wait(MAX_RESOURCE_MANAGER_THREAD_QUITTING_TIME)) {
            _thread.terminate();
        }
    }
}

void ResourceManager::setUrlPrefixOverride(const QString& prefix, const QString& replacement) {
    QMutexLocker locker(&_prefixMapLock);
    if (replacement.isEmpty()) {
        _prefixMap.erase(prefix);
    } else {
        _prefixMap[prefix] = replacement;
    }
}

QString ResourceManager::normalizeURL(const QString& urlString) {
    QString result = urlString;
    PrefixMap copy;

    {
        QMutexLocker locker(&_prefixMapLock);
        copy = _prefixMap;
    }

    foreach (const auto& entry, copy) {
        const auto& prefix = entry.first;
        const auto& replacement = entry.second;
        if (result.startsWith(prefix)) {
            qCDebug(networking) << "Replacing " << prefix << " with " << replacement;
            result.replace(0, prefix.size(), replacement);
        }
    }
    return result;
}

const QSet<QString>& getKnownUrls() {
    static QSet<QString> knownUrls;
    static std::once_flag once;
    std::call_once(once, [] {
        knownUrls.insert(URL_SCHEME_QRC);
        knownUrls.insert(HIFI_URL_SCHEME_FILE);
        knownUrls.insert(HIFI_URL_SCHEME_HTTP);
        knownUrls.insert(HIFI_URL_SCHEME_HTTPS);
        knownUrls.insert(HIFI_URL_SCHEME_FTP);
        knownUrls.insert(URL_SCHEME_ATP);
    });
    return knownUrls;
}

QUrl ResourceManager::normalizeURL(const QUrl& originalUrl) {
    QUrl url = QUrl(normalizeURL(originalUrl.toString()));
    auto scheme = url.scheme();
    if (!getKnownUrls().contains(scheme)) {
        // check the degenerative file case: on windows we can often have urls of the form c:/filename
        // this checks for and works around that case.
        QUrl urlWithFileScheme{ HIFI_URL_SCHEME_FILE + ":///" + url.toString() };
        if (!urlWithFileScheme.toLocalFile().isEmpty()) {
            return urlWithFileScheme;
        }
    }
    return url;
}

void ResourceManager::cleanup() {
    // cleanup the AssetClient thread
    DependencyManager::destroy<AssetClient>();
    _thread.quit();
    _thread.wait();
}

ResourceRequest* ResourceManager::createResourceRequest(
    QObject* parent,
    const QUrl& url,
    const bool isObservable,
    const qint64 callerId,
    const QString& extra
) {
    auto normalizedURL = normalizeURL(url);
    auto scheme = normalizedURL.scheme();

    ResourceRequest* request = nullptr;

    if (scheme == HIFI_URL_SCHEME_FILE || scheme == URL_SCHEME_QRC) {
        request = new FileResourceRequest(normalizedURL, isObservable, callerId, extra);
    } else if (scheme == HIFI_URL_SCHEME_HTTP || scheme == HIFI_URL_SCHEME_HTTPS || scheme == HIFI_URL_SCHEME_FTP) {
        request = new HTTPResourceRequest(normalizedURL, isObservable, callerId, extra);
    } else if (scheme == URL_SCHEME_ATP) {
        if (!_atpSupportEnabled) {
            qCDebug(networking) << "ATP support not enabled, unable to create request for URL: " << url.url();
            return nullptr;
        }
        request = new AssetResourceRequest(normalizedURL, isObservable, callerId, extra);
    } else {
        qCDebug(networking) << "Unknown scheme (" << scheme << ") for URL: " << url.url();
        return nullptr;
    }
    Q_ASSERT(request);

    if (parent) {
        QObject::connect(parent, &QObject::destroyed, request, &QObject::deleteLater);
    }
    request->moveToThread(&_thread);
    return request;
}

bool ResourceManager::resourceExists(const QUrl& url) {
    auto scheme = url.scheme();
    if (scheme == HIFI_URL_SCHEME_FILE) {
        QFileInfo file{ url.toString() };
        return file.exists();
    } else if (scheme == HIFI_URL_SCHEME_HTTP || scheme == HIFI_URL_SCHEME_HTTPS || scheme == HIFI_URL_SCHEME_FTP) {
        auto& networkAccessManager = NetworkAccessManager::getInstance();
        QNetworkRequest request{ url };

        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        request.setHeader(QNetworkRequest::UserAgentHeader, NetworkingConstants::VIRCADIA_USER_AGENT);

        auto reply = networkAccessManager.head(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        reply->deleteLater();

        return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
    } else if (scheme == URL_SCHEME_ATP && _atpSupportEnabled) {
        auto request = new AssetResourceRequest(url, ResourceRequest::IS_NOT_OBSERVABLE);
        ByteRange range;
        range.fromInclusive = 1;
        range.toExclusive = 1;
        request->setByteRange(range);
        request->setCacheEnabled(false);

        QEventLoop loop;

        QObject::connect(request, &AssetResourceRequest::finished, &loop, &QEventLoop::quit);

        request->send();

        loop.exec();

        request->deleteLater();

        return request->getResult() == ResourceRequest::Success;
    }

    qCDebug(networking) << "Unknown scheme (" << scheme << ") for URL: " << url.url();
    return false;
}
