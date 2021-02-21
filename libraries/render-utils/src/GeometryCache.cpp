//
//  GeometryCache.cpp
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 6/21/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GeometryCache.h"

#include <qmath.h>
#include <cmath>

#include <QtCore/QThreadPool>
#include <QtCore/QFileInfo>
#include <QtNetwork/QNetworkReply>

#include <shared/Shapes.h>
#include <shared/PlatformHacks.h>

#include <FSTReader.h>
#include <NumericalConstants.h>
#include <graphics/TextureMap.h>
#include <graphics/BufferViewHelpers.h>
#include <render/Args.h>
#include <shaders/Shaders.h>
#include <graphics/ShaderConstants.h>

#include "render-utils/ShaderConstants.h"
#include "TextureCache.h"
#include "RenderUtilsLogging.h"
#include "StencilMaskPass.h"
#include "FadeEffect.h"

#include "DeferredLightingEffect.h"

namespace gr {
    using graphics::slot::texture::Texture;
    using graphics::slot::buffer::Buffer;
}

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

//#define WANT_DEBUG

// @note: Originally size entity::NUM_SHAPES
//        As of Commit b93e91b9, render-utils no longer retains knowledge of
//        entity lib, and thus doesn't know about entity::NUM_SHAPES.  Should
//        the enumerations be altered, this will need to be updated.
// @see ShapeEntityItem.h
static std::array<GeometryCache::Shape, (GeometryCache::NUM_SHAPES - 1)> MAPPING{ {
        GeometryCache::Triangle,
        GeometryCache::Quad,
        GeometryCache::Hexagon,
        GeometryCache::Octagon,
        GeometryCache::Circle,
        GeometryCache::Cube,
        GeometryCache::Sphere,
        GeometryCache::Tetrahedron,
        GeometryCache::Octahedron,
        GeometryCache::Dodecahedron,
        GeometryCache::Icosahedron,
        GeometryCache::Torus,
        GeometryCache::Cone,
        GeometryCache::Cylinder,
} };

static const std::array<const char * const, GeometryCache::NUM_SHAPES> GEOCACHE_SHAPE_STRINGS{ {
        "Line",
        "Triangle",
        "Quad",
        "Hexagon",
        "Octagon",
        "Circle",
        "Cube",
        "Sphere",
        "Tetrahedron",
        "Octahedron",
        "Dodecahedron",
        "Icosahedron",
        "Torus",
        "Cone",
        "Cylinder"
    } };

const int GeometryCache::UNKNOWN_ID = -1;


static const int VERTICES_PER_TRIANGLE = 3;

static const gpu::Element POSITION_ELEMENT { gpu::VEC3, gpu::FLOAT, gpu::XYZ };
static const gpu::Element NORMAL_ELEMENT { gpu::VEC3, gpu::FLOAT, gpu::XYZ };
static const gpu::Element TEXCOORD0_ELEMENT { gpu::VEC2, gpu::FLOAT, gpu::UV };
static const gpu::Element TANGENT_ELEMENT { gpu::VEC3, gpu::FLOAT, gpu::XYZ };
static const gpu::Element COLOR_ELEMENT { gpu::VEC4, gpu::NUINT8, gpu::RGBA };
static const gpu::Element TEXCOORD4_ELEMENT { gpu::VEC4, gpu::FLOAT, gpu::XYZW };

static gpu::Stream::FormatPointer SOLID_STREAM_FORMAT;
static gpu::Stream::FormatPointer INSTANCED_SOLID_STREAM_FORMAT;
static gpu::Stream::FormatPointer INSTANCED_SOLID_FADE_STREAM_FORMAT;
static gpu::Stream::FormatPointer WIRE_STREAM_FORMAT;
static gpu::Stream::FormatPointer INSTANCED_WIRE_STREAM_FORMAT;
static gpu::Stream::FormatPointer INSTANCED_WIRE_FADE_STREAM_FORMAT;

static const uint SHAPE_VERTEX_STRIDE = sizeof(GeometryCache::ShapeVertex); // position, normal, texcoords, tangent
static const uint SHAPE_NORMALS_OFFSET = offsetof(GeometryCache::ShapeVertex, normal);
static const uint SHAPE_TEXCOORD0_OFFSET = offsetof(GeometryCache::ShapeVertex, uv);
static const uint SHAPE_TANGENT_OFFSET = offsetof(GeometryCache::ShapeVertex, tangent);

std::map<std::pair<bool, bool>, gpu::PipelinePointer> GeometryCache::_webPipelines;
std::map<std::pair<bool, bool>, gpu::PipelinePointer> GeometryCache::_gridPipelines;

void GeometryCache::computeSimpleHullPointListForShape(const int entityShape, const glm::vec3 &entityExtents, QVector<glm::vec3> &outPointList) {

    auto geometryCache = DependencyManager::get<GeometryCache>();
    const GeometryCache::Shape geometryShape = GeometryCache::getShapeForEntityShape( entityShape );
    const GeometryCache::ShapeData * shapeData = geometryCache->getShapeData( geometryShape );
    if (!shapeData){
        //--EARLY EXIT--( data isn't ready for some reason... )
        return;
    }

    const gpu::BufferView & shapeVerts = shapeData->_positionView;
    const gpu::BufferView::Size numItems = shapeVerts.getNumElements();

    outPointList.reserve((int)numItems);
    QVector<glm::vec3> uniqueVerts;
    uniqueVerts.reserve((int)numItems);

    const float MAX_INCLUSIVE_FILTER_DISTANCE_SQUARED = 1.0e-6f; //< 1mm^2
    for (gpu::BufferView::Index i = 0; i < (gpu::BufferView::Index)numItems; ++i) {
        const int numUniquePoints = (int)uniqueVerts.size();
        const geometry::Vec &curVert = shapeVerts.get<geometry::Vec>(i);
        bool isUniquePoint = true;

        for (int uniqueIndex = 0; uniqueIndex < numUniquePoints; ++uniqueIndex) {
            const geometry::Vec knownVert = uniqueVerts[uniqueIndex];
            const float distToKnownPoint = glm::length2(knownVert - curVert);

            if (distToKnownPoint <= MAX_INCLUSIVE_FILTER_DISTANCE_SQUARED) {
                isUniquePoint = false;
                break;
            }
        }

        if (!isUniquePoint) {

            //--EARLY ITERATION EXIT--
            continue;
        }


        uniqueVerts.push_back(curVert);
        outPointList.push_back(curVert * entityExtents);
    }
}

template <size_t SIDES>
std::vector<vec3> polygon() {
    std::vector<vec3> result;
    result.reserve(SIDES);
    double angleIncrement = 2.0 * M_PI / SIDES;
    for (size_t i = 0; i < SIDES; i++) {
        double angle = (double)i * angleIncrement;
        result.push_back(vec3{ cos(angle) * 0.5, 0.0, sin(angle) * 0.5 });
    }
    return result;
}

void GeometryCache::ShapeData::setupVertices(gpu::BufferPointer& vertexBuffer, const std::vector<ShapeVertex>& vertices) {
    gpu::Buffer::Size offset = vertexBuffer->getSize();
    vertexBuffer->append(vertices);

    gpu::Buffer::Size viewSize = vertices.size() * sizeof(ShapeVertex);

    _positionView = gpu::BufferView(vertexBuffer, offset,
        viewSize, SHAPE_VERTEX_STRIDE, POSITION_ELEMENT);
    _normalView = gpu::BufferView(vertexBuffer, offset + SHAPE_NORMALS_OFFSET,
        viewSize, SHAPE_VERTEX_STRIDE, NORMAL_ELEMENT);
    _texCoordView = gpu::BufferView(vertexBuffer, offset + SHAPE_TEXCOORD0_OFFSET,
        viewSize, SHAPE_VERTEX_STRIDE, TEXCOORD0_ELEMENT);
    _tangentView = gpu::BufferView(vertexBuffer, offset + SHAPE_TANGENT_OFFSET,
        viewSize, SHAPE_VERTEX_STRIDE, TANGENT_ELEMENT);
}

void GeometryCache::ShapeData::setupIndices(gpu::BufferPointer& indexBuffer, const geometry::IndexVector& indices, const geometry::IndexVector& wireIndices) {
    gpu::Buffer::Size offset = indexBuffer->getSize();
    if (!indices.empty()) {
        for (uint32_t i = 0; i < indices.size(); ++i) {
            indexBuffer->append((uint16_t)indices[i]);
        }
    }
    gpu::Size viewSize = indices.size() * sizeof(uint16_t);
    _indicesView = gpu::BufferView(indexBuffer, offset, viewSize, gpu::Element::INDEX_UINT16);

    offset = indexBuffer->getSize();
    if (!wireIndices.empty()) {
        for (uint32_t i = 0; i < wireIndices.size(); ++i) {
            indexBuffer->append((uint16_t)wireIndices[i]);
        }
    }
    viewSize = wireIndices.size() * sizeof(uint16_t);
    _wireIndicesView = gpu::BufferView(indexBuffer, offset, viewSize, gpu::Element::INDEX_UINT16);
}

void GeometryCache::ShapeData::setupBatch(gpu::Batch& batch) const {
    batch.setInputBuffer(gpu::Stream::POSITION, _positionView);
    batch.setInputBuffer(gpu::Stream::NORMAL, _normalView);
    batch.setInputBuffer(gpu::Stream::TEXCOORD, _texCoordView);
    batch.setInputBuffer(gpu::Stream::TANGENT, _tangentView);
    batch.setIndexBuffer(_indicesView);
}

void GeometryCache::ShapeData::draw(gpu::Batch& batch) const {
    uint32_t numIndices = (uint32_t)_indicesView.getNumElements();
    if (numIndices > 0) {
        setupBatch(batch);
        batch.drawIndexed(gpu::TRIANGLES, numIndices, 0);
    }
}

void GeometryCache::ShapeData::drawWire(gpu::Batch& batch) const {
    uint32_t numIndices = (uint32_t)_wireIndicesView.getNumElements();
    if (numIndices > 0) {
        batch.setInputBuffer(gpu::Stream::POSITION, _positionView);
        batch.setInputBuffer(gpu::Stream::NORMAL, _normalView);
        batch.setIndexBuffer(_wireIndicesView);
        batch.drawIndexed(gpu::LINES, numIndices, 0);
    }
}

void GeometryCache::ShapeData::drawInstances(gpu::Batch& batch, size_t count) const {
    uint32_t numIndices = (uint32_t)_indicesView.getNumElements();
    if (numIndices > 0) {
        setupBatch(batch);
        batch.drawIndexedInstanced((gpu::uint32)count, gpu::TRIANGLES, numIndices, 0);
    }
}

void GeometryCache::ShapeData::drawWireInstances(gpu::Batch& batch, size_t count) const {
    uint32_t numIndices = (uint32_t)_wireIndicesView.getNumElements();
    if (numIndices > 0) {
        batch.setInputBuffer(gpu::Stream::POSITION, _positionView);
        batch.setInputBuffer(gpu::Stream::NORMAL, _normalView);
        batch.setIndexBuffer(_wireIndicesView);
        batch.drawIndexedInstanced((gpu::uint32)count, gpu::LINES, numIndices, 0);
    }
}

static const size_t ICOSAHEDRON_TO_SPHERE_TESSELATION_COUNT = 3;

size_t GeometryCache::getShapeTriangleCount(Shape shape) {
    return _shapes[shape]._indicesView.getNumElements() / VERTICES_PER_TRIANGLE;
}

size_t GeometryCache::getSphereTriangleCount() {
    return getShapeTriangleCount(Sphere);
}

size_t GeometryCache::getCubeTriangleCount() {
    return getShapeTriangleCount(Cube);
}

using IndexPair = uint64_t;
using IndexPairs = std::unordered_set<IndexPair>;

static IndexPair indexToken(geometry::Index a, geometry::Index b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (((IndexPair)a) << 32) | ((IndexPair)b);
}

