//
//  AssignmentClient.cpp
//  assignment-client/src
//
//  Created by Stephen Birarda on 11/25/2013.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AssignmentClient.h"

#include <assert.h>

#include <QProcess>
#include <QSharedMemory>
#include <QThread>
#include <QTimer>

#include <shared/QtHelpers.h>
#include <AccountManager.h>
#include <AddressManager.h>
#include <Assignment.h>
#include <CrashAnnotations.h>
#include <LogHandler.h>
#include <LogUtils.h>
#include <LimitedNodeList.h>
#include <NodeList.h>
#include <udt/PacketHeaders.h>
#include <SharedUtil.h>
#include <ShutdownEventListener.h>

#include <Trace.h>
#include <StatTracker.h>
#include <ThreadHelpers.h>

#include "AssignmentClientLogging.h"
#include "AssignmentFactory.h"
#include "ResourceRequestObserver.h"

const QString ASSIGNMENT_CLIENT_TARGET_NAME = "assignment-client";
const long long ASSIGNMENT_REQUEST_INTERVAL_MSECS = 1 * 1000;

AssignmentClient::AssignmentClient(Assignment::Type requestAssignmentType, QString assignmentPool,
                                   quint16 listenPort, QUuid walletUUID, QString assignmentServerHostname,
                                   quint16 assignmentServerPort, quint16 assignmentMonitorPort,
                                   bool disableDomainPortAutoDiscovery) :
    _assignmentServerHostname(DEFAULT_ASSIGNMENT_SERVER_HOSTNAME)
{
    LogUtils::init();

    DependencyManager::set<tracing::Tracer>();
    DependencyManager::set<StatTracker>();
    DependencyManager::set<AccountManager>();
    DependencyManager::set<ResourceRequestObserver>();

    auto addressManager = DependencyManager::set<AddressManager>();

    // create a NodeList as an unassigned client, must be after addressManager
    auto nodeList = DependencyManager::set<NodeList>(NodeType::Unassigned, listenPort);

    nodeList->startThread();
    // set the logging target to the the CHILD_TARGET_NAME
    LogHandler::getInstance().setTargetName(ASSIGNMENT_CLIENT_TARGET_NAME);

    // make sure we output process IDs for a child AC otherwise it's insane to parse
    LogHandler::getInstance().setShouldOutputProcessID(true);

    // setup our _requestAssignment member variable from the passed arguments
    _requestAssignment = Assignment(Assignment::RequestCommand, requestAssignmentType, assignmentPool);

    // check for a wallet UUID on the command line or in the config
    // this would represent where the user running AC wants funds sent to
    if (!walletUUID.isNull()) {
        qCDebug(assignment_client) << "The destination wallet UUID for credits is" << uuidStringWithoutCurlyBraces(walletUUID);
        _requestAssignment.setWalletUUID(walletUUID);
    }

    // check for an overriden assignment server hostname
    if (assignmentServerHostname != "") {
        // change the hostname for our assignment server
        _assignmentServerHostname = assignmentServerHostname;
    }

    _assignmentServerSocket = SockAddr(_assignmentServerHostname, assignmentServerPort, true);
    if (_assignmentServerSocket.isNull()) {
        qCCritical(assignment_client) << "PAGE: Couldn't resolve domain server address" << _assignmentServerHostname;
    }
    _assignmentServerSocket.setObjectName("AssignmentServer");
    nodeList->setAssignmentServerSocket(_assignmentServerSocket);

    if (disableDomainPortAutoDiscovery == true) {
        _disableDomainPortAutoDiscovery = disableDomainPortAutoDiscovery;
        qCDebug(assignment_client) << "Disabling domain port auto discovery by the assignment client due to parsed command line parameter.";
    }

    nodeList->disableDomainPortAutoDiscovery(_disableDomainPortAutoDiscovery);

    qCDebug(assignment_client) << "Assignment server socket is" << _assignmentServerSocket;

    // call a timer function every ASSIGNMENT_REQUEST_INTERVAL_MSECS to ask for assignment, if required
    qCDebug(assignment_client) << "Waiting for assignment -" << _requestAssignment;

    if (_assignmentServerHostname != "localhost") {
        qCDebug(assignment_client) << "- will attempt to connect to domain-server on" << _assignmentServerSocket.getPort();
    }

    connect(&_requestTimer, SIGNAL(timeout()), SLOT(sendAssignmentRequest()));
    _requestTimer.start(ASSIGNMENT_REQUEST_INTERVAL_MSECS);

    // connections to AccountManager for authentication
    connect(DependencyManager::get<AccountManager>().data(), &AccountManager::authRequired,
            this, &AssignmentClient::handleAuthenticationRequest);

    // Create Singleton objects on main thread
    NetworkAccessManager::getInstance();

    // did we get an assignment-client monitor port?
    if (assignmentMonitorPort > 0) {
        _assignmentClientMonitorSocket = SockAddr(DEFAULT_ASSIGNMENT_CLIENT_MONITOR_HOSTNAME, assignmentMonitorPort);
        _assignmentClientMonitorSocket.setObjectName("AssignmentClientMonitor");

        qCDebug(assignment_client) << "Assignment-client monitor socket is" << _assignmentClientMonitorSocket;

        // Hook up a timer to send this child's status to the Monitor once per second
        setUpStatusToMonitor();
    }
    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();
    packetReceiver.registerListener(PacketType::CreateAssignment,
        PacketReceiver::makeUnsourcedListenerReference<AssignmentClient>(this, &AssignmentClient::handleCreateAssignmentPacket));
    packetReceiver.registerListener(PacketType::StopNode,
        PacketReceiver::makeUnsourcedListenerReference<AssignmentClient>(this, &AssignmentClient::handleStopNodePacket));
}

