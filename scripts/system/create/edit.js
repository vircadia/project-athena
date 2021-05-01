//  edit.js
//
//  Created by Brad Hefta-Gaub on 10/2/14.
//  Persist toolbar by HRS 6/11/15.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  This script allows you to edit entities with a new UI/UX for mouse and trackpad based editing
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

/* global Script, SelectionDisplay, LightOverlayManager, CameraManager, Grid, GridTool, EntityListTool, Vec3, SelectionManager,
   Overlays, OverlayWebWindow, UserActivityLogger, Settings, Entities, Tablet, Toolbars, Messages, Menu, Camera,
   progressDialog, tooltip, MyAvatar, Quat, Controller, Clipboard, HMD, UndoStack, OverlaySystemWindow,
   keyUpEventFromUIWindow:true */

(function() { // BEGIN LOCAL_SCOPE

"use strict";

var EDIT_TOGGLE_BUTTON = "com.highfidelity.interface.system.editButton";

var CONTROLLER_MAPPING_NAME = "com.highfidelity.editMode";

Script.include([
    "../libraries/stringHelpers.js",
    "../libraries/dataViewHelpers.js",
    "../libraries/progressDialog.js",
    "../libraries/ToolTip.js",
    "../libraries/entityCameraTool.js",
    "../libraries/utils.js",
    "../libraries/entityIconOverlayManager.js",
    "../libraries/gridTool.js",
    "entityList/entityList.js",
    "entitySelectionTool/entitySelectionTool.js",
    "audioFeedback/audioFeedback.js",
    "modules/brokenURLReport.js"    
]);

var CreateWindow = Script.require('./modules/createWindow.js');

var TITLE_OFFSET = 60;
var CREATE_TOOLS_WIDTH = 750;
var MAX_DEFAULT_ENTITY_LIST_HEIGHT = 942;
var ENTIRE_DOMAIN_SCAN_RADIUS = 27713;

var DEFAULT_IMAGE = Script.getExternalPath(Script.ExternalPaths.Assets, "Bazaar/Assets/Textures/Defaults/Interface/default_image.jpg");
var DEFAULT_PARTICLE = Script.getExternalPath(Script.ExternalPaths.Assets, "Bazaar/Assets/Textures/Defaults/Interface/default_particle.png");

var createToolsWindow = new CreateWindow(
    Script.resolvePath("qml/EditTools.qml"),
    'Create Tools',
    'com.highfidelity.create.createToolsWindow',
    function () {
        var windowHeight = Window.innerHeight - TITLE_OFFSET;
        if (windowHeight > MAX_DEFAULT_ENTITY_LIST_HEIGHT) {
            windowHeight = MAX_DEFAULT_ENTITY_LIST_HEIGHT;
        }
        return {
            size: {
                x: CREATE_TOOLS_WIDTH,
                y: windowHeight
            },
            position: {
                x: Window.x + Window.innerWidth - CREATE_TOOLS_WIDTH,
                y: Window.y + TITLE_OFFSET
            }
        }
    },
    false
);

/**
 * @description Returns true in case we should use the tablet version of the CreateApp
 * @returns boolean
 */
var shouldUseEditTabletApp = function() {
    return HMD.active || (!HMD.active && !Settings.getValue("desktopTabletBecomesToolbar", true));
};


var selectionDisplay = SelectionDisplay;
var selectionManager = SelectionManager;

var PARTICLE_SYSTEM_URL = Script.resolvePath("assets/images/icon-particles.svg");
var POINT_LIGHT_URL = Script.resolvePath("assets/images/icon-point-light.svg");
var SPOT_LIGHT_URL = Script.resolvePath("assets/images/icon-spot-light.svg");
var ZONE_URL = Script.resolvePath("assets/images/icon-zone.svg");
var MATERIAL_URL = Script.resolvePath("assets/images/icon-material.svg");

var entityIconOverlayManager = new EntityIconOverlayManager(["Light", "ParticleEffect", "Zone", "Material"], function(entityID) {
    var properties = Entities.getEntityProperties(entityID, ["type", "isSpotlight", "parentID", "name"]);
    if (properties.type === "Light") {
        return { 
            imageURL: properties.isSpotlight ? SPOT_LIGHT_URL : POINT_LIGHT_URL 
        };
    } else if (properties.type === "Zone") {
        return { imageURL: ZONE_URL };
    } else if (properties.type === "Material") {
        if (properties.parentID !== Uuid.NULL && properties.name !== "MATERIAL_" + entityShapeVisualizerSessionName) {
            return { imageURL: MATERIAL_URL };
        } else {
            return { imageURL: "" };
        }
    } else {
        return { imageURL: PARTICLE_SYSTEM_URL };
    }
});

var hmdMultiSelectMode = false;
var expectingRotateAsClickedSurface = false;
var keepSelectedOnNextClick = false;

var cameraManager = new CameraManager();

var grid = new Grid();
var gridTool = new GridTool({
    horizontalGrid: grid,
    createToolsWindow: createToolsWindow,
    shouldUseEditTabletApp: shouldUseEditTabletApp
});
gridTool.setVisible(false);

var entityShapeVisualizerSessionName = "SHAPE_VISUALIZER_" + Uuid.generate();

var EntityShapeVisualizer = Script.require('./modules/entityShapeVisualizer.js');
var entityShapeVisualizer = new EntityShapeVisualizer(["Zone"], entityShapeVisualizerSessionName);

var entityListTool = new EntityListTool(shouldUseEditTabletApp);

selectionManager.addEventListener(function () {
    selectionDisplay.updateHandles();
    entityIconOverlayManager.updatePositions();
    entityShapeVisualizer.setEntities(selectionManager.selections);
});

var DEGREES_TO_RADIANS = Math.PI / 180.0;
var RADIANS_TO_DEGREES = 180.0 / Math.PI;

var MIN_ANGULAR_SIZE = 2;
var MAX_ANGULAR_SIZE = 45;
var allowLargeModels = true;
var allowSmallModels = true;

var DEFAULT_DIMENSION = 0.20;

var DEFAULT_DIMENSIONS = {
    x: DEFAULT_DIMENSION,
    y: DEFAULT_DIMENSION,
    z: DEFAULT_DIMENSION
};

var DEFAULT_LIGHT_DIMENSIONS = Vec3.multiply(20, DEFAULT_DIMENSIONS);

var MENU_IMPORT_FROM_FILE = "Import Entities (.json) From a File";
var MENU_IMPORT_FROM_URL = "Import Entities (.json) From a URL";
var MENU_CREATE_SEPARATOR = "Create Application";
var SUBMENU_ENTITY_EDITOR_PREFERENCES = "Edit > Preferences";
var MENU_AUTO_FOCUS_ON_SELECT = "Auto Focus on Select";
var MENU_EASE_ON_FOCUS = "Ease Orientation on Focus";
var MENU_SHOW_ICONS_IN_CREATE_MODE = "Show Icons in Create Mode";
var MENU_CREATE_ENTITIES_GRABBABLE = "Create Entities As Grabbable (except Zones, Particles, and Lights)";
var MENU_ALLOW_SELECTION_LARGE = "Allow Selecting of Large Models";
var MENU_ALLOW_SELECTION_SMALL = "Allow Selecting of Small Models";
var MENU_ALLOW_SELECTION_LIGHTS = "Allow Selecting of Lights";
var MENU_ENTITY_LIST_DEFAULT_RADIUS = "Entity List Default Radius";

var SETTING_AUTO_FOCUS_ON_SELECT = "autoFocusOnSelect";
var SETTING_EASE_ON_FOCUS = "cameraEaseOnFocus";
var SETTING_SHOW_LIGHTS_AND_PARTICLES_IN_EDIT_MODE = "showLightsAndParticlesInEditMode";
var SETTING_SHOW_ZONES_IN_EDIT_MODE = "showZonesInEditMode";
var SETTING_EDITOR_COLUMNS_SETUP = "editorColumnsSetup";
var SETTING_ENTITY_LIST_DEFAULT_RADIUS = "entityListDefaultRadius";

var SETTING_EDIT_PREFIX = "Edit/";


var CREATE_ENABLED_ICON = "icons/tablet-icons/edit-i.svg";
var CREATE_DISABLED_ICON = "icons/tablet-icons/edit-disabled.svg";

// marketplace info, etc.  not quite ready yet.
var SHOULD_SHOW_PROPERTY_MENU = false;
var INSUFFICIENT_PERMISSIONS_ERROR_MSG = "You do not have the necessary permissions to edit on this domain.";
var INSUFFICIENT_PERMISSIONS_IMPORT_ERROR_MSG = "You do not have the necessary permissions to place items on this domain.";

var isActive = false;
var createButton = null;

var IMPORTING_SVO_OVERLAY_WIDTH = 144;
var IMPORTING_SVO_OVERLAY_HEIGHT = 30;
var IMPORTING_SVO_OVERLAY_MARGIN = 5;
var IMPORTING_SVO_OVERLAY_LEFT_MARGIN = 34;
var importingSVOImageOverlay = Overlays.addOverlay("image", {
    imageURL: Script.resolvePath("assets/images/hourglass.svg"),
    width: 20,
    height: 20,
    alpha: 1.0,
    x: Window.innerWidth - IMPORTING_SVO_OVERLAY_WIDTH,
    y: Window.innerHeight - IMPORTING_SVO_OVERLAY_HEIGHT,
    visible: false
});
var importingSVOTextOverlay = Overlays.addOverlay("text", {
    font: {
        size: 14
    },
    text: "Importing SVO...",
    leftMargin: IMPORTING_SVO_OVERLAY_LEFT_MARGIN,
    x: Window.innerWidth - IMPORTING_SVO_OVERLAY_WIDTH - IMPORTING_SVO_OVERLAY_MARGIN,
    y: Window.innerHeight - IMPORTING_SVO_OVERLAY_HEIGHT - IMPORTING_SVO_OVERLAY_MARGIN,
    width: IMPORTING_SVO_OVERLAY_WIDTH,
    height: IMPORTING_SVO_OVERLAY_HEIGHT,
    backgroundColor: {
        red: 80,
        green: 80,
        blue: 80
    },
    backgroundAlpha: 0.7,
    visible: false
});

function adjustPositionPerBoundingBox(position, direction, registration, dimensions, orientation) {
    // Adjust the position such that the bounding box (registration, dimensions and orientation) lies behind the original
    // position in the given direction.
    var CORNERS = [
        { x: 0, y: 0, z: 0 },
        { x: 0, y: 0, z: 1 },
        { x: 0, y: 1, z: 0 },
        { x: 0, y: 1, z: 1 },
        { x: 1, y: 0, z: 0 },
        { x: 1, y: 0, z: 1 },
        { x: 1, y: 1, z: 0 },
        { x: 1, y: 1, z: 1 },
    ];

    // Go through all corners and find least (most negative) distance in front of position.
    var distance = 0;
    for (var i = 0, length = CORNERS.length; i < length; i++) {
        var cornerVector =
            Vec3.multiplyQbyV(orientation, Vec3.multiplyVbyV(Vec3.subtract(CORNERS[i], registration), dimensions));
        var cornerDistance = Vec3.dot(cornerVector, direction);
        distance = Math.min(cornerDistance, distance);
    }
    position = Vec3.sum(Vec3.multiply(distance, direction), position);
    return position;
}

// Handles any edit mode updates required when domains have switched
function checkEditPermissionsAndUpdate() {
    if ((createButton === null) || (createButton === undefined)) {
        //--EARLY EXIT--( nothing to safely update )
        return;
    }

    var hasRezPermissions = (Entities.canRez() || Entities.canRezTmp());
    createButton.editProperties({
        icon: (hasRezPermissions ? CREATE_ENABLED_ICON : CREATE_DISABLED_ICON),
        captionColor: (hasRezPermissions ? "#ffffff" : "#888888"),
    });

    if (!hasRezPermissions && isActive) {
        that.setActive(false);
        tablet.gotoHomeScreen();
    }
}

// Copies the properties in `b` into `a`. `a` will be modified.
function copyProperties(a, b) {
    for (var key in b) {
        a[key] = b[key];
    }
    return a;
}

const DEFAULT_DYNAMIC_PROPERTIES = {
    dynamic: true,
    damping: 0.39347,
    angularDamping: 0.39347,
    gravity: { x: 0, y: -9.8, z: 0 },
};

const DEFAULT_NON_DYNAMIC_PROPERTIES = {
    dynamic: false,
    damping: 0,
    angularDamping: 0,
    gravity: { x: 0, y: 0, z: 0 },
};

const DEFAULT_ENTITY_PROPERTIES = {
    All: {
        description: "",
        rotation: { x: 0, y: 0, z: 0, w: 1 },
        collidesWith: "static,dynamic,kinematic,otherAvatar,myAvatar",
        collisionSoundURL: "",
        cloneable: false,
        ignoreIK: true,
        canCastShadow: true,
        href: "",
        script: "",
        serverScripts:"",
        velocity: {
            x: 0,
            y: 0,
            z: 0
        },
        angularVelocity: {
            x: 0,
            y: 0,
            z: 0
        },
        restitution: 0.5,
        friction: 0.5,
        density: 1000,
        dynamic: false,
    },
    Shape: {
        shape: "Box",
        dimensions: { x: 0.2, y: 0.2, z: 0.2 },
        color: { red: 0, green: 180, blue: 239 },
    },
    Text: {
        text: "Text",
        dimensions: {
            x: 0.65,
            y: 0.3,
            z: 0.01
        },
        textColor: { red: 255, green: 255, blue: 255 },
        backgroundColor: { red: 0, green: 0, blue: 0 },
        lineHeight: 0.06,
        faceCamera: false,
    },
    Zone: {
        dimensions: {
            x: 10,
            y: 10,
            z: 10
        },
        flyingAllowed: true,
        ghostingAllowed: true,
        filter: "",
        keyLightMode: "inherit",
        keyLightColor: { red: 255, green: 255, blue: 255 },
        keyLight: {
            intensity: 1.0,
            direction: {
                x: 0.0,
                y: -0.707106769084930, // 45 degrees
                z: 0.7071067690849304
            },
            castShadows: true
        },
        ambientLightMode: "inherit",
        ambientLight: {
            ambientIntensity: 0.5,
            ambientURL: ""
        },
        hazeMode: "inherit",
        haze: {
            hazeRange: 1000,
            hazeAltitudeEffect: false,
            hazeBaseRef: 0,
            hazeColor: {
                red: 128,
                green: 154,
                blue: 179
            },
            hazeBackgroundBlend: 0,
            hazeEnableGlare: false,
            hazeGlareColor: {
                red: 255,
                green: 229,
                blue: 179
            },
        },
        shapeType: "box",
        bloomMode: "inherit",
        avatarPriority: "inherit",
        screenshare: "inherit",
    },
    Model: {
        collisionShape: "none",
        compoundShapeURL: "",
        animation: {
            url: "",
            running: false,
            allowTranslation: false,
            loop: true,
            hold: false,
            currentFrame: 0,
            firstFrame: 0,
            lastFrame: 100000,
            fps: 30.0,
        }
    },
    Image: {
        dimensions: {
            x: 0.5385,
            y: 0.2819,
            z: 0.0092
        },
        shapeType: "box",
        collisionless: true,
        keepAspectRatio: false,
        imageURL: DEFAULT_IMAGE
    },
    Web: {
        dimensions: {
            x: 1.6,
            y: 0.9,
            z: 0.01
        },
        sourceUrl: "https://vircadia.com/",
        dpi: 30,
    },
    ParticleEffect: {
        lifespan: 1.5,
        maxParticles: 10,
        textures: DEFAULT_PARTICLE,
        emitRate: 5.5,
        emitSpeed: 0,
        speedSpread: 0,
        emitDimensions: { x: 0, y: 0, z: 0 },
        emitOrientation: { x: 0, y: 0, z: 0, w: 1 },
        emitterShouldTrail: true,
        particleRadius: 0.25,
        radiusStart: 0,
        radiusSpread: 0,
        particleColor: {
            red: 255,
            green: 255,
            blue: 255
        },
        colorSpread: {
            red: 0,
            green: 0,
            blue: 0
        },
        alpha: 0,
        alphaStart: 1,
        alphaSpread: 0,
        emitAcceleration: {
            x: 0,
            y: 2.5,
            z: 0
        },
        accelerationSpread: {
            x: 0,
            y: 0,
            z: 0
        },
        particleSpin: 0,
        spinSpread: 0,
        rotateWithEntity: false,
        polarStart: 0,
        polarFinish: Math.PI,
        azimuthStart: -Math.PI,
        azimuthFinish: Math.PI
    },
    Light: {
        color: { red: 255, green: 255, blue: 255 },
        intensity: 5.0,
        dimensions: DEFAULT_LIGHT_DIMENSIONS,
        falloffRadius: 1.0,
        isSpotlight: false,
        exponent: 1.0,
        cutoff: 75.0,
    },
};

var toolBar = (function () {
    var EDIT_SETTING = "io.highfidelity.isEditing"; // for communication with other scripts
    var that = {},
        toolBar,
        activeButton = null,
        systemToolbar = null,
        dialogWindow = null,
        tablet = null;

    function createNewEntity(requestedProperties) {
        var dimensions = requestedProperties.dimensions ? requestedProperties.dimensions : DEFAULT_DIMENSIONS;
        var position = getPositionToCreateEntity();
        var entityID = null;

        var properties = {};

        copyProperties(properties, DEFAULT_ENTITY_PROPERTIES.All);

        var type = requestedProperties.type;
        if (type === "Box" || type === "Sphere") {
            copyProperties(properties, DEFAULT_ENTITY_PROPERTIES.Shape);
        } else {
            copyProperties(properties, DEFAULT_ENTITY_PROPERTIES[type]);
        }

        // We apply the requested properties first so that they take priority over any default properties.
        copyProperties(properties, requestedProperties);

        if (properties.dynamic) {
            copyProperties(properties, DEFAULT_DYNAMIC_PROPERTIES);
        } else {
            copyProperties(properties, DEFAULT_NON_DYNAMIC_PROPERTIES);
        }


        if (position !== null && position !== undefined) {
            var direction;
            if (Camera.mode === "entity" || Camera.mode === "independent") {
                direction = Camera.orientation;
            } else {
                direction = MyAvatar.orientation;
            }
            direction = Vec3.multiplyQbyV(direction, Vec3.UNIT_Z);

            var PRE_ADJUST_ENTITY_TYPES = ["Box", "Sphere", "Shape", "Text", "Image", "Web", "Material"];
            if (PRE_ADJUST_ENTITY_TYPES.indexOf(properties.type) !== -1) {

                // Adjust position of entity per bounding box prior to creating it.
                var registration = properties.registration;
                if (registration === undefined) {
                    var DEFAULT_REGISTRATION = { x: 0.5, y: 0.5, z: 0.5 };
                    registration = DEFAULT_REGISTRATION;
                }

                var orientation = properties.orientation;
                if (orientation === undefined) {
                    properties.orientation = MyAvatar.orientation;
                    var DEFAULT_ORIENTATION = properties.orientation;
                    orientation = DEFAULT_ORIENTATION;
                } else {
                    // If the orientation is already defined, we perform the corresponding rotation assuming that
                    //  our start referential is the avatar referential.
                    properties.orientation = Quat.multiply(MyAvatar.orientation, properties.orientation);
                    var DEFAULT_ORIENTATION = properties.orientation;
                    orientation = DEFAULT_ORIENTATION;
                }

                position = adjustPositionPerBoundingBox(position, direction, registration, dimensions, orientation);
            }

            position = grid.snapToSurface(grid.snapToGrid(position, false, dimensions), dimensions);
            properties.position = position;

            if (!properties.grab) {
                properties.grab = {};
                if (Menu.isOptionChecked(MENU_CREATE_ENTITIES_GRABBABLE) &&
                    !(properties.type === "Zone" || properties.type === "Light" 
                    || properties.type === "ParticleEffect" || properties.type === "Web")) {
                    properties.grab.grabbable = true;
                } else {
                    properties.grab.grabbable = false;
                }
            }

            if (type === "Model") {
                properties.visible = false;
            }

            entityID = Entities.addEntity(properties);

            var dimensionsCheckCallback = function(){
                // Adjust position of entity per bounding box after it has been created and auto-resized.
                var initialDimensions = Entities.getEntityProperties(entityID, ["dimensions"]).dimensions;
                var DIMENSIONS_CHECK_INTERVAL = 200;
                var MAX_DIMENSIONS_CHECKS = 10;
                var dimensionsCheckCount = 0;
                var dimensionsCheckFunction = function () {
                    dimensionsCheckCount++;
                    var properties = Entities.getEntityProperties(entityID, ["dimensions", "registrationPoint", "rotation"]);
                    if (!Vec3.equal(properties.dimensions, initialDimensions)) {
                        position = adjustPositionPerBoundingBox(position, direction, properties.registrationPoint,
                            properties.dimensions, properties.rotation);
                        position = grid.snapToSurface(grid.snapToGrid(position, false, properties.dimensions),
                            properties.dimensions);
                        Entities.editEntity(entityID, {
                            position: position
                        });
                        selectionManager._update(false, this);
                    } else if (dimensionsCheckCount < MAX_DIMENSIONS_CHECKS) {
                        Script.setTimeout(dimensionsCheckFunction, DIMENSIONS_CHECK_INTERVAL);
                    }
                };
                Script.setTimeout(dimensionsCheckFunction, DIMENSIONS_CHECK_INTERVAL);
            }
            // Make sure the model entity is loaded before we try to figure out
            // its dimensions. We need to give ample time to load the entity.
            var MAX_LOADED_CHECKS = 100; // 100 * 100ms = 10 seconds.
            var LOADED_CHECK_INTERVAL = 100;
            var isLoadedCheckCount = 0;
            var entityIsLoadedCheck = function() {
                isLoadedCheckCount++;
                if (isLoadedCheckCount === MAX_LOADED_CHECKS || Entities.isLoaded(entityID)) {
                    var naturalDimensions = Entities.getEntityProperties(entityID, "naturalDimensions").naturalDimensions;
                    
                    if (isLoadedCheckCount === MAX_LOADED_CHECKS) {
                        console.log("Model entity failed to load in time: " + (MAX_LOADED_CHECKS * LOADED_CHECK_INTERVAL) + " ... setting dimensions to: " + JSON.stringify(naturalDimensions))
                    }
                    
                    Entities.editEntity(entityID, {
                        visible: true,
                        dimensions: naturalDimensions
                    })
                    dimensionsCheckCallback();
                    // We want to update the selection manager again since the script has moved on without us.
                    selectionManager.clearSelections(this);
                    entityListTool.sendUpdate();
                    selectionManager.setSelections([entityID], this);
                    return;
                }
                Script.setTimeout(entityIsLoadedCheck, LOADED_CHECK_INTERVAL);
            }
            
            var POST_ADJUST_ENTITY_TYPES = ["Model"];
            if (POST_ADJUST_ENTITY_TYPES.indexOf(properties.type) !== -1) {
                Script.setTimeout(entityIsLoadedCheck, LOADED_CHECK_INTERVAL);
            }

            SelectionManager.addEntity(entityID, false, this);
            SelectionManager.saveProperties();
            pushCommandForSelections([{
                entityID: entityID,
                properties: properties
            }], [], true);

        } else {
            Window.notifyEditError("Can't create " + properties.type + ": " +
                                   properties.type + " would be out of bounds.");
        }

        selectionManager.clearSelections(this);
        entityListTool.sendUpdate();
        selectionManager.setSelections([entityID], this);

        Window.setFocus();

        return entityID;
    }

    function closeExistingDialogWindow() {
        if (dialogWindow) {
            dialogWindow.close();
            dialogWindow = null;
        }
    }

    function cleanup() {
        that.setActive(false);
        if (tablet) {
            tablet.removeButton(activeButton);
        }
        if (systemToolbar) {
            systemToolbar.removeButton(EDIT_TOGGLE_BUTTON);
        }
    }

    var buttonHandlers = {}; // only used to tablet mode

    function addButton(name, handler) {
        buttonHandlers[name] = handler;
    }

    var SHAPE_TYPE_NONE = 0;
    var SHAPE_TYPE_SIMPLE_HULL = 1;
    var SHAPE_TYPE_SIMPLE_COMPOUND = 2;
    var SHAPE_TYPE_STATIC_MESH = 3;
    var SHAPE_TYPE_BOX = 4;
    var SHAPE_TYPE_SPHERE = 5;
    var DYNAMIC_DEFAULT = false;

    var MATERIAL_MODE_UV = 0;
    var MATERIAL_MODE_PROJECTED = 1;

    function handleNewModelDialogResult(result) {
        if (result) {
            var url = result.url;
            var shapeType;
            switch (result.collisionShapeIndex) {
            case SHAPE_TYPE_SIMPLE_HULL:
                shapeType = "simple-hull";
                break;
            case SHAPE_TYPE_SIMPLE_COMPOUND:
                shapeType = "simple-compound";
                break;
            case SHAPE_TYPE_STATIC_MESH:
                shapeType = "static-mesh";
                break;
            case SHAPE_TYPE_BOX:
                shapeType = "box";
                break;
            case SHAPE_TYPE_SPHERE:
                shapeType = "sphere";
                break;
            default:
                shapeType = "none";
            }

            var dynamic = result.dynamic !== null ? result.dynamic : DYNAMIC_DEFAULT;
            if (shapeType === "static-mesh" && dynamic) {
                // The prompt should prevent this case
                print("Error: model cannot be both static mesh and dynamic.  This should never happen.");
            } else if (url) {
                createNewEntity({
                    type: "Model",
                    modelURL: url,
                    shapeType: shapeType,
                    grab: {
                        grabbable: result.grabbable
                    },
                    dynamic: dynamic,
                    useOriginalPivot: result.useOriginalPivot
                });
            }
        }
    }

    function handleNewMaterialDialogResult(result) {
        if (result) {
            var materialURL = result.textInput;
            if (materialURL === "") {
                materialURL = "materialData";
            }
            //var materialMappingMode;
            //switch (result.comboBox) {
            //    case MATERIAL_MODE_PROJECTED:
            //        materialMappingMode = "projected";
            //        break;
            //    default:
            //        shapeType = "uv";
            //}
            var materialData = "";
            if (materialURL.startsWith("materialData")) {
                materialData = JSON.stringify({
                    "materials": {}
                });
            }

            var DEFAULT_LAYERED_MATERIAL_PRIORITY = 1;
            if (materialURL) {
                createNewEntity({
                    type: "Material",
                    materialURL: materialURL,
                    //materialMappingMode: materialMappingMode,
                    priority: DEFAULT_LAYERED_MATERIAL_PRIORITY,
                    materialData: materialData
                });
            }
        }
    }

    function fromQml(message) { // messages are {method, params}, like json-rpc. See also sendToQml.
        var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
        tablet.popFromStack();
        switch (message.method) {
            case "newModelDialogAdd":
                handleNewModelDialogResult(message.params);
                closeExistingDialogWindow();
                break;
            case "newModelDialogCancel":
                closeExistingDialogWindow();
                break;
            case "newEntityButtonClicked":
                buttonHandlers[message.params.buttonName]();
                break;
            case "newMaterialDialogAdd":
                handleNewMaterialDialogResult(message.params);
                closeExistingDialogWindow();
                break;
            case "newMaterialDialogCancel":
                closeExistingDialogWindow();
                break;
        }
    }

    var entitiesToDelete = [];
    var deletedEntityTimer = null;
    var DELETE_ENTITY_TIMER_TIMEOUT = 100;

    function checkDeletedEntityAndUpdate(entityID) {
        // Allow for multiple entity deletes before updating the entities selected.
        entitiesToDelete.push(entityID);
        if (deletedEntityTimer !== null) {
            Script.clearTimeout(deletedEntityTimer);
        }
        deletedEntityTimer = Script.setTimeout(function () {
            if (entitiesToDelete.length > 0) {
                selectionManager.removeEntities(entitiesToDelete, this);
            }
            entityListTool.removeEntities(entitiesToDelete, selectionManager.selections);
            entitiesToDelete = [];
            deletedEntityTimer = null;
        }, DELETE_ENTITY_TIMER_TIMEOUT);
    }

    function initialize() {
        Script.scriptEnding.connect(cleanup);
        Window.domainChanged.connect(function () {
            if (isActive) {
                tablet.gotoHomeScreen();
            }
            that.setActive(false);
            that.clearEntityList();
            checkEditPermissionsAndUpdate();
        });

        HMD.displayModeChanged.connect(function() {
            if (isActive) {
                tablet.gotoHomeScreen();    
            }
            that.setActive(false);
        });

        Entities.canAdjustLocksChanged.connect(function (canAdjustLocks) {
            if (isActive && !canAdjustLocks) {
                that.setActive(false);
            }
            checkEditPermissionsAndUpdate();
        });

        Entities.canRezChanged.connect(checkEditPermissionsAndUpdate);
        Entities.canRezTmpChanged.connect(checkEditPermissionsAndUpdate);
        var hasRezPermissions = (Entities.canRez() || Entities.canRezTmp());

        Entities.deletingEntity.connect(checkDeletedEntityAndUpdate);

        var createButtonIconRsrc = (hasRezPermissions ? CREATE_ENABLED_ICON : CREATE_DISABLED_ICON);
        tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
        activeButton = tablet.addButton({
            captionColor: hasRezPermissions ? "#ffffff" : "#888888",
            icon: createButtonIconRsrc,
            activeIcon: "icons/tablet-icons/edit-a.svg",
            text: "CREATE",
            sortOrder: 10
        });
        createButton = activeButton;
        tablet.screenChanged.connect(function (type, url) {
            var isGoingToHomescreenOnDesktop = (!shouldUseEditTabletApp() &&
                (url === 'hifi/tablet/TabletHome.qml' || url === ''));
            if (isActive && (type !== "QML" || url !== Script.resolvePath("qml/Edit.qml")) && !isGoingToHomescreenOnDesktop) {
                that.setActive(false);
            }
        });
        tablet.fromQml.connect(fromQml);
        createToolsWindow.fromQml.addListener(fromQml);

        createButton.clicked.connect(function() {
            if ( ! (Entities.canRez() || Entities.canRezTmp()) ) {
                Window.notifyEditError(INSUFFICIENT_PERMISSIONS_ERROR_MSG);
                return;
            }

            that.toggle();
        });

        addButton("importEntitiesButton", function() {
            importEntitiesFromFile();
        });

        addButton("importEntitiesFromUrlButton", function() {
            importEntitiesFromUrl();
        });

        addButton("openAssetBrowserButton", function() {
            Window.showAssetServer();
        });
        function createNewEntityDialogButtonCallback(entityType) {
            return function() {
                if (shouldUseEditTabletApp()) {
                    // tablet version of new-model dialog
                    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
                    tablet.pushOntoStack(Script.resolvePath("qml/New" + entityType + "Dialog.qml"));
                } else {
                    closeExistingDialogWindow();
                    var qmlPath = Script.resolvePath("qml/New" + entityType + "Window.qml");
                    var DIALOG_WINDOW_SIZE = { x: 500, y: 300 };
                    dialogWindow = Desktop.createWindow(qmlPath, {
                        title: "New " + entityType + " Entity",
                        additionalFlags: Desktop.ALWAYS_ON_TOP | Desktop.CLOSE_BUTTON_HIDES,
                        presentationMode: Desktop.PresentationMode.NATIVE,
                        size: DIALOG_WINDOW_SIZE,
                        visible: true
                    });
                    dialogWindow.fromQml.connect(fromQml);
                }
            };
        }

        addButton("newModelButton", createNewEntityDialogButtonCallback("Model"));

        addButton("newShapeButton", function () {
            createNewEntity({
                type: "Shape",
                shape: "Cube",
            });
        });

        addButton("newLightButton", function () {
            createNewEntity({
                type: "Light",
            });
        });

        addButton("newTextButton", function () {
            createNewEntity({
                type: "Text",
            });
        });

        addButton("newImageButton", function () {
            createNewEntity({
                type: "Image",
            });
        });

        addButton("newWebButton", function () {
            createNewEntity({
                type: "Web",
            });
        });

        addButton("newZoneButton", function () {
            createNewEntity({
                type: "Zone",
            });
        });

        addButton("newParticleButton", function () {
            createNewEntity({
                type: "ParticleEffect",
            });
        });

        addButton("newMaterialButton", createNewEntityDialogButtonCallback("Material"));

        var deactivateCreateIfDesktopWindowsHidden = function() {
            if (!shouldUseEditTabletApp() && !entityListTool.isVisible() && !createToolsWindow.isVisible()) {
                that.setActive(false);
            }
        };
        entityListTool.interactiveWindowHidden.addListener(this, deactivateCreateIfDesktopWindowsHidden);
        createToolsWindow.interactiveWindowHidden.addListener(this, deactivateCreateIfDesktopWindowsHidden);

        that.setActive(false);
    }

    that.clearEntityList = function () {
        entityListTool.clearEntityList();
    };

    that.toggle = function () {
        that.setActive(!isActive);
        if (!isActive) {
            tablet.gotoHomeScreen();
        }
    };

    that.setActive = function (active) {
        Settings.setValue(EDIT_SETTING, active);
        if (active) {
            Controller.captureEntityClickEvents();
        } else {
            Controller.releaseEntityClickEvents();

            closeExistingDialogWindow();
        }
        if (active === isActive) {
            return;
        }
        if (active && !Entities.canRez() && !Entities.canRezTmp()) {
            Window.notifyEditError(INSUFFICIENT_PERMISSIONS_ERROR_MSG);
            return;
        }
        Messages.sendLocalMessage("edit-events", JSON.stringify({
            enabled: active
        }));
        isActive = active;
        activeButton.editProperties({isActive: isActive});
        undoHistory.setEnabled(isActive);

        var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");

        if (!isActive) {
            entityListTool.setVisible(false);
            gridTool.setVisible(false);
            grid.setEnabled(false);
            propertiesTool.setVisible(false);
            selectionManager.clearSelections(this);
            cameraManager.disable();
            selectionDisplay.disableTriggerMapping();
            tablet.landscape = false;
            Controller.disableMapping(CONTROLLER_MAPPING_NAME);
        } else {
            if (shouldUseEditTabletApp()) {
                tablet.loadQMLSource(Script.resolvePath("qml/Edit.qml"), true);
            } else {
                // make other apps inactive while in desktop mode
                tablet.gotoHomeScreen();
            }
            UserActivityLogger.enabledEdit();
            entityListTool.setVisible(true);
            entityListTool.sendUpdate();
            gridTool.setVisible(true);
            grid.setEnabled(true);
            propertiesTool.setVisible(true);
            selectionDisplay.enableTriggerMapping();
            print("starting tablet in landscape mode");
            tablet.landscape = true;
            Controller.enableMapping(CONTROLLER_MAPPING_NAME);
            // Not sure what the following was meant to accomplish, but it currently causes
            // everybody else to think that Interface has lost focus overall. fogbugzid:558
            // Window.setFocus();
        }
        entityIconOverlayManager.setVisible(isActive && Menu.isOptionChecked(MENU_SHOW_ICONS_IN_CREATE_MODE));
    };

    initialize();
    return that;
})();

var selectedEntityID;
var orientation;
var intersection;


function rayPlaneIntersection(pickRay, point, normal) { //
    //
    //  This version of the test returns the intersection of a line with a plane
    //
    var collides = Vec3.dot(pickRay.direction, normal);

    var d = -Vec3.dot(point, normal);
    var t = -(Vec3.dot(pickRay.origin, normal) + d) / collides;

    return Vec3.sum(pickRay.origin, Vec3.multiply(pickRay.direction, t));
}

function rayPlaneIntersection2(pickRay, point, normal) {
    //
    //  This version of the test returns false if the ray is directed away from the plane
    //
    var collides = Vec3.dot(pickRay.direction, normal);
    var d = -Vec3.dot(point, normal);
    var t = -(Vec3.dot(pickRay.origin, normal) + d) / collides;
    if (t < 0.0) {
        return false;
    } else {
        return Vec3.sum(pickRay.origin, Vec3.multiply(pickRay.direction, t));
    }
}

function findClickedEntity(event) {
    var pickZones = event.isControl;

    if (pickZones) {
        Entities.setZonesArePickable(true);
    }

    var pickRay = Camera.computePickRay(event.x, event.y);
    var tabletIDs = getMainTabletIDs();
    if (tabletIDs.length > 0) {
        var overlayResult = Overlays.findRayIntersection(pickRay, true, tabletIDs);
        if (overlayResult.intersects) {
            return null;
        }
    }

    var entityResult = Entities.findRayIntersection(pickRay, true); // want precision picking
    var iconResult = entityIconOverlayManager.findRayIntersection(pickRay);
    iconResult.accurate = true;

    if (pickZones) {
        Entities.setZonesArePickable(false);
    }

    var result;
    if (expectingRotateAsClickedSurface) {
        if (!SelectionManager.hasSelection() || !SelectionManager.hasUnlockedSelection()) {
            audioFeedback.rejection();
            Window.notifyEditError("You have nothing selected, or the selection is locked.");
            expectingRotateAsClickedSurface = false;
        } else {
            //Rotate Selection according the Surface Normal
            var normalRotation = Quat.lookAtSimple(Vec3.ZERO, Vec3.multiply(entityResult.surfaceNormal, -1));
            selectionDisplay.rotateSelection(normalRotation);
            //Translate Selection according the clicked Surface
            var distanceFromSurface;
            if (selectionDisplay.getSpaceMode() === "world"){
                distanceFromSurface = SelectionManager.worldDimensions.z / 2;
            } else {
                distanceFromSurface = SelectionManager.localDimensions.z / 2;
            }
            selectionDisplay.moveSelection(Vec3.sum(entityResult.intersection, Vec3.multiplyQbyV( normalRotation, {"x": 0.0, "y":0.0, "z": distanceFromSurface})));
            selectionManager._update(false, this);
            pushCommandForSelections();
            expectingRotateAsClickedSurface = false;
            audioFeedback.action();
        }
        keepSelectedOnNextClick = true;
        return null;
    } else {
        if (iconResult.intersects) {
            result = iconResult;
        } else if (entityResult.intersects) {
            result = entityResult;
        } else {
            return null;
        }

        if (!result.accurate) {
            return null;
        }

        var foundEntity = result.entityID;
        return {
            pickRay: pickRay,
            entityID: foundEntity,
            intersection: result.intersection
        };
    }
}

// Handles selections on overlays while in edit mode by querying entities from
// entityIconOverlayManager.
function handleOverlaySelectionToolUpdates(channel, message, sender) {
    var wantDebug = false;
    if (sender !== MyAvatar.sessionUUID || channel !== 'entityToolUpdates')
        return;

    var data = JSON.parse(message);

    if (data.method === "selectOverlay") {
        if (!selectionDisplay.triggered() || selectionDisplay.triggeredHand === data.hand) {
            if (wantDebug) {
                print("setting selection to overlay " + data.overlayID);
            }
            var entity = entityIconOverlayManager.findEntity(data.overlayID);

            if (entity !== null) {
                if (hmdMultiSelectMode) {
                    selectionManager.addEntity(entity, true, this);
                } else {
                    selectionManager.setSelections([entity], this);
                }
            }
        }
    }
}

function handleMessagesReceived(channel, message, sender) {
    switch( channel ){
        case 'entityToolUpdates': {
            handleOverlaySelectionToolUpdates( channel, message, sender );
            break;
        }
        default: {
            return;
        }
    }
}

Messages.subscribe("entityToolUpdates");
Messages.messageReceived.connect(handleMessagesReceived);

var mouseHasMovedSincePress = false;
var mousePressStartTime = 0;
var mousePressStartPosition = {
    x: 0,
    y: 0
};
var mouseDown = false;

function mousePressEvent(event) {
    mouseDown = true;
    mousePressStartPosition = {
        x: event.x,
        y: event.y
    };
    mousePressStartTime = Date.now();
    mouseHasMovedSincePress = false;
    mouseCapturedByTool = false;

    if (propertyMenu.mousePressEvent(event) || progressDialog.mousePressEvent(event)) {
        mouseCapturedByTool = true;
        return;
    }
    if (isActive) {
        if (cameraManager.mousePressEvent(event) || selectionDisplay.mousePressEvent(event)) {
            // Event handled; do nothing.
            return;
        }
    }
}

var mouseCapturedByTool = false;
var lastMousePosition = null;
var CLICK_TIME_THRESHOLD = 500 * 1000; // 500 ms
var CLICK_MOVE_DISTANCE_THRESHOLD = 20;
var IDLE_MOUSE_TIMEOUT = 200;

var lastMouseMoveEvent = null;

function mouseMoveEventBuffered(event) {
    lastMouseMoveEvent = event;
}

function mouseMove(event) {
    if (mouseDown && !mouseHasMovedSincePress) {
        var timeSincePressMicro = Date.now() - mousePressStartTime;

        var dX = mousePressStartPosition.x - event.x;
        var dY = mousePressStartPosition.y - event.y;
        var sqDist = (dX * dX) + (dY * dY);

        // If less than CLICK_TIME_THRESHOLD has passed since the mouse click AND the mouse has moved
        // less than CLICK_MOVE_DISTANCE_THRESHOLD distance, then don't register this as a mouse move
        // yet. The goal is to provide mouse clicks that are more lenient to small movements.
        if (timeSincePressMicro < CLICK_TIME_THRESHOLD && sqDist < CLICK_MOVE_DISTANCE_THRESHOLD) {
            return;
        }
        mouseHasMovedSincePress = true;
    }

    if (!isActive) {
        return;
    }

    // allow the selectionDisplay and cameraManager to handle the event first, if it doesn't handle it, then do our own thing
    if (selectionDisplay.mouseMoveEvent(event) || propertyMenu.mouseMoveEvent(event) || cameraManager.mouseMoveEvent(event)) {
        return;
    }

    lastMousePosition = {
        x: event.x,
        y: event.y
    };
}

function mouseReleaseEvent(event) {
    mouseDown = false;

    if (lastMouseMoveEvent) {
        mouseMove(lastMouseMoveEvent);
        lastMouseMoveEvent = null;
    }
    if (propertyMenu.mouseReleaseEvent(event)) {
        return true;
    }
    if (isActive && selectionManager.hasSelection()) {
        tooltip.show(false);
    }
    if (mouseCapturedByTool) {

        return;
    }

    cameraManager.mouseReleaseEvent(event);

    if (!mouseHasMovedSincePress) {
        mouseClickEvent(event);
    }
}

function wasTabletOrEditHandleClicked(event) {
    var rayPick = Camera.computePickRay(event.x, event.y);
    var result = Overlays.findRayIntersection(rayPick, true);
    if (result.intersects) {
        var overlayID = result.overlayID;
        var tabletIDs = getMainTabletIDs();
        if (tabletIDs.indexOf(overlayID) >= 0) {
            return true;
        } else if (selectionDisplay.isEditHandle(overlayID)) {
            return true;
        }
    }
    return false;
}

function mouseClickEvent(event) {
    var wantDebug = false;
    var result, properties, tabletClicked;
    if (isActive && event.isLeftButton) {
        result = findClickedEntity(event);
        var tabletOrEditHandleClicked = wasTabletOrEditHandleClicked(event);
        if (tabletOrEditHandleClicked) {
            return;
        }

        if (result === null || result === undefined) {
            if (!event.isShifted) {
                if (!keepSelectedOnNextClick) {
                    selectionManager.clearSelections(this);
                }
                keepSelectedOnNextClick = false;
            }
            return;
        }
        toolBar.setActive(true);
        var pickRay = result.pickRay;
        var foundEntity = result.entityID;
        if (HMD.tabletID && foundEntity === HMD.tabletID) {
            return;
        }
        properties = Entities.getEntityProperties(foundEntity);
        var halfDiagonal = Vec3.length(properties.dimensions) / 2.0;

        if (wantDebug) {
            print("Checking properties: " + properties.id + " " + " - Half Diagonal:" + halfDiagonal);
        }
        //                P         P - Model
        //               /|         A - Palm
        //              / | d       B - unit vector toward tip
        //             /  |         X - base of the perpendicular line
        //            A---X----->B  d - distance fom axis
        //              x           x - distance from A
        //
        //            |X-A| = (P-A).B
        //            X === A + ((P-A).B)B
        //            d = |P-X|

        var A = pickRay.origin;
        var B = Vec3.normalize(pickRay.direction);
        var P = properties.position;

        var x = Vec3.dot(Vec3.subtract(P, A), B);

        var angularSize = 2 * Math.atan(halfDiagonal / Vec3.distance(Camera.getPosition(), properties.position)) *
                          180 / Math.PI;

        var sizeOK = (allowLargeModels || angularSize < MAX_ANGULAR_SIZE) &&
                     (allowSmallModels || angularSize > MIN_ANGULAR_SIZE);

        if (0 < x && sizeOK) {
            selectedEntityID = foundEntity;
            orientation = MyAvatar.orientation;
            intersection = rayPlaneIntersection(pickRay, P, Quat.getForward(orientation));

            if (!event.isShifted) {
                selectionManager.setSelections([foundEntity], this);
            } else {
                selectionManager.addEntity(foundEntity, true, this);
            }
            selectionManager.saveProperties();

            if (wantDebug) {
                print("Model selected: " + foundEntity);
            }
            selectionDisplay.select(selectedEntityID, event);

            if (Menu.isOptionChecked(MENU_AUTO_FOCUS_ON_SELECT)) {
                cameraManager.enable();
                cameraManager.focus(selectionManager.worldPosition,
                    selectionManager.worldDimensions,
                    Menu.isOptionChecked(MENU_EASE_ON_FOCUS));
            }
        }
    } else if (event.isRightButton) {
        result = findClickedEntity(event);
        if (result) {
            if (SHOULD_SHOW_PROPERTY_MENU !== true) {
                return;
            }
            properties = Entities.getEntityProperties(result.entityID);
            propertyMenu.setPosition(event.x, event.y);
            propertyMenu.show();
        } else {
            propertyMenu.hide();
        }
    }
}

Controller.mousePressEvent.connect(mousePressEvent);
Controller.mouseMoveEvent.connect(mouseMoveEventBuffered);
Controller.mouseReleaseEvent.connect(mouseReleaseEvent);


// In order for editVoxels and editModels to play nice together, they each check to see if a "delete" menu item already
// exists. If it doesn't they add it. If it does they don't. They also only delete the menu item if they were the one that
// added it.
var originalLightsArePickable = Entities.getLightsArePickable();

function setupModelMenus() {
    Menu.addMenuItem({
        menuName: "Edit",
        menuItemName: "Undo",
        shortcutKey: 'Ctrl+Z',
        position: 0,
    });
    Menu.addMenuItem({
        menuName: "Edit",
        menuItemName: "Redo",
        shortcutKey: 'Ctrl+Y',
        position: 1,
    });

    Menu.addMenuItem({
        menuName: "Edit",
        menuItemName: MENU_CREATE_SEPARATOR,
        isSeparator: true
    });
    Menu.addMenuItem({
        menuName: "Edit",
        menuItemName: MENU_IMPORT_FROM_FILE,
        afterItem: MENU_CREATE_SEPARATOR
    });
    Menu.addMenuItem({
        menuName: "Edit",
        menuItemName: MENU_IMPORT_FROM_URL,
        afterItem: MENU_IMPORT_FROM_FILE
    });

    Menu.addMenu(SUBMENU_ENTITY_EDITOR_PREFERENCES);

    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_CREATE_ENTITIES_GRABBABLE,
        position: 0,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_EDIT_PREFIX + MENU_CREATE_ENTITIES_GRABBABLE, false)
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_ALLOW_SELECTION_LARGE,
        afterItem: MENU_CREATE_ENTITIES_GRABBABLE,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_LARGE, true)
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_ALLOW_SELECTION_SMALL,
        afterItem: MENU_ALLOW_SELECTION_LARGE,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_SMALL, true)
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_ALLOW_SELECTION_LIGHTS,
        afterItem: MENU_ALLOW_SELECTION_SMALL,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_LIGHTS, false)
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_AUTO_FOCUS_ON_SELECT,
        afterItem: MENU_ALLOW_SELECTION_LIGHTS,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_AUTO_FOCUS_ON_SELECT) === "true"
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_EASE_ON_FOCUS,
        afterItem: MENU_AUTO_FOCUS_ON_SELECT,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_EASE_ON_FOCUS) === "true"
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_SHOW_ICONS_IN_CREATE_MODE,
        afterItem: MENU_EASE_ON_FOCUS,
        isCheckable: true,
        isChecked: Settings.getValue(SETTING_SHOW_LIGHTS_AND_PARTICLES_IN_EDIT_MODE) !== "false"
    });
    Menu.addMenuItem({
        menuName: SUBMENU_ENTITY_EDITOR_PREFERENCES,
        menuItemName: MENU_ENTITY_LIST_DEFAULT_RADIUS,
        afterItem: MENU_SHOW_ICONS_IN_CREATE_MODE
    });

    Entities.setLightsArePickable(false);
}

