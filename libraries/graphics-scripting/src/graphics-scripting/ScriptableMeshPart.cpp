//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptableMeshPart.h"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include <ScriptValue.h>
#include <RegisteredMetaTypes.h>
#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <graphics/Geometry.h>

#include "Forward.h"
#include "GraphicsScriptingUtil.h"
#include "OBJWriter.h"


QString scriptable::ScriptableMeshPart::toOBJ() {
    if (!getMeshPointer()) {
        if (context()) {
            context()->throwError(QString("null mesh"));
        } else {
            qCWarning(graphics_scripting) << "null mesh";
        }
        return QString();
    }
    return writeOBJToString({ getMeshPointer() });
}


bool scriptable::ScriptableMeshPart::isValidIndex(glm::uint32 vertexIndex, const QString& attributeName) const {
    return isValid() && parentMesh->isValidIndex(vertexIndex, attributeName);
}

bool scriptable::ScriptableMeshPart::setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributes) {
    if (!isValidIndex(vertexIndex)) {
        return false;
    }
    return buffer_helpers::mesh::setVertexAttributes(getMeshPointer(), vertexIndex, attributes);
}

QVariantMap scriptable::ScriptableMeshPart::getVertexAttributes(glm::uint32 vertexIndex) const {
    if (!isValidIndex(vertexIndex)) {
        return QVariantMap();
    }
    return parentMesh->getVertexAttributes(vertexIndex);
}

bool scriptable::ScriptableMeshPart::setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& value) {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return false;
    }
    auto slotNum = parentMesh->getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(slotNum));
    return buffer_helpers::setValue(bufferView, vertexIndex, value);
}

QVariant scriptable::ScriptableMeshPart::getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return false;
    }
    return parentMesh->getVertexProperty(vertexIndex, attributeName);
}

QVariantList scriptable::ScriptableMeshPart::queryVertexAttributes(QVariant selector) const {
    QVariantList result;
    if (!isValid()) {
        return result;
    }
    return parentMesh->queryVertexAttributes(selector);
}

glm::uint32 scriptable::ScriptableMeshPart::forEachVertex(ScriptValuePointer _callback) {
    // TODO: limit to vertices within the part's indexed range?
    return isValid() ? parentMesh->forEachVertex(_callback) : 0;
}

glm::uint32 scriptable::ScriptableMeshPart::updateVertexAttributes(ScriptValuePointer _callback) {
    // TODO: limit to vertices within the part's indexed range?
    return isValid() ? parentMesh->updateVertexAttributes(_callback) : 0;
}

bool scriptable::ScriptableMeshPart::replaceMeshPartData(scriptable::ScriptableMeshPartPointer src, const QVector<QString>& attributeNames) {
    auto target = getMeshPointer();
    auto source = src ? src->getMeshPointer() : nullptr;
    if (!target || !source) {
        if (context()) {
            context()->throwError("ScriptableMeshPart::replaceMeshData -- expected dest and src to be valid mesh proxy pointers");
        } else {
            qCWarning(graphics_scripting) << "ScriptableMeshPart::replaceMeshData -- expected dest and src to be valid mesh proxy pointers";
        }
        return false;
    }

    QVector<QString> attributes = attributeNames.isEmpty() ? src->parentMesh->getAttributeNames() : attributeNames;

    qCInfo(graphics_scripting) << "ScriptableMeshPart::replaceMeshData -- " <<
        "source:" << QString::fromStdString(source->displayName) <<
        "target:" << QString::fromStdString(target->displayName) <<
        "attributes:" << attributes;

    // remove attributes only found on target mesh, unless user has explicitly specified the relevant attribute names
    if (attributeNames.isEmpty()) {
        auto attributeViews = buffer_helpers::mesh::getAllBufferViews(target);
        for (const auto& a : attributeViews) {
            auto slot = buffer_helpers::ATTRIBUTES[a.first];
            if (!attributes.contains(a.first)) {
#ifdef SCRIPTABLE_MESH_DEBUG
                qCInfo(graphics_scripting) << "ScriptableMesh::replaceMeshData -- pruning target attribute" << a.first << slot;
#endif
                target->removeAttribute(slot);
            }
        }
    }

    target->setVertexBuffer(buffer_helpers::clone(source->getVertexBuffer()));
    target->setIndexBuffer(buffer_helpers::clone(source->getIndexBuffer()));
    target->setPartBuffer(buffer_helpers::clone(source->getPartBuffer()));

    for (const auto& a : attributes) {
        auto slot = buffer_helpers::ATTRIBUTES[a];
        if (slot == gpu::Stream::POSITION) {
            continue;
        }
#ifdef SCRIPTABLE_MESH_DEBUG
        auto& before = target->getAttributeBuffer(slot);
#endif
        auto& input = source->getAttributeBuffer(slot);
        if (input.getNumElements() == 0) {
#ifdef SCRIPTABLE_MESH_DEBUG
            qCInfo(graphics_scripting) << "ScriptableMeshPart::replaceMeshData buffer is empty -- pruning" << a << slot;
#endif
            target->removeAttribute(slot);
        } else {
#ifdef SCRIPTABLE_MESH_DEBUG
            if (before.getNumElements() == 0) {
                qCInfo(graphics_scripting) << "ScriptableMeshPart::replaceMeshData target buffer is empty -- adding" << a << slot;
            } else {
                qCInfo(graphics_scripting) << "ScriptableMeshPart::replaceMeshData target buffer exists -- updating" << a << slot;
            }
#endif
            target->addAttribute(slot, buffer_helpers::clone(input));
        }
#ifdef SCRIPTABLE_MESH_DEBUG
        auto& after = target->getAttributeBuffer(slot);
        qCInfo(graphics_scripting) << "ScriptableMeshPart::replaceMeshData" << a << slot << before.getNumElements() << " -> " << after.getNumElements();
#endif
    }


    return true;
}

