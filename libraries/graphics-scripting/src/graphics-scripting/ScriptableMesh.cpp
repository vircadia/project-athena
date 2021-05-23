//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptableMesh.h"

#include <ScriptValue.h>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <graphics/Geometry.h>
#include <RegisteredMetaTypes.h>
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptValue.h>

#include "Forward.h"
#include "ScriptableMeshPart.h"
#include "GraphicsScriptingUtil.h"
#include "OBJWriter.h"

// #define SCRIPTABLE_MESH_DEBUG 1

scriptable::ScriptableMesh::ScriptableMesh(const ScriptableMeshBase& other)
    : ScriptableMeshBase(other), Scriptable() {
    auto mesh = getMeshPointer();
    QString name = mesh ? QString::fromStdString(mesh->modelName) : "";
    if (name.isEmpty()) {
        name = mesh ? QString::fromStdString(mesh->displayName) : "";
    }
    auto parentModel = getParentModel();
    setObjectName(QString("%1#%2").arg(parentModel ? parentModel->objectName() : "").arg(name));
}

QVector<scriptable::ScriptableMeshPartPointer> scriptable::ScriptableMesh::getMeshParts() const {
    QVector<scriptable::ScriptableMeshPartPointer> out;
    for (glm::uint32 i = 0; i < getNumParts(); i++) {
        out << scriptable::make_scriptowned<scriptable::ScriptableMeshPart>(getSelf(), i);
    }
    return out;
}

glm::uint32 scriptable::ScriptableMesh::getNumIndices() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumIndices();
    }
    return 0;
}

glm::uint32 scriptable::ScriptableMesh::getNumVertices() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumVertices();
    }
    return 0;
}

QVector<glm::uint32> scriptable::ScriptableMesh::findNearbyVertexIndices(const glm::vec3& origin, float epsilon) const {
    QVector<glm::uint32> result;
    if (!isValid()) {
        return result;
    }
    const auto epsilon2 = epsilon*epsilon;
    buffer_helpers::forEach<glm::vec3>(buffer_helpers::mesh::getBufferView(getMeshPointer(), gpu::Stream::POSITION), [&](glm::uint32 index, const glm::vec3& position) {
        if (glm::length2(position - origin) <= epsilon2) {
            result << index;
        }
        return true;
    });
    return result;
}

QVector<glm::uint32> scriptable::ScriptableMesh::getIndices() const {
    if (auto mesh = getMeshPointer()) {
#ifdef SCRIPTABLE_MESH_DEBUG
        qCDebug(graphics_scripting, "getIndices mesh %p", mesh.get());
#endif
        return buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer());
    }
    return QVector<glm::uint32>();
}


glm::uint32 scriptable::ScriptableMesh::getNumAttributes() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumAttributes() + 1;
    }
    return 0;
}
QVector<QString> scriptable::ScriptableMesh::getAttributeNames() const {
    QVector<QString> result;
    if (auto mesh = getMeshPointer()) {
        for (const auto& a : buffer_helpers::ATTRIBUTES.toStdMap()) {
            auto bufferView = buffer_helpers::mesh::getBufferView(mesh, a.second);
            if (bufferView.getNumElements() > 0) {
                result << a.first;
            }
        }
    }
    return result;
}

QVariantMap scriptable::ScriptableMesh::getVertexAttributes(glm::uint32 vertexIndex) const {
    if (!isValidIndex(vertexIndex)) {
        return QVariantMap();
    }
    return buffer_helpers::mesh::getVertexAttributes(getMeshPointer(), vertexIndex).toMap();
}

bool scriptable::ScriptableMesh::setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributes) {
    for (const auto& name : attributes.keys()) {
        if (!isValidIndex(vertexIndex, name)) {
            return false;
        }
    }
    return buffer_helpers::mesh::setVertexAttributes(getMeshPointer(), vertexIndex, attributes);
}

int scriptable::ScriptableMesh::getSlotNumber(const QString& attributeName) const {
    if (auto mesh = getMeshPointer()) {
        return buffer_helpers::ATTRIBUTES.value(attributeName, -1);
    }
    return -1;
}

