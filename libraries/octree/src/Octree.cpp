//
//  Octree.cpp
//  libraries/octree/src
//
//  Created by Stephen Birarda on 3/13/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Octree.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <fstream> // to load voxels from file

#include <QDataStream>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QVector>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include <GeometryUtil.h>
#include <Gzip.h>
#include <LogHandler.h>
#include <NetworkAccessManager.h>
#include <OctalCode.h>
#include <udt/PacketHeaders.h>
#include <ResourceManager.h>
#include <SharedUtil.h>
#include <PathUtils.h>
#include <ViewFrustum.h>

#include "OctreeConstants.h"
#include "OctreeLogging.h"
#include "OctreeQueryNode.h"
#include "OctreeUtils.h"
#include "OctreeEntitiesFileParser.h"

QVector<QString> PERSIST_EXTENSIONS = {"json", "json.gz"};

Octree::Octree(bool shouldReaverage) :
    _rootElement(NULL),
    _isDirty(true),
    _shouldReaverage(shouldReaverage),
    _isViewing(false),
    _isServer(false)
{
}

Octree::~Octree() {
    // This will delete all children, don't create a new root in this case.
    eraseAllOctreeElements(false);
}

// Recurses voxel tree calling the RecurseOctreeOperation function for each element.
// stops recursion if operation function returns false.
void Octree::recurseTreeWithOperation(const RecurseOctreeOperation& operation, void* extraData) {
    recurseElementWithOperation(_rootElement, operation, extraData);
}

// Recurses voxel element with an operation function
void Octree::recurseElementWithOperation(const OctreeElementPointer& element, const RecurseOctreeOperation& operation, void* extraData,
                        int recursionCount) {
    if (recursionCount > DANGEROUSLY_DEEP_RECURSION) {
        HIFI_FCDEBUG(octree(), "Octree::recurseElementWithOperation() reached DANGEROUSLY_DEEP_RECURSION, bailing!");
        return;
    }

    if (operation(element, extraData)) {
        for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
            OctreeElementPointer child = element->getChildAtIndex(i);
            if (child) {
                recurseElementWithOperation(child, operation, extraData, recursionCount + 1);
            }
        }
    }
}

void Octree::recurseTreeWithOperationSorted(const RecurseOctreeOperation& operation, const RecurseOctreeSortingOperation& sortingOperation, void* extraData) {
    recurseElementWithOperationSorted(_rootElement, operation, sortingOperation, extraData);
}

// Recurses voxel element with an operation function, calling operation on its children in a specific order
bool Octree::recurseElementWithOperationSorted(const OctreeElementPointer& element, const RecurseOctreeOperation& operation,
                                               const RecurseOctreeSortingOperation& sortingOperation, void* extraData, int recursionCount) {
    if (recursionCount > DANGEROUSLY_DEEP_RECURSION) {
        HIFI_FCDEBUG(octree(), "Octree::recurseElementWithOperationSorted() reached DANGEROUSLY_DEEP_RECURSION, bailing!");
        // If we go too deep, we want to keep searching other paths
        return true;
    }

    bool keepSearching = operation(element, extraData);

    std::vector<SortedChild> sortedChildren;
    for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
        OctreeElementPointer child = element->getChildAtIndex(i);
        if (child) {
            float priority = sortingOperation(child, extraData);
            if (priority < FLT_MAX) {
                sortedChildren.emplace_back(priority, child);
            }
        }
    }

    if (sortedChildren.size() > 1) {
        static auto comparator = [](const SortedChild& left, const SortedChild& right) { return left.first < right.first; };
        std::sort(sortedChildren.begin(), sortedChildren.end(), comparator);
    }

    for (auto it = sortedChildren.begin(); it != sortedChildren.end(); ++it) {
        const SortedChild& sortedChild = *it;
        // Our children were sorted, so if one hits something, we don't need to check the others
        if (!recurseElementWithOperationSorted(sortedChild.second, operation, sortingOperation, extraData, recursionCount + 1)) {
            return false;
        }
    }
    // We checked all our children and didn't find anything.
    // Stop if we hit something in this element.  Continue if we didn't.
    return keepSearching;
}

void Octree::recurseTreeWithOperator(RecurseOctreeOperator* operatorObject) {
    recurseElementWithOperator(_rootElement, operatorObject);
}

