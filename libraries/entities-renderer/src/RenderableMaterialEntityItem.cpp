//
//  Created by Sam Gondelman on 1/18/2018
//  Copyright 2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableMaterialEntityItem.h"

#include "RenderPipelines.h"
#include "GeometryCache.h"

#include <procedural/Procedural.h>

using namespace render;
using namespace render::entities;

void MaterialEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this, entity] {
        withWriteLock([&] {
            _renderTransform = getModelTransform();
            // Material entities should fit inside a cube entity of the same size, so a sphere that has dimensions 1x1x1
            // is a half unit sphere.  However, the geometry cache renders a UNIT sphere, so we need to scale down.
            const float MATERIAL_ENTITY_SCALE = 0.5f;
            _renderTransform.postScale(MATERIAL_ENTITY_SCALE);
            _renderTransform.postScale(ENTITY_ITEM_DEFAULT_DIMENSIONS);
        });
    });
}

void MaterialEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    bool deleteNeeded = false;
    bool addNeeded = _retryApply;
    bool transformChanged = false;
    {
        MaterialMappingMode mode = entity->getMaterialMappingMode();
        if (mode != _materialMappingMode) {
            _materialMappingMode = mode;
            transformChanged = true;
        }
    }
    {
        bool repeat = entity->getMaterialRepeat();
        if (repeat != _materialRepeat) {
            _materialRepeat = repeat;
            transformChanged = true;
        }
    }
    {
        glm::vec2 mappingPos = entity->getMaterialMappingPos();
        glm::vec2 mappingScale = entity->getMaterialMappingScale();
        float mappingRot = entity->getMaterialMappingRot();
        if (mappingPos != _materialMappingPos || mappingScale != _materialMappingScale || mappingRot != _materialMappingRot) {
            _materialMappingPos = mappingPos;
            _materialMappingScale = mappingScale;
            _materialMappingRot = mappingRot;
            transformChanged |= _materialMappingMode == MaterialMappingMode::UV;
        }
    }
    {
        Transform transform = entity->getTransform();
        glm::vec3 dimensions = entity->getUnscaledDimensions();
        if (transform != _transform || dimensions != _dimensions) {
            _transform = transform;
            _dimensions = dimensions;
            transformChanged |= _materialMappingMode == MaterialMappingMode::PROJECTED;
        }
    }

    {
        auto material = getMaterial();
        // Update the old material regardless of if it's going to change
        if (transformChanged && material && !_parentID.isNull()) {
            deleteNeeded = true;
            addNeeded = true;
            applyTextureTransform(material);
        }
    }

    bool urlChanged = false;
    std::string newCurrentMaterialName = _currentMaterialName;
    {
        QString materialURL = entity->getMaterialURL();
        if (materialURL != _materialURL) {
            _materialURL = materialURL;
            if (_materialURL.contains("#")) {
                auto split = _materialURL.split("#");
                newCurrentMaterialName = split.last().toStdString();
            } else if (_materialURL.contains("?")) {
                qDebug() << "DEPRECATED: Use # instead of ? for material URLS:" << _materialURL;
                auto split = _materialURL.split("?");
                newCurrentMaterialName = split.last().toStdString();
            }
            urlChanged = true;
        }
    }

    bool usingMaterialData = _materialURL.startsWith("materialData");
    bool materialDataChanged = false;
    QUuid oldParentID = _parentID;
    QString oldParentMaterialName = _parentMaterialName;
    {
        QString materialData = entity->getMaterialData();
        if (materialData != _materialData) {
            _materialData = materialData;
            if (usingMaterialData) {
                materialDataChanged = true;
            }
        }
    }
    {
        QString parentMaterialName = entity->getParentMaterialName();
        if (parentMaterialName != _parentMaterialName) {
            _parentMaterialName = parentMaterialName;
            deleteNeeded = true;
            addNeeded = true;
        }
    }
    {
        QUuid parentID = entity->getParentID();
        if (parentID != _parentID) {
            _parentID = parentID;
            deleteNeeded = true;
            addNeeded = true;
        }
    }
    {
        quint16 priority = entity->getPriority();
        if (priority != _priority) {
            _priority = priority;
            deleteNeeded = true;
            addNeeded = true;
        }
    }

    if (urlChanged && !usingMaterialData) {
        _networkMaterial = DependencyManager::get<MaterialCache>()->getMaterial(_materialURL);
        auto onMaterialRequestFinished = [this, entity, oldParentID, oldParentMaterialName, newCurrentMaterialName](bool success) {
            deleteMaterial(oldParentID, oldParentMaterialName);
            if (success) {
                _texturesLoaded = false;
                _parsedMaterials = _networkMaterial->parsedMaterials;
                setCurrentMaterialName(newCurrentMaterialName);
                applyMaterial(entity);
                emit requestRenderUpdate();
            } else {
                _retryApply = false;
                _texturesLoaded = true;
            }
        };
        if (_networkMaterial) {
            if (_networkMaterial->isLoaded()) {
                onMaterialRequestFinished(!_networkMaterial->isFailed());
            } else {
                connect(_networkMaterial.data(), &Resource::finished, this, [this, onMaterialRequestFinished](bool success) {
                    onMaterialRequestFinished(success);
                });
            }
        }
    } else if (materialDataChanged && usingMaterialData) {
        deleteMaterial(oldParentID, oldParentMaterialName);
        _texturesLoaded = false;
        _parsedMaterials = NetworkMaterialResource::parseJSONMaterials(QJsonDocument::fromJson(_materialData.toUtf8()), _materialURL);
        // Since our material changed, the current name might not be valid anymore, so we need to update
        setCurrentMaterialName(newCurrentMaterialName);
        applyMaterial(entity);
    } else {
        if (deleteNeeded) {
            deleteMaterial(oldParentID, oldParentMaterialName);
        }
        if (addNeeded) {
            applyMaterial(entity);
        }
    }

    {
        auto material = getMaterial();
        bool newTexturesLoaded = material ? !material->isMissingTexture() : false;
        if (!_texturesLoaded && newTexturesLoaded) {
            material->checkResetOpacityMap();
        }
        _texturesLoaded = newTexturesLoaded;
    }

    if (!_texturesLoaded || _retryApply) {
        emit requestRenderUpdate();
    }
}

