//
//  GraphicsScriptingInterface.cpp
//  libraries/graphics-scripting/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GraphicsScriptingInterface.h"
#include "GraphicsScriptingUtil.h"
#include "OBJWriter.h"
#include "RegisteredMetaTypes.h"
#include "ScriptEngineLogging.h"
#include "ScriptableMesh.h"
#include "ScriptableMeshPart.h"
#include <GeometryUtil.h>
#include <QUuid>
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptValue.h>
#include <ScriptValueUtils.h>
#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <shared/QtHelpers.h>
#include <SpatiallyNestable.h>

GraphicsScriptingInterface::GraphicsScriptingInterface(QObject* parent) : QObject(parent), Scriptable() {
}

void GraphicsScriptingInterface::jsThrowError(const QString& error) {
    if (context()) {
        context()->throwError(error);
    } else {
        qCWarning(graphics_scripting) << "GraphicsScriptingInterface::jsThrowError (without valid JS context):" << error;
    }
}

bool GraphicsScriptingInterface::canUpdateModel(const QUuid& uuid, int meshIndex, int partNumber) {
    auto provider = getModelProvider(uuid);
    return provider && provider->canReplaceModelMeshPart(meshIndex, partNumber);
}

bool GraphicsScriptingInterface::updateModel(const QUuid& uuid, const scriptable::ScriptableModelPointer& model) {
    if (!model) {
        jsThrowError("null model argument");
    }

    auto base = model->operator scriptable::ScriptableModelBasePointer();
    if (!base) {
        jsThrowError("could not get base model pointer");
        return false;
    }

    auto provider = getModelProvider(uuid);
    if (!provider) {
        jsThrowError("provider unavailable");
        return false;
    }

    if (!provider->canReplaceModelMeshPart(-1, -1)) {
        jsThrowError("provider does not support updating mesh parts");
        return false;
    }

#ifdef SCRIPTABLE_MESH_DEBUG
    qDebug() << "replaceScriptableModelMeshPart" << model->toString() << -1 << -1;
#endif
    return provider->replaceScriptableModelMeshPart(base, -1, -1);
}

scriptable::ModelProviderPointer GraphicsScriptingInterface::getModelProvider(const QUuid& uuid) {
    QString error;
    if (auto appProvider = DependencyManager::get<scriptable::ModelProviderFactory>()) {
        if (auto provider = appProvider->lookupModelProvider(uuid)) {
            return provider;
        } else {
            error = "provider unavailable for " + uuid.toString();
        }
    } else {
        error = "appProvider unavailable";
    }
    jsThrowError(error);
    return nullptr;
}

scriptable::ScriptableModelPointer GraphicsScriptingInterface::newModel(const scriptable::ScriptableMeshes& meshes) {
    auto modelWrapper = scriptable::make_scriptowned<scriptable::ScriptableModel>();
    modelWrapper->setObjectName("js::model");
    if (meshes.isEmpty()) {
        jsThrowError("expected [meshes] array as first argument");
    } else {
        int i = 0;
        for (const auto& mesh : meshes) {
#ifdef SCRIPTABLE_MESH_DEBUG
            qDebug() << "newModel" << i << meshes.size() << mesh;
#endif
            if (mesh) {
                modelWrapper->append(*mesh);
            } else {
                jsThrowError(QString("invalid mesh at index: %1").arg(i));
                break;
            }
            i++;
        }
    }
    return modelWrapper;
}

scriptable::ScriptableModelPointer GraphicsScriptingInterface::getModel(const QUuid& uuid) {
    QString error;
    bool success;
    QString providerType = "unknown";
    if (auto nestable = DependencyManager::get<SpatialParentFinder>()->find(uuid, success).lock()) {
        providerType = SpatiallyNestable::nestableTypeToString(nestable->getNestableType());
        if (auto provider = getModelProvider(uuid)) {
            auto modelObject = provider->getScriptableModel();
            const bool found = !modelObject.objectID.isNull();
            if (found && uuid == AVATAR_SELF_ID) {
                // special case override so that scripts can rely on matching intput/output UUIDs
                modelObject.objectID = AVATAR_SELF_ID;
            }
            if (modelObject.objectID == uuid) {
                if (modelObject.meshes.size()) {
                    auto modelWrapper = scriptable::make_scriptowned<scriptable::ScriptableModel>(modelObject);
                    modelWrapper->setObjectName(providerType+"::"+uuid.toString()+"::model");
                    return modelWrapper;
                } else {
                    error = "no meshes available: " + modelObject.objectID.toString();
                }
            } else {
                error = QString("objectID mismatch: %1 (result contained %2 meshes)").arg(modelObject.objectID.toString()).arg(modelObject.meshes.size());
            }
        } else {
            error = "model provider unavailable";
        }
    } else {
        error = "model object not found";
    }
    jsThrowError(QString("failed to get meshes from %1 provider for uuid %2 (%3)").arg(providerType).arg(uuid.toString()).arg(error));
    return nullptr;
}

