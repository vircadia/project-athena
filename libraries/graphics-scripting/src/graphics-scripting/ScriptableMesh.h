//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include "ScriptableModel.h"

#include <glm/glm.hpp>
#include <graphics/BufferViewHelpers.h>
#include <DependencyManager.h>

#include <memory>
#include <QPointer>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtCore/QSharedPointer>

#include "GraphicsScriptingUtil.h"

#include <graphics/Geometry.h>
#include <Scriptable.h>

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

namespace scriptable {
    /*@jsdoc
     * A handle to in-memory mesh data in a {@link GraphicsModel}.
     * 
     * <p>Create using the {@link Graphics} API, {@link GraphicsModel.cloneModel}, or {@link GraphicsMesh.cloneMesh}.</p>
     *
     * @class GraphicsMesh
     * @hideconstructor
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @property {number} numParts - The number of mesh parts.
     *     <em>Read-only.</em>
     * @property {GraphicsMeshPart[]} parts - The mesh parts.
     *     <em>Read-only.</em>
     * @property {number} numIndices - The total number of vertex indices in the mesh.
     *     <em>Read-only.</em>
     * @property {number} numVertices - The total number of vertices in the mesh.
     *     <em>Read-only.</em>
     * @property {number} numAttributes - The number of vertex attributes.
     *     <em>Read-only.</em>
     * @property {Graphics.BufferTypeName[]} attributeNames - The names of the vertex attributes.
     *     <em>Read-only.</em>
     * @property {boolean} valid - <code>true</code> if the mesh is valid, <code>false</code> if it isn't.
     *     <em>Read-only.</em>
     * @property {boolean} strong - <code>true</code> if the mesh is valid and able to be used, <code>false</code> if it isn't.
     *     <em>Read-only.</em>
     * @property {Graphics.MeshExtents} extents - The mesh extents, in model coordinates.
     *     <em>Read-only.</em>
     * @property {Object<Graphics.BufferTypeName, Graphics.BufferFormat>} bufferFormats - Details of the buffers used for the 
     *     mesh.
     *     <em>Read-only.</em>
     *
     * @borrows GraphicsMesh.getVertextAttributes as getVertextAttributes
     * @borrows GraphicsMesh.setVertextAttributes as setVertextAttributes
     */
    class ScriptableMesh : public ScriptableMeshBase, Scriptable {
        Q_OBJECT
    public:
        Q_PROPERTY(glm::uint32 numParts READ getNumParts)
        Q_PROPERTY(glm::uint32 numAttributes READ getNumAttributes)
        Q_PROPERTY(glm::uint32 numVertices READ getNumVertices)
        Q_PROPERTY(glm::uint32 numIndices READ getNumIndices)
        Q_PROPERTY(QVector<QString> attributeNames READ getAttributeNames)
        Q_PROPERTY(QVector<scriptable::ScriptableMeshPartPointer> parts READ getMeshParts)
        Q_PROPERTY(bool valid READ isValid)
        Q_PROPERTY(bool strong READ hasValidStrongMesh)
        Q_PROPERTY(QVariantMap extents READ getMeshExtents)
        Q_PROPERTY(QVariantMap bufferFormats READ getBufferFormats)
        QVariantMap getBufferFormats() const;

        operator const ScriptableMeshBase*() const { return (qobject_cast<const scriptable::ScriptableMeshBase*>(this)); }

        ScriptableMesh(WeakModelProviderPointer provider, ScriptableModelBasePointer model, MeshPointer mesh, QObject* parent)
            : ScriptableMeshBase(provider, model, mesh, parent), Scriptable() { strongMesh = mesh; }
        ScriptableMesh(MeshPointer mesh, QObject* parent)
            : ScriptableMeshBase(WeakModelProviderPointer(), nullptr, mesh, parent), Scriptable() { strongMesh = mesh; }
        ScriptableMesh(const ScriptableMeshBase& other);
        ScriptableMesh(const ScriptableMesh& other) : ScriptableMeshBase(other), Scriptable() {};
        virtual ~ScriptableMesh();

