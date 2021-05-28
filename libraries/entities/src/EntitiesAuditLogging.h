//
//  EntitiesAuditLogging.h
//  libraries/entities/src
//
//  Created by Kalila L on Feb 5 2021.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef vircadia_EntitiesAuditLogging_h
#define vircadia_EntitiesAuditLogging_h

#include <QLoggingCategory>
#include <QJsonObject>
#include <QTimer> 

Q_DECLARE_LOGGING_CATEGORY(entities_audit);

class EntitiesAuditLogging : public QObject {
    Q_OBJECT
public:
    virtual ~EntitiesAuditLogging();

    bool isProcessorRunning();
    void startAuditLogProcessor();
    void stopAuditLogProcessor();
    void setAuditEditLoggingInterval(float interval) { _auditEditLoggingInterval = interval; };
    void processAddEntityPacket(const QString& sender, const QString& entityID, const QString& entityType);
    void processEditEntityPacket(const QString& sender, const QString& entityID);

private:
    void processAuditLogBuffers();

    QJsonObject auditLogAddBuffer;
    QJsonObject auditLogEditBuffer;
    QTimer* _auditLogProcessorTimer { nullptr };
    float _auditEditLoggingInterval;
};

#endif // vircadia_EntitiesAuditLogging_h
