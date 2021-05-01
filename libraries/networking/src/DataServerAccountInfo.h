//
//  DataServerAccountInfo.h
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2/21/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DataServerAccountInfo_h
#define hifi_DataServerAccountInfo_h

#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtNetwork/qnetworkreply.h>

#include "OAuthAccessToken.h"

const float SATOSHIS_PER_CREDIT = 100000000.0f;

class DataServerAccountInfo : public QObject {
    Q_OBJECT
    const static QString EMPTY_KEY;
public:
    DataServerAccountInfo() {};
    DataServerAccountInfo(const DataServerAccountInfo& otherInfo);
    DataServerAccountInfo& operator=(const DataServerAccountInfo& otherInfo);

    const OAuthAccessToken& getAccessToken() const { return _accessToken; }
    void setAccessToken(const OAuthAccessToken& accessToken) { _accessToken = accessToken; }
    void setAccessTokenFromJSON(const QJsonObject& jsonObject);

    const QString& getUsername() const { return _username; }
    void setUsername(const QString& username);

    const QString& getXMPPPassword() const { return _xmppPassword; }
    void setXMPPPassword(const QString& xmppPassword);

    const QString& getDiscourseApiKey() const { return _discourseApiKey; }
    void setDiscourseApiKey(const QString& discourseApiKey);
    
    QByteArray getUsernameSignature(const QUuid& connectionToken);
    bool hasPrivateKey() const { return !_privateKey.isEmpty(); }
    void setPrivateKey(const QByteArray& privateKey) { _privateKey = privateKey; }

    QByteArray signPlaintext(const QByteArray& plaintext);

    void setDomainID(const QUuid& domainID) { _domainID = domainID; }
    const QUuid& getDomainID() const { return _domainID; }

    void setTemporaryDomain(const QUuid& domainID, const QString& key) { _temporaryDomainID = domainID; _temporaryDomainApiKey = key; }
    const QString& getTemporaryDomainKey(const QUuid& domainID) { return domainID == _temporaryDomainID ? _temporaryDomainApiKey : EMPTY_KEY; }

    bool hasProfile() const;

    void setProfileInfoFromJSON(const QJsonObject& jsonObject);

    friend QDataStream& operator<<(QDataStream &out, const DataServerAccountInfo& info);
    friend QDataStream& operator>>(QDataStream &in, DataServerAccountInfo& info);

private:
    void swap(DataServerAccountInfo& otherInfo);

    OAuthAccessToken _accessToken;
    QString _username;
    QString _xmppPassword;
    QString _discourseApiKey;
    QUuid _domainID;
    QUuid _temporaryDomainID;
    QString _temporaryDomainApiKey;
    QByteArray _privateKey;

};

#endif // hifi_DataServerAccountInfo_h
