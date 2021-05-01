//
//  AssignmentClient.h
//  assignment-client/src
//
//  Created by Stephen Birarda on 11/25/2013.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssignmentClient_h
#define hifi_AssignmentClient_h

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>

#include "ThreadedAssignment.h"

class QSharedMemory;

class AssignmentClient : public QObject {
    Q_OBJECT
public:
    AssignmentClient(Assignment::Type requestAssignmentType, QString assignmentPool,
                     quint16 listenPort,
                     QString assignmentServerHostname, quint16 assignmentServerPort,
                     quint16 assignmentMonitorPort);
    ~AssignmentClient();

private slots:
    void sendAssignmentRequest();
    void assignmentCompleted();
    void handleAuthenticationRequest();
    void sendStatusPacketToACM();
    void stopAssignmentClient();

public slots:
    void aboutToQuit();

private slots:
    void handleCreateAssignmentPacket(QSharedPointer<ReceivedMessage> message);
    void handleStopNodePacket(QSharedPointer<ReceivedMessage> message);

private:
    void setUpStatusToMonitor();

    Assignment _requestAssignment;
    QPointer<ThreadedAssignment> _currentAssignment;
    bool _isAssigned { false };
    QString _assignmentServerHostname;
    HifiSockAddr _assignmentServerSocket;
    QTimer _requestTimer; // timer for requesting and assignment
    QTimer _statsTimerACM; // timer for sending stats to assignment client monitor
    QUuid _childAssignmentUUID = QUuid::createUuid();

 protected:
    HifiSockAddr _assignmentClientMonitorSocket;
};

#endif // hifi_AssignmentClient_h