#ifdef SCRIPTABLE_MESH_TODO
bool GraphicsScriptingInterface::updateMeshPart(scriptable::ScriptableMeshPointer mesh, scriptable::ScriptableMeshPartPointer part) {
    Q_ASSERT(mesh);
    Q_ASSERT(part);
    Q_ASSERT(part->parentMesh);
    auto tmp = exportMeshPart(mesh, part->partIndex);
    if (part->parentMesh == mesh) {
#ifdef SCRIPTABLE_MESH_DEBUG
        qCInfo(graphics_scripting) << "updateMeshPart -- update via clone" << mesh << part;
#endif
        tmp->replaceMeshData(part->cloneMeshPart());
        return false;
    } else {
#ifdef SCRIPTABLE_MESH_DEBUG
        qCInfo(graphics_scripting) << "updateMeshPart -- update via inplace" << mesh << part;
#endif
        tmp->replaceMeshData(part);
        return true;
    }
}
#endif

scriptable::ScriptableMeshPointer GraphicsScriptingInterface::newMesh(const QVariantMap& ifsMeshData) {
    // TODO: this is bare-bones way for now to improvise a new mesh from the scripting side
    //  in the future we want to support a formal C++ structure data type here instead

    /*@jsdoc
     * IFS (Indexed-Face Set) data defining a mesh.
     * @typedef {object} Graphics.IFSData
     * @property {string} [name=""] - Mesh name. (Useful for debugging.)
     * @property {Graphics.MeshTopology} topology - Element interpretation. <em>Currently only triangles is supported.</em>
     * @property {number[]} indices - Vertex indices to use for the mesh faces, in tuples per the <code>topology</code>.
     * @property {Vec3[]} positions - Vertex positions, in model coordinates.
     * @property {Vec3[]} [normals=[]] - Vertex normals (normalized).
     * @property {Vec3[]} [colors=[]] - Vertex colors (normalized).
     * @property {Vec2[]} [texCoords0=[]] - Vertex texture coordinates (normalized).
     */
    QString meshName = ifsMeshData.value("name").toString();
    QString topologyName = ifsMeshData.value("topology").toString();
    QVector<glm::uint32> indices = buffer_helpers::variantToVector<glm::uint32>(ifsMeshData.value("indices"));
    QVector<glm::vec3> vertices = buffer_helpers::variantToVector<glm::vec3>(ifsMeshData.value("positions"));
    QVector<glm::vec3> normals = buffer_helpers::variantToVector<glm::vec3>(ifsMeshData.value("normals"));
    QVector<glm::vec3> colors = buffer_helpers::variantToVector<glm::vec3>(ifsMeshData.value("colors"));
    QVector<glm::vec2> texCoords0 = buffer_helpers::variantToVector<glm::vec2>(ifsMeshData.value("texCoords0"));

    const auto numVertices = vertices.size();
    const auto numIndices = indices.size();
    const auto topology = graphics::TOPOLOGIES.key(topologyName);

    // TODO: support additional topologies (POINTS and LINES ought to "just work" --
    //   if MeshPartPayload::drawCall is updated to actually check the Mesh::Part::_topology value
    //   (TRIANGLE_STRIP, TRIANGLE_FAN, QUADS, QUAD_STRIP may need additional conversion code though)
    static const QStringList acceptableTopologies{ "triangles" };

    // sanity checks
    QString error;
    if (!topologyName.isEmpty() && !acceptableTopologies.contains(topologyName)) {
        error = QString("expected .topology to be %1").arg(acceptableTopologies.join(" | "));
    } else if (!numIndices) {
        error = QString("expected non-empty [uint32,...] array for .indices (got type=%1)").arg(ifsMeshData.value("indices").typeName());
    } else if (numIndices % 3 != 0) {
        error = QString("expected 'triangle faces' for .indices (ie: length to be divisible by 3) length=%1").arg(numIndices);
    } else if (!numVertices) {
        error = "expected non-empty [glm::vec3(),...] array for .positions";
    } else {
        const gpu::uint32 maxVertexIndex = numVertices;
        int i = 0;
        for (const auto& ind : indices) {
            if (ind >= maxVertexIndex) {
                error = QString("index out of .indices[%1] index=%2 >= maxVertexIndex=%3").arg(i).arg(ind).arg(maxVertexIndex);
                break;
            }
            i++;
        }
    }
    if (!error.isEmpty()) {
        jsThrowError(error);
        return nullptr;
    }

    if (ifsMeshData.contains("normals") && normals.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .normals to #" << numVertices;
        normals.resize(numVertices);
    }
    if (ifsMeshData.contains("colors") && colors.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .colors to #" << numVertices;
        colors.resize(numVertices);
    }
    if (ifsMeshData.contains("texCoords0") && texCoords0.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .texCoords0 to #" << numVertices;
        texCoords0.resize(numVertices);
    }
    if (ifsMeshData.contains("texCoords1")) {
        qCWarning(graphics_scripting) << "newMesh - texCoords1 not yet supported; ignoring";
    }

    graphics::MeshPointer mesh(new graphics::Mesh());
    mesh->modelName = "graphics::newMesh";
    mesh->displayName = meshName.toStdString();

    // TODO: newFromVector does inbound type conversion, but not compression or packing
    //  (later we should autodetect if fitting into gpu::INDEX_UINT16 and reduce / pack normals etc.)
    mesh->setIndexBuffer(buffer_helpers::newFromVector(indices, gpu::Format::INDEX_INT32));
    mesh->setVertexBuffer(buffer_helpers::newFromVector(vertices, gpu::Format::VEC3F_XYZ));
    if (normals.size()) {
        mesh->addAttribute(gpu::Stream::NORMAL, buffer_helpers::newFromVector(normals, gpu::Format::VEC3F_XYZ));
    }
    if (colors.size()) {
        mesh->addAttribute(gpu::Stream::COLOR, buffer_helpers::newFromVector(colors, gpu::Format::VEC3F_XYZ));
    }
    if (texCoords0.size()) {
        mesh->addAttribute(gpu::Stream::TEXCOORD0, buffer_helpers::newFromVector(texCoords0, gpu::Format::VEC2F_UV));
    }
    QVector<graphics::Mesh::Part> parts = {{ 0, indices.size(), 0, topology }};
    mesh->setPartBuffer(buffer_helpers::newFromVector(parts, gpu::Element::PART_DRAWCALL));
    return scriptable::make_scriptowned<scriptable::ScriptableMesh>(mesh, nullptr);
}