template <size_t N>
void setupFlatShape(GeometryCache::ShapeData& shapeData, const geometry::Solid<N>& shape, gpu::BufferPointer& vertexBuffer, gpu::BufferPointer& indexBuffer) {
    using namespace geometry;
    std::vector<GeometryCache::ShapeVertex> vertices;
    IndexVector solidIndices, wireIndices;
    IndexPairs wireSeenIndices;

    size_t faceCount = shape.faces.size();
    size_t faceIndexCount = triangulatedFaceIndexCount<N>();

    vertices.reserve(N * faceCount);
    solidIndices.reserve(faceIndexCount * faceCount);

    Index baseVertex = 0;
    for (size_t f = 0; f < faceCount; f++) {
        const Face<N>& face = shape.faces[f];
        // Compute the face normal
        vec3 faceNormal = shape.getFaceNormal(f);

        // Find two points on this face with the same Y tex coords, and find the vector going from the one with the smaller X tex coord to the one with the larger X tex coord
        vec3 faceTangent = vec3(0.0f);
        Index i1 = 0;
        Index i2 = i1 + 1;
        while (i1 < N) {
            if (shape.texCoords[f * N + i1].y == shape.texCoords[f * N + i2].y) {
                break;
            }
            if (i2 == N - 1) {
                i1++;
                i2 = i1 + 1;
            } else {
                i2++;
            }
        }

        if (i1 < N && i2 < N) {
            vec3 p1 = shape.vertices[face[i1]];
            vec3 p2 = shape.vertices[face[i2]];
            faceTangent = glm::normalize(p1 - p2);
            if (shape.texCoords[f * N + i1].x < shape.texCoords[f * N + i2].x) {
                faceTangent *= -1.0f;
            }
        }

        // Create the vertices for the face
        for (Index i = 0; i < N; i++) {
            Index originalIndex = face[i];
            vertices.emplace_back(shape.vertices[originalIndex], faceNormal, shape.texCoords[f * N + i], faceTangent);
        }

        // Create the wire indices for unseen edges
        for (Index i = 0; i < N; i++) {
            Index a = i;
            Index b = (i + 1) % N;
            auto token = indexToken(face[a], face[b]);
            if (0 == wireSeenIndices.count(token)) {
                wireSeenIndices.insert(token);
                wireIndices.push_back(a + baseVertex);
                wireIndices.push_back(b + baseVertex);
            }
        }

        // Create the solid face indices
        for (Index i = 0; i < N - 2; i++) {
            solidIndices.push_back(0 + baseVertex);
            solidIndices.push_back(i + 1 + baseVertex);
            solidIndices.push_back(i + 2 + baseVertex);
        }
        baseVertex += (Index)N;
    }

    shapeData.setupVertices(vertexBuffer, vertices);
    shapeData.setupIndices(indexBuffer, solidIndices, wireIndices);
}

vec2 calculateSphereTexCoord(const vec3& vertex) {
    float u = 1.0f - (std::atan2(-vertex.z, -vertex.x) / ((float)M_PI) + 1.0f) * 0.5f;
    if (vertex.y == 1.0f || vertex.y == -1.0f) {
        // Remember points at the top so we don't treat them as being along the seam
        u = NAN;
    }
    float v = 0.5f - std::asin(vertex.y) / (float)M_PI;
    return vec2(u, v);
}

const float M_PI_TIMES_2 = 2.0f * (float)M_PI;

vec3 calculateSphereTangent(float u) {
    float phi = u * M_PI_TIMES_2;
    return -glm::normalize(glm::vec3(glm::sin(phi), 0.0f, glm::cos(phi)));
}

template <size_t N>
void setupSmoothShape(GeometryCache::ShapeData& shapeData, const geometry::Solid<N>& shape, gpu::BufferPointer& vertexBuffer, gpu::BufferPointer& indexBuffer) {
    using namespace geometry;

    std::vector<GeometryCache::ShapeVertex> vertices;
    vertices.reserve(shape.vertices.size());
    for (const auto& vertex : shape.vertices) {
        // We'll fill in the correct tangents later, once we correct the UVs
        vertices.emplace_back(vertex, vertex, calculateSphereTexCoord(vertex), vec3(0.0f));
    }

    // We need to fix up the sphere's UVs because it's actually a tesselated icosahedron.  See http://mft-dev.dk/uv-mapping-sphere/
    size_t faceCount = shape.faces.size();
    for (size_t f = 0; f < faceCount; f++) {
        // Fix zipper
        {
            float& u1 = vertices[shape.faces[f][0]].uv.x;
            float& u2 = vertices[shape.faces[f][1]].uv.x;
            float& u3 = vertices[shape.faces[f][2]].uv.x;

            if (glm::isnan(u1)) {
                u1 = (u2 + u3) / 2.0f;
            }
            if (glm::isnan(u2)) {
                u2 = (u1 + u3) / 2.0f;
            }
            if (glm::isnan(u3)) {
                u3 = (u1 + u2) / 2.0f;
            }

            const float U_THRESHOLD = 0.25f;
            float max = glm::max(u1, glm::max(u2, u3));
            float min = glm::min(u1, glm::min(u2, u3));

            if (max - min > U_THRESHOLD) {
                if (u1 < U_THRESHOLD) {
                    u1 += 1.0f;
                }
                if (u2 < U_THRESHOLD) {
                    u2 += 1.0f;
                }
                if (u3 < U_THRESHOLD) {
                    u3 += 1.0f;
                }
            }
        }

        // Fix swirling at poles
        for (Index i = 0; i < N; i++) {
            Index originalIndex = shape.faces[f][i];
            if (shape.vertices[originalIndex].y == 1.0f || shape.vertices[originalIndex].y == -1.0f) {
                float uSum = 0.0f;
                for (Index i2 = 1; i2 <= N - 1; i2++) {
                    float u = vertices[shape.faces[f][(i + i2) % N]].uv.x;
                    uSum += u;
                }
                uSum /= (float)(N - 1);
                vertices[originalIndex].uv.x = uSum;
                break;
            }
        }

        // Fill in tangents
        for (Index i = 0; i < N; i++) {
            vec3 tangent = calculateSphereTangent(vertices[shape.faces[f][i]].uv.x);
            vertices[shape.faces[f][i]].tangent = tangent;
        }
    }

    IndexVector solidIndices, wireIndices;
    IndexPairs wireSeenIndices;

    size_t faceIndexCount = triangulatedFaceIndexCount<N>();

    solidIndices.reserve(faceIndexCount * faceCount);

    Index baseVertex = 0;
    for (size_t f = 0; f < faceCount; f++) {
        const Face<N>& face = shape.faces[f];
        // Create the wire indices for unseen edges
        for (Index i = 0; i < N; i++) {
            Index a = face[i];
            Index b = face[(i + 1) % N];
            auto token = indexToken(a, b);
            if (0 == wireSeenIndices.count(token)) {
                wireSeenIndices.insert(token);
                wireIndices.push_back(a + baseVertex);
                wireIndices.push_back(b + baseVertex);
            }
        }

        // Create the solid face indices
        for (Index i = 0; i < N - 2; i++) {
            solidIndices.push_back(face[i] + baseVertex);
            solidIndices.push_back(face[i + 1] + baseVertex);
            solidIndices.push_back(face[i + 2] + baseVertex);
        }
    }

    shapeData.setupVertices(vertexBuffer, vertices);
    shapeData.setupIndices(indexBuffer, solidIndices, wireIndices);
}

template <uint32_t N>
void extrudePolygon(GeometryCache::ShapeData& shapeData, gpu::BufferPointer& vertexBuffer, gpu::BufferPointer& indexBuffer, bool isConical = false) {
    using namespace geometry;
    std::vector<GeometryCache::ShapeVertex> vertices;
    IndexVector solidIndices, wireIndices;

    // Top (if not conical) and bottom faces
    std::vector<vec3> shape = polygon<N>();
    if (isConical) {
        for (uint32_t i = 0; i < N; i++) {
            vertices.emplace_back(vec3(0.0f, 0.5f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec2((float)i / (float)N, 1.0f), vec3(0.0f));
        }
    } else {
        for (const vec3& v : shape) {
            vertices.emplace_back(vec3(v.x, 0.5f, v.z), vec3(0.0f, 1.0f, 0.0f), vec2(v.x, v.z) + vec2(0.5f), vec3(1.0f, 0.0f, 0.0f));
        }
    }
    for (const vec3& v : shape) {
        vertices.emplace_back(vec3(v.x, -0.5f, v.z), vec3(0.0f, -1.0f, 0.0f), vec2(-v.x, v.z) + vec2(0.5f), vec3(-1.0f, 0.0f, 0.0f));
    }
    Index baseVertex = 0;
    for (uint32_t i = 2; i < N; i++) {
        solidIndices.push_back(baseVertex + 0);
        solidIndices.push_back(baseVertex + i);
        solidIndices.push_back(baseVertex + i - 1);
        solidIndices.push_back(baseVertex + N);
        solidIndices.push_back(baseVertex + i + N - 1);
        solidIndices.push_back(baseVertex + i + N);
    }
    for (uint32_t i = 1; i <= N; i++) {
        wireIndices.push_back(baseVertex + (i % N));
        wireIndices.push_back(baseVertex + i - 1);
        wireIndices.push_back(baseVertex + (i % N) + N);
        wireIndices.push_back(baseVertex + (i - 1) + N);
    }

    // Now do the sides
    baseVertex += 2 * N;

    for (uint32_t i = 0; i < N; i++) {
        vec3 left = shape[i];
        vec3 right = shape[(i + 1) % N];
        vec3 normal = glm::normalize(left + right);
        vec3 topLeft = (isConical ? vec3(0.0f, 0.5f, 0.0f) : vec3(left.x, 0.5f, left.z));
        vec3 topRight = (isConical ? vec3(0.0f, 0.5f, 0.0f) : vec3(right.x, 0.5f, right.z));
        vec3 bottomLeft = vec3(left.x, -0.5f, left.z);
        vec3 bottomRight = vec3(right.x, -0.5f, right.z);
        vec3 tangent = glm::normalize(bottomLeft - bottomRight);

        // Our tex coords go in the opposite direction as our vertices
        float u = 1.0f - (float)i / (float)N;
        float u2 = 1.0f - (float)(i + 1) / (float)N;

        vertices.emplace_back(topLeft, normal, vec2(u, 0.0f), tangent);
        vertices.emplace_back(bottomLeft, normal, vec2(u, 1.0f), tangent);
        vertices.emplace_back(topRight, normal, vec2(u2, 0.0f), tangent);
        vertices.emplace_back(bottomRight, normal, vec2(u2, 1.0f), tangent);

        solidIndices.push_back(baseVertex + 0);
        solidIndices.push_back(baseVertex + 2);
        solidIndices.push_back(baseVertex + 1);
        solidIndices.push_back(baseVertex + 1);
        solidIndices.push_back(baseVertex + 2);
        solidIndices.push_back(baseVertex + 3);
        wireIndices.push_back(baseVertex + 0);
        wireIndices.push_back(baseVertex + 1);
        wireIndices.push_back(baseVertex + 3);
        wireIndices.push_back(baseVertex + 2);
        baseVertex += 4;
    }

    shapeData.setupVertices(vertexBuffer, vertices);
    shapeData.setupIndices(indexBuffer, solidIndices, wireIndices);
}

// FIXME solids need per-face vertices, but smooth shaded
// components do not.  Find a way to support using draw elements
// or draw arrays as appropriate
// Maybe special case cone and cylinder since they combine flat
// and smooth shading
void GeometryCache::buildShapes() {
    using namespace geometry;
    auto vertexBuffer = std::make_shared<gpu::Buffer>();
    auto indexBuffer = std::make_shared<gpu::Buffer>();
    // Cube
    setupFlatShape(_shapes[Cube], geometry::cube(), _shapeVertices, _shapeIndices);
    //Quad renders as flat Cube
    setupFlatShape(_shapes[Quad], geometry::cube(), _shapeVertices, _shapeIndices);
    // Tetrahedron
    setupFlatShape(_shapes[Tetrahedron], geometry::tetrahedron(), _shapeVertices, _shapeIndices);
    // Icosahedron
    setupFlatShape(_shapes[Icosahedron], geometry::icosahedron(), _shapeVertices, _shapeIndices);
    // Octahedron
    setupFlatShape(_shapes[Octahedron], geometry::octahedron(), _shapeVertices, _shapeIndices);
    // Dodecahedron
    setupFlatShape(_shapes[Dodecahedron], geometry::dodecahedron(), _shapeVertices, _shapeIndices);

    // Sphere
    // FIXME this uses way more vertices than required.  Should find a way to calculate the indices
    // using shared vertices for better vertex caching
    auto sphere = geometry::tesselate(geometry::icosahedron(), ICOSAHEDRON_TO_SPHERE_TESSELATION_COUNT);
    sphere.fitDimension(1.0f);
    setupSmoothShape(_shapes[Sphere], sphere, _shapeVertices, _shapeIndices);

    // Line
    {
        ShapeData& shapeData = _shapes[Line];
        shapeData.setupVertices(_shapeVertices, std::vector<ShapeVertex> {
            ShapeVertex(vec3(-0.5f, 0.0f, 0.0f), vec3(-0.5f, 0.0f, 0.0f), vec2(0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f)),
            ShapeVertex(vec3(0.5f, 0.0f, 0.0f), vec3(0.5f, 0.0f, 0.0f), vec2(0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f))
        });
        IndexVector wireIndices;
        // Only two indices
        wireIndices.push_back(0);
        wireIndices.push_back(1);
        shapeData.setupIndices(_shapeIndices, IndexVector(), wireIndices);
    }

    //Triangle,
    extrudePolygon<3>(_shapes[Triangle], _shapeVertices, _shapeIndices);
    //Hexagon,
    extrudePolygon<6>(_shapes[Hexagon], _shapeVertices, _shapeIndices);
    //Octagon,
    extrudePolygon<8>(_shapes[Octagon], _shapeVertices, _shapeIndices);
    //Cylinder,
    extrudePolygon<64>(_shapes[Cylinder], _shapeVertices, _shapeIndices);
    //Cone,
    extrudePolygon<64>(_shapes[Cone], _shapeVertices, _shapeIndices, true);
    // Circle renders as flat Cylinder
    extrudePolygon<64>(_shapes[Circle], _shapeVertices, _shapeIndices);
    // Not implemented yet:
    //Torus,
}

const GeometryCache::ShapeData * GeometryCache::getShapeData(const Shape shape) const {
    if (((int)shape < 0) || ((int)shape >= (int)_shapes.size())) {
        qCWarning(renderutils) << "GeometryCache::getShapeData - Invalid shape " << shape << " specified. Returning default fallback.";

        //--EARLY EXIT--( No valid shape data for shape )
        return nullptr;
    }

    return &_shapes[shape];
}

GeometryCache::Shape GeometryCache::getShapeForEntityShape(int entityShape) {
    if ((entityShape < 0) || (entityShape >= (int)MAPPING.size())) {
        qCWarning(renderutils) << "GeometryCache::getShapeForEntityShape - Invalid shape " << entityShape << " specified. Returning default fallback.";

        //--EARLY EXIT--( fall back to default assumption )
        return GeometryCache::Sphere;
    }

    return MAPPING[entityShape];
}

QString GeometryCache::stringFromShape(GeometryCache::Shape geoShape)
{
    if (((int)geoShape < 0) || ((int)geoShape >= (int)GeometryCache::NUM_SHAPES)) {
        qCWarning(renderutils) << "GeometryCache::stringFromShape - Invalid shape " << geoShape << " specified.";

        //--EARLY EXIT--
        return "INVALID_GEOCACHE_SHAPE";
    }

    return GEOCACHE_SHAPE_STRINGS[geoShape];
}

gpu::Stream::FormatPointer& getSolidStreamFormat() {
    if (!SOLID_STREAM_FORMAT) {
        SOLID_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
        SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD0, gpu::Stream::TEXCOORD0, TEXCOORD0_ELEMENT);
        SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::TANGENT, gpu::Stream::TANGENT, TANGENT_ELEMENT);
    }
    return SOLID_STREAM_FORMAT;
}

