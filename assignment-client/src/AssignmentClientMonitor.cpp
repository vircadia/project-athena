//
//  AssignmentClientMonitor.cpp
//  assignment-client/src
//
//  Created by Stephen Birarda on 1/10/2014.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AssignmentClientMonitor.h"

#include <memory>
#include <signal.h>

#include <QDir>
#include <QStandardPaths>

#include <AddressManager.h>
#include <LogHandler.h>
#include <udt/PacketHeaders.h>

#include "AssignmentClientApp.h"
#include "AssignmentClientChildData.h"
#include "SharedUtil.h"
#include <QtCore/QJsonDocument>
#ifdef _POSIX_SOURCE
#include <sys/resource.h>
#endif

const QString ASSIGNMENT_CLIENT_MONITOR_TARGET_NAME = "assignment-client-monitor";
const int WAIT_FOR_CHILD_MSECS = 1000;

#ifdef Q_OS_WIN
void* PROCESS_GROUP = createProcessGroup();
#endif

AssignmentClientMonitor::AssignmentClientMonitor(const unsigned int numAssignmentClientForks,
                                                 const unsigned int minAssignmentClientForks,
                                                 const unsigned int maxAssignmentClientForks,
                                                 Assignment::Type requestAssignmentType, QString assignmentPool,
                                                 quint16 listenPort, quint16 childMinListenPort, QUuid walletUUID, QString assignmentServerHostname,
                                                 quint16 assignmentServerPort, quint16 httpStatusServerPort, QString logDirectory,
                                                 bool disableDomainPortAutoDiscovery) :
    _httpManager(QHostAddress::LocalHost, httpStatusServerPort, "", this),
    _numAssignmentClientForks(numAssignmentClientForks),
    _minAssignmentClientForks(minAssignmentClientForks),
    _maxAssignmentClientForks(maxAssignmentClientForks),
    _requestAssignmentType(requestAssignmentType),
    _assignmentPool(assignmentPool),
    _walletUUID(walletUUID),
    _assignmentServerHostname(assignmentServerHostname),
    _assignmentServerPort(assignmentServerPort),
    _childMinListenPort(childMinListenPort),
    _disableDomainPortAutoDiscovery(disableDomainPortAutoDiscovery)
{
    qDebug() << "_requestAssignmentType =" << _requestAssignmentType;

    if (!logDirectory.isEmpty()) {
        _wantsChildFileLogging = true;
        _logDirectory = QDir(logDirectory);
    }

    // start the Logging class with the parent's target name
    LogHandler::getInstance().setTargetName(ASSIGNMENT_CLIENT_MONITOR_TARGET_NAME);

    // make sure we output process IDs for a monitor otherwise it's insane to parse
    LogHandler::getInstance().setShouldOutputProcessID(true);

    // create a NodeList so we can receive stats from children
    DependencyManager::registerInheritance<LimitedNodeList, NodeList>();
    auto addressManager = DependencyManager::set<AddressManager>();
    auto nodeList = DependencyManager::set<LimitedNodeList>(listenPort);

    auto& packetReceiver = DependencyManager::get<NodeList>()->getPacketReceiver();
    packetReceiver.registerListener(PacketType::AssignmentClientStatus,
        PacketReceiver::makeUnsourcedListenerReference<AssignmentClientMonitor>(this, &AssignmentClientMonitor::handleChildStatusPacket));

    adjustOSResources(std::max(_numAssignmentClientForks, _maxAssignmentClientForks));
    // use QProcess to fork off a process for each of the child assignment clients
    for (unsigned int i = 0; i < _numAssignmentClientForks; i++) {
        spawnChildClient();
    }

    connect(&_checkSparesTimer, &QTimer::timeout, this, &AssignmentClientMonitor::checkSpares);

    _checkSparesTimer.start(NODE_SILENCE_THRESHOLD_MSECS * 3);
}

AssignmentClientMonitor::~AssignmentClientMonitor() {
    stopChildProcesses();
}

void AssignmentClientMonitor::simultaneousWaitOnChildren(int waitMsecs) {
    QElapsedTimer waitTimer;
    waitTimer.start();

    // loop as long as we still have processes around and we're inside the wait window
    while(_childProcesses.size() > 0 && !waitTimer.hasExpired(waitMsecs)) {
        // continue processing events so we can handle a process finishing up
        QCoreApplication::processEvents();
    }
}