bool Octree::recurseElementWithOperator(const OctreeElementPointer& element,
                                        RecurseOctreeOperator* operatorObject, int recursionCount) {
    if (recursionCount > DANGEROUSLY_DEEP_RECURSION) {
        HIFI_FCDEBUG(octree(), "Octree::recurseElementWithOperator() reached DANGEROUSLY_DEEP_RECURSION, bailing!");
        return false;
    }

    if (operatorObject->preRecursion(element)) {
        for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
            OctreeElementPointer child = element->getChildAtIndex(i);

            // If there is no child at that location, the Operator may want to create a child at that location.
            // So give the operator a chance to do so....
            if (!child) {
                child = operatorObject->possiblyCreateChildAt(element, i);
            }

            if (child) {
                if (!recurseElementWithOperator(child, operatorObject, recursionCount + 1)) {
                    break; // stop recursing if operator returns false...
                }
            }
        }
    }

    return operatorObject->postRecursion(element);
}


OctreeElementPointer Octree::nodeForOctalCode(const OctreeElementPointer& ancestorElement, const unsigned char* needleCode,
                                              OctreeElementPointer* parentOfFoundElement) const {
    // special case for NULL octcode
    if (!needleCode) {
        return _rootElement;
    }

    // find the appropriate branch index based on this ancestorElement
    if (*needleCode > 0) {
        int branchForNeedle = branchIndexWithDescendant(ancestorElement->getOctalCode(), needleCode);
        OctreeElementPointer childElement = ancestorElement->getChildAtIndex(branchForNeedle);

        if (childElement) {
            if (*childElement->getOctalCode() == *needleCode) {

                // If the caller asked for the parent, then give them that too...
                if (parentOfFoundElement) {
                    *parentOfFoundElement = ancestorElement;
                }
                // the fact that the number of sections is equivalent does not always guarantee
                // that this is the same element, however due to the recursive traversal
                // we know that this is our element
                return childElement;
            } else {
                // we need to go deeper
                return nodeForOctalCode(childElement, needleCode, parentOfFoundElement);
            }
        }
    }

    // we've been given a code we don't have a element for
    // return this element as the last created parent
    return ancestorElement;
}

// returns the element created!
OctreeElementPointer Octree::createMissingElement(const OctreeElementPointer& lastParentElement,
                                                  const unsigned char* codeToReach, int recursionCount) {

    if (recursionCount > DANGEROUSLY_DEEP_RECURSION) {
        HIFI_FCDEBUG(octree(), "Octree::createMissingElement() reached DANGEROUSLY_DEEP_RECURSION, bailing!");
        return lastParentElement;
    }
    int indexOfNewChild = branchIndexWithDescendant(lastParentElement->getOctalCode(), codeToReach);

    // If this parent element is a leaf, then you know the child path doesn't exist, so deal with
    // breaking up the leaf first, which will also create a child path
    if (lastParentElement->requiresSplit()) {
        lastParentElement->splitChildren();
    } else if (!lastParentElement->getChildAtIndex(indexOfNewChild)) {
        // we could be coming down a branch that was already created, so don't stomp on it.
        lastParentElement->addChildAtIndex(indexOfNewChild);
    }

    // This works because we know we traversed down the same tree so if the length is the same, then the whole code is the same
    if (*lastParentElement->getChildAtIndex(indexOfNewChild)->getOctalCode() == *codeToReach) {
        return lastParentElement->getChildAtIndex(indexOfNewChild);
    } else {
        return createMissingElement(lastParentElement->getChildAtIndex(indexOfNewChild), codeToReach, recursionCount + 1);
    }
}

