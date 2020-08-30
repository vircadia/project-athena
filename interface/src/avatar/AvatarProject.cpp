//
//  AvatarProject.cpp
//
//
//  Created by Thijs Wenker on 12/7/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AvatarProject.h"

#include <FSTReader.h>

#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>
#include <QTimer>

#include "FBXSerializer.h"
#include <ui/TabletScriptingInterface.h>
#include "scripting/HMDScriptingInterface.h"

AvatarProject* AvatarProject::openAvatarProject(const QString& path, AvatarProjectStatus::AvatarProjectStatus& status) {
    status = AvatarProjectStatus::NONE;

    if (!path.toLower().endsWith(".fst")) {
        status = AvatarProjectStatus::ERROR_OPEN_INVALID_FILE_TYPE;
        return nullptr;
    }

    QFileInfo fstFileInfo{ path };
    if (!fstFileInfo.absoluteDir().exists()) {
        status = AvatarProjectStatus::ERROR_OPEN_PROJECT_FOLDER;
        return nullptr;
    }

    if (!fstFileInfo.exists()) {
        status = AvatarProjectStatus::ERROR_OPEN_FIND_FST;
        return nullptr;
    }

    QFile file{ fstFileInfo.filePath() };
    if (!file.open(QIODevice::ReadOnly)) {
        status = AvatarProjectStatus::ERROR_OPEN_OPEN_FST;
        return nullptr;
    }

    const auto project = new AvatarProject(path, file.readAll());

    QFileInfo fbxFileInfo{ project->getFBXPath() };
    if (!fbxFileInfo.exists()) {
        project->deleteLater();
        status = AvatarProjectStatus::ERROR_OPEN_FIND_MODEL;
        return nullptr;
    }

    QQmlEngine::setObjectOwnership(project, QQmlEngine::CppOwnership);
    status = AvatarProjectStatus::SUCCESS;
    return project;
}

AvatarProject* AvatarProject::createAvatarProject(const QString& projectsFolder, const QString& avatarProjectName,
                                                  const QString& avatarModelPath, const QString& textureFolder,
                                                  AvatarProjectStatus::AvatarProjectStatus& status) {
    status = AvatarProjectStatus::NONE;

    if (!isValidNewProjectName(projectsFolder, avatarProjectName)) {
        status = AvatarProjectStatus::ERROR_CREATE_PROJECT_NAME;
        return nullptr;
    }

    QDir projectDir(projectsFolder + "/" + avatarProjectName);
    if (!projectDir.mkpath(".")) {
        status = AvatarProjectStatus::ERROR_CREATE_CREATING_DIRECTORIES;
        return nullptr;
    }

    QDir projectTexturesDir(projectDir.path() + "/textures");
    if (!projectTexturesDir.mkpath(".")) {
        status = AvatarProjectStatus::ERROR_CREATE_CREATING_DIRECTORIES;
        return nullptr;
    }

    QDir projectScriptsDir(projectDir.path() + "/scripts");
    if (!projectScriptsDir.mkpath(".")) {
        status = AvatarProjectStatus::ERROR_CREATE_CREATING_DIRECTORIES;
        return nullptr;
    }

    const auto fileName = QFileInfo(avatarModelPath).fileName();
    const auto newModelPath = projectDir.absoluteFilePath(fileName);
    const auto newFSTPath = projectDir.absoluteFilePath("avatar.fst");
    QFile::copy(avatarModelPath, newModelPath);

    QFileInfo fbxInfo{ newModelPath };
    if (!fbxInfo.exists() || !fbxInfo.isFile()) {
        status = AvatarProjectStatus::ERROR_CREATE_FIND_MODEL;
        return nullptr;
    }

    QFile fbx{ fbxInfo.filePath() };
    if (!fbx.open(QIODevice::ReadOnly)) {
        status = AvatarProjectStatus::ERROR_CREATE_OPEN_MODEL;
        return nullptr;
    }

    std::shared_ptr<hfm::Model> hfmModel;

    try {
        const QByteArray fbxContents = fbx.readAll();
        hfmModel = FBXSerializer().read(fbxContents, QVariantHash(), fbxInfo.filePath());
    } catch (const QString& error) {
        Q_UNUSED(error)
        status = AvatarProjectStatus::ERROR_CREATE_READ_MODEL;
        return nullptr;
    }
    QStringList textures{};

    auto addTextureToList = [&textures](hfm::Texture texture) mutable {
        if (!texture.filename.isEmpty() && texture.content.isEmpty() && !textures.contains(texture.filename)) {
            textures << texture.filename;
        }
    };

    foreach(const HFMMaterial material, hfmModel->materials) {
        addTextureToList(material.normalTexture);
        addTextureToList(material.albedoTexture);
        addTextureToList(material.opacityTexture);
        addTextureToList(material.glossTexture);
        addTextureToList(material.roughnessTexture);
        addTextureToList(material.specularTexture);
        addTextureToList(material.metallicTexture);
        addTextureToList(material.emissiveTexture);
        addTextureToList(material.occlusionTexture);
        addTextureToList(material.scatteringTexture);
        addTextureToList(material.lightmapTexture);
    }

    QDir textureDir(textureFolder.isEmpty() ? fbxInfo.absoluteDir() : textureFolder);

    for (const auto& texture : textures) {
        QString sourcePath = textureDir.path() + "/" + texture;
        QString targetPath = projectTexturesDir.path() + "/" + texture;

        QFileInfo sourceTexturePath(sourcePath);
        if (sourceTexturePath.exists()) {
            QFile::copy(sourcePath, targetPath);
        }
    }

    auto fst = FST::createFSTFromModel(newFSTPath, newModelPath, *hfmModel);
    fst->setName(avatarProjectName);

    if (!fst->write()) {
        status = AvatarProjectStatus::ERROR_CREATE_WRITE_FST;
        return nullptr;
    }

    status = AvatarProjectStatus::SUCCESS;
    return new AvatarProject(fst);
}

