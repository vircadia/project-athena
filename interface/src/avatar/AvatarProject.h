//
//  AvatarProject.h
//
//
//  Created by Thijs Wenker on 12/7/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_AvatarProject_h
#define hifi_AvatarProject_h

#include "AvatarDoctor.h"
#include "ProjectFile.h"
#include "FST.h"

#include <QObject>
#include <QDir>
#include <QVariantHash>
#include <QStandardPaths>

namespace AvatarProjectStatus {
    Q_NAMESPACE
    enum AvatarProjectStatus {
        NONE,
        SUCCESS,
        ERROR_CREATE_PROJECT_NAME,
        ERROR_CREATE_CREATING_DIRECTORIES,
        ERROR_CREATE_FIND_MODEL,
        ERROR_CREATE_OPEN_MODEL,
        ERROR_CREATE_READ_MODEL,
        ERROR_CREATE_WRITE_FST,
        ERROR_OPEN_INVALID_FILE_TYPE,
        ERROR_OPEN_PROJECT_FOLDER,
        ERROR_OPEN_FIND_FST,
        ERROR_OPEN_OPEN_FST,
        ERROR_OPEN_FIND_MODEL
    };
    Q_ENUM_NS(AvatarProjectStatus)
}


class AvatarProject : public QObject {
    Q_OBJECT
    Q_PROPERTY(FST* fst READ getFST CONSTANT)

    Q_PROPERTY(QStringList projectFiles READ getProjectFiles NOTIFY projectFilesChanged)

    Q_PROPERTY(QString projectFolderPath READ getProjectPath CONSTANT)
    Q_PROPERTY(QString projectFSTPath READ getFSTPath CONSTANT)
    Q_PROPERTY(QString projectFBXPath READ getFBXPath CONSTANT)
    Q_PROPERTY(QString name READ getProjectName WRITE setProjectName NOTIFY nameChanged)
    Q_PROPERTY(bool hasErrors READ getHasErrors WRITE setHasErrors NOTIFY hasErrorsChanged)

public:
    Q_INVOKABLE QStringList getProjectFiles() const;
    Q_INVOKABLE AvatarDoctor* diagnose();


    Q_INVOKABLE QString getProjectName() const { return _fst->getName(); }
    Q_INVOKABLE void setProjectName(const QString& newProjectName) {
        if (newProjectName.trimmed().length() > 0) {
            _fst->setName(newProjectName);
            _fst->write();
            emit nameChanged();
        }
    }
    Q_INVOKABLE QString getProjectPath() const { return _projectPath; }
    Q_INVOKABLE QString getFSTPath() const { return _fst->getPath(); }
    Q_INVOKABLE QString getFBXPath() const {
        return QDir::cleanPath(QDir(_projectPath).absoluteFilePath(_fst->getModelPath()));
    }
    Q_INVOKABLE bool getHasErrors() const { return _hasErrors; }
    Q_INVOKABLE void setHasErrors(bool hasErrors) {
        _hasErrors = hasErrors;
        emit hasErrorsChanged();
    }

    /**
     * returns the AvatarProject or a nullptr on failure.
     */
    static AvatarProject* openAvatarProject(const QString& path, AvatarProjectStatus::AvatarProjectStatus& status);
    static AvatarProject* createAvatarProject(const QString& projectsFolder,
                                              const QString& avatarProjectName,
                                              const QString& avatarModelPath,
                                              const QString& textureFolder,
                                              AvatarProjectStatus::AvatarProjectStatus& status);

    static bool isValidNewProjectName(const QString& projectPath, const QString& projectName);

    static QString getDefaultProjectsPath() {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Vircadia Projects";
    }

signals:
    void nameChanged();
    void projectFilesChanged();
    void hasErrorsChanged();

private:
    AvatarProject(const QString& fstPath, const QByteArray& data);
    AvatarProject(FST* fst);

    ~AvatarProject() { _fst->deleteLater(); }

    FST* getFST() { return _fst; }

    void refreshProjectFiles();
    void appendDirectory(const QString& prefix, const QDir& dir);
    QStringList getScriptPaths(const QDir& scriptsDir) const;

    FST* _fst;

    QDir _directory;
    QList<ProjectFilePath> _projectFiles{};
    QString _projectPath;

    bool _hasErrors { false };
};

#endif  // hifi_AvatarProject_h
