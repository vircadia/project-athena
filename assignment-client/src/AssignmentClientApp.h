//
//  AssignmentClientapp.h
//  assignment-client/src
//
//  Created by Seth Alves on 2/19/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssignmentClientApp_h
#define hifi_AssignmentClientApp_h


#include <QApplication>

const QString ASSIGNMENT_TYPE_OVERRIDE_OPTION = "t";
const QString ASSIGNMENT_POOL_OPTION = "pool";
const QString ASSIGNMENT_CLIENT_LISTEN_PORT_OPTION = "p";
const QString ASSIGNMENT_WALLET_DESTINATION_ID_OPTION = "wallet";
const QString CUSTOM_ASSIGNMENT_SERVER_HOSTNAME_OPTION = "a";
const QString ASSIGNMENT_MONITOR_MIN_CHILDREN_LISTEN_PORT_OPTION = "min-listen-port";
const QString CUSTOM_ASSIGNMENT_SERVER_PORT_OPTION = "server-port";
const QString ASSIGNMENT_NUM_FORKS_OPTION = "n";
const QString ASSIGNMENT_MIN_FORKS_OPTION = "min";
const QString ASSIGNMENT_MAX_FORKS_OPTION = "max";
const QString ASSIGNMENT_CLIENT_MONITOR_PORT_OPTION = "monitor-port";
const QString ASSIGNMENT_HTTP_STATUS_PORT = "http-status-port";
const QString ASSIGNMENT_LOG_DIRECTORY = "log-directory";
const QString ASSIGNMENT_DISABLE_DOMAIN_AUTO_PORT_DISCOVERY = "disable-domain-port-auto-discovery";

class AssignmentClientApp : public QCoreApplication {
    Q_OBJECT
public:
    AssignmentClientApp(int argc, char* argv[]);
};

#endif // hifi_AssignmentClientApp_h
