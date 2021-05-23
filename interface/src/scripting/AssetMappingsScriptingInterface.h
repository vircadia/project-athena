//
//  AssetMappingsScriptingInterface.h
//  libraries/script-engine/src
//
//  Created by Ryan Huffman on 2016-03-09.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_AssetMappingsScriptingInterface_h
#define hifi_AssetMappingsScriptingInterface_h

#include <QtCore/QObject>

#include <AssetClient.h>
#include <QSortFilterProxyModel>

#include "DependencyManager.h"

class AssetMappingModel : public QStandardItemModel {
    Q_OBJECT
    Q_PROPERTY(bool autoRefreshEnabled READ isAutoRefreshEnabled WRITE setAutoRefreshEnabled)
    Q_PROPERTY(int numPendingBakes READ getNumPendingBakes NOTIFY numPendingBakesChanged)

public:
    AssetMappingModel();

    Q_INVOKABLE void refresh();

    bool isAutoRefreshEnabled();
    void setAutoRefreshEnabled(bool enabled);

    bool isKnownMapping(QString path) const { return _pathToItemMap.contains(path); }
    bool isKnownFolder(QString path) const;

    int getNumPendingBakes() const { return _numPendingBakes;  }

public slots:
    void clear();

signals:
    void numPendingBakesChanged(int newCount);
    void errorGettingMappings(QString errorString);
    void updated();

private:
    void setupRoles();

    QHash<QString, QStandardItem*> _pathToItemMap;
    QTimer _autoRefreshTimer;
    int _numPendingBakes{ 0 };
};

Q_DECLARE_METATYPE(AssetMappingModel*)

class AssetMappingsScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

    Q_PROPERTY(AssetMappingModel* mappingModel READ getAssetMappingModel CONSTANT)
    Q_PROPERTY(QAbstractProxyModel* proxyModel READ getProxyModel CONSTANT)
public:
    AssetMappingsScriptingInterface();

    Q_INVOKABLE AssetMappingModel* getAssetMappingModel() { return &_assetMappingModel; }
    Q_INVOKABLE QAbstractProxyModel* getProxyModel() { return &_proxyModel; }

    Q_INVOKABLE bool isKnownMapping(QString path) const { return _assetMappingModel.isKnownMapping(path); }
    Q_INVOKABLE bool isKnownFolder(QString path) const { return _assetMappingModel.isKnownFolder(path); }

    Q_INVOKABLE void setMapping(QString path, QString hash, QJSValue callback = QJSValue());
    Q_INVOKABLE void getMapping(QString path, QJSValue callback = QJSValue());
    Q_INVOKABLE void uploadFile(QString path, QString mapping, QJSValue startedCallback = QJSValue(), QJSValue completedCallback = QJSValue(), bool dropEvent = false);
    Q_INVOKABLE void deleteMappings(QStringList paths, QJSValue callback = QJSValue());
    Q_INVOKABLE void deleteMapping(QString path, QJSValue callback) { deleteMappings(QStringList(path), callback = QJSValue()); }
    Q_INVOKABLE void getAllMappings(QJSValue callback = QJSValue());
    Q_INVOKABLE void renameMapping(QString oldPath, QString newPath, QJSValue callback = QJSValue());
    Q_INVOKABLE void setBakingEnabled(QStringList paths, bool enabled, QJSValue callback = QJSValue());
    Q_INVOKABLE void sortProxyModel(int column, Qt::SortOrder order = Qt::AscendingOrder);

protected:
    QSet<AssetRequest*> _pendingRequests;
    AssetMappingModel _assetMappingModel;
    QSortFilterProxyModel _proxyModel;
};


#endif // hifi_AssetMappingsScriptingInterface_h
