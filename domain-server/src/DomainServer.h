//
//  DomainServer.h
//  domain-server/src
//
//  Created by Stephen Birarda on 9/26/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DomainServer_h
#define hifi_DomainServer_h

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QQueue>
#include <QtCore/QSharedPointer>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QHostAddress>
#include <QAbstractNativeEventFilter>

#include <Assignment.h>
#include <HTTPSConnection.h>
#include <LimitedNodeList.h>

#include "AssetsBackupHandler.h"
#include "DomainGatekeeper.h"
#include "DomainMetadata.h"
#include "DomainServerSettingsManager.h"
#include "DomainServerWebSessionData.h"
#include "DomainContentBackupManager.h"

#include "PendingAssignedNodeData.h"
#include "DomainServerExporter.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(domain_server)
Q_DECLARE_LOGGING_CATEGORY(domain_server_ice)
Q_DECLARE_LOGGING_CATEGORY(domain_server_auth)

typedef QSharedPointer<Assignment> SharedAssignmentPointer;

using Subnet = QPair<QHostAddress, int>;
using SubnetList = std::vector<Subnet>;

const int INVALID_ICE_LOOKUP_ID = -1;

enum ReplicationServerDirection {
    Upstream,
    Downstream
};

class DomainServer : public QCoreApplication, public HTTPSRequestHandler {
    Q_OBJECT
public:
    DomainServer(int argc, char* argv[]);
    ~DomainServer();

    static void parseCommandLine(int argc, char* argv[]);

    enum DomainType {
        NonMetaverse,
        MetaverseDomain,
        MetaverseTemporaryDomain
    };

    static int const EXIT_CODE_REBOOT;

    bool handleHTTPRequest(HTTPConnection* connection, const QUrl& url, bool skipSubHandler = false) override;
    bool handleHTTPSRequest(HTTPSConnection* connection, const QUrl& url, bool skipSubHandler = false) override;

    static const QString REPLACEMENT_FILE_EXTENSION;

    bool isAssetServerEnabled();

    void screensharePresence(QString roomname, QUuid avatarID, int expiration_seconds = 0);

public slots:
    /// Called by NodeList to inform us a node has been added
    void nodeAdded(SharedNodePointer node);
    /// Called by NodeList to inform us a node has been killed
    void nodeKilled(SharedNodePointer node);

    void restart();

private slots:
    void processRequestAssignmentPacket(QSharedPointer<ReceivedMessage> packet);
    void processListRequestPacket(QSharedPointer<ReceivedMessage> packet, SharedNodePointer sendingNode);
    void processNodeJSONStatsPacket(QSharedPointer<ReceivedMessage> packetList, SharedNodePointer sendingNode);
    void processPathQueryPacket(QSharedPointer<ReceivedMessage> packet);
    void processNodeDisconnectRequestPacket(QSharedPointer<ReceivedMessage> message);
    void processICEServerHeartbeatDenialPacket(QSharedPointer<ReceivedMessage> message);
    void processICEServerHeartbeatACK(QSharedPointer<ReceivedMessage> message);
    void processAvatarZonePresencePacket(QSharedPointer<ReceivedMessage> packet);

    void handleDomainContentReplacementFromURLRequest(QSharedPointer<ReceivedMessage> message);
    void handleOctreeFileReplacementRequest(QSharedPointer<ReceivedMessage> message);
    bool handleOctreeFileReplacement(QByteArray octreeFile, QString sourceFilename, QString name, QString username);

    void processOctreeDataRequestMessage(QSharedPointer<ReceivedMessage> message);
    void processOctreeDataPersistMessage(QSharedPointer<ReceivedMessage> message);

    void performIPAddressPortUpdate(const HifiSockAddr& newPublicSockAddr);
    void sendHeartbeatToMetaverse() { sendHeartbeatToMetaverse(QString(), int()); }
    void sendHeartbeatToIceServer();
    void nodePingMonitor();

