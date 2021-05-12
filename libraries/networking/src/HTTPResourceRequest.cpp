//
//  HTTPResourceRequest.cpp
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "HTTPResourceRequest.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMetaEnum>

#include <SharedUtil.h>
#include <StatTracker.h>

#include "NetworkAccessManager.h"
#include "NetworkLogging.h"
#include "NetworkingConstants.h"

HTTPResourceRequest::~HTTPResourceRequest() {
    if (_reply) {
        _reply->disconnect(this);
        _reply->deleteLater();
        _reply = nullptr;
    }
}

void HTTPResourceRequest::setupTimer() {
    Q_ASSERT(!_sendTimer);
    static const int TIMEOUT_MS = 10000;

    _sendTimer = new QTimer();
    connect(this, &QObject::destroyed, _sendTimer, &QTimer::deleteLater);
    connect(_sendTimer, &QTimer::timeout, this, &HTTPResourceRequest::onTimeout);

    _sendTimer->setSingleShot(true);
    _sendTimer->start(TIMEOUT_MS);
}

void HTTPResourceRequest::cleanupTimer() {
    Q_ASSERT(_sendTimer);
    _sendTimer->disconnect(this);
    _sendTimer->deleteLater();
    _sendTimer = nullptr;
}

void HTTPResourceRequest::doSend() {
    DependencyManager::get<StatTracker>()->incrementStat(STAT_HTTP_REQUEST_STARTED);

    QNetworkRequest networkRequest(_url);
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader, NetworkingConstants::VIRCADIA_USER_AGENT);

    if (_cacheEnabled) {
        networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
    } else {
        networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    }

    if (_byteRange.isSet()) {
        QString byteRange;
        if (_byteRange.fromInclusive < 0) {
            byteRange = QString("bytes=%1").arg(_byteRange.fromInclusive);
        } else {
            // HTTP byte ranges are inclusive on the `to` end: [from, to]
            byteRange = QString("bytes=%1-%2").arg(_byteRange.fromInclusive).arg(_byteRange.toExclusive - 1);
        }
        networkRequest.setRawHeader("Range", byteRange.toLatin1());
    }
    networkRequest.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, false);

    _reply = NetworkAccessManager::getInstance().get(networkRequest);
    
    connect(_reply, &QNetworkReply::finished, this, &HTTPResourceRequest::onRequestFinished);
    connect(_reply, &QNetworkReply::downloadProgress, this, &HTTPResourceRequest::onDownloadProgress);

    setupTimer();
}