setupModelMenus(); // do this when first running our script.

function cleanupModelMenus() {
    Menu.removeMenuItem("Edit", "Undo");
    Menu.removeMenuItem("Edit", "Redo");

    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_ALLOW_SELECTION_LARGE);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_ALLOW_SELECTION_SMALL);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_ALLOW_SELECTION_LIGHTS);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_AUTO_FOCUS_ON_SELECT);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_EASE_ON_FOCUS);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_SHOW_ICONS_IN_CREATE_MODE);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_CREATE_ENTITIES_GRABBABLE);
    Menu.removeMenuItem(SUBMENU_ENTITY_EDITOR_PREFERENCES, MENU_ENTITY_LIST_DEFAULT_RADIUS);
    Menu.removeMenu(SUBMENU_ENTITY_EDITOR_PREFERENCES);
    Menu.removeMenuItem("Edit", MENU_IMPORT_FROM_URL);
    Menu.removeMenuItem("Edit", MENU_IMPORT_FROM_FILE);
    Menu.removeSeparator("Edit", MENU_CREATE_SEPARATOR);    
}

Script.scriptEnding.connect(function () {
    toolBar.setActive(false);
    Settings.setValue(SETTING_AUTO_FOCUS_ON_SELECT, Menu.isOptionChecked(MENU_AUTO_FOCUS_ON_SELECT));
    Settings.setValue(SETTING_EASE_ON_FOCUS, Menu.isOptionChecked(MENU_EASE_ON_FOCUS));
    Settings.setValue(SETTING_SHOW_LIGHTS_AND_PARTICLES_IN_EDIT_MODE, Menu.isOptionChecked(MENU_SHOW_ICONS_IN_CREATE_MODE));

    Settings.setValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_LARGE, Menu.isOptionChecked(MENU_ALLOW_SELECTION_LARGE));
    Settings.setValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_SMALL, Menu.isOptionChecked(MENU_ALLOW_SELECTION_SMALL));
    Settings.setValue(SETTING_EDIT_PREFIX + MENU_ALLOW_SELECTION_LIGHTS, Menu.isOptionChecked(MENU_ALLOW_SELECTION_LIGHTS));


    progressDialog.cleanup();
    cleanupModelMenus();
    tooltip.cleanup();
    selectionDisplay.cleanup();
    entityShapeVisualizer.cleanup();
    Entities.setLightsArePickable(originalLightsArePickable);

    Overlays.deleteOverlay(importingSVOImageOverlay);
    Overlays.deleteOverlay(importingSVOTextOverlay);

    Controller.keyReleaseEvent.disconnect(keyReleaseEvent);
    Controller.keyPressEvent.disconnect(keyPressEvent);

    Controller.mousePressEvent.disconnect(mousePressEvent);
    Controller.mouseMoveEvent.disconnect(mouseMoveEventBuffered);
    Controller.mouseReleaseEvent.disconnect(mouseReleaseEvent);

    Messages.messageReceived.disconnect(handleMessagesReceived);
    Messages.unsubscribe("entityToolUpdates");
    createButton = null;
});