gpu::Stream::FormatPointer& getInstancedSolidStreamFormat() {
    if (!INSTANCED_SOLID_STREAM_FORMAT) {
        INSTANCED_SOLID_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        INSTANCED_SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        INSTANCED_SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
        INSTANCED_SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD0, gpu::Stream::TEXCOORD0, TEXCOORD0_ELEMENT);
        INSTANCED_SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::TANGENT, gpu::Stream::TANGENT, TANGENT_ELEMENT);
        INSTANCED_SOLID_STREAM_FORMAT->setAttribute(gpu::Stream::COLOR, gpu::Stream::COLOR, COLOR_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
    }
    return INSTANCED_SOLID_STREAM_FORMAT;
}

gpu::Stream::FormatPointer& getInstancedSolidFadeStreamFormat() {
    if (!INSTANCED_SOLID_FADE_STREAM_FORMAT) {
        INSTANCED_SOLID_FADE_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD0, gpu::Stream::TEXCOORD0, TEXCOORD0_ELEMENT);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TANGENT, gpu::Stream::TANGENT, TANGENT_ELEMENT);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::COLOR, gpu::Stream::COLOR, COLOR_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD2, gpu::Stream::TEXCOORD2, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD3, gpu::Stream::TEXCOORD3, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_SOLID_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD4, gpu::Stream::TEXCOORD4, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
    }
    return INSTANCED_SOLID_FADE_STREAM_FORMAT;
}

gpu::Stream::FormatPointer& getWireStreamFormat() {
    if (!WIRE_STREAM_FORMAT) {
        WIRE_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        WIRE_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        WIRE_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
    }
    return WIRE_STREAM_FORMAT;
}

gpu::Stream::FormatPointer& getInstancedWireStreamFormat() {
    if (!INSTANCED_WIRE_STREAM_FORMAT) {
        INSTANCED_WIRE_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        INSTANCED_WIRE_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        INSTANCED_WIRE_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
        INSTANCED_WIRE_STREAM_FORMAT->setAttribute(gpu::Stream::COLOR, gpu::Stream::COLOR, COLOR_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
    }
    return INSTANCED_WIRE_STREAM_FORMAT;
}

gpu::Stream::FormatPointer& getInstancedWireFadeStreamFormat() {
    if (!INSTANCED_WIRE_FADE_STREAM_FORMAT) {
        INSTANCED_WIRE_FADE_STREAM_FORMAT = std::make_shared<gpu::Stream::Format>(); // 1 for everyone
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::POSITION, gpu::Stream::POSITION, POSITION_ELEMENT);
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::NORMAL, gpu::Stream::NORMAL, NORMAL_ELEMENT);
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::COLOR, gpu::Stream::COLOR, COLOR_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD2, gpu::Stream::TEXCOORD2, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD3, gpu::Stream::TEXCOORD3, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
        INSTANCED_WIRE_FADE_STREAM_FORMAT->setAttribute(gpu::Stream::TEXCOORD4, gpu::Stream::TEXCOORD4, TEXCOORD4_ELEMENT, 0, gpu::Stream::PER_INSTANCE);
    }
    return INSTANCED_WIRE_FADE_STREAM_FORMAT;
}

QHash<SimpleProgramKey, gpu::PipelinePointer> GeometryCache::_simplePrograms;

gpu::ShaderPointer GeometryCache::_simpleShader;
gpu::ShaderPointer GeometryCache::_transparentShader;
gpu::ShaderPointer GeometryCache::_unlitShader;
gpu::ShaderPointer GeometryCache::_simpleFadeShader;
gpu::ShaderPointer GeometryCache::_unlitFadeShader;
gpu::ShaderPointer GeometryCache::_forwardSimpleShader;
gpu::ShaderPointer GeometryCache::_forwardTransparentShader;
gpu::ShaderPointer GeometryCache::_forwardUnlitShader;
gpu::ShaderPointer GeometryCache::_forwardSimpleFadeShader;
gpu::ShaderPointer GeometryCache::_forwardUnlitFadeShader;

std::map<std::tuple<bool, bool, bool, graphics::MaterialKey::CullFaceMode>, render::ShapePipelinePointer> GeometryCache::_shapePipelines;

GeometryCache::GeometryCache() :
_nextID(0) {
    // Let's register its special shapePipeline factory:
    initializeShapePipelines();
    buildShapes();
}

GeometryCache::~GeometryCache() {
#ifdef WANT_DEBUG
    qCDebug(renderutils) << "GeometryCache::~GeometryCache()... ";
    qCDebug(renderutils) << "    _registeredLine3DVBOs.size():" << _registeredLine3DVBOs.size();
    qCDebug(renderutils) << "    _line3DVBOs.size():" << _line3DVBOs.size();
    qCDebug(renderutils) << "    BatchItemDetails... population:" << GeometryCache::BatchItemDetails::population;
#endif //def WANT_DEBUG
}

void GeometryCache::releaseID(int id) {
    _registeredQuad3DTextures.remove(id);
    _lastRegisteredQuad2DTexture.remove(id);
    _registeredQuad2DTextures.remove(id);
    _lastRegisteredQuad3D.remove(id);
    _registeredQuad3D.remove(id);

    _lastRegisteredQuad2D.remove(id);
    _registeredQuad2D.remove(id);

    _lastRegisteredBevelRects.remove(id);
    _registeredBevelRects.remove(id);

    _lastRegisteredLine3D.remove(id);
    _registeredLine3DVBOs.remove(id);

    _lastRegisteredLine2D.remove(id);
    _registeredLine2DVBOs.remove(id);

    _registeredVertices.remove(id);

    _lastRegisteredDashedLines.remove(id);
    _registeredDashedLines.remove(id);

    _lastRegisteredGridBuffer.remove(id);
    _registeredGridBuffers.remove(id);
}

void GeometryCache::initializeShapePipelines() {
    if (_shapePipelines.empty()) {
        const int NUM_PIPELINES = 8;
        for (int i = 0; i < NUM_PIPELINES; ++i) {
            bool transparent = i & 1;
            bool unlit = i & 2;
            bool forward = i & 4;
            for (int cullFaceMode = graphics::MaterialKey::CullFaceMode::CULL_NONE; cullFaceMode < graphics::MaterialKey::CullFaceMode::NUM_CULL_FACE_MODES; cullFaceMode++) {
                auto cullMode = (graphics::MaterialKey::CullFaceMode)cullFaceMode;
                _shapePipelines[std::make_tuple(transparent, unlit, forward, cullMode)] = getShapePipeline(false, transparent, unlit, false, forward, cullMode);
            }
        }
    }
}

render::ShapePipelinePointer GeometryCache::getShapePipeline(bool textured, bool transparent, bool unlit, bool depthBias, bool forward,
        graphics::MaterialKey::CullFaceMode cullFaceMode) {

    return std::make_shared<render::ShapePipeline>(getSimplePipeline(textured, transparent, unlit, depthBias, false, true, forward, cullFaceMode), nullptr,
        [](const render::ShapePipeline& pipeline, gpu::Batch& batch, render::Args* args) {
            batch.setResourceTexture(gr::Texture::MaterialAlbedo, DependencyManager::get<TextureCache>()->getWhiteTexture());
            DependencyManager::get<DeferredLightingEffect>()->setupKeyLightBatch(args, batch);
        }
    );
}

render::ShapePipelinePointer GeometryCache::getFadingShapePipeline(bool textured, bool transparent, bool unlit, bool depthBias, bool forward,
        graphics::MaterialKey::CullFaceMode cullFaceMode) {
    auto fadeEffect = DependencyManager::get<FadeEffect>();
    auto fadeBatchSetter = fadeEffect->getBatchSetter();
    auto fadeItemSetter = fadeEffect->getItemStoredSetter();
    return std::make_shared<render::ShapePipeline>(getSimplePipeline(textured, transparent, unlit, depthBias, true, true, forward, cullFaceMode), nullptr,
        [fadeBatchSetter, fadeItemSetter](const render::ShapePipeline& shapePipeline, gpu::Batch& batch, render::Args* args) {
            batch.setResourceTexture(gr::Texture::MaterialAlbedo, DependencyManager::get<TextureCache>()->getWhiteTexture());
            fadeBatchSetter(shapePipeline, batch, args);
        },
        fadeItemSetter
    );
}