int Octree::readElementData(const OctreeElementPointer& destinationElement, const unsigned char* nodeData, int bytesAvailable,
                            ReadBitstreamToTreeParams& args) {

    int bytesLeftToRead = bytesAvailable;
    int bytesRead = 0;

    // give this destination element the child mask from the packet
    const unsigned char ALL_CHILDREN_ASSUMED_TO_EXIST = 0xFF;

    if ((size_t)bytesLeftToRead < sizeof(unsigned char)) {
        qCDebug(octree) << "UNEXPECTED: readElementData() only had " << bytesLeftToRead << " bytes. "
                    "Not enough for meaningful data.";
        return bytesAvailable; // assume we read the entire buffer...
    }

    if (destinationElement->getScale() < SCALE_AT_DANGEROUSLY_DEEP_RECURSION) {
        qCDebug(octree) << "UNEXPECTED: readElementData() destination element is unreasonably small ["
                << destinationElement->getScale() << " meters] "
                << " Discarding " << bytesAvailable << " remaining bytes.";
        return bytesAvailable; // assume we read the entire buffer...
    }

    unsigned char colorInPacketMask = *nodeData;
    bytesRead += sizeof(colorInPacketMask);
    bytesLeftToRead -= sizeof(colorInPacketMask);

    for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
        // check the colors mask to see if we have a child to color in
        if (oneAtBit(colorInPacketMask, i)) {
            // addChildAtIndex() should actually be called getOrAddChildAtIndex().
            // When it adds the child it automatically sets the detinationElement dirty.
            OctreeElementPointer childElementAt = destinationElement->addChildAtIndex(i);

            int childElementDataRead = childElementAt->readElementDataFromBuffer(nodeData + bytesRead, bytesLeftToRead, args);
            childElementAt->setSourceUUID(args.sourceUUID);

            bytesRead += childElementDataRead;
            bytesLeftToRead -= childElementDataRead;

            // It's possible that we already had this version of element data, in which case the unpacking of the data
            // wouldn't flag childElementAt as dirty, so we manually flag it here... if the element is to be rendered.
            if (childElementAt->getShouldRender() && !childElementAt->isRendered()) {
                childElementAt->setDirtyBit(); // force dirty!
                _isDirty = true;
            }
        }
        if (destinationElement->isDirty()) {
            _isDirty = true;
        }
    }

    unsigned char childrenInTreeMask = ALL_CHILDREN_ASSUMED_TO_EXIST;
    unsigned char childInBufferMask = 0;
    int bytesForMasks = args.includeExistsBits ? sizeof(childrenInTreeMask) + sizeof(childInBufferMask)
                                                : sizeof(childInBufferMask);

    if (bytesLeftToRead < bytesForMasks) {
        if (bytesLeftToRead > 0) {
            qCDebug(octree) << "UNEXPECTED: readElementDataFromBuffer() only had " << bytesLeftToRead << " bytes before masks. "
                        "Not enough for meaningful data.";
        }
        return bytesAvailable; // assume we read the entire buffer...
    }

    childrenInTreeMask = args.includeExistsBits ? *(nodeData + bytesRead) : ALL_CHILDREN_ASSUMED_TO_EXIST;
    childInBufferMask = *(nodeData + bytesRead + (args.includeExistsBits ? sizeof(childrenInTreeMask) : 0));

    int childIndex = 0;
    bytesRead += bytesForMasks;
    bytesLeftToRead -= bytesForMasks;

    while (bytesLeftToRead > 0 && childIndex < NUMBER_OF_CHILDREN) {
        // check the exists mask to see if we have a child to traverse into

        if (oneAtBit(childInBufferMask, childIndex)) {
            auto childAt = destinationElement->getChildAtIndex(childIndex);
            if (!childAt) {
                // add a child at that index, if it doesn't exist
                childAt = destinationElement->addChildAtIndex(childIndex);
                bool nodeIsDirty = destinationElement->isDirty();
                if (nodeIsDirty) {
                    _isDirty = true;
                }
            }

            // tell the child to read the subsequent data
            int lowerLevelBytes = readElementData(childAt, nodeData + bytesRead, bytesLeftToRead, args);

            bytesRead += lowerLevelBytes;
            bytesLeftToRead -= lowerLevelBytes;
        }
        childIndex++;
    }

    if (args.includeExistsBits) {
        for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
            // now also check the childrenInTreeMask, if the mask is missing the bit, then it means we need to delete this child
            // subtree/element, because it shouldn't actually exist in the tree.
            if (!oneAtBit(childrenInTreeMask, i) && destinationElement->getChildAtIndex(i)) {
                destinationElement->safeDeepDeleteChildAtIndex(i);
                _isDirty = true; // by definition!
            }
        }
    }

    // if this is the root, and there is more data to read, allow it to read it's element data...
    if (destinationElement == _rootElement  && rootElementHasData() && bytesLeftToRead > 0) {
        // tell the element to read the subsequent data
        int rootDataSize = _rootElement->readElementDataFromBuffer(nodeData + bytesRead, bytesLeftToRead, args);
        bytesRead += rootDataSize;
    }

    return bytesRead;
}