var lastOrientation = null;
var lastPosition = null;

// Do some stuff regularly, like check for placement of various overlays
Script.update.connect(function (deltaTime) {
    progressDialog.move();
    selectionDisplay.checkControllerMove();
    var dOrientation = Math.abs(Quat.dot(Camera.orientation, lastOrientation) - 1);
    var dPosition = Vec3.distance(Camera.position, lastPosition);
    if (dOrientation > 0.001 || dPosition > 0.001) {
        propertyMenu.hide();
        lastOrientation = Camera.orientation;
        lastPosition = Camera.position;
    }
    if (lastMouseMoveEvent) {
        mouseMove(lastMouseMoveEvent);
        lastMouseMoveEvent = null;
    }
});

function insideBox(center, dimensions, point) {
    return (Math.abs(point.x - center.x) <= (dimensions.x / 2.0)) &&
           (Math.abs(point.y - center.y) <= (dimensions.y / 2.0)) &&
           (Math.abs(point.z - center.z) <= (dimensions.z / 2.0));
}

function selectAllEntitiesInCurrentSelectionBox(keepIfTouching) {
    if (selectionManager.hasSelection()) {
        // Get all entities touching the bounding box of the current selection
        var boundingBoxCorner = Vec3.subtract(selectionManager.worldPosition,
            Vec3.multiply(selectionManager.worldDimensions, 0.5));
        var entities = Entities.findEntitiesInBox(boundingBoxCorner, selectionManager.worldDimensions);

        if (!keepIfTouching) {
            var isValid;
            if (selectionManager.localPosition === null || selectionManager.localPosition === undefined) {
                isValid = function (position) {
                    return insideBox(selectionManager.worldPosition, selectionManager.worldDimensions, position);
                };
            } else {
                isValid = function (position) {
                    var localPosition = Vec3.multiplyQbyV(Quat.inverse(selectionManager.localRotation),
                        Vec3.subtract(position,
                            selectionManager.localPosition));
                    return insideBox(Vec3.ZERO, selectionManager.localDimensions, localPosition);
                };
            }
            for (var i = 0; i < entities.length; ++i) {
                var properties = Entities.getEntityProperties(entities[i]);
                if (!isValid(properties.position)) {
                    entities.splice(i, 1);
                    --i;
                }
            }
        }
        selectionManager.setSelections(entities, this);
    }
}

