//
//  ScriptableModel.cpp
//  libraries/graphics-scripting
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptableModel.h"

#include <ScriptValue.h>

#include "GraphicsScriptingUtil.h"
#include "ScriptableMesh.h"
#include "graphics/Material.h"
#include "image/TextureProcessing.h"

// #define SCRIPTABLE_MESH_DEBUG 1

scriptable::ScriptableMaterial& scriptable::ScriptableMaterial::operator=(const scriptable::ScriptableMaterial& material) {
    name = material.name;
    model = material.model;
    opacity = material.opacity;
    albedo = material.albedo;

    if (model.toStdString() == graphics::Material::HIFI_PBR) {
        opacityCutoff = material.opacityCutoff;
        opacityMapMode = material.opacityMapMode;
        roughness = material.roughness;
        metallic = material.metallic;
        scattering = material.scattering;
        unlit = material.unlit;
        emissive = material.emissive;
        emissiveMap = material.emissiveMap;
        albedoMap = material.albedoMap;
        opacityMap = material.opacityMap;
        metallicMap = material.metallicMap;
        specularMap = material.specularMap;
        roughnessMap = material.roughnessMap;
        glossMap = material.glossMap;
        normalMap = material.normalMap;
        bumpMap = material.bumpMap;
        occlusionMap = material.occlusionMap;
        lightMap = material.lightMap;
        scatteringMap = material.scatteringMap;
        cullFaceMode = material.cullFaceMode;
    } else if (model.toStdString() == graphics::Material::HIFI_SHADER_SIMPLE) {
        procedural = material.procedural;
    }

    defaultFallthrough = material.defaultFallthrough;
    propertyFallthroughs = material.propertyFallthroughs;

    key = material.key;

    return *this;
}

scriptable::ScriptableMaterial::ScriptableMaterial(const graphics::MaterialPointer& material) {
    if (material) {
        name = material->getName().c_str();
        model = material->getModel().c_str();
        opacity = material->getOpacity();
        albedo = material->getAlbedo();

        if (model.toStdString() == graphics::Material::HIFI_PBR) {
            opacityCutoff = material->getOpacityCutoff();
            opacityMapMode = QString(graphics::MaterialKey::getOpacityMapModeName(material->getOpacityMapMode()).c_str());
            roughness = material->getRoughness();
            metallic = material->getMetallic();
            scattering = material->getScattering();
            unlit = material->isUnlit();
            emissive = material->getEmissive();

            auto map = material->getTextureMap(graphics::Material::MapChannel::EMISSIVE_MAP);
            if (map && map->getTextureSource()) {
                emissiveMap = map->getTextureSource()->getUrl().toString();
            }

            map = material->getTextureMap(graphics::Material::MapChannel::ALBEDO_MAP);
            if (map && map->getTextureSource()) {
                albedoMap = map->getTextureSource()->getUrl().toString();
                if (map->useAlphaChannel()) {
                    opacityMap = albedoMap;
                }
            }

            map = material->getTextureMap(graphics::Material::MapChannel::METALLIC_MAP);
            if (map && map->getTextureSource()) {
                if (map->getTextureSource()->getType() == image::TextureUsage::Type::METALLIC_TEXTURE) {
                    metallicMap = map->getTextureSource()->getUrl().toString();
                } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::SPECULAR_TEXTURE) {
                    specularMap = map->getTextureSource()->getUrl().toString();
                }
            }

            map = material->getTextureMap(graphics::Material::MapChannel::ROUGHNESS_MAP);
            if (map && map->getTextureSource()) {
                if (map->getTextureSource()->getType() == image::TextureUsage::Type::ROUGHNESS_TEXTURE) {
                    roughnessMap = map->getTextureSource()->getUrl().toString();
                } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::GLOSS_TEXTURE) {
                    glossMap = map->getTextureSource()->getUrl().toString();
                }
            }

            map = material->getTextureMap(graphics::Material::MapChannel::NORMAL_MAP);
            if (map && map->getTextureSource()) {
                if (map->getTextureSource()->getType() == image::TextureUsage::Type::NORMAL_TEXTURE) {
                    normalMap = map->getTextureSource()->getUrl().toString();
                } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::BUMP_TEXTURE) {
                    bumpMap = map->getTextureSource()->getUrl().toString();
                }
            }

            map = material->getTextureMap(graphics::Material::MapChannel::OCCLUSION_MAP);
            if (map && map->getTextureSource()) {
                occlusionMap = map->getTextureSource()->getUrl().toString();
            }

            map = material->getTextureMap(graphics::Material::MapChannel::LIGHT_MAP);
            if (map && map->getTextureSource()) {
                lightMap = map->getTextureSource()->getUrl().toString();
            }

            map = material->getTextureMap(graphics::Material::MapChannel::SCATTERING_MAP);
            if (map && map->getTextureSource()) {
                scatteringMap = map->getTextureSource()->getUrl().toString();
            }

            for (int i = 0; i < graphics::Material::NUM_TEXCOORD_TRANSFORMS; i++) {
                texCoordTransforms[i] = material->getTexCoordTransform(i);
            }

            cullFaceMode = QString(graphics::MaterialKey::getCullFaceModeName(material->getCullFaceMode()).c_str());
        } else if (model.toStdString() == graphics::Material::HIFI_SHADER_SIMPLE) {
            procedural = material->getProceduralString();
        }

        defaultFallthrough = material->getDefaultFallthrough();
        propertyFallthroughs = material->getPropertyFallthroughs();

        key = material->getKey();
    }
}