void HTTPResourceRequest::onRequestFinished() {
    Q_ASSERT(_state == InProgress);
    Q_ASSERT(_reply);

    cleanupTimer();

    // Content-Range headers have the form: 
    //
    //   Content-Range: <unit> <range-start>-<range-end>/<size>
    //   Content-Range: <unit> <range-start>-<range-end>/*
    //   Content-Range: <unit> */<size>
    //
    static auto parseContentRangeHeader = [](QString contentRangeHeader) -> std::pair<bool, uint64_t> {
        auto unitRangeParts = contentRangeHeader.split(' ');
        if (unitRangeParts.size() != 2) {
            return { false, 0 };
        }

        auto rangeSizeParts = unitRangeParts[1].split('/');
        if (rangeSizeParts.size() != 2) {
            return { false, 0 };
        }

        auto sizeStr = rangeSizeParts[1];
        if (sizeStr == "*") {
            return { true, 0 };
        } else {
            bool ok;
            auto size = sizeStr.toLong(&ok);
            return { ok, size };
        }
    };

    static auto parseMediaType = [](QString contentTypeHeader) -> std::pair<bool, QString> {
        auto contentTypeParts = contentTypeHeader.split(';');
        if (contentTypeParts.size() < 1) {
            return { false, "" };
        }

        return { true, contentTypeParts[0] };
    };

    switch(_reply->error()) {
        case QNetworkReply::NoError:
            _data = _reply->readAll();
            _loadedFromCache = _reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool();
            _result = Success;

            if (_byteRange.isSet()) {
                auto statusCode = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (statusCode == 206) {
                    _rangeRequestSuccessful = true;
                    auto contentRangeHeader = _reply->rawHeader("Content-Range");
                    bool success;
                    uint64_t size;
                    std::tie(success, size) = parseContentRangeHeader(contentRangeHeader);
                    if (success) {
                        _totalSizeOfResource = size;
                    } else {
                        qWarning(networking) << "Error parsing content-range header: " << contentRangeHeader;
                        _totalSizeOfResource = 0;
                    }
                } else {
                    _rangeRequestSuccessful = false;
                    _totalSizeOfResource = _data.size();
                }
            }

            {
                auto contentTypeHeader = _reply->rawHeader("Content-Type");
                bool success;
                QString mediaType;
                std::tie(success, mediaType) = parseMediaType(contentTypeHeader);
                if (success) {
                    _webMediaType = mediaType;
                }
            }

            recordBytesDownloadedInStats(STAT_HTTP_RESOURCE_TOTAL_BYTES, _data.size());

            break;

        case QNetworkReply::TimeoutError:
            _result = Timeout;
            break;

        case QNetworkReply::ContentNotFoundError: // Script.include('https://httpbin.org/status/404')
            _result = NotFound;
            break;

        case QNetworkReply::ProtocolInvalidOperationError: // Script.include('https://httpbin.org/status/400')
            _result = InvalidURL;
            break;

        case QNetworkReply::UnknownContentError: // Script.include('QUrl("https://httpbin.org/status/402")')
        case QNetworkReply::ContentOperationNotPermittedError: //Script.include('https://httpbin.org/status/403')
        case QNetworkReply::AuthenticationRequiredError: // Script.include('https://httpbin.org/basic-auth/user/passwd')
            _result = AccessDenied;
            break;

        case QNetworkReply::RemoteHostClosedError:  // Script.include('http://127.0.0.1:22')
        case QNetworkReply::ConnectionRefusedError: // Script.include(http://127.0.0.1:1')
        case QNetworkReply::HostNotFoundError:      // Script.include('http://foo.bar.highfidelity.io')
        case QNetworkReply::ServiceUnavailableError: // Script.include('QUrl("https://httpbin.org/status/503")')
            _result = ServerUnavailable;
            break;

        case QNetworkReply::UnknownServerError: // Script.include('QUrl("https://httpbin.org/status/504")')
        case QNetworkReply::InternalServerError: // Script.include('QUrl("https://httpbin.org/status/500")')
        default:
            qCDebug(networking) << "HTTPResourceRequest error:" << QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(_reply->error());
            _result = Error;
            break;
    }
    _reply->disconnect(this);
    _reply->deleteLater();
    _reply = nullptr;
    
    _state = Finished;
    emit finished();

    auto statTracker = DependencyManager::get<StatTracker>();
    if (_result == Success) {
        statTracker->incrementStat(STAT_HTTP_REQUEST_SUCCESS);

        if (loadedFromCache()) {
            statTracker->incrementStat(STAT_HTTP_REQUEST_CACHE);
        }
    } else {
        statTracker->incrementStat(STAT_HTTP_REQUEST_FAILED);
    }
}

void HTTPResourceRequest::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    Q_ASSERT(_state == InProgress);
    
    // We've received data, so reset the timer
    _sendTimer->start();

    emit progress(bytesReceived, bytesTotal);

    recordBytesDownloadedInStats(STAT_HTTP_RESOURCE_TOTAL_BYTES, bytesReceived);
}

void HTTPResourceRequest::onTimeout() {
    qDebug() << "Timeout: " << _reply->isFinished();
    Q_ASSERT(_state == InProgress);
    _reply->disconnect(this);
    _reply->abort();
    _reply->deleteLater();
    _reply = nullptr;

    cleanupTimer();
    
    _result = Timeout;
    _state = Finished;
    emit finished();

    DependencyManager::get<StatTracker>()->incrementStat(STAT_HTTP_REQUEST_FAILED);
}