function sortSelectedEntities(selected) {
    var sortedEntities = selected.slice();
    var begin = 0;
    while (begin < sortedEntities.length) {
        var elementRemoved = false;
        var next = begin + 1;
        while (next < sortedEntities.length) {
            var beginID = sortedEntities[begin];
            var nextID = sortedEntities[next];

            if (Entities.isChildOfParent(beginID, nextID)) {
                sortedEntities[begin] = nextID;
                sortedEntities[next] = beginID;
                sortedEntities.splice(next, 1);
                elementRemoved = true;
                break;
            } else if (Entities.isChildOfParent(nextID, beginID)) {
                sortedEntities.splice(next, 1);
                elementRemoved = true;
                break;
            }
            next++;
        }
        if (!elementRemoved) {
            begin++;
        }
    }
    return sortedEntities;
}

function recursiveDelete(entities, childrenList, deletedIDs, entityHostType) {
    var wantDebug = false;
    var entitiesLength = entities.length;
    var initialPropertySets = Entities.getMultipleEntityProperties(entities);
    var entityHostTypes = Entities.getMultipleEntityProperties(entities, 'entityHostType');
    for (var i = 0; i < entitiesLength; ++i) {
        var entityID = entities[i];

        if (entityHostTypes[i].entityHostType !== entityHostType) {
            if (wantDebug) {
                console.log("Skipping deletion of entity " + entityID + " with conflicting entityHostType: " +
                    entityHostTypes[i].entityHostType + ", expected: " + entityHostType);
            }
            continue;
        }

        var children = Entities.getChildrenIDs(entityID);
        var grandchildrenList = [];
        recursiveDelete(children, grandchildrenList, deletedIDs, entityHostType);
        childrenList.push({
            entityID: entityID,
            properties: initialPropertySets[i],
            children: grandchildrenList
        });
        deletedIDs.push(entityID);
        Entities.deleteEntity(entityID);
    }
}