void Octree::readBitstreamToTree(const unsigned char * bitstream, uint64_t bufferSizeBytes,
                                 ReadBitstreamToTreeParams& args) {
    int bytesRead = 0;
    const unsigned char* bitstreamAt = bitstream;

    // If destination element is not included, set it to root
    if (!args.destinationElement) {
        args.destinationElement = _rootElement;
    }

    // Keep looping through the buffer calling readElementData() this allows us to pack multiple root-relative Octal codes
    // into a single network packet. readElementData() basically goes down a tree from the root, and fills things in from there
    // if there are more bytes after that, it's assumed to be another root relative tree

    while (bitstreamAt < bitstream + bufferSizeBytes) {
        OctreeElementPointer bitstreamRootElement = nodeForOctalCode(args.destinationElement,
                                                                     (unsigned char *)bitstreamAt, NULL);
        int numberOfThreeBitSectionsInStream = numberOfThreeBitSectionsInCode(bitstreamAt, bufferSizeBytes);
        if (numberOfThreeBitSectionsInStream > UNREASONABLY_DEEP_RECURSION) {
            HIFI_FCDEBUG(octree(), "UNEXPECTED: parsing of the octal code would make UNREASONABLY_DEEP_RECURSION... "
                        "numberOfThreeBitSectionsInStream:" << numberOfThreeBitSectionsInStream <<
                        "This buffer is corrupt. Returning.");
            return;
        }

        if (numberOfThreeBitSectionsInStream == OVERFLOWED_OCTCODE_BUFFER) {
            qCDebug(octree) << "UNEXPECTED: parsing of the octal code would overflow the buffer. "
                        "This buffer is corrupt. Returning.";
            return;
        }

        int numberOfThreeBitSectionsFromNode = numberOfThreeBitSectionsInCode(bitstreamRootElement->getOctalCode());

        // if the octal code returned is not on the same level as the code being searched for, we have OctreeElements to create
        if (numberOfThreeBitSectionsInStream != numberOfThreeBitSectionsFromNode) {

            // Note: we need to create this element relative to root, because we're assuming that the bitstream for the initial
            // octal code is always relative to root!
            bitstreamRootElement = createMissingElement(args.destinationElement, (unsigned char*) bitstreamAt);
            if (bitstreamRootElement->isDirty()) {
                _isDirty = true;
            }
        }

        auto octalCodeBytes = bytesRequiredForCodeLength(numberOfThreeBitSectionsInStream);

        int theseBytesRead = 0;
        theseBytesRead += (int)octalCodeBytes;
        int lowerLevelBytes = readElementData(bitstreamRootElement, bitstreamAt + octalCodeBytes,
                                       bufferSizeBytes - (bytesRead + (int)octalCodeBytes), args);

        theseBytesRead += lowerLevelBytes;

        // skip bitstream to new startPoint
        bitstreamAt += theseBytesRead;
        bytesRead += theseBytesRead;
    }
}

void Octree::eraseAllOctreeElements(bool createNewRoot) {
    if (createNewRoot) {
        _rootElement = createNewElement();
    } else {
        _rootElement.reset(); // this will recurse and delete all children
    }

    _isDirty = true;
}

// Note: this is an expensive call. Don't call it unless you really need to reaverage the entire tree (from startElement)
void Octree::reaverageOctreeElements(OctreeElementPointer startElement) {
    if (!startElement) {
        startElement = getRoot();
    }
    // if our tree is a reaveraging tree, then we do this, otherwise we don't do anything
    if (_shouldReaverage) {
        static int recursionCount;
        if (startElement == _rootElement) {
            recursionCount = 0;
        } else {
            recursionCount++;
        }
        if (recursionCount > UNREASONABLY_DEEP_RECURSION) {
            qCDebug(octree, "Octree::reaverageOctreeElements()... bailing out of UNREASONABLY_DEEP_RECURSION");
            recursionCount--;
            return;
        }

        bool hasChildren = false;

        for (int i = 0; i < NUMBER_OF_CHILDREN; i++) {
            if (startElement->getChildAtIndex(i)) {
                reaverageOctreeElements(startElement->getChildAtIndex(i));
                hasChildren = true;
            }
        }

        // collapseIdenticalLeaves() returns true if it collapses the leaves
        // in which case we don't need to set the average color
        if (hasChildren && !startElement->collapseChildren()) {
            startElement->calculateAverageFromChildren();
        }
        recursionCount--;
    }
}