/*@jsdoc
 * Details of buffer's format.
 * @typedef {object} Graphics.BufferFormat
 * @property {number} slot - Slot.
 * @property {number} length - Length.
 * @property {number} byteLength - Byte length.
 * @property {number} offset - Offset.
 * @property {number} stride - Stride.
 * @property {Graphics.BufferElementFormat} element - Element format.
 */
QVariantMap scriptable::ScriptableMesh::getBufferFormats() const {
    QVariantMap result;
    for (const auto& a : buffer_helpers::ATTRIBUTES.toStdMap()) {
        auto bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), a.second);
        result[a.first] = QVariantMap{
            { "slot", a.second },
            { "length", (glm::uint32)bufferView.getNumElements() },
            { "byteLength", (glm::uint32)bufferView._size },
            { "offset", (glm::uint32) bufferView._offset },
            { "stride", (glm::uint32)bufferView._stride },
            { "element", scriptable::toVariant(bufferView._element) },
        };
    }
    return result;
}

bool scriptable::ScriptableMesh::removeAttribute(const QString& attributeName) {
    auto slot = isValid() ? getSlotNumber(attributeName) : -1;
    if (slot < 0) {
        return false;
    }
    if (slot == gpu::Stream::POSITION) {
        context()->throwError("cannot remove .position attribute");
        return false;
    }
    if (buffer_helpers::mesh::getBufferView(getMeshPointer(), slot).getNumElements()) {
        getMeshPointer()->removeAttribute(slot);
        return true;
    }
    return false;
}

glm::uint32 scriptable::ScriptableMesh::addAttribute(const QString& attributeName, const QVariant& defaultValue) {
    auto slot = isValid() ? getSlotNumber(attributeName) : -1;
    if (slot < 0) {
        return 0;
    }
    auto mesh = getMeshPointer();
    auto numVertices = getNumVertices();
    if (!getAttributeNames().contains(attributeName)) {
        QVector<QVariant> values;
        values.fill(defaultValue, numVertices);
        mesh->addAttribute(slot, buffer_helpers::newFromVector(values, gpu::Stream::getDefaultElements()[slot]));
        return values.size();
    } else {
        auto bufferView = buffer_helpers::mesh::getBufferView(mesh, slot);
        auto current = (glm::uint32)bufferView.getNumElements();
        if (current < numVertices) {
            bufferView = buffer_helpers::resized(bufferView, numVertices);
            for (glm::uint32 i = current; i < numVertices; i++) {
                buffer_helpers::setValue<QVariant>(bufferView, i, defaultValue);
            }
            return numVertices - current;
        } else if (current > numVertices) {
            qCDebug(graphics_scripting) << QString("current=%1 > numVertices=%2").arg(current).arg(numVertices);
            return 0;
        }
    }
    return 0;
}

glm::uint32 scriptable::ScriptableMesh::fillAttribute(const QString& attributeName, const QVariant& value) {
    auto slot = isValid() ? getSlotNumber(attributeName) : -1;
    if (slot < 0) {
        return 0;
    }
    auto mesh = getMeshPointer();
    auto numVertices = getNumVertices();
    QVector<QVariant> values;
    values.fill(value, numVertices);
    mesh->addAttribute(slot, buffer_helpers::newFromVector(values, gpu::Stream::getDefaultElements()[slot]));
    return true;
}

QVariantMap scriptable::ScriptableMesh::getMeshExtents() const {
    auto mesh = getMeshPointer();
    auto box = mesh ? mesh->evalPartsBound(0, (int)mesh->getNumParts()) : AABox();
    return scriptable::toVariant(box).toMap();
}

glm::uint32 scriptable::ScriptableMesh::getNumParts() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumParts();
    }
    return 0;
}

QVariantList scriptable::ScriptableMesh::queryVertexAttributes(QVariant selector) const {
    QVariantList result;
    const auto& attributeName = selector.toString();
    if (!isValidIndex(0, attributeName)) {
        return result;
    }
    auto slotNum = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(slotNum));
    glm::uint32 numElements = (glm::uint32)bufferView.getNumElements();
    for (glm::uint32 i = 0; i < numElements; i++) {
        result << buffer_helpers::getValue<QVariant>(bufferView, i, qUtf8Printable(attributeName));
    }
    return result;
}