        const scriptable::MeshPointer getOwnedMeshPointer() const { return strongMesh; }
        scriptable::ScriptableMeshPointer getSelf() const { return const_cast<scriptable::ScriptableMesh*>(this); }
        bool isValid() const { return !weakMesh.expired(); }
        bool hasValidStrongMesh() const { return (bool)strongMesh; }
        glm::uint32 getNumParts() const;
        glm::uint32 getNumVertices() const;
        glm::uint32 getNumAttributes() const;
        glm::uint32 getNumIndices() const;
        QVector<QString> getAttributeNames() const;
        QVector<scriptable::ScriptableMeshPartPointer> getMeshParts() const;
        QVariantMap getMeshExtents() const;

        operator bool() const { return !weakMesh.expired(); }
        int getSlotNumber(const QString& attributeName) const;

    public slots:
        
        /*@jsdoc
         * Gets the model the mesh is part of.
         * <p><em>Currently doesn't work.</em></p>
         * @function GraphicsMesh.getParentModel
         * @returns {GraphicsModel} The model the mesh is part of, <code>null</code> if it isn't part of a model.
         */
        const scriptable::ScriptableModelPointer getParentModel() const { return qobject_cast<scriptable::ScriptableModel*>(model); }
        
        /*@jsdoc
         * Gets the vertex indices.
         * @function GraphicsMesh.getIndices
         * @returns {number[]} The vertex indices.
         */
        QVector<glm::uint32> getIndices() const;

        /*@jsdoc
         * Gets the indices of nearby vertices.
         * @function GraphicsMesh.findNearbyVertexIndices
         * @param {Vec3} origin - The search position, in model coordinates.
         * @param {number} [epsilon=1e-6] - The search distance. If a vertex is within this distance from the 
         *    <code>origin</code> it is considered to be "nearby".
         * @returns {number[]} The indices of nearby vertices.
         */
        QVector<glm::uint32> findNearbyVertexIndices(const glm::vec3& origin, float epsilon = 1e-6) const;

        /*@jsdoc
         * Adds an attribute for all vertices.
         * @function GraphicsMesh.addAttribute
         * @param {Graphics.BufferTypeName} name - The name of the attribute.
         * @param {Graphics.BufferType} [defaultValue] - The value to give the attributes added to the vertices.
         * @returns {number} The number of vertices the attribute was added to, <code>0</code> if the <code>name</code> was 
         *     invalid or all vertices already had the attribute.
         */
        glm::uint32 addAttribute(const QString& attributeName, const QVariant& defaultValue = QVariant());

        /*@jsdoc
         * Sets the value of an attribute for all vertices.
         * @function GraphicsMesh.fillAttribute
         * @param {Graphics.BufferTypeName} name - The name of the attribute. The attribute is added to the vertices if not 
         *     already present.
         * @param {Graphics.BufferType} value - The value to give the attributes.
         * @returns {number} <code>1</code> if the attribute name was valid and the attribute values were set, <code>0</code> 
         *     otherwise.
         */
        glm::uint32 fillAttribute(const QString& attributeName, const QVariant& value);

        /*@jsdoc
         * Removes an attribute from all vertices.
         * <p>Note: The <code>"position"</code> attribute cannot be removed.</p>
         * @function GraphicsMesh.removeAttribute
         * @param {Graphics.BufferTypeName} name - The name of the attribute to remove. 
         * @returns {boolean} <code>true</code> if the attribute existed and was removed, <code>false</code> otherwise.
         */
        bool removeAttribute(const QString& attributeName);

        /*@jsdoc
         * Gets the value of an attribute for all vertices.
         * @function GraphicsMesh.queryVertexAttributes
         * @param {Graphics.BufferTypeName} name - The name of the attribute to get the vertex values of.
         * @throws Throws an error if the <code>name</code> is invalid or isn't used in the mesh.
         * @returns {Graphics.BufferType[]} The attribute values for all vertices.
         */
        QVariantList queryVertexAttributes(QVariant selector) const;

