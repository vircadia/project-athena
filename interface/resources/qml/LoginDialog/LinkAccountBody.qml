//
//  LinkAccountBody.qml
//
//  Created by Clement on 7/18/16
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import Hifi 1.0
import QtQuick 2.7
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 as OriginalStyles

import "." as LoginDialog

import controlsUit 1.0 as HifiControlsUit
import stylesUit 1.0 as HifiStylesUit
import TabletScriptingInterface 1.0

Item {
    z: -2
    id: linkAccountBody
    clip: true
    height: root.height
    width: root.width
    property int textFieldHeight: 31
    property string fontFamily: "Raleway"
    property int fontSize: 15
    property int textFieldFontSize: 18
    property bool fontBold: true
    readonly property var passwordImageRatio: 16 / 23

    property bool keyboardEnabled: false
    property bool keyboardRaised: false
    property bool punctuationMode: false

    property bool withSteam: withSteam
    property bool linkSteam: linkSteam
    property bool withOculus: withOculus
    property bool linkOculus: linkOculus
    property string errorString: errorString
    property bool lostFocus: false

    readonly property bool loginDialogPoppedUp: loginDialog.getLoginDialogPoppedUp()
    // If not logging into domain, then we must be logging into the metaverse...
    readonly property bool isLoggingInToDomain: loginDialog.getDomainLoginRequested()
    readonly property string domainLoginDomain: loginDialog.getDomainLoginDomain()

    QtObject {
        id: d
        readonly property int minWidth: 480
        readonly property int minWidthButton: !root.isTablet ? 256 : 174
        property int maxWidth: root.width
        readonly property int minHeight: 120
        readonly property int minHeightButton: 36
        property int maxHeight: root.height

        function resize() {
            maxWidth = root.width;
            maxHeight = root.height;
            var targetWidth = Math.max(titleWidth, mainContainer.width);
            var targetHeight =  hifi.dimensions.contentSpacing.y + mainContainer.height + 4 * hifi.dimensions.contentSpacing.y;

            var newWidth = Math.max(d.minWidth, Math.min(d.maxWidth, targetWidth));
            if (!isNaN(newWidth)) {
                parent.width = root.width = newWidth;
            }

            parent.height = root.height = Math.max(d.minHeight, Math.min(d.maxHeight, targetHeight)) + hifi.dimensions.contentSpacing.y;
        }
    }

    function login() {
        // make sure the metaverse server is set so we don't send your password to the wrong place!
        if (!isLoggingInToDomain) {
            Settings.setValue("private/selectedMetaverseURL", metaverseServerField.text);
        }
        
        if (keepMeLoggedInCheckbox.checked) {
            if (!isLoggingInToDomain) {
                Settings.setValue("keepMeLoggedIn/savedUsername", emailField.text);
            } else {
                // ####### TODO
            }
        }

        if (!isLoggingInToDomain) {
            loginDialog.login(emailField.text, passwordField.text);
        } else {
            loginDialog.loginDomain(emailField.text, passwordField.text);
        }
        
        if (linkAccountBody.loginDialogPoppedUp) {
            var data;
            if (linkAccountBody.linkSteam) {
                data = {
                    "action": "user linking hifi account with Steam"
                };
            } else {
                data = {
                    "action": "user logging in"
                };
            }
            UserActivityLogger.logAction("encourageLoginDialog", data);
        }
        bodyLoader.setSource("LoggingInBody.qml", { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader, "withSteam": linkAccountBody.withSteam,
            "withOculus": linkAccountBody.withOculus, "linkSteam": linkAccountBody.linkSteam, "linkOculus": linkAccountBody.linkOculus,
            "displayName":displayNameField.text, "isLoggingInToDomain": linkAccountBody.isLoggingInToDomain, "domainLoginDomain": linkAccountBody.domainLoginDomain });
    }

    function init() {
        // going to/from sign in/up dialog.
        loginErrorMessage.text = linkAccountBody.errorString;
        loginErrorMessage.visible = (linkAccountBody.errorString  !== "");
        if (loginErrorMessageTextMetrics.width > displayNameField.width) {
            loginErrorMessage.wrapMode = Text.WordWrap;
            errorContainer.height = (loginErrorMessageTextMetrics.width / displayNameField.width) * loginErrorMessageTextMetrics.height;
        }
        var domainLoginText = "Log In to Domain\n" + domainLoginDomain;
        loginDialogText.text = (!isLoggingInToDomain) ? "Log In to Metaverse" : domainLoginText;
        loginButton.text = (!linkAccountBody.linkSteam && !linkAccountBody.linkOculus) ? "Log In" : "Link Account";
        loginButton.text = (!isLoggingInToDomain) ? "Log In to Metaverse" : "Log In to Domain";
        loginButton.color = hifi.buttons.blue;
        displayNameField.placeholderText = "Display Name (optional)";
        var savedDisplayName = Settings.getValue("Avatar/displayName", "");
        displayNameField.text = savedDisplayName;
        emailField.placeholderText = "Username or Email";
        if (!isLoggingInToDomain) {
            var savedUsername = Settings.getValue("keepMeLoggedIn/savedUsername", "");
            emailField.text = keepMeLoggedInCheckbox.checked ? savedUsername === "Unknown user" ? "" : savedUsername : "";

            var metaverseServer = Settings.getValue("private/selectedMetaverseURL", "");
            console.log("Saved metaverse server:", metaverseServer);
            metaverseServerField.text = metaverseServer;
        } else {
            // ####### TODO
        }

        if (linkAccountBody.linkSteam || linkAccountBody.linkOculus) {
            loginButton.width = (passwordField.width - hifi.dimensions.contentSpacing.x) / 2;
            loginButton.anchors.right = displayNameField.right;
        } else {
            loginButton.anchors.left = displayNameField.left;
        }
        loginContainer.visible = true;
    }

    Item {
        z: 10
        id: mainContainer
        width: root.width
        height: root.height
        onHeightChanged: d.resize(); onWidthChanged: d.resize();

        LoginDialog.LoginDialogLightbox {
            id: lightboxPopup;
            visible: false;
            anchors.fill: parent;
        }

        Item {
            id: loginContainer
            width: displayNameField.width
            height: errorContainer.height + loginDialogTextContainer.height + displayNameField.height + emailField.height + passwordField.height + metaverseServerField.height + 5.5 * hifi.dimensions.contentSpacing.y +
                keepMeLoggedInCheckbox.height + loginButton.height + cantAccessTextMetrics.height + continueButton.height
            anchors {
                top: parent.top
                topMargin: root.bannerHeight + 0.25 * parent.height
                left: parent.left
                leftMargin: (parent.width - displayNameField.width) / 2
            }

            Item {
                id: errorContainer
                width: parent.width
                height: loginErrorMessageTextMetrics.height
                anchors {
                    bottom: loginDialogTextContainer.top
                    bottomMargin: hifi.dimensions.contentSpacing.y
                    left: loginDialogTextContainer.left
                    right: loginDialogTextContainer.right
                }
                TextMetrics {
                    id: loginErrorMessageTextMetrics
                    font: loginErrorMessage.font
                    text: loginErrorMessage.text
                }
                Text {
                    id: loginErrorMessage;
                    color: "red";
                    font.family: linkAccountBody.fontFamily
                    font.pixelSize: linkAccountBody.textFieldFontSize
                    font.bold: linkAccountBody.fontBold
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                    }
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: ""
                    visible: false
                }
            }
            
            Item {
                id: loginDialogTextContainer
                height: 56
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 1.5 * hifi.dimensions.contentSpacing.y
                }
            
                Text {
                    id: loginDialogText
                    text: qsTr("Log In")
                    lineHeight: 1
                    color: "white"
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                    }
                    font.family: linkAccountBody.fontFamily
                    font.pixelSize: 24
                    font.bold: linkAccountBody.fontBold
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            HifiControlsUit.TextField {
                id: displayNameField
                width: root.bannerWidth
                height: linkAccountBody.textFieldHeight
                font.pixelSize: linkAccountBody.textFieldFontSize
                styleRenderType: Text.QtRendering
                anchors {
                    top: loginDialogTextContainer.bottom
                    topMargin: 1.5 * hifi.dimensions.contentSpacing.y
                }
                placeholderText: "Display Name (optional)"
                activeFocusOnPress: true
                Keys.onPressed: {
                    switch (event.key) {
                        case Qt.Key_Tab:
                            event.accepted = true;
                            emailField.focus = true;
                            break;
                        case Qt.Key_Backtab:
                            event.accepted = true;
                            metaverseServerField.focus = true;
                            break;
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                            event.accepted = true;
                            linkAccountBody.login();
                            break;
                    }
                }
                onFocusChanged: {
                    root.text = "";
                    if (focus) {
                        root.isPassword = false;
                    }
                }
            }
            HifiControlsUit.TextField {
                id: emailField
                width: root.bannerWidth
                height: linkAccountBody.textFieldHeight
                font.pixelSize: linkAccountBody.textFieldFontSize
                styleRenderType: Text.QtRendering
                anchors {
                    top: displayNameField.bottom
                    topMargin: 1.5 * hifi.dimensions.contentSpacing.y
                }
                placeholderText: "Username or Email"
                activeFocusOnPress: true
                Keys.onPressed: {
                    switch (event.key) {
                        case Qt.Key_Tab:
                            event.accepted = true;
                            passwordField.focus = true;
                            break;
                        case Qt.Key_Backtab:
                            event.accepted = true;
                            displayNameField.focus = true;
                            break;
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                            event.accepted = true;
                            
                            linkAccountBody.login();
                            break;
                    }
                }
                onFocusChanged: {
                    root.text = "";
                    if (focus) {
                        root.isPassword = false;
                    }
                }
            }
            HifiControlsUit.TextField {
                id: passwordField
                width: root.bannerWidth
                height: linkAccountBody.textFieldHeight
                font.pixelSize: linkAccountBody.textFieldFontSize
                styleRenderType: Text.QtRendering
                placeholderText: "Password"
                activeFocusOnPress: true
                echoMode: passwordFieldMouseArea.showPassword ? TextInput.Normal : TextInput.Password
                anchors {
                    top: emailField.bottom
                    topMargin: 1.5 * hifi.dimensions.contentSpacing.y
                }

                onFocusChanged: {
                    root.text = "";
                    root.isPassword = focus;
                }

                Item {
                    id: showPasswordContainer
                    z: 10
                    // width + image's rightMargin
                    width: showPasswordImage.width + 8
                    height: parent.height
                    anchors {
                        right: parent.right
                    }

                    Image {
                        id: showPasswordImage
                        width: passwordField.height * passwordImageRatio
                        height: passwordField.height * passwordImageRatio
                        anchors {
                            right: parent.right
                            rightMargin: 8
                            top: parent.top
                            topMargin: passwordFieldMouseArea.showPassword ? 6 : 8
                            bottom: parent.bottom
                            bottomMargin: passwordFieldMouseArea.showPassword ? 5 : 8
                        }
                        source: passwordFieldMouseArea.showPassword ?  "../../images/eyeClosed.svg" : "../../images/eyeOpen.svg"
                        MouseArea {
                            id: passwordFieldMouseArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            property bool showPassword: false
                            onClicked: {
                                showPassword = !showPassword;
                            }
                        }
                    }
                }
                Keys.onPressed: {
                    switch (event.key) {
                        case Qt.Key_Tab:
                            event.accepted = true;
                            metaverseServerField.focus = true;
                            break;
                        case Qt.Key_Backtab:
                            event.accepted = true;
                            emailField.focus = true;
                            break;
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                            event.accepted = true;
                            linkAccountBody.login();
                            break;
                    }
                }
            }
            HifiControlsUit.TextField {
                id: metaverseServerField
                width: root.bannerWidth
                height: linkAccountBody.textFieldHeight
                font.pixelSize: linkAccountBody.textFieldFontSize
                styleRenderType: Text.QtRendering
                anchors {
                    top: passwordField.bottom
                    topMargin: 1.5 * hifi.dimensions.contentSpacing.y
                }
                placeholderText: "Metaverse Server (optional)"
                activeFocusOnPress: true
                Keys.onPressed: {
                    switch (event.key) {
                        case Qt.Key_Tab:
                            event.accepted = true;
                            displayNameField.focus = true;
                            break;
                        case Qt.Key_Backtab:
                            event.accepted = true;
                            passwordField.focus = true;
                            break;
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                            event.accepted = true;
                            if (!isLoggingInToDomain) {
                                console.log("Setting metaverse server to", metaverseServerField.text);
                                Settings.setValue("private/selectedMetaverseURL", metaverseServerField.text);
                            }
                            linkAccountBody.login();
                            break;
                    }
                }
                onFocusChanged: {
                    root.text = "";
                    if (focus) {
                        root.isPassword = false;
                    }
                }
            }
            HifiControlsUit.CheckBox {
                id: keepMeLoggedInCheckbox
                checked: !isLoggingInToDomain ? Settings.getValue("keepMeLoggedIn", false) : false;  // ####### TODO
                text: qsTr("Keep Me Logged In");
                boxSize: 18;
                labelFontFamily: linkAccountBody.fontFamily
                labelFontSize: 18;
                color: hifi.colors.white;
                visible: !isLoggingInToDomain
                anchors {
                    top: metaverseServerField.bottom;
                    topMargin: hifi.dimensions.contentSpacing.y;
                    left: metaverseServerField.left;
                }
                onCheckedChanged: {
                    Settings.setValue("keepMeLoggedIn", checked);
                    if (!isLoggingInToDomain) {
                        if (keepMeLoggedInCheckbox.checked) {
                            Settings.setValue("keepMeLoggedIn/savedUsername", emailField.text);
                        } else {
                            Settings.setValue("keepMeLoggedIn/savedUsername", "");
                        }
                    } else {
                        // ####### TODO
                    }
                }
                Component.onCompleted: {
                    if (!isLoggingInToDomain) {
                        keepMeLoggedInCheckbox.checked = !Account.loggedIn;
                    } else {
                        // ####### TODO
                    }
                }
            }
            HifiControlsUit.Button {
                id: cancelButton
                width: (passwordField.width - hifi.dimensions.contentSpacing.x) / 2;
                height: d.minHeightButton
                text: qsTr("Cancel")
                fontFamily: linkAccountBody.fontFamily
                fontSize: linkAccountBody.fontSize
                fontBold: linkAccountBody.fontBold
                color: hifi.buttons.noneBorderlessWhite;
                visible: linkAccountBody.linkSteam || linkAccountBody.linkOculus
                anchors {
                    top: keepMeLoggedInCheckbox.bottom
                    topMargin: hifi.dimensions.contentSpacing.y
                }
                onClicked: {
                    if (linkAccountBody.loginDialogPoppedUp) {
                        var data = {
                            "action": "user clicked cancel at link account screen"
                        };
                        UserActivityLogger.logAction("encourageLoginDialog", data);
                    }
                    bodyLoader.setSource("CompleteProfileBody.qml", { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader, "withSteam": linkAccountBody.withSteam,
                        "withOculus": linkAccountBody.withOculus, "errorString": "" });
                }
            }
            HifiControlsUit.Button {
                id: loginButton
                width: passwordField.width
                height: d.minHeightButton
                text: qsTr("Log In")
                fontFamily: linkAccountBody.fontFamily
                fontSize: linkAccountBody.fontSize
                fontBold: linkAccountBody.fontBold
                anchors {
                    top: keepMeLoggedInCheckbox.bottom
                    topMargin: hifi.dimensions.contentSpacing.y
                }
                onClicked: {
                    linkAccountBody.login();
                }
            }
            TextMetrics {
                id: cantAccessTextMetrics
                font: cantAccessText.font
                text: "Can't access your account?"
            }
            HifiStylesUit.ShortcutText {
                id: cantAccessText
                z: 10
                visible: !linkAccountBody.linkSteam && !linkAccountBody.linkOculus && !linkAccountBody.isLoggingInToDomain
                anchors {
                    top: loginButton.bottom
                    topMargin: hifi.dimensions.contentSpacing.y
                    left: displayNameField.left
                }
                font.family: linkAccountBody.fontFamily
                font.pixelSize: linkAccountBody.textFieldFontSize
                font.bold: linkAccountBody.fontBold

                text: "<a href='https://metaverse.vircadia.com/users/password/new'> Can't access your account?</a>"

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                linkColor: hifi.colors.blueAccent
                onLinkActivated: {
                    Tablet.playSound(TabletEnums.ButtonClick);
                    Window.openUrl(link);
                    lightboxPopup.titleText = "Can't Access Account";
                    lightboxPopup.bodyText = lightboxPopup.cantAccessBodyText;
                    lightboxPopup.button2text = "CLOSE";
                    lightboxPopup.button2method = function() {
                        lightboxPopup.visible = false;
                    }
                    lightboxPopup.visible = true;
                    if (linkAccountBody.loginDialogPoppedUp) {
                        var data = {
                            "action": "user clicked can't access account"
                        };
                        UserActivityLogger.logAction("encourageLoginDialog", data);
                    }
                }
            }
            HifiControlsUit.Button {
                id: continueButton;
                width: displayNameField.width;
                height: d.minHeightButton
                color: hifi.buttons.none;
                anchors {
                    top: cantAccessText.bottom
                    topMargin: hifi.dimensions.contentSpacing.y
                    left: displayNameField.left
                }
                text: qsTr("CONTINUE WITH STEAM")
                fontFamily: linkAccountBody.fontFamily
                fontSize: linkAccountBody.fontSize
                fontBold: linkAccountBody.fontBold
                buttonGlyph: hifi.glyphs.steamSquare
                buttonGlyphSize: 24
                buttonGlyphRightMargin: 10
                onClicked: {
                    if (loginDialog.isOculusRunning()) {
                        linkAccountBody.withOculus = true;
                        loginDialog.loginThroughOculus();
                    } else
                    if (loginDialog.isSteamRunning()) {
                        linkAccountBody.withSteam = true;
                        loginDialog.loginThroughSteam();
                    }
                    if (linkAccountBody.loginDialogPoppedUp) {
                        var data;
                        if (linkAccountBody.withOculus) {
                            data = {
                                "action": "user clicked login through Oculus"
                            };
                        } else if (linkAccountBody.withSteam) {
                            data = {
                                "action": "user clicked login through Steam"
                            };
                        }
                        UserActivityLogger.logAction("encourageLoginDialog", data);
                    }

                    bodyLoader.setSource("LoggingInBody.qml", { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader,
                        "withSteam": linkAccountBody.withSteam, "withOculus": linkAccountBody.withOculus, "linkSteam": linkAccountBody.linkSteam, "linkOculus": linkAccountBody.linkOculus,
                        "displayName":displayNameField.text});
                }
                Component.onCompleted: {
                    if (linkAccountBody.linkSteam || linkAccountBody.linkOculus) {
                        continueButton.visible = false;
                        return;
                    }
                    if (loginDialog.isOculusRunning()) {
                        continueButton.text = qsTr("CONTINUE WITH OCULUS");
                        continueButton.buttonGlyph = hifi.glyphs.oculus;
                    } else if (loginDialog.isSteamRunning()) {
                        continueButton.text = qsTr("CONTINUE WITH STEAM");
                        continueButton.buttonGlyph = hifi.glyphs.steamSquare;
                    } else {
                        continueButton.visible = false;
                    }
                }
            }
        }

        Item {
            id: signUpContainer
            width: loginContainer.width
            height: signUpTextMetrics.height
            visible: !linkAccountBody.linkSteam && !linkAccountBody.linkOculus && !linkAccountBody.isLoggingInToDomain
            anchors {
                left: loginContainer.left
                top: loginContainer.bottom
                topMargin: 0.05 * parent.height
            }
            TextMetrics {
                id: signUpTextMetrics
                font: signUpText.font
                text: signUpText.text
            }
            Text {
                id: signUpText
                text: qsTr("Don't have an account?")
                anchors {
                    left: parent.left
                }
                lineHeight: 1
                color: "white"
                font.family: linkAccountBody.fontFamily
                font.pixelSize: linkAccountBody.textFieldFontSize
                font.bold: linkAccountBody.fontBold
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

            HifiStylesUit.ShortcutText {
                id: signUpShortcutText
                z: 10
                font.family: linkAccountBody.fontFamily
                font.pixelSize: linkAccountBody.textFieldFontSize
                font.bold: linkAccountBody.fontBold
                anchors {
                     left: signUpText.right
                     leftMargin: hifi.dimensions.contentSpacing.x
                }

                text: "<a href='https://metaverse.vircadia.com/users/register'>Sign Up</a>"

                linkColor: hifi.colors.blueAccent
                onLinkActivated: {
                    Tablet.playSound(TabletEnums.ButtonClick);
                    if (linkAccountBody.loginDialogPoppedUp) {
                        var data = {
                            "action": "user clicked sign up button"
                        };
                        UserActivityLogger.logAction("encourageLoginDialog", data);
                    }
                    bodyLoader.setSource("SignUpBody.qml", { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader,
                        "errorString": "" });
                }
            }

            Text {
                id: signUpTextSecond
                text: qsTr("or")
                anchors {
                    left: signUpShortcutText.right
                    leftMargin: hifi.dimensions.contentSpacing.x
                }
                lineHeight: 1
                color: "white"
                font.family: linkAccountBody.fontFamily
                font.pixelSize: linkAccountBody.textFieldFontSize
                font.bold: linkAccountBody.fontBold
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                visible: loginDialog.getLoginDialogPoppedUp() && !linkAccountBody.linkSteam && !linkAccountBody.linkOculus;
            }

            TextMetrics {
                id: dismissButtonTextMetrics
                font: loginErrorMessage.font
                text: dismissButton.text
            }
            HifiControlsUit.Button {
                id: dismissButton
                width: loginButton.width
                height: d.minHeightButton
                anchors {
                    top: signUpText.bottom
                    topMargin: hifi.dimensions.contentSpacing.y
                    left: loginButton.left
                }
                text: qsTr("Use without account, log in anonymously")
                fontCapitalization: Font.MixedCase
                fontFamily: linkAccountBody.fontFamily
                fontSize: linkAccountBody.fontSize
                fontBold: linkAccountBody.fontBold
                visible: loginDialog.getLoginDialogPoppedUp() && !linkAccountBody.linkSteam && !linkAccountBody.linkOculus;
                onClicked: {
                    if (linkAccountBody.loginDialogPoppedUp) {
                        var data = {
                            "action": "user dismissed login screen"
                        };
                        UserActivityLogger.logAction("encourageLoginDialog", data);
                        loginDialog.dismissLoginDialog();
                    }
                    root.tryDestroy();
                }
            }
        }
    }

    Connections {
        target: loginDialog
        onFocusEnabled: {
            if (!linkAccountBody.lostFocus) {
                Qt.callLater(function() {
                    displayNameField.forceActiveFocus();
                });
            }
        }
        onFocusDisabled: {
            linkAccountBody.lostFocus = !root.isTablet && !root.isOverlay;
            if (linkAccountBody.lostFocus) {
                Qt.callLater(function() {
                    displayNameField.focus = false;
                    emailField.focus = false;
                    passwordField.focus = false;
                });
            }
        }
    }

    Component.onCompleted: {
        //but rise Tablet's one instead for Tablet interface
        root.keyboardEnabled = HMD.active;
        root.keyboardRaised = Qt.binding( function() { return keyboardRaised; })
        root.text = "";
        d.resize();
        init();
        Qt.callLater(function() {
            displayNameField.forceActiveFocus();
        });
    }

    Keys.onPressed: {
        if (!visible) {
            return;
        }

        switch (event.key) {
            case Qt.Key_Enter:
            case Qt.Key_Return:
                event.accepted = true;
                Settings.setValue("keepMeLoggedIn/savedUsername", emailField.text);
                linkAccountBody.login();
                break;
        }
    }
}