QString GraphicsScriptingInterface::exportModelToOBJ(const scriptable::ScriptableModelPointer& model) {
    const auto& in = model->getConstMeshes();
    if (in.size()) {
        QList<scriptable::MeshPointer> meshes;
        foreach (auto meshProxy, in) {
            if (meshProxy) {
                meshes.append(getMeshPointer(meshProxy));
            }
        }
        if (meshes.size()) {
            return writeOBJToString(meshes);
        }
    }
    jsThrowError("null mesh");
    return QString();
}

MeshPointer GraphicsScriptingInterface::getMeshPointer(const scriptable::ScriptableMesh& meshProxy) {
    return meshProxy.getMeshPointer();
}
MeshPointer GraphicsScriptingInterface::getMeshPointer(scriptable::ScriptableMesh& meshProxy) {
    return getMeshPointer(&meshProxy);
}
MeshPointer GraphicsScriptingInterface::getMeshPointer(scriptable::ScriptableMeshPointer meshProxy) {
    MeshPointer result;
    if (!meshProxy) {
        jsThrowError("expected meshProxy as first parameter");
        return result;
    }
    auto mesh = meshProxy->getMeshPointer();
    if (!mesh) {
        jsThrowError("expected valid meshProxy as first parameter");
        return result;
    }
    return mesh;
}

