//
//  MaterialBaker.cpp
//  libraries/baking/src
//
//  Created by Sam Gondelman on 2/26/2019
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MaterialBaker.h"

#include <unordered_map>

#include "QJsonObject"
#include "QJsonDocument"

#include "MaterialBakingLoggingCategory.h"

#include <SharedUtil.h>
#include <PathUtils.h>
#include <ScriptEngine.h>
#include <ScriptValue.h>

#include <graphics-scripting/GraphicsScriptingInterface.h>

std::function<QThread*()> MaterialBaker::_getNextOvenWorkerThreadOperator;

static int materialNum = 0;

MaterialBaker::MaterialBaker(const QString& materialData, bool isURL, const QString& bakedOutputDir, QUrl destinationPath) :
    _scriptEngine(newScriptEngine()),
    _materialData(materialData),
    _isURL(isURL),
    _destinationPath(destinationPath),
    _bakedOutputDir(bakedOutputDir),
    _textureOutputDir(bakedOutputDir + "/materialTextures/" + QString::number(materialNum++))
{
}

void MaterialBaker::bake() {
    qDebug(material_baking) << "Material Baker" << _materialData << "bake starting";

    // once our script is loaded, kick off a the processing
    connect(this, &MaterialBaker::originalMaterialLoaded, this, &MaterialBaker::processMaterial);

    if (!_materialResource) {
        // first load the material (either locally or remotely)
        loadMaterial();
    } else {
        // we already have a material passed to us, use that
        if (_materialResource->isLoaded()) {
            processMaterial();
        } else {
            connect(_materialResource.data(), &Resource::finished, this, &MaterialBaker::originalMaterialLoaded);
        }
    }
}

void MaterialBaker::abort() {
    Baker::abort();

    for (auto& textureBaker : _textureBakers) {
        textureBaker->abort();
    }
}

void MaterialBaker::loadMaterial() {
    if (!_isURL) {
        qCDebug(material_baking) << "Loading local material" << _materialData;

        _materialResource = NetworkMaterialResourcePointer(new NetworkMaterialResource());
        // TODO: add baseURL to allow these to reference relative files next to them
        _materialResource->parsedMaterials = NetworkMaterialResource::parseJSONMaterials(QJsonDocument::fromJson(_materialData.toUtf8()), QUrl());
    } else {
        qCDebug(material_baking) << "Downloading material" << _materialData;
        _materialResource = DependencyManager::get<MaterialCache>()->getMaterial(_materialData);
    }

    if (_materialResource) {
        if (_materialResource->isLoaded()) {
            emit originalMaterialLoaded();
        } else {
            connect(_materialResource.data(), &Resource::finished, this, &MaterialBaker::originalMaterialLoaded);
        }
    } else {
        handleError("Error loading " + _materialData);
    }
}