    void handleConnectedNode(SharedNodePointer newNode, quint64 requestReceiveTime);
    void handleTempDomainSuccess(QNetworkReply* requestReply);
    void handleTempDomainError(QNetworkReply* requestReply);

    void handleMetaverseHeartbeatError(QNetworkReply* requestReply);

    void queuedQuit(QString quitMessage, int exitCode);

    void handleKeypairChange();

    void updateICEServerAddresses();
    void handleICEHostInfo(const QHostInfo& hostInfo);

    void sendICEServerAddressToMetaverseAPI();
    void handleSuccessfulICEServerAddressUpdate(QNetworkReply* requestReply);
    void handleFailedICEServerAddressUpdate(QNetworkReply* requestReply);

    void handleSuccessfulScreensharePresence(QNetworkReply* requestReply, QJsonObject callbackData);
    void handleFailedScreensharePresence(QNetworkReply* requestReply);

    void updateReplicatedNodes();
    void updateDownstreamNodes();
    void updateUpstreamNodes();
    void initializeExporter();
    void initializeMetadataExporter();

    void tokenGrantFinished();
    void profileRequestFinished();

    void aboutToQuit();

signals:
    void iceServerChanged();
    void userConnected();
    void userDisconnected();

private:
    QUuid getID();

    QString getContentBackupDir();
    QString getEntitiesDirPath();
    QString getEntitiesFilePath();
    QString getEntitiesReplacementFilePath();

    void maybeHandleReplacementEntityFile();

    void setupNodeListAndAssignments();
    bool optionallySetupOAuth();
    bool optionallyReadX509KeyAndCertificate();

    void getTemporaryName(bool force = false);

    static bool isPacketVerified(const udt::Packet& packet);

    bool resetAccountManagerAccessToken();

    void setupAutomaticNetworking();
    void setupICEHeartbeatForFullNetworking();
    void setupHeartbeatToMetaverse();
    void sendHeartbeatToMetaverse(const QString& networkAddress, const int port);

    void randomizeICEServerAddress(bool shouldTriggerHostLookup);

    unsigned int countConnectedUsers();

    void handleKillNode(SharedNodePointer nodeToKill);
    void broadcastNodeDisconnect(const SharedNodePointer& disconnnectedNode);

    void sendDomainListToNode(const SharedNodePointer& node, quint64 requestPacketReceiveTime, const HifiSockAddr& senderSockAddr, bool newConnection);

    bool isInInterestSet(const SharedNodePointer& nodeA, const SharedNodePointer& nodeB);

    QUuid connectionSecretForNodes(const SharedNodePointer& nodeA, const SharedNodePointer& nodeB);
    void broadcastNewNode(const SharedNodePointer& node);

    void parseAssignmentConfigs(QSet<Assignment::Type>& excludedTypes);
    void addStaticAssignmentToAssignmentHash(Assignment* newAssignment);
    void createStaticAssignmentsForType(Assignment::Type type, const QVariantList& configList);
    void populateDefaultStaticAssignmentsExcludingTypes(const QSet<Assignment::Type>& excludedTypes);
    void populateStaticScriptedAssignmentsFromSettings();

    SharedAssignmentPointer dequeueMatchingAssignment(const QUuid& checkInUUID, NodeType_t nodeType);
    SharedAssignmentPointer deployableAssignmentForRequest(const Assignment& requestAssignment);
    void refreshStaticAssignmentAndAddToQueue(SharedAssignmentPointer& assignment);
    void addStaticAssignmentsToQueue();

    QUrl oauthRedirectURL();
    QUrl oauthAuthorizationURL(const QUuid& stateUUID = QUuid::createUuid());

    std::pair<bool, QString>  isAuthenticatedRequest(HTTPConnection* connection);

    QNetworkReply* profileRequestGivenTokenReply(QNetworkReply* tokenReply);
    Headers setupCookieHeadersFromProfileReply(QNetworkReply* profileReply);

