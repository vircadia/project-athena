//
//  AssignmentClientMonitor.h
//  assignment-client/src
//
//  Created by Stephen Birarda on 1/10/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssignmentClientMonitor_h
#define hifi_AssignmentClientMonitor_h

#include <QtCore/QCoreApplication>
#include <QtCore/qpointer.h>
#include <QtCore/QProcess>
#include <QtCore/QDateTime>
#include <QDir>

#include <Assignment.h>

#include "AssignmentClientChildData.h"
#include <HTTPManager.h>
#include <HTTPConnection.h>

extern const char* NUM_FORKS_PARAMETER;

struct ACProcess {
    QProcess* process; // looks like a dangling pointer, but is parented by the AssignmentClientMonitor 
    QString logStdoutPath;
    QString logStderrPath;
};

class AssignmentClientMonitor : public QObject, public HTTPRequestHandler {
    Q_OBJECT
public:
    AssignmentClientMonitor(const unsigned int numAssignmentClientForks, const unsigned int minAssignmentClientForks,
                            const unsigned int maxAssignmentClientForks, Assignment::Type requestAssignmentType,
                            QString assignmentPool, quint16 listenPort, quint16 childMinListenPort,
                            QString assignmentServerHostname, quint16 assignmentServerPort, quint16 httpStatusServerPort,
                            QString logDirectory);
    ~AssignmentClientMonitor();

    void stopChildProcesses();
private slots:
    void checkSpares();
    void childProcessFinished(qint64 pid, quint16 port, int exitCode, QProcess::ExitStatus exitStatus);
    void handleChildStatusPacket(QSharedPointer<ReceivedMessage> message);

    bool handleHTTPRequest(HTTPConnection* connection, const QUrl& url, bool skipSubHandler = false) override;

public slots:
    void aboutToQuit();

private:
    void spawnChildClient();
    void simultaneousWaitOnChildren(int waitMsecs);
    void adjustOSResources(unsigned int numForks) const;

    QTimer _checkSparesTimer; // every few seconds see if it need fewer or more spare children

    QDir _logDirectory;

    HTTPManager _httpManager;

    const unsigned int _numAssignmentClientForks;
    const unsigned int _minAssignmentClientForks;
    const unsigned int _maxAssignmentClientForks;

    Assignment::Type _requestAssignmentType;
    QString _assignmentPool;
    QString _assignmentServerHostname;
    quint16 _assignmentServerPort;

    QMap<qint64, ACProcess> _childProcesses;

    quint16 _childMinListenPort;
    QSet<quint16> _childListenPorts;

    bool _wantsChildFileLogging { false };
};

#endif // hifi_AssignmentClientMonitor_h
