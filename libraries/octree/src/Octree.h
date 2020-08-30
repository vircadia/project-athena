//
//  Octree.h
//  libraries/octree/src
//
//  Created by Stephen Birarda on 3/13/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Octree_h
#define hifi_Octree_h

#include <memory>
#include <set>
#include <stdint.h>

#include <QHash>
#include <QObject>
#include <QtCore/QJsonObject>

#include <shared/ReadWriteLockable.h>
#include <SimpleMovingAverage.h>
#include <ViewFrustum.h>

#include "OctreeElement.h"
#include "OctreeElementBag.h"
#include "OctreePacketData.h"
#include "OctreeSceneStats.h"
#include "OctreeUtils.h"

class ReadBitstreamToTreeParams;
class Octree;
class OctreeElement;
class OctreePacketData;
class Shape;
using OctreePointer = std::shared_ptr<Octree>;

extern QVector<QString> PERSIST_EXTENSIONS;

/// derive from this class to use the Octree::recurseTreeWithOperator() method
class RecurseOctreeOperator {
public:
    virtual bool preRecursion(const OctreeElementPointer& element) = 0;
    virtual bool postRecursion(const OctreeElementPointer& element) = 0;
    virtual OctreeElementPointer possiblyCreateChildAt(const OctreeElementPointer& element, int childIndex) { return NULL; }
};

// Callback function, for recuseTreeWithOperation
using RecurseOctreeOperation = std::function<bool(const OctreeElementPointer&, void*)>;
// Function for sorting octree children during recursion.  If return value == FLT_MAX, child is discarded
using RecurseOctreeSortingOperation = std::function<float(const OctreeElementPointer&, void*)>;
using SortedChild = std::pair<float, OctreeElementPointer>;
typedef QHash<uint, AACube> CubeList;

const bool NO_EXISTS_BITS         = false;
const bool WANT_EXISTS_BITS       = true;

const int NO_BOUNDARY_ADJUST     = 0;
const int LOW_RES_MOVING_ADJUST  = 1;

class EncodeBitstreamParams {
public:
    bool includeExistsBits;
    NodeData* nodeData;

    // output hints from the encode process
    typedef enum {
        UNKNOWN,
        DIDNT_FIT,
        FINISHED
    } reason;
    reason stopReason;

    EncodeBitstreamParams(bool includeExistsBits = WANT_EXISTS_BITS,
                          NodeData* nodeData = nullptr) :
            includeExistsBits(includeExistsBits),
            nodeData(nodeData),
            stopReason(UNKNOWN)
    {
    }

    void displayStopReason() {
        printf("StopReason: ");
        switch (stopReason) {
            case UNKNOWN: qDebug("UNKNOWN"); break;
            case DIDNT_FIT: qDebug("DIDNT_FIT"); break;
            case FINISHED: qDebug("FINISHED"); break;
        }
    }

    QString getStopReason() {
        switch (stopReason) {
            case UNKNOWN: return QString("UNKNOWN"); break;
            case DIDNT_FIT: return QString("DIDNT_FIT"); break;
            case FINISHED: return QString("FINISHED"); break;
        }
    }

    std::function<void(const QUuid& dataID, quint64 itemLastEdited)> trackSend { [](const QUuid&, quint64){} };
};

class ReadBitstreamToTreeParams {
public:
    bool includeExistsBits;
    OctreeElementPointer destinationElement;
    QUuid sourceUUID;
    SharedNodePointer sourceNode;
    int elementsPerPacket = 0;
    int entitiesPerPacket = 0;

    ReadBitstreamToTreeParams(
        bool includeExistsBits = WANT_EXISTS_BITS,
        OctreeElementPointer destinationElement = NULL,
        QUuid sourceUUID = QUuid(),
        SharedNodePointer sourceNode = SharedNodePointer()) :
            includeExistsBits(includeExistsBits),
            destinationElement(destinationElement),
            sourceUUID(sourceUUID),
            sourceNode(sourceNode)
    {}
};

class Octree : public QObject, public std::enable_shared_from_this<Octree>, public ReadWriteLockable {
    Q_OBJECT
public:
    Octree(bool shouldReaverage = false);
    virtual ~Octree();

