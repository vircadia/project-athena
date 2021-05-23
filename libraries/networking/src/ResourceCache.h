 //
//  ResourceCache.h
//  libraries/shared/src
//
//  Created by Andrzej Kapolka on 2/27/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ResourceCache_h
#define hifi_ResourceCache_h

#include <atomic>
#include <mutex>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>
#include <QtCore/QWeakPointer>
#include <QtCore/QReadWriteLock>
#include <QtCore/QQueue>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <DependencyManager.h>

#include "ResourceManager.h"

Q_DECLARE_METATYPE(size_t)

class QNetworkReply;
class QTimer;

class Resource;

static const qint64 BYTES_PER_MEGABYTES = 1024 * 1024;
static const qint64 BYTES_PER_GIGABYTES = 1024 * BYTES_PER_MEGABYTES;
static const qint64 MAXIMUM_CACHE_SIZE = 10 * BYTES_PER_GIGABYTES;  // 10GB

// Windows can have troubles allocating that much memory in ram sometimes
// so default cache size at 100 MB on windows (1GB otherwise)
#ifdef Q_OS_WIN32
static const qint64 DEFAULT_UNUSED_MAX_SIZE = 100 * BYTES_PER_MEGABYTES;
#else
static const qint64 DEFAULT_UNUSED_MAX_SIZE = 1024 * BYTES_PER_MEGABYTES;
#endif
static const qint64 MIN_UNUSED_MAX_SIZE = 0;
static const qint64 MAX_UNUSED_MAX_SIZE = MAXIMUM_CACHE_SIZE;

// We need to make sure that these items are available for all instances of
// ResourceCache derived classes. Since we can't count on the ordering of
// static members destruction, we need to use this Dependency manager implemented
// object instead
class ResourceCacheSharedItems : public Dependency  {
    SINGLETON_DEPENDENCY

    using Mutex = std::recursive_mutex;
    using Lock = std::unique_lock<Mutex>;

public:
    bool appendRequest(QWeakPointer<Resource> newRequest);
    void removeRequest(QWeakPointer<Resource> doneRequest);
    void setRequestLimit(uint32_t limit);
    uint32_t getRequestLimit() const;
    QList<QSharedPointer<Resource>> getPendingRequests() const;
    QSharedPointer<Resource> getHighestPendingRequest();
    uint32_t getPendingRequestsCount() const;
    QList<QSharedPointer<Resource>> getLoadingRequests() const;
    uint32_t getLoadingRequestsCount() const;
    void clear();

private:
    ResourceCacheSharedItems() = default;

    mutable Mutex _mutex;
    QList<QWeakPointer<Resource>> _pendingRequests;
    QList<QWeakPointer<Resource>> _loadingRequests;
    const uint32_t DEFAULT_REQUEST_LIMIT = 10;
    uint32_t _requestLimit { DEFAULT_REQUEST_LIMIT };
};

/// Wrapper to expose resources to JS/QML
class ScriptableResource : public QObject {

    /*@jsdoc
     * Information about a cached resource. Created by {@link AnimationCache.prefetch}, {@link MaterialCache.prefetch}, 
     * {@link ModelCache.prefetch}, {@link SoundCache.prefetch}, or {@link TextureCache.prefetch}.
     *
     * @class ResourceObject
     * @hideconstructor
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     * @hifi-server-entity
     * @hifi-assignment-client
     *
     * @property {string} url - URL of the resource. <em>Read-only.</em>
     * @property {Resource.State} state - Current loading state. <em>Read-only.</em>
     */
    Q_OBJECT
    Q_PROPERTY(QUrl url READ getURL)
    Q_PROPERTY(int state READ getState NOTIFY stateChanged)

public:

    /*@jsdoc
     * The loading state of a resource.
     * @typedef {object} Resource.State
     * @property {number} QUEUED - The resource is queued up, waiting to be loaded.
     * @property {number} LOADING - The resource is downloading.
     * @property {number} LOADED - The resource has finished downloading but is not complete.
     * @property {number} FINISHED - The resource has completely finished loading and is ready.
     * @property {number} FAILED - The resource has failed to download.
     */
    enum State {
        QUEUED,
        LOADING,
        LOADED,
        FINISHED,
        FAILED,
    };
    Q_ENUM(State)

    ScriptableResource(const QUrl& url);
    virtual ~ScriptableResource() = default;

