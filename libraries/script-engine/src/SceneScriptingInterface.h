//
//  SceneScriptingInterface.h
//  libraries/script-engine
//
//  Created by Sam Gateau on 2/24/15.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SceneScriptingInterface_h
#define hifi_SceneScriptingInterface_h

#include <DependencyManager.h>

/*@jsdoc
 * The <code>Scene</code> API provides some control over what is rendered.
 *
 * @namespace Scene
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {boolean} shouldRenderAvatars - <code>true</code> if avatars are rendered, <code>false</code> if they aren't.
 * @property {boolean} shouldRenderEntities - <code>true</code> if entities (domain, avatar, and local) are rendered, 
 *     <code>false</code> if they aren't.
 */
class SceneScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    Q_PROPERTY(bool shouldRenderAvatars READ shouldRenderAvatars WRITE setShouldRenderAvatars)
    Q_PROPERTY(bool shouldRenderEntities READ shouldRenderEntities WRITE setShouldRenderEntities)
    bool shouldRenderAvatars() const { return _shouldRenderAvatars; }
    bool shouldRenderEntities() const { return _shouldRenderEntities; }
    void setShouldRenderAvatars(bool shouldRenderAvatars);
    void setShouldRenderEntities(bool shouldRenderEntities);

signals:

    /*@jsdoc
     * Triggered when whether or not avatars are rendered changes.
     * @function Scene.shouldRenderAvatarsChanged
     * @param {boolean} shouldRenderAvatars - <code>true</code> if avatars are rendered, <code>false</code> if they aren't.
     * @returns {Signal}
     * @example <caption>Report when the rendering of avatars changes.</caption>
     * Scene.shouldRenderAvatarsChanged.connect(function (shouldRenderAvatars) {
     *     print("Should render avatars changed to: " + shouldRenderAvatars);
     * });
     */
    void shouldRenderAvatarsChanged(bool shouldRenderAvatars);

    /*@jsdoc
     * Triggered when whether or not entities are rendered changes.
     * @function Scene.shouldRenderEntitiesChanged
     * @param {boolean} shouldRenderEntities - <code>true</code> if entities (domain, avatar, and local) are rendered, 
     *     <code>false</code> if they aren't.
     * @returns {Signal}
     */
    void shouldRenderEntitiesChanged(bool shouldRenderEntities);

protected:
    bool _shouldRenderAvatars { true };
    bool _shouldRenderEntities { true };
};

#endif // hifi_SceneScriptingInterface_h 