bool scriptable::ScriptableMeshPart::dedupeVertices(float epsilon) {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return false;
    }
    auto positions = mesh->getVertexBuffer();
    auto numPositions = positions.getNumElements();
    const auto epsilon2 = epsilon*epsilon;

    QVector<glm::vec3> uniqueVerts;
    uniqueVerts.reserve((int)numPositions);
    QMap<glm::uint32,glm::uint32> remapIndices;

    for (glm::uint32 i = 0; i < numPositions; i++) {
        const glm::uint32 numUnique = uniqueVerts.size();
        const auto& position = positions.get<glm::vec3>(i);
        bool unique = true;
        for (glm::uint32 j = 0; j < numUnique; j++) {
            if (glm::length2(uniqueVerts[j] - position) <= epsilon2) {
                remapIndices[i] = j;
                unique = false;
                break;
            }
        }
        if (unique) {
            uniqueVerts << position;
            remapIndices[i] = numUnique;
        }
    }

    qCInfo(graphics_scripting) << "//VERTS before" << numPositions << "after" << uniqueVerts.size();

    auto indices = mesh->getIndexBuffer();
    auto numIndices = indices.getNumElements();
    auto esize = indices._element.getSize();
    QVector<glm::uint32> newIndices;
    newIndices.reserve((int)numIndices);
    for (glm::uint32 i = 0; i < numIndices; i++) {
        glm::uint32 index = esize == 4 ? indices.get<glm::uint32>(i) : indices.get<quint16>(i);
        if (remapIndices.contains(index)) {
            newIndices << remapIndices[index];
        } else {
            qCInfo(graphics_scripting) << i << index << "!remapIndices[index]";
        }
    }

    mesh->setIndexBuffer(buffer_helpers::newFromVector(newIndices, { gpu::SCALAR, gpu::UINT32, gpu::INDEX }));
    mesh->setVertexBuffer(buffer_helpers::newFromVector(uniqueVerts, gpu::Element::VEC3F_XYZ));

    auto attributeViews = buffer_helpers::mesh::getAllBufferViews(mesh);
    glm::uint32 numUniqueVerts = uniqueVerts.size();
    for (const auto& a : attributeViews) {
        auto& view = a.second;
        auto slot = buffer_helpers::ATTRIBUTES[a.first];
        if (slot == gpu::Stream::POSITION) {
            continue;
        }
        auto newView = buffer_helpers::resized(view, numUniqueVerts);
#ifdef SCRIPTABLE_MESH_DEBUG
        qCInfo(graphics_scripting) << "ScriptableMeshPart::dedupeVertices" << a.first << slot << view.getNumElements();
        qCInfo(graphics_scripting) << a.first << "before: #" << view.getNumElements() << "after: #" << newView.getNumElements();
#endif
        glm::uint32 numElements = (glm::uint32)view.getNumElements();
        for (glm::uint32 i = 0; i < numElements; i++) {
            glm::uint32 fromVertexIndex = i;
            glm::uint32 toVertexIndex = remapIndices.contains(fromVertexIndex) ? remapIndices[fromVertexIndex] : fromVertexIndex;
            buffer_helpers::setValue<QVariant>(newView, toVertexIndex, buffer_helpers::getValue<QVariant>(view, fromVertexIndex, "dedupe"));
        }
        mesh->addAttribute(slot, newView);
    }
    return true;
}

