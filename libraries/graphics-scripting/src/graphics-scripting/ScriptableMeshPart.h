//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QtCore/QSharedPointer>

#include "ScriptableMesh.h"
#include <Scriptable.h>

class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

namespace scriptable {
    /*@jsdoc
     * A handle to in-memory mesh part data in a {@link GraphicsModel}.
     *
     * <p>Create using the {@link Graphics} API, {@link GraphicsModel.cloneModel}, {@link GraphicsMesh.cloneMesh}, or 
     * {@link GraphicsMeshPart.cloneMeshPart}.</p>
     *
     * @class GraphicsMeshPart
     * @hideconstructor
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @property {boolean} valid - <code>true</code> if the mesh part is valid, <code>false</code> if it isn't.
     *     <em>Read-only.</em>
     * @property {number} partIndex - The index of the part within the <em>whole</em> mesh (i.e., parent and mesh parts).
     *     <em>Read-only.</em>
     * @property {number} firstVertexIndex - The index of the first vertex.
     * @property {number} baseVertexIndex - The index of the base vertex.
     * @property {number} lastVertexIndex - The index of the last vertex.
     * @property {Graphics.MeshTopology} topology - The element interpretation. <em>Currently only triangles is supported.</em>
     * @property {number} numIndices - The number of vertex indices in the mesh part.
     * @property {number} numVertices - The number of vertices in the <em>whole</em> mesh  (i.e., parent and mesh parts).
     *     <em>Read-only.</em>
     * @property {number} numVerticesPerFace - The number of vertices per face, per the <code>topology</code> (e.g., 3 for 
     *     triangles).
     *     <em>Read-only.</em>
     * @property {number} numFaces - The number of faces represented by the mesh part.
     *     <em>Read-only.</em>
     * @property {number} numAttributes - The number of vertex attributes in the <em>whole</em> mesh  (i.e., parent and mesh 
     *     parts).
     *     <em>Read-only.</em>
     * @property {Graphics.BufferTypeName[]} attributeNames - The names of the vertex attributes in the <em>whole</em> mesh 
     *     (i.e., parent and mesh parts).
     *     <em>Read-only.</em>
     * @property {Graphics.MeshExtents} extents - The mesh part extents, in model coordinates.
     *     <em>Read-only.</em>
     * @property {Object<Graphics.BufferTypeName, Graphics.BufferFormat>} bufferFormats - Details of the buffers used for the 
     *     <em>whole</em> mesh (i.e., parent and mesh parts).
     *     <em>Read-only.</em>

     * @borrows GraphicsMesh.addAttribute as addAttribute
     * @borrows GraphicsMesh.getVertexAttributes as getVertextAttributes
     * @borrows GraphicsMesh.setVertexAttributes as setVertextAttributes
     */
    class ScriptableMeshPart : public QObject, Scriptable {
        Q_OBJECT
        Q_PROPERTY(bool valid READ isValid)
        Q_PROPERTY(glm::uint32 partIndex MEMBER partIndex CONSTANT)
        Q_PROPERTY(glm::uint32 firstVertexIndex READ getFirstVertexIndex WRITE setFirstVertexIndex)
        Q_PROPERTY(glm::uint32 baseVertexIndex READ getBaseVertexIndex WRITE setBaseVertexIndex)
        Q_PROPERTY(glm::uint32 lastVertexIndex READ getLastVertexIndex WRITE setLastVertexIndex)
        Q_PROPERTY(int numVerticesPerFace READ getTopologyLength)
        // NOTE: making read-only for now (see also GraphicsScriptingInterface::newMesh and MeshPartPayload::drawCall)
        Q_PROPERTY(graphics::Mesh::Topology topology READ getTopology)

        Q_PROPERTY(glm::uint32 numFaces READ getNumFaces)
        Q_PROPERTY(glm::uint32 numAttributes READ getNumAttributes)
        Q_PROPERTY(glm::uint32 numVertices READ getNumVertices)
        Q_PROPERTY(glm::uint32 numIndices READ getNumIndices WRITE setNumIndices)

        Q_PROPERTY(QVariantMap extents READ getPartExtents)
        Q_PROPERTY(QVector<QString> attributeNames READ getAttributeNames)
        Q_PROPERTY(QVariantMap bufferFormats READ getBufferFormats)