void GeometryCache::renderShape(gpu::Batch& batch, Shape shape) {
    batch.setInputFormat(getSolidStreamFormat());
    _shapes[shape].draw(batch);
}

void GeometryCache::renderWireShape(gpu::Batch& batch, Shape shape) {
    batch.setInputFormat(getWireStreamFormat());
    _shapes[shape].drawWire(batch);
}

void GeometryCache::renderShape(gpu::Batch& batch, Shape shape, const glm::vec4& color) {
    batch.setInputFormat(getSolidStreamFormat());
    // Color must be set after input format
    batch._glColor4f(color.r, color.g, color.b, color.a);
    _shapes[shape].draw(batch);
}

void GeometryCache::renderWireShape(gpu::Batch& batch, Shape shape, const glm::vec4& color) {
    batch.setInputFormat(getWireStreamFormat());
    // Color must be set after input format
    batch._glColor4f(color.r, color.g, color.b, color.a);
    _shapes[shape].drawWire(batch);
}

void setupBatchInstance(gpu::Batch& batch, gpu::BufferPointer colorBuffer) {
    gpu::BufferView colorView(colorBuffer, COLOR_ELEMENT);
    batch.setInputBuffer(gpu::Stream::COLOR, colorView);
}

void GeometryCache::renderShapeInstances(gpu::Batch& batch, Shape shape, size_t count, gpu::BufferPointer& colorBuffer) {
    batch.setInputFormat(getInstancedSolidStreamFormat());
    setupBatchInstance(batch, colorBuffer);
    _shapes[shape].drawInstances(batch, count);
}

void GeometryCache::renderWireShapeInstances(gpu::Batch& batch, Shape shape, size_t count, gpu::BufferPointer& colorBuffer) {
    batch.setInputFormat(getInstancedWireStreamFormat());
    setupBatchInstance(batch, colorBuffer);
    _shapes[shape].drawWireInstances(batch, count);
}

void setupBatchFadeInstance(gpu::Batch& batch, gpu::BufferPointer colorBuffer,
    gpu::BufferPointer fadeBuffer1, gpu::BufferPointer fadeBuffer2, gpu::BufferPointer fadeBuffer3) {
    gpu::BufferView colorView(colorBuffer, COLOR_ELEMENT);
    gpu::BufferView texCoord2View(fadeBuffer1, TEXCOORD4_ELEMENT);
    gpu::BufferView texCoord3View(fadeBuffer2, TEXCOORD4_ELEMENT);
    gpu::BufferView texCoord4View(fadeBuffer3, TEXCOORD4_ELEMENT);
    batch.setInputBuffer(gpu::Stream::COLOR, colorView);
    batch.setInputBuffer(gpu::Stream::TEXCOORD2, texCoord2View);
    batch.setInputBuffer(gpu::Stream::TEXCOORD3, texCoord3View);
    batch.setInputBuffer(gpu::Stream::TEXCOORD4, texCoord4View);
}

void GeometryCache::renderFadeShapeInstances(gpu::Batch& batch, Shape shape, size_t count, gpu::BufferPointer& colorBuffer,
    gpu::BufferPointer& fadeBuffer1, gpu::BufferPointer& fadeBuffer2, gpu::BufferPointer& fadeBuffer3) {
    batch.setInputFormat(getInstancedSolidFadeStreamFormat());
    setupBatchFadeInstance(batch, colorBuffer, fadeBuffer1, fadeBuffer2, fadeBuffer3);
    _shapes[shape].drawInstances(batch, count);
}

void GeometryCache::renderWireFadeShapeInstances(gpu::Batch& batch, Shape shape, size_t count, gpu::BufferPointer& colorBuffer,
    gpu::BufferPointer& fadeBuffer1, gpu::BufferPointer& fadeBuffer2, gpu::BufferPointer& fadeBuffer3) {
    batch.setInputFormat(getInstancedWireFadeStreamFormat());
    setupBatchFadeInstance(batch, colorBuffer, fadeBuffer1, fadeBuffer2, fadeBuffer3);
    _shapes[shape].drawWireInstances(batch, count);
}

void GeometryCache::renderCube(gpu::Batch& batch) {
    renderShape(batch, Cube);
}

void GeometryCache::renderWireCube(gpu::Batch& batch) {
    renderWireShape(batch, Cube);
}

void GeometryCache::renderCube(gpu::Batch& batch, const glm::vec4& color) {
    renderShape(batch, Cube, color);
}

void GeometryCache::renderWireCube(gpu::Batch& batch, const glm::vec4& color) {
    renderWireShape(batch, Cube, color);
}

void GeometryCache::renderSphere(gpu::Batch& batch) {
    renderShape(batch, Sphere);
}

void GeometryCache::renderWireSphere(gpu::Batch& batch) {
    renderWireShape(batch, Sphere);
}

void GeometryCache::renderSphere(gpu::Batch& batch, const glm::vec4& color) {
    renderShape(batch, Sphere, color);
}

void GeometryCache::renderWireSphere(gpu::Batch& batch, const glm::vec4& color) {
    renderWireShape(batch, Sphere, color);
}

void GeometryCache::renderGrid(gpu::Batch& batch, const glm::vec2& minCorner, const glm::vec2& maxCorner,
        int majorRows, int majorCols, float majorEdge, int minorRows, int minorCols, float minorEdge,
        const glm::vec4& color, bool forward, int id) {

    if (majorRows == 0 || majorCols == 0) {
        return;
    }

    Vec2FloatPair majorKey(glm::vec2(majorRows, majorCols), majorEdge);
    Vec2FloatPair minorKey(glm::vec2(minorRows, minorCols), minorEdge);
    Vec2FloatPairPair key(majorKey, minorKey);

    // Make the gridbuffer
    GridBuffer gridBuffer;
    if (id != UNKNOWN_ID) {
        auto gridBufferIter = _registeredGridBuffers.find(id);
        bool hadGridBuffer = gridBufferIter != _registeredGridBuffers.end();
        if (hadGridBuffer) {
            gridBuffer = gridBufferIter.value();
        } else {
            GridSchema gridSchema;
            gridBuffer = std::make_shared<gpu::Buffer>(sizeof(GridSchema), (const gpu::Byte*)&gridSchema);
        }

        if (!hadGridBuffer || _lastRegisteredGridBuffer[id] != key) {
            _registeredGridBuffers[id] = gridBuffer;
            _lastRegisteredGridBuffer[id] = key;

            gridBuffer.edit<GridSchema>().period = glm::vec4(majorRows, majorCols, minorRows, minorCols);
            gridBuffer.edit<GridSchema>().offset.x = -(majorEdge / majorRows) / 2;
            gridBuffer.edit<GridSchema>().offset.y = -(majorEdge / majorCols) / 2;
            gridBuffer.edit<GridSchema>().offset.z = minorRows == 0 ? 0 : -(minorEdge / minorRows) / 2;
            gridBuffer.edit<GridSchema>().offset.w = minorCols == 0 ? 0 : -(minorEdge / minorCols) / 2;
            gridBuffer.edit<GridSchema>().edge = glm::vec4(glm::vec2(majorEdge),
                // If rows or columns are not set, do not draw minor gridlines
                glm::vec2((minorRows != 0 && minorCols != 0) ? minorEdge : 0.0f));
        }
    }

    // Set the grid pipeline
    useGridPipeline(batch, gridBuffer, color.a < 1.0f, forward);

    static const glm::vec2 MIN_TEX_COORD(0.0f, 0.0f);
    static const glm::vec2 MAX_TEX_COORD(1.0f, 1.0f);
    renderQuad(batch, minCorner, maxCorner, MIN_TEX_COORD, MAX_TEX_COORD, color, id);
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec2>& points, const QVector<glm::vec4>& colors) {
    BatchItemDetails& details = _registeredVertices[id];

    if (details.isCreated) {
        details.clear();
#ifdef WANT_DEBUG
        qCDebug(renderutils) << "updateVertices()... RELEASING REGISTERED";
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2 + 3; // vertices + normals
    const int NUM_POS_COORDS = 2;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    details.isCreated = true;
    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    auto verticesBuffer = std::make_shared<gpu::Buffer>();
    auto colorBuffer = std::make_shared<gpu::Buffer>();
    auto streamFormat = std::make_shared<gpu::Stream::Format>();
    auto stream = std::make_shared<gpu::BufferStream>();

    details.verticesBuffer = verticesBuffer;
    details.colorBuffer = colorBuffer;
    details.streamFormat = streamFormat;
    details.stream = stream;

    details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::XYZ), 0);
    details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
    // TODO: circle3D overlays use this to define their vertices, so they need tex coords
    details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

    details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
    details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);

    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    float* vertexData = new float[details.vertices * FLOATS_PER_VERTEX];
    float* vertex = vertexData;

    int* colorData = new int[details.vertices];
    int* colorDataAt = colorData;

    const glm::vec3 NORMAL(0.0f, -1.0f, 0.0f);
    auto pointCount = points.size();
    auto colorCount = colors.size();
    int compactColor = 0;
    for (auto i = 0; i < pointCount; i++) {
        const auto& point = points[i];
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = NORMAL.x;
        *(vertex++) = NORMAL.y;
        *(vertex++) = NORMAL.z;
        if (i < colorCount) {
            const auto& color = colors[i];
            compactColor = ((int(color.x * 255.0f) & 0xFF)) |
                ((int(color.y * 255.0f) & 0xFF) << 8) |
                ((int(color.z * 255.0f) & 0xFF) << 16) |
                ((int(color.w * 255.0f) & 0xFF) << 24);
        }
        *(colorDataAt++) = compactColor;
    }
    details.verticesBuffer->append(sizeof(float) * FLOATS_PER_VERTEX * details.vertices, (gpu::Byte*) vertexData);
    details.colorBuffer->append(sizeof(int) * details.vertices, (gpu::Byte*) colorData);
    delete[] vertexData;
    delete[] colorData;

#ifdef WANT_DEBUG
    qCDebug(renderutils) << "new registered linestrip buffer made -- _registeredVertices.size():" << _registeredVertices.size();