OctreeElementPointer Octree::getOctreeElementAt(float x, float y, float z, float s) const {
    unsigned char* octalCode = pointToOctalCode(x,y,z,s);
    OctreeElementPointer element = nodeForOctalCode(_rootElement, octalCode, NULL);
    if (*element->getOctalCode() != *octalCode) {
        element = NULL;
    }
    delete[] octalCode; // cleanup memory
    return element;
}

OctreeElementPointer Octree::getOctreeEnclosingElementAt(float x, float y, float z, float s) const {
    unsigned char* octalCode = pointToOctalCode(x,y,z,s);
    OctreeElementPointer element = nodeForOctalCode(_rootElement, octalCode, NULL);

    delete[] octalCode; // cleanup memory
    return element;
}


OctreeElementPointer Octree::getOrCreateChildElementAt(float x, float y, float z, float s) {
    return getRoot()->getOrCreateChildElementAt(x, y, z, s);
}

OctreeElementPointer Octree::getOrCreateChildElementContaining(const AACube& box) {
    return getRoot()->getOrCreateChildElementContaining(box);
}



class SphereArgs {
public:
    glm::vec3 center;
    float radius;
    glm::vec3& penetration;
    bool found;
    void* penetratedObject; /// the type is defined by the type of Octree, the caller is assumed to know the type
};

bool findSpherePenetrationOp(const OctreeElementPointer& element, void* extraData) {
    SphereArgs* args = static_cast<SphereArgs*>(extraData);

    // coarse check against bounds
    if (!element->getAACube().expandedContains(args->center, args->radius)) {
        return false;
    }
    if (element->hasContent()) {
        glm::vec3 elementPenetration;
        if (element->findSpherePenetration(args->center, args->radius, elementPenetration, &args->penetratedObject)) {
            // NOTE: it is possible for this penetration accumulation algorithm to produce a
            // final penetration vector with zero length.
            args->penetration = addPenetrations(args->penetration, elementPenetration);
            args->found = true;
        }
    }
    if (!element->isLeaf()) {
        return true; // recurse on children
    }
    return false;
}

bool Octree::findSpherePenetration(const glm::vec3& center, float radius, glm::vec3& penetration,
                    void** penetratedObject, Octree::lockType lockType, bool* accurateResult) {

    SphereArgs args = {
        center,
        radius,
        penetration,
        false,
        NULL };
    penetration = glm::vec3(0.0f, 0.0f, 0.0f);

    bool requireLock = lockType == Octree::Lock;
    bool lockResult = withReadLock([&]{
        recurseTreeWithOperation(findSpherePenetrationOp, &args);
        if (penetratedObject) {
            *penetratedObject = args.penetratedObject;
        }
    }, requireLock);

    if (accurateResult) {
        *accurateResult = lockResult; // if user asked to accuracy or result, let them know this is accurate
    }
    return args.found;
}

class CapsuleArgs {
public:
    glm::vec3 start;
    glm::vec3 end;
    float radius;
    glm::vec3& penetration;
    bool found;
};

class ContentArgs {
public:
    AACube cube;
    CubeList* cubes;
};

bool findCapsulePenetrationOp(const OctreeElementPointer& element, void* extraData) {
    CapsuleArgs* args = static_cast<CapsuleArgs*>(extraData);

    // coarse check against bounds
    if (!element->getAACube().expandedIntersectsSegment(args->start, args->end, args->radius)) {
        return false;
    }
    if (element->hasContent()) {
        glm::vec3 nodePenetration;
        if (element->getAACube().findCapsulePenetration(args->start, args->end, args->radius, nodePenetration)) {
            args->penetration = addPenetrations(args->penetration, nodePenetration);
            args->found = true;
        }
    }
    if (!element->isLeaf()) {
        return true; // recurse on children
    }
    return false;
}

