//
//  MaterialBaker.h
//  libraries/baking/src
//
//  Created by Sam Gondelman on 2/26/2019
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MaterialBaker_h
#define hifi_MaterialBaker_h

#include "Baker.h"

#include "TextureBaker.h"
#include "baking/TextureFileNamer.h"

#include <procedural/ProceduralMaterialCache.h>
#include <ScriptEngine.h>

static const QString BAKED_MATERIAL_EXTENSION = ".baked.json";

using TextureKey = QPair<QUrl, image::TextureUsage::Type>;

class MaterialBaker : public Baker {
    Q_OBJECT
public:
    MaterialBaker(const QString& materialData, bool isURL, const QString& bakedOutputDir, QUrl destinationPath = QUrl());

    QString getMaterialData() const { return _materialData; }
    bool isURL() const { return _isURL; }
    QString getBakedMaterialData() const { return _bakedMaterialData; }

    void setMaterials(const QHash<QString, hfm::Material>& materials, const QString& baseURL);
    void setMaterials(const NetworkMaterialResourcePointer& materialResource);

    NetworkMaterialResourcePointer getNetworkMaterialResource() const { return _materialResource; }

    static void setNextOvenWorkerThreadOperator(std::function<QThread*()> getNextOvenWorkerThreadOperator) { _getNextOvenWorkerThreadOperator = getNextOvenWorkerThreadOperator; }

public slots:
    virtual void bake() override;
    virtual void abort() override;

signals:
    void originalMaterialLoaded();

private slots:
    void processMaterial();
    void outputMaterial();
    void handleFinishedTextureBaker();

private:
    void loadMaterial();

    QString _materialData;
    bool _isURL;
    QUrl _destinationPath;

    NetworkMaterialResourcePointer _materialResource;

    QHash<TextureKey, QSharedPointer<TextureBaker>> _textureBakers;
    QMultiHash<TextureKey, std::shared_ptr<NetworkMaterial>> _materialsNeedingRewrite;

    QString _bakedOutputDir;
    QString _textureOutputDir;
    QString _bakedMaterialData;

    ScriptEnginePointer _scriptEngine;
    static std::function<QThread*()> _getNextOvenWorkerThreadOperator;
    TextureFileNamer _textureFileNamer;

    void addTexture(const QString& materialName, image::TextureUsage::Type textureUsage, const hfm::Texture& texture);
    struct TextureUsageHash {
        std::size_t operator()(image::TextureUsage::Type textureUsage) const {
            return static_cast<std::size_t>(textureUsage);
        }
    };
    std::unordered_map<std::string, std::unordered_map<image::TextureUsage::Type, std::pair<QByteArray, QString>, TextureUsageHash>> _textureContentMap;
};

#endif // !hifi_MaterialBaker_h