#endif
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec2>& points, const glm::vec4& color) {
    updateVertices(id, points, QVector<glm::vec4>({ color }));
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec3>& points, const QVector<glm::vec4>& colors) {
    BatchItemDetails& details = _registeredVertices[id];
    if (details.isCreated) {
        details.clear();
#ifdef WANT_DEBUG
        qCDebug(renderutils) << "updateVertices()... RELEASING REGISTERED";
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3 + 3; // vertices + normals
    const int NUM_POS_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    details.isCreated = true;
    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    auto verticesBuffer = std::make_shared<gpu::Buffer>();
    auto colorBuffer = std::make_shared<gpu::Buffer>();
    auto streamFormat = std::make_shared<gpu::Stream::Format>();
    auto stream = std::make_shared<gpu::BufferStream>();

    details.verticesBuffer = verticesBuffer;
    details.colorBuffer = colorBuffer;
    details.streamFormat = streamFormat;
    details.stream = stream;

    details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
    details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
    details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

    details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
    details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);

    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    // Default to white
    int compactColor = 0xFFFFFFFF;
    float* vertexData = new float[details.vertices * FLOATS_PER_VERTEX];
    float* vertex = vertexData;

    int* colorData = new int[details.vertices];
    int* colorDataAt = colorData;

    const glm::vec3 NORMAL(0.0f, -1.0f, 0.0f);
    auto pointCount = points.size();
    auto colorCount = colors.size();
    for (auto i = 0; i < pointCount; i++) {
        const glm::vec3& point = points[i];
        if (i < colorCount) {
            const glm::vec4& color = colors[i];
            compactColor = ((int(color.x * 255.0f) & 0xFF)) |
                ((int(color.y * 255.0f) & 0xFF) << 8) |
                ((int(color.z * 255.0f) & 0xFF) << 16) |
                ((int(color.w * 255.0f) & 0xFF) << 24);
        }
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = point.z;
        *(vertex++) = NORMAL.x;
        *(vertex++) = NORMAL.y;
        *(vertex++) = NORMAL.z;
        *(colorDataAt++) = compactColor;
    }

    details.verticesBuffer->append(sizeof(float) * FLOATS_PER_VERTEX * details.vertices, (gpu::Byte*) vertexData);
    details.colorBuffer->append(sizeof(int) * details.vertices, (gpu::Byte*) colorData);
    delete[] vertexData;
    delete[] colorData;

#ifdef WANT_DEBUG
    qCDebug(renderutils) << "new registered linestrip buffer made -- _registeredVertices.size():" << _registeredVertices.size();
#endif
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec3>& points, const glm::vec4& color) {
    updateVertices(id, points, QVector<glm::vec4>({ color }));
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec3>& points, const QVector<glm::vec2>& texCoords, const glm::vec4& color) {
    BatchItemDetails& details = _registeredVertices[id];

    if (details.isCreated) {
        details.clear();
#ifdef WANT_DEBUG
        qCDebug(renderutils) << "updateVertices()... RELEASING REGISTERED";
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3 + 3 + 2; // vertices + normals + tex coords
    const int NUM_POS_COORDS = 3;
    const int NUM_NORMAL_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    const int VERTEX_TEX_OFFSET = VERTEX_NORMAL_OFFSET + NUM_NORMAL_COORDS * sizeof(float);
    details.isCreated = true;
    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    auto verticesBuffer = std::make_shared<gpu::Buffer>();
    auto colorBuffer = std::make_shared<gpu::Buffer>();
    auto streamFormat = std::make_shared<gpu::Stream::Format>();
    auto stream = std::make_shared<gpu::BufferStream>();

    details.verticesBuffer = verticesBuffer;
    details.colorBuffer = colorBuffer;
    details.streamFormat = streamFormat;
    details.stream = stream;

    details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
    details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
    details.streamFormat->setAttribute(gpu::Stream::TEXCOORD, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV), VERTEX_TEX_OFFSET);
    details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

    details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
    details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);

    assert(points.size() == texCoords.size());

    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
        ((int(color.y * 255.0f) & 0xFF) << 8) |
        ((int(color.z * 255.0f) & 0xFF) << 16) |
        ((int(color.w * 255.0f) & 0xFF) << 24);

    float* vertexData = new float[details.vertices * FLOATS_PER_VERTEX];
    float* vertex = vertexData;

    int* colorData = new int[details.vertices];
    int* colorDataAt = colorData;

    const glm::vec3 NORMAL(0.0f, -1.0f, 0.0f);
    for (int i = 0; i < points.size(); i++) {
        glm::vec3 point = points[i];
        glm::vec2 texCoord = texCoords[i];
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = point.z;
        *(vertex++) = NORMAL.x;
        *(vertex++) = NORMAL.y;
        *(vertex++) = NORMAL.z;
        *(vertex++) = texCoord.x;
        *(vertex++) = texCoord.y;

        *(colorDataAt++) = compactColor;
    }

    details.verticesBuffer->append(sizeof(float) * FLOATS_PER_VERTEX * details.vertices, (gpu::Byte*) vertexData);
    details.colorBuffer->append(sizeof(int) * details.vertices, (gpu::Byte*) colorData);
    delete[] vertexData;
    delete[] colorData;

#ifdef WANT_DEBUG
    qCDebug(renderutils) << "new registered linestrip buffer made -- _registeredVertices.size():" << _registeredVertices.size();
#endif
}

void GeometryCache::renderVertices(gpu::Batch& batch, gpu::Primitive primitiveType, int id) {
    BatchItemDetails& details = _registeredVertices[id];
    if (details.isCreated) {
        batch.setInputFormat(details.streamFormat);
        batch.setInputStream(0, *details.stream);
        batch.draw(primitiveType, details.vertices, 0);
    }
}


void GeometryCache::renderBevelCornersRect(gpu::Batch& batch, int x, int y, int width, int height, int bevelDistance, const glm::vec4& color, int id) {
    bool registered = (id != UNKNOWN_ID);
    Vec3Pair key(glm::vec3(x, y, 0.0f), glm::vec3(width, height, bevelDistance));
    BatchItemDetails& details = _registeredBevelRects[id];
    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec3Pair& lastKey = _lastRegisteredBevelRects[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredBevelRects[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderBevelCornersRect()... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderBevelCornersRect()... REUSING PREVIOUSLY REGISTERED";
        }
#endif // def WANT_DEBUG
    }

    if (!details.isCreated) {
        static const int FLOATS_PER_VERTEX = 2; // vertices
        static const int NUM_VERTICES = 8;
        static const int NUM_FLOATS = NUM_VERTICES * FLOATS_PER_VERTEX;

        details.isCreated = true;
        details.vertices = NUM_VERTICES;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::XYZ));
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        float vertexBuffer[NUM_FLOATS]; // only vertices, no normals because we're a 2D quad
        int vertexPoint = 0;

        // Triangle strip points
        //      3 ------ 5      //
        //    /            \    //
        //  1                7  //
        //  |                |  //
        //  2                8  //
        //    \            /    //
        //      4 ------ 6      //

        // 1
        vertexBuffer[vertexPoint++] = x;
        vertexBuffer[vertexPoint++] = y + height - bevelDistance;
        // 2
        vertexBuffer[vertexPoint++] = x;
        vertexBuffer[vertexPoint++] = y + bevelDistance;
        // 3
        vertexBuffer[vertexPoint++] = x + bevelDistance;
        vertexBuffer[vertexPoint++] = y + height;
        // 4
        vertexBuffer[vertexPoint++] = x + bevelDistance;
        vertexBuffer[vertexPoint++] = y;
        // 5
        vertexBuffer[vertexPoint++] = x + width - bevelDistance;
        vertexBuffer[vertexPoint++] = y + height;
        // 6
        vertexBuffer[vertexPoint++] = x + width - bevelDistance;
        vertexBuffer[vertexPoint++] = y;
        // 7
        vertexBuffer[vertexPoint++] = x + width;
        vertexBuffer[vertexPoint++] = y + height - bevelDistance;
        // 8
        vertexBuffer[vertexPoint++] = x + width;
        vertexBuffer[vertexPoint++] = y + bevelDistance;

        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);
        int colors[NUM_VERTICES] = { compactColor, compactColor, compactColor, compactColor,
            compactColor, compactColor, compactColor, compactColor };


        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::TRIANGLE_STRIP, details.vertices, 0);
}

void GeometryCache::renderQuad(gpu::Batch& batch, const glm::vec2& minCorner, const glm::vec2& maxCorner, const glm::vec4& color, int id) {
    bool registered = (id != UNKNOWN_ID);
    Vec4Pair key(glm::vec4(minCorner.x, minCorner.y, maxCorner.x, maxCorner.y), color);
    BatchItemDetails& details = _registeredQuad2D[id];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec4Pair & lastKey = _lastRegisteredQuad2D[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredQuad2D[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderQuad() 2D ... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderQuad() 2D ... REUSING PREVIOUSLY REGISTERED";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2 + 3; // vertices + normals
    const int VERTICES = 4; // 1 quad = 4 vertices
    const int NUM_POS_COORDS = 2;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);

    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = VERTICES;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        const glm::vec3 NORMAL(0.0f, 0.0f, 1.0f);
        float vertexBuffer[VERTICES * FLOATS_PER_VERTEX] = {
            minCorner.x, minCorner.y, NORMAL.x, NORMAL.y, NORMAL.z,
            maxCorner.x, minCorner.y, NORMAL.x, NORMAL.y, NORMAL.z,
            minCorner.x, maxCorner.y, NORMAL.x, NORMAL.y, NORMAL.z,
            maxCorner.x, maxCorner.y, NORMAL.x, NORMAL.y, NORMAL.z,
        };

        const int NUM_COLOR_SCALARS_PER_QUAD = 4;
        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);
        int colors[NUM_COLOR_SCALARS_PER_QUAD] = { compactColor, compactColor, compactColor, compactColor };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
}

void GeometryCache::renderUnitQuad(gpu::Batch& batch, const glm::vec4& color, int id) {
    static const glm::vec2 topLeft(-1, 1);
    static const glm::vec2 bottomRight(1, -1);
    static const glm::vec2 texCoordTopLeft(0.0f, 1.0f);
    static const glm::vec2 texCoordBottomRight(1.0f, 0.0f);
    renderQuad(batch, topLeft, bottomRight, texCoordTopLeft, texCoordBottomRight, color, id);
}

void GeometryCache::renderQuad(gpu::Batch& batch, const glm::vec2& minCorner, const glm::vec2& maxCorner,
    const glm::vec2& texCoordMinCorner, const glm::vec2& texCoordMaxCorner,
    const glm::vec4& color, int id) {

    Vec4PairVec4 key(Vec4Pair(glm::vec4(minCorner.x, minCorner.y, maxCorner.x, maxCorner.y),
        glm::vec4(texCoordMinCorner.x, texCoordMinCorner.y, texCoordMaxCorner.x, texCoordMaxCorner.y)),
        color);
    BatchItemDetails& details = _registeredQuad2DTextures[id];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (details.isCreated) {
        Vec4PairVec4& lastKey = _lastRegisteredQuad2DTexture[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredQuad2DTexture[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderQuad() 2D+texture ... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderQuad() 2D+texture ... REUSING PREVIOUSLY REGISTERED";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2 + 3 + 2; // vertices + normals + tex coords
    const int VERTICES = 4; // 1 quad = 4 vertices
    const int NUM_POS_COORDS = 2;
    const int NUM_NORMAL_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    const int VERTEX_TEXCOORD_OFFSET = VERTEX_NORMAL_OFFSET + NUM_NORMAL_COORDS * sizeof(float);

    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = VERTICES;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();

        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;

        details.streamFormat = streamFormat;
        details.stream = stream;

        // zzmp: fix the normal across all renderQuad
        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::TEXCOORD, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV), VERTEX_TEXCOORD_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        const glm::vec3 NORMAL(0.0f, 0.0f, 1.0f);
        float vertexBuffer[VERTICES * FLOATS_PER_VERTEX] = {
            minCorner.x, minCorner.y, NORMAL.x, NORMAL.y, NORMAL.z, texCoordMinCorner.x, texCoordMinCorner.y,
            maxCorner.x, minCorner.y, NORMAL.x, NORMAL.y, NORMAL.z, texCoordMaxCorner.x, texCoordMinCorner.y,
            minCorner.x, maxCorner.y, NORMAL.x, NORMAL.y, NORMAL.z, texCoordMinCorner.x, texCoordMaxCorner.y,
            maxCorner.x, maxCorner.y, NORMAL.x, NORMAL.y, NORMAL.z, texCoordMaxCorner.x, texCoordMaxCorner.y,
        };


        const int NUM_COLOR_SCALARS_PER_QUAD = 4;
        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);
        int colors[NUM_COLOR_SCALARS_PER_QUAD] = { compactColor, compactColor, compactColor, compactColor };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
}

void GeometryCache::renderQuad(gpu::Batch& batch, const glm::vec3& minCorner, const glm::vec3& maxCorner, const glm::vec4& color, int id) {
    bool registered = (id != UNKNOWN_ID);
    Vec3PairVec4 key(Vec3Pair(minCorner, maxCorner), color);
    BatchItemDetails& details = _registeredQuad3D[id];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec3PairVec4& lastKey = _lastRegisteredQuad3D[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredQuad3D[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderQuad() 3D ... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderQuad() 3D ... REUSING PREVIOUSLY REGISTERED";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3 + 3; // vertices + normals
    const int VERTICES = 4; // 1 quad = 4 vertices
    const int NUM_POS_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);

    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = VERTICES;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();

        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;

        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        const glm::vec3 NORMAL(0.0f, 0.0f, 1.0f);
        float vertexBuffer[VERTICES * FLOATS_PER_VERTEX] = {
            minCorner.x, minCorner.y, minCorner.z, NORMAL.x, NORMAL.y, NORMAL.z,
            maxCorner.x, minCorner.y, minCorner.z, NORMAL.x, NORMAL.y, NORMAL.z,
            minCorner.x, maxCorner.y, maxCorner.z, NORMAL.x, NORMAL.y, NORMAL.z,
            maxCorner.x, maxCorner.y, maxCorner.z, NORMAL.x, NORMAL.y, NORMAL.z,
        };

        const int NUM_COLOR_SCALARS_PER_QUAD = 4;
        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);
        int colors[NUM_COLOR_SCALARS_PER_QUAD] = { compactColor, compactColor, compactColor, compactColor };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
}

void GeometryCache::renderQuad(gpu::Batch& batch, const glm::vec3& topLeft, const glm::vec3& bottomLeft,
    const glm::vec3& bottomRight, const glm::vec3& topRight,
    const glm::vec2& texCoordTopLeft, const glm::vec2& texCoordBottomLeft,
    const glm::vec2& texCoordBottomRight, const glm::vec2& texCoordTopRight,
    const glm::vec4& color, int id) {

#ifdef WANT_DEBUG
    qCDebug(renderutils) << "renderQuad() vec3 + texture VBO...";
    qCDebug(renderutils) << "    topLeft:" << topLeft;
    qCDebug(renderutils) << "    bottomLeft:" << bottomLeft;
    qCDebug(renderutils) << "    bottomRight:" << bottomRight;
    qCDebug(renderutils) << "    topRight:" << topRight;
    qCDebug(renderutils) << "    texCoordTopLeft:" << texCoordTopLeft;
    qCDebug(renderutils) << "    texCoordBottomRight:" << texCoordBottomRight;
    qCDebug(renderutils) << "    color:" << color;
#endif //def WANT_DEBUG

    bool registered = (id != UNKNOWN_ID);
    Vec3PairVec4Pair key(Vec3Pair(topLeft, bottomRight),
        Vec4Pair(glm::vec4(texCoordTopLeft.x, texCoordTopLeft.y, texCoordBottomRight.x, texCoordBottomRight.y),
        color));

    BatchItemDetails& details = _registeredQuad3DTextures[id];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec3PairVec4Pair& lastKey = _lastRegisteredQuad3DTexture[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredQuad3DTexture[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderQuad() 3D+texture ... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderQuad() 3D+texture ... REUSING PREVIOUSLY REGISTERED";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3 + 3 + 2; // vertices + normals + tex coords
    const int VERTICES = 4; // 1 quad = 4 vertices
    const int NUM_POS_COORDS = 3;
    const int NUM_NORMAL_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    const int VERTEX_TEXCOORD_OFFSET = VERTEX_NORMAL_OFFSET + NUM_NORMAL_COORDS * sizeof(float);


    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = VERTICES;
        details.vertexSize = FLOATS_PER_VERTEX; // NOTE: this isn't used for BatchItemDetails maybe we can get rid of it

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::TEXCOORD, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV), VERTEX_TEXCOORD_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        const glm::vec3 NORMAL(0.0f, 0.0f, 1.0f);
        float vertexBuffer[VERTICES * FLOATS_PER_VERTEX] = {
            bottomLeft.x, bottomLeft.y, bottomLeft.z, NORMAL.x, NORMAL.y, NORMAL.z, texCoordBottomLeft.x, texCoordBottomLeft.y,
            bottomRight.x, bottomRight.y, bottomRight.z, NORMAL.x, NORMAL.y, NORMAL.z, texCoordBottomRight.x, texCoordBottomRight.y,
            topLeft.x, topLeft.y, topLeft.z, NORMAL.x, NORMAL.y, NORMAL.z, texCoordTopLeft.x, texCoordTopLeft.y,
            topRight.x, topRight.y, topRight.z, NORMAL.x, NORMAL.y, NORMAL.z, texCoordTopRight.x, texCoordTopRight.y,
        };

        const int NUM_COLOR_SCALARS_PER_QUAD = 4;
        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);
        int colors[NUM_COLOR_SCALARS_PER_QUAD] = { compactColor, compactColor, compactColor, compactColor };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::TRIANGLE_STRIP, 4, 0);
}

