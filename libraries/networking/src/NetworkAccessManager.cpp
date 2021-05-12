//
//  NetworkAccessManager.cpp
//  libraries/networking/src
//
//  Created by Clement on 7/1/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "NetworkAccessManager.h"

#include "AtpReply.h"
#include "ResourceCache.h"

#include <QStandardPaths>
#include <QThreadStorage>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkDiskCache>

#include <shared/GlobalAppProperties.h>

QThreadStorage<QNetworkAccessManager*> networkAccessManagers;

QNetworkAccessManager& NetworkAccessManager::getInstance() {
    if (!networkAccessManagers.hasLocalData()) {
        auto networkAccessManager = new QNetworkAccessManager();
        // Setup disk cache if not already
        if (!networkAccessManager->cache()) {
            QString cacheDir = qApp->property(hifi::properties::APP_LOCAL_DATA_PATH).toString();
            if (cacheDir.isEmpty()) {
#ifdef Q_OS_ANDROID
                QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
                QString cachePath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#endif
                cacheDir = !cachePath.isEmpty() ? cachePath : "interfaceCache";
            }
            QNetworkDiskCache* cache = new QNetworkDiskCache();
            cache->setMaximumCacheSize(MAXIMUM_CACHE_SIZE);
            cache->setCacheDirectory(cacheDir);
            networkAccessManager->setCache(cache);
            qInfo() << "NetworkAccessManager disk cache set up at" << cacheDir
                     << "(size:" << MAXIMUM_CACHE_SIZE / BYTES_PER_GIGABYTES << "GB)";
        }
        networkAccessManagers.setLocalData(networkAccessManager);
    }

    return *networkAccessManagers.localData();
}

QNetworkReply* NetworkAccessManager::createRequest(Operation operation, const QNetworkRequest& request, QIODevice* device) {
    if (request.url().scheme() == "atp" && operation == GetOperation) {
        return new AtpReply(request.url());
        //auto url = request.url().toString();
        //return QNetworkAccessManager::createRequest(operation, request, device);
    } else {
        return QNetworkAccessManager::createRequest(operation, request, device);
    }
}