bool scriptable::ScriptableMeshPart::removeAttribute(const QString& attributeName) {
    return isValid() && parentMesh->removeAttribute(attributeName);
}

glm::uint32 scriptable::ScriptableMeshPart::addAttribute(const QString& attributeName, const QVariant& defaultValue) {
    return isValid() ? parentMesh->addAttribute(attributeName, defaultValue): 0;
}

glm::uint32 scriptable::ScriptableMeshPart::fillAttribute(const QString& attributeName, const QVariant& value) {
    return isValid() ? parentMesh->fillAttribute(attributeName, value) : 0;
}

QVector<glm::uint32> scriptable::ScriptableMeshPart::findNearbyPartVertexIndices(const glm::vec3& origin, float epsilon) const {
    QSet<glm::uint32> result;
    if (!isValid()) {
        return result.toList().toVector();
    }
    auto mesh = getMeshPointer();
    auto offset = getFirstVertexIndex();
    auto numIndices = getNumIndices();
    auto vertexBuffer = mesh->getVertexBuffer();
    auto indexBuffer = mesh->getIndexBuffer();
    const auto epsilon2 = epsilon*epsilon;

    for (glm::uint32 i = 0; i < numIndices; i++) {
        auto vertexIndex = buffer_helpers::getValue<glm::uint32>(indexBuffer, offset + i);
        if (result.contains(vertexIndex)) {
            continue;
        }
        const auto& position = buffer_helpers::getValue<glm::vec3>(vertexBuffer, vertexIndex);
        if (glm::length2(position - origin) <= epsilon2) {
            result << vertexIndex;
        }
    }
    return result.toList().toVector();
}

scriptable::ScriptableMeshPartPointer scriptable::ScriptableMeshPart::cloneMeshPart() {
    if (parentMesh) {
        if (auto clone = parentMesh->cloneMesh()) {
            return clone->getMeshParts().value(partIndex);
        }
    }
    return nullptr;
}

QVariantMap scriptable::ScriptableMeshPart::scaleToFit(float unitScale) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        auto center = box.calcCenter();
        float maxDimension = glm::distance(box.getMaximumPoint(), box.getMinimumPoint());
        return scale(glm::vec3(unitScale / maxDimension), center);
    }
    return {};
}
QVariantMap scriptable::ScriptableMeshPart::translate(const glm::vec3& translation) {
    return transform(glm::translate(translation));
}
QVariantMap scriptable::ScriptableMeshPart::scale(const glm::vec3& scale, const glm::vec3& origin) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        glm::vec3 center = glm::isnan(origin.x) ? box.calcCenter() : origin;
        return transform(glm::translate(center) * glm::scale(scale));
    }
    return {};
}
QVariantMap scriptable::ScriptableMeshPart::rotateDegrees(const glm::vec3& eulerAngles, const glm::vec3& origin) {
    return rotate(glm::quat(glm::radians(eulerAngles)), origin);
}
QVariantMap scriptable::ScriptableMeshPart::rotate(const glm::quat& rotation, const glm::vec3& origin) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        glm::vec3 center = glm::isnan(origin.x) ? box.calcCenter() : origin;
        return transform(glm::translate(center) * glm::toMat4(rotation));
    }
    return {};
}
QVariantMap scriptable::ScriptableMeshPart::transform(const glm::mat4& transform) {
    if (auto mesh = getMeshPointer()) {
        const auto& pos = buffer_helpers::mesh::getBufferView(mesh, gpu::Stream::POSITION);
        const glm::uint32 num = (glm::uint32)pos.getNumElements();
        for (glm::uint32 i = 0; i < num; i++) {
            auto& position = pos.edit<glm::vec3>(i);
            position = transform * glm::vec4(position, 1.0f);
        }
        return parentMesh->getMeshExtents();
    }
    return {};
}


