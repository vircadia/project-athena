//
//  Created by Bradley Austin Davis on 2016-06-16
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ToolbarScriptingInterface_h
#define hifi_ToolbarScriptingInterface_h

#include <mutex>

#include <QtCore/QObject>

#include <DependencyManager.h>
#include "QmlWrapper.h"

class QQuickItem;

// No JSDoc for ToolbarButtonProxy because ToolbarProxy#addButton() doesn't work.
class ToolbarButtonProxy : public QmlWrapper {
    Q_OBJECT

public:
    ToolbarButtonProxy(QObject* qmlObject, QObject* parent = nullptr);

    Q_INVOKABLE void editProperties(const QVariantMap& properties);

signals:
    void clicked();

protected:
    QQuickItem* _qmlButton { nullptr };
    QVariantMap _properties;
};

Q_DECLARE_METATYPE(ToolbarButtonProxy*);

/*@jsdoc
 * An instance of a toolbar.
 * 
 * <p>Retrieve an existing toolbar or create a new toolbar using {@link Toolbars.getToolbar}.</p>
 *
 * @class ToolbarProxy
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */
class ToolbarProxy : public QmlWrapper {
    Q_OBJECT
public:
    ToolbarProxy(QObject* qmlObject, QObject* parent = nullptr);

    /*@jsdoc
     * <em>Currently doesn't work.</em>
     * @function ToolbarProxy#addButton
     * @param {object} properties - Button properties
     * @returns {object} The button added.
     * @deprecated This method is deprecated and will be removed.
     */
    Q_INVOKABLE ToolbarButtonProxy* addButton(const QVariant& properties);

    /*@jsdoc
     * <em>Currently doesn't work.</em>
     * @function ToolbarProxy#removeButton
     * @param {string} name - Button name.
     * @deprecated This method is deprecated and will be removed.
     */
    Q_INVOKABLE void removeButton(const QVariant& name);


    // QmlWrapper methods.

    /*@jsdoc
     * Sets the value of a toolbar property. A property is added to the toolbar if the named property doesn't already 
     * exist.
     * @function ToolbarProxy#writeProperty
     * @parm {string} propertyName - The name of the property. Toolbar properties are those in the QML implementation of the
     *     toolbar.
     * @param {object} propertyValue - The value of the property.
     */

    /*@jsdoc
     * Sets the values of toolbar properties. A property is added to the toolbar if a named property doesn't already
     * exist.
     * @function ToolbarProxy#writeProperties
     * @param {object} properties - The names and values of the properties to set. Toolbar properties are those in the QML 
     *     implementation of the toolbar.
     */

    /*@jsdoc
     * Gets the value of a toolbar property.
     * @function ToolbarProxy#readProperty
     * @param {string} propertyName - The property name. Toolbar properties are those in the QML implementation of the toolbar.
     * @returns {object} The value of the property if the property name is valid, otherwise <code>undefined</code>.
     */

    /*@jsdoc
     * Gets the values of toolbar properties.
     * @function ToolbarProxy#readProperties
     * @param {string[]} propertyList - The names of the properties to get the values of. Toolbar properties are those in the 
     *     QML implementation of the toolbar.
     * @returns {object} The names and values of the specified properties. If the toolbar doesn't have a particular property 
     *     then the result doesn't include that property.
     */
};

Q_DECLARE_METATYPE(ToolbarProxy*);

/*@jsdoc
 * The <code>Toolbars</code> API provides facilities to work with the system or other toolbar.
 *
 * <p>See also the {@link Tablet} API for use of the system tablet and toolbar in desktop and HMD modes.</p>
 *
 * @namespace Toolbars
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */
class ToolbarScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
public:

    /*@jsdoc
     * Gets an instance of a toolbar. A new toolbar is created if one with the specified name doesn't already exist.
     * @function Toolbars.getToolbar
     * @param {string} name - A unique name that identifies the toolbar.
     * @returns {ToolbarProxy} The toolbar instance.
     */
    Q_INVOKABLE ToolbarProxy* getToolbar(const QString& toolbarId);

signals:
    /*@jsdoc
     * Triggered when the visibility of a toolbar changes.
     * @function Toolbars.toolbarVisibleChanged
     * @param {boolean} isVisible - <code>true</code> if the toolbar is visible, <code>false</code> if it is hidden.
     * @param {string} toolbarName - The name of the toolbar.
     * @returns {Signal}
     * @example <caption>Briefly hide the system toolbar.</caption>
     * Toolbars.toolbarVisibleChanged.connect(function(visible, name) {
     *     print("Toolbar " + name + " visible changed to " + visible);
     * });
     * 
     * var toolbar = Toolbars.getToolbar("com.highfidelity.interface.toolbar.system");
     * if (toolbar) {
     *     toolbar.writeProperty("visible", false);
     *     Script.setTimeout(function () {
     *         toolbar.writeProperty("visible", true);
     *     }, 2000);
     * }
     */
    void toolbarVisibleChanged(bool isVisible, QString toolbarName);
};


#endif // hifi_ToolbarScriptingInterface_h
