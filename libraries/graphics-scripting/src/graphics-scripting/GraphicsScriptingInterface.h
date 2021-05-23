//
//  GraphicsScriptingInterface.h
//  libraries/graphics-scripting/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GraphicsScriptingInterface_h
#define hifi_GraphicsScriptingInterface_h

#include <QtCore/QSharedPointer>
#include <QtCore/QObject>
#include <QUrl>

#include "ScriptableMesh.h"
#include <DependencyManager.h>
#include "RegisteredMetaTypes.h"
#include <Scriptable.h>

class ScriptEngine;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

/*@jsdoc
 * The <code>Graphics</code> API enables you to access and manipulate avatar, entity, and overlay models in the rendered scene. 
 * This includes getting mesh and material information for applying {@link Entities.EntityProperties-Material|Material} 
 * entities.
 *
 * @namespace Graphics
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */

class GraphicsScriptingInterface : public QObject, public Scriptable, public Dependency {
    Q_OBJECT

public:
    static void registerMetaTypes(ScriptEngine* engine);
    GraphicsScriptingInterface(QObject* parent = nullptr);

public slots:
    /*@jsdoc
     * Gets a handle to the model data used for displaying an avatar, 3D entity, or 3D overlay.
     * <p>Note: The model data may be used for more than one instance of the item displayed in the scene.</p>
     * @function Graphics.getModel
     * @param {UUID} id - The ID of the avatar, 3D entity, or 3D overlay.
     * @returns {GraphicsModel} The model data for the avatar, entity, or overlay, as displayed. This includes the results of 
     *     applying any {@link Entities.EntityProperties-Material|Material} entities to the item.
     * @example <caption>Report some details of your avatar's model.</caption>
     * var model = Graphics.getModel(MyAvatar.sessionUUID);
     * var meshes = model.meshes;
     * var numMeshparts = 0;
     * for (var i = 0; i < meshes.length; i++) {
     *     numMeshparts += meshes[i].numParts;
     * }
     * 
     * print("Avatar:", MyAvatar.skeletonModelURL);
     * print("Number of meshes:", model.numMeshes);
     * print("Number of mesh parts:", numMeshparts);
     * print("Material names: ", model.materialNames);
     * print("Material layers:", Object.keys(model.materialLayers));
     */
    scriptable::ScriptableModelPointer getModel(const QUuid& uuid);

    /*@jsdoc
     * Updates the model for an avatar, 3D entity, or 3D overlay in the rendered scene.
     * @function Graphics.updateModel
     * @param {Uuid} id - The ID of the avatar, 3D entity, or 3D overlay to update.
     * @param {GraphicsModel} model - The model to update the avatar, 3D entity, or 3D overlay with.
     * @returns {boolean} <code>true</code> if the update was completed successfully, <code>false</code> if it wasn't.
     */
    bool updateModel(const QUuid& uuid, const scriptable::ScriptableModelPointer& model);

    /*@jsdoc
     * Checks whether the model for an avatar, entity, or overlay can be updated in the rendered scene. Only avatars, 
     * <code>"Model"</code> entities and <code>"model"</code> overlays can have their meshes updated.
     * @function Graphics.canUpdateModel
     * @param {Uuid} id - The ID of the avatar, entity, or overlay.
     * @param {number} [meshIndex=-1] - <em>Not used.</em>
     * @param {number} [partNumber=-1] - <em>Not used.</em>
     * @returns {boolean} <code>true</code> if the model can be updated, <code>false</code> if it can't.
     * @example <caption>Test whether different types of items can be updated.</caption>
     * var modelEntityID = Entities.addEntity({
     *     type: "Model",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(Camera.orientation, { x: -0.5, y: 0, z: -3 })),
     *     rotation: MyAvatar.orientation,
     *     modelURL: "https://apidocs.vircadia.dev/models/cowboy-hat.fbx",
     *     dimensions: { x: 0.8569, y: 0.3960, z: 1.0744 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * var shapeEntityID = Entities.addEntity({
     *     type: "Shape",
     *     shape: "Cylinder",
     *     position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(Camera.orientation, { x: 0.5, y: 0, z: -3 })),
     *     dimensions: { x: 0.4, y: 0.6, z: 0.4 },
     *     lifetime: 300  // Delete after 5 minutes.
     * });
     * 
     * Script.setTimeout(function () {
     *     print("Can update avatar:", Graphics.canUpdateModel(MyAvatar.sessionUUID));  // true
     *     print("Can update model entity:", Graphics.canUpdateModel(modelEntityID));  // true
     *     print("Can update shape entity:", Graphics.canUpdateModel(shapeEntityID));  // false
     * }, 1000);  // Wait for the entities to rez.
     */
    bool canUpdateModel(const QUuid& uuid, int meshIndex = -1, int partNumber = -1);

    /*@jsdoc
     * Creates a new graphics model from meshes.
     * @function Graphics.newModel
     * @param {GraphicsMesh[]} meshes - The meshes to include in the model.
     * @returns {GraphicsModel} The new graphics model.
     */
    scriptable::ScriptableModelPointer newModel(const scriptable::ScriptableMeshes& meshes);

    /*@jsdoc
     * Creates a new graphics mesh.
     * @function Graphics.newMesh
     * @param {Graphics.IFSData} ifsMeshData - Index-Faced Set (IFS) data defining the mesh.
     * @returns {GraphicsMesh} The new graphics mesh.
     */
    scriptable::ScriptableMeshPointer newMesh(const QVariantMap& ifsMeshData);

#ifdef SCRIPTABLE_MESH_TODO
    scriptable::ScriptableMeshPartPointer exportMeshPart(scriptable::ScriptableMeshPointer mesh, int partNumber = -1) {
        return scriptable::make_scriptowned<scriptable::ScriptableMeshPart>(mesh, part);
    }
    bool updateMeshPart(scriptable::ScriptableMeshPointer mesh, scriptable::ScriptableMeshPartPointer part);
#endif

    /*@jsdoc
     * Exports a model to OBJ format.
     * @function Graphics.exportModelToOBJ
     * @param {GraphicsModel} model - The model to export.
     * @returns {string} The OBJ format representation of the model.
     */
    // FIXME: If you put the OBJ on the Asset Server and rez it, Interface keeps crashing until the model is removed.
    QString exportModelToOBJ(const scriptable::ScriptableModelPointer& model);

private:
    scriptable::ModelProviderPointer getModelProvider(const QUuid& uuid);
    void jsThrowError(const QString& error);
    scriptable::MeshPointer getMeshPointer(scriptable::ScriptableMeshPointer meshProxy);
    scriptable::MeshPointer getMeshPointer(scriptable::ScriptableMesh& meshProxy);
    scriptable::MeshPointer getMeshPointer(const scriptable::ScriptableMesh& meshProxy);

};

namespace scriptable {
    ScriptValuePointer scriptableMaterialToScriptValue(ScriptEngine* engine, const scriptable::ScriptableMaterial &material);
};

Q_DECLARE_METATYPE(NestableType)

#endif // hifi_GraphicsScriptingInterface_h