uint qHash(const glm::vec3& point) {
    // NOTE: TREE_SCALE = 16384 (15 bits) and multiplier is 1024 (11 bits),
    // so each component (26 bits) uses more than its alloted 21 bits.
    // however we don't expect to span huge cubes so it is ok if we wrap
    // (every 2^21 / 2^10 = 2048 meters).
    const uint BITS_PER_COMPONENT = 21;
    const quint64 MAX_SCALED_COMPONENT = 2097152; // 2^21
    const float RESOLUTION_PER_METER = 1024.0f; // 2^10
    return qHash((quint64)(point.x * RESOLUTION_PER_METER) % MAX_SCALED_COMPONENT +
        (((quint64)(point.y * RESOLUTION_PER_METER)) % MAX_SCALED_COMPONENT << BITS_PER_COMPONENT) +
        (((quint64)(point.z * RESOLUTION_PER_METER)) % MAX_SCALED_COMPONENT << 2 * BITS_PER_COMPONENT));
}

bool findContentInCubeOp(const OctreeElementPointer& element, void* extraData) {
    ContentArgs* args = static_cast<ContentArgs*>(extraData);

    // coarse check against bounds
    const AACube& cube = element->getAACube();
    if (!cube.touches(args->cube)) {
        return false;
    }
    if (!element->isLeaf()) {
        return true; // recurse on children
    }
    if (element->hasContent()) {
        // NOTE: the voxel's center is unique so we use it as the input for the key.
        // We use the qHash(glm::vec()) as the key as an optimization for the code that uses CubeLists.
        args->cubes->insert(qHash(cube.calcCenter()), cube);
        return true;
    }
    return false;
}

bool Octree::findCapsulePenetration(const glm::vec3& start, const glm::vec3& end, float radius,
                    glm::vec3& penetration, Octree::lockType lockType, bool* accurateResult) {

    CapsuleArgs args = { start, end, radius, penetration, false };
    penetration = glm::vec3(0.0f, 0.0f, 0.0f);


    bool requireLock = lockType == Octree::Lock;
    bool lockResult = withReadLock([&]{
        recurseTreeWithOperation(findCapsulePenetrationOp, &args);
    }, requireLock);

    if (accurateResult) {
        *accurateResult = lockResult; // if user asked to accuracy or result, let them know this is accurate
    }

    return args.found;
}

bool Octree::findContentInCube(const AACube& cube, CubeList& cubes) {
    return withTryReadLock([&]{
        ContentArgs args = { cube, &cubes };
        recurseTreeWithOperation(findContentInCubeOp, &args);
    });
}

class GetElementEnclosingArgs {
public:
    OctreeElementPointer element;
    glm::vec3 point;
};

// Find the smallest colored voxel enclosing a point (if there is one)
bool getElementEnclosingOperation(const OctreeElementPointer& element, void* extraData) {
    GetElementEnclosingArgs* args = static_cast<GetElementEnclosingArgs*>(extraData);
    if (element->getAACube().contains(args->point)) {
        if (element->hasContent() && element->isLeaf()) {
            // we've reached a solid leaf containing the point, return the element.
            args->element = element;
            return false;
        }
    } else {
        //  The point is not inside this voxel, so stop recursing.
        return false;
    }
    return true; // keep looking
}

OctreeElementPointer Octree::getElementEnclosingPoint(const glm::vec3& point, Octree::lockType lockType, bool* accurateResult) {
    GetElementEnclosingArgs args;
    args.point = point;
    args.element = NULL;

    bool requireLock = lockType == Octree::Lock;
    bool lockResult = withReadLock([&]{
        recurseTreeWithOperation(getElementEnclosingOperation, (void*)&args);
    }, requireLock);

    if (accurateResult) {
        *accurateResult = lockResult; // if user asked to accuracy or result, let them know this is accurate
    }

    return args.element;
}

bool Octree::readFromFile(const char* fileName) {
    QString qFileName = findMostRecentFileExtension(fileName, PERSIST_EXTENSIONS);

    if (qFileName.endsWith(".json.gz")) {
        return readJSONFromGzippedFile(qFileName);
    }

    QFile file(qFileName);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream fileInputStream(&file);
    QFileInfo fileInfo(qFileName);
    uint64_t fileLength = fileInfo.size();
    QUrl relativeURL = QUrl::fromLocalFile(qFileName).adjusted(QUrl::RemoveFilename);

    bool success = readFromStream(fileLength, fileInputStream, false, relativeURL);

    file.close();

    return success;
}