    /// Your tree class must implement this to create the correct element type
    virtual OctreeElementPointer createNewElement(unsigned char * octalCode = NULL) = 0;

    // These methods will allow the OctreeServer to send your tree inbound edit packets of your
    // own definition. Implement these to allow your octree based server to support editing
    virtual PacketType expectedDataPacketType() const { return PacketType::Unknown; }
    virtual PacketVersion expectedVersion() const { return versionForPacketType(expectedDataPacketType()); }
    virtual bool handlesEditPacketType(PacketType packetType) const { return false; }
    virtual int processEditPacketData(ReceivedMessage& message, const unsigned char* editData, int maxLength,
                                      const SharedNodePointer& sourceNode) { return 0; }

    virtual bool rootElementHasData() const { return false; }
    virtual void releaseSceneEncodeData(OctreeElementExtraEncodeData* extraEncodeData) const { }

    // Why preUpdate() and update()?
    // Because EntityTree needs them.
    virtual void preUpdate() { }
    virtual void update(bool simulate = true) { }

    OctreeElementPointer getRoot() { return _rootElement; }

    virtual void eraseDomainAndNonOwnedEntities() { _isDirty = true; };
    virtual void eraseAllOctreeElements(bool createNewRoot = true);

    virtual void readBitstreamToTree(const unsigned char* bitstream,  uint64_t bufferSizeBytes, ReadBitstreamToTreeParams& args);
    void reaverageOctreeElements(OctreeElementPointer startElement = OctreeElementPointer());

    /// Find the voxel at position x,y,z,s
    /// \return pointer to the OctreeElement or NULL if none at x,y,z,s.
    OctreeElementPointer getOctreeElementAt(float x, float y, float z, float s) const;

    /// Find the voxel at position x,y,z,s
    /// \return pointer to the OctreeElement or to the smallest enclosing parent if none at x,y,z,s.
    OctreeElementPointer getOctreeEnclosingElementAt(float x, float y, float z, float s) const;

    OctreeElementPointer getOrCreateChildElementAt(float x, float y, float z, float s);
    OctreeElementPointer getOrCreateChildElementContaining(const AACube& box);

    void recurseTreeWithOperation(const RecurseOctreeOperation& operation, void* extraData = NULL);
    void recurseTreeWithOperationSorted(const RecurseOctreeOperation& operation, const RecurseOctreeSortingOperation& sortingOperation, void* extraData = NULL);

    void recurseTreeWithOperator(RecurseOctreeOperator* operatorObject);

    bool isDirty() const { return _isDirty; }
    void clearDirtyBit() { _isDirty = false; }
    void setDirtyBit() { _isDirty = true; }

    // output hints from the encode process
    typedef enum {
        Lock,
        TryLock
    } lockType;


    bool findSpherePenetration(const glm::vec3& center, float radius, glm::vec3& penetration, void** penetratedObject = NULL,
                                    Octree::lockType lockType = Octree::TryLock, bool* accurateResult = NULL);

    bool findCapsulePenetration(const glm::vec3& start, const glm::vec3& end, float radius, glm::vec3& penetration,
                                    Octree::lockType lockType = Octree::TryLock, bool* accurateResult = NULL);

    /// \param cube query cube in world-frame (meters)
    /// \param[out] cubes list of cubes (world-frame) of child elements that have content
    bool findContentInCube(const AACube& cube, CubeList& cubes);

    /// \param point query point in world-frame (meters)
    /// \param lockType how to lock the tree (Lock, TryLock, NoLock)
    /// \param[out] accurateResult pointer to output result, will be set "true" or "false" if non-null
    OctreeElementPointer getElementEnclosingPoint(const glm::vec3& point,
                                    Octree::lockType lockType = Octree::TryLock, bool* accurateResult = NULL);

    // Note: this assumes the fileFormat is the HIO individual voxels code files
    void loadOctreeFile(const char* fileName);

