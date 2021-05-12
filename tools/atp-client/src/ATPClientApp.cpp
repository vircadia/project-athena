//
//  ATPClientApp.cpp
//  tools/atp-client/src
//
//  Created by Seth Alves on 2017-3-15
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ATPClientApp.h"

#include <QDataStream>
#include <QTextStream>
#include <QThread>
#include <QFile>
#include <QLoggingCategory>
#include <QCommandLineParser>

#include <NetworkLogging.h>
#include <NetworkingConstants.h>
#include <MetaverseAPI.h>
#include <SharedLogging.h>
#include <AddressManager.h>
#include <DependencyManager.h>
#include <SettingHandle.h>
#include <AssetUpload.h>
#include <StatTracker.h>

#define HIGH_FIDELITY_ATP_CLIENT_USER_AGENT "Mozilla/5.0 (HighFidelityATPClient)"
#define TIMEOUT_MILLISECONDS 8000

ATPClientApp::ATPClientApp(int argc, char* argv[]) :
    QCoreApplication(argc, argv)
{
    // parse command-line
    QCommandLineParser parser;
    parser.setApplicationDescription("High Fidelity ATP-Client");

    const QCommandLineOption helpOption = parser.addHelpOption();

    const QCommandLineOption verboseOutput("v", "verbose output");
    parser.addOption(verboseOutput);

    const QCommandLineOption uploadOption("T", "upload local file", "local-file-to-send");
    parser.addOption(uploadOption);

    const QCommandLineOption authOption("u", "set usename and pass", "username:password");
    parser.addOption(authOption);

    const QCommandLineOption outputFilenameOption("o", "output filename", "output-file-name");
    parser.addOption(outputFilenameOption);

    const QCommandLineOption domainAddressOption("d", "domain-server address", "127.0.0.1");
    parser.addOption(domainAddressOption);

    const QCommandLineOption listenPortOption("listenPort", "listen port", QString::number(INVALID_PORT));
    parser.addOption(listenPortOption);

    if (!parser.parse(QCoreApplication::arguments())) {
        qCritical() << parser.errorText() << endl;
        parser.showHelp();
        Q_UNREACHABLE();
    }

    if (parser.isSet(helpOption)) {
        parser.showHelp();
        Q_UNREACHABLE();
    }

    _verbose = parser.isSet(verboseOutput);
    if (!_verbose) {
        QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

        const_cast<QLoggingCategory*>(&networking())->setEnabled(QtDebugMsg, false);
        const_cast<QLoggingCategory*>(&networking())->setEnabled(QtInfoMsg, false);
        const_cast<QLoggingCategory*>(&networking())->setEnabled(QtWarningMsg, false);

        const_cast<QLoggingCategory*>(&shared())->setEnabled(QtDebugMsg, false);
        const_cast<QLoggingCategory*>(&shared())->setEnabled(QtInfoMsg, false);
        const_cast<QLoggingCategory*>(&shared())->setEnabled(QtWarningMsg, false);
    }

    QStringList posArgs = parser.positionalArguments();
    if (posArgs.size() != 1) {
        qDebug() << "give remote url argument";
        parser.showHelp();
        Q_UNREACHABLE();
    }

    _url = QUrl(posArgs[0]);
    if (_url.scheme() != "atp") {
        qDebug() << "url should start with atp:";
        parser.showHelp();
        Q_UNREACHABLE();
    }

    int domainPort = 40103;
    if (_url.port() != -1) {
        domainPort = _url.port();
    }

    if (parser.isSet(outputFilenameOption)) {
        _localOutputFile = parser.value(outputFilenameOption);
    }

    if (parser.isSet(uploadOption)) {
        _localUploadFile = parser.value(uploadOption);
    }

    if (parser.isSet(authOption)) {
        QStringList pieces = parser.value(authOption).split(":");
        if (pieces.size() != 2) {
            qDebug() << "-u should be followed by username:password";
            parser.showHelp();
            Q_UNREACHABLE();
        }

        _username = pieces[0];
        _password = pieces[1];
        _waitingForLogin = true;
    }

    if (parser.isSet(listenPortOption)) {
        _listenPort = parser.value(listenPortOption).toInt();
    }

    _domainServerAddress = QString("127.0.0.1") + ":" + QString::number(domainPort);
    if (parser.isSet(domainAddressOption)) {
        _domainServerAddress = parser.value(domainAddressOption);
    } else if (!_url.host().isEmpty()) {
        QUrl domainURL;
        domainURL.setScheme("hifi");
        domainURL.setHost(_url.host());
        _domainServerAddress = domainURL.toString();
    }

    DependencyManager::registerInheritance<LimitedNodeList, NodeList>();

    DependencyManager::set<StatTracker>();
    DependencyManager::set<AccountManager>(false, [&]{ return QString(HIGH_FIDELITY_ATP_CLIENT_USER_AGENT); });
    DependencyManager::set<AddressManager>();
    DependencyManager::set<NodeList>(NodeType::Agent, _listenPort);

    auto accountManager = DependencyManager::get<AccountManager>();
    accountManager->setIsAgent(true);
    accountManager->setAuthURL(MetaverseAPI::getCurrentMetaverseServerURL());

    auto nodeList = DependencyManager::get<NodeList>();

    // setup a timer for domain-server check ins
    _domainCheckInTimer = new QTimer(nodeList.data());
    connect(_domainCheckInTimer, &QTimer::timeout, nodeList.data(), &NodeList::sendDomainServerCheckIn);
    _domainCheckInTimer->start(DOMAIN_SERVER_CHECK_IN_MSECS);

    // start the nodeThread so its event loop is running
    // (must happen after the checkin timer is created with the nodelist as it's parent)
    nodeList->startThread();

    const DomainHandler& domainHandler = nodeList->getDomainHandler();
    connect(&domainHandler, SIGNAL(domainURLChanged(QUrl)), SLOT(domainChanged(QUrl)));
    connect(&domainHandler, &DomainHandler::domainConnectionRefused, this, &ATPClientApp::domainConnectionRefused);

    connect(nodeList.data(), &NodeList::nodeAdded, this, &ATPClientApp::nodeAdded);
    connect(nodeList.data(), &NodeList::nodeKilled, this, &ATPClientApp::nodeKilled);
    connect(nodeList.data(), &NodeList::nodeActivated, this, &ATPClientApp::nodeActivated);
    connect(nodeList.data(), &NodeList::packetVersionMismatch, this, &ATPClientApp::notifyPacketVersionMismatch);
    nodeList->addSetOfNodeTypesToNodeInterestSet(NodeSet() << NodeType::AudioMixer << NodeType::AvatarMixer
                                                 << NodeType::EntityServer << NodeType::AssetServer << NodeType::MessagesMixer);

    if (_verbose) {
        QString username = accountManager->getAccountInfo().getUsername();
        qDebug() << "cached username is" << username << ", isLoggedIn =" << accountManager->isLoggedIn();
    }

    if (!_username.isEmpty()) {

        connect(accountManager.data(), &AccountManager::newKeypair, this, [&](){
            if (_verbose) {
                qDebug() << "new keypair has been created.";
            }
        });

        connect(accountManager.data(), &AccountManager::loginComplete, this, [&](){
            if (_verbose) {
                qDebug() << "login successful";
            }
            _waitingForLogin = false;
            go();
        });
        connect(accountManager.data(), &AccountManager::loginFailed, this, [&](){
            qDebug() << "login failed.";
            _waitingForLogin = false;
            go();
        });
        accountManager->requestAccessToken(_username, _password);
    }

    if (_verbose) {
        qDebug() << "domain-server address is" << _domainServerAddress;
    }

    DependencyManager::get<AddressManager>()->handleLookupString(_domainServerAddress, false);

    QTimer* _timeoutTimer = new QTimer(this);
    _timeoutTimer->setSingleShot(true);
    connect(_timeoutTimer, &QTimer::timeout, this, &ATPClientApp::timedOut);
    _timeoutTimer->start(TIMEOUT_MILLISECONDS);
}