function unparentSelectedEntities() {
    if (SelectionManager.hasSelection() && SelectionManager.hasUnlockedSelection()) {
        var selectedEntities = selectionManager.selections;
        var parentCheck = false;

        if (selectedEntities.length < 1) {
            audioFeedback.rejection();
            Window.notifyEditError("You must have an entity selected in order to unparent it.");
            return;
        }
        selectedEntities.forEach(function (id, index) {
            var parentId = Entities.getEntityProperties(id, ["parentID"]).parentID;
            if (parentId !== null && parentId.length > 0 && parentId !== Uuid.NULL) {
                parentCheck = true;
            }
            Entities.editEntity(id, {parentID: null});
            return true;
        });
        if (parentCheck) {
            audioFeedback.confirmation();
            if (selectedEntities.length > 1) {
                Window.notify("Entities unparented");
            } else {
                Window.notify("Entity unparented");
            }
            //Refresh
            entityListTool.sendUpdate();
            selectionManager._update(false, this);
        } else {
            audioFeedback.rejection();
            if (selectedEntities.length > 1) {
                Window.notify("Selected Entities have no parents");
            } else {
                Window.notify("Selected Entity does not have a parent");
            }
        }
    } else {
        audioFeedback.rejection();
        Window.notifyEditError("You have nothing selected or the selection has locked entities.");
    }
}
function parentSelectedEntities() {
    if (SelectionManager.hasSelection() && SelectionManager.hasUnlockedSelection()) {
        var selectedEntities = selectionManager.selections;
        if (selectedEntities.length <= 1) {
            audioFeedback.rejection();
            Window.notifyEditError("You must have multiple entities selected in order to parent them");
            return;
        }
        var parentCheck = false;
        var lastEntityId = selectedEntities[selectedEntities.length - 1];
        selectedEntities.forEach(function (id, index) {
            if (lastEntityId !== id) {
                var parentId = Entities.getEntityProperties(id, ["parentID"]).parentID;
                if (parentId !== lastEntityId) {
                    parentCheck = true;
                }
                Entities.editEntity(id, {parentID: lastEntityId});
            }
        });

        if (parentCheck) {
            audioFeedback.confirmation();
            Window.notify("Entities parented");
            //Refresh
            entityListTool.sendUpdate();
            selectionManager._update(false, this);
        } else {
            audioFeedback.rejection();
            Window.notify("Entities are already parented to last");
        }
    } else {
        audioFeedback.rejection();
        Window.notifyEditError("You have nothing selected or the selection has locked entities.");
    }
}
function deleteSelectedEntities() {
    if (SelectionManager.hasSelection() && SelectionManager.hasUnlockedSelection()) {
        var deletedIDs = [];

        SelectionManager.saveProperties();
        var savedProperties = [];
        var newSortedSelection = sortSelectedEntities(selectionManager.selections);
        var entityHostTypes = Entities.getMultipleEntityProperties(newSortedSelection, 'entityHostType');
        for (var i = 0; i < newSortedSelection.length; ++i) {
            var entityID = newSortedSelection[i];
            var initialProperties = SelectionManager.savedProperties[entityID];
            if (initialProperties.locked ||
                (initialProperties.avatarEntity && initialProperties.owningAvatarID !== MyAvatar.sessionUUID)) {
                continue;
            }
            var children = Entities.getChildrenIDs(entityID);
            var childList = [];
            recursiveDelete(children, childList, deletedIDs, entityHostTypes[i].entityHostType);
            savedProperties.push({
                entityID: entityID,
                properties: initialProperties,
                children: childList
            });
            deletedIDs.push(entityID);
            Entities.deleteEntity(entityID);
        }

        if (savedProperties.length > 0) {
            SelectionManager.clearSelections(this);
            pushCommandForSelections([], savedProperties);
            entityListTool.deleteEntities(deletedIDs);
        }
    } else {
        audioFeedback.rejection();
        Window.notifyEditError("You have nothing selected or the selection has locked entities.");        
    }
}

function toggleSelectedEntitiesLocked() {
    if (SelectionManager.hasSelection()) {
        var locked = !Entities.getEntityProperties(SelectionManager.selections[0], ["locked"]).locked;
        for (var i = 0; i < selectionManager.selections.length; i++) {
            var entityID = SelectionManager.selections[i];
            Entities.editEntity(entityID, {
                locked: locked
            });
        }
        entityListTool.sendUpdate();
        selectionManager._update(false, this);
    }
}

function toggleSelectedEntitiesVisible() {
    if (SelectionManager.hasSelection()) {
        var visible = !Entities.getEntityProperties(SelectionManager.selections[0], ["visible"]).visible;
        for (var i = 0; i < selectionManager.selections.length; i++) {
            var entityID = SelectionManager.selections[i];
            Entities.editEntity(entityID, {
                visible: visible
            });
        }
        entityListTool.sendUpdate();
        selectionManager._update(false, this);
    }
}

function onFileSaveChanged(filename) {
    Window.saveFileChanged.disconnect(onFileSaveChanged);
    if (filename !== "") {
        var success = Clipboard.exportEntities(filename, selectionManager.selections);
        if (!success) {
            Window.notifyEditError("Export failed.");
        }
    }
}

function onFileOpenChanged(filename) {
    // disconnect the event, otherwise the requests will stack up
    try {
        // Not all calls to onFileOpenChanged() connect an event.
        Window.browseChanged.disconnect(onFileOpenChanged);
    } catch (e) {
        // Ignore.
    }

    var importURL = null;
    if (filename !== "") {
        importURL = filename;
        if (!/^(http|https):\/\//.test(filename)) {
            importURL = "file:///" + importURL;
        }
    }
    if (importURL) {
        if (!isActive && (Entities.canRez() && Entities.canRezTmp())) {
            toolBar.toggle();
        }
        importSVO(importURL);
    }
}

function onPromptTextChanged(prompt) {
    Window.promptTextChanged.disconnect(onPromptTextChanged);
    if (prompt !== "") {
        if (!isActive && (Entities.canRez() && Entities.canRezTmp())) {
            toolBar.toggle();
        }
        importSVO(prompt);
    }
}

function onPromptTextChangedDefaultRadiusUserPref(prompt) {
    Window.promptTextChanged.disconnect(onPromptTextChangedDefaultRadiusUserPref);
    if (prompt !== "") {
        var radius = parseInt(prompt);
        if (radius < 0 || isNaN(radius)){
            radius = 100;
        }
        Settings.setValue(SETTING_ENTITY_LIST_DEFAULT_RADIUS, radius);
    }
}

function handleMenuEvent(menuItem) {
    if (menuItem === MENU_ALLOW_SELECTION_SMALL) {
        allowSmallModels = Menu.isOptionChecked(MENU_ALLOW_SELECTION_SMALL);
    } else if (menuItem === MENU_ALLOW_SELECTION_LARGE) {
        allowLargeModels = Menu.isOptionChecked(MENU_ALLOW_SELECTION_LARGE);
    } else if (menuItem === MENU_ALLOW_SELECTION_LIGHTS) {
        Entities.setLightsArePickable(Menu.isOptionChecked(MENU_ALLOW_SELECTION_LIGHTS));
    } else if (menuItem === "Delete") {
        deleteSelectedEntities();
    } else if (menuItem === "Undo") {
        undoHistory.undo();
    } else if (menuItem === "Redo") {
        undoHistory.redo();
    } else if (menuItem === MENU_SHOW_ICONS_IN_CREATE_MODE) {
        entityIconOverlayManager.setVisible(isActive && Menu.isOptionChecked(MENU_SHOW_ICONS_IN_CREATE_MODE));
    } else if (menuItem === MENU_CREATE_ENTITIES_GRABBABLE) {
        Settings.setValue(SETTING_EDIT_PREFIX + menuItem, Menu.isOptionChecked(menuItem));
    } else if (menuItem === MENU_ENTITY_LIST_DEFAULT_RADIUS) {
        Window.promptTextChanged.connect(onPromptTextChangedDefaultRadiusUserPref);
        Window.promptAsync("Entity List Default Radius (in meters)", "" + Settings.getValue(SETTING_ENTITY_LIST_DEFAULT_RADIUS, 100));         
    } else if (menuItem === MENU_IMPORT_FROM_FILE) {
        importEntitiesFromFile();
    } else if (menuItem === MENU_IMPORT_FROM_URL) {
        importEntitiesFromUrl();
    }
    tooltip.show(false);
}

var HALF_TREE_SCALE = 16384;