    public:
        ScriptableMeshPart(scriptable::ScriptableMeshPointer parentMesh, int partIndex);
        ScriptableMeshPart& operator=(const ScriptableMeshPart& view) { parentMesh=view.parentMesh; return *this; };
        ScriptableMeshPart(const ScriptableMeshPart& other) : QObject(other.parent()), Scriptable(), parentMesh(other.parentMesh), partIndex(other.partIndex) {}
        bool isValid() const { auto mesh = getMeshPointer(); return mesh && partIndex < mesh->getNumParts(); }

    public slots:

        /*@jsdoc
         * Gets the vertex indices.
         * @function GraphicsMeshPart.getIndices
         * @returns {number[]} The vertex indices.
         */
        QVector<glm::uint32> getIndices() const;
        
        /*@jsdoc
         * Sets the vertex indices.
         * @function GraphicsMeshPart.setIndices
         * @param {number[]} indices - The vertex indices.
         * @returns {boolean} <code>true</code> if successful, <code>false</code> if not.
         * @throws Throws an error if the number of indices isn't the same, or an index is invalid.
         */
        bool setIndices(const QVector<glm::uint32>& indices);

        /*@jsdoc
         * Gets the indices of nearby vertices in the mesh part.
         * @function GraphicsMeshPart.findNearbyPartVertexIndices
         * @param {Vec3} origin - The search position, in model coordinates.
         * @param {number} [epsilon=1e-6] - The search distance. If a vertex is within this distance from the
         *    <code>origin</code> it is considered to be "nearby".
         * @returns {number[]} The indices of nearby vertices.
         */
        QVector<glm::uint32> findNearbyPartVertexIndices(const glm::vec3& origin, float epsilon = 1e-6) const;

        /*@jsdoc
         * Gets the value of an attribute for all vertices in the <em>whole</em> mesh (i.e., parent and mesh parts).
         * @function GraphicsMeshPArt.queryVertexAttributes
         * @param {Graphics.BufferTypeName} name - The name of the attribute to get the vertex values of.
         * @throws Throws an error if the <code>name</code> is invalid or isn't used in the mesh.
         * @returns {Graphics.BufferType[]} The attribute values for all vertices.
         */
        QVariantList queryVertexAttributes(QVariant selector) const;

        // @borrows jsdoc from GraphicsMesh.
        QVariantMap getVertexAttributes(glm::uint32 vertexIndex) const;

        // @borrows jsdoc from GraphicsMesh.
        bool setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributeValues);

        /*@jsdoc
         * Gets the value of a vertex's attribute.
         * @function GraphicsMeshPart.getVertexProperty
         * @param {number} index - The vertex index.
         * @param {Graphics.BufferTypeName} name - The name of the vertex attribute to get.
         * @returns {Graphics.BufferType} The value of the vertex attribute.
         * @throws Throws an error if the <code>index</code> is invalid or <code>name</code> is invalid or isn't used in the
         *     mesh.
         */
        QVariant getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const;

