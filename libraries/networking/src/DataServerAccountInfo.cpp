//
//  DataServerAccountInfo.cpp
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2/18/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DataServerAccountInfo.h"

#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QDebug>
#include <QtCore/QDataStream>
#include <QtCore/QCryptographicHash>

#include <UUID.h>

#include "NetworkLogging.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

const QString DataServerAccountInfo::EMPTY_KEY = QString();

DataServerAccountInfo::DataServerAccountInfo(const DataServerAccountInfo& otherInfo) : QObject() {
    _accessToken = otherInfo._accessToken;
    _username = otherInfo._username;
    _xmppPassword = otherInfo._xmppPassword;
    _discourseApiKey = otherInfo._discourseApiKey;
    _privateKey = otherInfo._privateKey;
    _domainID = otherInfo._domainID;
    _temporaryDomainID = otherInfo._temporaryDomainID;
    _temporaryDomainApiKey = otherInfo._temporaryDomainApiKey;
}

DataServerAccountInfo& DataServerAccountInfo::operator=(const DataServerAccountInfo& otherInfo) {
    DataServerAccountInfo temp(otherInfo);
    swap(temp);
    return *this;
}

void DataServerAccountInfo::swap(DataServerAccountInfo& otherInfo) {
    using std::swap;

    swap(_accessToken, otherInfo._accessToken);
    swap(_username, otherInfo._username);
    swap(_xmppPassword, otherInfo._xmppPassword);
    swap(_discourseApiKey, otherInfo._discourseApiKey);
    swap(_privateKey, otherInfo._privateKey);
    swap(_domainID, otherInfo._domainID);
    swap(_temporaryDomainID, otherInfo._temporaryDomainID);
    swap(_temporaryDomainApiKey, otherInfo._temporaryDomainApiKey);
}

void DataServerAccountInfo::setAccessTokenFromJSON(const QJsonObject& jsonObject) {
    _accessToken = OAuthAccessToken(jsonObject);
}

void DataServerAccountInfo::setUsername(const QString& username) {
    if (_username != username) {
        _username = username;
        
        qCDebug(networking) << "Username changed to" << username;
    }
}

void DataServerAccountInfo::setXMPPPassword(const QString& xmppPassword) {
     if (_xmppPassword != xmppPassword) {
         _xmppPassword = xmppPassword;
     }
}

void DataServerAccountInfo::setDiscourseApiKey(const QString& discourseApiKey) {
    if (_discourseApiKey != discourseApiKey) {
        _discourseApiKey = discourseApiKey;
    }
}

bool DataServerAccountInfo::hasProfile() const {
    return _username.length() > 0;
}

void DataServerAccountInfo::setProfileInfoFromJSON(const QJsonObject& jsonObject) {
    QJsonObject user = jsonObject["data"].toObject()["user"].toObject();
    setUsername(user["username"].toString());
    setXMPPPassword(user["xmpp_password"].toString());
    setDiscourseApiKey(user["discourse_api_key"].toString());
}

QByteArray DataServerAccountInfo::getUsernameSignature(const QUuid& connectionToken) {
    auto lowercaseUsername = _username.toLower().toUtf8();
    auto plaintext = lowercaseUsername.append(connectionToken.toRfc4122());

    auto signature = signPlaintext(plaintext);
    if (!signature.isEmpty()) {
        qDebug(networking) << "Returning username" << _username
            << "signed with connection UUID" << uuidStringWithoutCurlyBraces(connectionToken);
    } else {
        qCDebug(networking) << "Error signing username with connection token";
        qCDebug(networking) << "Will re-attempt on next domain-server check in.";
    }

    return signature;
}

QByteArray DataServerAccountInfo::signPlaintext(const QByteArray& plaintext) {
    if (!_privateKey.isEmpty()) {
        const char* privateKeyData = _privateKey.constData();
        RSA* rsaPrivateKey = d2i_RSAPrivateKey(NULL,
                                               reinterpret_cast<const unsigned char**>(&privateKeyData),
                                               _privateKey.size());
        if (rsaPrivateKey) {
            QByteArray signature(RSA_size(rsaPrivateKey), 0);
            unsigned int signatureBytes = 0;

            QByteArray hashedPlaintext = QCryptographicHash::hash(plaintext, QCryptographicHash::Sha256);

            int encryptReturn = RSA_sign(NID_sha256,
                                         reinterpret_cast<const unsigned char*>(hashedPlaintext.constData()),
                                         hashedPlaintext.size(),
                                         reinterpret_cast<unsigned char*>(signature.data()),
                                         &signatureBytes,
                                         rsaPrivateKey);

            // free the private key RSA struct now that we are done with it
            RSA_free(rsaPrivateKey);

            if (encryptReturn != -1) {
                return signature;
            }
        } else {
            qCDebug(networking) << "Could not create RSA struct from QByteArray private key.";
        }
    }
    return QByteArray();
}

QDataStream& operator<<(QDataStream &out, const DataServerAccountInfo& info) {
    out << info._accessToken << info._username << info._xmppPassword << info._discourseApiKey
        << info._privateKey << info._domainID
        << info._temporaryDomainID << info._temporaryDomainApiKey;
    return out;
}

QDataStream& operator>>(QDataStream &in, DataServerAccountInfo& info) {
    in >> info._accessToken >> info._username >> info._xmppPassword >> info._discourseApiKey
        >> info._privateKey >> info._domainID
        >> info._temporaryDomainID >> info._temporaryDomainApiKey;
    return in;
}
