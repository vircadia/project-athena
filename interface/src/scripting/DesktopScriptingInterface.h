//
//  DesktopScriptingInterface.h
//  interface/src/scripting
//
//  Created by David Rowe on 25 Aug 2015.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DesktopScriptingInterface_h
#define hifi_DesktopScriptingInterface_h

#include <QObject>

#include <DependencyManager.h>

#include "ui/InteractiveWindow.h"

/*@jsdoc
 * The <code>Desktop</code> API provides the dimensions of the computer screen, sets the opacity of the HUD surface, and 
 * enables QML and HTML windows to be shown inside or outside of Interface.
 *
 * @namespace Desktop
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {number} width - The width of the computer screen including task bar and system menu, in pixels. 
 *     <em>Read-only.</em>
 * @property {number} height - The height of the computer screen including task bar and system menu, in pixels. 
 *     <em>Read-only.</em>
 * @property {InteractiveWindow.Flags} ALWAYS_ON_TOP - A flag value that makes an {@link InteractiveWindow} always display on 
 *     top. <em>Read-only.</em>
 * @property {InteractiveWindow.Flags} CLOSE_BUTTON_HIDES - A flag value that makes an {@link InteractiveWindow} hide instead 
 *     of closing when the user clicks the "close" button.<em> Read-only.</em>
 * @property {InteractiveWindow.PresentationModes} PresentationMode - The possible display options for an 
 *     {@link InteractiveWindow}: display inside Interface or in a separate desktop window. <em>Read-only.</em>
 * @property {InteractiveWindow.DockAreas} DockArea - The possible docking locations of an {@link InteractiveWindow}: top, 
 *     bottom, left, or right of the Interface window. 
 *     <em>Read-only.</em>
 * @property {InteractiveWindow.RelativePositionAnchors} RelativePositionAnchor - The possible relative position anchors for an 
 *     {@link InteractiveWindow}: none, top left, top right, bottom right, or bottom left of the Interface window. 
 *     <em>Read-only.</em>
 */
class DesktopScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    Q_PROPERTY(int width READ getWidth)  // Physical width of screen(s) including task bars and system menus
    Q_PROPERTY(int height READ getHeight)  // Physical height of screen(s) including task bars and system menus

    Q_PROPERTY(QVariantMap PresentationMode READ getPresentationMode CONSTANT FINAL)
    Q_PROPERTY(QVariantMap DockArea READ getDockArea CONSTANT FINAL)
    Q_PROPERTY(QVariantMap RelativePositionAnchor READ getRelativePositionAnchor CONSTANT FINAL)
    Q_PROPERTY(int ALWAYS_ON_TOP READ flagAlwaysOnTop CONSTANT FINAL)
    Q_PROPERTY(int CLOSE_BUTTON_HIDES READ flagCloseButtonHides CONSTANT FINAL)

public:
    DesktopScriptingInterface(QObject* parent= nullptr, bool restricted = false);

    /*@jsdoc
     * Sets the opacity of the HUD surface.
     * @function Desktop.setHUDAlpha
     * @param {number} alpha - The opacity, <code>0.0 &ndash; 1.0</code>.
     */
    Q_INVOKABLE void setHUDAlpha(float alpha);

    /*@jsdoc
     * Opens a QML window within Interface: in the Interface window in desktop mode or on the HUD surface in HMD mode. If a 
     * window of the specified name already exists, it is shown, otherwise a new window is created from the QML.
     * @function Desktop.show
     * @param {string} url - The QML file that specifies the window content.
     * @param {string} name - A unique name for the window.
     * @example <caption>Open the general settings dialog.</caption>
     * Desktop.show("hifi/dialogs/GeneralPreferencesDialog.qml", "GeneralPreferencesDialog");
     */
    Q_INVOKABLE void show(const QString& path, const QString&  title);

    /*@jsdoc
     * Creates a new window that can be displayed either within Interface or as a separate desktop window.
     * @function Desktop.createWindow
     * @param {string} url - The QML file that specifies the window content. The QML file can use a <code>WebView</code> 
     *     control (defined by "WebView.qml" included in the Interface install) to embed an HTML web page (complete with  
     *     <code>EventBridge</code> object).
     * @param {InteractiveWindow.WindowProperties} [properties] - Initial window properties.
     * @returns {InteractiveWindow} A new window object.
     * @example <caption>Open a dialog in its own window separate from Interface.</caption>
     * var nativeWindow = Desktop.createWindow(Script.resourcesPath() + 'qml/OverlayWindowTest.qml', {
     *     title: "Native Window",
     *     presentationMode: Desktop.PresentationMode.NATIVE,
     *     size: { x: 500, y: 400 }
     * });
     *
     * Script.scriptEnding.connect(function () {
     *     nativeWindow.close();
     * });
     */
    Q_INVOKABLE InteractiveWindowPointer createWindow(const QString& sourceUrl, const QVariantMap& properties = QVariantMap());

    int getWidth();
    int getHeight();


private:
    static int flagAlwaysOnTop() { return AlwaysOnTop; }
    static int flagCloseButtonHides() { return CloseButtonHides; }

    Q_INVOKABLE InteractiveWindowPointer createWindowOnThread(const QString& sourceUrl, const QVariantMap& properties, QThread* targetThread);

    static QVariantMap getDockArea();
    static QVariantMap getRelativePositionAnchor();
    Q_INVOKABLE static QVariantMap getPresentationMode();
    const bool _restricted;
};


#endif // hifi_DesktopScriptingInterface_h
