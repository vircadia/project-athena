//
//  AvatarApp.qml
//
//  Created by Zach Fox on 2019-05-02
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.10
import QtQuick.Layouts 1.3
import "../simplifiedConstants" as SimplifiedConstants
import "../simplifiedControls" as SimplifiedControls
import "./components" as AvatarAppComponents
import stylesUit 1.0 as HifiStylesUit
import TabletScriptingInterface 1.0
import "qrc:////qml//hifi//models" as HifiModels  // Absolute path so the same code works everywhere.

Rectangle {
    id: root

    property bool inventoryReceived: false
    property bool isDebuggingFirstUseTutorial: false
    property bool keyboardRaised: false
    property int numUpdatesAvailable: 0
    property string avatarPreviewUrl: ""

    onAvatarPreviewUrlChanged: {
        sendToScript({
            "source": "AvatarApp.qml",
            "method": "updateAvatarThumbnailURL",
            "data": {
                "avatarThumbnailURL": root.avatarPreviewUrl
            }
        });
    }

    SimplifiedConstants.SimplifiedConstants {
        id: simplifiedUI
    }

    color: simplifiedUI.colors.darkBackground

    Component.onCompleted: {
        Commerce.getLoginStatus();
    }

    Connections {
        target: MyAvatar

        onSkeletonModelURLChanged: {
            root.updatePreviewUrl();
        }
    }

    Image {
        id: accent
        source: "images/accent.svg"
        anchors.top: parent.top
        anchors.right: parent.right
        width: 60
        height: 103
        transform: Scale {
            yScale: -1
            origin.x: accent.width / 2
            origin.y: accent.height / 2
        }
    }

    AvatarAppComponents.DisplayNameHeader {
        id: displayNameHeader
        previewUrl: root.avatarPreviewUrl
        loading: !inventoryContentsList.visible
        anchors.top: parent.top
        anchors.topMargin: 30
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.rightMargin: 24
    }


    Item {
        id: emptyInventoryContainer
        visible: false
        anchors.top: displayNameHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Flickable {
            id: emptyInventoryFlickable
            anchors.fill: parent
            contentWidth: parent.width
            contentHeight: emptyInventoryLayout.height
            clip: true

            ColumnLayout {
                id: emptyInventoryLayout
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.leftMargin: 26
                anchors.right: parent.right
                anchors.rightMargin: 26
                spacing: 0

                HifiStylesUit.GraphikSemiBold {
                    text: "Stand out from the crowd!"
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: paintedHeight
                    Layout.topMargin: 16
                    size: 28
                    color: simplifiedUI.colors.text.white
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                HifiStylesUit.GraphikRegular {
                    text: "Create your custom avatar."
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: paintedHeight
                    Layout.topMargin: 2
                    size: 18
                    wrapMode: Text.Wrap
                    color: simplifiedUI.colors.text.white
                    horizontalAlignment: Text.AlignHCenter
                }

                Image {
                    id: avatarImage;
                    source: "images/avatarProfilePic.png"
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: 450
                    Layout.alignment: Qt.AlignHCenter
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }

                Image {
                    source: "images/qrCode.jpg"
                    Layout.preferredWidth: 190
                    Layout.preferredHeight: 190
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: -160
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }

                HifiStylesUit.GraphikSemiBold {
                    text: "Scan for Mobile App"
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: paintedHeight
                    Layout.topMargin: 12
                    size: 28
                    color: simplifiedUI.colors.text.white
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }

        SimplifiedControls.VerticalScrollBar {
            parent: emptyInventoryFlickable
        }
    }


    Item {
        id: avatarInfoTextContainer
        visible: !emptyInventoryContainer.visible
        width: parent.implicitWidth
        height: childrenRect.height
        anchors.top: displayNameHeader.bottom
        anchors.topMargin: 30
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.rightMargin: 24

        HifiStylesUit.GraphikRegular {
            id: yourAvatarsTitle
            text: "Your Avatars"
            anchors.top: parent.top
            verticalAlignment: TextInput.AlignVCenter
            horizontalAlignment: TextInput.AlignLeft
            color: simplifiedUI.colors.text.white
            size: 22
        }
        HifiStylesUit.GraphikRegular {
            id: yourAvatarsSubtitle
            text: "These are the avatars that you've created and uploaded via the Avatar Creator."
            width: parent.width
            wrapMode: Text.Wrap
            anchors.top: yourAvatarsTitle.bottom
            anchors.topMargin: 6
            verticalAlignment: TextInput.AlignVCenter
            horizontalAlignment: TextInput.AlignLeft
            color: simplifiedUI.colors.text.darkGrey
            size: 14
        }
    }

    HifiModels.PSFListModel {
        id: avatarAppInventoryModel
        itemsPerPage: 4
        listModelName: 'inventory'
        listView: inventoryContentsList
        getPage: function () {
            var editionFilter = "";
            var primaryFilter = "avatar";
            var titleFilter = "";

            Commerce.inventory(
                editionFilter,
                primaryFilter,
                titleFilter,
                avatarAppInventoryModel.currentPageToRetrieve,
                avatarAppInventoryModel.itemsPerPage
            );
        }
        processPage: function(data) {
            inventoryReceived = true;
            data.assets.forEach(function (item) {
                if (item.status.length > 1) { console.warn("Unrecognized inventory status", item); }
                item.status = item.status[0];
            });
            return data.assets;
        }
    }

    Item {
        anchors.top: avatarInfoTextContainer.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !emptyInventoryContainer.visible
            
        AnimatedImage {
            visible: !(inventoryContentsList.visible || errorText.visible)
            anchors.centerIn: parent
            width: 72
            height: width
            source: "../images/loading.gif"
        }

        ListView {
            id: inventoryContentsList
            visible: avatarAppInventoryModel.count !== 0 && !errorText.visible
            interactive: contentItem.height > height
            clip: true
            model: avatarAppInventoryModel
            anchors.fill: parent
            width: parent.width
            delegate: AvatarAppComponents.AvatarAppListDelegate {
                id: avatarAppListDelegate
                itemName: title
                itemPreviewImageUrl: preview
                itemHref: download_url
                standaloneOptimized: model.standalone_optimized
                standaloneIncompatible: model.standalone_incompatible
            }
        }

        HifiStylesUit.GraphikRegular {
            id: errorText
            text: ""
            visible: text !== ""
            anchors.fill: parent
            size: 22
            color: simplifiedUI.colors.text.white
            wrapMode: Text.Wrap 
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        SimplifiedControls.VerticalScrollBar {
            parent: inventoryContentsList
        }
    }


    function getInventory() {
        avatarAppInventoryModel.getFirstPage();
    }

    function updatePreviewUrl() {
        var previewUrl = "";
        var downloadUrl = "";
        for (var i = 0; i < avatarAppInventoryModel.count; ++i) {
            downloadUrl = avatarAppInventoryModel.get(i).download_url;
            previewUrl = avatarAppInventoryModel.get(i).preview;
            if (MyAvatar.skeletonModelURL === downloadUrl) {
                if (previewUrl.indexOf("missing.png") > -1) {
                    previewUrl = "../../images/defaultAvatar.svg"; // Extra `../` because the image is stored 2 levels up from `DisplayNameHeader.qml`
                }
                root.avatarPreviewUrl = previewUrl;
                return;
            }
        }
        
        root.avatarPreviewUrl = "../../images/defaultAvatar.svg";
    }

    function fromScript(message) {
        switch (message.method) {
            default:
                console.log('AvatarApp.qml: Unrecognized message from JS');
                break;
        }
    }
    signal sendToScript(var message);
}