bool Octree::readJSONFromGzippedFile(QString qFileName) {
    QFile file(qFileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Cannot open gzipped json file for reading: " << qFileName;
        return false;
    }
    QByteArray compressedJsonData = file.readAll();
    QByteArray jsonData;

    if (!gunzip(compressedJsonData, jsonData)) {
        qCritical() << "json File not in gzip format: " << qFileName;
        return false;
    }

    QDataStream jsonStream(jsonData);
    QUrl relativeURL = QUrl::fromLocalFile(qFileName).adjusted(QUrl::RemoveFilename);

    return readJSONFromStream(-1, jsonStream, false, relativeURL);
}

bool Octree::readFromURL(
    const QString& urlString,
    const bool isObservable,
    const qint64 callerId,
    const bool isImport
) {
    QString trimmedUrl = urlString.trimmed();
    auto request = std::unique_ptr<ResourceRequest>(
        DependencyManager::get<ResourceManager>()->createResourceRequest(
            this, trimmedUrl, isObservable, callerId, "Octree::readFromURL"));

    if (!request) {
        return false;
    }

    QEventLoop loop;
    connect(request.get(), &ResourceRequest::finished, &loop, &QEventLoop::quit);
    request->send();
    loop.exec();

    if (request->getResult() != ResourceRequest::Success) {
        return false;
    }

    auto data = request->getData();

    QByteArray uncompressedJsonData;
    bool wasCompressed = gunzip(data, uncompressedJsonData);

    QUrl relativeURL = QUrl(urlString).adjusted(QUrl::RemoveFilename);

    if (wasCompressed) {
        QDataStream inputStream(uncompressedJsonData);
        return readFromStream(uncompressedJsonData.size(), inputStream, isImport, relativeURL);
    }

    QDataStream inputStream(data);
    return readFromStream(data.size(), inputStream, isImport, relativeURL);
}

bool Octree::readFromByteArray(
    const QString& urlString,
    const QByteArray& data
) {
    QString trimmedUrl = urlString.trimmed();

    QByteArray uncompressedJsonData;
    bool wasCompressed = gunzip(data, uncompressedJsonData);

    QUrl relativeURL = QUrl(urlString).adjusted(QUrl::RemoveFilename);

    if (wasCompressed) {
        QDataStream inputStream(uncompressedJsonData);
        return readFromStream(uncompressedJsonData.size(), inputStream, false, relativeURL);
    }

    QDataStream inputStream(data);
    return readFromStream(data.size(), inputStream, false, relativeURL);
}

bool Octree::readFromStream(
    uint64_t streamLength,
    QDataStream& inputStream,
    const bool isImport,
    const QUrl& relativeURL
) {
    // decide if this is binary SVO or JSON-formatted SVO
    QIODevice *device = inputStream.device();
    char firstChar;
    device->getChar(&firstChar);
    device->ungetChar(firstChar);

    if (firstChar == (char) PacketType::EntityData) {
        qCWarning(octree) << "Reading from binary SVO no longer supported";
        return false;
    } else {
        qCDebug(octree) << "Reading from JSON SVO Stream length:" << streamLength;
        return readJSONFromStream(streamLength, inputStream, isImport, relativeURL);
    }
}


const int READ_JSON_BUFFER_SIZE = 2048;

bool Octree::readJSONFromStream(
    uint64_t streamLength,
    QDataStream& inputStream,
    const bool isImport,
    const QUrl& relativeURL
) {
    // if the data is gzipped we may not have a useful bytesAvailable() result, so just keep reading until
    // we get an eof.  Leave streamLength parameter for consistency.

    QByteArray jsonBuffer;
    char* rawData = new char[READ_JSON_BUFFER_SIZE];
    while (!inputStream.atEnd()) {
        int got = inputStream.readRawData(rawData, READ_JSON_BUFFER_SIZE - 1);
        if (got < 0) {
            qCritical() << "error while reading from json stream";
            delete[] rawData;
            return false;
        }
        if (got == 0) {
            break;
        }
        jsonBuffer += QByteArray(rawData, got);
    }

    OctreeEntitiesFileParser octreeParser;
    octreeParser.setRelativeURL(relativeURL);
    octreeParser.setEntitiesString(jsonBuffer);

    QVariantMap asMap;
    if (!octreeParser.parseEntities(asMap)) {
        qCritical() << "Couldn't parse Entities JSON:" << octreeParser.getErrorString().c_str();
        return false;
    }

    bool success = readFromMap(asMap, isImport);
    delete[] rawData;
    return success;
}