void GeometryCache::renderDashedLine(gpu::Batch& batch, const glm::vec3& start, const glm::vec3& end, const glm::vec4& color,
    const float dash_length, const float gap_length, int id) {

    bool registered = (id != UNKNOWN_ID);
    Vec3PairVec2Pair key(Vec3Pair(start, end), Vec2Pair(glm::vec2(color.x, color.y), glm::vec2(color.z, color.w)));
    BatchItemDetails& details = _registeredDashedLines[id];

    // if this is a registered , and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        if (_lastRegisteredDashedLines[id] != key) {
            details.clear();
            _lastRegisteredDashedLines[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderDashedLine()... RELEASING REGISTERED";
#endif // def WANT_DEBUG
        }
    }

    if (!details.isCreated) {

        int compactColor = ((int(color.x * 255.0f) & 0xFF)) |
            ((int(color.y * 255.0f) & 0xFF) << 8) |
            ((int(color.z * 255.0f) & 0xFF) << 16) |
            ((int(color.w * 255.0f) & 0xFF) << 24);

        // draw each line segment with appropriate gaps
        const float SEGMENT_LENGTH = dash_length + gap_length;
        float length = glm::distance(start, end);
        float segmentCount = length / SEGMENT_LENGTH;
        int segmentCountFloor = (int)glm::floor(segmentCount);

        glm::vec3 segmentVector = (end - start) / segmentCount;
        glm::vec3 dashVector = segmentVector / SEGMENT_LENGTH * dash_length;
        glm::vec3 gapVector = segmentVector / SEGMENT_LENGTH * gap_length;

        const int FLOATS_PER_VERTEX = 3 + 3; // vertices + normals
        const int NUM_POS_COORDS = 3;
        const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
        details.vertices = (segmentCountFloor + 1) * 2;
        details.vertexSize = FLOATS_PER_VERTEX;
        details.isCreated = true;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);

        int* colorData = new int[details.vertices];
        int* colorDataAt = colorData;

        float* vertexData = new float[details.vertices * FLOATS_PER_VERTEX];
        float* vertex = vertexData;

        const glm::vec3 NORMAL(1.0f, 0.0f, 0.0f);
        glm::vec3 point = start;
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = point.z;
        *(vertex++) = NORMAL.x;
        *(vertex++) = NORMAL.y;
        *(vertex++) = NORMAL.z;
        *(colorDataAt++) = compactColor;

        for (int i = 0; i < segmentCountFloor; i++) {
            point += dashVector;
            *(vertex++) = point.x;
            *(vertex++) = point.y;
            *(vertex++) = point.z;
            *(vertex++) = NORMAL.x;
            *(vertex++) = NORMAL.y;
            *(vertex++) = NORMAL.z;
            *(colorDataAt++) = compactColor;

            point += gapVector;
            *(vertex++) = point.x;
            *(vertex++) = point.y;
            *(vertex++) = point.z;
            *(vertex++) = NORMAL.x;
            *(vertex++) = NORMAL.y;
            *(vertex++) = NORMAL.z;
            *(colorDataAt++) = compactColor;
        }
        *(vertex++) = end.x;
        *(vertex++) = end.y;
        *(vertex++) = end.z;
        *(vertex++) = NORMAL.x;
        *(vertex++) = NORMAL.y;
        *(vertex++) = NORMAL.z;
        *(colorDataAt++) = compactColor;

        details.verticesBuffer->append(sizeof(float) * FLOATS_PER_VERTEX * details.vertices, (gpu::Byte*) vertexData);
        details.colorBuffer->append(sizeof(int) * details.vertices, (gpu::Byte*) colorData);
        delete[] vertexData;
        delete[] colorData;

#ifdef WANT_DEBUG
        if (registered) {
            qCDebug(renderutils) << "new registered dashed line buffer made -- _registeredVertices:" << _registeredDashedLines.size();
        } else {
            qCDebug(renderutils) << "new dashed lines buffer made -- _dashedLines:" << _dashedLines.size();
        }
#endif
    }

    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::LINES, details.vertices, 0);
}


int GeometryCache::BatchItemDetails::population = 0;

GeometryCache::BatchItemDetails::BatchItemDetails() :
verticesBuffer(NULL),
colorBuffer(NULL),
streamFormat(NULL),
stream(NULL),
vertices(0),
vertexSize(0),
isCreated(false) {
    population++;
#ifdef WANT_DEBUG
    qCDebug(renderutils) << "BatchItemDetails()... population:" << population << "**********************************";
#endif
}

GeometryCache::BatchItemDetails::BatchItemDetails(const GeometryCache::BatchItemDetails& other) :
verticesBuffer(other.verticesBuffer),
colorBuffer(other.colorBuffer),
streamFormat(other.streamFormat),
stream(other.stream),
vertices(other.vertices),
vertexSize(other.vertexSize),
isCreated(other.isCreated) {
    population++;
#ifdef WANT_DEBUG
    qCDebug(renderutils) << "BatchItemDetails()... population:" << population << "**********************************";
#endif
}

GeometryCache::BatchItemDetails::~BatchItemDetails() {
    population--;
    clear();
#ifdef WANT_DEBUG
    qCDebug(renderutils) << "~BatchItemDetails()... population:" << population << "**********************************";
#endif
}

void GeometryCache::BatchItemDetails::clear() {
    isCreated = false;
    uniformBuffer.reset();
    verticesBuffer.reset();
    colorBuffer.reset();
    streamFormat.reset();
    stream.reset();
}

void GeometryCache::renderLine(gpu::Batch& batch, const glm::vec3& p1, const glm::vec3& p2,
    const glm::vec4& color1, const glm::vec4& color2, int id) {

    bool registered = (id != UNKNOWN_ID);
    Vec3Pair key(p1, p2);

    BatchItemDetails& details = _registeredLine3DVBOs[id];

    int compactColor1 = ((int(color1.x * 255.0f) & 0xFF)) |
        ((int(color1.y * 255.0f) & 0xFF) << 8) |
        ((int(color1.z * 255.0f) & 0xFF) << 16) |
        ((int(color1.w * 255.0f) & 0xFF) << 24);

    int compactColor2 = ((int(color2.x * 255.0f) & 0xFF)) |
        ((int(color2.y * 255.0f) & 0xFF) << 8) |
        ((int(color2.z * 255.0f) & 0xFF) << 16) |
        ((int(color2.w * 255.0f) & 0xFF) << 24);


    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec3Pair& lastKey = _lastRegisteredLine3D[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredLine3D[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderLine() 3D ... RELEASING REGISTERED line";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderLine() 3D ... REUSING PREVIOUSLY REGISTERED line";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3 + 3; // vertices + normals
    const int NUM_POS_COORDS = 3;
    const int VERTEX_NORMAL_OFFSET = NUM_POS_COORDS * sizeof(float);
    const int vertices = 2;
    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = vertices;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), VERTEX_NORMAL_OFFSET);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);

        const glm::vec3 NORMAL(1.0f, 0.0f, 0.0f);
        float vertexBuffer[vertices * FLOATS_PER_VERTEX] = {
            p1.x, p1.y, p1.z, NORMAL.x, NORMAL.y, NORMAL.z,
            p2.x, p2.y, p2.z, NORMAL.x, NORMAL.y, NORMAL.z };

        const int NUM_COLOR_SCALARS = 2;
        int colors[NUM_COLOR_SCALARS] = { compactColor1, compactColor2 };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);

#ifdef WANT_DEBUG
        if (id == UNKNOWN_ID) {
            qCDebug(renderutils) << "new renderLine() 3D VBO made -- _line3DVBOs.size():" << _line3DVBOs.size();
        } else {
            qCDebug(renderutils) << "new registered renderLine() 3D VBO made -- _registeredLine3DVBOs.size():" << _registeredLine3DVBOs.size();
        }
#endif
    }

    // this is what it takes to render a quad
    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::LINES, 2, 0);
}