function getPositionToCreateEntity(extra) {
    var CREATE_DISTANCE = 2;
    var position;
    var delta = extra !== undefined ? extra : 0;
    if (Camera.mode === "entity" || Camera.mode === "independent") {
        position = Vec3.sum(Camera.position, Vec3.multiply(Quat.getForward(Camera.orientation), CREATE_DISTANCE + delta));
    } else {
        position = Vec3.sum(MyAvatar.position, Vec3.multiply(Quat.getForward(MyAvatar.orientation), CREATE_DISTANCE + delta));
    }

    if (position.x > HALF_TREE_SCALE || position.y > HALF_TREE_SCALE || position.z > HALF_TREE_SCALE) {
        return null;
    }
    return position;
}

function importSVO(importURL) {
    if (!Entities.canRez() && !Entities.canRezTmp()) {
        Window.notifyEditError(INSUFFICIENT_PERMISSIONS_IMPORT_ERROR_MSG);
        return;
    }

    Overlays.editOverlay(importingSVOTextOverlay, {
        visible: true
    });
    Overlays.editOverlay(importingSVOImageOverlay, {
        visible: true
    });

    var success = Clipboard.importEntities(importURL);

    if (success) {
        var VERY_LARGE = 10000;
        var isLargeImport = Clipboard.getClipboardContentsLargestDimension() >= VERY_LARGE;
        var position = Vec3.ZERO;
        if (!isLargeImport) {
            position = getPositionToCreateEntity(Clipboard.getClipboardContentsLargestDimension() / 2);
        }
        if (position !== null && position !== undefined) {
            var pastedEntityIDs = Clipboard.pasteEntities(position);
            if (!isLargeImport) {
                // The first entity in Clipboard gets the specified position with the rest being relative to it. Therefore, move
                // entities after they're imported so that they're all the correct distance in front of and with geometric mean
                // centered on the avatar/camera direction.
                var deltaPosition = Vec3.ZERO;
                var entityPositions = [];
                var entityParentIDs = [];

                var propType = Entities.getEntityProperties(pastedEntityIDs[0], ["type"]).type;
                var NO_ADJUST_ENTITY_TYPES = ["Zone", "Light", "ParticleEffect"];
                if (NO_ADJUST_ENTITY_TYPES.indexOf(propType) === -1) {
                    var targetDirection;
                    if (Camera.mode === "entity" || Camera.mode === "independent") {
                        targetDirection = Camera.orientation;
                    } else {
                        targetDirection = MyAvatar.orientation;
                    }
                    targetDirection = Vec3.multiplyQbyV(targetDirection, Vec3.UNIT_Z);

                    var targetPosition = getPositionToCreateEntity();
                    var deltaParallel = HALF_TREE_SCALE;  // Distance to move entities parallel to targetDirection.
                    var deltaPerpendicular = Vec3.ZERO;  // Distance to move entities perpendicular to targetDirection.
                    for (var i = 0, length = pastedEntityIDs.length; i < length; i++) {
                        var curLoopEntityProps = Entities.getEntityProperties(pastedEntityIDs[i], ["position", "dimensions",
                            "registrationPoint", "rotation", "parentID"]);
                        var adjustedPosition = adjustPositionPerBoundingBox(targetPosition, targetDirection,
                            curLoopEntityProps.registrationPoint, curLoopEntityProps.dimensions, curLoopEntityProps.rotation);
                        var delta = Vec3.subtract(adjustedPosition, curLoopEntityProps.position);
                        var distance = Vec3.dot(delta, targetDirection);
                        deltaParallel = Math.min(distance, deltaParallel);
                        deltaPerpendicular = Vec3.sum(Vec3.subtract(delta, Vec3.multiply(distance, targetDirection)),
                            deltaPerpendicular);
                        entityPositions[i] = curLoopEntityProps.position;
                        entityParentIDs[i] = curLoopEntityProps.parentID;
                    }
                    deltaPerpendicular = Vec3.multiply(1 / pastedEntityIDs.length, deltaPerpendicular);
                    deltaPosition = Vec3.sum(Vec3.multiply(deltaParallel, targetDirection), deltaPerpendicular);
                }

                if (grid.getSnapToGrid()) {
                    var firstEntityProps = Entities.getEntityProperties(pastedEntityIDs[0], ["position", "dimensions",
                        "registrationPoint"]);
                    var positionPreSnap = Vec3.sum(deltaPosition, firstEntityProps.position);
                    position = grid.snapToSurface(grid.snapToGrid(positionPreSnap, false, firstEntityProps.dimensions,
                            firstEntityProps.registrationPoint), firstEntityProps.dimensions, firstEntityProps.registrationPoint);
                    deltaPosition = Vec3.subtract(position, firstEntityProps.position);
                }

                if (!Vec3.equal(deltaPosition, Vec3.ZERO)) {
                    for (var editEntityIndex = 0, numEntities = pastedEntityIDs.length; editEntityIndex < numEntities; editEntityIndex++) {
                        if (Uuid.isNull(entityParentIDs[editEntityIndex])) {
                            Entities.editEntity(pastedEntityIDs[editEntityIndex], {
                                position: Vec3.sum(deltaPosition, entityPositions[editEntityIndex])
                            });
                        }
                    }
                }
            }

            if (isActive) {
                selectionManager.setSelections(pastedEntityIDs, this);
            }
        } else {
            Window.notifyEditError("Can't import entities: entities would be out of bounds.");
        }
    } else {
        Window.notifyEditError("There was an error importing the entity file.");
    }

    Overlays.editOverlay(importingSVOTextOverlay, {
        visible: false
    });
    Overlays.editOverlay(importingSVOImageOverlay, {
        visible: false
    });
}
Window.svoImportRequested.connect(importSVO);

Menu.menuItemEvent.connect(handleMenuEvent);

var keyPressEvent = function (event) {
    if (isActive) {
        cameraManager.keyPressEvent(event);
    }
};
var keyReleaseEvent = function (event) {
    if (isActive) {
        cameraManager.keyReleaseEvent(event);
    }
};
Controller.keyReleaseEvent.connect(keyReleaseEvent);
Controller.keyPressEvent.connect(keyPressEvent);

function deleteKey(value) {
    if (value === 0) { // on release
        deleteSelectedEntities();
    }
}
function deselectKey(value) {
    if (value === 0) { // on release
        selectionManager.clearSelections(this);
    }
}
function toggleKey(value) {
    if (value === 0) { // on release
        selectionDisplay.toggleSpaceMode();
    }
}
function focusKey(value) {
    if (value === 0) { // on release
        setCameraFocusToSelection();
    }
}
function gridKey(value) {
    if (value === 0) { // on release
        alignGridToSelection();
    }
}
function viewGridKey(value) {
    if (value === 0) { // on release
        toggleGridVisibility();
    }
}
function snapKey(value) {
    if (value === 0) { // on release
        entityListTool.toggleSnapToGrid();
    }
}
function gridToAvatarKey(value) {
    if (value === 0) { // on release
        alignGridToAvatar();
    }
}
function rotateAsNextClickedSurfaceKey(value) {
    if (value === 0) { // on release
        rotateAsNextClickedSurface();
    }
}
function quickRotate90xKey(value) {
    if (value === 0) { // on release
        selectionDisplay.rotate90degreeSelection("X");
    }
}
function quickRotate90yKey(value) {
    if (value === 0) { // on release
        selectionDisplay.rotate90degreeSelection("Y");
    }
}
function quickRotate90zKey(value) {
    if (value === 0) { // on release
        selectionDisplay.rotate90degreeSelection("Z");
    }
}
function recursiveAdd(newParentID, parentData) {
    if (parentData.children !== undefined) {
        var children = parentData.children;
        for (var i = 0; i < children.length; i++) {
            var childProperties = children[i].properties;
            childProperties.parentID = newParentID;
            var newChildID = Entities.addEntity(childProperties);
            recursiveAdd(newChildID, children[i]);
        }
    }
}

var UndoHistory = function(onUpdate) {
    this.history = [];
    // The current position is the index of the last executed action in the history array.
    //
    //     -1 0 1 2 3    <- position
    //        A B C D    <- actions in history
    //
    // If our lastExecutedIndex is 1, the last executed action is B.
    // If we undo, we undo B (index 1). If we redo, we redo C (index 2).
    this.lastExecutedIndex = -1;
    this.enabled = true;
    this.onUpdate = onUpdate;
};

UndoHistory.prototype.pushCommand = function(undoFn, undoArgs, redoFn, redoArgs) {
    if (!this.enabled) {
        return;
    }
    // Delete any history following the last executed action.
    this.history.splice(this.lastExecutedIndex + 1);
    this.history.push({
        undoFn: undoFn,
        undoArgs: undoArgs,
        redoFn: redoFn,
        redoArgs: redoArgs
    });
    this.lastExecutedIndex++;

    if (this.onUpdate) {
        this.onUpdate();
    }
};
UndoHistory.prototype.setEnabled = function(enabled) {
    this.enabled = enabled;
    if (this.onUpdate) {
        this.onUpdate();
    }
};
UndoHistory.prototype.canUndo = function() {
    return this.enabled && this.lastExecutedIndex >= 0;
};
UndoHistory.prototype.canRedo = function() {
    return this.enabled && this.lastExecutedIndex < this.history.length - 1;
};
UndoHistory.prototype.undo = function() {
    if (!this.canUndo()) {
        console.warn("Cannot undo action");
        return;
    }

    var command = this.history[this.lastExecutedIndex];
    command.undoFn(command.undoArgs);
    this.lastExecutedIndex--;

    if (this.onUpdate) {
        this.onUpdate();
    }
};
UndoHistory.prototype.redo = function() {
    if (!this.canRedo()) {
        console.warn("Cannot redo action");
        return;
    }

    var command = this.history[this.lastExecutedIndex + 1];
    command.redoFn(command.redoArgs);
    this.lastExecutedIndex++;

    if (this.onUpdate) {
        this.onUpdate();
    }
};

function updateUndoRedoMenuItems() {
    Menu.setMenuEnabled("Edit > Undo", undoHistory.canUndo());
    Menu.setMenuEnabled("Edit > Redo", undoHistory.canRedo());
}
var undoHistory = new UndoHistory(updateUndoRedoMenuItems);
updateUndoRedoMenuItems();

// When an entity has been deleted we need a way to "undo" this deletion.  Because it's not currently
// possible to create an entity with a specific id, earlier undo commands to the deleted entity
// will fail if there isn't a way to find the new entity id.
var DELETED_ENTITY_MAP = {};

function applyEntityProperties(data) {
    var editEntities = data.editEntities;
    var createEntities = data.createEntities;
    var deleteEntities = data.deleteEntities;
    var selectedEntityIDs = [];
    var selectEdits = createEntities.length === 0 || !data.selectCreated;
    var i, entityID, entityProperties;
    for (i = 0; i < createEntities.length; i++) {
        entityID = createEntities[i].entityID;
        entityProperties = createEntities[i].properties;
        var newEntityID = Entities.addEntity(entityProperties);
        recursiveAdd(newEntityID, createEntities[i]);
        DELETED_ENTITY_MAP[entityID] = newEntityID;
        if (data.selectCreated) {
            selectedEntityIDs.push(newEntityID);
        }
    }
    for (i = 0; i < deleteEntities.length; i++) {
        entityID = deleteEntities[i].entityID;
        if (DELETED_ENTITY_MAP[entityID] !== undefined) {
            entityID = DELETED_ENTITY_MAP[entityID];
        }
        Entities.deleteEntity(entityID);
        var index = selectedEntityIDs.indexOf(entityID);
        if (index >= 0) {
            selectedEntityIDs.splice(index, 1);
        }
    }
    for (i = 0; i < editEntities.length; i++) {
        entityID = editEntities[i].entityID;
        if (DELETED_ENTITY_MAP[entityID] !== undefined) {
            entityID = DELETED_ENTITY_MAP[entityID];
        }
        entityProperties = editEntities[i].properties;
        if (entityProperties !== null) {
            Entities.editEntity(entityID, entityProperties);
        }
        if (selectEdits) {
            selectedEntityIDs.push(entityID);
        }
    }

    // We might be getting an undo while edit.js is disabled. If that is the case, don't set
    // our selections, causing the edit widgets to display.
    if (isActive) {
        selectionManager.setSelections(selectedEntityIDs, this);
        selectionManager.saveProperties();
    }
}

// For currently selected entities, push a command to the UndoStack that uses the current entity properties for the
// redo command, and the saved properties for the undo command.  Also, include create and delete entity data.
function pushCommandForSelections(createdEntityData, deletedEntityData, doNotSaveEditProperties) {
    doNotSaveEditProperties = false;
    var undoData = {
        editEntities: [],
        createEntities: deletedEntityData || [],
        deleteEntities: createdEntityData || [],
        selectCreated: true
    };
    var redoData = {
        editEntities: [],
        createEntities: createdEntityData || [],
        deleteEntities: deletedEntityData || [],
        selectCreated: true
    };
    for (var i = 0; i < SelectionManager.selections.length; i++) {
        var entityID = SelectionManager.selections[i];
        var initialProperties = SelectionManager.savedProperties[entityID];
        var currentProperties = null;
        if (!initialProperties) {
            continue;
        }

        if (doNotSaveEditProperties) {
            initialProperties = null;
        } else {
            currentProperties = Entities.getEntityProperties(entityID);
        }

        undoData.editEntities.push({
            entityID: entityID,
            properties: initialProperties
        });
        redoData.editEntities.push({
            entityID: entityID,
            properties: currentProperties
        });
    }
    undoHistory.pushCommand(applyEntityProperties, undoData, applyEntityProperties, redoData);
}