namespace {
    QVector<int> metaTypeIds{
        qRegisterMetaType<scriptable::ScriptableMeshes>(),
        qRegisterMetaType<scriptable::ScriptableMeshes>("ScriptableMeshes"),
        qRegisterMetaType<scriptable::ScriptableMeshes>("scriptable::ScriptableMeshes"),
        qRegisterMetaType<QVector<scriptable::ScriptableMeshPointer>>("QVector<scriptable::ScriptableMeshPointer>"),
        qRegisterMetaType<scriptable::ScriptableMeshPointer>(),
        qRegisterMetaType<scriptable::ScriptableModelPointer>(),
        qRegisterMetaType<scriptable::ScriptableMeshPartPointer>(),
        qRegisterMetaType<scriptable::ScriptableMaterial>(),
        qRegisterMetaType<scriptable::ScriptableMaterialLayer>(),
        qRegisterMetaType<QVector<scriptable::ScriptableMaterialLayer>>(),
        qRegisterMetaType<scriptable::MultiMaterialMap>(),
        qRegisterMetaType<graphics::Mesh::Topology>(),
    };
}

namespace scriptable {
    template <typename T> int registerQPointerMetaType(ScriptEngine* engine) {
        scriptRegisterSequenceMetaType<QVector<QPointer<T>>>(engine);
        return scriptRegisterMetaType<QPointer<T>>(
            engine,
            [](ScriptEngine* engine, const QPointer<T>& object) -> ScriptValuePointer {
                if (!object) {
                    return engine->nullValue();
                }
                return engine->newQObject(object, ScriptEngine::QtOwnership, ScriptEngine::ExcludeDeleteLater | ScriptEngine::AutoCreateDynamicProperties);
            },
            [](const ScriptValuePointer& value, QPointer<T>& out) {
                auto obj = value->toQObject();
#ifdef SCRIPTABLE_MESH_DEBUG
                qCInfo(graphics_scripting) << "qpointer_qobject_cast" << obj << value.toString();
#endif
                if (auto tmp = qobject_cast<T*>(obj)) {
                    out = QPointer<T>(tmp);
                    return;
                }
#if 0
                if (auto tmp = static_cast<T*>(obj)) {
#ifdef SCRIPTABLE_MESH_DEBUG
                    qCInfo(graphics_scripting) << "qpointer_qobject_cast -- via static_cast" << obj << tmp << value.toString();
#endif
                    out = QPointer<T>(tmp);
                    return;
                }
#endif
                out = nullptr;
            }
        );
    }

    ScriptValuePointer qVectorScriptableMaterialLayerToScriptValue(ScriptEngine* engine, const QVector<scriptable::ScriptableMaterialLayer>& vector) {
        return scriptValueFromSequence(engine, vector);
    }

    void qVectorScriptableMaterialLayerFromScriptValue(const ScriptValuePointer& array, QVector<scriptable::ScriptableMaterialLayer>& result) {
        scriptValueToSequence(array, result);
    }