void AssignmentClientMonitor::childProcessFinished(qint64 pid, quint16 listenPort, int exitCode, QProcess::ExitStatus exitStatus) {
    auto message = "Child process " + QString::number(pid) + " on port " + QString::number(listenPort) +
                   "has %1 with exit code " + QString::number(exitCode) + ".";

    if (listenPort) {
        _childListenPorts.remove(listenPort);
    }

    if (_childProcesses.remove(pid)) {
        message.append(" Removed from internal map.");
    } else {
        message.append(" Could not find process in internal map.");
    }

    switch (exitStatus) {
        case QProcess::NormalExit:
            qDebug() << qPrintable(message.arg("returned"));
            break;
        case QProcess::CrashExit:
            qCritical() << qPrintable(message.arg("crashed"));
            break;
    }
}

void AssignmentClientMonitor::stopChildProcesses() {
    qDebug() << "Stopping child processes";
    auto nodeList = DependencyManager::get<NodeList>();

    // ask child processes to terminate
    for (auto& ac : _childProcesses) {
        if (ac.process->processId() > 0) {
            qDebug() << "Attempting to terminate child process" << ac.process->processId();
            ac.process->terminate();
        }
    }

    simultaneousWaitOnChildren(WAIT_FOR_CHILD_MSECS);

    if (_childProcesses.size() > 0) {
        // ask even more firmly
        for (auto& ac : _childProcesses) {
            if (ac.process->processId() > 0) {
                qDebug() << "Attempting to kill child process" << ac.process->processId();
                ac.process->kill();
            }
        }

        simultaneousWaitOnChildren(WAIT_FOR_CHILD_MSECS);
    }
}

void AssignmentClientMonitor::aboutToQuit() {
    stopChildProcesses();
}