    // Octree exporters
    bool toJSONDocument(QJsonDocument* doc, const OctreeElementPointer& element = nullptr);
    bool toJSONString(QString& jsonString, const OctreeElementPointer& element = nullptr);
    bool toJSON(QByteArray* data, const OctreeElementPointer& element = nullptr, bool doGzip = false);
    bool writeToFile(const char* filename, const OctreeElementPointer& element = nullptr, QString persistAsFileType = "json.gz");
    bool writeToJSONFile(const char* filename, const OctreeElementPointer& element = nullptr, bool doGzip = false);
    virtual bool writeToMap(QVariantMap& entityDescription, OctreeElementPointer element, bool skipDefaultValues,
                            bool skipThoseWithBadParents) = 0;
    virtual bool writeToJSON(QString& jsonString, const OctreeElementPointer& element) = 0;

    // Octree importers
    bool readFromFile(const char* filename);
    bool readFromURL(const QString& url, const bool isObservable = true, const qint64 callerId = -1, const bool isImport = false); // will support file urls as well...
    bool readFromByteArray(const QString& url, const QByteArray& byteArray);
    bool readFromStream(uint64_t streamLength, QDataStream& inputStream, const bool isImport = false, const QUrl& urlString = QUrl());
    bool readJSONFromStream(uint64_t streamLength, QDataStream& inputStream, const bool isImport = false, const QUrl& urlString = QUrl());
    bool readJSONFromGzippedFile(QString qFileName);
    virtual bool readFromMap(QVariantMap& entityDescription, const bool isImport = false) = 0;

    uint64_t getOctreeElementsCount();

    bool getShouldReaverage() const { return _shouldReaverage; }

    void recurseElementWithOperation(const OctreeElementPointer& element, const RecurseOctreeOperation& operation,
                void* extraData, int recursionCount = 0);
    bool recurseElementWithOperationSorted(const OctreeElementPointer& element, const RecurseOctreeOperation& operation,
        const RecurseOctreeSortingOperation& sortingOperation, void* extraData, int recursionCount = 0);

    bool recurseElementWithOperator(const OctreeElementPointer& element, RecurseOctreeOperator* operatorObject, int recursionCount = 0);

    bool getIsViewing() const { return _isViewing; } /// This tree is receiving inbound viewer datagrams.
    void setIsViewing(bool isViewing) { _isViewing = isViewing; }

    bool getIsServer() const { return _isServer; } /// Is this a server based tree. Allows guards for certain operations
    void setIsServer(bool isServer) { _isServer = isServer; }

    bool getIsClient() const { return !_isServer; } /// Is this a client based tree. Allows guards for certain operations
    void setIsClient(bool isClient) { _isServer = !isClient; }

    virtual void dumpTree() { }
    virtual void pruneTree() { }

    void setOctreeVersionInfo(QUuid id, int64_t dataVersion) {
        _persistID = id;
        _persistDataVersion = dataVersion;
    }

    virtual void resetEditStats() { }
    virtual quint64 getAverageDecodeTime() const { return 0; }
    virtual quint64 getAverageLookupTime() const { return 0;  }
    virtual quint64 getAverageUpdateTime() const { return 0;  }
    virtual quint64 getAverageCreateTime() const { return 0;  }
    virtual quint64 getAverageLoggingTime() const { return 0;  }
    virtual quint64 getAverageFilterTime() const { return 0; }

    void incrementPersistDataVersion() { _persistDataVersion++; }


protected:
    void deleteOctalCodeFromTreeRecursion(const OctreeElementPointer& element, void* extraData);

    static bool countOctreeElementsOperation(const OctreeElementPointer& element, void* extraData);

    OctreeElementPointer nodeForOctalCode(const OctreeElementPointer& ancestorElement, const unsigned char* needleCode, OctreeElementPointer* parentOfFoundElement) const;
    OctreeElementPointer createMissingElement(const OctreeElementPointer& lastParentElement, const unsigned char* codeToReach, int recursionCount = 0);
    int readElementData(const OctreeElementPointer& destinationElement, const unsigned char* nodeData,
                int bufferSizeBytes, ReadBitstreamToTreeParams& args);

    OctreeElementPointer _rootElement = nullptr;

    QUuid _persistID { QUuid::createUuid() };
    int _persistDataVersion { 0 };

    bool _isDirty;
    bool _shouldReaverage;

    bool _isViewing;
    bool _isServer;
};

#endif // hifi_Octree_h