var ServerScriptStatusMonitor = function(entityID, statusCallback) {
    var self = this;

    self.entityID = entityID;
    self.active = true;
    self.sendRequestTimerID = null;

    var onStatusReceived = function(success, isRunning, status, errorInfo) {
        if (self.active) {
            statusCallback({
                statusRetrieved: success,
                isRunning: isRunning,
                status: status,
                errorInfo: errorInfo
            });
            self.sendRequestTimerID = Script.setTimeout(function() {
                if (self.active) {
                    Entities.getServerScriptStatus(entityID, onStatusReceived);
                }
            }, 1000);
        }
    };
    self.stop = function() {
        self.active = false;
    };

    Entities.getServerScriptStatus(entityID, onStatusReceived);
};

var PropertiesTool = function (opts) {
    var that = {};

    var webView = null;
    webView = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    webView.setVisible = function(value) {};

    var visible = false;

    // This keeps track of the last entity ID that was selected. If multiple entities
    // are selected or if no entity is selected this will be `null`.
    var currentSelectedEntityID = null;
    var statusMonitor = null;
    var blockPropertyUpdates = false;

    that.setVisible = function (newVisible) {
        visible = newVisible;
        webView.setVisible(shouldUseEditTabletApp() && visible);
        createToolsWindow.setVisible(!shouldUseEditTabletApp() && visible);
    };

    that.setVisible(false);

    function emitScriptEvent(data) {
        var dataString = JSON.stringify(data);
        webView.emitScriptEvent(dataString);
        createToolsWindow.emitScriptEvent(dataString);
    }

    function updateScriptStatus(info) {
        info.type = "server_script_status";
        emitScriptEvent(info);
    }

    function resetScriptStatus() {
        updateScriptStatus({
            statusRetrieved: undefined,
            isRunning: undefined,
            status: "",
            errorInfo: ""
        });
    }

    that.setSpaceMode = function(spaceMode) {
        emitScriptEvent({
            type: 'setSpaceMode',
            spaceMode: spaceMode
        })
    };

    function updateSelections(selectionUpdated, caller) {
        if (HMD.active && visible) {
            webView.setLandscape(true);
        } else {
            if (!visible) {
                hmdMultiSelectMode = false;
                webView.setLandscape(false);
            }
        }
        
        if (blockPropertyUpdates) {
            return;
        }

        var data = {
            type: 'update',
            spaceMode: selectionDisplay.getSpaceMode(),
            isPropertiesToolUpdate: caller === this,
        };

        if (selectionUpdated) {
            resetScriptStatus();

            if (selectionManager.selections.length !== 1) {
                if (statusMonitor !== null) {
                    statusMonitor.stop();
                    statusMonitor = null;
                }
                currentSelectedEntityID = null;
            } else if (currentSelectedEntityID !== selectionManager.selections[0]) {
                if (statusMonitor !== null) {
                    statusMonitor.stop();
                }
                var entityID = selectionManager.selections[0];
                currentSelectedEntityID = entityID;
                statusMonitor = new ServerScriptStatusMonitor(entityID, updateScriptStatus);
            }
        }

        var selections = [];
        for (var i = 0; i < selectionManager.selections.length; i++) {
            var entity = {};
            entity.id = selectionManager.selections[i];
            entity.properties = Entities.getEntityProperties(selectionManager.selections[i]);
            if (entity.properties.rotation !== undefined) {
                entity.properties.rotation = Quat.safeEulerAngles(entity.properties.rotation);
            }
            if (entity.properties.localRotation !== undefined) {
                entity.properties.localRotation = Quat.safeEulerAngles(entity.properties.localRotation);
            }
            if (entity.properties.emitOrientation !== undefined) {
                entity.properties.emitOrientation = Quat.safeEulerAngles(entity.properties.emitOrientation);
            }
            if (entity.properties.keyLight !== undefined && entity.properties.keyLight.direction !== undefined) {
                entity.properties.keyLight.direction = Vec3.toPolar(entity.properties.keyLight.direction);
                entity.properties.keyLight.direction.z = 0.0;
            }
            selections.push(entity);
        }
        data.selections = selections;

        emitScriptEvent(data);
    }
    selectionManager.addEventListener(updateSelections, this);


    var onWebEventReceived = function(data) {
        try {
            data = JSON.parse(data);
        } catch(e) {
            return;
        }
        var i, properties, dY, diff, newPosition;
        if (data.type === "update") {

            if (data.properties || data.propertiesMap) {
                var propertiesMap = data.propertiesMap;
                if (propertiesMap === undefined) {
                    propertiesMap = [{
                        entityIDs: data.ids,
                        properties: data.properties,
                    }];
                }

                var sendListUpdate = false;
                propertiesMap.forEach(function(propertiesObject) {
                    var properties = propertiesObject.properties;
                    var updateEntityIDs = propertiesObject.entityIDs;
                    if (properties.dynamic === false) {
                        // this object is leaving dynamic, so we zero its velocities
                        properties.localVelocity = Vec3.ZERO;
                        properties.localAngularVelocity = Vec3.ZERO;
                    }
                    if (properties.rotation !== undefined) {
                        properties.rotation = Quat.fromVec3Degrees(properties.rotation);
                    }
                    if (properties.localRotation !== undefined) {
                        properties.localRotation = Quat.fromVec3Degrees(properties.localRotation);
                    }
                    if (properties.emitOrientation !== undefined) {
                        properties.emitOrientation = Quat.fromVec3Degrees(properties.emitOrientation);
                    }
                    if (properties.keyLight !== undefined && properties.keyLight.direction !== undefined) {
                        var currentKeyLightDirection = Vec3.toPolar(Entities.getEntityProperties(selectionManager.selections[0], ['keyLight.direction']).keyLight.direction);
                        if (properties.keyLight.direction.x === undefined) {
                            properties.keyLight.direction.x = currentKeyLightDirection.x;
                        }
                        if (properties.keyLight.direction.y === undefined) {
                            properties.keyLight.direction.y = currentKeyLightDirection.y;
                        }
                        properties.keyLight.direction = Vec3.fromPolar(properties.keyLight.direction.x, properties.keyLight.direction.y);
                    }

                    updateEntityIDs.forEach(function (entityID) {
                        Entities.editEntity(entityID, properties);
                    });

                    if (properties.name !== undefined || properties.modelURL !== undefined || properties.imageURL !== undefined ||
                        properties.materialURL !== undefined || properties.visible !== undefined || properties.locked !== undefined) {

                        sendListUpdate = true;
                    }

                });
                if (sendListUpdate) {
                    entityListTool.sendUpdate();
                }
            }

            if (data.onlyUpdateEntities) {
                blockPropertyUpdates = true;
            } else {
                pushCommandForSelections();
                SelectionManager.saveProperties();
            }
            selectionManager._update(false, this);
            blockPropertyUpdates = false;
            
            if (data.snapToGrid !== undefined) {
                entityListTool.setListMenuSnapToGrid(data.snapToGrid);
            }
        } else if (data.type === 'saveUserData' || data.type === 'saveMaterialData') {
            data.ids.forEach(function(entityID) {
                Entities.editEntity(entityID, data.properties);
            });
        } else if (data.type === "action") {
            if (data.action === "moveSelectionToGrid") {
                if (selectionManager.hasSelection()) {
                    selectionManager.saveProperties();
                    dY = grid.getOrigin().y - (selectionManager.worldPosition.y - selectionManager.worldDimensions.y / 2);
                    diff = {
                        x: 0,
                        y: dY,
                        z: 0
                    };
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        properties = selectionManager.savedProperties[selectionManager.selections[i]];
                        newPosition = Vec3.sum(properties.position, diff);
                        Entities.editEntity(selectionManager.selections[i], {
                            position: newPosition
                        });
                    }
                    pushCommandForSelections();
                    selectionManager._update(false, this);
                }
            } else if (data.action === "moveAllToGrid") {
                if (selectionManager.hasSelection()) {
                    selectionManager.saveProperties();
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        properties = selectionManager.savedProperties[selectionManager.selections[i]];
                        var bottomY = properties.boundingBox.center.y - properties.boundingBox.dimensions.y / 2;
                        dY = grid.getOrigin().y - bottomY;
                        diff = {
                            x: 0,
                            y: dY,
                            z: 0
                        };
                        newPosition = Vec3.sum(properties.position, diff);
                        Entities.editEntity(selectionManager.selections[i], {
                            position: newPosition
                        });
                    }
                    pushCommandForSelections();
                    selectionManager._update(false, this);
                }
            } else if (data.action === "resetToNaturalDimensions") {
                if (selectionManager.hasSelection()) {
                    selectionManager.saveProperties();
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        properties = selectionManager.savedProperties[selectionManager.selections[i]];
                        var naturalDimensions = properties.naturalDimensions;

                        // If any of the natural dimensions are not 0, resize
                        if (properties.type === "Model" && naturalDimensions.x === 0 && naturalDimensions.y === 0 &&
                            naturalDimensions.z === 0) {
                            Window.notifyEditError("Cannot reset entity to its natural dimensions: Model URL" +
                                " is invalid or the model has not yet been loaded.");
                        } else {
                            Entities.editEntity(selectionManager.selections[i], {
                                dimensions: properties.naturalDimensions
                            });
                        }
                    }
                    pushCommandForSelections();
                    selectionManager._update(false, this);
                }
            } else if (data.action === "previewCamera") {
                if (selectionManager.hasSelection()) {
                    Camera.mode = "entity";
                    Camera.cameraEntity = selectionManager.selections[0];
                }
            } else if (data.action === "rescaleDimensions") {
                var multiplier = data.percentage / 100.0;
                if (selectionManager.hasSelection()) {
                    selectionManager.saveProperties();
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        properties = selectionManager.savedProperties[selectionManager.selections[i]];
                        Entities.editEntity(selectionManager.selections[i], {
                            dimensions: Vec3.multiply(multiplier, properties.dimensions)
                        });
                    }
                    pushCommandForSelections();
                    selectionManager._update(false, this);
                }
            } else if (data.action === "reloadClientScripts") {
                if (selectionManager.hasSelection()) {
                    var timestamp = Date.now();
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        Entities.editEntity(selectionManager.selections[i], {
                            scriptTimestamp: timestamp
                        });
                    }
                }
            } else if (data.action === "reloadServerScripts") {
                if (selectionManager.hasSelection()) {
                    for (i = 0; i < selectionManager.selections.length; i++) {
                        Entities.reloadServerScripts(selectionManager.selections[i]);
                    }
                }
            }
        } else if (data.type === "propertiesPageReady") {
            updateSelections(true);
        } else if (data.type === "tooltipsRequest") {
            emitScriptEvent({
                type: 'tooltipsReply',
                tooltips: Script.require('./assets/data/createAppTooltips.json'),
                hmdActive: HMD.active,
            });
        } else if (data.type === "propertyRangeRequest") {
            var propertyRanges = {};
            data.properties.forEach(function (property) {
                propertyRanges[property] = Entities.getPropertyInfo(property);
            });
            emitScriptEvent({
                type: 'propertyRangeReply',
                propertyRanges: propertyRanges,
            });
        } else if (data.type === "materialTargetRequest") {
            var parentModelData;
            var properties = Entities.getEntityProperties(data.entityID, ["type", "parentID"]);
            if (properties.type === "Material" && properties.parentID !== Uuid.NULL) {
                var parentType = Entities.getEntityProperties(properties.parentID, ["type"]).type;
                if (parentType === "Model" || Entities.getNestableType(properties.parentID) === "avatar") {
                    parentModelData = Graphics.getModel(properties.parentID);
                } else if (parentType === "Shape" || parentType === "Box" || parentType === "Sphere") {
                    parentModelData = {};
                    parentModelData.numMeshes = 1;
                    parentModelData.materialNames = [];
                }
            }
            emitScriptEvent({
                type: 'materialTargetReply',
                entityID: data.entityID,
                materialTargetData: parentModelData,
            });
        } else if (data.type === "zoneListRequest") {
            emitScriptEvent({
                type: 'zoneListRequest',
                zones: getExistingZoneList()
            });
        }
    };

    HMD.displayModeChanged.connect(function() {
        emitScriptEvent({
            type: 'hmdActiveChanged',
            hmdActive: HMD.active,
        });
    });

    createToolsWindow.webEventReceived.addListener(this, onWebEventReceived);

    webView.webEventReceived.connect(this, onWebEventReceived);

    return that;
};