    QJsonObject jsonForSocket(const HifiSockAddr& socket);
    QJsonObject jsonObjectForNode(const SharedNodePointer& node);

    bool shouldReplicateNode(const Node& node);

    void setupGroupCacheRefresh();

    QString pathForRedirect(QString path = QString()) const;

    void updateReplicationNodes(ReplicationServerDirection direction);

    HTTPSConnection* connectionFromReplyWithState(QNetworkReply* reply);

    bool processPendingContent(HTTPConnection* connection, QString itemName, QString filename, QByteArray dataChunk);

    bool forwardMetaverseAPIRequest(HTTPConnection* connection,
                                    const QString& metaversePath,
                                    const QString& requestSubobject,
                                    std::initializer_list<QString> requiredData = { },
                                    std::initializer_list<QString> optionalData = { },
                                    bool requireAccessToken = true);

    QString operationToString(const QNetworkAccessManager::Operation &op);

    SubnetList _acSubnetWhitelist;

    std::vector<QString> _replicatedUsernames;

    DomainGatekeeper _gatekeeper;
    DomainServerExporter _exporter;

    HTTPManager _httpManager;
    HTTPManager* _httpExporterManager { nullptr };
    HTTPManager* _httpMetadataExporterManager { nullptr };
    
    std::unique_ptr<HTTPSManager> _httpsManager;

    QHash<QUuid, SharedAssignmentPointer> _allAssignments;
    QQueue<SharedAssignmentPointer> _unfulfilledAssignments;

    bool _isUsingDTLS { false };

    bool _oauthEnable { false };
    QUrl _oauthProviderURL;
    QString _oauthClientID;
    QString _oauthClientSecret;
    QString _hostname;

    std::unordered_map<QUuid, QByteArray> _ephemeralACScripts;

    QSet<QUuid> _webAuthenticationStateSet;
    QHash<QUuid, DomainServerWebSessionData> _cookieSessionHash;

    QString _automaticNetworkingSetting;

    DomainServerSettingsManager _settingsManager;

    HifiSockAddr _iceServerSocket;
    std::unique_ptr<NLPacket> _iceServerHeartbeatPacket;

    // These will be parented to this, they are not dangling
    DomainMetadata* _metadata { nullptr };
    QTimer* _iceHeartbeatTimer { nullptr };
    QTimer* _metaverseHeartbeatTimer { nullptr };
    QTimer* _metaverseGroupCacheTimer { nullptr };
    QTimer* _nodePingMonitorTimer { nullptr };

    QList<QHostAddress> _iceServerAddresses;
    QSet<QHostAddress> _failedIceServerAddresses;
    int _iceAddressLookupID { INVALID_ICE_LOOKUP_ID };
    int _noReplyICEHeartbeats { 0 };
    int _numHeartbeatDenials { 0 };
    bool _connectedToICEServer { false };

    DomainType _type { DomainType::NonMetaverse };

    friend class DomainGatekeeper;
    friend class DomainMetadata;

    static QString _iceServerAddr;
    static int _iceServerPort;
    static bool _overrideDomainID; // should we override the domain-id from settings?
    static QUuid _overridingDomainID; // what should we override it with?
    static bool _getTempName;
    static QString _userConfigFilename;
    static int _parentPID;

    bool _sendICEServerAddressToMetaverseAPIInProgress { false };
    bool _sendICEServerAddressToMetaverseAPIRedo { false };

    std::unique_ptr<DomainContentBackupManager> _contentManager { nullptr };

    QHash<QUuid, QPointer<HTTPSConnection>> _pendingOAuthConnections;

    std::unordered_map<int, QByteArray> _pendingUploadedContents;
    std::unordered_map<int, std::unique_ptr<QTemporaryFile>> _pendingContentFiles;

    QThread _assetClientThread;
};


#endif // hifi_DomainServer_h
