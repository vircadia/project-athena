//
//  SimplifiedTopBar.qml
//
//  Created by Zach Fox on 2019-05-02
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.10
import hifi.simplifiedUI.simplifiedControls 1.0 as SimplifiedControls
import "../simplifiedConstants" as SimplifiedConstants
import "../inputDeviceButton" as InputDeviceButton
import stylesUit 1.0 as HifiStylesUit
import TabletScriptingInterface 1.0
import QtGraphicalEffects 1.0
import "qrc:////qml//hifi//models" as HifiModels  // Absolute path so the same code works everywhere.

Rectangle {
    id: root
    focus: true
    
    signal keyPressEvent(int key, int modifiers)
    Keys.onPressed: {
        keyPressEvent(event.key, event.modifiers);
    }
    signal keyReleaseEvent(int key, int modifiers)
    Keys.onReleased: {
        keyReleaseEvent(event.key, event.modifiers);
    }

    SimplifiedConstants.SimplifiedConstants {
        id: simplifiedUI
    }

    color: simplifiedUI.colors.darkBackground
    anchors.fill: parent

    property bool inventoryFullyReceived: false

    Component.onCompleted: {
        var numTimesRun = Settings.getValue("simplifiedUI/SUIScriptExecutionCount", 0);
        numTimesRun++;
        Settings.setValue("simplifiedUI/SUIScriptExecutionCount", numTimesRun);
        Commerce.getLoginStatus();
    }

    Connections {
        target: MyAvatar

        onSkeletonModelURLChanged: {
            root.updatePreviewUrl();

            if ((MyAvatar.skeletonModelURL.indexOf("defaultAvatar") > -1 || MyAvatar.skeletonModelURL.indexOf("fst") === -1) &&
                topBarInventoryModel.count > 0) {
                Settings.setValue("simplifiedUI/alreadyAutoSelectedAvatarFromInventory", true);
                MyAvatar.useFullAvatarURL(topBarInventoryModel.get(0).download_url);
            }
        }
    }

    HifiModels.PSFListModel {
        id: topBarInventoryModel
        itemsPerPage: 8
        listModelName: 'inventory'
        getPage: function () {
            var editionFilter = "";
            var primaryFilter = "avatar";
            var titleFilter = "";

            Commerce.inventory(
                editionFilter,
                primaryFilter,
                titleFilter,
                topBarInventoryModel.currentPageToRetrieve,
                topBarInventoryModel.itemsPerPage
            );
        }
        processPage: function(data) {
            data.assets.forEach(function (item) {
                if (item.status.length > 1) { console.warn("Unrecognized inventory status", item); }
                item.status = item.status[0];
            });
            return data.assets;
        }
    }


    Item {
        id: avatarButtonContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 2
        width: 48
        height: width

        Image {
            id: avatarButtonImage
            source: "../images/defaultAvatar.svg"
            anchors.centerIn: parent
            width: 32
            height: width
            sourceSize.width: width
            sourceSize.height: height
            mipmap: true
            fillMode: Image.PreserveAspectCrop
            layer.enabled: true
            layer.effect: OpacityMask {
                maskSource: mask
            }

            MouseArea {
                id: avatarButtonImageMouseArea
                anchors.fill: parent
                hoverEnabled: enabled
                onEntered: {
                    Tablet.playSound(TabletEnums.ButtonHover);
                }
                onClicked: {
                    Tablet.playSound(TabletEnums.ButtonClick);
                    
                    if (Account.loggedIn) {
                        sendToScript({
                            "source": "SimplifiedTopBar.qml",
                            "method": "toggleAvatarApp"
                        });
                    } else {
                        DialogsManager.showLoginDialog();
                    }
                }
            }
        }

        Rectangle {
            z: -1
            id: borderMask
            width: avatarButtonImageMouseArea.containsMouse ? avatarButtonImage.width + 4 : avatarButtonImage.width - 4
            height: width
            radius: width
            anchors.centerIn: avatarButtonImage
            color: "#FFFFFF"

            Behavior on width {
                enabled: true
                SmoothedAnimation { velocity: 80 }
            }
        }

        Rectangle {
            id: mask
            anchors.fill: avatarButtonImage
            radius: avatarButtonImage.width
            visible: false
        }
    }


    InputDeviceButton.InputDeviceButton {
        id: inputDeviceButton
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: avatarButtonContainer.right
        anchors.leftMargin: 2
        width: 32
        height: width
    }


    Item {
        id: outputDeviceButtonContainer
        visible: false // An experiment. Will you notice?
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: inputDeviceButton.right
        anchors.leftMargin: 7
        width: 32
        height: width

        Image {
            id: outputDeviceButton
            property bool outputMuted: AudioScriptingInterface.avatarGain === simplifiedUI.numericConstants.mutedValue &&
                AudioScriptingInterface.serverInjectorGain === simplifiedUI.numericConstants.mutedValue &&
                AudioScriptingInterface.localInjectorGain === simplifiedUI.numericConstants.mutedValue &&
                AudioScriptingInterface.systemInjectorGain === simplifiedUI.numericConstants.mutedValue
            source: outputDeviceButton.outputMuted ? "./images/outputDeviceMuted.svg" : "./images/outputDeviceLoud.svg"
            anchors.centerIn: parent
            width: outputDeviceButton.outputMuted ? 25 : 26
            height: 22
            visible: false
            mipmap: true
        }

        ColorOverlay {
            anchors.fill: outputDeviceButton
            opacity: outputDeviceButtonMouseArea.containsMouse ? 1.0 : 0.7
            source: outputDeviceButton
            color: (outputDeviceButton.outputMuted ? simplifiedUI.colors.controls.outputVolumeButton.text.muted : simplifiedUI.colors.controls.outputVolumeButton.text.noisy)
        }

        MouseArea {
            id: outputDeviceButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                Tablet.playSound(TabletEnums.ButtonHover);
            }
            onClicked: {
                Tablet.playSound(TabletEnums.ButtonClick);

                if (!outputDeviceButton.outputMuted && !AudioScriptingInterface.muted) {
                    AudioScriptingInterface.muted = true;
                }

                sendToScript({
                    "source": "SimplifiedTopBar.qml",
                    "method": "setOutputMuted",
                    "data": {
                        "outputMuted": !outputDeviceButton.outputMuted
                    }
                });
            }
        }
    }


    Item {
        id: statusButtonContainer
        visible: false // An experiment. Will you notice?
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: outputDeviceButtonContainer.right
        anchors.leftMargin: 8
        width: 36
        height: width

        Rectangle {
            id: statusButton
            property string currentStatus
            anchors.centerIn: parent
            width: 22
            height: width
            radius: width/2
            visible: false
        }

        ColorOverlay {
            anchors.fill: statusButton
            opacity: statusButton.currentStatus ? (statusButtonMouseArea.containsMouse ? 1.0 : 0.7) : 0.7
            source: statusButton
            color: if (statusButton.currentStatus === "busy") {
                "#ff001a"
            } else if (statusButton.currentStatus === "available") {
                "#009036"
            } else if (statusButton.currentStatus) {
                "#ffed00"
            } else {
                "#7e8c81"
            }
        }

        Image {
            id: statusIcon
            source: statusButton.currentStatus === "available" ? "images/statusPresent.svg" : "images/statusAway.svg"
            anchors.centerIn: parent
            width: statusButton.currentStatus === "busy" ? 13 : 14
            height: statusButton.currentStatus === "busy" ? 2 : 10
            mipmap: true
        }

        ColorOverlay {
            anchors.fill: statusIcon
            opacity: statusButton.currentStatus ? (statusButtonMouseArea.containsMouse ? 1.0 : 0.7) : 0.7
            source: statusIcon
            color: "#ffffff"
        }

        MouseArea {
            id: statusButtonMouseArea
            anchors.fill: parent
            enabled: statusButton.currentStatus
            hoverEnabled: true
            onEntered: {
                Tablet.playSound(TabletEnums.ButtonHover);
            }
            onClicked: {
                Tablet.playSound(TabletEnums.ButtonClick);

                sendToScript({
                    "source": "SimplifiedTopBar.qml",
                    "method": "toggleStatus"
                });
            }
        }
    }


    TextMetrics {
        id: goToTextFieldMetrics
        font: goToTextField.font
        text: goToTextField.longPlaceholderText
    }


    Item {
        id: goToTextFieldContainer
        anchors.left: statusButtonContainer.right
        anchors.leftMargin: 12
        anchors.right: (hmdButtonContainer.visible ? hmdButtonContainer.left : helpButtonContainer.left)
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height

        SimplifiedControls.TextField {
            id: goToTextField
            readonly property string shortPlaceholderText: "Jump to..."
            readonly property string longPlaceholderText: "Quickly jump to a location by typing '/LocationName'"
            anchors.centerIn: parent
            width: Math.min(parent.width, 445)
            height: 35
            leftPadding: 8
            rightPadding: 8
            bottomBorderVisible: false
            backgroundColor: "#1D1D1D"
            placeholderTextColor: "#8E8E8E"
            font.pixelSize: 14
            placeholderText: width - leftPadding - rightPadding < goToTextFieldMetrics.width ? shortPlaceholderText : longPlaceholderText
            blankPlaceholderTextOnFocus: false
            clip: true
            selectByMouse: true
            autoScroll: true
            onAccepted: {
                if (goToTextField.length > 0) {
                    AddressManager.handleLookupString(goToTextField.text);
                    goToTextField.text = "";
                }
                parent.forceActiveFocus();
            }
        }
    }



    Item {
        id: hmdButtonContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: helpButtonContainer.left
        anchors.rightMargin: 3
        width: 48
        height: width
        visible: false

        Image {
            id: displayModeImage
            source: HMD.active ? "images/desktopMode.svg" : "images/vrMode.svg"
            anchors.centerIn: parent
            width: HMD.active ? 25 : 26
            height: HMD.active ? 22 : 14
            visible: false
            mipmap: true
        }

        ColorOverlay {
            anchors.fill: displayModeImage
            opacity: displayModeMouseArea.containsMouse ? 1.0 : 0.7
            source: displayModeImage
            color: simplifiedUI.colors.text.white
        }

        MouseArea {
            id: displayModeMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                Tablet.playSound(TabletEnums.ButtonHover);
            }
            onClicked: {
                Tablet.playSound(TabletEnums.ButtonClick);
                var displayPluginCount = Window.getDisplayPluginCount();
                if (HMD.active) {
                    // Switch to desktop mode - selects first VR display plugin
                    for (var i = 0; i < displayPluginCount; i++) {
                        if (!Window.isDisplayPluginHmd(i)) {
                            Window.setActiveDisplayPlugin(i);
                            return;
                        }
                    }
                } else {
                    // Switch to VR mode - selects first HMD display plugin
                    for (var i = 0; i < displayPluginCount; i++) {
                        if (Window.isDisplayPluginHmd(i)) {
                            Window.setActiveDisplayPlugin(i);
                            return;
                        }
                    }
                }
            }

            Component.onCompleted: {
                // Don't show VR button unless they have a VR headset.
                var displayPluginCount = Window.getDisplayPluginCount();
                for (var i = 0; i < displayPluginCount; i++) {
                    if (Window.isDisplayPluginHmd(i)) {
                        hmdButtonContainer.visible = true;
                        return;
                    }
                }
            }
        }
    }


    Item {
        id: helpButtonContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: settingsButtonContainer.left
        anchors.rightMargin: 4
        width: 36
        height: width

        Image {
            id: helpButtonImage
            source: "images/questionMark.svg"
            anchors.centerIn: parent
            width: 13
            height: 22
            visible: false
            mipmap: true
        }

        ColorOverlay {
            opacity: helpButtonMouseArea.containsMouse ? 1.0 : 0.7
            anchors.fill: helpButtonImage
            source: helpButtonImage
            color: simplifiedUI.colors.text.white
        }

        MouseArea {
            id: helpButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                Tablet.playSound(TabletEnums.ButtonHover);
            }
            onClicked: {
                Tablet.playSound(TabletEnums.ButtonClick);
                sendToScript({
                    "source": "SimplifiedTopBar.qml",
                    "method": "toggleHelpApp"
                });
            }
        }
    }



    Item {
        id: settingsButtonContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 3
        width: 36
        height: width

        Image {
            id: settingsButtonImage
            source: "images/settings.svg"
            anchors.centerIn: parent
            width: 22
            height: 22
            visible: false
            mipmap: true
        }

        ColorOverlay {
            opacity: settingsButtonMouseArea.containsMouse ? 1.0 : 0.7
            anchors.fill: settingsButtonImage
            source: settingsButtonImage
            color: simplifiedUI.colors.text.white
        }

        MouseArea {
            id: settingsButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                Tablet.playSound(TabletEnums.ButtonHover);
            }
            onClicked: {
                Tablet.playSound(TabletEnums.ButtonClick);
                sendToScript({
                    "source": "SimplifiedTopBar.qml",
                    "method": "toggleSettingsApp"
                });
            }
        }
    }


    function updatePreviewUrl() {        
        var previewUrl = "";
        var downloadUrl = "";
        for (var i = 0; i < topBarInventoryModel.count; ++i) {
            downloadUrl = topBarInventoryModel.get(i).download_url;
            previewUrl = topBarInventoryModel.get(i).preview;
            if (MyAvatar.skeletonModelURL === downloadUrl) {
                if (previewUrl.indexOf("missing.png") > -1) {
                    previewUrl = "../images/defaultAvatar.svg";
                }
                avatarButtonImage.source = previewUrl;
                return;
            }
        }
        
        avatarButtonImage.source = "../images/defaultAvatar.svg";
    }


    function fromScript(message) {
        if (message.source !== "simplifiedUI.js") {
            return;
        }

        switch (message.method) {
            case "updateAvatarThumbnailURL":
                if (message.data.avatarThumbnailURL.indexOf("defaultAvatar.svg") > -1) {
                    avatarButtonImage.source = "../images/defaultAvatar.svg";
                } else {
                    avatarButtonImage.source = message.data.avatarThumbnailURL;
                }
                break;

            case "updateStatusButton":
                statusButton.currentStatus = message.data.currentStatus;
                break;

            default:
                console.log('SimplifiedTopBar.qml: Unrecognized message from JS');
                break;
        }
    }
    signal sendToScript(var message)
}