    /*@jsdoc
     * A material in a {@link GraphicsModel}.
     * @typedef {object} Graphics.Material
     * @property {string} name - The name of the material.
     * @property {string} model - Different material models support different properties and rendering modes. Supported models 
     *     are: <code>"hifi_pbr"</code> and <code>"hifi_shader_simple"</code>.
     * @property {Vec3|string} [albedo] - The albedo color. Component values are in the range <code>0.0</code> &ndash;
     *     <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     * @property {number|string} [opacity] - The opacity, range <code>0.0</code> &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *
     * @property {number|string} [opacityCutoff] - The opacity cutoff threshold used to determine the opaque texels of the
     *     <code>opacityMap</code> when <code>opacityMapMode</code> is <code>"OPACITY_MAP_MASK"</code>. Range <code>0.0</code>
     *     &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {number|string} [roughness] - The roughness, range <code>0.0</code> &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {number|string} [metallic] - The metallicness, range <code>0.0</code> &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {number|string} [scattering] - The scattering, range <code>0.0</code> &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {boolean|string} [unlit] - <code>true</code> if the material is unaffected by lighting, <code>false</code> if it
     *     it is lit by the key light and local lights.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {Vec3|string} [emissive] - The emissive color, i.e., the color that the material emits. Component values are
     *     in the range <code>0.0</code> &ndash; <code>1.0</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [albedoMap] - The URL of the albedo texture image.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [opacityMap] - The URL of the opacity texture image.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [opacityMapMode] - The mode defining the interpretation of the opacity map. Values can be:
     *     <ul>
     *         <li><code>"OPACITY_MAP_OPAQUE"</code> for ignoring the opacity map information.</li>
     *         <li><code>"OPACITY_MAP_MASK"</code> for using the <code>opacityMap</code> as a mask, where only the texel greater
     *         than <code>opacityCutoff</code> are visible and rendered opaque.</li>
     *         <li><code>"OPACITY_MAP_BLEND"</code> for using the <code>opacityMap</code> for alpha blending the material surface
     *         with the background.</li>
     *     </ul>
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [occlusionMap] - The URL of the occlusion texture image.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [lightMap] - The URL of the light map texture image.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [lightmapParams] - Parameters for controlling how <code>lightMap</code> is used.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     *     <p><em>Currently not used.</em></p>
     * @property {string} [scatteringMap] - The URL of the scattering texture image.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [emissiveMap] - The URL of the emissive texture image.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [metallicMap] - The URL of the metallic texture image.
     *     If <code>"fallthrough"</code> then it and <code>specularMap</code> fall through to the material below.
     *     Only use one of <code>metallicMap</code> and <code>specularMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [specularMap] - The URL of the specular texture image.
     *     Only use one of <code>metallicMap</code> and <code>specularMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [roughnessMap] - The URL of the roughness texture image.
     *     If <code>"fallthrough"</code> then it and <code>glossMap</code> fall through to the material below.
     *     Only use one of <code>roughnessMap</code> and <code>glossMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [glossMap] - The URL of the gloss texture image.
     *     Only use one of <code>roughnessMap</code> and <code>glossMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [normalMap] - The URL of the normal texture image.
     *     If <code>"fallthrough"</code> then it and <code>bumpMap</code> fall through to the material below.
     *     Only use one of <code>normalMap</code> and <code>bumpMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [bumpMap] - The URL of the bump texture image.
     *     Only use one of <code>normalMap</code> and <code>bumpMap</code>.
     *     <code>"hifi_pbr"</code> model only.
     * @property {string} [materialParams] - Parameters for controlling the material projection and repetition.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     *     <p><em>Currently not used.</em></p>
     * @property {string} [cullFaceMode="CULL_BACK"] - Specifies Which faces of the geometry to render. Values can be:
     *     <ul>
     *         <li><code>"CULL_NONE"</code> to render both sides of the geometry.</li>
     *         <li><code>"CULL_FRONT"</code> to cull the front faces of the geometry.</li>
     *         <li><code>"CULL_BACK"</code> (the default) to cull the back faces of the geometry.</li>
     *     </ul>
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {Mat4|string} [texCoordTransform0] - The transform to use for all of the maps apart from
     *     <code>occlusionMap</code> and <code>lightMap</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     * @property {Mat4|string} [texCoordTransform1] - The transform to use for <code>occlusionMap</code> and
     *     <code>lightMap</code>.
     *     If <code>"fallthrough"</code> then it falls through to the material below.
     *     <code>"hifi_pbr"</code> model only.
     *
     * @property {string} procedural - The definition of a procedural shader material.
     *     <code>"hifi_shader_simple"</code> model only.
     *     <p><em>Currently not used.</em></p>
     *
     * @property {boolean} defaultFallthrough - <code>true</code> if all properties fall through to the material below unless 
     *     they are set, <code>false</code> if properties respect their individual fall-through settings.
     */
    ScriptValuePointer scriptableMaterialToScriptValue(ScriptEngine* engine, const scriptable::ScriptableMaterial &material) {
        ScriptValuePointer obj = engine->newObject();
        obj->setProperty("name", material.name);
        obj->setProperty("model", material.model);

        bool hasPropertyFallthroughs = !material.propertyFallthroughs.empty();

        const ScriptValuePointer FALLTHROUGH(engine->newValue("fallthrough"));
        if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::OPACITY_VAL_BIT)) {
            obj->setProperty("opacity", FALLTHROUGH);
        } else if (material.key.isTranslucentFactor()) {
            obj->setProperty("opacity", material.opacity);
        }

        if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::ALBEDO_VAL_BIT)) {
            obj->setProperty("albedo", FALLTHROUGH);
        } else if (material.key.isAlbedo()) {
            obj->setProperty("albedo", vec3ColorToScriptValue(engine, material.albedo));
        }

        if (material.model.toStdString() == graphics::Material::HIFI_PBR) {
            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::OPACITY_CUTOFF_VAL_BIT)) {
                obj->setProperty("opacityCutoff", FALLTHROUGH);
            } else if (material.key.isOpacityCutoff()) {
                obj->setProperty("opacityCutoff", material.opacityCutoff);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::OPACITY_MAP_MODE_BIT)) {
                obj->setProperty("opacityMapMode", FALLTHROUGH);
            } else if (material.key.isOpacityMapMode()) {
                obj->setProperty("opacityMapMode", material.opacityMapMode);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::GLOSSY_VAL_BIT)) {
                obj->setProperty("roughness", FALLTHROUGH);
            } else if (material.key.isGlossy()) {
                obj->setProperty("roughness", material.roughness);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::METALLIC_VAL_BIT)) {
                obj->setProperty("metallic", FALLTHROUGH);
            } else if (material.key.isMetallic()) {
                obj->setProperty("metallic", material.metallic);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::SCATTERING_VAL_BIT)) {
                obj->setProperty("scattering", FALLTHROUGH);
            } else if (material.key.isScattering()) {
                obj->setProperty("scattering", material.scattering);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::UNLIT_VAL_BIT)) {
                obj->setProperty("unlit", FALLTHROUGH);
            } else if (material.key.isUnlit()) {
                obj->setProperty("unlit", material.unlit);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::EMISSIVE_VAL_BIT)) {
                obj->setProperty("emissive", FALLTHROUGH);
            } else if (material.key.isEmissive()) {
                obj->setProperty("emissive", vec3ColorToScriptValue(engine, material.emissive));
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::EMISSIVE_MAP_BIT)) {
                obj->setProperty("emissiveMap", FALLTHROUGH);
            } else if (!material.emissiveMap.isEmpty()) {
                obj->setProperty("emissiveMap", material.emissiveMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::ALBEDO_MAP_BIT)) {
                obj->setProperty("albedoMap", FALLTHROUGH);
            } else if (!material.albedoMap.isEmpty()) {
                obj->setProperty("albedoMap", material.albedoMap);
            }

            if (!material.opacityMap.isEmpty()) {
                obj->setProperty("opacityMap", material.opacityMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::OCCLUSION_MAP_BIT)) {
                obj->setProperty("occlusionMap", FALLTHROUGH);
            } else if (!material.occlusionMap.isEmpty()) {
                obj->setProperty("occlusionMap", material.occlusionMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::LIGHT_MAP_BIT)) {
                obj->setProperty("lightMap", FALLTHROUGH);
            } else if (!material.lightMap.isEmpty()) {
                obj->setProperty("lightMap", material.lightMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::SCATTERING_MAP_BIT)) {
                obj->setProperty("scatteringMap", FALLTHROUGH);
            } else if (!material.scatteringMap.isEmpty()) {
                obj->setProperty("scatteringMap", material.scatteringMap);
            }

            // Only set one of each of these
            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::METALLIC_MAP_BIT)) {
                obj->setProperty("metallicMap", FALLTHROUGH);
            } else if (!material.metallicMap.isEmpty()) {
                obj->setProperty("metallicMap", material.metallicMap);
            } else if (!material.specularMap.isEmpty()) {
                obj->setProperty("specularMap", material.specularMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::ROUGHNESS_MAP_BIT)) {
                obj->setProperty("roughnessMap", FALLTHROUGH);
            } else if (!material.roughnessMap.isEmpty()) {
                obj->setProperty("roughnessMap", material.roughnessMap);
            } else if (!material.glossMap.isEmpty()) {
                obj->setProperty("glossMap", material.glossMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::MaterialKey::NORMAL_MAP_BIT)) {
                obj->setProperty("normalMap", FALLTHROUGH);
            } else if (!material.normalMap.isEmpty()) {
                obj->setProperty("normalMap", material.normalMap);
            } else if (!material.bumpMap.isEmpty()) {
                obj->setProperty("bumpMap", material.bumpMap);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::Material::TEXCOORDTRANSFORM0)) {
                obj->setProperty("texCoordTransform0", FALLTHROUGH);
            } else if (material.texCoordTransforms[0] != mat4()) {
                obj->setProperty("texCoordTransform0", mat4toScriptValue(engine, material.texCoordTransforms[0]));
            }
            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::Material::TEXCOORDTRANSFORM1)) {
                obj->setProperty("texCoordTransform1", FALLTHROUGH);
            } else if (material.texCoordTransforms[1] != mat4()) {
                obj->setProperty("texCoordTransform1", mat4toScriptValue(engine, material.texCoordTransforms[1]));
            }

            // These need to be implemented, but set the fallthrough for now
            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::Material::LIGHTMAP_PARAMS)) {
                obj->setProperty("lightmapParams", FALLTHROUGH);
            }
            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::Material::MATERIAL_PARAMS)) {
                obj->setProperty("materialParams", FALLTHROUGH);
            }

            if (hasPropertyFallthroughs && material.propertyFallthroughs.at(graphics::Material::CULL_FACE_MODE)) {
                obj->setProperty("cullFaceMode", FALLTHROUGH);
            } else if (!material.cullFaceMode.isEmpty()) {
                obj->setProperty("cullFaceMode", material.cullFaceMode);
            }
        } else if (material.model.toStdString() == graphics::Material::HIFI_SHADER_SIMPLE) {
            obj->setProperty("procedural", material.procedural);
        }

        obj->setProperty("defaultFallthrough", material.defaultFallthrough);

        return obj;
    }

    void scriptableMaterialFromScriptValue(const ScriptValuePointer &object, scriptable::ScriptableMaterial& material) {
        // No need to convert from ScriptValuePointer to ScriptableMaterial
    }

    ScriptValuePointer scriptableMaterialLayerToScriptValue(ScriptEngine* engine, const scriptable::ScriptableMaterialLayer &materialLayer) {
        ScriptValuePointer obj = engine->newObject();
        obj->setProperty("material", scriptableMaterialToScriptValue(engine, materialLayer.material));
        obj->setProperty("priority", materialLayer.priority);
        return obj;
    }

    void scriptableMaterialLayerFromScriptValue(const ScriptValuePointer &object, scriptable::ScriptableMaterialLayer& materialLayer) {
        // No need to convert from ScriptValuePointer to ScriptableMaterialLayer
    }

    ScriptValuePointer multiMaterialMapToScriptValue(ScriptEngine* engine, const scriptable::MultiMaterialMap& map) {
        ScriptValuePointer obj = engine->newObject();
        for (auto key : map.keys()) {
            obj->setProperty(key, qVectorScriptableMaterialLayerToScriptValue(engine, map[key]));
        }
        return obj;
    }

    void multiMaterialMapFromScriptValue(const ScriptValuePointer& map, scriptable::MultiMaterialMap& result) {
        // No need to convert from ScriptValuePointer to MultiMaterialMap
    }

    template <typename T> int registerDebugEnum(ScriptEngine* engine, const DebugEnums<T>& debugEnums) {
        static const DebugEnums<T>& instance = debugEnums;
        return scriptRegisterMetaType<T>(
            engine,
            [](ScriptEngine* engine, const T& topology) -> ScriptValuePointer {
                return engine->newValue(instance.value(topology));
            },
            [](const ScriptValuePointer& value, T& topology) {
                topology = instance.key(value->toString());
            }
        );
    }
}