ItemKey MaterialEntityRenderer::getKey() {
    auto builder = ItemKey::Builder().withTypeShape().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (!_visible) {
        builder.withInvisible();
    }

    const auto drawMaterial = getMaterial();
    if (drawMaterial) {
        auto matKey = drawMaterial->getKey();
        if (matKey.isTranslucent()) {
            builder.withTransparent();
        }
    }

    return builder.build();
}

ShapeKey MaterialEntityRenderer::getShapeKey() {
    ShapeKey::Builder builder;
    graphics::MaterialKey drawMaterialKey;
    const auto drawMaterial = getMaterial();
    if (drawMaterial) {
        drawMaterialKey = drawMaterial->getKey();
    }

    if (drawMaterialKey.isTranslucent()) {
        builder.withTranslucent();
    }

    if (drawMaterial && drawMaterial->isProcedural() && drawMaterial->isReady()) {
        builder.withOwnPipeline();
    } else {
        builder.withMaterial();

        if (drawMaterialKey.isNormalMap()) {
            builder.withTangents();
        }
        if (drawMaterialKey.isLightMap()) {
            builder.withLightMap();
        }
        if (drawMaterialKey.isUnlit()) {
            builder.withUnlit();
        }
    }

    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }

    return builder.build();
}

void MaterialEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableMaterialEntityItem::render");
    Q_ASSERT(args->_batch);
    gpu::Batch& batch = *args->_batch;

    // Don't render if our parent is set or our material is null
    if (!_parentID.isNull()) {
        return;
    }

    graphics::MaterialPointer drawMaterial = getMaterial();
    bool proceduralRender = false;
    Transform textureTransform;
    textureTransform.setTranslation(glm::vec3(_materialMappingPos, 0));
    textureTransform.setRotation(glm::vec3(0, 0, glm::radians(_materialMappingRot)));
    textureTransform.setScale(glm::vec3(_materialMappingScale, 1));

    Transform transform;
    withReadLock([&] {
        transform = _renderTransform;
    });

    if (!drawMaterial) {
        return;
    }

    if (drawMaterial->isProcedural() && drawMaterial->isReady()) {
        proceduralRender = true;
    }

    transform.setRotation(BillboardModeHelpers::getBillboardRotation(transform.getTranslation(), transform.getRotation(), _billboardMode,
        args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE ? BillboardModeHelpers::getPrimaryViewFrustumPosition() : args->getViewFrustum().getPosition()));
    batch.setModelTransform(transform, _prevRenderTransform);
    if (args->_renderMode == Args::RenderMode::DEFAULT_RENDER_MODE || args->_renderMode == Args::RenderMode::MIRROR_RENDER_MODE) {
        _prevRenderTransform = transform;
    }

    if (!proceduralRender) {
        drawMaterial->setTextureTransforms(textureTransform, MaterialMappingMode::UV, true);
        // bind the material
        if (RenderPipelines::bindMaterial(drawMaterial, batch, args->_renderMode, args->_enableTexturing)) {
            args->_details._materialSwitches++;
        }

        // Draw!
        DependencyManager::get<GeometryCache>()->renderSphere(batch);
    } else {
        auto proceduralDrawMaterial = std::static_pointer_cast<graphics::ProceduralMaterial>(drawMaterial);
        glm::vec4 outColor = glm::vec4(drawMaterial->getAlbedo(), drawMaterial->getOpacity());
        outColor = proceduralDrawMaterial->getColor(outColor);
        proceduralDrawMaterial->prepare(batch, transform.getTranslation(), transform.getScale(),
                                        transform.getRotation(), _created, ProceduralProgramKey(outColor.a < 1.0f));
        if (render::ShapeKey(args->_globalShapeKey).isWireframe() || _primitiveMode == PrimitiveMode::LINES) {
            DependencyManager::get<GeometryCache>()->renderWireSphere(batch, outColor);
        } else {
            DependencyManager::get<GeometryCache>()->renderSphere(batch, outColor);
        }
    }

    args->_details._trianglesRendered += (int)DependencyManager::get<GeometryCache>()->getSphereTriangleCount();
}