    /*@jsdoc
      * Releases the resource.
      * @function ResourceObject#release
      */
    Q_INVOKABLE void release();

    const QUrl& getURL() const { return _url; }
    int getState() const { return (int)_state; }
    const QSharedPointer<Resource>& getResource() const { return _resource; }

    bool isInScript() const;
    void setInScript(bool isInScript);

signals:

    /*@jsdoc
     * Triggered when the resource's download progress changes.
     * @function ResourceObject#progressChanged
     * @param {number} bytesReceived - Bytes downloaded so far.
     * @param {number} bytesTotal - Total number of bytes in the resource.
     * @returns {Signal}
     */
    void progressChanged(uint64_t bytesReceived, uint64_t bytesTotal);

    /*@jsdoc
     * Triggered when the resource's loading state changes.
     * @function ResourceObject#stateChanged
     * @param {Resource.State} state - New state.
     * @returns {Signal}
     */
    void stateChanged(int state);

protected:
    void setState(State state) { _state = state; emit stateChanged(_state); }

private slots:
    void loadingChanged();
    void loadedChanged();
    void finished(bool success);

private:
    void disconnectHelper();

    friend class ResourceCache;

    // Holds a ref to the resource to keep it in scope
    QSharedPointer<Resource> _resource;

    QMetaObject::Connection _progressConnection;
    QMetaObject::Connection _loadingConnection;
    QMetaObject::Connection _loadedConnection;
    QMetaObject::Connection _finishedConnection;

    QUrl _url;
    State _state{ QUEUED };
};

Q_DECLARE_METATYPE(ScriptableResource*);

/// Base class for resource caches.
class ResourceCache : public QObject {
    Q_OBJECT

    Q_PROPERTY(size_t numTotal READ getNumTotalResources NOTIFY dirty)
    Q_PROPERTY(size_t numCached READ getNumCachedResources NOTIFY dirty)
    Q_PROPERTY(size_t sizeTotal READ getSizeTotalResources NOTIFY dirty)
    Q_PROPERTY(size_t sizeCached READ getSizeCachedResources NOTIFY dirty)

public:

    size_t getNumTotalResources() const { return _numTotalResources; }
    size_t getSizeTotalResources() const { return _totalResourcesSize; }
    size_t getNumCachedResources() const { return _numUnusedResources; }
    size_t getSizeCachedResources() const { return _unusedResourcesSize; }

    Q_INVOKABLE QVariantList getResourceList();

    static void setRequestLimit(uint32_t limit);
    static uint32_t getRequestLimit() { return DependencyManager::get<ResourceCacheSharedItems>()->getRequestLimit(); }
    
    void setUnusedResourceCacheSize(qint64 unusedResourcesMaxSize);
    qint64 getUnusedResourceCacheSize() const { return _unusedResourcesMaxSize; }

    static QList<QSharedPointer<Resource>> getLoadingRequests();
    static uint32_t getPendingRequestCount();
    static uint32_t getLoadingRequestCount();

    ResourceCache(QObject* parent = nullptr);
    virtual ~ResourceCache();
    
    void refreshAll();
    void clearUnusedResources();

signals:

    void dirty();

protected slots:

    void updateTotalSize(const qint64& deltaSize);

    // Prefetches a resource to be held by the ScriptEngine.
    // Left as a protected member so subclasses can overload prefetch
    // and delegate to it (see TextureCache::prefetch(const QUrl&, int).
    ScriptableResource* prefetch(const QUrl& url, void* extra, size_t extraHash);

    // FIXME: The return type is not recognized by JavaScript.
    /// Loads a resource from the specified URL and returns it.
    /// If the caller is on a different thread than the ResourceCache,
    /// returns an empty smart pointer and loads its asynchronously.
    /// \param fallback a fallback URL to load if the desired one is unavailable
    // FIXME: std::numeric_limits<size_t>::max() could be a valid extraHash
    QSharedPointer<Resource> getResource(const QUrl& url, const QUrl& fallback = QUrl()) { return getResource(url, fallback, nullptr, std::numeric_limits<size_t>::max()); }
    QSharedPointer<Resource> getResource(const QUrl& url, const QUrl& fallback, void* extra, size_t extraHash);

private slots:
    void clearATPAssets();

protected:
    // Prefetches a resource to be held by the ScriptEngine.
    // Pointers created through this method should be owned by the caller,
    // which should be a ScriptEngine with ScriptableResource registered, so that
    // the ScriptEngine will delete the pointer when it is garbage collected.
    // JSDoc is provided on more general function signature.
    Q_INVOKABLE ScriptableResource* prefetch(const QUrl& url) { return prefetch(url, nullptr, std::numeric_limits<size_t>::max()); }