void MaterialBaker::processMaterial() {
    if (!_materialResource || _materialResource->parsedMaterials.networkMaterials.size() == 0) {
        handleError("Error processing " + _materialData);
        return;
    }

    if (QDir(_textureOutputDir).exists()) {
        qWarning() << "Output path" << _textureOutputDir << "already exists. Continuing.";
    } else {
        qCDebug(material_baking) << "Creating materialTextures output folder" << _textureOutputDir;
        if (!QDir().mkpath(_textureOutputDir)) {
            handleError("Failed to create materialTextures output folder " + _textureOutputDir);
        }
    }

    for (auto networkMaterial : _materialResource->parsedMaterials.networkMaterials) {
        if (networkMaterial.second) {
            auto textures = networkMaterial.second->getTextures();
            for (auto texturePair : textures) {
                auto mapChannel = texturePair.first;
                auto textureMap = texturePair.second;
                if (textureMap.texture && textureMap.texture->_textureSource) {
                    auto type = textureMap.texture->getTextureType();

                    QByteArray content;
                    QUrl textureURL;
                    {
                        bool foundEmbeddedTexture = false;
                        auto textureContentMapIter = _textureContentMap.find(networkMaterial.second->getName());
                        if (textureContentMapIter != _textureContentMap.end()) {
                            auto textureUsageIter = textureContentMapIter->second.find(type);
                            if (textureUsageIter != textureContentMapIter->second.end()) {
                                content = textureUsageIter->second.first;
                                textureURL = textureUsageIter->second.second;
                                foundEmbeddedTexture = true;
                            }
                        }
                        if (!foundEmbeddedTexture && textureMap.texture->_textureSource) {
                            textureURL = textureMap.texture->_textureSource->getUrl().adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment);
                        }
                    }

                    QString cleanURL = textureURL.toDisplayString();
                    auto idx = cleanURL.lastIndexOf('.');
                    QString extension = idx >= 0 ? cleanURL.mid(idx + 1).toLower() : "";

                    if (QImageReader::supportedImageFormats().contains(extension.toLatin1())) {
                        TextureKey textureKey(textureURL, type);
                        if (!_textureBakers.contains(textureKey)) {
                            auto baseTextureFileName = _textureFileNamer.createBaseTextureFileName(textureURL.fileName(), type);

                            QSharedPointer<TextureBaker> textureBaker {
                                new TextureBaker(textureURL, type, _textureOutputDir, baseTextureFileName, content),
                                &TextureBaker::deleteLater
                            };
                            textureBaker->setMapChannel(mapChannel);
                            connect(textureBaker.data(), &TextureBaker::finished, this, &MaterialBaker::handleFinishedTextureBaker);
                            _textureBakers.insert(textureKey, textureBaker);
                            textureBaker->moveToThread(_getNextOvenWorkerThreadOperator ? _getNextOvenWorkerThreadOperator() : thread());
                            // By default, Qt will invoke this bake immediately if the TextureBaker is on the same worker thread as this MaterialBaker.
                            // We don't want that, because threads may be waiting for work while this thread is stuck processing a TextureBaker.
                            // On top of that, _textureBakers isn't fully populated.
                            // So, use Qt::QueuedConnection.
                            // TODO: Better thread utilization at the top level, not just the MaterialBaker level
                            QMetaObject::invokeMethod(textureBaker.data(), "bake", Qt::QueuedConnection);
                        }
                        _materialsNeedingRewrite.insert(textureKey, networkMaterial.second);
                    } else {
                        qCDebug(material_baking) << "Texture extension not supported: " << extension;
                    }
                }
            }
        }
    }

    if (_textureBakers.empty()) {
        outputMaterial();
    }
}

void MaterialBaker::handleFinishedTextureBaker() {
    auto baker = qobject_cast<TextureBaker*>(sender());

    if (baker) {
        TextureKey textureKey = { baker->getTextureURL(), baker->getTextureType() };
        if (!baker->hasErrors()) {
            // this TextureBaker is done and everything went according to plan
            qCDebug(material_baking) << "Re-writing texture references to" << baker->getTextureURL();

            auto newURL = QUrl(_textureOutputDir).resolved(baker->getMetaTextureFileName());
            auto relativeURL = QDir(_bakedOutputDir).relativeFilePath(newURL.toString());

            if (!_destinationPath.isEmpty()) {
                relativeURL = _destinationPath.resolved(relativeURL).toDisplayString();
            }

            // Replace the old texture URLs
            for (auto networkMaterial : _materialsNeedingRewrite.values(textureKey)) {
                networkMaterial->getTextureMap(baker->getMapChannel())->getTextureSource()->setUrl(relativeURL);
            }
        } else {
            // this texture failed to bake - this doesn't fail the entire bake but we need to add the errors from
            // the texture to our warnings
            _warningList << baker->getWarnings();
        }

        _materialsNeedingRewrite.remove(textureKey);
        _textureBakers.remove(textureKey);

        if (_textureBakers.empty()) {
            outputMaterial();
        }
    } else {
        handleWarning("Unidentified baker finished and signaled to material baker to handle texture. Material: " + _materialData);
    }
}