scriptable::ScriptableMaterialLayer& scriptable::ScriptableMaterialLayer::operator=(const scriptable::ScriptableMaterialLayer& materialLayer) {
    material = materialLayer.material;
    priority = materialLayer.priority;

    return *this;
}

scriptable::ScriptableModelBase& scriptable::ScriptableModelBase::operator=(const scriptable::ScriptableModelBase& other) {
    provider = other.provider;
    objectID = other.objectID;
    for (const auto& mesh : other.meshes) {
        append(mesh);
    }
    materialLayers = other.materialLayers;
    materialNames = other.materialNames;
    return *this;
}

scriptable::ScriptableModelBase::~ScriptableModelBase() {
#ifdef SCRIPTABLE_MESH_DEBUG
    qCDebug(graphics_scripting) << "~ScriptableModelBase" << this;
#endif
    // makes cleanup order more deterministic to help with debugging
    for (auto& m : meshes) {
        m.strongMesh.reset();
    }
    meshes.clear();
    materialLayers.clear();
    materialNames.clear();
}

void scriptable::ScriptableModelBase::append(scriptable::WeakMeshPointer mesh) {
    meshes << ScriptableMeshBase{ provider, this, mesh, this /*parent*/ };
}

void scriptable::ScriptableModelBase::append(const ScriptableMeshBase& mesh) {
    if (mesh.provider.lock().get() != provider.lock().get()) {
        qCDebug(graphics_scripting) << "warning: appending mesh from different provider..." << mesh.provider.lock().get() << " != " << provider.lock().get();
    }
    meshes << mesh;
}

void scriptable::ScriptableModelBase::appendMaterial(const graphics::MaterialLayer& materialLayer, int shapeID, std::string materialName) {
    materialLayers[QString::number(shapeID)].push_back(ScriptableMaterialLayer(materialLayer));
    materialLayers["mat::" + QString::fromStdString(materialName)].push_back(ScriptableMaterialLayer(materialLayer));
}

void scriptable::ScriptableModelBase::appendMaterials(const std::unordered_map<std::string, graphics::MultiMaterial>& materialsToAppend) {
    auto materialsToAppendCopy = materialsToAppend;
    for (auto& multiMaterial : materialsToAppendCopy) {
        while (!multiMaterial.second.empty()) {
            materialLayers[QString(multiMaterial.first.c_str())].push_back(ScriptableMaterialLayer(multiMaterial.second.top()));
            multiMaterial.second.pop();
        }
    }
}

void scriptable::ScriptableModelBase::appendMaterialNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
        materialNames.append(QString::fromStdString(name));
    }
}

QString scriptable::ScriptableModel::toString() const {
    return QString("[ScriptableModel%1%2 numMeshes=%3]")
        .arg(objectID.isNull() ? "" : " objectID="+objectID.toString())
        .arg(objectName().isEmpty() ? "" : " name=" +objectName())
        .arg(meshes.size());
}

scriptable::ScriptableModelPointer scriptable::ScriptableModel::cloneModel(const QVariantMap& options) {
    scriptable::ScriptableModelPointer clone = scriptable::ScriptableModelPointer(new scriptable::ScriptableModel(*this));
    clone->meshes.clear();
    for (const auto &mesh : getConstMeshes()) {
        auto cloned = mesh->cloneMesh();
        if (auto tmp = qobject_cast<scriptable::ScriptableMeshBase*>(cloned)) {
            clone->meshes << *tmp;
            tmp->deleteLater(); // schedule our copy for cleanup
        } else {
            qCDebug(graphics_scripting) << "error cloning mesh" << cloned;
        }
    }
    return clone;
}


const scriptable::ScriptableMeshes scriptable::ScriptableModel::getConstMeshes() const {
    scriptable::ScriptableMeshes out;
    for (const auto& mesh : meshes) {
        const scriptable::ScriptableMesh* m = qobject_cast<const scriptable::ScriptableMesh*>(&mesh);
        if (!m) {
            m = scriptable::make_scriptowned<scriptable::ScriptableMesh>(mesh);
        } else {
            qCDebug(graphics_scripting) << "reusing scriptable mesh" << m;
        }
        const scriptable::ScriptableMeshPointer mp = scriptable::ScriptableMeshPointer(const_cast<scriptable::ScriptableMesh*>(m));
        out << mp;
    }
    return out;
}

scriptable::ScriptableMeshes scriptable::ScriptableModel::getMeshes() {
    scriptable::ScriptableMeshes out;
    for (auto& mesh : meshes) {
        scriptable::ScriptableMesh* m = qobject_cast<scriptable::ScriptableMesh*>(&mesh);
        if (!m) {
            m = scriptable::make_scriptowned<scriptable::ScriptableMesh>(mesh);
        } else {
            qCDebug(graphics_scripting) << "reusing scriptable mesh" << m;
        }
        scriptable::ScriptableMeshPointer mp = scriptable::ScriptableMeshPointer(m);
        out << mp;
    }
    return out;
}

#if 0
glm::uint32 scriptable::ScriptableModel::forEachVertexAttribute(ScriptValuePointer callback) {
    glm::uint32 result = 0;
    scriptable::ScriptableMeshes in = getMeshes();
    if (in.size()) {
        foreach (scriptable::ScriptableMeshPointer meshProxy, in) {
            result += meshProxy->mapAttributeValues(callback);
        }
    }
    return result;
}
#endif