void GeometryCache::renderLine(gpu::Batch& batch, const glm::vec2& p1, const glm::vec2& p2,
    const glm::vec4& color1, const glm::vec4& color2, int id) {

    bool registered = (id != UNKNOWN_ID);
    Vec2Pair key(p1, p2);

    BatchItemDetails& details = _registeredLine2DVBOs[id];

    int compactColor1 = ((int(color1.x * 255.0f) & 0xFF)) |
        ((int(color1.y * 255.0f) & 0xFF) << 8) |
        ((int(color1.z * 255.0f) & 0xFF) << 16) |
        ((int(color1.w * 255.0f) & 0xFF) << 24);

    int compactColor2 = ((int(color2.x * 255.0f) & 0xFF)) |
        ((int(color2.y * 255.0f) & 0xFF) << 8) |
        ((int(color2.z * 255.0f) & 0xFF) << 16) |
        ((int(color2.w * 255.0f) & 0xFF) << 24);


    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.isCreated) {
        Vec2Pair& lastKey = _lastRegisteredLine2D[id];
        if (lastKey != key) {
            details.clear();
            _lastRegisteredLine2D[id] = key;
#ifdef WANT_DEBUG
            qCDebug(renderutils) << "renderLine() 2D ... RELEASING REGISTERED line";
#endif // def WANT_DEBUG
        }
#ifdef WANT_DEBUG
        else {
            qCDebug(renderutils) << "renderLine() 2D ... REUSING PREVIOUSLY REGISTERED line";
        }
#endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2;
    const int vertices = 2;
    if (!details.isCreated) {

        details.isCreated = true;
        details.vertices = vertices;
        details.vertexSize = FLOATS_PER_VERTEX;

        auto verticesBuffer = std::make_shared<gpu::Buffer>();
        auto colorBuffer = std::make_shared<gpu::Buffer>();
        auto streamFormat = std::make_shared<gpu::Stream::Format>();
        auto stream = std::make_shared<gpu::BufferStream>();

        details.verticesBuffer = verticesBuffer;
        details.colorBuffer = colorBuffer;
        details.streamFormat = streamFormat;
        details.stream = stream;

        details.streamFormat->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
        details.streamFormat->setAttribute(gpu::Stream::COLOR, 1, gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA));

        details.stream->addBuffer(details.verticesBuffer, 0, details.streamFormat->getChannels().at(0)._stride);
        details.stream->addBuffer(details.colorBuffer, 0, details.streamFormat->getChannels().at(1)._stride);


        float vertexBuffer[vertices * FLOATS_PER_VERTEX] = { p1.x, p1.y, p2.x, p2.y };

        const int NUM_COLOR_SCALARS = 2;
        int colors[NUM_COLOR_SCALARS] = { compactColor1, compactColor2 };

        details.verticesBuffer->append(sizeof(vertexBuffer), (gpu::Byte*) vertexBuffer);
        details.colorBuffer->append(sizeof(colors), (gpu::Byte*) colors);

#ifdef WANT_DEBUG
        if (id == UNKNOWN_ID) {
            qCDebug(renderutils) << "new renderLine() 2D VBO made -- _line3DVBOs.size():" << _line2DVBOs.size();
        } else {
            qCDebug(renderutils) << "new registered renderLine() 2D VBO made -- _registeredLine2DVBOs.size():" << _registeredLine2DVBOs.size();
        }
#endif
    }

    // this is what it takes to render a quad
    batch.setInputFormat(details.streamFormat);
    batch.setInputStream(0, *details.stream);
    batch.draw(gpu::LINES, 2, 0);
}

void GeometryCache::useSimpleDrawPipeline(gpu::Batch& batch, bool noBlend) {
    static std::once_flag once;
    std::call_once(once, [&]() {
        auto program = gpu::Shader::createProgram(shader::render_utils::program::standardDrawTexture);

        auto state = std::make_shared<gpu::State>();

        // enable decal blend
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        PrepareStencil::testMaskResetNoAA(*state);

        _standardDrawPipeline = gpu::Pipeline::create(program, state);


        auto stateNoBlend = std::make_shared<gpu::State>();
        PrepareStencil::testMaskDrawShape(*stateNoBlend);

        auto programNoBlend = gpu::Shader::createProgram(shader::render_utils::program::standardDrawTextureNoBlend);
        _standardDrawPipelineNoBlend = gpu::Pipeline::create(programNoBlend, stateNoBlend);
    });

    if (noBlend) {
        batch.setPipeline(_standardDrawPipelineNoBlend);
    } else {
        batch.setPipeline(_standardDrawPipeline);
    }
}