void GraphicsScriptingInterface::registerMetaTypes(ScriptEngine* engine) {
    scriptRegisterSequenceMetaType<QVector<scriptable::ScriptableMaterialLayer>>(engine);

    scriptable::registerQPointerMetaType<scriptable::ScriptableModel>(engine);
    scriptable::registerQPointerMetaType<scriptable::ScriptableMesh>(engine);
    scriptable::registerQPointerMetaType<scriptable::ScriptableMeshPart>(engine);

    scriptable::registerDebugEnum<graphics::Mesh::Topology>(engine, graphics::TOPOLOGIES);
    scriptable::registerDebugEnum<gpu::Type>(engine, gpu::TYPES);
    scriptable::registerDebugEnum<gpu::Semantic>(engine, gpu::SEMANTICS);
    scriptable::registerDebugEnum<gpu::Dimension>(engine, gpu::DIMENSIONS);

    scriptRegisterMetaType(engine, scriptable::scriptableMaterialToScriptValue, scriptable::scriptableMaterialFromScriptValue);
    scriptRegisterMetaType(engine, scriptable::scriptableMaterialLayerToScriptValue, scriptable::scriptableMaterialLayerFromScriptValue);
    scriptRegisterMetaType(engine, scriptable::qVectorScriptableMaterialLayerToScriptValue, scriptable::qVectorScriptableMaterialLayerFromScriptValue);
    scriptRegisterMetaType(engine, scriptable::multiMaterialMapToScriptValue, scriptable::multiMaterialMapFromScriptValue);

    Q_UNUSED(metaTypeIds);
}