var PopupMenu = function () {
    var self = this;

    var MENU_ITEM_HEIGHT = 21;
    var MENU_ITEM_SPACING = 1;
    var TEXT_MARGIN = 7;

    var overlays = [];
    var overlayInfo = {};

    var visible = false;

    var upColor = {
        red: 0,
        green: 0,
        blue: 0
    };
    var downColor = {
        red: 192,
        green: 192,
        blue: 192
    };
    var overColor = {
        red: 128,
        green: 128,
        blue: 128
    };

    self.onSelectMenuItem = function () {};

    self.addMenuItem = function (name) {
        var id = Overlays.addOverlay("text", {
            text: name,
            backgroundAlpha: 1.0,
            backgroundColor: upColor,
            topMargin: TEXT_MARGIN,
            leftMargin: TEXT_MARGIN,
            width: 210,
            height: MENU_ITEM_HEIGHT,
            font: {
                size: 12
            },
            visible: false
        });
        overlays.push(id);
        overlayInfo[id] = {
            name: name
        };
        return id;
    };

    self.updateMenuItemText = function (id, newText) {
        Overlays.editOverlay(id, {
            text: newText
        });
    };

    self.setPosition = function (x, y) {
        for (var key in overlayInfo) {
            Overlays.editOverlay(key, {
                x: x,
                y: y
            });
            y += MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
        }
    };

    self.onSelected = function () {};

    var pressingOverlay = null;
    var hoveringOverlay = null;

    self.mousePressEvent = function (event) {
        if (event.isLeftButton) {
            var overlay = Overlays.getOverlayAtPoint({
                x: event.x,
                y: event.y
            });
            if (overlay in overlayInfo) {
                pressingOverlay = overlay;
                Overlays.editOverlay(pressingOverlay, {
                    backgroundColor: downColor
                });
            } else {
                self.hide();
            }
            return false;
        }
    };
    self.mouseMoveEvent = function (event) {
        if (visible) {
            var overlay = Overlays.getOverlayAtPoint({
                x: event.x,
                y: event.y
            });
            if (!pressingOverlay) {
                if (hoveringOverlay !== null && overlay !== hoveringOverlay) {
                    Overlays.editOverlay(hoveringOverlay, {
                        backgroundColor: upColor
                    });
                    hoveringOverlay = null;
                }
                if (overlay !== hoveringOverlay && overlay in overlayInfo) {
                    Overlays.editOverlay(overlay, {
                        backgroundColor: overColor
                    });
                    hoveringOverlay = overlay;
                }
            }
        }
        return false;
    };
    self.mouseReleaseEvent = function (event) {
        var overlay = Overlays.getOverlayAtPoint({
            x: event.x,
            y: event.y
        });
        if (pressingOverlay !== null && pressingOverlay !== undefined) {
            if (overlay === pressingOverlay) {
                self.onSelectMenuItem(overlayInfo[overlay].name);
            }
            Overlays.editOverlay(pressingOverlay, {
                backgroundColor: upColor
            });
            pressingOverlay = null;
            self.hide();
        }
    };

    self.setVisible = function (newVisible) {
        if (newVisible !== visible) {
            visible = newVisible;
            for (var key in overlayInfo) {
                Overlays.editOverlay(key, {
                    visible: newVisible
                });
            }
        }
    };
    self.show = function () {
        self.setVisible(true);
    };
    self.hide = function () {
        self.setVisible(false);
    };

    function cleanup() {
        for (var i = 0; i < overlays.length; i++) {
            Overlays.deleteOverlay(overlays[i]);
        }
        Controller.mousePressEvent.disconnect(self.mousePressEvent);
        Controller.mouseMoveEvent.disconnect(self.mouseMoveEvent);
        Controller.mouseReleaseEvent.disconnect(self.mouseReleaseEvent);

        Entities.canRezChanged.disconnect(checkEditPermissionsAndUpdate);
        Entities.canRezTmpChanged.disconnect(checkEditPermissionsAndUpdate);
    }

    Controller.mousePressEvent.connect(self.mousePressEvent);
    Controller.mouseMoveEvent.connect(self.mouseMoveEvent);
    Controller.mouseReleaseEvent.connect(self.mouseReleaseEvent);
    Script.scriptEnding.connect(cleanup);

    return this;
};

function whenPressed(fn) {
    return function(value) {
        if (value > 0) {
            fn();
        }
    };
}

function whenReleased(fn) {
    return function(value) {
        if (value === 0) {
            fn();
        }
    };
}

var isOnMacPlatform = Controller.getValue(Controller.Hardware.Application.PlatformMac);

var mapping = Controller.newMapping(CONTROLLER_MAPPING_NAME);
if (isOnMacPlatform) {
    mapping.from([Controller.Hardware.Keyboard.Backspace]).to(deleteKey);
} else {
    mapping.from([Controller.Hardware.Keyboard.Delete]).to(deleteKey);
}
mapping.from([Controller.Hardware.Keyboard.T]).to(toggleKey);
mapping.from([Controller.Hardware.Keyboard.F]).to(focusKey);
mapping.from([Controller.Hardware.Keyboard.J]).to(gridKey);
mapping.from([Controller.Hardware.Keyboard.G]).to(viewGridKey);
mapping.from([Controller.Hardware.Keyboard.H]).to(snapKey);
mapping.from([Controller.Hardware.Keyboard.K]).to(gridToAvatarKey);
mapping.from([Controller.Hardware.Keyboard["0"]]).to(rotateAsNextClickedSurfaceKey);
mapping.from([Controller.Hardware.Keyboard["7"]]).to(quickRotate90xKey);
mapping.from([Controller.Hardware.Keyboard["8"]]).to(quickRotate90yKey);
mapping.from([Controller.Hardware.Keyboard["9"]]).to(quickRotate90zKey);
mapping.from([Controller.Hardware.Keyboard.X])
    .when([Controller.Hardware.Keyboard.Control])
    .to(whenReleased(function() { selectionManager.cutSelectedEntities() }));
mapping.from([Controller.Hardware.Keyboard.C])
    .when([Controller.Hardware.Keyboard.Control])
    .to(whenReleased(function() { selectionManager.copySelectedEntities() }));
mapping.from([Controller.Hardware.Keyboard.V])
    .when([Controller.Hardware.Keyboard.Control])
    .to(whenReleased(function() { selectionManager.pasteEntities() }));
mapping.from([Controller.Hardware.Keyboard.D])
    .when([Controller.Hardware.Keyboard.Control])
    .to(whenReleased(function() { selectionManager.duplicateSelection() }));

// Bind undo to ctrl-shift-z to maintain backwards-compatibility
mapping.from([Controller.Hardware.Keyboard.Z])
    .when([Controller.Hardware.Keyboard.Control, Controller.Hardware.Keyboard.Shift])
    .to(whenPressed(function() { undoHistory.redo() }));


mapping.from([Controller.Hardware.Keyboard.P])
    .when([Controller.Hardware.Keyboard.Control, Controller.Hardware.Keyboard.Shift])
    .to(whenReleased(function() { unparentSelectedEntities(); }));

mapping.from([Controller.Hardware.Keyboard.P])
    .when([Controller.Hardware.Keyboard.Control, !Controller.Hardware.Keyboard.Shift])
    .to(whenReleased(function() { parentSelectedEntities(); }));

keyUpEventFromUIWindow = function(keyUpEvent) {
    var WANT_DEBUG_MISSING_SHORTCUTS = false;

    var pressedValue = 0.0;

    if ((!isOnMacPlatform && keyUpEvent.keyCodeString === "Delete")
        || (isOnMacPlatform && keyUpEvent.keyCodeString === "Backspace")) {

        deleteKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "T") {
        toggleKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "F") {
        focusKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "J") {
        gridKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "G") {
        viewGridKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "H") {
        snapKey(pressedValue);    
    } else if (keyUpEvent.keyCodeString === "K") {
        gridToAvatarKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "0") {
        rotateAsNextClickedSurfaceKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "7") {
        quickRotate90xKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "8") {
        quickRotate90yKey(pressedValue);
    } else if (keyUpEvent.keyCodeString === "9") {
        quickRotate90zKey(pressedValue);        
    } else if (keyUpEvent.controlKey && keyUpEvent.keyCodeString === "X") {
        selectionManager.cutSelectedEntities();
    } else if (keyUpEvent.controlKey && keyUpEvent.keyCodeString === "C") {
        selectionManager.copySelectedEntities();
    } else if (keyUpEvent.controlKey && keyUpEvent.keyCodeString === "V") {
        selectionManager.pasteEntities();
    } else if (keyUpEvent.controlKey && keyUpEvent.keyCodeString === "D") {
        selectionManager.duplicateSelection();
    } else if (!isOnMacPlatform && keyUpEvent.controlKey && !keyUpEvent.shiftKey && keyUpEvent.keyCodeString === "Z") {
        undoHistory.undo(); // undo is only handled via handleMenuItem on Mac
    } else if (keyUpEvent.controlKey && !keyUpEvent.shiftKey && keyUpEvent.keyCodeString === "P") {
        parentSelectedEntities();
    } else if (keyUpEvent.controlKey && keyUpEvent.shiftKey && keyUpEvent.keyCodeString === "P") {
        unparentSelectedEntities();
    } else if (!isOnMacPlatform &&
              ((keyUpEvent.controlKey && keyUpEvent.shiftKey && keyUpEvent.keyCodeString === "Z") ||
               (keyUpEvent.controlKey && keyUpEvent.keyCodeString === "Y"))) {
        undoHistory.redo(); // redo is only handled via handleMenuItem on Mac
    } else if (WANT_DEBUG_MISSING_SHORTCUTS) {
        console.warn("unhandled key event: " + JSON.stringify(keyUpEvent))
    }
};

var propertyMenu = new PopupMenu();

propertyMenu.onSelectMenuItem = function (name) {
};

var propertiesTool = new PropertiesTool();

selectionDisplay.onSpaceModeChange = function(spaceMode) {
    entityListTool.setSpaceMode(spaceMode);
    propertiesTool.setSpaceMode(spaceMode);
};

function getExistingZoneList() {
    var center = { "x": 0, "y": 0, "z": 0 };
    var existingZoneIDs = Entities.findEntitiesByType("Zone", center, ENTIRE_DOMAIN_SCAN_RADIUS);
    var listExistingZones = [];
    var thisZone = {};
    var properties;
    for (var k = 0; k < existingZoneIDs.length; k++) {
        properties = Entities.getEntityProperties(existingZoneIDs[k], ["name"]);
        thisZone = {
            "id": existingZoneIDs[k],
            "name": properties.name
        };
        listExistingZones.push(thisZone);
    }
    listExistingZones.sort(zoneSortOrder);
    return listExistingZones;
}

function zoneSortOrder(a, b) {
    var nameA = a.name.toUpperCase();
    var nameB = b.name.toUpperCase();
    if (nameA > nameB) {
        return 1;    
    } else if (nameA < nameB) {
        return -1;
    }
    if (a.name > b.name) {
        return 1;    
    } else if (a.name < b.name) {
        return -1;
    }
    return 0;
}

function getParentState(id) {
    var state = "NONE";
    var properties = Entities.getEntityProperties(id, ["parentID"]);
    var children = getDomainOnlyChildrenIDs(id);
    if (properties.parentID !== Uuid.NULL) {
        if (children.length > 0) {
            state = "PARENT_CHILDREN";
        } else {
            state = "CHILDREN";
        }
    } else {
        if (children.length > 0) {
            state = "PARENT";
        }
    }
    return state;
}

function getDomainOnlyChildrenIDs(id) {
    var allChildren = Entities.getChildrenIDs(id);
    var realChildren = [];
    var properties;
    for (var i = 0; i < allChildren.length; i++) {
        properties = Entities.getEntityProperties(allChildren[i], ["name"]);
        if (properties.name !== undefined && properties.name !== entityShapeVisualizerSessionName) {
            realChildren.push(allChildren[i]);
        }
    }
    return realChildren;
}

function importEntitiesFromFile() {
    Window.browseChanged.connect(onFileOpenChanged);
    Window.browseAsync("Select .json to Import", "", "*.json");    
}

function importEntitiesFromUrl() {
    Window.promptTextChanged.connect(onPromptTextChanged);
    Window.promptAsync("URL of a .json to import", "");    
}

function setCameraFocusToSelection() {
    cameraManager.enable();
    if (selectionManager.hasSelection()) {
        cameraManager.focus(selectionManager.worldPosition, selectionManager.worldDimensions,
                            Menu.isOptionChecked(MENU_EASE_ON_FOCUS));
    }
}

function alignGridToSelection() {
    if (selectionManager.hasSelection()) {
        if (!grid.getVisible()) {
            grid.setVisible(true, true);
        }
        grid.moveToSelection();
    }
}

function alignGridToAvatar() {
    if (!grid.getVisible()) {
        grid.setVisible(true, true);
    }
    grid.moveToAvatar();
}

function toggleGridVisibility() {
    if (!grid.getVisible()) {
        grid.setVisible(true, true);
    } else {
        grid.setVisible(false, true);
    }
}

function rotateAsNextClickedSurface() {
    if (!SelectionManager.hasSelection() || !SelectionManager.hasUnlockedSelection()) {
        audioFeedback.rejection();
        Window.notifyEditError("You have nothing selected, or the selection is locked.");
        expectingRotateAsClickedSurface = false;
    } else {
        expectingRotateAsClickedSurface = true;
    }
}

}()); // END LOCAL_SCOPE