ATPClientApp::~ATPClientApp() {
    if (_domainCheckInTimer) {
        QMetaObject::invokeMethod(_domainCheckInTimer, "deleteLater", Qt::QueuedConnection);
    }
    if (_timeoutTimer) {
        QMetaObject::invokeMethod(_timeoutTimer, "deleteLater", Qt::QueuedConnection);
    }
}

void ATPClientApp::domainConnectionRefused(const QString& reasonMessage, int reasonCodeInt, const QString& extraInfo) {
    // this is non-fatal if we are trying to get an authenticated connection -- it will be retried.
    if (_verbose) {
        qDebug() << "domainConnectionRefused";
    }
}

void ATPClientApp::domainChanged(QUrl domainURL) {
    if (_verbose) {
        qDebug() << "domainChanged";
    }
}

void ATPClientApp::nodeAdded(SharedNodePointer node) {
    if (_verbose) {
        qDebug() << "node added: " << node->getType();
    }
}

void ATPClientApp::nodeActivated(SharedNodePointer node) {
    if (!_waitingForLogin && node->getType() == NodeType::AssetServer) {
        _waitingForNode = false;
        go();
    }
}

void ATPClientApp::go() {
    if (_waitingForLogin || _waitingForNode) {
        return;
    }

    auto path = _url.path();

    if (_verbose) {
        qDebug() << "path is " << path;
    }

    if (!_localUploadFile.isEmpty()) {
        uploadAsset();
    } else if (path == "/") {
        listAssets();
    } else {
        lookupAsset();
    }
}

