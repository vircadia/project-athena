//
//  interstitialPage.js
//  scripts/system
//
//  Created by Dante Ruiz on 08/02/2018.
//  Copyright 2012 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

/* global Script, Controller, Overlays, Quat, MyAvatar, Entities, print, Vec3, AddressManager, Render, Window, Toolbars,
   Camera, HMD, location, Account, Xform*/

(function() {
    Script.include("/~/system/libraries/Xform.js");
    Script.include("/~/system/libraries/globals.js");
    var DEBUG = false;
    var TOTAL_LOADING_PROGRESS = 3.7;
    var EPSILON = 0.05;
    var TEXTURE_EPSILON = 0.01;
    var isVisible = false;
    var VOLUME = 0.4;
    var tune = SoundCache.getSound(Script.resourcesPath() + "sounds/crystals_and_voices.mp3");
    var sample = null;
    var MAX_LEFT_MARGIN = 1.9;
    var INNER_CIRCLE_WIDTH = 4.7;
    var DEFAULT_Z_OFFSET = 5.45;
    var LOADING_IMAGE_WIDTH_PIXELS = 1024;
    var previousCameraMode = Camera.mode;

    var renderViewTask = Render.getConfig("RenderMainView");
    var toolbar = Toolbars.getToolbar("com.highfidelity.interface.toolbar.system");
    var request = Script.require('request').request;
    var BUTTON_PROPERTIES = {
        text: "Interstitial"
    };

    var tablet = null;
    var button = null;

    var errorConnectingToDomain = false;

    // Tips have a character limit of 69
    var userTips = [
        "Tip: Visit TheSpot to explore featured domains!",
        "Tip: Visit our docs online to learn more about scripting!",
        "Tip: Don't want others invading your personal space? Turn on the Bubble!",
        "Tip: Want to make a friend? Shake hands with them in VR!",
        "Tip: Enjoy live music? Visit Rust to dance your heart out!",
        "Tip: Use the Create app to import models and create custom entities.",
        "Tip: We're open source! Feel free to contribute to our code on GitHub!",
        "Tip: What emotes have you used in the Emote app?",
        "Tip: Take and share your snapshots with everyone using the Snap app.",
        "Tip: Did you know you can show websites in-world by creating a web entity?",
        "Tip: Find out more information about domains by visiting our website!",
        "Tip: Print your snapshots from the Snap app to share with others!",
        "Tip: Log in to make friends, visit new domains, and save avatars!"
    ];

    var DEFAULT_DIMENSIONS = { x: 24, y: 24, z: 24 };

    var BLACK_SPHERE = Script.resolvePath("/~/system/assets/models/black-sphere.fbx");
    var BUTTON = Script.resourcesPath() + "images/interstitialPage/button.png";
    var BUTTON_HOVER = Script.resourcesPath() + "images/interstitialPage/button_hover.png";
    var LOADING_BAR_PLACARD = Script.resourcesPath() + "images/loadingBar_placard.png";
    var LOADING_BAR_PROGRESS = Script.resourcesPath() + "images/loadingBar_progress.png";

    ModelCache.prefetch(BLACK_SPHERE);
    TextureCache.prefetch(BUTTON);
    TextureCache.prefetch(BUTTON_HOVER);
    TextureCache.prefetch(LOADING_BAR_PLACARD);
    TextureCache.prefetch(LOADING_BAR_PROGRESS);

    var loadingSphereID = Overlays.addOverlay("model", {
        name: "Loading-Sphere",
        position: Vec3.sum(Vec3.sum(MyAvatar.position, { x: 0.0, y: -1.0, z: 0.0 }), Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0.95, z: 0 })),
        orientation: Quat.multiply(Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }), MyAvatar.orientation),
        url: BLACK_SPHERE,
        dimensions: DEFAULT_DIMENSIONS,
        alpha: 1,
        visible: isVisible,
        ignoreRayIntersection: true,
        drawInFront: true,
        grabbable: false,
        parentID: MyAvatar.SELF_ID
    });

    var anchorOverlay = Overlays.addOverlay("cube", {
        dimensions: { x: 0.2, y: 0.2, z: 0.2 },
        visible: false,
        grabbable: false,
        ignoreRayIntersection: true,
        localPosition: { x: 0.0, y: getAnchorLocalYOffset(), z: DEFAULT_Z_OFFSET },
        orientation: Quat.multiply(Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }), MyAvatar.orientation),
        solid: true,
        drawInFront: true,
        parentID: loadingSphereID
    });


    var domainName = "";
    var domainNameTextID = Overlays.addOverlay("text3d", {
        name: "Loading-Destination-Card-Text",
        localPosition: { x: 0.0, y: 0.8, z: -0.001 },
        text: domainName,
        textAlpha: 1,
        backgroundAlpha: 1,
        lineHeight: 0.42,
        visible: isVisible,
        ignoreRayIntersection: true,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });

    var domainText = "";
    var domainDescription = Overlays.addOverlay("text3d", {
        name: "Loading-Hostname",
        localPosition: { x: 0.0, y: 0.32, z: 0.0 },
        text: domainText,
        textAlpha: 1,
        backgroundAlpha: 1,
        lineHeight: 0.13,
        visible: isVisible,
        ignoreRayIntersection: true,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });

    var toolTip = "";

    var domainToolTip = Overlays.addOverlay("text3d", {
        name: "Loading-Tooltip",
        localPosition: { x: 0.0, y: -1.6, z: 0.0 },
        text: toolTip,
        textAlpha: 1,
        backgroundAlpha: 0.00393,
        lineHeight: 0.13,
        visible: isVisible,
        ignoreRayIntersection: true,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });

    var loadingToTheSpotText = Overlays.addOverlay("text3d", {
        name: "Loading-Destination-Card-Text",
        localPosition: { x: 0.0, y: -1.687, z: -0.3 },
        text: "Go To TheSpot",
        textAlpha: 1,
        backgroundAlpha: 0.00393,
        lineHeight: 0.10,
        visible: isVisible,
        ignoreRayIntersection: true,
        dimensions: { x: 1, y: 0.17 },
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });

    var loadingToTheSpotID = Overlays.addOverlay("image3d", {
        name: "Loading-Destination-Card-GoTo-Image",
        localPosition: { x: 0.0, y: -1.75, z: -0.3 },
        url: BUTTON,
        alpha: 1,
        visible: isVisible,
        emissive: true,
        ignoreRayIntersection: false,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });

    var loadingToTheSpotHoverID = Overlays.addOverlay("image3d", {
        name: "Loading-Destination-Card-GoTo-Image-Hover",
        localPosition: { x: 0.0, y: -1.75, z: -0.3 },
        url: BUTTON_HOVER,
        alpha: 1,
        visible: false,
        emissive: true,
        ignoreRayIntersection: false,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0, y: 180, z: 0 }),
        parentID: anchorOverlay
    });


    var loadingBarProgress = Overlays.addOverlay("image3d", {
        name: "Loading-Bar-Progress",
        localPosition: { x: 0.0, y: -0.91, z: 0.0 },
        url: LOADING_BAR_PROGRESS,
        alpha: 1,
        dimensions: { x: TOTAL_LOADING_PROGRESS, y: 0.3},
        visible: isVisible,
        emissive: true,
        ignoreRayIntersection: false,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0.0, y: 180.0, z: 0.0 }),
        parentID: anchorOverlay,
        keepAspectRatio: false
    });

    var loadingBarPlacard = Overlays.addOverlay("image3d", {
        name: "Loading-Bar-Placard",
        localPosition: { x: 0.0, y: -0.99, z: 0.4 },
        url: LOADING_BAR_PLACARD,
        alpha: 1,
        dimensions: { x: 4, y: 2.8 },
        visible: isVisible,
        emissive: true,
        ignoreRayIntersection: false,
        drawInFront: true,
        grabbable: false,
        localOrientation: Quat.fromVec3Degrees({ x: 0.0, y: 180.0, z: 0.0 }),
        parentID: anchorOverlay
    });

    var TARGET_UPDATE_HZ = 30;
    var BASIC_TIMER_INTERVAL_MS = 1000 / TARGET_UPDATE_HZ;
    var lastInterval = Date.now();
    var currentDomain = "no domain";
    var timer = null;
    var target = 0;
    var textureMemSizeStabilityCount = 0;
    var textureMemSizeAtLastCheck = 0;

    var connectionToDomainFailed = false;

    function getAnchorLocalYOffset() {
        var loadingSpherePosition = Overlays.getProperty(loadingSphereID, "position");
        var loadingSphereOrientation = Overlays.getProperty(loadingSphereID, "rotation");
        var overlayXform = new Xform(loadingSphereOrientation, loadingSpherePosition);
        var worldToOverlayXform = overlayXform.inv();
        var headPosition = MyAvatar.getHeadPosition();
        var headPositionInOverlaySpace = worldToOverlayXform.xformPoint(headPosition);
        return headPositionInOverlaySpace.y;
    }

    function getLeftMargin(overlayID, text) {
        var textSize = Overlays.textSize(overlayID, text);
        var sizeDifference = ((INNER_CIRCLE_WIDTH - textSize.width) / 2);
        var leftMargin = -(MAX_LEFT_MARGIN - sizeDifference);
        return leftMargin;
    }

    function lerp(a, b, t) {
        return ((1 - t) * a + t * b);
    }

    function resetValues() {
        updateProgressBar(0.0);
    }

    function startInterstitialPage() {
        if (timer === null) {
            updateOverlays(false);
            startAudio();
            target = 0;
            textureMemSizeStabilityCount = 0;
            textureMemSizeAtLastCheck = 0;
            currentProgress = 0.0;
            connectionToDomainFailed = false;
            previousCameraMode = Camera.mode;
            Camera.mode = "first person look at";
            updateProgressBar(0.0);
            scaleInterstitialPage(MyAvatar.sensorToWorldScale);
            timer = Script.setTimeout(update, 2000);
        }
    }

    function toggleInterstitialPage(isInErrorState) {
        errorConnectingToDomain = isInErrorState;
        if (!errorConnectingToDomain) {
            domainChanged(location);
        }
    }

    function restartAudio() {
        tune.ready.disconnect(restartAudio);
        startAudio();
    }

    function startAudio() {
        if (tune.downloaded) {
            sample = Audio.playSound(tune, {
                localOnly: true,
                position: MyAvatar.getHeadPosition(),
                volume: VOLUME
            });
        } else {
            tune.ready.connect(restartAudio);
        }
    }

    function endAudio() {
        sample.stop();
        sample = null;
    }

    function domainChanged(domain) {
        if (domain !== currentDomain) {
            MyAvatar.restoreAnimation();
            resetValues();
            var name = location.placename;
            domainName = name.charAt(0).toUpperCase() + name.slice(1);
            var doRequest = true;
            if (name.length === 0 && location.href === "file:///~/serverless/tutorial.json") {
                domainName = "Serverless Domain (Tutorial)";
                doRequest = false;
            }
            var domainNameLeftMargin = getLeftMargin(domainNameTextID, domainName);
            var textProperties = {
                text: domainName,
                leftMargin: domainNameLeftMargin
            };

            // check to be sure we are going to look for an actual domain
            if (!domain) {
                doRequest = false;
            }

            if (doRequest) {
                var url = Account.metaverseServerURL + '/api/v1/places/' + domain;
                request({
                    uri: url
                }, function(error, data) {
                    if (data.status === "success") {
                        var domainInfo = data.data;
                        var domainDescriptionText = domainInfo.place.description;
                        var leftMargin = getLeftMargin(domainDescription, domainDescriptionText);
                        var domainDescriptionProperties = {
                            text: domainDescriptionText,
                            leftMargin: leftMargin
                        };
                        Overlays.editOverlay(domainDescription, domainDescriptionProperties);
                    }
                });
            } else {
                var domainDescriptionProperties = {
                    text: ""
                };
                Overlays.editOverlay(domainDescription, domainDescriptionProperties);
            }

            var randomIndex = Math.floor(Math.random() * userTips.length);
            var tip = userTips[randomIndex];
            var tipLeftMargin = getLeftMargin(domainToolTip, tip);
            var toolTipProperties = {
                text: tip,
                leftMargin: tipLeftMargin
            };

            Overlays.editOverlay(domainNameTextID, textProperties);
            Overlays.editOverlay(domainToolTip, toolTipProperties);


            startInterstitialPage();
            currentDomain = domain;
        }
    }

    var THE_PLACE = (About.buildVersion === "dev") ? "hifi://TheSpot-dev" : "hifi://TheSpot";
    function clickedOnOverlay(overlayID, event) {
        if (loadingToTheSpotHoverID === overlayID) {
            location.handleLookupString(THE_PLACE);
            Overlays.editOverlay(loadingToTheSpotHoverID, { visible: false });
            Overlays.editOverlay(loadingToTheSpotID, { visible: true });
        }
    }

    function onEnterOverlay(overlayID, event) {
        if (currentDomain === "no domain") {
            return;
        }
        if (overlayID === loadingToTheSpotID) {
            Overlays.editOverlay(loadingToTheSpotID, { visible: false });
            Overlays.editOverlay(loadingToTheSpotHoverID, { visible: true });
        }
    }

    function onLeaveOverlay(overlayID, event) {
        if (currentDomain === "no domain") {
            return;
        }
        if (overlayID === loadingToTheSpotHoverID) {
            Overlays.editOverlay(loadingToTheSpotHoverID, { visible: false });
            Overlays.editOverlay(loadingToTheSpotID, { visible: true });
        }
    }

    var currentProgress = 0.0;

    function updateOverlays(physicsEnabled) {

        if (isInterstitialOverlaysVisible !== !physicsEnabled && !physicsEnabled === true) {
            // visible changed to true.
            isInterstitialOverlaysVisible = !physicsEnabled;
        }

        var properties = {
            visible: !physicsEnabled
        };

        var mainSphereProperties = {
            visible: !physicsEnabled
        };

        var domainTextProperties = {
            text: domainText,
            visible: !physicsEnabled
        };

        var loadingBarProperties = {
            visible: !physicsEnabled
        };

        if (!HMD.active) {
            MyAvatar.headOrientation = Quat.multiply(Quat.cancelOutRollAndPitch(MyAvatar.headOrientation), Quat.fromPitchYawRollDegrees(-3.0, 0, 0));
        }

        renderViewTask.getConfig("LightingModel")["enableAmbientLight"] = physicsEnabled;
        renderViewTask.getConfig("LightingModel")["enableDirectionalLight"] = physicsEnabled;
        renderViewTask.getConfig("LightingModel")["enablePointLight"] = physicsEnabled;
        Overlays.editOverlay(loadingSphereID, mainSphereProperties);
        Overlays.editOverlay(loadingToTheSpotText, properties);
        Overlays.editOverlay(loadingToTheSpotID, properties);
        Overlays.editOverlay(domainNameTextID, properties);
        Overlays.editOverlay(domainDescription, domainTextProperties);
        Overlays.editOverlay(domainToolTip, properties);
        Overlays.editOverlay(loadingBarPlacard, properties);
        Overlays.editOverlay(loadingBarProgress, loadingBarProperties);

        if (!DEBUG) {
            Menu.setIsOptionChecked("Show Overlays", physicsEnabled);
            if (!HMD.active) {
                toolbar.writeProperty("visible", physicsEnabled);
            }
        }

        resetValues();

        if (physicsEnabled) {
            Camera.mode = previousCameraMode;
        }

        if (isInterstitialOverlaysVisible !== !physicsEnabled && !physicsEnabled === false) {
            // visible changed to false.
            isInterstitialOverlaysVisible = !physicsEnabled;
        }
    }

    function scaleInterstitialPage(sensorToWorldScale) {
        var yOffset = getAnchorLocalYOffset();
        var localPosition = {
            x: 0.0,
            y: yOffset,
            z: 5.45
        };

        Overlays.editOverlay(anchorOverlay, { localPosition: localPosition });
    }

    function sleep(milliseconds) {
        var start = new Date().getTime();
        for (var i = 0; i < 1e7; i++) {
            if ((new Date().getTime() - start) > milliseconds) {
                break;
            }
        }
    }

    function updateProgressBar(progress) {
        var progressPercentage = progress / TOTAL_LOADING_PROGRESS;
        var subImageWidth = progressPercentage * LOADING_IMAGE_WIDTH_PIXELS;

        var start = TOTAL_LOADING_PROGRESS / 2;
        var end = 0;
        var xLocalPosition = (progressPercentage * (end - start)) + start;
        var properties = {
            localPosition: { x: xLocalPosition, y: (HMD.active ? -0.93 : -0.91), z: 0.0 },
            dimensions: {
                x: progress,
                y: 0.3
            },
            localOrientation: Quat.fromVec3Degrees({ x: 0.0, y: 180.0, z: 0.0 }),
            subImage: {
                x: 0.0,
                y: 0.0,
                width: subImageWidth,
                height: 128
            }
        };

        Overlays.editOverlay(loadingBarProgress, properties);
    }

    var MAX_TEXTURE_STABILITY_COUNT = 30;
    var INTERVAL_PROGRESS = 0.04;
    var INTERVAL_PROGRESS_PHYSICS_ENABLED = 0.09;
    function update() {
        var renderStats = Render.getConfig("Stats");
        var physicsEnabled = Window.isPhysicsEnabled();
        var thisInterval = Date.now();
        var deltaTime = (thisInterval - lastInterval);
        lastInterval = thisInterval;

        var domainLoadingProgressPercentage = Window.domainLoadingProgress();
        var progress = ((TOTAL_LOADING_PROGRESS * 0.4) * domainLoadingProgressPercentage);
        if (progress >= target) {
            target = progress;
        }

        if (currentProgress >= ((TOTAL_LOADING_PROGRESS * 0.4) - TEXTURE_EPSILON)) {
            var textureResourceGPUMemSize = renderStats.textureResourceGPUMemSize;
            var texturePopulatedGPUMemSize = renderStats.textureResourcePopulatedGPUMemSize;

            if (textureMemSizeAtLastCheck === textureResourceGPUMemSize) {
                textureMemSizeStabilityCount++;
            } else {
                textureMemSizeStabilityCount = 0;
            }

            textureMemSizeAtLastCheck = textureResourceGPUMemSize;

            if (textureMemSizeStabilityCount >= MAX_TEXTURE_STABILITY_COUNT) {

                if (textureResourceGPUMemSize > 0) {
                    var gpuPercantage = (TOTAL_LOADING_PROGRESS * 0.6) * (texturePopulatedGPUMemSize / textureResourceGPUMemSize);
                    var totalProgress = progress + gpuPercantage;
                    if (totalProgress >= target) {
                        target = totalProgress;
                    }
                }
            }
        }

        if ((physicsEnabled && (currentProgress < TOTAL_LOADING_PROGRESS))) {
            target = TOTAL_LOADING_PROGRESS;
        }

        currentProgress = lerp(currentProgress, target, (physicsEnabled ? INTERVAL_PROGRESS_PHYSICS_ENABLED : INTERVAL_PROGRESS));

        updateProgressBar(currentProgress);

        if (errorConnectingToDomain) {
            updateOverlays(errorConnectingToDomain);
            // setting hover id to invisible
            Overlays.editOverlay(loadingToTheSpotHoverID, { visible: false });
            endAudio();
            currentDomain = "no domain";
            timer = null;
            // The toolbar doesn't become visible in time to match the speed of
            // the signal handling of redirectErrorStateChanged in both this script
            // and the redirectOverlays.js script.  Use a sleep function to ensure
            // the toolbar becomes visible again.
            sleep(300);
            if (!HMD.active) {
                toolbar.writeProperty("visible", true);
            }
            return;
        } else if ((physicsEnabled && (currentProgress >= (TOTAL_LOADING_PROGRESS - EPSILON)))) {
            updateOverlays((physicsEnabled || connectionToDomainFailed));
            // setting hover id to invisible
            Overlays.editOverlay(loadingToTheSpotHoverID, { visible: false });
            endAudio();
            currentDomain = "no domain";
            timer = null;
            return;
        }
        timer = Script.setTimeout(update, BASIC_TIMER_INTERVAL_MS);
    }
    var whiteColor = { red: 255, green: 255, blue: 255 };
    var greyColor = { red: 125, green: 125, blue: 125 };

    Overlays.mouseReleaseOnOverlay.connect(clickedOnOverlay);
    Overlays.hoverEnterOverlay.connect(onEnterOverlay);
    Overlays.hoverLeaveOverlay.connect(onLeaveOverlay);
    location.hostChanged.connect(domainChanged);
    Window.redirectErrorStateChanged.connect(toggleInterstitialPage);

    MyAvatar.sensorToWorldScaleChanged.connect(scaleInterstitialPage);
    MyAvatar.sessionUUIDChanged.connect(function() {
        var avatarSessionUUID = MyAvatar.sessionUUID;
        Overlays.editOverlay(loadingSphereID, {
            position: Vec3.sum(Vec3.sum(MyAvatar.position, {x: 0.0, y: -1.0, z: 0.0}), Vec3.multiplyQbyV(MyAvatar.orientation, {x: 0, y: 0.95, z: 0})),
            orientation: Quat.multiply(Quat.fromVec3Degrees({x: 0, y: 180, z: 0}), MyAvatar.orientation),
            parentID: avatarSessionUUID
        });
    });

    var toggle = true;
    if (DEBUG) {
        tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
        button = tablet.addButton(BUTTON_PROPERTIES);

        button.clicked.connect(function() {
            toggle = !toggle;
            updateOverlays(toggle);
        });
    }

    // set left margin of text.
    var loadingTextProperties = {
        leftMargin: getLeftMargin(loadingToTheSpotText, "Go To TheSpot") + 0.045
    };

    Overlays.editOverlay(loadingToTheSpotText, loadingTextProperties);

    function cleanup() {
        Overlays.deleteOverlay(loadingSphereID);
        Overlays.deleteOverlay(loadingToTheSpotID);
        Overlays.deleteOverlay(loadingToTheSpotHoverID);
        Overlays.deleteOverlay(domainNameTextID);
        Overlays.deleteOverlay(domainDescription);
        Overlays.deleteOverlay(domainToolTip);
        Overlays.deleteOverlay(loadingBarPlacard);
        Overlays.deleteOverlay(loadingBarProgress);
        Overlays.deleteOverlay(anchorOverlay);

        if (DEBUG) {
            tablet.removeButton(button);
        }

        renderViewTask.getConfig("LightingModel")["enableAmbientLight"] = true;
        renderViewTask.getConfig("LightingModel")["enableDirectionalLight"] = true;
        renderViewTask.getConfig("LightingModel")["enablePointLight"] = true;
        Menu.setIsOptionChecked("Show Overlays", true);
        if (!HMD.active) {
            toolbar.writeProperty("visible", true);
        }
    }

    // location.hostname may already be set by the time the script is loaded.
    // Show the interstitial page if the domain isn't loaded.
    if (!location.isConnected) {
        domainChanged(location.hostname);
    }

    Script.scriptEnding.connect(cleanup);
}());