QVariant scriptable::ScriptableMesh::getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return QVariant();
    }
    auto slotNum = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(slotNum));
    return buffer_helpers::getValue<QVariant>(bufferView, vertexIndex, qUtf8Printable(attributeName));
}

bool scriptable::ScriptableMesh::setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& value) {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return false;
    }
    auto slotNum = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(slotNum));
    return buffer_helpers::setValue(bufferView, vertexIndex, value);
}

/*@jsdoc
 * Called for each vertex when {@link GraphicsMesh.updateVertexAttributes} is called.
 * @callback GraphicsMesh~forEachVertextCallback
 * @param {Object<Graphics.BufferTypeName, Graphics.BufferType>} attributes - The attributes  of the vertex.
 * @param {number} index - The vertex index.
 * @param {object} properties - The properties of the mesh, per {@link GraphicsMesh}.
 */
glm::uint32 scriptable::ScriptableMesh::forEachVertex(ScriptValuePointer _callback) {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return 0;
    }
    auto scopedHandler = jsBindCallback(_callback);

    // destructure so we can still invoke callback scoped, but with a custom signature (obj, i, jsMesh)
    auto scope = scopedHandler->property("scope");
    auto callback = scopedHandler->property("callback");
    auto js = engine() ? engine() : scopedHandler->engine(); // cache value to avoid resolving each iteration
    if (!js) {
        return 0;
    }
    auto meshPart = js ? js->toScriptValue(getSelf()) : js->nullValue();
    int numProcessed = 0;
    buffer_helpers::mesh::forEachVertex(mesh, [&](glm::uint32 index, const QVariantMap& values) {
        auto result = callback->call(scope, { js->toScriptValue(values), js->newValue(index), meshPart });
        if (js->hasUncaughtException()) {
            js->currentContext()->throwValue(js->uncaughtException());
            return false;
        }
        numProcessed++;
        return true;
    });
    return numProcessed;
}

/*@jsdoc
 * Called for each vertex when {@link GraphicsMesh.updateVertexAttributes} is called. The value returned by the script function 
 * should be the modified attributes to update the vertex with, or <code>false</code> to not update the particular vertex.
 * @callback GraphicsMesh~updateVertexAttributesCallback
 * @param {Object<Graphics.BufferTypeName, Graphics.BufferType>} attributes - The attributes  of the vertex.
 * @param {number} index - The vertex index.
 * @param {object} properties - The properties of the mesh, per {@link GraphicsMesh}.
 * @returns {Object<Graphics.BufferTypeName, Graphics.BufferType>|boolean} The attribute values to update the vertex with, or 
 *     <code>false</code> to not update the vertex.
 */
glm::uint32 scriptable::ScriptableMesh::updateVertexAttributes(ScriptValuePointer _callback) {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return 0;
    }
    auto scopedHandler = jsBindCallback(_callback);

    // destructure so we can still invoke callback scoped, but with a custom signature (obj, i, jsMesh)
    auto scope = scopedHandler->property("scope");
    auto callback = scopedHandler->property("callback");
    auto js = engine() ? engine() : scopedHandler->engine(); // cache value to avoid resolving each iteration
    if (!js) {
        return 0;
    }
    auto meshPart = js ? js->toScriptValue(getSelf()) : js->nullValue();
    int numProcessed = 0;
    auto attributeViews = buffer_helpers::mesh::getAllBufferViews(mesh);
    buffer_helpers::mesh::forEachVertex(mesh, [&](glm::uint32 index, const QVariantMap& values) {
        auto obj = js->toScriptValue(values);
        auto result = callback->call(scope, { obj, js->newValue(index), meshPart });
        if (js->hasUncaughtException()) {
            js->currentContext()->throwValue(js->uncaughtException());
            return false;
        }
        if (result->isBool() && !result->toBool()) {
            // bail without modifying data if user explicitly returns false
            return true;
        }
        if (result->isObject() && !result->strictlyEquals(obj)) {
            // user returned a new object (ie: instead of modifying input properties)
            obj = result;
        }
        for (const auto& a : attributeViews) {
            const auto& attribute = obj->property(a.first);
            if (attribute->isValid()) {
                buffer_helpers::setValue(a.second, index, attribute->toVariant());
            }
        }
        numProcessed++;
        return true;
    });
    return numProcessed;
}