void AssignmentClient::stopAssignmentClient() {
    qCDebug(assignment_client) << "Forced stop of assignment-client.";

    _requestTimer.stop();
    _statsTimerACM.stop();

    if (_currentAssignment) {
        // grab the thread for the current assignment
        QThread* currentAssignmentThread = _currentAssignment->thread();

        // ask the current assignment to stop
        QMetaObject::invokeMethod(_currentAssignment, "stop");

        auto PROCESS_EVENTS_INTERVAL_MS = 100;
        while (!currentAssignmentThread->wait(PROCESS_EVENTS_INTERVAL_MS)) {
            QCoreApplication::processEvents();
        }
    }
}

AssignmentClient::~AssignmentClient() {
    // remove the NodeList from the DependencyManager
    DependencyManager::destroy<NodeList>();
}

void AssignmentClient::aboutToQuit() {
    crash::annotations::setShutdownState(true);
    stopAssignmentClient();
}

void AssignmentClient::setUpStatusToMonitor() {
    // send a stats packet every 1 seconds
    connect(&_statsTimerACM, &QTimer::timeout, this, &AssignmentClient::sendStatusPacketToACM);
    _statsTimerACM.start(1000);
}

void AssignmentClient::sendStatusPacketToACM() {
    // tell the assignment client monitor what this assignment client is doing (if anything)
    auto nodeList = DependencyManager::get<NodeList>();

    quint8 assignmentType = Assignment::Type::AllTypes;

    if (_currentAssignment) {
        assignmentType = _currentAssignment->getType();
    }

    auto statusPacket = NLPacket::create(PacketType::AssignmentClientStatus, sizeof(assignmentType) + NUM_BYTES_RFC4122_UUID);

    statusPacket->write(_childAssignmentUUID.toRfc4122());
    statusPacket->writePrimitive(assignmentType);

    nodeList->sendPacket(std::move(statusPacket), _assignmentClientMonitorSocket);
}

void AssignmentClient::sendAssignmentRequest() {
    if (!_currentAssignment && !_isAssigned) {
        crash::annotations::setShutdownState(false);

        auto nodeList = DependencyManager::get<NodeList>();

        if (_assignmentServerHostname == "localhost" && !_disableDomainPortAutoDiscovery) {
            // we want to check again for the local domain-server port in case the DS has restarted
            quint16 localAssignmentServerPort;
            if (nodeList->getLocalServerPortFromSharedMemory(DOMAIN_SERVER_LOCAL_PORT_SMEM_KEY, localAssignmentServerPort)) {
                if (localAssignmentServerPort == 0) {
                    qCWarning(assignment_client) << "ALERT: Server port from shared memory is 0";
                } else {
                    if (localAssignmentServerPort != _assignmentServerSocket.getPort()) {
                        qCDebug(assignment_client) << "Port for local assignment server read from shared memory is"
                            << localAssignmentServerPort;

                        _assignmentServerSocket.setPort(localAssignmentServerPort);
                        nodeList->setAssignmentServerSocket(_assignmentServerSocket);
                    }
                }
            } else {
                qCWarning(assignment_client) << "ALERT: Failed to read local assignment server port from shared memory ("
                    << DOMAIN_SERVER_LOCAL_PORT_SMEM_KEY
                    << ")- will send assignment request to previous assignment server socket.";
            }
        }

        nodeList->sendAssignment(_requestAssignment);
    }
}