bool Octree::writeToFile(const char* fileName, const OctreeElementPointer& element, QString persistAsFileType) {
    // make the sure file extension makes sense
    QString qFileName = fileNameWithoutExtension(QString(fileName), PERSIST_EXTENSIONS) + "." + persistAsFileType;
    QByteArray byteArray = qFileName.toUtf8();
    const char* cFileName = byteArray.constData();

    bool success = false;
    if (persistAsFileType == "json") {
        success = writeToJSONFile(cFileName, element);
    } else if (persistAsFileType == "json.gz") {
        success = writeToJSONFile(cFileName, element, true);
    } else {
        qCDebug(octree) << "unable to write octree to file of type" << persistAsFileType;
    }
    return success;
}

bool Octree::toJSONDocument(QJsonDocument* doc, const OctreeElementPointer& element) {
    QVariantMap entityDescription;

    OctreeElementPointer top;
    if (element) {
        top = element;
    } else {
        top = _rootElement;
    }

    // include the "bitstream" version
    PacketType expectedType = expectedDataPacketType();
    PacketVersion expectedVersion = versionForPacketType(expectedType);
    entityDescription["Version"] = (int) expectedVersion;

    // store the entity data
    bool entityDescriptionSuccess = writeToMap(entityDescription, top, true, true);
    if (!entityDescriptionSuccess) {
        qCritical("Failed to convert Entities to QVariantMap while saving to json.");
        return false;
    }


    bool noEntities = entityDescription["Entities"].toList().empty();
    QJsonDocument jsonDocTree = QJsonDocument::fromVariant(entityDescription);
    QJsonValue entitiesJson = jsonDocTree["Entities"];
    if (entitiesJson.isNull() || (entitiesJson.toArray().empty() && !noEntities)) {
        // Json version of entities too large.
        return false;
    } else {
        *doc = jsonDocTree;
    }

    return true;
}

bool Octree::toJSONString(QString& jsonString, const OctreeElementPointer& element) {
    OctreeElementPointer top;
    if (element) {
        top = element;
    } else {
        top = _rootElement;
    }

    jsonString += QString("{\n  \"DataVersion\": %1,\n  \"Entities\": [").arg(_persistDataVersion);

    writeToJSON(jsonString, top);

    // include the "bitstream" version
    PacketType expectedType = expectedDataPacketType();
    PacketVersion expectedVersion = versionForPacketType(expectedType);

    jsonString += QString("\n    ],\n  \"Id\": \"%1\",\n  \"Version\": %2\n}\n").arg(_persistID.toString()).arg((int)expectedVersion);

    return true;
}

bool Octree::toJSON(QByteArray* data, const OctreeElementPointer& element, bool doGzip) {
    QString jsonString;
    toJSONString(jsonString);

    if (doGzip) {
        if (!gzip(jsonString.toUtf8(), *data, -1)) {
            qCritical("Unable to gzip data while saving to json.");
            return false;
        }
    } else {
        *data = jsonString.toUtf8();
    }

    return true;
}

bool Octree::writeToJSONFile(const char* fileName, const OctreeElementPointer& element, bool doGzip) {
    qCDebug(octree, "Saving JSON SVO to file %s...", fileName);

    QByteArray jsonDataForFile;
    if (!toJSON(&jsonDataForFile, element, doGzip)) {
        return false;
    }

    QSaveFile persistFile(fileName);
    bool success = false;
    if (persistFile.open(QIODevice::WriteOnly)) {
        if (persistFile.write(jsonDataForFile) != -1) {
            success = persistFile.commit();
            if (!success) {
                qCritical() << "Failed to commit to JSON save file:" << persistFile.errorString();
            }
        } else {
            qCritical("Failed to write to JSON file.");
        }
    } else {
        qCritical("Failed to open JSON file for writing.");
    }

    return success;
}

uint64_t Octree::getOctreeElementsCount() {
    uint64_t nodeCount = 0;
    recurseTreeWithOperation(countOctreeElementsOperation, &nodeCount);
    return nodeCount;
}

bool Octree::countOctreeElementsOperation(const OctreeElementPointer& element, void* extraData) {
    (*(uint64_t*)extraData)++;
    return true; // keep going
}