    /// Creates a new resource.
    virtual QSharedPointer<Resource> createResource(const QUrl& url) = 0;
    virtual QSharedPointer<Resource> createResourceCopy(const QSharedPointer<Resource>& resource) = 0;

    void addUnusedResource(const QSharedPointer<Resource>& resource);
    void removeUnusedResource(const QSharedPointer<Resource>& resource);

    /// Attempt to load a resource if requests are below the limit, otherwise queue the resource for loading
    /// \return true if the resource began loading, otherwise false if the resource is in the pending queue
    static bool attemptRequest(QSharedPointer<Resource> resource);
    static void requestCompleted(QWeakPointer<Resource> resource);
    static bool attemptHighestPriorityRequest();

private:
    friend class Resource;
    friend class ScriptableResourceCache;

    void reserveUnusedResource(qint64 resourceSize);
    void removeResource(const QUrl& url, size_t extraHash, qint64 size = 0);

    void resetTotalResourceCounter();
    void resetUnusedResourceCounter();
    void resetResourceCounters();

    // Resources
    QHash<QUrl, QHash<size_t, QWeakPointer<Resource>>> _resources;
    QReadWriteLock _resourcesLock { QReadWriteLock::Recursive };
    int _lastLRUKey = 0;

    std::atomic<size_t> _numTotalResources { 0 };
    std::atomic<qint64> _totalResourcesSize { 0 };

    // Cached resources
    QMap<int, QSharedPointer<Resource>> _unusedResources;
    QReadWriteLock _unusedResourcesLock { QReadWriteLock::Recursive };
    qint64 _unusedResourcesMaxSize = DEFAULT_UNUSED_MAX_SIZE;

    std::atomic<size_t> _numUnusedResources { 0 };
    std::atomic<qint64> _unusedResourcesSize { 0 };
};

/// Wrapper to expose resource caches to JS/QML
class ScriptableResourceCache : public QObject {
    Q_OBJECT

    // JSDoc 3.5.5 doesn't augment name spaces with @property definitions so the following properties JSDoc is copied to the 
    // different exposed cache classes.

    /*@jsdoc
     * @property {number} numTotal - Total number of total resources. <em>Read-only.</em>
     * @property {number} numCached - Total number of cached resource. <em>Read-only.</em>
     * @property {number} sizeTotal - Size in bytes of all resources. <em>Read-only.</em>
     * @property {number} sizeCached - Size in bytes of all cached resources. <em>Read-only.</em>
     */
    Q_PROPERTY(size_t numTotal READ getNumTotalResources NOTIFY dirty)
    Q_PROPERTY(size_t numCached READ getNumCachedResources NOTIFY dirty)
    Q_PROPERTY(size_t sizeTotal READ getSizeTotalResources NOTIFY dirty)
    Q_PROPERTY(size_t sizeCached READ getSizeCachedResources NOTIFY dirty)

    /*@jsdoc
     * @property {number} numGlobalQueriesPending - Total number of global queries pending (across all resource cache managers).
     *     <em>Read-only.</em>
     * @property {number} numGlobalQueriesLoading - Total number of global queries loading (across all resource cache managers).
     *     <em>Read-only.</em>
     */
    Q_PROPERTY(size_t numGlobalQueriesPending READ getNumGlobalQueriesPending NOTIFY dirty)
    Q_PROPERTY(size_t numGlobalQueriesLoading READ getNumGlobalQueriesLoading NOTIFY dirty)

public:
    ScriptableResourceCache(QSharedPointer<ResourceCache> resourceCache);

    /*@jsdoc
     * Gets the URLs of all resources in the cache.
     * @function ResourceCache.getResourceList
     * @returns {string[]} The URLs of all resources in the cache.
     * @example <caption>Report cached resources.</caption>
     * // Replace AnimationCache with MaterialCache, ModelCache, SoundCache, or TextureCache as appropriate.
     *
     * var cachedResources = AnimationCache.getResourceList();
     * print("Cached resources: " + JSON.stringify(cachedResources));
     */
    Q_INVOKABLE QVariantList getResourceList();