scriptable::ScriptableMeshPart::ScriptableMeshPart(scriptable::ScriptableMeshPointer parentMesh, int partIndex)
    : QObject(), parentMesh(parentMesh), partIndex(partIndex)  {
    setObjectName(QString("%1.part[%2]").arg(parentMesh ? parentMesh->objectName() : "").arg(partIndex));
}

QVector<glm::uint32> scriptable::ScriptableMeshPart::getIndices() const {
    if (auto mesh = getMeshPointer()) {
#ifdef SCRIPTABLE_MESH_DEBUG
        qCDebug(graphics_scripting, "getIndices mesh %p", mesh.get());
#endif
        return buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer());
    }
    return QVector<glm::uint32>();
}

bool scriptable::ScriptableMeshPart::setFirstVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex)) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._startIndex = vertexIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setBaseVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex)) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._baseVertex = vertexIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setLastVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex) || vertexIndex <= getFirstVertexIndex()) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._numIndices = vertexIndex - part._startIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setIndices(const QVector<glm::uint32>& indices) {
    if (!isValid()) {
        return false;
    }
    glm::uint32 len = indices.size();
    if (len != getNumIndices()) {
        context()->throwError(QString("setIndices: currently new indicies must be assign 1:1 across old indicies (indicies.size()=%1, numIndices=%2)")
                              .arg(len).arg(getNumIndices()));
        return false;
    }
    auto mesh = getMeshPointer();
    auto indexBuffer = mesh->getIndexBuffer();

    // first loop to validate all indices are valid
    for (glm::uint32 i = 0; i < len; i++) {
        if (!isValidIndex(indices.at(i))) {
            return false;
        }
    }
    const auto first = getFirstVertexIndex();
    // now actually apply them
    for (glm::uint32 i = 0; i < len; i++) {
        buffer_helpers::setValue(indexBuffer, first + i, indices.at(i));
    }
    return true;
}

const graphics::Mesh::Part& scriptable::ScriptableMeshPart::getMeshPart() const {
    static const graphics::Mesh::Part invalidPart;
    if (!isValid()) {
        return invalidPart;
    }
    return getMeshPointer()->getPartBuffer().get<graphics::Mesh::Part>(partIndex);
}

// FIXME: how we handle topology will need to be reworked if wanting to support TRIANGLE_STRIP, QUADS and QUAD_STRIP
bool scriptable::ScriptableMeshPart::setTopology(graphics::Mesh::Topology topology) {
    if (!isValid()) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    switch (topology) {
#ifdef DEV_BUILD
    case graphics::Mesh::Topology::POINTS:
    case graphics::Mesh::Topology::LINES:
#endif
    case graphics::Mesh::Topology::TRIANGLES:
        part._topology = topology;
        return true;
    default:
        context()->throwError("changing topology to " + graphics::toString(topology) + " is not yet supported");
        return false;
    }
}

glm::uint32 scriptable::ScriptableMeshPart::getTopologyLength() const {
    switch(getTopology()) {
    case graphics::Mesh::Topology::POINTS: return 1;
    case graphics::Mesh::Topology::LINES: return 2;
    case graphics::Mesh::Topology::TRIANGLES: return 3;
    case graphics::Mesh::Topology::QUADS: return 4;
    default: qCDebug(graphics_scripting) << "getTopologyLength -- unrecognized topology" << getTopology();
    }
    return 0;
}

QVector<glm::uint32> scriptable::ScriptableMeshPart::getFace(glm::uint32 faceIndex) const {
    switch (getTopology()) {
    case graphics::Mesh::Topology::POINTS:
    case graphics::Mesh::Topology::LINES:
    case graphics::Mesh::Topology::TRIANGLES:
    case graphics::Mesh::Topology::QUADS:
        if (faceIndex < getNumFaces()) {
            return getIndices().mid(faceIndex * getTopologyLength(), getTopologyLength());
        }
    /* fall-thru */
    default: return QVector<glm::uint32>();
    }
}

QVariantMap scriptable::ScriptableMeshPart::getPartExtents() const {
    graphics::Box box;
    if (auto mesh = getMeshPointer()) {
        box = mesh->evalPartBound(partIndex);
    }
    return scriptable::toVariant(box).toMap();
}
