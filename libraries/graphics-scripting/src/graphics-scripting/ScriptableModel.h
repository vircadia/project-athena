//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include "Forward.h"
#include "GraphicsScriptingUtil.h"

namespace scriptable {

    using ScriptableMeshes = QVector<scriptable::ScriptableMeshPointer>;

    /*@jsdoc
     * A handle to in-memory model data such as may be used in displaying avatars, 3D entities, or 3D overlays in the rendered 
     * scene. Changes made to the model are visible only to yourself; they are not persisted.
     * <p>Note: The model may be used for more than one instance of an item displayed in the scene. Modifying the model updates 
     * all instances displayed.</p>
     *
     * <p>Create using the {@link Graphics} API or {@link GraphicsModel.cloneModel}.</p>
     *
     * @class GraphicsModel
     * @hideconstructor
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @property {Uuid} objectID - The ID of the entity or avatar that the model is associated with, if any; <code>null</code> 
     *     if the model is not associated with an entity or avatar.
     *     <em>Read-only.</em>
     * @property {number} numMeshes - The number of meshes in the model.
     *     <em>Read-only.</em>
     * @property {GraphicsMesh[]} meshes - The meshes in the model. Each mesh may have more than one mesh part.
     *     <em>Read-only.</em>
     * @property {string[]} materialNames - The names of the materials used by each mesh part in the model. The names are in
     *     the order of the <code>meshes</code> and their mesh parts.
     *     <em>Read-only.</em>
     * @property {Object.<string,Graphics.MaterialLayer[]>} materialLayers - The mapping from mesh parts and material
     *     names to material layers. The mesh parts are numbered from <code>"0"</code> per the array indexes of 
     *     <code>materialNames</code>. The material names are those used in <code>materialNames</code>. (You can look up a 
     *     material layer by mesh part number or by material name.)
     *     <em>Read-only.</em>
     */
    class ScriptableModel : public ScriptableModelBase {
        Q_OBJECT
        Q_PROPERTY(QUuid objectID MEMBER objectID CONSTANT)
        Q_PROPERTY(glm::uint32 numMeshes READ getNumMeshes)
        Q_PROPERTY(ScriptableMeshes meshes READ getMeshes)
        Q_PROPERTY(scriptable::MultiMaterialMap materialLayers READ getMaterialLayers)
        Q_PROPERTY(QVector<QString> materialNames READ getMaterialNames)

    public:
        ScriptableModel(QObject* parent = nullptr) : ScriptableModelBase(parent) {}
        ScriptableModel(const ScriptableModel& other) : ScriptableModelBase(other) {}
        ScriptableModel(const ScriptableModelBase& other) : ScriptableModelBase(other) {}
        ScriptableModel& operator=(const ScriptableModelBase& view) {  ScriptableModelBase::operator=(view); return *this; }

        operator scriptable::ScriptableModelBasePointer() {
            return QPointer<scriptable::ScriptableModelBase>(qobject_cast<scriptable::ScriptableModelBase*>(this));
        }
        ScriptableMeshes getMeshes();
        const ScriptableMeshes getConstMeshes() const;

        scriptable::MultiMaterialMap getMaterialLayers() { return materialLayers; }
        QVector<QString> getMaterialNames() { return materialNames; }

    public slots:

        /*@jsdoc
         * Makes a copy of the model.
         * @function GraphicsModel.cloneModel
         * @param {object} [options] - <em>Not used.</em>
         * @returns {GraphicsModel} A copy of the model.
         */
        scriptable::ScriptableModelPointer cloneModel(const QVariantMap& options = QVariantMap());
        
        /*@jsdoc
         * Gets a string description of the model.
         * @function GraphicsModel.toString
         * @returns {string} A string description of the model.
         * @example <caption>Report the string description of your avatar's model.</caption>
         * var model = Graphics.getModel(MyAvatar.sessionUUID);
         * print("Avatar model info:", model.toString());
         */
        QString toString() const;

    protected:
        glm::uint32 getNumMeshes() { return meshes.size(); }

    };

}

Q_DECLARE_METATYPE(scriptable::ScriptableModelPointer)
Q_DECLARE_METATYPE(QVector<scriptable::ScriptableModelPointer>)
Q_DECLARE_METATYPE(scriptable::ScriptableMaterial)
Q_DECLARE_METATYPE(scriptable::ScriptableMaterialLayer)
Q_DECLARE_METATYPE(scriptable::MultiMaterialMap)