        /*@jsdoc
         * Sets the value of a vertex's attribute.
         * @function GraphicsMeshPart.setVertexProperty
         * @param {number} index - The vertex index.
         * @param {Graphics.BufferTypeName} name - The name of the vertex attribute to set.
         * @param {Graphics.BufferType} value - The vertex attribute value to set.
         * @returns {boolean} <code>true</code> if the vertex attribute value was set, <code>false</code> if it wasn't.
         * @throws Throws an error if the <code>index</code> is invalid or <code>name</code> is invalid or isn't used in the
         *     mesh.
         */
        bool setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& attributeValues);

        /*@jsdoc
         * Gets the vertex indices that make up a face.
         * @function GraphicsMeshPart.getFace
         * @param {number} index - The index of the face.
         * @returns {number[]} The vertex indices that make up the face, of number per the mesh <code>topology</code>.
         */
        QVector<glm::uint32> getFace(glm::uint32 faceIndex) const;

        /*@jsdoc
         * Scales the mesh to so that it's maximum model coordinate dimension is a specified length.
         * @function GraphicsMeshPart.scaleToFit
         * @param {number} scale - The target dimension.
         * @returns {Graphics.MeshExtents} The resulting mesh extents, in model coordinates.
         */
        QVariantMap scaleToFit(float unitScale);

        /*@jsdoc
         * Translates the mesh part.
         * @function GraphicsMeshPart.translate
         * @param {Vec3} translation - The translation to apply, in model coordinates.
         * @returns {Graphics.MeshExtents} The rseulting mesh extents, in model coordinates.
         */
        QVariantMap translate(const glm::vec3& translation);

        /*@jsdoc
         * Scales the mesh part.
         * @function GraphicsMeshPart.scale
         * @param {Vec3} scale - The scale to apply in each model coordinate direction.
         * @param {Vec3} [origin] - The origin to scale about. If not specified, the center of the mesh part is used.
         * @returns {Graphics.MeshExtents} The resulting mesh extents, in model coordinates.
         */
        QVariantMap scale(const glm::vec3& scale, const glm::vec3& origin = glm::vec3(NAN));

        /*@jsdoc
         * Rotates the mesh part, using Euler angles.
         * @function GraphicsMeshPart.rotateDegrees
         * @param {Vec3} eulerAngles - The rotation to perform, in mesh coordinates, as Euler angles in degrees.
         * @param {Vec3} [origin] - The point about which to rotate, in model coordinates.
         *     <p><strong>Warning:</strong> Currently doesn't work as expected.</p>
         * @returns {Graphics.MeshExtents} The resulting mesh extents, in model coordinates.
         */
        QVariantMap rotateDegrees(const glm::vec3& eulerAngles, const glm::vec3& origin = glm::vec3(NAN));

        /*@jsdoc
         * Rotates the mesh part, using a quaternion.
         * @function GraphicsMeshPart.rotate
         * @param {Quat} rotation - The rotation to perform, in model coordinates.
         * @param {Vec3} [origin] - The point about which to rotate, in model coordinates.
         *     <p><strong>Warning:</strong> Currently doesn't work as expected.</p>
         * @returns {Graphics.MeshExtents} The resulting mesh extents, in model coordinates.
         */
        QVariantMap rotate(const glm::quat& rotation, const glm::vec3& origin = glm::vec3(NAN));

        /*@jsdoc
         * Scales, rotates, and translates the mesh.
         * @function GraphicsMeshPart.transform
         * @param {Mat4} transform - The scale, rotate, and translate transform to apply.
         * @returns {Graphics.MeshExtents} The resulting mesh extents, in model coordinates.
         */
        QVariantMap transform(const glm::mat4& transform);

        // @borrows jsdoc from GraphicsMesh.
        glm::uint32 addAttribute(const QString& attributeName, const QVariant& defaultValue = QVariant());

        /*@jsdoc
         * Sets the value of an attribute for all vertices in the <em>whole</em> mesh (i.e., parent and mesh parts).
         * @function GraphicsMeshPart.fillAttribute
         * @param {Graphics.BufferTypeName} name - The name of the attribute. The attribute is added to the vertices if not
         *     already present.
         * @param {Graphics.BufferType} value - The value to give the attributes.
         * @returns {number} <code>1</code> if the attribute name was valid and the attribute values were set, <code>0</code>
         *     otherwise.
         */
        glm::uint32 fillAttribute(const QString& attributeName, const QVariant& value);

        /*@jsdoc
         * Removes an attribute from all vertices in the <em>whole</em> mesh (i.e., parent and mesh parts).
         * <p>Note: The <code>"position"</code> attribute cannot be removed.</p>
         * @function GraphicsMeshPArt.removeAttribute
         * @param {Graphics.BufferTypeName} name - The name of the attribute to remove.
         * @returns {boolean} <code>true</code> if the attribute existed and was removed, <code>false</code> otherwise.
         */
        bool removeAttribute(const QString& attributeName);

        /*@jsdoc
         * Deduplicates vertices.
         * @function GraphicsMeshPart.dedupeVertices
         * @param {number} [epsilon=1e-6] - The deduplicadtion distance. If a pair of vertices is within this distance of each 
         *    other they are combined into a single vertex.
         * @returns {boolean} <code>true</code> if the deduplication succeeded, <code>false</code> if it didn't.
         */
        bool dedupeVertices(float epsilon = 1e-6);

        /*@jsdoc
         * Gets the parent mesh.
         * @function GraphicsMeshPart.getParentMesh
         * @returns {GraphicsMesh} The parent mesh.
         */
        scriptable::ScriptableMeshPointer getParentMesh() const { return parentMesh; }

        /*@jsdoc
         * Replaces a mesh part with a copy of another mesh part.
         * @function GraphicsMeshPart.replaceMeshPartData
         * @param {GrphicsMeshPart} source - The mesh part to copy.
         * @param {Graphics.BufferTypeName[]} [attributes] - The attributes to copy. If not specified, all attributes are 
         *     copied from the source.
         * @throws Throws an error if the mesh part of source mesh part aren't valid.
         * @returns {boolean} <code>true</code> if the mesh part was successfully replaced, <code>false</code> if it wasn't.
         */
        bool replaceMeshPartData(scriptable::ScriptableMeshPartPointer source, const QVector<QString>& attributeNames = QVector<QString>());
        
        /*@jsdoc
         * Makes a copy of the mesh part.
         * @function GraphicsMeshPart.cloneMeshPart
         * @returns {GraphicsMeshPart} A copy of the mesh part.
         */
        scriptable::ScriptableMeshPartPointer cloneMeshPart();

        /*@jsdoc
         * Exports the mesh part to OBJ format.
         * @function GraphicsMeshPart.toOBJ
         * @returns {string} The OBJ format representation of the mesh part.
         */
        QString toOBJ();


        // ScriptEngine-specific wrappers

        /*@jsdoc
         * Updates vertex attributes by calling a function for each vertex in the <em>whole</em> mesh (i.e., the parent and 
         * mesh parts). The function can return modified attributes to update the vertex with.
         * @function GraphicsMeshPart.updateVertexAttributes
         * @param {GraphicsMesh~updateVertexAttributesCallback} callback - The function to call for each vertex.
         * @returns {number} The number of vertices the callback was called for.
         */
        glm::uint32 updateVertexAttributes(ScriptValuePointer callback);

        /*@jsdoc
         * Calls a function for each vertex in the <em>whole</em> mesh (i.e., parent and mesh parts).
         * @function GraphicsMeshPArt.forEachVertex
         * @param {GraphicsMesh~forEachVertexCallback} callback - The function to call for each vertex.
         * @returns {number} The number of vertices the callback was called for.
         */
        glm::uint32 forEachVertex(ScriptValuePointer callback);

        /*@jsdoc
         * Checks if an index is valid and, optionally, that vertex has a particular attribute.
         * @function GraphicsMeshPart.isValidIndex
         * @param {number} index - The index to check.
         * @param {Graphics.BufferTypeName} [attribute] - The attribute to check.
         * @returns {boolean} <code>true</code> if the index is valid and that vertex has the attribute if specified.
         * @throws Throws an error if the <code>index</code> if invalid or <code>name</code> is invalid or isn't used in the
         *     mesh.
         */
        // FIXME: Should return false rather than throw an error.
        bool isValidIndex(glm::uint32 vertexIndex, const QString& attributeName = QString()) const;

    public:
        scriptable::ScriptableMeshPointer parentMesh;
        glm::uint32 partIndex;

    protected:
        const graphics::Mesh::Part& getMeshPart() const;
        scriptable::MeshPointer getMeshPointer() const { return parentMesh ? parentMesh->getMeshPointer() : nullptr; }
        QVariantMap getBufferFormats() { return isValid()  ? parentMesh->getBufferFormats() : QVariantMap(); }
        glm::uint32 getNumAttributes() const { return isValid() ? parentMesh->getNumAttributes() : 0; }

        bool setTopology(graphics::Mesh::Topology topology);
        graphics::Mesh::Topology getTopology() const { return isValid() ? getMeshPart()._topology : graphics::Mesh::Topology(); }
        glm::uint32 getTopologyLength() const;
        glm::uint32 getNumIndices() const { return isValid() ? getMeshPart()._numIndices : 0; }
        bool setNumIndices(glm::uint32 numIndices) { return setLastVertexIndex(getFirstVertexIndex() + numIndices); }
        glm::uint32 getNumVertices() const { return isValid() ? parentMesh->getNumVertices() : 0; }

        bool setFirstVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getFirstVertexIndex() const { return isValid() ? getMeshPart()._startIndex : 0; }
        bool setLastVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getLastVertexIndex() const { return isValid() ? getFirstVertexIndex() + getNumIndices() - 1 : 0; }
        bool setBaseVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getBaseVertexIndex() const { return isValid() ? getMeshPart()._baseVertex : 0; }

        glm::uint32 getNumFaces() const { return getNumIndices() / getTopologyLength(); }
        QVector<QString> getAttributeNames() const { return isValid() ? parentMesh->getAttributeNames() : QVector<QString>(); }
        QVariantMap getPartExtents() const;
    };
}

Q_DECLARE_METATYPE(scriptable::ScriptableMeshPartPointer)
Q_DECLARE_METATYPE(QVector<scriptable::ScriptableMeshPartPointer>)