// protect against user scripts sending bogus values
bool scriptable::ScriptableMesh::isValidIndex(glm::uint32 vertexIndex, const QString& attributeName) const {
    if (!isValid()) {
        return false;
    }
    const auto last = getNumVertices() - 1;
    if (vertexIndex > last) {
        if (context()) {
            context()->throwError(QString("vertexIndex=%1 out of range (firstVertexIndex=%2, lastVertexIndex=%3)").arg(vertexIndex).arg(0).arg(last));
        }
        return false;
    }
    if (!attributeName.isEmpty()) {
        auto slotNum = getSlotNumber(attributeName);
        if (slotNum < 0) {
            if (context()) {
                context()->throwError(QString("invalid attributeName=%1").arg(attributeName));
            }
            return false;
        }
        auto view = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(slotNum));
        if (vertexIndex >= (glm::uint32)view.getNumElements()) {
            if (context()) {
                context()->throwError(QString("vertexIndex=%1 out of range (attribute=%2, numElements=%3)").arg(vertexIndex).arg(attributeName).arg(view.getNumElements()));
            }
            return false;
        }
    }
    return true;
}


scriptable::ScriptableMeshPointer scriptable::ScriptableMesh::cloneMesh() {
    auto mesh = getMeshPointer();
    if (!mesh) {
        qCInfo(graphics_scripting) << "ScriptableMesh::cloneMesh -- !meshPointer";
        return nullptr;
    }
    auto clone = buffer_helpers::mesh::clone(mesh);

    auto meshPointer = scriptable::make_scriptowned<scriptable::ScriptableMesh>(provider, model, clone, nullptr);
    return scriptable::ScriptableMeshPointer(meshPointer);
}

// note: we don't always want the JS side to prevent mesh data from being freed (hence weak pointers unless parented QObject)
scriptable::ScriptableMeshBase::ScriptableMeshBase(
    scriptable::WeakModelProviderPointer provider, scriptable::ScriptableModelBasePointer model, scriptable::WeakMeshPointer weakMesh, QObject* parent
    ) : QObject(parent), provider(provider), model(model), weakMesh(weakMesh) {
    if (parent) {
#ifdef SCRIPTABLE_MESH_DEBUG
        qCDebug(graphics_scripting) << "ScriptableMeshBase -- have parent QObject, creating strong neshref" << weakMesh.lock().get() << parent;
#endif
        strongMesh = weakMesh.lock();
    }
}

scriptable::ScriptableMeshBase::ScriptableMeshBase(scriptable::WeakMeshPointer weakMesh, QObject* parent) :
    scriptable::ScriptableMeshBase(scriptable::WeakModelProviderPointer(), nullptr, weakMesh, parent) {
}

scriptable::ScriptableMeshBase& scriptable::ScriptableMeshBase::operator=(const scriptable::ScriptableMeshBase& view) {
    provider = view.provider;
    model = view.model;
    weakMesh = view.weakMesh;
    strongMesh = view.strongMesh;
    return *this;
}

scriptable::ScriptableMeshBase::~ScriptableMeshBase() {
#ifdef SCRIPTABLE_MESH_DEBUG
    qCInfo(graphics_scripting) << "//~ScriptableMeshBase" << this << "strongMesh:"  << strongMesh.use_count() << "weakMesh:" << weakMesh.use_count();
#endif
    strongMesh.reset();
}

scriptable::ScriptableMesh::~ScriptableMesh() {
#ifdef SCRIPTABLE_MESH_DEBUG
    qCInfo(graphics_scripting) << "//~ScriptableMesh" << this << "strongMesh:"  << strongMesh.use_count() << "weakMesh:" << weakMesh.use_count();
#endif
    strongMesh.reset();
}