QStringList AvatarProject::getScriptPaths(const QDir& scriptsDir) const {
    QStringList result{};
    constexpr auto flags = QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden;
    if (!scriptsDir.exists()) {
        return result;
    }

    for (const auto& script : scriptsDir.entryInfoList({}, flags)) {
        if (script.fileName().toLower().endsWith(".js")) {
            result.push_back("scripts/" + script.fileName());
        }
    }

    return result;
}

bool AvatarProject::isValidNewProjectName(const QString& projectPath, const QString& projectName) {
    if (projectPath.trimmed().isEmpty() || projectName.trimmed().isEmpty()) {
        return false;
    }
    QDir dir(projectPath + "/" + projectName);
    return !dir.exists();
}

AvatarProject::AvatarProject(const QString& fstPath, const QByteArray& data) :
    AvatarProject::AvatarProject(new FST(fstPath, FSTReader::readMapping(data))) {
}
AvatarProject::AvatarProject(FST* fst) {
    _fst = fst;
    auto fileInfo = QFileInfo(getFSTPath());
    _directory = fileInfo.absoluteDir();

    _fst->setScriptPaths(getScriptPaths(QDir(_directory.path() + "/scripts")));
    _fst->write();

    refreshProjectFiles();

    _projectPath = fileInfo.absoluteDir().absolutePath();
}

void AvatarProject::appendDirectory(const QString& prefix, const QDir& dir) {
    constexpr auto flags = QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden;
    for (auto& entry : dir.entryInfoList({}, flags)) {
        if (entry.isFile()) {
            _projectFiles.append({ entry.absoluteFilePath(), prefix + entry.fileName() });
        } else if (entry.isDir()) {
            appendDirectory(prefix + entry.fileName() + "/", entry.absoluteFilePath());
        }
    }
}

void AvatarProject::refreshProjectFiles() {
    _projectFiles.clear();
    appendDirectory("", _directory);
}

QStringList AvatarProject::getProjectFiles() const {
    QStringList paths;
    for (auto& path : _projectFiles) {
        paths.append(path.relativePath); 
    }
    return paths;
}

AvatarDoctor* AvatarProject::diagnose() {
    return new AvatarDoctor(QUrl(getFSTPath()));
}