    /*@jsdoc
     * @function ResourceCache.updateTotalSize
     * @param {number} deltaSize - Delta size.
     * @deprecated This function is deprecated and will be removed.
     */
    Q_INVOKABLE void updateTotalSize(const qint64& deltaSize);

    /*@jsdoc
     * Prefetches a resource.
     * @function ResourceCache.prefetch
     * @param {string} url - The URL of the resource to prefetch.
     * @returns {ResourceObject} A resource object.
     * @example <caption>Prefetch a resource and wait until it has loaded.</caption>
     * // Replace AnimationCache with MaterialCache, ModelCache, SoundCache, or TextureCache as appropriate.
     * // TextureCache has its own version of this function.
     * 
     * var resourceURL = "https://cdn-1.vircadia.com/eu-c-1/vircadia-public/clement/production/animations/sitting_idle.fbx";
     * var resourceObject = AnimationCache.prefetch(resourceURL);
     * 
     * function checkIfResourceLoaded(state) {
     *     if (state === Resource.State.FINISHED) {
     *         print("Resource loaded and ready.");
     *     } else if (state === Resource.State.FAILED) {
     *         print("Resource not loaded.");
     *     }
     * }
     * 
     * // Resource may have already been loaded.
     * print("Resource state: " + resourceObject.state);
     * checkIfResourceLoaded(resourceObject.state);
     * 
     * // Resource may still be loading.
     * resourceObject.stateChanged.connect(function (state) {
     *     print("Resource state changed to: " + state);
     *     checkIfResourceLoaded(state);
     * });
     */
    Q_INVOKABLE ScriptableResource* prefetch(const QUrl& url) { return prefetch(url, nullptr, std::numeric_limits<size_t>::max()); }
    
    // FIXME: This function variation shouldn't be in the API.
    Q_INVOKABLE ScriptableResource* prefetch(const QUrl& url, void* extra, size_t extraHash);

signals:

    /*@jsdoc
     * Triggered when the cache content has changed.
     * @function ResourceCache.dirty
     * @returns {Signal}
     */
    void dirty();

private:
    QSharedPointer<ResourceCache> _resourceCache;

    size_t getNumTotalResources() const { return _resourceCache->getNumTotalResources(); }
    size_t getSizeTotalResources() const { return _resourceCache->getSizeTotalResources(); }
    size_t getNumCachedResources() const { return _resourceCache->getNumCachedResources(); }
    size_t getSizeCachedResources() const { return _resourceCache->getSizeCachedResources(); }

    size_t getNumGlobalQueriesPending() const { return ResourceCache::getPendingRequestCount(); }
    size_t getNumGlobalQueriesLoading() const { return ResourceCache::getLoadingRequestCount(); }
};

/// Base class for resources.
class Resource : public QObject {
    Q_OBJECT

public:
    Resource() : QObject(), _loaded(true) {}
    Resource(const Resource& other);
    Resource(const QUrl& url);
    virtual ~Resource();

    virtual QString getType() const { return "Resource"; }

    /// Returns the key last used to identify this resource in the unused map.
    int getLRUKey() const { return _lruKey; }

    /// Makes sure that the resource has started loading.
    void ensureLoading();

    /// Sets the load priority for one owner.
    virtual void setLoadPriority(const QPointer<QObject>& owner, float priority);

    /// Sets a set of priorities at once.
    virtual void setLoadPriorities(const QHash<QPointer<QObject>, float>& priorities);

    /// Clears the load priority for one owner.
    virtual void clearLoadPriority(const QPointer<QObject>& owner);

    /// Returns the highest load priority across all owners.
    float getLoadPriority();

    /// Checks whether the resource has loaded.
    virtual bool isLoaded() const { return _loaded; }

    /// Checks whether the resource has failed to download.
    virtual bool isFailed() const { return _failedToLoad; }

    /// For loading resources, returns the number of bytes received.
    qint64 getBytesReceived() const { return _bytesReceived; }
    
    /// For loading resources, returns the number of total bytes (<= zero if unknown).
    qint64 getBytesTotal() const { return _bytesTotal; }

    /// For loaded resources, returns the number of actual bytes (defaults to total bytes if not explicitly set).
    qint64 getBytes() const { return _bytes; }