void AssignmentClientMonitor::spawnChildClient() {
    QProcess* assignmentClient = new QProcess(this);

    quint16 listenPort = 0;
    // allocate a port

    if (_childMinListenPort) {
        for (listenPort = _childMinListenPort; _childListenPorts.contains(listenPort); listenPort++) {
            if (_maxAssignmentClientForks &&
                (listenPort >= _maxAssignmentClientForks + _childMinListenPort)) {
                listenPort = 0;
                qDebug() << "Insufficient listen ports";
                break;
            }
        }
    }
    if (listenPort) {
        _childListenPorts.insert(listenPort);
    }

    // unparse the parts of the command-line that the child cares about
    QStringList _childArguments;
    if (_assignmentPool != "") {
        _childArguments.append("--" + ASSIGNMENT_POOL_OPTION);
        _childArguments.append(_assignmentPool);
    }
    if (!_walletUUID.isNull()) {
        _childArguments.append("--" + ASSIGNMENT_WALLET_DESTINATION_ID_OPTION);
        _childArguments.append(_walletUUID.toString());
    }
    if (_assignmentServerHostname != "") {
        _childArguments.append("--" + CUSTOM_ASSIGNMENT_SERVER_HOSTNAME_OPTION);
        _childArguments.append(_assignmentServerHostname);
    }
    if (_assignmentServerPort != DEFAULT_DOMAIN_SERVER_PORT) {
        _childArguments.append("--" + CUSTOM_ASSIGNMENT_SERVER_PORT_OPTION);
        _childArguments.append(QString::number(_assignmentServerPort));
    }
    if (_requestAssignmentType != Assignment::AllTypes) {
        _childArguments.append("--" + ASSIGNMENT_TYPE_OVERRIDE_OPTION);
        _childArguments.append(QString::number(_requestAssignmentType));
    }
    if (_disableDomainPortAutoDiscovery) {
        _childArguments.append("--" + ASSIGNMENT_DISABLE_DOMAIN_AUTO_PORT_DISCOVERY);
    }

    if (listenPort) {
        _childArguments.append("-" + ASSIGNMENT_CLIENT_LISTEN_PORT_OPTION);
        _childArguments.append(QString::number(listenPort));
    }

    // tell children which assignment monitor port to use
    // for now they simply talk to us on localhost
    _childArguments.append("--" + ASSIGNMENT_CLIENT_MONITOR_PORT_OPTION);
    _childArguments.append(QString::number(DependencyManager::get<NodeList>()->getLocalSockAddr().getPort()));

    _childArguments.append("--" + PARENT_PID_OPTION);
    _childArguments.append(QString::number(QCoreApplication::applicationPid()));

    QString nowString, stdoutFilenameTemp, stderrFilenameTemp, stdoutPathTemp, stderrPathTemp;


    if (_wantsChildFileLogging) {
        // Setup log files
        const QString DATETIME_FORMAT = "yyyyMMdd.hh.mm.ss.zzz";

        if (!_logDirectory.exists()) {
            qDebug() << "Log directory (" << _logDirectory.absolutePath() << ") does not exist, creating.";
            _logDirectory.mkpath(_logDirectory.absolutePath());
        }

        nowString = QDateTime::currentDateTime().toString(DATETIME_FORMAT);
        stdoutFilenameTemp = QString("ac-%1-stdout.txt").arg(nowString);
        stderrFilenameTemp = QString("ac-%1-stderr.txt").arg(nowString);
        stdoutPathTemp = _logDirectory.absoluteFilePath(stdoutFilenameTemp);
        stderrPathTemp = _logDirectory.absoluteFilePath(stderrFilenameTemp);

        // reset our output and error files
        assignmentClient->setStandardOutputFile(stdoutPathTemp);
        assignmentClient->setStandardErrorFile(stderrPathTemp);
    }

    // make sure that the output from the child process appears in our output
    assignmentClient->setProcessChannelMode(QProcess::ForwardedChannels);
    assignmentClient->start(QCoreApplication::applicationFilePath(), _childArguments);

#ifdef Q_OS_WIN
    addProcessToGroup(PROCESS_GROUP, assignmentClient->processId());
#endif

    QString stdoutPath, stderrPath;

    if (_wantsChildFileLogging) {

        // Update log path to use PID in filename
        auto stdoutFilename = QString("ac-%1_%2-stdout.txt").arg(nowString).arg(assignmentClient->processId());
        auto stderrFilename = QString("ac-%1_%2-stderr.txt").arg(nowString).arg(assignmentClient->processId());
        stdoutPath = _logDirectory.absoluteFilePath(stdoutFilename);
        stderrPath = _logDirectory.absoluteFilePath(stderrFilename);

        qDebug() << "Renaming " << stdoutPathTemp << " to " << stdoutPath;
        if (!_logDirectory.rename(stdoutFilenameTemp, stdoutFilename)) {
            qDebug() << "Failed to rename " << stdoutFilenameTemp;
            stdoutPath = stdoutPathTemp;
            stdoutFilename = stdoutFilenameTemp;
        }

        qDebug() << "Renaming " << stderrPathTemp << " to " << stderrPath;
        if (!QFile::rename(stderrPathTemp, stderrPath)) {
            qDebug() << "Failed to rename " << stderrFilenameTemp;
            stderrPath = stderrPathTemp;
            stderrFilename = stderrFilenameTemp;
        }

        qDebug() << "Child stdout being written to: " << stdoutFilename;
        qDebug() << "Child stderr being written to: " << stderrFilename;
    }

    if (assignmentClient->processId() > 0) {
        auto pid = assignmentClient->processId();
        // make sure we hear that this process has finished when it does
        connect(assignmentClient, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                this, [this, listenPort, pid](int exitCode, QProcess::ExitStatus exitStatus) {
                    childProcessFinished(pid, listenPort, exitCode, exitStatus);
            });

        qDebug() << "Spawned a child client with PID" << assignmentClient->processId();

        _childProcesses.insert(assignmentClient->processId(), { assignmentClient, stdoutPath, stderrPath });
    }
}

void AssignmentClientMonitor::checkSpares() {
    auto nodeList = DependencyManager::get<NodeList>();
    QUuid aSpareId = "";
    unsigned int spareCount = 0;
    unsigned int totalCount = 0;

    nodeList->removeSilentNodes();

    nodeList->eachNode([&](const SharedNodePointer& node) {
        AssignmentClientChildData* childData = static_cast<AssignmentClientChildData*>(node->getLinkedData());
        totalCount ++;
        if (childData->getChildType() == Assignment::Type::AllTypes) {
            ++spareCount;
            aSpareId = node->getUUID();
        }
    });

    // Spawn or kill children, as needed.  If --min or --max weren't specified, allow the child count
    // to drift up or down as far as needed.

    if (spareCount < 1 || totalCount < _minAssignmentClientForks) {
        if (!_maxAssignmentClientForks || totalCount < _maxAssignmentClientForks) {
            spawnChildClient();
        }
    }

    if (spareCount > 1) {
        if (!_minAssignmentClientForks || totalCount > _minAssignmentClientForks) {
            // kill aSpareId
            qDebug() << "asking child" << aSpareId << "to exit.";
            SharedNodePointer childNode = nodeList->nodeWithUUID(aSpareId);
            childNode->activateLocalSocket();

            auto diePacket = NLPacket::create(PacketType::StopNode, 0);
            nodeList->sendPacket(std::move(diePacket), *childNode);
        }
    }
}

