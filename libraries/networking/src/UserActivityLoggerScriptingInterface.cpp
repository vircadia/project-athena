//
//  UserActivityLoggerScriptingInterface.h
//  libraries/networking/src
//
//  Created by Ryan Huffman on 6/06/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "UserActivityLoggerScriptingInterface.h"
#include "UserActivityLogger.h"

void UserActivityLoggerScriptingInterface::enabledEdit() {
    doLogAction("enabled_edit");
}

void UserActivityLoggerScriptingInterface::openedTablet(bool visibleToOthers) {
    doLogAction("opened_tablet", { { "visible_to_others", visibleToOthers } });
}

void UserActivityLoggerScriptingInterface::closedTablet() {
    doLogAction("closed_tablet");
}

void UserActivityLoggerScriptingInterface::toggledAway(bool isAway) {
    doLogAction("toggled_away", { { "is_away", isAway } });
}

void UserActivityLoggerScriptingInterface::tutorialProgress( QString stepName, int stepNumber, float secondsToComplete,
        float tutorialElapsedTime, QString tutorialRunID, int tutorialVersion, QString controllerType) {
    doLogAction("tutorial_progress", {
        { "tutorial_run_id", tutorialRunID },
        { "tutorial_version", tutorialVersion },
        { "step", stepName },
        { "step_number", stepNumber },
        { "seconds_to_complete", secondsToComplete },
        { "tutorial_elapsed_seconds", tutorialElapsedTime },
        { "controller_type", controllerType }
    });

}

void UserActivityLoggerScriptingInterface::palAction(QString action, QString target) {
    QJsonObject payload;
    payload["action"] = action;
    if (target.length() > 0) {
        payload["target"] = target;
    }
    doLogAction("pal_activity", payload);
}

void UserActivityLoggerScriptingInterface::palOpened(float secondsOpened) {
    doLogAction("pal_opened", {
        { "seconds_opened", secondsOpened }
    });
}

void UserActivityLoggerScriptingInterface::makeUserConnection(QString otherID, bool success, QString detailsString) {
    QJsonObject payload;
    payload["otherUser"] = otherID;
    payload["success"] = success;
    if (detailsString.length() > 0) {
        payload["details"] = detailsString;
    }
    doLogAction("makeUserConnection", payload);
}

void UserActivityLoggerScriptingInterface::privacyShieldToggled(bool newValue) {
    doLogAction(newValue ? "privacyShieldOn" : "privacyShieldOff");
}

void UserActivityLoggerScriptingInterface::privacyShieldActivated() {
    doLogAction("privacyShieldActivated");
}

void UserActivityLoggerScriptingInterface::logAction(QString action, QVariantMap details) {
    doLogAction(action, QJsonObject::fromVariantMap(details));
}

void UserActivityLoggerScriptingInterface::doLogAction(QString action, QJsonObject details) {
    QMetaObject::invokeMethod(&UserActivityLogger::getInstance(), "logAction",
                              Q_ARG(QString, action),
                              Q_ARG(QJsonObject, details));
}