        /*@jsdoc
         * Gets the attributes and attribute values of a vertex.
         * @function GraphicsMesh.getVertexAttributes
         * @param {number} index - The vertex to get the attributes for.
         * @returns {Object<Graphics.BufferTypeName,Graphics.BufferType>} The attribute names and values for the vertex.
         * @throws Throws an error if the <code>index</code> is invalid.
         */
        QVariantMap getVertexAttributes(glm::uint32 vertexIndex) const;

        /*@jsdoc
         * Updates attribute values of a vertex.
         * @function GraphicsMesh.setVertexAttributes
         * @param {number} index - The vertex to set the attributes for.
         * @param {Object<Graphics.BufferTypeNAme,Graphics.BufferType>} values - The attribute names and values. Need not 
         *     specify unchanged values.
         * @returns {boolean} <code>true</code> if the index and the attribute names and values were valid and the vertex was 
         *     updated, <code>false</code> otherwise.
         * @throws Throws an error if the <code>index</code> is invalid or one of the attribute names is invalid or isn't used 
         *     in the mesh.
         */
        // @borrows jsdoc from GraphicsMesh
        bool setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributeValues);

        /*@jsdoc
         * Gets the value of a vertex's attribute.
         * @function GraphicsMesh.getVertexProperty
         * @param {number} index - The vertex index.
         * @param {Graphics.BufferTypeName} name - The name of the vertex attribute to get.
         * @returns {Graphics.BufferType} The value of the vertex attribute.
         * @throws Throws an error if the <code>index</code> is invalid or <code>name</code> is invalid or isn't used in the 
         *     mesh.
         */
        QVariant getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const;

        /*@jsdoc
         * Sets the value of a vertex's attribute.
         * @function GraphicsMesh.setVertexProperty
         * @param {number} index - The vertex index.
         * @param {Graphics.BufferTypeName} name - The name of the vertex attribute to set.
         * @param {Graphics.BufferType} value - The vertex attribute value to set.
         * @returns {boolean} <code>true</code> if the vertex attribute value was set, <code>false</code> if it wasn't.
         * @throws Throws an error if the <code>index</code> is invalid or <code>name</code> is invalid or isn't used in the
         *     mesh.
         */
        bool setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& value);

        /*@jsdoc
         * Makes a copy of the mesh.
         * @function GraphicsMesh.cloneMesh
         * @returns {GraphicsMesh} A copy of the mesh.
         */
        scriptable::ScriptableMeshPointer cloneMesh();

        // ScriptEngine-specific wrappers

        /*@jsdoc
         * Updates vertex attributes by calling a function for each vertex. The function can return modified attributes to 
         * update the vertex with.
         * @function GraphicsMesh.updateVertexAttributes
         * @param {GraphicsMesh~updateVertexAttributesCallback} callback - The function to call for each vertex.
         * @returns {number} The number of vertices the callback was called for.
         */
        glm::uint32 updateVertexAttributes(ScriptValuePointer callback);

        /*@jsdoc
         * Calls a function for each vertex.
         * @function GraphicsMesh.forEachVertex
         * @param {GraphicsMesh~forEachVertexCallback} callback - The function to call for each vertex.
         * @returns {number} The number of vertices the callback was called for.
         */
        glm::uint32 forEachVertex(ScriptValuePointer callback);

        /*@jsdoc
         * Checks if an index is valid and, optionally, that vertex has a particular attribute.
         * @function GraphicsMesh.isValidIndex
         * @param {number} index - The index to check.
         * @param {Graphics.BufferTypeName} [attribute] - The attribute to check.
         * @returns {boolean} <code>true</code> if the index is valid and that vertex has the attribute if specified.
         * @throws Throws an error if the <code>index</code> if invalid or <code>name</code> is invalid or isn't used in the 
         *     mesh.
         */
        // FIXME: Should return false rather than throw an error.
        bool isValidIndex(glm::uint32 vertexIndex, const QString& attributeName = QString()) const;
    };

}

Q_DECLARE_METATYPE(scriptable::ScriptableMeshPointer)
Q_DECLARE_METATYPE(QVector<scriptable::ScriptableMeshPointer>)