void MaterialBaker::outputMaterial() {
    if (_materialResource) {
        QJsonObject json;
        if (_materialResource->parsedMaterials.networkMaterials.size() == 1) {
            auto networkMaterial = _materialResource->parsedMaterials.networkMaterials.begin();
            auto scriptableMaterial = scriptable::ScriptableMaterial(networkMaterial->second);
            QVariant materialVariant = scriptable::scriptableMaterialToScriptValue(_scriptEngine.data(), scriptableMaterial)->toVariant();
            json.insert("materials", QJsonDocument::fromVariant(materialVariant).object());
        } else {
            QJsonArray materialArray;
            for (auto networkMaterial : _materialResource->parsedMaterials.networkMaterials) {
                auto scriptableMaterial = scriptable::ScriptableMaterial(networkMaterial.second);
                QVariant materialVariant = scriptable::scriptableMaterialToScriptValue(_scriptEngine.data(), scriptableMaterial)->toVariant();
                materialArray.append(QJsonDocument::fromVariant(materialVariant).object());
            }
            json.insert("materials", materialArray);
        }

        QByteArray outputMaterial = QJsonDocument(json).toJson(QJsonDocument::Compact);
        if (_isURL) {
            auto fileName = QUrl(_materialData).fileName();
            auto baseName = fileName.left(fileName.lastIndexOf('.'));
            auto bakedFilename = baseName + BAKED_MATERIAL_EXTENSION;

            _bakedMaterialData = _bakedOutputDir + "/" + bakedFilename;

            QFile bakedFile;
            bakedFile.setFileName(_bakedMaterialData);
            if (!bakedFile.open(QIODevice::WriteOnly)) {
                handleError("Error opening " + _bakedMaterialData + " for writing");
                return;
            }

            bakedFile.write(outputMaterial);

            // Export successful
            _outputFiles.push_back(_bakedMaterialData);
            qCDebug(material_baking) << "Exported" << _materialData << "to" << _bakedMaterialData;
        } else {
            _bakedMaterialData = QString(outputMaterial);
            qCDebug(material_baking) << "Converted" << _materialData << "to" << _bakedMaterialData;
        }
    }

    // emit signal to indicate the material baking is finished
    emit finished();
}

void MaterialBaker::addTexture(const QString& materialName, image::TextureUsage::Type textureUsage, const hfm::Texture& texture) {
    auto& textureUsageMap = _textureContentMap[materialName.toStdString()];
    if (textureUsageMap.find(textureUsage) == textureUsageMap.end() && !texture.content.isEmpty()) {
        textureUsageMap[textureUsage] = { texture.content, texture.filename };
    }
};

void MaterialBaker::setMaterials(const QHash<QString, hfm::Material>& materials, const QString& baseURL) {
    _materialResource = NetworkMaterialResourcePointer(new NetworkMaterialResource(), [](NetworkMaterialResource* ptr) { ptr->deleteLater(); });
    for (auto& material : materials) {
        _materialResource->parsedMaterials.names.push_back(material.name.toStdString());
        _materialResource->parsedMaterials.networkMaterials[material.name.toStdString()] = std::make_shared<NetworkMaterial>(material, baseURL);

        // Store any embedded texture content
        addTexture(material.name, image::TextureUsage::NORMAL_TEXTURE, material.normalTexture);
        addTexture(material.name, image::TextureUsage::ALBEDO_TEXTURE, material.albedoTexture);
        addTexture(material.name, image::TextureUsage::GLOSS_TEXTURE, material.glossTexture);
        addTexture(material.name, image::TextureUsage::ROUGHNESS_TEXTURE, material.roughnessTexture);
        addTexture(material.name, image::TextureUsage::SPECULAR_TEXTURE, material.specularTexture);
        addTexture(material.name, image::TextureUsage::METALLIC_TEXTURE, material.metallicTexture);
        addTexture(material.name, image::TextureUsage::EMISSIVE_TEXTURE, material.emissiveTexture);
        addTexture(material.name, image::TextureUsage::OCCLUSION_TEXTURE, material.occlusionTexture);
        addTexture(material.name, image::TextureUsage::SCATTERING_TEXTURE, material.scatteringTexture);
        addTexture(material.name, image::TextureUsage::LIGHTMAP_TEXTURE, material.lightmapTexture);
    }
}

void MaterialBaker::setMaterials(const NetworkMaterialResourcePointer& materialResource) {
    _materialResource = materialResource;
}