void GeometryCache::useGridPipeline(gpu::Batch& batch, GridBuffer gridBuffer, bool transparent, bool forward) {
    if (_gridPipelines.empty()) {
        using namespace shader::render_utils::program;
        const float DEPTH_BIAS = 0.001f;

        static const std::vector<std::tuple<bool, bool, uint32_t>> keys = {
            std::make_tuple(false, false, grid), std::make_tuple(false, true, grid_forward), std::make_tuple(true, false, grid_translucent), std::make_tuple(true, true, grid_translucent_forward)
        };

        for (auto& key : keys) {
            gpu::StatePointer state = gpu::StatePointer(new gpu::State());
            state->setDepthTest(true, !std::get<0>(key), gpu::LESS_EQUAL);
            if (std::get<0>(key)) {
                PrepareStencil::testMaskResetNoAA(*state);
            } else {
                PrepareStencil::testMaskDrawShape(*state);
            }
            state->setBlendFunction(std::get<0>(key),
                gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
                gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
            state->setCullMode(gpu::State::CULL_NONE);
            state->setDepthBias(DEPTH_BIAS);

            _gridPipelines[{std::get<0>(key), std::get<1>(key)}] = gpu::Pipeline::create(gpu::Shader::createProgram(std::get<2>(key)), state);
        }
    }

    batch.setPipeline(_gridPipelines[{ transparent, forward }]);
    batch.setUniformBuffer(0, gridBuffer);
}



class SimpleProgramKey {
public:
    enum FlagBit {
        IS_TEXTURED_BIT = 0,
        IS_TRANSPARENT_BIT,
        IS_UNLIT_BIT,
        IS_DEPTH_BIASED_BIT,
        IS_FADING_BIT,
        IS_ANTIALIASED_BIT,
        IS_FORWARD_BIT,
        IS_CULL_FACE_NONE_BIT,   // if neither of these are set, we're CULL_FACE_BACK
        IS_CULL_FACE_FRONT_BIT,

        NUM_FLAGS,
    };
    typedef std::bitset<NUM_FLAGS> Flags;

    bool isTextured() const { return _flags[IS_TEXTURED_BIT]; }
    bool isTransparent() const { return _flags[IS_TRANSPARENT_BIT]; }
    bool isUnlit() const { return _flags[IS_UNLIT_BIT]; }
    bool hasDepthBias() const { return _flags[IS_DEPTH_BIASED_BIT]; }
    bool isFading() const { return _flags[IS_FADING_BIT]; }
    bool isAntiAliased() const { return _flags[IS_ANTIALIASED_BIT]; }
    bool isForward() const { return _flags[IS_FORWARD_BIT]; }
    bool isCullFaceNone() const { return _flags[IS_CULL_FACE_NONE_BIT]; }
    bool isCullFaceFront() const { return _flags[IS_CULL_FACE_FRONT_BIT]; }

    Flags _flags = 0;

    unsigned long getRaw() const { return _flags.to_ulong(); }

    SimpleProgramKey(bool textured = false, bool transparent = false, bool unlit = false, bool depthBias = false, bool fading = false,
        bool isAntiAliased = true, bool forward = false, graphics::MaterialKey::CullFaceMode cullFaceMode = graphics::MaterialKey::CULL_BACK) {
        _flags.set(IS_TEXTURED_BIT, textured);
        _flags.set(IS_TRANSPARENT_BIT, transparent);
        _flags.set(IS_UNLIT_BIT, unlit);
        _flags.set(IS_DEPTH_BIASED_BIT, depthBias);
        _flags.set(IS_FADING_BIT, fading);
        _flags.set(IS_ANTIALIASED_BIT, isAntiAliased);
        _flags.set(IS_FORWARD_BIT, forward);

        switch (cullFaceMode) {
            case graphics::MaterialKey::CullFaceMode::CULL_NONE:
                _flags.set(IS_CULL_FACE_NONE_BIT);
                _flags.reset(IS_CULL_FACE_FRONT_BIT);
                break;
            case graphics::MaterialKey::CullFaceMode::CULL_FRONT:
                _flags.reset(IS_CULL_FACE_NONE_BIT);
                _flags.set(IS_CULL_FACE_FRONT_BIT);
                break;
            case graphics::MaterialKey::CullFaceMode::CULL_BACK:
                _flags.reset(IS_CULL_FACE_NONE_BIT);
                _flags.reset(IS_CULL_FACE_FRONT_BIT);
                break;
            default:
                break;
        }
    }

    SimpleProgramKey(int bitmask) : _flags(bitmask) {}
};

inline uint qHash(const SimpleProgramKey& key, uint seed) {
    return qHash(key.getRaw(), seed);
}

inline bool operator==(const SimpleProgramKey& a, const SimpleProgramKey& b) {
    return a.getRaw() == b.getRaw();
}

void GeometryCache::bindWebBrowserProgram(gpu::Batch& batch, bool transparent, bool forward) {
    batch.setPipeline(getWebBrowserProgram(transparent, forward));
}

gpu::PipelinePointer GeometryCache::getWebBrowserProgram(bool transparent, bool forward) {
    if (_webPipelines.empty()) {
        using namespace shader::render_utils::program;
        const int NUM_WEB_PIPELINES = 4;
        for (int i = 0; i < NUM_WEB_PIPELINES; ++i) {
            bool transparent = i & 1;
            bool forward = i & 2;

            // For any non-opaque or non-deferred pipeline, we use web_browser_forward
            auto pipeline = (transparent || forward) ? web_browser_forward : web_browser;

            gpu::StatePointer state = gpu::StatePointer(new gpu::State());
            state->setDepthTest(true, !transparent, gpu::LESS_EQUAL);
            PrepareStencil::testMaskDrawShapeNoAA(*state);
            state->setBlendFunction(transparent,
                gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
                gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
            state->setCullMode(gpu::State::CULL_NONE);

            _webPipelines[{ transparent, forward }] = gpu::Pipeline::create(gpu::Shader::createProgram(pipeline), state);
        }
    }

    return _webPipelines[{ transparent, forward }];
}

void GeometryCache::bindSimpleProgram(gpu::Batch& batch, bool textured, bool transparent, bool unlit, bool depthBiased, bool isAntiAliased,
        bool forward, graphics::MaterialKey::CullFaceMode cullFaceMode) {
    batch.setPipeline(getSimplePipeline(textured, transparent, unlit, depthBiased, false, isAntiAliased, forward, cullFaceMode));

    // If not textured, set a default albedo map
    if (!textured) {
        batch.setResourceTexture(gr::Texture::MaterialAlbedo,
            DependencyManager::get<TextureCache>()->getWhiteTexture());
    }
}

gpu::PipelinePointer GeometryCache::getSimplePipeline(bool textured, bool transparent, bool unlit, bool depthBiased, bool fading, bool isAntiAliased,
        bool forward, graphics::MaterialKey::CullFaceMode cullFaceMode) {
    SimpleProgramKey config { textured, transparent, unlit, depthBiased, fading, isAntiAliased, forward, cullFaceMode };

    // If the pipeline already exists, return it
    auto it = _simplePrograms.find(config);
    if (it != _simplePrograms.end()) {
        return it.value();
    }

    // Compile the shaders
    if (!fading) {
        static std::once_flag once;
        std::call_once(once, [&]() {
            using namespace shader::render_utils::program;

            _forwardSimpleShader = gpu::Shader::createProgram(simple_forward);
            _forwardTransparentShader = gpu::Shader::createProgram(simple_translucent_forward);
            _forwardUnlitShader = gpu::Shader::createProgram(simple_unlit_forward);

            _simpleShader = gpu::Shader::createProgram(simple);
            _transparentShader = gpu::Shader::createProgram(simple_translucent);
            _unlitShader = gpu::Shader::createProgram(simple_unlit);
        });
    } else {
        static std::once_flag once;
        std::call_once(once, [&]() {
            using namespace shader::render_utils::program;
            // Fading is currently disabled during forward rendering
            _forwardSimpleFadeShader = gpu::Shader::createProgram(simple_forward);
            _forwardUnlitFadeShader = gpu::Shader::createProgram(simple_unlit_forward);

            _simpleFadeShader = gpu::Shader::createProgram(simple_fade);
            _unlitFadeShader = gpu::Shader::createProgram(simple_unlit_fade);
        });
    }

    // If the pipeline did not exist, make it
    auto state = std::make_shared<gpu::State>();
    if (config.isCullFaceNone()) {
        state->setCullMode(gpu::State::CULL_NONE);
    } else if (config.isCullFaceFront()) {
        state->setCullMode(gpu::State::CULL_FRONT);
    } else {
        state->setCullMode(gpu::State::CULL_BACK);
    }
    state->setDepthTest(true, !config.isTransparent(), gpu::LESS_EQUAL);
    if (config.hasDepthBias()) {
        state->setDepthBias(1.0f);
        state->setDepthBiasSlopeScale(1.0f);
    }
    state->setBlendFunction(config.isTransparent(),
        gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
        gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);

    if (config.isAntiAliased()) {
        config.isTransparent() ? PrepareStencil::testMaskResetNoAA(*state) : PrepareStencil::testMaskDrawShape(*state);
    } else {
        PrepareStencil::testMaskDrawShapeNoAA(*state);
    }

    gpu::ShaderPointer program;
    if (config.isForward()) {
        program = (config.isUnlit()) ? (config.isFading() ? _forwardUnlitFadeShader : _forwardUnlitShader) :
                                       (config.isFading() ? _forwardSimpleFadeShader : (config.isTransparent() ? _forwardTransparentShader : _forwardSimpleShader));
    } else {
        program = (config.isUnlit()) ? (config.isFading() ? _unlitFadeShader : _unlitShader) :
                                       (config.isFading() ? _simpleFadeShader : (config.isTransparent() ? _transparentShader : _simpleShader));
    }
    gpu::PipelinePointer pipeline = gpu::Pipeline::create(program, state);
    _simplePrograms.insert(config, pipeline);
    return pipeline;
}

uint32_t toCompactColor(const glm::vec4& color) {
    uint32_t compactColor = ((int(color.x * 255.0f) & 0xFF)) |
        ((int(color.y * 255.0f) & 0xFF) << 8) |
        ((int(color.z * 255.0f) & 0xFF) << 16) |
        ((int(color.w * 255.0f) & 0xFF) << 24);
    return compactColor;
}

static const size_t INSTANCE_COLOR_BUFFER = 0;

void renderInstances(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, bool isWire,
    const render::ShapePipelinePointer& pipeline, GeometryCache::Shape shape) {
    // Add pipeline to name
    std::string instanceName = (isWire ? "wire_shapes_" : "solid_shapes_") + std::to_string(shape) + "_" + std::to_string(std::hash<render::ShapePipelinePointer>()(pipeline));

    // Add color to named buffer
    {
        gpu::BufferPointer instanceColorBuffer = batch.getNamedBuffer(instanceName, INSTANCE_COLOR_BUFFER);
        auto compactColor = toCompactColor(color);
        instanceColorBuffer->append(compactColor);
    }

    // Add call to named buffer
    batch.setupNamedCalls(instanceName, [args, isWire, pipeline, shape](gpu::Batch& batch, gpu::Batch::NamedBatchData& data) {
        batch.setPipeline(pipeline->pipeline);
        pipeline->prepare(batch, args);

        if (isWire) {
            DependencyManager::get<GeometryCache>()->renderWireShapeInstances(batch, shape, data.count(), data.buffers[INSTANCE_COLOR_BUFFER]);
        } else {
            DependencyManager::get<GeometryCache>()->renderShapeInstances(batch, shape, data.count(), data.buffers[INSTANCE_COLOR_BUFFER]);
        }
    });
}

static const size_t INSTANCE_FADE_BUFFER1 = 1;
static const size_t INSTANCE_FADE_BUFFER2 = 2;
static const size_t INSTANCE_FADE_BUFFER3 = 3;

void renderFadeInstances(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, int fadeCategory, float fadeThreshold,
    const glm::vec3& fadeNoiseOffset, const glm::vec3& fadeBaseOffset, const glm::vec3& fadeBaseInvSize, bool isWire,
    const render::ShapePipelinePointer& pipeline, GeometryCache::Shape shape) {
    // Add pipeline to name
    std::string instanceName = (isWire ? "wire_shapes_" : "solid_shapes_") + std::to_string(shape) + "_" + std::to_string(std::hash<render::ShapePipelinePointer>()(pipeline));

    // Add color to named buffer
    {
        gpu::BufferPointer instanceColorBuffer = batch.getNamedBuffer(instanceName, INSTANCE_COLOR_BUFFER);
        auto compactColor = toCompactColor(color);
        instanceColorBuffer->append(compactColor);
    }
    // Add fade parameters to named buffers
    {
        gpu::BufferPointer fadeBuffer1 = batch.getNamedBuffer(instanceName, INSTANCE_FADE_BUFFER1);
        gpu::BufferPointer fadeBuffer2 = batch.getNamedBuffer(instanceName, INSTANCE_FADE_BUFFER2);
        gpu::BufferPointer fadeBuffer3 = batch.getNamedBuffer(instanceName, INSTANCE_FADE_BUFFER3);
        // Pack parameters in 3 vec4s
        glm::vec4 fadeData1;
        glm::vec4 fadeData2;
        glm::vec4 fadeData3;
        FadeEffect::packToAttributes(fadeCategory, fadeThreshold, fadeNoiseOffset, fadeBaseOffset, fadeBaseInvSize,
            fadeData1, fadeData2, fadeData3);
        fadeBuffer1->append(fadeData1);
        fadeBuffer2->append(fadeData2);
        fadeBuffer3->append(fadeData3);
    }

    // Add call to named buffer
    batch.setupNamedCalls(instanceName, [args, isWire, pipeline, shape](gpu::Batch& batch, gpu::Batch::NamedBatchData& data) {
        auto& buffers = data.buffers;
        batch.setPipeline(pipeline->pipeline);
        pipeline->prepare(batch, args);

        if (isWire) {
            DependencyManager::get<GeometryCache>()->renderWireFadeShapeInstances(batch, shape, data.count(),
                buffers[INSTANCE_COLOR_BUFFER], buffers[INSTANCE_FADE_BUFFER1], buffers[INSTANCE_FADE_BUFFER2], buffers[INSTANCE_FADE_BUFFER3]);
        }
        else {
            DependencyManager::get<GeometryCache>()->renderFadeShapeInstances(batch, shape, data.count(),
                buffers[INSTANCE_COLOR_BUFFER], buffers[INSTANCE_FADE_BUFFER1], buffers[INSTANCE_FADE_BUFFER2], buffers[INSTANCE_FADE_BUFFER3]);
        }
    });
}

void GeometryCache::renderSolidShapeInstance(RenderArgs* args, gpu::Batch& batch, GeometryCache::Shape shape, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderInstances(args, batch, color, false, pipeline, shape);
}

void GeometryCache::renderWireShapeInstance(RenderArgs* args, gpu::Batch& batch, GeometryCache::Shape shape, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderInstances(args, batch, color, true, pipeline, shape);
}

void GeometryCache::renderSolidFadeShapeInstance(RenderArgs* args, gpu::Batch& batch, GeometryCache::Shape shape, const glm::vec4& color,
    int fadeCategory, float fadeThreshold, const glm::vec3& fadeNoiseOffset, const glm::vec3& fadeBaseOffset, const glm::vec3& fadeBaseInvSize,
    const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderFadeInstances(args, batch, color, fadeCategory, fadeThreshold, fadeNoiseOffset, fadeBaseOffset, fadeBaseInvSize, false, pipeline, shape);
}

void GeometryCache::renderWireFadeShapeInstance(RenderArgs* args, gpu::Batch& batch, GeometryCache::Shape shape, const glm::vec4& color,
    int fadeCategory, float fadeThreshold, const glm::vec3& fadeNoiseOffset, const glm::vec3& fadeBaseOffset, const glm::vec3& fadeBaseInvSize,
    const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderFadeInstances(args, batch, color, fadeCategory, fadeThreshold, fadeNoiseOffset, fadeBaseOffset, fadeBaseInvSize, true, pipeline, shape);
}

void GeometryCache::renderSolidSphereInstance(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderInstances(args, batch, color, false, pipeline, GeometryCache::Sphere);
}

void GeometryCache::renderWireSphereInstance(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
    renderInstances(args, batch, color, true, pipeline, GeometryCache::Sphere);
}

// Enable this in a debug build to cause 'box' entities to iterate through all the
// available shape types, both solid and wireframes
//#define DEBUG_SHAPES


void GeometryCache::renderSolidCubeInstance(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    assert(pipeline != nullptr);
#ifdef DEBUG_SHAPES
    static auto startTime = usecTimestampNow();
    renderInstances(INSTANCE_NAME, batch, color, pipeline, [](gpu::Batch& batch, gpu::Batch::NamedBatchData& data) {

        auto usecs = usecTimestampNow();
        usecs -= startTime;
        auto msecs = usecs / USECS_PER_MSEC;
        float seconds = msecs;
        seconds /= MSECS_PER_SECOND;
        float fractionalSeconds = seconds - floor(seconds);
        int shapeIndex = (int)seconds;

        // Every second we flip to the next shape.
        static const int SHAPE_COUNT = 5;
        GeometryCache::Shape shapes[SHAPE_COUNT] = {
            GeometryCache::Cube,
            GeometryCache::Tetrahedron,
            GeometryCache::Sphere,
            GeometryCache::Icosahedron,
            GeometryCache::Line,
        };

        shapeIndex %= SHAPE_COUNT;
        GeometryCache::Shape shape = shapes[shapeIndex];

        // For the first half second for a given shape, show the wireframe, for the second half, show the solid.
        if (fractionalSeconds > 0.5f) {
            renderInstances(INSTANCE_NAME, batch, color, true, pipeline, shape);
        } else {
            renderInstances(INSTANCE_NAME, batch, color, false, pipeline, shape);
        }
    });
#else
    renderInstances(args, batch, color, false, pipeline, GeometryCache::Cube);
#endif
}

void GeometryCache::renderWireCubeInstance(RenderArgs* args, gpu::Batch& batch, const glm::vec4& color, const render::ShapePipelinePointer& pipeline) {
    static const std::string INSTANCE_NAME = __FUNCTION__;
    assert(pipeline != nullptr);
    renderInstances(args, batch, color, true, pipeline, GeometryCache::Cube);
}

graphics::MeshPointer GeometryCache::meshFromShape(Shape geometryShape, glm::vec3 color) {
    auto shapeData = getShapeData(geometryShape);

    qDebug() << "GeometryCache::getMeshProxyListFromShape" << shapeData << stringFromShape(geometryShape);

    auto positionsBufferView = buffer_helpers::clone(shapeData->_positionView);
    auto normalsBufferView = buffer_helpers::clone(shapeData->_normalView);
    auto indexBufferView = buffer_helpers::clone(shapeData->_indicesView);

    gpu::BufferView::Size numVertices = positionsBufferView.getNumElements();
    Q_ASSERT(numVertices == normalsBufferView.getNumElements());

    // apply input color across all vertices
    auto colorsBufferView = buffer_helpers::clone(shapeData->_normalView);
    for (gpu::BufferView::Size i = 0; i < numVertices; i++) {
        colorsBufferView.edit<glm::vec3>((gpu::BufferView::Index)i) = color;
    }

    graphics::MeshPointer mesh(new graphics::Mesh());
    mesh->setVertexBuffer(positionsBufferView);
    mesh->setIndexBuffer(indexBufferView);
    mesh->addAttribute(gpu::Stream::NORMAL, normalsBufferView);
    mesh->addAttribute(gpu::Stream::COLOR, colorsBufferView);

    const auto startIndex = 0, baseVertex = 0;
    graphics::Mesh::Part part(startIndex, (graphics::Index)indexBufferView.getNumElements(), baseVertex, graphics::Mesh::TRIANGLES);
    auto partBuffer = new gpu::Buffer(sizeof(graphics::Mesh::Part), (gpu::Byte*)&part);
    mesh->setPartBuffer(gpu::BufferView(partBuffer, gpu::Element::PART_DRAWCALL));

    mesh->modelName = GeometryCache::stringFromShape(geometryShape).toStdString();
    mesh->displayName = QString("GeometryCache/shape::%1").arg(GeometryCache::stringFromShape(geometryShape)).toStdString();

    return mesh;
}