void ATPClientApp::nodeKilled(SharedNodePointer node) {
    if (_verbose) {
        qDebug() << "nodeKilled" << node->getType();
    }
}

void ATPClientApp::timedOut() {
    finish(1);
}

void ATPClientApp::notifyPacketVersionMismatch() {
    if (_verbose) {
        qDebug() << "packet version mismatch";
    }
    finish(1);
}

void ATPClientApp::uploadAsset() {
    auto path = _url.path();
    if (path == "/") {
        qDebug() << "cannot upload to path of /";
        QCoreApplication::exit(1);
    }

    auto upload = DependencyManager::get<AssetClient>()->createUpload(_localUploadFile);
    QObject::connect(upload, &AssetUpload::finished, this, [=](AssetUpload* upload, const QString& hash) mutable {
        if (upload->getError() != AssetUpload::NoError) {
            qDebug() << "upload failed: " << upload->getErrorString();
        } else {
            setMapping(hash);
        }

        upload->deleteLater();
    });

    upload->start();
}

void ATPClientApp::setMapping(QString hash) {
    auto path = _url.path();
    auto assetClient = DependencyManager::get<AssetClient>();
    auto request = assetClient->createSetMappingRequest(path, hash);

    connect(request, &SetMappingRequest::finished, this, [=](SetMappingRequest* request) mutable {
        if (request->getError() != SetMappingRequest::NoError) {
            qDebug() << "upload succeeded, but couldn't set mapping: " << request->getErrorString();
        } else if (_verbose) {
            qDebug() << "mapping set.";
        }
        request->deleteLater();
        finish(0);
    });

    request->start();
}

void ATPClientApp::listAssets() {
    auto request = DependencyManager::get<AssetClient>()->createGetAllMappingsRequest();
    QObject::connect(request, &GetAllMappingsRequest::finished, this, [=](GetAllMappingsRequest* request) mutable {
        auto result = request->getError();
        if (result == GetAllMappingsRequest::NotFound) {
            qDebug() << "not found: " << request->getErrorString();
        } else if (result == GetAllMappingsRequest::NoError) {
            auto mappings = request->getMappings();
            for (auto& kv : mappings ) {
                qDebug() << kv.first << kv.second.hash;
            }
        } else {
            qDebug() << "error -- " << request->getError() << " -- " << request->getErrorString();
        }
        request->deleteLater();
        finish(0);
    });
    request->start();
}

void ATPClientApp::lookupAsset() {
    auto path = _url.path();
    auto request = DependencyManager::get<AssetClient>()->createGetMappingRequest(path);
    QObject::connect(request, &GetMappingRequest::finished, this, [=](GetMappingRequest* request) mutable {
        auto result = request->getError();
        if (result == GetMappingRequest::NotFound) {
            qDebug() << "not found: " << request->getErrorString();
        } else if (result == GetMappingRequest::NoError) {
            qDebug() << "found, hash is " << request->getHash();
            download(request->getHash());
        } else {
            qDebug() << "error -- " << request->getError() << " -- " << request->getErrorString();
        }
        request->deleteLater();
    });
    request->start();
}

void ATPClientApp::download(AssetUtils::AssetHash hash) {
    auto assetClient = DependencyManager::get<AssetClient>();
    auto assetRequest = new AssetRequest(hash);

    connect(assetRequest, &AssetRequest::finished, this, [this](AssetRequest* request) mutable {
        Q_ASSERT(request->getState() == AssetRequest::Finished);

        if (request->getError() == AssetRequest::Error::NoError) {
            QString data = QString::fromUtf8(request->getData());
            if (_localOutputFile == "" || _localOutputFile == "-") {
                QTextStream cout(stdout);
                cout << data;
            } else {
                QFile outputHandle(_localOutputFile);
                if (outputHandle.open(QIODevice::ReadWrite)) {
                    QTextStream stream( &outputHandle );
                    stream << data;
                } else {
                    qDebug() << "couldn't open output file:" << _localOutputFile;
                }
            }
        }

        request->deleteLater();
        finish(0);
    });

    assetRequest->start();
}

void ATPClientApp::finish(int exitCode) {
    auto nodeList = DependencyManager::get<NodeList>();

    // send the domain a disconnect packet, force stoppage of domain-server check-ins
    nodeList->getDomainHandler().disconnect("Finishing");
    nodeList->setIsShuttingDown(true);

    // tell the packet receiver we're shutting down, so it can drop packets
    nodeList->getPacketReceiver().setShouldDropPackets(true);

    // remove the NodeList from the DependencyManager
    DependencyManager::destroy<NodeList>();

    QCoreApplication::exit(exitCode);
}