    /// For loading resources, returns the load progress.
    float getProgress() const { return (_bytesTotal <= 0) ? 0.0f : (float)_bytesReceived / _bytesTotal; }
    
    /// Refreshes the resource.
    virtual void refresh();

    void setSelf(const QWeakPointer<Resource>& self) { _self = self; }

    void setCache(ResourceCache* cache) { _cache = cache; }

    virtual void deleter() { allReferencesCleared(); }
    
    const QUrl& getURL() const { return _url; }

    unsigned int getDownloadAttempts() { return _attempts; }
    unsigned int getDownloadAttemptsRemaining() { return _attemptsRemaining; }

    virtual void setExtra(void* extra) {};
    void setExtraHash(size_t extraHash) { _extraHash = extraHash; }
    size_t getExtraHash() const { return _extraHash; }

signals:
    /// Fired when the resource begins downloading.
    void loading();

    /// Fired when the resource has been downloaded.
    /// This can be used instead of downloadFinished to access data before it is processed.
    void loaded(const QByteArray request);

    /// Fired when the resource has finished loading.
    void finished(bool success);

    /// Fired when the resource failed to load.
    void failed(QNetworkReply::NetworkError error);

    /// Fired when the resource is refreshed.
    void onRefresh();

    /// Fired on progress updates.
    void onProgress(uint64_t bytesReceived, uint64_t bytesTotal);

    /// Fired when the size changes (through setSize).
    void updateSize(qint64 deltaSize);

protected slots:
    void attemptRequest();

protected:
    virtual void init(bool resetLoaded = true);

    /// Called by ResourceCache to begin loading this Resource.
    /// This method can be overriden to provide custom request functionality. If this is done,
    /// downloadFinished and ResourceCache::requestCompleted must be called.
    virtual void makeRequest();

    /// Checks whether the resource is cacheable.
    virtual bool isCacheable() const { return _loaded; }

    /// Called when the download has finished.
    /// This should be overridden by subclasses that need to process the data once it is downloaded.
    virtual void downloadFinished(const QByteArray& data) { finishedLoading(true); }

    /// Called when the download is finished and processed, sets the number of actual bytes.
    void setSize(const qint64& bytes);

    /// Called when the download is finished and processed.
    /// This should be called by subclasses that override downloadFinished to mark the end of processing.
    Q_INVOKABLE void finishedLoading(bool success);

    Q_INVOKABLE void allReferencesCleared();

    /// Return true if the resource will be retried
    virtual bool handleFailedRequest(ResourceRequest::Result result);

    QUrl _url;
    QUrl _effectiveBaseURL { _url };
    QUrl _activeUrl;
    ByteRange _requestByteRange;
    bool _shouldFailOnRedirect { false };

    // _loaded == true means we are in a loaded and usable state. It is possible that there may still be
    // active requests/loading while in this state. Example: Progressive KTX downloads, where higher resolution
    // mips are being download.
    bool _startedLoading = false;
    bool _failedToLoad = false;
    bool _loaded = false;

    QHash<QPointer<QObject>, float> _loadPriorities;
    QWeakPointer<Resource> _self;
    QPointer<ResourceCache> _cache;

    qint64 _bytesReceived { 0 };
    qint64 _bytesTotal { 0 };
    qint64 _bytes { 0 };

    int _requestID;
    ResourceRequest* _request { nullptr };

    size_t _extraHash { std::numeric_limits<size_t>::max() };

public slots:
    void handleDownloadProgress(uint64_t bytesReceived, uint64_t bytesTotal);
    void handleReplyFinished();

private:
    friend class ResourceCache;
    friend class ScriptableResource;
    
    void setLRUKey(int lruKey) { _lruKey = lruKey; }
    
    void retry();
    void reinsert();

    bool isInScript() const { return _isInScript; }
    void setInScript(bool isInScript) { _isInScript = isInScript; }
    
    int _lruKey{ 0 };
    QTimer* _replyTimer{ nullptr };
    unsigned int _attempts{ 0 };
    static const int MAX_ATTEMPTS = 8;
    unsigned int _attemptsRemaining { MAX_ATTEMPTS };
    bool _isInScript{ false };
};

uint qHash(const QPointer<QObject>& value, uint seed = 0);

#endif // hifi_ResourceCache_h
