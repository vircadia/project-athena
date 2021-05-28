//
//  EntitiesAuditLogging.cpp
//  libraries/entities/src
//
//  Created by Kalila L on Feb 5 2021.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntitiesAuditLogging.h"

#include <QJsonObject>
#include <QTimer> 

Q_LOGGING_CATEGORY(entities_audit, "vircadia.entities.audit");

EntitiesAuditLogging::~EntitiesAuditLogging() {
    EntitiesAuditLogging::stopAuditLogProcessor();
}

void EntitiesAuditLogging::processAuditLogBuffers() {
    if (!auditLogAddBuffer.isEmpty()) {
        QJsonObject objectToOutput;
        objectToOutput.insert("add", auditLogAddBuffer);
        qCDebug(entities_audit) << objectToOutput;
        auditLogAddBuffer = QJsonObject();
    }
    if (!auditLogEditBuffer.isEmpty()) {
        QJsonObject objectToOutput;
        objectToOutput.insert("edit", auditLogEditBuffer);
        qCDebug(entities_audit) << objectToOutput;
        auditLogEditBuffer = QJsonObject();
    }
}

void EntitiesAuditLogging::startAuditLogProcessor() {
    _auditLogProcessorTimer = new QTimer();
    connect(_auditLogProcessorTimer, &QTimer::timeout, this, &EntitiesAuditLogging::processAuditLogBuffers);
    _auditLogProcessorTimer->start(_auditEditLoggingInterval);
}

void EntitiesAuditLogging::stopAuditLogProcessor() {
    if (_auditLogProcessorTimer) {
        _auditLogProcessorTimer->stop();
        _auditLogProcessorTimer->deleteLater();
        _auditLogProcessorTimer = nullptr;
    }
}

bool EntitiesAuditLogging::isProcessorRunning() {
    if (_auditLogProcessorTimer && _auditLogProcessorTimer != NULL && _auditLogProcessorTimer->isActive()) {
        return true;
    } else {
        return false;
    }
}

void EntitiesAuditLogging::processAddEntityPacket(const QString& sender, const QString& entityID, const QString& entityType) {
    QJsonValue findExisting = auditLogAddBuffer.take(sender);
    if (!findExisting.isUndefined()) {
        QJsonObject existingObject = findExisting.toObject();
        if (!existingObject.contains(entityID)) {
            existingObject.insert(entityID, entityType);
        }
        auditLogAddBuffer.insert(sender, existingObject);
    } else {
        QJsonObject newEntry{ { entityID, entityType } };
        auditLogAddBuffer.insert(sender, newEntry);
    }
}

void EntitiesAuditLogging::processEditEntityPacket(const QString& sender, const QString& entityID) {
    QJsonValue findExisting = auditLogEditBuffer.take(sender);
    if (!findExisting.isUndefined()) {
        QJsonObject existingObject = findExisting.toObject();
        if (!existingObject.contains(entityID)) {
            existingObject.insert(entityID, 1);
        } else {
            existingObject[entityID] = existingObject[entityID].toInt() + 1;
        }
        auditLogEditBuffer.insert(sender, existingObject);
    } else {
        QJsonObject newEntry{ { entityID, 1 } };
        auditLogEditBuffer.insert(sender, newEntry);
    }
}