void AssignmentClientMonitor::handleChildStatusPacket(QSharedPointer<ReceivedMessage> message) {
    // read out the sender ID
    QUuid senderID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));

    auto nodeList = DependencyManager::get<NodeList>();

    SharedNodePointer matchingNode = nodeList->nodeWithUUID(senderID);
    const SockAddr& senderSockAddr = message->getSenderSockAddr();

    AssignmentClientChildData* childData = nullptr;

    if (!matchingNode) {
        // The parent only expects to be talking with programs running on this same machine.
        if (senderSockAddr.getAddress() == QHostAddress::LocalHost ||
                senderSockAddr.getAddress() == QHostAddress::LocalHostIPv6) {

            if (!senderID.isNull()) {
                // We don't have this node yet - we should add it
                matchingNode = DependencyManager::get<LimitedNodeList>()->addOrUpdateNode(senderID, NodeType::Unassigned,
                                                                                          senderSockAddr, senderSockAddr);

                auto newChildData = std::unique_ptr<AssignmentClientChildData>
                    { new AssignmentClientChildData(Assignment::Type::AllTypes) };
                matchingNode->setLinkedData(std::move(newChildData));
            } else {
                // tell unknown assignment-client child to exit.
                qDebug() << "Asking unknown child at" << senderSockAddr << "to exit.";

                auto diePacket = NLPacket::create(PacketType::StopNode, 0);
                nodeList->sendPacket(std::move(diePacket), senderSockAddr);

                return;
            }
        }
    }
    childData = dynamic_cast<AssignmentClientChildData*>(matchingNode->getLinkedData());

    if (childData) {
        // update our records about how to reach this child
        matchingNode->setLocalSocket(senderSockAddr);

        // get child's assignment type out of the packet
        quint8 assignmentType;
        message->readPrimitive(&assignmentType);

        childData->setChildType(Assignment::Type(assignmentType));

        // note when this child talked
        matchingNode->setLastHeardMicrostamp(usecTimestampNow());
    }
}

bool AssignmentClientMonitor::handleHTTPRequest(HTTPConnection* connection, const QUrl& url, bool skipSubHandler) {
    if (url.path() == "/status") {
        QByteArray response;

        QJsonObject status;
        QJsonObject servers;
        for (auto& ac : _childProcesses) {
            QJsonObject server;

            server["pid"] = ac.process->processId();
            server["logStdout"] = ac.logStdoutPath;
            server["logStderr"] = ac.logStderrPath;

            servers[QString::number(ac.process->processId())] = server;
        }

        status["servers"] = servers;

        QJsonDocument document { status };

        connection->respond(HTTPConnection::StatusCode200, document.toJson());
    } else {
        connection->respond(HTTPConnection::StatusCode404);
    }


    return true;
}

void AssignmentClientMonitor::adjustOSResources(unsigned int numForks) const
{
#ifdef _POSIX_SOURCE
    // QProcess on Unix uses six (I think) descriptors, some temporarily, for each child proc.
    // Formula based on tests with a Ubuntu 16.04 VM.
    unsigned requiredDescriptors = 30 + 6 * numForks;
    struct rlimit descLimits;
    if (getrlimit(RLIMIT_NOFILE, &descLimits) == 0) {
        if (descLimits.rlim_cur < requiredDescriptors) {
            descLimits.rlim_cur = requiredDescriptors;
            if (setrlimit(RLIMIT_NOFILE, &descLimits) == 0) {
                qDebug() << "Resetting descriptor limit to" << requiredDescriptors;
            } else {
                const char *const errorString = strerror(errno);
                qDebug() << "Failed to reset descriptor limit to" << requiredDescriptors << ":" << errorString;
            }
        }
    } else {
        const char *const errorString = strerror(errno);
        qDebug() << "Failed to read descriptor limit:" << errorString;
    }
#endif
}