void AssignmentClient::handleCreateAssignmentPacket(QSharedPointer<ReceivedMessage> message) {
    qCDebug(assignment_client) << "Received a PacketType::CreateAssignment - attempting to unpack.";

    if (_currentAssignment) {
        qCWarning(assignment_client) << "Received a PacketType::CreateAssignment while still running an active assignment. Ignoring.";
        return;
    }

    // construct the deployed assignment from the packet data
    _currentAssignment = AssignmentFactory::unpackAssignment(*message);

    if (_currentAssignment && !_isAssigned) {
        qDebug(assignment_client) << "Received an assignment -" << *_currentAssignment;
        _isAssigned = true;

        auto nodeList = DependencyManager::get<NodeList>();

        // switch our DomainHandler hostname and port to whoever sent us the assignment

        nodeList->getDomainHandler().setSockAddr(message->getSenderSockAddr(), _assignmentServerHostname);
        nodeList->getDomainHandler().setAssignmentUUID(_currentAssignment->getUUID());

        qCDebug(assignment_client) << "Destination IP for assignment is" << nodeList->getDomainHandler().getIP().toString();

        // start the deployed assignment
        QThread* workerThread = new QThread();
        workerThread->setObjectName("ThreadedAssignment Worker");

        connect(workerThread, &QThread::started, _currentAssignment.data(), [this] {
            setThreadName("ThreadedAssignment Worker");
            _currentAssignment->run();
        });

        // Once the ThreadedAssignment says it is finished - we ask it to deleteLater
        // This is a queued connection so that it is put into the event loop to be processed by the worker
        // thread when it is ready.
        connect(_currentAssignment.data(), &ThreadedAssignment::finished, _currentAssignment.data(),
                &ThreadedAssignment::deleteLater, Qt::QueuedConnection);

        // once it is deleted, we quit the worker thread
        connect(_currentAssignment.data(), &ThreadedAssignment::destroyed, workerThread, &QThread::quit);

        // have the worker thread remove itself once it is done
        connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);

        // once the worker thread says it is done, we consider the assignment completed
        connect(workerThread, &QThread::destroyed, this, &AssignmentClient::assignmentCompleted);

        _currentAssignment->moveToThread(workerThread);

        // Starts an event loop, and emits workerThread->started()
        workerThread->start();
    } else {
        qCWarning(assignment_client) << "ALERT: Received an assignment that could not be unpacked. Re-requesting.";
    }
}

void AssignmentClient::handleStopNodePacket(QSharedPointer<ReceivedMessage> message) {
    const SockAddr& senderSockAddr = message->getSenderSockAddr();

    if (senderSockAddr.getAddress() == QHostAddress::LocalHost ||
        senderSockAddr.getAddress() == QHostAddress::LocalHostIPv6) {

        qCDebug(assignment_client) << "AssignmentClientMonitor at" << senderSockAddr << "requested stop via PacketType::StopNode.";
        QCoreApplication::quit();
    } else {
        qCWarning(assignment_client) << "Got a stop packet from other than localhost.";
    }
}

void AssignmentClient::handleAuthenticationRequest() {
    const QString DATA_SERVER_USERNAME_ENV = "HIFI_AC_USERNAME";
    const QString DATA_SERVER_PASSWORD_ENV = "HIFI_AC_PASSWORD";

    // this node will be using an authentication server, let's make sure we have a username/password
    QProcessEnvironment sysEnvironment = QProcessEnvironment::systemEnvironment();

    QString username = sysEnvironment.value(DATA_SERVER_USERNAME_ENV);
    QString password = sysEnvironment.value(DATA_SERVER_PASSWORD_ENV);

    auto accountManager = DependencyManager::get<AccountManager>();

    if (!username.isEmpty() && !password.isEmpty()) {
        // ask the account manager to log us in from the env variables
        accountManager->requestAccessToken(username, password);
    } else {
        qCWarning(assignment_client) << "Authentication was requested against" << qPrintable(accountManager->getAuthURL().toString())
            << "but both or one of" << qPrintable(DATA_SERVER_USERNAME_ENV)
            << "/" << qPrintable(DATA_SERVER_PASSWORD_ENV) << "are not set. Unable to authenticate.";

        return;
    }
}

void AssignmentClient::assignmentCompleted() {
    crash::annotations::setShutdownState(true);

    // we expect that to be here the previous assignment has completely cleaned up
    assert(_currentAssignment.isNull());

    // reset our current assignment pointer to null now that it has been deleted
    _currentAssignment = nullptr;

    // reset the logging target to the the CHILD_TARGET_NAME
    LogHandler::getInstance().setTargetName(ASSIGNMENT_CLIENT_TARGET_NAME);

    qCDebug(assignment_client) << "Assignment finished or never started - waiting for new assignment.";

    auto nodeList = DependencyManager::get<NodeList>();

    // tell the packet receiver to stop dropping packets
    nodeList->getPacketReceiver().setShouldDropPackets(false);

    // reset our NodeList by switching back to unassigned and clearing the list
    nodeList->setOwnerType(NodeType::Unassigned);
    nodeList->reset("Assignment completed");
    nodeList->resetNodeInterestSet();

    _isAssigned = false;
}