void MaterialEntityRenderer::setCurrentMaterialName(const std::string& currentMaterialName) {
    if (_parsedMaterials.networkMaterials.find(currentMaterialName) != _parsedMaterials.networkMaterials.end()) {
        _currentMaterialName = currentMaterialName;
    } else if (_parsedMaterials.names.size() > 0) {
        _currentMaterialName = _parsedMaterials.names[0];
    }
}

std::shared_ptr<NetworkMaterial> MaterialEntityRenderer::getMaterial() const {
    auto material = _parsedMaterials.networkMaterials.find(_currentMaterialName);
    if (material != _parsedMaterials.networkMaterials.end()) {
        return material->second;
    } else {
        return nullptr;
    }
}

void MaterialEntityRenderer::deleteMaterial(const QUuid& oldParentID, const QString& oldParentMaterialName) {
    std::shared_ptr<NetworkMaterial> material = _appliedMaterial;
    if (!material || oldParentID.isNull()) {
        return;
    }

    // Our parent could be an entity or an avatar
    std::string oldParentMaterialNameStd = oldParentMaterialName.toStdString();
    if (EntityTreeRenderer::removeMaterialFromEntity(oldParentID, material, oldParentMaterialNameStd)) {
        _appliedMaterial = nullptr;
        return;
    }

    if (EntityTreeRenderer::removeMaterialFromAvatar(oldParentID, material, oldParentMaterialNameStd)) {
        _appliedMaterial = nullptr;
        return;
    }

    // if a remove fails, our parent is gone, so we don't need to retry, EXCEPT:
    // MyAvatar can change UUIDs when you switch domains, which leads to a timing issue.  Let's just make
    // sure we weren't attached to MyAvatar by trying this (if we weren't, this will have no effect)
    if (EntityTreeRenderer::removeMaterialFromAvatar(AVATAR_SELF_ID, material, oldParentMaterialNameStd)) {
        _appliedMaterial = nullptr;
        return;
    }
}

void MaterialEntityRenderer::applyTextureTransform(std::shared_ptr<NetworkMaterial>& material) {
    Transform textureTransform;
    if (_materialMappingMode == MaterialMappingMode::UV) {
        textureTransform.setTranslation(glm::vec3(_materialMappingPos, 0.0f));
        textureTransform.setRotation(glm::vec3(0.0f, 0.0f, glm::radians(_materialMappingRot)));
        textureTransform.setScale(glm::vec3(_materialMappingScale, 1.0f));
    } else if (_materialMappingMode == MaterialMappingMode::PROJECTED) {
        textureTransform = _transform;
        textureTransform.postScale(_dimensions);
        // Pass the inverse transform here so we don't need to compute it in the shaders
        textureTransform.evalFromRawMatrix(textureTransform.getInverseMatrix());
    }
    material->setTextureTransforms(textureTransform, _materialMappingMode, _materialRepeat);
}

void MaterialEntityRenderer::applyMaterial(const TypedEntityPointer& entity) {
    _retryApply = false;

    std::shared_ptr<NetworkMaterial> material = getMaterial();
    QUuid parentID = _parentID;
    if (!material || parentID.isNull()) {
        _appliedMaterial = nullptr;
        return;
    }

    applyTextureTransform(material);

    graphics::MaterialLayer materialLayer = graphics::MaterialLayer(material, _priority);

    if (material->isProcedural()) {
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(material);
        procedural->setBoundOperator([this](RenderArgs* args) { return getBound(args); });
        entity->setHasVertexShader(procedural->hasVertexShader());
    }

    // Our parent could be an entity or an avatar
    std::string parentMaterialName = _parentMaterialName.toStdString();
    if (EntityTreeRenderer::addMaterialToEntity(parentID, materialLayer, parentMaterialName)) {
        _appliedMaterial = material;
        return;
    }

    if (EntityTreeRenderer::addMaterialToAvatar(parentID, materialLayer, parentMaterialName)) {
        _appliedMaterial = material;
        return;
    }

    // if we've reached this point, we couldn't find our parent, so we need to try again later
    _retryApply = true;
}
