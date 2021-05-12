//  entityProperties.js
//
//  Created by Ryan Huffman on 13 Nov 2014
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

/* global alert, augmentSpinButtons, clearTimeout, console, document, Element, 
   EventBridge, JSONEditor, openEventBridge, setTimeout, window, _, $ */

var currentTab = "base";

const DEGREES_TO_RADIANS = Math.PI / 180.0;

const NO_SELECTION = ",";

const PROPERTY_SPACE_MODE = Object.freeze({
    ALL: 0,
    LOCAL: 1,
    WORLD: 2
});

const PROPERTY_SELECTION_VISIBILITY = Object.freeze({
    SINGLE_SELECTION: 1,
    MULTIPLE_SELECTIONS: 2,
    MULTI_DIFF_SELECTIONS: 4,
    ANY_SELECTIONS: 7 /* SINGLE_SELECTION | MULTIPLE_SELECTIONS | MULTI_DIFF_SELECTIONS */
});

// Multiple-selection behavior
const PROPERTY_MULTI_DISPLAY_MODE = Object.freeze({
    DEFAULT: 0,
    /**
     * Comma separated values
     * Limited for properties with type "string" or "textarea" and readOnly enabled
     */
    COMMA_SEPARATED_VALUES: 1
});

const GROUPS = [
    {
        id: "base",
        label: "ENTITY",
        properties: [
            {
                label: NO_SELECTION,
                type: "icon",
                icons: ENTITY_TYPE_ICON,
                propertyID: "type",
                replaceID: "placeholder-property-type",
            },
            {
                label: "Name",
                type: "string",
                propertyID: "name",
                placeholder: "Name",
                replaceID: "placeholder-property-name",
            },
            {
                label: "ID",
                type: "string",
                propertyID: "id",
                placeholder: "ID",
                readOnly: true,
                replaceID: "placeholder-property-id",
                multiDisplayMode: PROPERTY_MULTI_DISPLAY_MODE.COMMA_SEPARATED_VALUES,
            },
            {
                label: "Description",
                type: "string",
                propertyID: "description",
            },
            {
                label: "Parent",
                type: "string",
                propertyID: "parentID",
                onChange: parentIDChanged,
            },
            {
                label: "Parent Joint Index",
                type: "number",
                propertyID: "parentJointIndex",
            },
            {
                label: "",
                glyph: "&#xe006;",
                type: "bool",
                propertyID: "locked",
                replaceID: "placeholder-property-locked",
            },
            {
                label: "",
                glyph: "&#xe007;",
                type: "bool",
                propertyID: "visible",
                replaceID: "placeholder-property-visible",
            },
            {
                label: "Render Layer",
                type: "dropdown",
                options: {
                    world: "World",
                    front: "Front",
                    hud: "HUD"
                },
                propertyID: "renderLayer",
            },
            {
                label: "Primitive Mode",
                type: "dropdown",
                options: {
                    solid: "Solid",
                    lines: "Wireframe",
                },
                propertyID: "primitiveMode",
            },
            {
                label: "Billboard Mode",
                type: "dropdown",
                options: {
                    none: "None",
                    yaw: "Yaw",
                    full: "Full"
                },
                propertyID: "billboardMode",
            },
            {
                label: "Render With Zones",
                type: "multipleZonesSelection",
                propertyID: "renderWithZones",
            }
        ]
    },
    {
        id: "shape",
        label: "SHAPE",        
        properties: [
            {
                label: "Shape",
                type: "dropdown",
                options: { Cube: "Box", Sphere: "Sphere", Tetrahedron: "Tetrahedron", Octahedron: "Octahedron", 
                           Icosahedron: "Icosahedron", Dodecahedron: "Dodecahedron", Hexagon: "Hexagon", 
                           Triangle: "Triangle", Octagon: "Octagon", Cylinder: "Cylinder", Cone: "Cone", 
                           Circle: "Circle", Quad: "Quad" },
                propertyID: "shape",
            },
            {
                label: "Color",
                type: "color",
                propertyID: "color",
            },
            {
                label: "Alpha",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.01,
                decimals: 2,
                propertyID: "shapeAlpha",
                propertyName: "alpha",
            },            
        ]
    },
    {
        id: "text",
        label: "TEXT",
        properties: [
            {
                label: "Text",
                type: "string",
                propertyID: "text",
            },
            {
                label: "Text Color",
                type: "color",
                propertyID: "textColor",
            },
            {
                label: "Text Alpha",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.01,
                decimals: 2,
                propertyID: "textAlpha",
            },
            {
                label: "Background Color",
                type: "color",
                propertyID: "backgroundColor",
            },
            {
                label: "Background Alpha",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.01,
                decimals: 2,
                propertyID: "backgroundAlpha",
            },
            {
                label: "Line Height",
                type: "number-draggable",
                min: 0,
                step: 0.001,
                decimals: 4,
                unit: "m",
                propertyID: "lineHeight",
            },
            {
                label: "Font",
                type: "string",
                propertyID: "font",
            },
            {
                label: "Effect",
                type: "dropdown",
                options: {
                    none: "None",
                    outline: "Outline",
                    "outline fill": "Outline with fill",
                    shadow: "Shadow"
                },
                propertyID: "textEffect",
            },
            {
                label: "Effect Color",
                type: "color",
                propertyID: "textEffectColor",
            },
            {
                label: "Effect Thickness",
                type: "number-draggable",
                min: 0.0,
                max: 0.5,
                step: 0.01,
                decimals: 2,
                propertyID: "textEffectThickness",
            },
            {
                label: "Alignment",
                type: "dropdown",
                options: {
                    left: "Left",
                    center: "Center",
                    right: "Right"
                },
                propertyID: "textAlignment",
                propertyName: "alignment", // actual entity property name
            },
            {
                label: "Top Margin",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "topMargin",
            },
            {
                label: "Right Margin",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "rightMargin",
            },
            {
                label: "Bottom Margin",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "bottomMargin",
            },
            {
                label: "Left Margin",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "leftMargin",
            },
            {
                label: "Unlit",
                type: "bool",
                propertyID: "unlit",
            }
        ]
    },
    {
        id: "zone",
        label: "ZONE",
        properties: [
            {
                label: "Shape Type",
                type: "dropdown",
                options: { "box": "Box", "sphere": "Sphere",
                           "cylinder-y": "Cylinder", "compound": "Use Compound Shape URL" },
                propertyID: "zoneShapeType",
                propertyName: "shapeType", // actual entity property name
            },
            {
                label: "Compound Shape URL",
                type: "string",
                propertyID: "zoneCompoundShapeURL",
                propertyName: "compoundShapeURL", // actual entity property name
            },
            {
                label: "Flying Allowed",
                type: "bool",
                propertyID: "flyingAllowed",
            },
            {
                label: "Ghosting Allowed",
                type: "bool",
                propertyID: "ghostingAllowed",
            },
            {
                label: "Filter",
                type: "string",
                propertyID: "filterURL",
            }
        ]
    },
    {
        id: "zone_key_light",
        label: "ZONE KEY LIGHT",
        properties: [
            {
                label: "Key Light",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "keyLightMode",
                
            },
            {
                label: "Key Light Color",
                type: "color",
                propertyID: "keyLight.color",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Light Intensity",
                type: "number-draggable",
                min: -40,
                max: 40,
                step: 0.01,
                decimals: 2,
                propertyID: "keyLight.intensity",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Light Horizontal Angle",
                type: "number-draggable",
                step: 0.1,
                multiplier: DEGREES_TO_RADIANS,
                decimals: 2,
                unit: "deg",
                propertyID: "keyLight.direction.y",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Light Vertical Angle",
                type: "number-draggable",
                step: 0.1,
                multiplier: DEGREES_TO_RADIANS,
                decimals: 2,
                unit: "deg",
                propertyID: "keyLight.direction.x",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Cast Shadows",
                type: "bool",
                propertyID: "keyLight.castShadows",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Shadow Bias",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.01,
                decimals: 2,
                propertyID: "keyLight.shadowBias",
                showPropertyRule: { "keyLightMode": "enabled" },
            },
            {
                label: "Shadow Max Distance",
                type: "number-draggable",
                min: 0,
                max: 250,
                step: 0.1,
                decimals: 2,
                propertyID: "keyLight.shadowMaxDistance",
                showPropertyRule: { "keyLightMode": "enabled" },
            }    
        ]
    },    
    {
        id: "zone_skybox",
        label: "ZONE SKYBOX",
        properties: [
            {
                label: "Skybox",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "skyboxMode",
            },
            {
                label: "Skybox Color",
                type: "color",
                propertyID: "skybox.color",
                showPropertyRule: { "skyboxMode": "enabled" },
            },
            {
                label: "Skybox Source",
                type: "string",
                propertyID: "skybox.url",
                showPropertyRule: { "skyboxMode": "enabled" },
            }
        ]
    },
    {
        id: "zone_ambient_light",
        label: "ZONE AMBIENT LIGHT",
        properties: [
            {
                label: "Ambient Light",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "ambientLightMode",
            },
            {
                label: "Ambient Intensity",
                type: "number-draggable",
                min: -200,
                max: 200,
                step: 0.1,
                decimals: 2,
                propertyID: "ambientLight.ambientIntensity",
                showPropertyRule: { "ambientLightMode": "enabled" },
            },
            {
                label: "Ambient Source",
                type: "string",
                propertyID: "ambientLight.ambientURL",
                showPropertyRule: { "ambientLightMode": "enabled" },
            },
            {
                type: "buttons",
                buttons: [ { id: "copy", label: "Copy from Skybox", 
                             className: "black", onClick: copySkyboxURLToAmbientURL } ],
                propertyID: "copyURLToAmbient",
                showPropertyRule: { "ambientLightMode": "enabled" },
            }
        ]
    },
    {
        id: "zone_haze",
        label: "ZONE HAZE",
        properties: [
            {
                label: "Haze",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "hazeMode",
            },
            {
                label: "Range",
                type: "number-draggable",
                min: 1,
                max: 10000,
                step: 1,
                decimals: 0,
                unit: "m",
                propertyID: "haze.hazeRange",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Use Altitude",
                type: "bool",
                propertyID: "haze.hazeAltitudeEffect",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Base",
                type: "number-draggable",
                min: -16000,
                max: 16000,
                step: 1,
                decimals: 0,
                unit: "m",
                propertyID: "haze.hazeBaseRef",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Ceiling",
                type: "number-draggable",
                min: -16000,
                max: 16000,
                step: 1,
                decimals: 0,
                unit: "m",
                propertyID: "haze.hazeCeiling",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Haze Color",
                type: "color",
                propertyID: "haze.hazeColor",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Background Blend",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.001,
                decimals: 3,
                propertyID: "haze.hazeBackgroundBlend",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Enable Glare",
                type: "bool",
                propertyID: "haze.hazeEnableGlare",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Glare Color",
                type: "color",
                propertyID: "haze.hazeGlareColor",
                showPropertyRule: { "hazeMode": "enabled" },
            },
            {
                label: "Glare Angle",
                type: "number-draggable",
                min: 0,
                max: 180,
                step: 1,
                decimals: 0,
                propertyID: "haze.hazeGlareAngle",
                showPropertyRule: { "hazeMode": "enabled" },
            }
        ]
    },
    {
        id: "zone_bloom",
        label: "ZONE BLOOM",
        properties: [
            {
                label: "Bloom",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "bloomMode",
            },
            {
                label: "Bloom Intensity",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.001,
                decimals: 3,
                propertyID: "bloom.bloomIntensity",
                showPropertyRule: { "bloomMode": "enabled" },
            },
            {
                label: "Bloom Threshold",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.001,
                decimals: 3,
                propertyID: "bloom.bloomThreshold",
                showPropertyRule: { "bloomMode": "enabled" },
            },
            {
                label: "Bloom Size",
                type: "number-draggable",
                min: 0,
                max: 2,
                step: 0.001,
                decimals: 3,
                propertyID: "bloom.bloomSize",
                showPropertyRule: { "bloomMode": "enabled" },
            }
        ]
    },
    {
        id: "zone_avatar_priority",
        label: "ZONE AVATAR PRIORITY",
        properties: [
            {
                label: "Avatar Priority",
                type: "dropdown",
                options: { inherit: "Inherit", crowd: "Crowd", hero: "Hero" },
                propertyID: "avatarPriority",
            },
            {
                label: "Screen-share",
                type: "dropdown",
                options: { inherit: "Inherit", disabled: "Off", enabled: "On" },
                propertyID: "screenshare",
            }
        ]
    },
    {
        id: "model",
        label: "MODEL",
        properties: [
            {
                label: "Model",
                type: "string",
                placeholder: "URL",
                propertyID: "modelURL",
                buttons: [ { id: "reload", label: "F", className: "glyph", onClick: reloadModelURL } ],
                hideIfCertified: true,
            },
            {
                label: "Collision Shape",
                type: "dropdown",
                options: { "none": "No Collision", "box": "Box", "sphere": "Sphere", "compound": "Compound" , 
                           "simple-hull": "Basic - Whole model", "simple-compound": "Good - Sub-meshes" , 
                           "static-mesh": "Exact - All polygons (non-dynamic only)" },
                propertyID: "shapeType",
            },
            {
                label: "Compound Shape",
                type: "string",
                propertyID: "compoundShapeURL",
                hideIfCertified: true,
            },
            {
                label: "Use Original Pivot",
                type: "bool",
                propertyID: "useOriginalPivot",
            },
            {
                label: "Animation",
                type: "string",
                propertyID: "animation.url",
                hideIfCertified: true,
            },
            {
                label: "Play Automatically",
                type: "bool",
                propertyID: "animation.running",
            },
            {
                label: "Loop",
                type: "bool",
                propertyID: "animation.loop",
            },
            {
                label: "Allow Translation",
                type: "bool",
                propertyID: "animation.allowTranslation",
            },
            {
                label: "Hold",
                type: "bool",
                propertyID: "animation.hold",
            },
            {
                label: "Animation Frame",
                type: "number-draggable",
                propertyID: "animation.currentFrame",
            },
            {
                label: "First Frame",
                type: "number-draggable",
                propertyID: "animation.firstFrame",
            },
            {
                label: "Last Frame",
                type: "number-draggable",
                propertyID: "animation.lastFrame",
            },
            {
                label: "Animation FPS",
                type: "number-draggable",
                propertyID: "animation.fps",
            },
            {
                label: "Texture",
                type: "textarea",
                propertyID: "textures",
            },
            {
                label: "Original Texture",
                type: "textarea",
                propertyID: "originalTextures",
                readOnly: true,
                hideIfCertified: true,
            },
            {
                label: "Group Culled",
                type: "bool",
                propertyID: "groupCulled",
            }
        ]
    },
    {
        id: "image",
        label: "IMAGE",
        properties: [
            {
                label: "Image",
                type: "string",
                placeholder: "URL",
                propertyID: "imageURL",
            },
            {
                label: "Color",
                type: "color",
                propertyID: "imageColor",
                propertyName: "color", // actual entity property name
            },
            {
                label: "Alpha",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.01,
                decimals: 2,
                propertyID: "imageAlpha",
                propertyName: "alpha",
            },            
            {
                label: "Emissive",
                type: "bool",
                propertyID: "emissive",
            },
            {
                label: "Sub Image",
                type: "rect",
                min: 0,
                step: 1,
                subLabels: [ "x", "y", "w", "h" ],
                propertyID: "subImage",
            },
            {
                label: "Keep Aspect Ratio",
                type: "bool",
                propertyID: "keepAspectRatio",
            }
        ]
    },
    {
        id: "web",
        label: "WEB",
        properties: [
            {
                label: "Source",
                type: "string",
                propertyID: "sourceUrl",
            },
            {
                label: "Source Resolution",
                type: "number-draggable",
                propertyID: "dpi",
            },
            {
                label: "Web Color",
                type: "color",
                propertyID: "webColor",
                propertyName: "color", // actual entity property name
            },
            {
                label: "Web Alpha",
                type: "number-draggable",
                step: 0.001,
                decimals: 3,
                propertyID: "webAlpha",
                propertyName: "alpha",
                min: 0,
                max: 1,
            },
            {
                label: "Use Background",
                type: "bool",
                propertyID: "useBackground",
            },
            {
                label: "Max FPS",
                type: "number-draggable",
                step: 1,
                decimals: 0,
                propertyID: "maxFPS",
            },
            {
                label: "Input Mode",
                type: "dropdown",
                options: {
                    touch: "Touch events",
                    mouse: "Mouse events"
                },
                propertyID: "inputMode",
            },
            {
                label: "Focus Highlight",
                type: "bool",
                propertyID: "showKeyboardFocusHighlight",
            },            
            {
                label: "Script URL",
                type: "string",
                propertyID: "scriptURL",
                placeholder: "URL",
            },
            {
                label: "User Agent",
                type: "string",
                propertyID: "userAgent",
                placeholder: "User Agent",
            }
        ]
    },
    {
        id: "light",
        label: "LIGHT",
        properties: [
            {
                label: "Light Color",
                type: "color",
                propertyID: "lightColor",
                propertyName: "color", // actual entity property name
            },
            {
                label: "Intensity",
                type: "number-draggable",
                min: -1000,
                max: 10000,
                step: 0.1,
                decimals: 2,
                propertyID: "intensity",
            },
            {
                label: "Fall-Off Radius",
                type: "number-draggable",
                min: 0,
                max: 10000,
                step: 0.1,
                decimals: 2,
                unit: "m",
                propertyID: "falloffRadius",
            },
            {
                label: "Spotlight",
                type: "bool",
                propertyID: "isSpotlight",
            },
            {
                label: "Spotlight Exponent",
                type: "number-draggable",
                min: 0,
                step: 0.01,
                decimals: 2,
                propertyID: "exponent",
            },
            {
                label: "Spotlight Cut-Off",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "cutoff",
            }
        ]
    },
    {
        id: "material",
        label: "MATERIAL",
        properties: [
            {
                label: "Material URL",
                type: "string",
                propertyID: "materialURL",
            },
            {
                label: "Material Data",
                type: "textarea",
                buttons: [ { id: "clear", label: "Clear Material Data", className: "red", onClick: clearMaterialData }, 
                           { id: "edit", label: "Edit as JSON", className: "blue", onClick: newJSONMaterialEditor },
                           { id: "save", label: "Save Material Data", className: "black", onClick: saveMaterialData } ],
                propertyID: "materialData",
            },
            {
                label: "Material Target",
                type: "dynamic-multiselect",
                propertyUpdate: materialTargetPropertyUpdate,
                propertyID: "parentMaterialName",
                selectionVisibility: PROPERTY_SELECTION_VISIBILITY.SINGLE_SELECTION,
            },
            {
                label: "Priority",
                type: "number-draggable",
                min: 0,
                propertyID: "priority",
            },
            {
                label: "Material Mapping Mode",
                type: "dropdown",
                options: {
                    uv: "UV space", projected: "3D projected"
                },
                propertyID: "materialMappingMode",
            },
            {
                label: "Material Position",
                type: "vec2",
                vec2Type: "xyz",
                min: 0,
                max: 1,
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y" ],
                propertyID: "materialMappingPos",
            },
            {
                label: "Material Scale",
                type: "vec2",
                vec2Type: "xyz",
                min: 0,
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y" ],
                propertyID: "materialMappingScale",
            },
            {
                label: "Material Rotation",
                type: "number-draggable",
                step: 0.1,
                decimals: 2,
                unit: "deg",
                propertyID: "materialMappingRot",
            },
            {
                label: "Material Repeat",
                type: "bool",
                propertyID: "materialRepeat",
            }
        ]
    },
    {
        id: "grid",
        label: "GRID",
        properties: [
            {
                label: "Color",
                type: "color",
                propertyID: "gridColor",
                propertyName: "color", // actual entity property name
            },
            {
                label: "Follow Camera",
                type: "bool",
                propertyID: "followCamera",
            },
            {
                label: "Major Grid Every",
                type: "number-draggable",
                min: 0,
                step: 1,
                decimals: 0,
                propertyID: "majorGridEvery",
            },
            {
                label: "Minor Grid Every",
                type: "number-draggable",
                min: 0,
                step: 0.01,
                decimals: 2,
                propertyID: "minorGridEvery",
            }
        ]
    },
    {
        id: "particles",
        label: "PARTICLES",
        properties: [
            {
                label: "Emit",
                type: "bool",
                propertyID: "isEmitting",
            },
            {
                label: "Lifespan",
                type: "number-draggable",
                unit: "s",
                step: 0.01,
                decimals: 2,
                propertyID: "lifespan",
            },
            {
                label: "Max Particles",
                type: "number-draggable",
                step: 1,
                propertyID: "maxParticles",
            },
            {
                label: "Texture",
                type: "texture",
                propertyID: "particleTextures",
                propertyName: "textures", // actual entity property name
            }
        ]
    },
    {
        id: "particles_emit",
        label: "PARTICLES EMIT",
        properties: [
            {
                label: "Emit Rate",
                type: "number-draggable",
                step: 1,
                propertyID: "emitRate",
            },
            {
                label: "Emit Speed",
                type: "number-draggable",
                step: 0.1,
                decimals: 2,
                propertyID: "emitSpeed",
            },
            {
                label: "Speed Spread",
                type: "number-draggable",
                step: 0.1,
                decimals: 2,
                propertyID: "speedSpread",
            },
            {
                label: "Shape Type",
                type: "dropdown",
                options: { "box": "Box", "ellipsoid": "Ellipsoid", 
                           "cylinder-y": "Cylinder", "circle": "Circle", "plane": "Plane",
                           "compound": "Use Compound Shape URL" },
                propertyID: "particleShapeType",
                propertyName: "shapeType",
            },
            {
                label: "Compound Shape URL",
                type: "string",
                propertyID: "particleCompoundShapeURL",
                propertyName: "compoundShapeURL",
            },
            {
                label: "Emit Dimensions",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                round: 100,
                subLabels: [ "x", "y", "z" ],
                propertyID: "emitDimensions",
            },
            {
                label: "Emit Radius Start",
                type: "number-draggable",
                step: 0.001,
                decimals: 3,
                propertyID: "emitRadiusStart"
            },
            {
                label: "Emit Orientation",
                type: "vec3",
                vec3Type: "pyr",
                step: 0.01,
                round: 100,
                subLabels: [ "x", "y", "z" ],
                unit: "deg",
                propertyID: "emitOrientation",
            },
            {
                label: "Trails",
                type: "bool",
                propertyID: "emitterShouldTrail",
            }
        ]
    },
    {
        id: "particles_size",
        label: "PARTICLES SIZE",
        properties: [
            {
                type: "triple",
                label: "Size",
                propertyID: "particleRadiusTriple",
                properties: [
                    {
                        label: "Start",
                        type: "number-draggable",
                        step: 0.01,
                        decimals: 2,
                        propertyID: "radiusStart",
                        fallbackProperty: "particleRadius",
                    },
                    {
                        label: "Middle",
                        type: "number-draggable",
                        step: 0.01,
                        decimals: 2,
                        propertyID: "particleRadius",
                    },
                    {
                        label: "Finish",
                        type: "number-draggable",
                        step: 0.01,
                        decimals: 2,
                        propertyID: "radiusFinish",
                        fallbackProperty: "particleRadius",
                    }
                ]
            },
            {
                label: "Size Spread",
                type: "number-draggable",
                step: 0.01,
                decimals: 2,
                propertyID: "radiusSpread",
            }
        ]
    },
    {
        id: "particles_color",
        label: "PARTICLES COLOR",
        properties: [
            {
                type: "triple",
                label: "Color",
                propertyID: "particleColorTriple",
                properties: [
                    {
                        label: "Start",
                        type: "color",
                        propertyID: "colorStart",
                        fallbackProperty: "color",
                    },
                    {
                        label: "Middle",
                        type: "color",
                        propertyID: "particleColor",
                        propertyName: "color", // actual entity property name
                    },
                    {
                        label: "Finish",
                        type: "color",
                        propertyID: "colorFinish",
                        fallbackProperty: "color",
                    }
                ]
            },
            {
                label: "Color Spread",
                type: "vec3rgb",
                vec3Type: "vec3rgb",
                min: 0,
                max: 255,
                step: 1,       
                decimals: 0,
                subLabels: [ "r", "g", "b" ],
                propertyID: "colorSpread",
            },
            {
                type: "triple",
                label: "Alpha",
                propertyID: "particleAlphaTriple",
                properties: [
                    {
                        label: "Start",
                        type: "number-draggable",
                        step: 0.001,
                        decimals: 3,
                        propertyID: "alphaStart",
                        fallbackProperty: "alpha",
                    },
                    {
                        label: "Middle",
                        type: "number-draggable",
                        step: 0.001,
                        decimals: 3,
                        propertyID: "alpha",
                    },
                    {
                        label: "Finish",
                        type: "number-draggable",
                        step: 0.001,
                        decimals: 3,
                        propertyID: "alphaFinish",
                        fallbackProperty: "alpha",
                    }
                ]
            },
            {
                label: "Alpha Spread",
                type: "number-draggable",
                step: 0.001,
                decimals: 3,
                propertyID: "alphaSpread",
            }
        ]
    },
    {
        id: "particles_behavior",
        label: "PARTICLES BEHAVIOR",
        properties: [
            {
                label: "Emit Acceleration",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                round: 100,
                subLabels: [ "x", "y", "z" ],
                propertyID: "emitAcceleration",
            },
            {
                label: "Acceleration Spread",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                round: 100,
                subLabels: [ "x", "y", "z" ],
                propertyID: "accelerationSpread",
            },
            {
                type: "triple",
                label: "Spin",
                propertyID: "particleSpinTriple",
                properties: [
                    {
                        label: "Start",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "spinStart",
                        fallbackProperty: "particleSpin",
                    },
                    {
                        label: "Middle",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "particleSpin",
                    },
                    {
                        label: "Finish",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "spinFinish",
                        fallbackProperty: "particleSpin",
                    }
                ]
            },
            {
                label: "Spin Spread",
                type: "number-draggable",
                step: 0.1,
                decimals: 2,
                multiplier: DEGREES_TO_RADIANS,
                unit: "deg",
                propertyID: "spinSpread",
            },
            {
                label: "Rotate with Entity",
                type: "bool",
                propertyID: "rotateWithEntity",
            }
        ]
    },
    {
        id: "particles_constraints",
        label: "PARTICLES CONSTRAINTS",
        properties: [
            {
                type: "triple",
                label: "Horizontal Angle",
                propertyID: "particlePolarTriple",
                properties: [
                    {
                        label: "Start",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "polarStart",
                    },
                    {
                        label: "Finish",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "polarFinish",
                    }
                ],
            },
            {
                type: "triple",
                label: "Vertical Angle",
                propertyID: "particleAzimuthTriple",
                properties: [
                    {
                        label: "Start",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "azimuthStart",
                    },
                    {
                        label: "Finish",
                        type: "number-draggable",
                        step: 0.1,
                        decimals: 2,
                        multiplier: DEGREES_TO_RADIANS,
                        unit: "deg",
                        propertyID: "azimuthFinish",
                    }
                ]
            }
        ]
    },
    {
        id: "spatial",
        label: "SPATIAL",
        properties: [
            {
                label: "Position",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "m",
                propertyID: "position",
                spaceMode: PROPERTY_SPACE_MODE.WORLD,
            },
            {
                label: "Local Position",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "m",
                propertyID: "localPosition",
                spaceMode: PROPERTY_SPACE_MODE.LOCAL,
            },
            {
                label: "Rotation",
                type: "vec3",
                vec3Type: "pyr",
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "deg",
                propertyID: "rotation",
                spaceMode: PROPERTY_SPACE_MODE.WORLD,
            },
            {
                label: "Local Rotation",
                type: "vec3",
                vec3Type: "pyr",
                step: 0.1,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "deg",
                propertyID: "localRotation",
                spaceMode: PROPERTY_SPACE_MODE.LOCAL,
            },
            {
                label: "Dimensions",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "m",
                propertyID: "dimensions",
                spaceMode: PROPERTY_SPACE_MODE.WORLD,
            },
            {
                label: "Local Dimensions",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "m",
                propertyID: "localDimensions",
                spaceMode: PROPERTY_SPACE_MODE.LOCAL,
            },
            {
                label: "Scale",
                type: "number-draggable",
                defaultValue: 100,
                unit: "%",
                buttons: [ { id: "rescale", label: "Rescale", className: "blue", onClick: rescaleDimensions }, 
                           { id: "reset", label: "Reset Dimensions", className: "red", onClick: resetToNaturalDimensions } ],
                propertyID: "scale",
            },
            {
                label: "Pivot",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.001,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "(ratio of dimension)",
                propertyID: "registrationPoint",
            },
            {
                label: "Align",
                type: "buttons",
                buttons: [ { id: "selection", label: "Selection to Grid", className: "black", onClick: moveSelectionToGrid },
                           { id: "all", label: "All to Grid", className: "black", onClick: moveAllToGrid } ],
                propertyID: "alignToGrid",
            }
        ]
    },
    {
        id: "behavior",
        label: "BEHAVIOR",
        properties: [
            {
                label: "Grabbable",
                type: "bool",
                propertyID: "grab.grabbable",
            },
            {
                label: "Cloneable",
                type: "bool",
                propertyID: "cloneable",
            },
            {
                label: "Clone Lifetime",
                type: "number-draggable",
                min: -1,
                unit: "s",
                propertyID: "cloneLifetime",
                showPropertyRule: { "cloneable": "true" },
            },
            {
                label: "Clone Limit",
                type: "number-draggable",
                min: 0,
                propertyID: "cloneLimit",
                showPropertyRule: { "cloneable": "true" },
            },
            {
                label: "Clone Dynamic",
                type: "bool",
                propertyID: "cloneDynamic",
                showPropertyRule: { "cloneable": "true" },
            },
            {
                label: "Clone Avatar Entity",
                type: "bool",
                propertyID: "cloneAvatarEntity",
                showPropertyRule: { "cloneable": "true" },
            },
            {
                label: "Triggerable",
                type: "bool",
                propertyID: "grab.triggerable",
            },
            {
                label: "Follow Controller",
                type: "bool",
                propertyID: "grab.grabFollowsController",
            },
            {
                label: "Cast Shadows",
                type: "bool",
                propertyID: "canCastShadow",
            },
            {
                label: "Link",
                type: "string",
                propertyID: "href",
                placeholder: "URL",
            },
            {
                label: "Ignore Pick Intersection",
                type: "bool",
                propertyID: "ignorePickIntersection",
            },
            {
                label: "Lifetime",
                type: "number",
                unit: "s",
                propertyID: "lifetime",
            }
        ]
    },
    {
        id: "scripts",
        label: "SCRIPTS",
        properties: [
            {
                label: "Script",
                type: "string",
                buttons: [ { id: "reload", label: "F", className: "glyph", onClick: reloadScripts } ],
                propertyID: "script",
                placeholder: "URL",
                hideIfCertified: true,
            },
            {
                label: "Server Script",
                type: "string",
                buttons: [ { id: "reload", label: "F", className: "glyph", onClick: reloadServerScripts } ],
                propertyID: "serverScripts",
                placeholder: "URL",
            },
            {
                label: "Server Script Status",
                type: "placeholder",
                indentedLabel: true,
                propertyID: "serverScriptStatus",
                selectionVisibility: PROPERTY_SELECTION_VISIBILITY.SINGLE_SELECTION,
            },
            {
                label: "User Data",
                type: "textarea",
                buttons: [ { id: "clear", label: "Clear User Data", className: "red", onClick: clearUserData }, 
                           { id: "edit", label: "Edit as JSON", className: "blue", onClick: newJSONEditor },
                           { id: "save", label: "Save User Data", className: "black", onClick: saveUserData } ],
                propertyID: "userData",
            }
        ]
    },
    {
        id: "collision",
        label: "COLLISION",
        properties: [
            {
                label: "Collides",
                type: "bool",
                inverse: true,
                propertyID: "collisionless",
            },
            {
                label: "Static Entities",
                type: "bool",
                propertyID: "collidesWithStatic",
                propertyName: "static", // actual subProperty name
                subPropertyOf: "collidesWith",
                showPropertyRule: { "collisionless": "false" },
            },
            {
                label: "Kinematic Entities",
                type: "bool",
                propertyID: "collidesWithKinematic",
                propertyName: "kinematic", // actual subProperty name
                subPropertyOf: "collidesWith",
                showPropertyRule: { "collisionless": "false" },
            },
            {
                label: "Dynamic Entities",
                type: "bool",
                propertyID: "collidesWithDynamic",
                propertyName: "dynamic", // actual subProperty name
                subPropertyOf: "collidesWith",
                showPropertyRule: { "collisionless": "false" },
            },
            {
                label: "My Avatar",
                type: "bool",
                propertyID: "collidesWithMyAvatar",
                propertyName: "myAvatar", // actual subProperty name
                subPropertyOf: "collidesWith",
                showPropertyRule: { "collisionless": "false" },
            },
            {
                label: "Other Avatars",
                type: "bool",
                propertyID: "collidesWithOtherAvatar",
                propertyName: "otherAvatar", // actual subProperty name
                subPropertyOf: "collidesWith",
                showPropertyRule: { "collisionless": "false" },
            },
            {
                label: "Collision Sound",
                type: "string",
                placeholder: "URL",
                propertyID: "collisionSoundURL",
                showPropertyRule: { "collisionless": "false" },
                hideIfCertified: true,
            },
            {
                label: "Dynamic",
                type: "bool",
                propertyID: "dynamic",
            }
        ]
    },
    {
        id: "physics",
        label: "PHYSICS",
        properties: [
            {
                label: "Linear Velocity",
                type: "vec3",
                vec3Type: "xyz",
                step: 0.01,
                decimals: 4,
                subLabels: [ "x", "y", "z" ],
                unit: "m/s",
                propertyID: "localVelocity",
            },
            {
                label: "Linear Damping",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.001,
                decimals: 4,
                propertyID: "damping",
            },
            {
                label: "Angular Velocity",
                type: "vec3",
                vec3Type: "pyr",
                multiplier: DEGREES_TO_RADIANS,
                decimals: 6,
                step: 1,
                subLabels: [ "x", "y", "z" ],
                unit: "deg/s",
                propertyID: "localAngularVelocity",
            },
            {
                label: "Angular Damping",
                type: "number-draggable",
                min: 0,
                max: 1,
                step: 0.001,
                decimals: 4,
                propertyID: "angularDamping",
            },
            {
                label: "Bounciness",
                type: "number-draggable",
                step: 0.001,
                decimals: 4,
                propertyID: "restitution",
            },
            {
                label: "Friction",
                type: "number-draggable",
                step: 0.01,
                decimals: 4,
                propertyID: "friction",
            },
            {
                label: "Density",
                type: "number-draggable",
                step: 1,
                decimals: 4,
                propertyID: "density",
            },
            {
                label: "Gravity",
                type: "vec3",
                vec3Type: "xyz",
                subLabels: [ "x", "y", "z" ],
                step: 0.1,
                decimals: 4,
                unit: "m/s<sup>2</sup>",
                propertyID: "gravity",
            }
        ]
    },
];

const GROUPS_PER_TYPE = {
  None: [ 'base', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Shape: [ 'base', 'shape', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Text: [ 'base', 'text', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Zone: [ 'base', 'zone', 'zone_key_light', 'zone_skybox', 'zone_ambient_light', 'zone_haze', 
            'zone_bloom', 'zone_avatar_priority', 'spatial', 'behavior', 'scripts', 'physics' ],
  Model: [ 'base', 'model', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Image: [ 'base', 'image', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Web: [ 'base', 'web', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Light: [ 'base', 'light', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Material: [ 'base', 'material', 'spatial', 'behavior', 'scripts', 'physics' ],
  ParticleEffect: [ 'base', 'particles', 'particles_emit', 'particles_size', 'particles_color', 
                    'particles_behavior', 'particles_constraints', 'spatial', 'behavior', 'scripts', 'physics' ],
  PolyLine: [ 'base', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  PolyVox: [ 'base', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
  Grid: [ 'base', 'grid', 'spatial', 'behavior', 'scripts', 'physics' ],
  Multiple: [ 'base', 'spatial', 'behavior', 'scripts', 'collision', 'physics' ],
};

const EDITOR_TIMEOUT_DURATION = 1500;
const DEBOUNCE_TIMEOUT = 125;

const COLOR_MIN = 0;
const COLOR_MAX = 255;
const COLOR_STEP = 1;

const MATERIAL_PREFIX_STRING = "mat::";

const PENDING_SCRIPT_STATUS = "[ Fetching status ]";
const NOT_RUNNING_SCRIPT_STATUS = "Not running";
const ENTITY_SCRIPT_STATUS = {
    pending: "Pending",
    loading: "Loading",
    error_loading_script: "Error loading script", // eslint-disable-line camelcase
    error_running_script: "Error running script", // eslint-disable-line camelcase
    running: "Running",
    unloaded: "Unloaded"
};

const ENABLE_DISABLE_SELECTOR = "input, textarea, span, .dropdown dl, .color-picker";

const PROPERTY_NAME_DIVISION = {
    GROUP: 0,
    PROPERTY: 1,
    SUB_PROPERTY: 2,
};

const RECT_ELEMENTS = {
    X_NUMBER: 0,
    Y_NUMBER: 1,
    WIDTH_NUMBER: 2,
    HEIGHT_NUMBER: 3,
};

const VECTOR_ELEMENTS = {
    X_NUMBER: 0,
    Y_NUMBER: 1,
    Z_NUMBER: 2,
};

const COLOR_ELEMENTS = {
    COLOR_PICKER: 0,
    RED_NUMBER: 1,
    GREEN_NUMBER: 2,
    BLUE_NUMBER: 3,
};

const TEXTURE_ELEMENTS = {
    IMAGE: 0,
    TEXT_INPUT: 1,
};

const JSON_EDITOR_ROW_DIV_INDEX = 2;

let elGroups = {};
let properties = {};
let propertyRangeRequests = [];
let colorPickers = {};
let particlePropertyUpdates = {};
let selectedEntityIDs = new Set();
let currentSelections = [];
let createAppTooltip = new CreateAppTooltip();
let currentSpaceMode = PROPERTY_SPACE_MODE.LOCAL;
let zonesList = [];

function createElementFromHTML(htmlString) {
    let elTemplate = document.createElement('template');
    elTemplate.innerHTML = htmlString.trim();
    return elTemplate.content.firstChild;
}

function isFlagSet(value, flag) {
    return (value & flag) === flag;
}

/**
 * GENERAL PROPERTY/GROUP FUNCTIONS
 */

function getPropertyInputElement(propertyID) {
    let property = properties[propertyID];
    switch (property.data.type) {
        case 'string':
        case 'number':
        case 'bool':
        case 'dropdown':
        case 'textarea':
        case 'texture':
            return property.elInput;
        case 'multipleZonesSelection':
            return property.elInput;
        case 'number-draggable':
            return property.elNumber.elInput;
        case 'rect':
            return {
                x: property.elNumberX.elInput,
                y: property.elNumberY.elInput,
                width: property.elNumberWidth.elInput,
                height: property.elNumberHeight.elInput
            };
        case 'vec3': 
        case 'vec2':
            return { x: property.elNumberX.elInput, y: property.elNumberY.elInput, z: property.elNumberZ.elInput };
        case 'color':
            return { red: property.elNumberR.elInput, green: property.elNumberG.elInput, blue: property.elNumberB.elInput };
        case 'vec3rgb':
            return { red: property.elNumberR.elInput, green: property.elNumberG.elInput, blue: property.elNumberB.elInput };            
        case 'icon':
            return property.elLabel;
        case 'dynamic-multiselect':
            return property.elDivOptions;
        default:
            return undefined;
    }
}

function enableChildren(el, selector) {
    let elSelectors = el.querySelectorAll(selector);
    for (let selectorIndex = 0; selectorIndex < elSelectors.length; ++selectorIndex) {
        elSelectors[selectorIndex].removeAttribute('disabled');
    }
}

function disableChildren(el, selector) {
    let elSelectors = el.querySelectorAll(selector);
    for (let selectorIndex = 0; selectorIndex < elSelectors.length; ++selectorIndex) {
        elSelectors[selectorIndex].setAttribute('disabled', 'disabled');
    }
}

function enableProperties() {
    enableChildren(document.getElementById("properties-list"), ENABLE_DISABLE_SELECTOR);
    enableChildren(document, ".colpick");
    enableAllMultipleZoneSelector();
}

function disableProperties() {
    disableChildren(document.getElementById("properties-list"), ENABLE_DISABLE_SELECTOR);
    disableChildren(document, ".colpick");
    for (let pickKey in colorPickers) {
        colorPickers[pickKey].colpickHide();
    }
    disableAllMultipleZoneSelector();
}

function showPropertyElement(propertyID, show) {
    setPropertyVisibility(properties[propertyID], show);
}

function setPropertyVisibility(property, visible) {
    property.elContainer.style.display = visible ? null : "none";
}

function resetProperties() {
    for (let propertyID in properties) { 
        let property = properties[propertyID];
        let propertyData = property.data;
        
        switch (propertyData.type) {
            case 'number':
            case 'string': {
                property.elInput.classList.remove('multi-diff');
                if (propertyData.defaultValue !== undefined) {
                    property.elInput.value = propertyData.defaultValue;
                } else {
                    property.elInput.value = "";
                }
                break;
            }
            case 'bool': {
                property.elInput.classList.remove('multi-diff');
                property.elInput.checked = false;
                break;
            }
            case 'number-draggable': {
                if (propertyData.defaultValue !== undefined) {
                    property.elNumber.setValue(propertyData.defaultValue, false);
                } else { 
                    property.elNumber.setValue("", false);
                }
                break;
            }
            case 'rect': {
                property.elNumberX.setValue("", false);
                property.elNumberY.setValue("", false);
                property.elNumberWidth.setValue("", false);
                property.elNumberHeight.setValue("", false);
                break;
            }
            case 'vec3': 
            case 'vec2': {
                property.elNumberX.setValue("", false);
                property.elNumberY.setValue("", false);
                if (property.elNumberZ !== undefined) {
                    property.elNumberZ.setValue("", false);
                }
                break;
            }
            case 'color': {
                property.elColorPicker.style.backgroundColor = "rgb(" + 0 + "," + 0 + "," + 0 + ")";
                property.elNumberR.setValue("", false);
                property.elNumberG.setValue("", false);
                property.elNumberB.setValue("", false);
                break;
            }
            case 'vec3rgb': {
                property.elNumberR.setValue("", false);
                property.elNumberG.setValue("", false);
                property.elNumberB.setValue("", false);
                break;
            }            
            case 'dropdown': {
                property.elInput.classList.remove('multi-diff');
                property.elInput.value = "";
                setDropdownText(property.elInput);
                break;
            }
            case 'textarea': {
                property.elInput.classList.remove('multi-diff');
                property.elInput.value = "";
                setTextareaScrolling(property.elInput);
                break;
            }
            case 'multipleZonesSelection': {
                property.elInput.classList.remove('multi-diff');
                property.elInput.value = "[]";
                setZonesSelectionData(property.elInput, false);
                break;
            }
            case 'icon': {
                property.elSpan.style.display = "none";
                break;
            }
            case 'texture': {
                property.elInput.classList.remove('multi-diff');
                property.elInput.value = "";
                property.elInput.imageLoad(property.elInput.value);
                break;
            }
            case 'dynamic-multiselect': {
                resetDynamicMultiselectProperty(property.elDivOptions);
                break;
            }
        }

        let showPropertyRules = properties[propertyID].showPropertyRules;
        if (showPropertyRules !== undefined) {
            for (let propertyToHide in showPropertyRules) {
                showPropertyElement(propertyToHide, false);
            }
        }
    }

    resetServerScriptStatus();
}

function resetServerScriptStatus() {
    let elServerScriptError = document.getElementById("property-serverScripts-error");
    let elServerScriptStatus = document.getElementById("property-serverScripts-status");
    elServerScriptError.parentElement.style.display = "none";
    elServerScriptStatus.innerText = NOT_RUNNING_SCRIPT_STATUS;
}

function showGroupsForType(type) {
    if (type === "Box" || type === "Sphere") {
        showGroupsForTypes(["Shape"]);
        showOnTheSamePage(["Shape"]);
        return;
    }
    if (type === "None") {
        showGroupsForTypes(["None"]);
        return;        
    }
    showGroupsForTypes([type]);
    showOnTheSamePage([type]);
}

function getGroupsForTypes(types) {
    return Object.keys(elGroups).filter((groupKey) => {
        return types.map(type => GROUPS_PER_TYPE[type].includes(groupKey)).every(function (hasGroup) {
            return hasGroup;
        });
    });
}

function showGroupsForTypes(types) {
    Object.entries(elGroups).forEach(([groupKey, elGroup]) => {
        if (types.map(type => GROUPS_PER_TYPE[type].includes(groupKey)).every(function (hasGroup) { return hasGroup; })) {
            elGroup.style.display = "none";
            if (types !== "None") {
                document.getElementById("tab-" + groupKey).style.display = "block";
            } else {
                document.getElementById("tab-" + groupKey).style.display = "none";
            }
        } else {
            elGroup.style.display = "none";
            document.getElementById("tab-" + groupKey).style.display = "none";
        }
    });
}

function getFirstSelectedID() {
    if (selectedEntityIDs.size === 0) {
        return null;
    }
    return selectedEntityIDs.values().next().value;
}

/**
 * Returns true when the user is currently dragging the numeric slider control of the property
 * @param propertyName - name of property
 * @returns {boolean} currentlyDragging
 */
function isCurrentlyDraggingProperty(propertyName) {
    return properties[propertyName] && properties[propertyName].dragging === true;
}

const SUPPORTED_FALLBACK_TYPES = ['number', 'number-draggable', 'rect', 'vec3', 'vec2', 'color', 'vec3rgb'];

function getMultiplePropertyValue(originalPropertyName) {
    // if this is a compound property name (i.e. animation.running)
    // then split it by . up to 3 times to find property value

    let propertyData = null;
    if (properties[originalPropertyName] !== undefined) {
        propertyData = properties[originalPropertyName].data;
    }

    let propertyValues = [];
    let splitPropertyName = originalPropertyName.split('.');
    if (splitPropertyName.length > 1) {
        let propertyGroupName = splitPropertyName[PROPERTY_NAME_DIVISION.GROUP];
        let propertyName = splitPropertyName[PROPERTY_NAME_DIVISION.PROPERTY];
        propertyValues = currentSelections.map(selection => {
            let groupProperties = selection.properties[propertyGroupName];
            if (groupProperties === undefined || groupProperties[propertyName] === undefined) {
                return undefined;
            }
            if (splitPropertyName.length === PROPERTY_NAME_DIVISION.SUB_PROPERTY + 1) {
                let subPropertyName = splitPropertyName[PROPERTY_NAME_DIVISION.SUB_PROPERTY];
                return groupProperties[propertyName][subPropertyName];
            } else {
                return groupProperties[propertyName];
            }
        });
    } else {
        propertyValues = currentSelections.map(selection => selection.properties[originalPropertyName]);
    }

    if (propertyData !== null && propertyData.fallbackProperty !== undefined &&
        SUPPORTED_FALLBACK_TYPES.includes(propertyData.type)) {

        let fallbackMultiValue = null;

        for (let i = 0; i < propertyValues.length; ++i) {
            let isPropertyNotNumber = false;
            let propertyValue = propertyValues[i];
            if (propertyValue === undefined) {
                continue;
            }
            switch (propertyData.type) {
                case 'number':
                case 'number-draggable':
                    isPropertyNotNumber = isNaN(propertyValue) || propertyValue === null;
                    break;
                case 'rect':
                case 'vec3':
                case 'vec2':
                    isPropertyNotNumber = isNaN(propertyValue.x) || propertyValue.x === null;
                    break;
                case 'color':
                    isPropertyNotNumber = isNaN(propertyValue.red) || propertyValue.red === null;
                    break;
                case 'vec3rgb':
                    isPropertyNotNumber = isNaN(propertyValue.red) || propertyValue.red === null;
                    break;
            }
            if (isPropertyNotNumber) {
                if (fallbackMultiValue === null) {
                    fallbackMultiValue = getMultiplePropertyValue(propertyData.fallbackProperty);
                }
                propertyValues[i] = fallbackMultiValue.values[i];
            }
        }
    }

    const firstValue = propertyValues[0];
    const isMultiDiffValue = !propertyValues.every((x) => deepEqual(firstValue, x));

    if (isMultiDiffValue) {
        return {
            value: undefined,
            values: propertyValues,
            isMultiDiffValue: true
        }
    }

    return {
        value: propertyValues[0],
        values: propertyValues,
        isMultiDiffValue: false
    };
}

/**
 * Retrieve more detailed info for differing Numeric MultiplePropertyValue
 * @param multiplePropertyValue - input multiplePropertyValue
 * @param propertyData
 * @returns {{keys: *[], propertyComponentDiff, averagePerPropertyComponent}}
 */
function getDetailedNumberMPVDiff(multiplePropertyValue, propertyData) {
    let detailedValues = {};
    // Fixed numbers can't be easily averaged since they're strings, so lets keep an array of unmodified numbers
    let unmodifiedValues = {};
    const DEFAULT_KEY = 0;
    let uniqueKeys = new Set([]);
    multiplePropertyValue.values.forEach(function(propertyValue) {
        if (typeof propertyValue === "object") {
            Object.entries(propertyValue).forEach(function([key, value]) {
                if (!uniqueKeys.has(key)) {
                    uniqueKeys.add(key);
                    detailedValues[key] = [];
                    unmodifiedValues[key] = [];
                }
                detailedValues[key].push(applyInputNumberPropertyModifiers(value, propertyData));
                unmodifiedValues[key].push(value);
            });
        } else {
            if (!uniqueKeys.has(DEFAULT_KEY)) {
                uniqueKeys.add(DEFAULT_KEY);
                detailedValues[DEFAULT_KEY] = [];
                unmodifiedValues[DEFAULT_KEY] = [];
            }
            detailedValues[DEFAULT_KEY].push(applyInputNumberPropertyModifiers(propertyValue, propertyData));
            unmodifiedValues[DEFAULT_KEY].push(propertyValue);
        }
    });
    let keys = [...uniqueKeys];

    let propertyComponentDiff = {};
    Object.entries(detailedValues).forEach(function([key, value]) {
        propertyComponentDiff[key] = [...new Set(value)].length > 1;
    });

    let averagePerPropertyComponent = {};
    Object.entries(unmodifiedValues).forEach(function([key, value]) {
        let average = value.reduce((a, b) => a + b) / value.length;
        averagePerPropertyComponent[key] = applyInputNumberPropertyModifiers(average, propertyData);
    });

    return {
        keys,
        propertyComponentDiff,
        averagePerPropertyComponent,
    };
}

function getDetailedSubPropertyMPVDiff(multiplePropertyValue, subPropertyName) {
    let isChecked = false;
    let checkedValues = multiplePropertyValue.values.map((value) => value.split(",").includes(subPropertyName));
    let isMultiDiff = !checkedValues.every(value => value === checkedValues[0]);
    if (!isMultiDiff) {
        isChecked = checkedValues[0];
    }
    return {
        isChecked,
        isMultiDiff
    }
}

function updateVisibleSpaceModeProperties() {
    for (let propertyID in properties) {
        if (properties.hasOwnProperty(propertyID)) {
            let property = properties[propertyID];
            let propertySpaceMode = property.spaceMode;
            let elProperty = properties[propertyID].elContainer;
            if (propertySpaceMode !== PROPERTY_SPACE_MODE.ALL && propertySpaceMode !== currentSpaceMode) {
                elProperty.classList.add('spacemode-hidden');
            } else {
                elProperty.classList.remove('spacemode-hidden');
            }
        }
    }
}

/**
 * PROPERTY UPDATE FUNCTIONS
 */

function createPropertyUpdateObject(originalPropertyName, propertyValue) {
    let propertyUpdate = {};
    // if this is a compound property name (i.e. animation.running) then split it by . up to 3 times
    let splitPropertyName = originalPropertyName.split('.');
    if (splitPropertyName.length > 1) {
        let propertyGroupName = splitPropertyName[PROPERTY_NAME_DIVISION.GROUP];
        let propertyName = splitPropertyName[PROPERTY_NAME_DIVISION.PROPERTY];
        propertyUpdate[propertyGroupName] = {};
        if (splitPropertyName.length === PROPERTY_NAME_DIVISION.SUB_PROPERTY + 1) {
            let subPropertyName = splitPropertyName[PROPERTY_NAME_DIVISION.SUB_PROPERTY];
            propertyUpdate[propertyGroupName][propertyName] = {};
            propertyUpdate[propertyGroupName][propertyName][subPropertyName] = propertyValue;
        } else {
            propertyUpdate[propertyGroupName][propertyName] = propertyValue;
        }
    } else {
        propertyUpdate[originalPropertyName] = propertyValue;
    }
    return propertyUpdate;
}

function updateProperty(originalPropertyName, propertyValue, isParticleProperty) {
    let propertyUpdate = createPropertyUpdateObject(originalPropertyName, propertyValue);

    // queue up particle property changes with the debounced sync to avoid  
    // causing particle emitting to reset excessively with each value change
    if (isParticleProperty) {
        Object.keys(propertyUpdate).forEach(function (propertyUpdateKey) {
            particlePropertyUpdates[propertyUpdateKey] = propertyUpdate[propertyUpdateKey];
        });
        particleSyncDebounce();
    } else {
        // only update the entity property value itself if in the middle of dragging
        // prevent undo command push, saving new property values, and property update
        // callback until drag is complete (additional update sent via dragEnd callback)
        let onlyUpdateEntity = isCurrentlyDraggingProperty(originalPropertyName);
        updateProperties(propertyUpdate, onlyUpdateEntity);
    }
}

let particleSyncDebounce = _.debounce(function () {
    updateProperties(particlePropertyUpdates);
    particlePropertyUpdates = {};
}, DEBOUNCE_TIMEOUT);

function updateProperties(propertiesToUpdate, onlyUpdateEntity) {
    if (onlyUpdateEntity === undefined) {
        onlyUpdateEntity = false;
    }
    EventBridge.emitWebEvent(JSON.stringify({
        ids: [...selectedEntityIDs],
        type: "update",
        properties: propertiesToUpdate,
        onlyUpdateEntities: onlyUpdateEntity
    }));
}

function updateMultiDiffProperties(propertiesMapToUpdate, onlyUpdateEntity) {
    if (onlyUpdateEntity === undefined) {
        onlyUpdateEntity = false;
    }
    EventBridge.emitWebEvent(JSON.stringify({
        type: "update",
        propertiesMap: propertiesMapToUpdate,
        onlyUpdateEntities: onlyUpdateEntity
    }));
}

function createEmitTextPropertyUpdateFunction(property) {
    return function() {
        property.elInput.classList.remove('multi-diff');
        updateProperty(property.name, this.value, property.isParticleProperty);
    };
}

function createEmitCheckedPropertyUpdateFunction(property) {
    return function() {
        updateProperty(property.name, property.data.inverse ? !this.checked : this.checked, property.isParticleProperty);
    };
}

function createDragStartFunction(property) {
    return function() {
        property.dragging = true;
    };
}

function createDragEndFunction(property) {
    return function() {
        property.dragging = false;

        if (this.multiDiffModeEnabled) {
            let propertyMultiValue = getMultiplePropertyValue(property.name);
            let updateObjects = [];
            const selectedEntityIDsArray = [...selectedEntityIDs];

            for (let i = 0; i < selectedEntityIDsArray.length; ++i) {
                let entityID = selectedEntityIDsArray[i];
                updateObjects.push({
                    entityIDs: [entityID],
                    properties: createPropertyUpdateObject(property.name, propertyMultiValue.values[i]),
                });
            }

            // send a full updateMultiDiff post-dragging to count as an action in the undo stack
            updateMultiDiffProperties(updateObjects);
        } else {
            // send an additional update post-dragging to consider whole property change from dragStart to dragEnd to be 1 action
            this.valueChangeFunction();
        }
    };
}

function createEmitNumberPropertyUpdateFunction(property) {
    return function() {
        let value = parseFloat(applyOutputNumberPropertyModifiers(parseFloat(this.value), property.data));
        updateProperty(property.name, value, property.isParticleProperty);
    };
}

function createEmitNumberPropertyComponentUpdateFunction(property, propertyComponent) {
    return function() {
        let propertyMultiValue = getMultiplePropertyValue(property.name);
        let value = parseFloat(applyOutputNumberPropertyModifiers(parseFloat(this.value), property.data));

        if (propertyMultiValue.isMultiDiffValue) {
            let updateObjects = [];
            const selectedEntityIDsArray = [...selectedEntityIDs];

            for (let i = 0; i < selectedEntityIDsArray.length; ++i) {
                let entityID = selectedEntityIDsArray[i];

                let propertyObject = propertyMultiValue.values[i];
                propertyObject[propertyComponent] = value;

                let updateObject = createPropertyUpdateObject(property.name, propertyObject);
                updateObjects.push({
                    entityIDs: [entityID],
                    properties: updateObject,
                });

                mergeDeep(currentSelections[i].properties, updateObject);
            }

            // only update the entity property value itself if in the middle of dragging
            // prevent undo command push, saving new property values, and property update
            // callback until drag is complete (additional update sent via dragEnd callback)
            let onlyUpdateEntity = isCurrentlyDraggingProperty(property.name);
            updateMultiDiffProperties(updateObjects, onlyUpdateEntity);
        } else {
            let propertyValue = propertyMultiValue.value;
            propertyValue[propertyComponent] = value;
            updateProperty(property.name, propertyValue, property.isParticleProperty);
        }
    };
}

function createEmitColorPropertyUpdateFunction(property) {
    return function() {
        emitColorPropertyUpdate(property.name, property.elNumberR.elInput.value, property.elNumberG.elInput.value,
                                property.elNumberB.elInput.value, property.isParticleProperty);
    };
}

function emitColorPropertyUpdate(propertyName, red, green, blue, isParticleProperty) {
    let newValue = {
        red: red,
        green: green,
        blue: blue
    };
    updateProperty(propertyName, newValue, isParticleProperty);
}

function toggleBooleanCSV(inputCSV, property, enable) {
    let values = inputCSV.split(",");
    if (enable && !values.includes(property)) {
        values.push(property);
    } else if (!enable && values.includes(property)) {
        values = values.filter(value => value !== property);
    }
    return values.join(",");
}

function updateCheckedSubProperty(propertyName, propertyMultiValue, subPropertyElement, subPropertyString, isParticleProperty) {
    if (propertyMultiValue.isMultiDiffValue) {
        let updateObjects = [];
        const selectedEntityIDsArray = [...selectedEntityIDs];

        for (let i = 0; i < selectedEntityIDsArray.length; ++i) {
            let newValue = toggleBooleanCSV(propertyMultiValue.values[i], subPropertyString, subPropertyElement.checked);
            updateObjects.push({
                entityIDs: [selectedEntityIDsArray[i]],
                properties: createPropertyUpdateObject(propertyName, newValue),
            });
        }

        updateMultiDiffProperties(updateObjects);
    } else {
        updateProperty(propertyName, toggleBooleanCSV(propertyMultiValue.value, subPropertyString, subPropertyElement.checked),
            isParticleProperty);
    }
}

/**
 * PROPERTY ELEMENT CREATION FUNCTIONS
 */

function createStringProperty(property, elProperty) {
    let elementID = property.elementID;
    let propertyData = property.data;
    
    elProperty.className = "text";
    
    let elInput = createElementFromHTML(`
        <input id="${elementID}"
               type="text"
               ${propertyData.placeholder ? 'placeholder="' + propertyData.placeholder + '"' : ''}
               ${propertyData.readOnly ? 'readonly' : ''}/>
        `);


    elInput.addEventListener('change', createEmitTextPropertyUpdateFunction(property));
    if (propertyData.onChange !== undefined) {
        elInput.addEventListener('change', propertyData.onChange);
    }


    let elMultiDiff = document.createElement('span');
    elMultiDiff.className = "multi-diff";

    elProperty.appendChild(elInput);
    elProperty.appendChild(elMultiDiff);
    
    if (propertyData.buttons !== undefined) {
        addButtons(elProperty, elementID, propertyData.buttons, false);
    }

    return elInput;
}

function createBoolProperty(property, elProperty) {   
    let propertyName = property.name;
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className = "checkbox";

    if (propertyData.glyph !== undefined) {
        let elSpan = document.createElement('span');
        elSpan.innerHTML = propertyData.glyph;
        elSpan.className = 'icon';
        elProperty.appendChild(elSpan);
    }
    
    let elInput = document.createElement('input');
    elInput.setAttribute("id", elementID);
    elInput.setAttribute("type", "checkbox");
    
    elProperty.appendChild(elInput);
    elProperty.appendChild(createElementFromHTML(`<label for=${elementID}>&nbsp;</label>`));
    
    let subPropertyOf = propertyData.subPropertyOf;
    if (subPropertyOf !== undefined) {
        elInput.addEventListener('change', function() {
            let subPropertyMultiValue = getMultiplePropertyValue(subPropertyOf);

            updateCheckedSubProperty(subPropertyOf,
                                     subPropertyMultiValue,
                                     elInput, propertyName, property.isParticleProperty);
        });
    } else {
        elInput.addEventListener('change', createEmitCheckedPropertyUpdateFunction(property));
    }
    
    return elInput;
}

function createNumberProperty(property, elProperty) {
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className = "text";

    let elInput = createElementFromHTML(`
        <input id="${elementID}"
               class='hide-spinner'
               type="number"
               ${propertyData.placeholder ? 'placeholder="' + propertyData.placeholder + '"' : ''}
               ${propertyData.readOnly ? 'readonly' : ''}/>
        `);

    if (propertyData.min !== undefined) {
        elInput.setAttribute("min", propertyData.min);
    }
    if (propertyData.max !== undefined) {
        elInput.setAttribute("max", propertyData.max);
    }
    if (propertyData.step !== undefined) {
        elInput.setAttribute("step", propertyData.step);
    }
    if (propertyData.defaultValue !== undefined) {
        elInput.value = propertyData.defaultValue;
    }

    elInput.addEventListener('change', createEmitNumberPropertyUpdateFunction(property));

    let elMultiDiff = document.createElement('span');
    elMultiDiff.className = "multi-diff";

    elProperty.appendChild(elInput);
    elProperty.appendChild(elMultiDiff);

    if (propertyData.buttons !== undefined) {
        addButtons(elProperty, elementID, propertyData.buttons, false);
    }

    return elInput;
}

function updateNumberMinMax(property) {
    let elInput = property.elInput;
    let min = property.data.min;
    let max = property.data.max;
    if (min !== undefined) {
        elInput.setAttribute("min", min);
    }
    if (max !== undefined) {
        elInput.setAttribute("max", max);
    }
}

/**
 *
 * @param {object} property - property update on step
 * @param {string} [propertyComponent] - propertyComponent to update on step (e.g. enter 'x' to just update position.x)
 * @returns {Function}
 */
function createMultiDiffStepFunction(property, propertyComponent) {
    return function(step, shouldAddToUndoHistory) {
        if (shouldAddToUndoHistory === undefined) {
            shouldAddToUndoHistory = false;
        }

        let propertyMultiValue = getMultiplePropertyValue(property.name);
        if (!propertyMultiValue.isMultiDiffValue) {
            console.log("setMultiDiffStepFunction is only supposed to be called in MultiDiff mode.");
            return;
        }

        let multiplier = property.data.multiplier !== undefined ? property.data.multiplier : 1;

        let applyDelta = step * multiplier;

        if (selectedEntityIDs.size !== propertyMultiValue.values.length) {
            console.log("selectedEntityIDs and propertyMultiValue got out of sync.");
            return;
        }
        let updateObjects = [];
        const selectedEntityIDsArray = [...selectedEntityIDs];

        for (let i = 0; i < selectedEntityIDsArray.length; ++i) {
            let entityID = selectedEntityIDsArray[i];

            let updatedValue;
            if (propertyComponent !== undefined) {
                let objectToUpdate = propertyMultiValue.values[i];
                objectToUpdate[propertyComponent] += applyDelta;
                updatedValue = objectToUpdate;
            } else {
                updatedValue = propertyMultiValue.values[i] + applyDelta;
            }
            let propertiesUpdate = createPropertyUpdateObject(property.name, updatedValue);
            updateObjects.push({
                entityIDs: [entityID],
                properties: propertiesUpdate
            });
            // We need to store these so that we can send a full update on the dragEnd
            mergeDeep(currentSelections[i].properties, propertiesUpdate);
        }

        updateMultiDiffProperties(updateObjects, !shouldAddToUndoHistory);
    }
}

function createNumberDraggableProperty(property, elProperty) { 
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className += " draggable-number-container";

    let dragStartFunction = createDragStartFunction(property);
    let dragEndFunction = createDragEndFunction(property);
    let elDraggableNumber = new DraggableNumber(propertyData.min, propertyData.max, propertyData.step,
                                                propertyData.decimals, dragStartFunction, dragEndFunction);

    let defaultValue = propertyData.defaultValue;
    if (defaultValue !== undefined) {
        elDraggableNumber.elInput.value = defaultValue;
    }

    let valueChangeFunction = createEmitNumberPropertyUpdateFunction(property);
    elDraggableNumber.setValueChangeFunction(valueChangeFunction);

    elDraggableNumber.setMultiDiffStepFunction(createMultiDiffStepFunction(property));
    
    elDraggableNumber.elInput.setAttribute("id", elementID);
    elProperty.appendChild(elDraggableNumber.elDiv);

    if (propertyData.buttons !== undefined) {
        addButtons(elDraggableNumber.elDiv, elementID, propertyData.buttons, false);
    }
    
    return elDraggableNumber;
}

function updateNumberDraggableMinMax(property) {
    let propertyData = property.data;
    property.elNumber.updateMinMax(propertyData.min, propertyData.max);
}

function createRectProperty(property, elProperty) {
    let propertyData = property.data;

    elProperty.className = "rect";

    let elXYRow = document.createElement('div');
    elXYRow.className = "rect-row fstuple";
    elProperty.appendChild(elXYRow);

    let elWidthHeightRow = document.createElement('div');
    elWidthHeightRow.className = "rect-row fstuple";
    elProperty.appendChild(elWidthHeightRow);


    let elNumberX = createTupleNumberInput(property, propertyData.subLabels[RECT_ELEMENTS.X_NUMBER]);
    let elNumberY = createTupleNumberInput(property, propertyData.subLabels[RECT_ELEMENTS.Y_NUMBER]);
    let elNumberWidth = createTupleNumberInput(property, propertyData.subLabels[RECT_ELEMENTS.WIDTH_NUMBER]);
    let elNumberHeight = createTupleNumberInput(property, propertyData.subLabels[RECT_ELEMENTS.HEIGHT_NUMBER]);

    elXYRow.appendChild(elNumberX.elDiv);
    elXYRow.appendChild(elNumberY.elDiv);
    elWidthHeightRow.appendChild(elNumberWidth.elDiv);
    elWidthHeightRow.appendChild(elNumberHeight.elDiv);

    elNumberX.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'x'));
    elNumberY.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'y'));
    elNumberWidth.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'width'));
    elNumberHeight.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'height'));

    elNumberX.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'x'));
    elNumberY.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'y'));
    elNumberX.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'width'));
    elNumberY.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'height'));

    let elResult = [];
    elResult[RECT_ELEMENTS.X_NUMBER] = elNumberX;
    elResult[RECT_ELEMENTS.Y_NUMBER] = elNumberY;
    elResult[RECT_ELEMENTS.WIDTH_NUMBER] = elNumberWidth;
    elResult[RECT_ELEMENTS.HEIGHT_NUMBER] = elNumberHeight;
    return elResult;
}

function updateRectMinMax(property) {
    let min = property.data.min;
    let max = property.data.max;
    property.elNumberX.updateMinMax(min, max);
    property.elNumberY.updateMinMax(min, max);
    property.elNumberWidth.updateMinMax(min, max);
    property.elNumberHeight.updateMinMax(min, max);
}

function createVec3Property(property, elProperty) {
    let propertyData = property.data;

    elProperty.className = propertyData.vec3Type + " fstuple";

    let elNumberX = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.X_NUMBER]);
    let elNumberY = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.Y_NUMBER]);
    let elNumberZ = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.Z_NUMBER]);
    elProperty.appendChild(elNumberX.elDiv);
    elProperty.appendChild(elNumberY.elDiv);
    elProperty.appendChild(elNumberZ.elDiv);

    elNumberX.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'x'));
    elNumberY.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'y'));
    elNumberZ.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'z'));

    elNumberX.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'x'));
    elNumberY.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'y'));
    elNumberZ.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'z'));

    let elResult = [];
    elResult[VECTOR_ELEMENTS.X_NUMBER] = elNumberX;
    elResult[VECTOR_ELEMENTS.Y_NUMBER] = elNumberY;
    elResult[VECTOR_ELEMENTS.Z_NUMBER] = elNumberZ;
    return elResult;
}

function createVec3rgbProperty(property, elProperty) {
    let propertyData = property.data;

    elProperty.className = propertyData.vec3Type + " fstuple";

    let elNumberR = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.X_NUMBER]);
    let elNumberG = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.Y_NUMBER]);
    let elNumberB = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.Z_NUMBER]);
    elProperty.appendChild(elNumberR.elDiv);
    elProperty.appendChild(elNumberG.elDiv);
    elProperty.appendChild(elNumberB.elDiv);

    elNumberR.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'red'));
    elNumberG.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'green'));
    elNumberB.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'blue'));

    elNumberR.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'red'));
    elNumberG.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'green'));
    elNumberB.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'blue'));

    let elResult = [];
    elResult[VECTOR_ELEMENTS.X_NUMBER] = elNumberR;
    elResult[VECTOR_ELEMENTS.Y_NUMBER] = elNumberG;
    elResult[VECTOR_ELEMENTS.Z_NUMBER] = elNumberB;
    return elResult;
}

function createVec2Property(property, elProperty) {
    let propertyData = property.data;
    
    elProperty.className = propertyData.vec2Type + " fstuple";

    let elTuple = document.createElement('div');
    elTuple.className = "tuple";
    
    elProperty.appendChild(elTuple);
    
    let elNumberX = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.X_NUMBER]);
    let elNumberY = createTupleNumberInput(property, propertyData.subLabels[VECTOR_ELEMENTS.Y_NUMBER]);
    elProperty.appendChild(elNumberX.elDiv);
    elProperty.appendChild(elNumberY.elDiv);

    elNumberX.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'x'));
    elNumberY.setValueChangeFunction(createEmitNumberPropertyComponentUpdateFunction(property, 'y'));

    elNumberX.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'x'));
    elNumberY.setMultiDiffStepFunction(createMultiDiffStepFunction(property, 'y'));
    
    let elResult = [];
    elResult[VECTOR_ELEMENTS.X_NUMBER] = elNumberX;
    elResult[VECTOR_ELEMENTS.Y_NUMBER] = elNumberY;
    return elResult;
}

function updateVectorMinMax(property) {
    let min = property.data.min;
    let max = property.data.max;
    property.elNumberX.updateMinMax(min, max);
    property.elNumberY.updateMinMax(min, max);
    if (property.elNumberZ) {
        property.elNumberZ.updateMinMax(min, max);
    }
}

function createColorProperty(property, elProperty) {
    let propertyName = property.name;
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className += " rgb fstuple";

    let elColorPicker = document.createElement('div');
    elColorPicker.className = "color-picker";
    elColorPicker.setAttribute("id", elementID);

    let elTuple = document.createElement('div');
    elTuple.className = "tuple";

    elProperty.appendChild(elColorPicker);
    elProperty.appendChild(elTuple);

    if (propertyData.min === undefined) {
        propertyData.min = COLOR_MIN;
    }
    if (propertyData.max === undefined) {
        propertyData.max = COLOR_MAX;
    }
    if (propertyData.step === undefined) {
        propertyData.step = COLOR_STEP;
    }

    let elNumberR = createTupleNumberInput(property, "red");
    let elNumberG = createTupleNumberInput(property, "green");
    let elNumberB = createTupleNumberInput(property, "blue");
    elTuple.appendChild(elNumberR.elDiv);
    elTuple.appendChild(elNumberG.elDiv);
    elTuple.appendChild(elNumberB.elDiv);

    let valueChangeFunction = createEmitColorPropertyUpdateFunction(property);
    elNumberR.setValueChangeFunction(valueChangeFunction);
    elNumberG.setValueChangeFunction(valueChangeFunction);
    elNumberB.setValueChangeFunction(valueChangeFunction);

    let colorPickerID = "#" + elementID;
    colorPickers[colorPickerID] = $(colorPickerID).colpick({
        colorScheme: 'dark',
        layout: 'rgbhex',
        color: '000000',
        submit: false, // We don't want to have a submission button
        onShow: function(colpick) {
            // The original color preview within the picker needs to be updated on show because
            // prior to the picker being shown we don't have access to the selections' starting color.
            colorPickers[colorPickerID].colpickSetColor({
                "r": elNumberR.elInput.value,
                "g": elNumberG.elInput.value,
                "b": elNumberB.elInput.value
            });

            // Set the color picker active after setting the color, otherwise an update will be sent on open.
            $(colorPickerID).attr('active', 'true');
        },
        onHide: function(colpick) {
            $(colorPickerID).attr('active', 'false');
        },
        onChange: function(hsb, hex, rgb, el) {
            $(el).css('background-color', '#' + hex);
            if ($(colorPickerID).attr('active') === 'true') {
                emitColorPropertyUpdate(propertyName, rgb.r, rgb.g, rgb.b);
            }
        }
    });

    let elResult = [];
    elResult[COLOR_ELEMENTS.COLOR_PICKER] = elColorPicker;
    elResult[COLOR_ELEMENTS.RED_NUMBER] = elNumberR;
    elResult[COLOR_ELEMENTS.GREEN_NUMBER] = elNumberG;
    elResult[COLOR_ELEMENTS.BLUE_NUMBER] = elNumberB;
    return elResult;
}

function createDropdownProperty(property, propertyID, elProperty) { 
    let elementID = property.elementID;
    let propertyData = property.data;
    
    elProperty.className = "dropdown";
                        
    let elInput = document.createElement('select');
    elInput.setAttribute("id", elementID);
    elInput.setAttribute("propertyID", propertyID);
    
    for (let optionKey in propertyData.options) {
        let option = document.createElement('option');
        option.value = optionKey;
        option.text = propertyData.options[optionKey];
        elInput.add(option);
    }

    elInput.addEventListener('change', createEmitTextPropertyUpdateFunction(property));

    elProperty.appendChild(elInput);

    return elInput;
}

function createTextareaProperty(property, elProperty) {
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className = "textarea";

    let elInput = document.createElement('textarea');
    elInput.setAttribute("id", elementID);
    if (propertyData.readOnly) {
        elInput.readOnly = true;
    }

    elInput.addEventListener('change', createEmitTextPropertyUpdateFunction(property));

    let elMultiDiff = document.createElement('span');
    elMultiDiff.className = "multi-diff";

    elProperty.appendChild(elInput);
    elProperty.appendChild(elMultiDiff);

    if (propertyData.buttons !== undefined) {
        addButtons(elProperty, elementID, propertyData.buttons, true);
    }

    return elInput;
}

function createIconProperty(property, elProperty) {
    let elementID = property.elementID;

    elProperty.className = "value";

    let elSpan = document.createElement('span');
    elSpan.setAttribute("id", elementID + "-icon");
    elSpan.className = 'icon';

    elProperty.appendChild(elSpan);

    return elSpan;
}

function createTextureProperty(property, elProperty) {
    let elementID = property.elementID;

    elProperty.className = "texture";

    let elDiv = document.createElement("div");
    let elImage = document.createElement("img");
    elDiv.className = "texture-image no-texture";
    elDiv.appendChild(elImage);

    let elInput = document.createElement('input');
    elInput.setAttribute("id", elementID);
    elInput.setAttribute("type", "text"); 

    let imageLoad = function(url) {
        elDiv.style.display = null;
        if (url.slice(0, 5).toLowerCase() === "atp:/" || url.slice(0, 9).toLowerCase() === "file:///~") {
            elImage.src = "";
            elImage.style.display = "none";
            elDiv.classList.remove("with-texture");
            elDiv.classList.remove("no-texture");
            elDiv.classList.add("no-preview");
        } else if (url.length > 0) {
            elDiv.classList.remove("no-texture");
            elDiv.classList.remove("no-preview");
            elDiv.classList.add("with-texture");
            elImage.src = url;
            elImage.style.display = "block";
        } else {
            elImage.src = "";
            elImage.style.display = "none";
            elDiv.classList.remove("with-texture");
            elDiv.classList.remove("no-preview");
            elDiv.classList.add("no-texture");
        }
    };
    elInput.imageLoad = imageLoad;
    elInput.setMultipleValues = function() {
        elDiv.style.display = "none";
    };
    elInput.addEventListener('change', createEmitTextPropertyUpdateFunction(property));
    elInput.addEventListener('change', function(ev) {
        imageLoad(ev.target.value);
    });

    elProperty.appendChild(elInput);
    let elMultiDiff = document.createElement('span');
    elMultiDiff.className = "multi-diff";
    elProperty.appendChild(elMultiDiff);
    elProperty.appendChild(elDiv);

    let elResult = [];
    elResult[TEXTURE_ELEMENTS.IMAGE] = elImage;
    elResult[TEXTURE_ELEMENTS.TEXT_INPUT] = elInput;
    return elResult;
}

function createButtonsProperty(property, elProperty) {
    let elementID = property.elementID;
    let propertyData = property.data;
    
    elProperty.className = "text";

    if (propertyData.buttons !== undefined) {
        addButtons(elProperty, elementID, propertyData.buttons, false);
    }

    return elProperty;
}

function createDynamicMultiselectProperty(property, elProperty) {
    let elementID = property.elementID;
    let propertyData = property.data;

    elProperty.className = "dynamic-multiselect";

    let elDivOptions = document.createElement('div');
    elDivOptions.setAttribute("id", elementID + "-options");
    elDivOptions.style = "overflow-y:scroll;max-height:160px;";

    let elDivButtons = document.createElement('div');
    elDivButtons.setAttribute("id", elDivOptions.getAttribute("id") + "-buttons");

    let elLabel = document.createElement('label');
    elLabel.innerText = "No Options";
    elDivOptions.appendChild(elLabel);

    let buttons = [ { id: "selectAll", label: "Select All", className: "black", onClick: selectAllMaterialTarget }, 
                    { id: "clearAll", label: "Clear All", className: "black", onClick: clearAllMaterialTarget } ];
    addButtons(elDivButtons, elementID, buttons, false);

    elProperty.appendChild(elDivOptions);
    elProperty.appendChild(elDivButtons);

    return elDivOptions;
}

function resetDynamicMultiselectProperty(elDivOptions) {
    let elInputs = elDivOptions.getElementsByTagName("input");
    while (elInputs.length > 0) {
        let elDivOption = elInputs[0].parentNode;
        elDivOption.parentNode.removeChild(elDivOption);
    }
    elDivOptions.firstChild.style.display = null; // show "No Options" text
    elDivOptions.parentNode.lastChild.style.display = "none"; // hide Select/Clear all buttons
}

function createTupleNumberInput(property, subLabel) {
    let propertyElementID = property.elementID;
    let propertyData = property.data;
    let elementID = propertyElementID + "-" + subLabel.toLowerCase();

    let elLabel = document.createElement('label');
    elLabel.className = "sublabel " + subLabel;
    elLabel.innerText = subLabel[0].toUpperCase() + subLabel.slice(1);
    elLabel.setAttribute("for", elementID);
    elLabel.style.visibility = "visible";

    let dragStartFunction = createDragStartFunction(property);
    let dragEndFunction = createDragEndFunction(property);
    let elDraggableNumber = new DraggableNumber(propertyData.min, propertyData.max, propertyData.step, 
                                                propertyData.decimals, dragStartFunction, dragEndFunction); 
    elDraggableNumber.elInput.setAttribute("id", elementID);
    elDraggableNumber.elDiv.className += " fstuple";
    elDraggableNumber.elDiv.insertBefore(elLabel, elDraggableNumber.elLeftArrow);

    return elDraggableNumber;
}

function addButtons(elProperty, propertyID, buttons, newRow) {
    let elDiv = document.createElement('div');
    elDiv.className = "row";

    buttons.forEach(function(button) {
        let elButton = document.createElement('input');
        elButton.className = button.className;
        elButton.setAttribute("type", "button");
        elButton.setAttribute("id", propertyID + "-button-" + button.id);
        elButton.setAttribute("value", button.label);
        elButton.addEventListener("click", button.onClick);
        if (newRow) {
            elDiv.appendChild(elButton);
        } else {
            elProperty.appendChild(elButton);
        }
    });
    
    if (newRow) {
        elProperty.appendChild(document.createElement('br'));
        elProperty.appendChild(elDiv);
    }
}

function createProperty(propertyData, propertyElementID, propertyName, propertyID, elProperty) {
    let property = {
        data: propertyData, 
        elementID: propertyElementID, 
        name: propertyName,
        elProperty: elProperty,
    };
    let propertyType = propertyData.type;

    switch (propertyType) {
        case 'string': {
            property.elInput = createStringProperty(property, elProperty);
            break;
        }
        case 'bool': {
            property.elInput = createBoolProperty(property, elProperty);
            break;
        }
        case 'number': {
            property.elInput = createNumberProperty(property, elProperty);
            break;
        }
        case 'number-draggable': {
            property.elNumber = createNumberDraggableProperty(property, elProperty);
            break;
        }
        case 'rect': {
            let elRect = createRectProperty(property, elProperty);
            property.elNumberX = elRect[RECT_ELEMENTS.X_NUMBER];
            property.elNumberY = elRect[RECT_ELEMENTS.Y_NUMBER];
            property.elNumberWidth = elRect[RECT_ELEMENTS.WIDTH_NUMBER];
            property.elNumberHeight = elRect[RECT_ELEMENTS.HEIGHT_NUMBER];
            break;
        }
        case 'vec3': {
            let elVec3 = createVec3Property(property, elProperty);  
            property.elNumberX = elVec3[VECTOR_ELEMENTS.X_NUMBER];
            property.elNumberY = elVec3[VECTOR_ELEMENTS.Y_NUMBER];
            property.elNumberZ = elVec3[VECTOR_ELEMENTS.Z_NUMBER];
            break;
        }
        case 'vec2': {
            let elVec2 = createVec2Property(property, elProperty);  
            property.elNumberX = elVec2[VECTOR_ELEMENTS.X_NUMBER];
            property.elNumberY = elVec2[VECTOR_ELEMENTS.Y_NUMBER];
            break;
        }
        case 'color': {
            let elColor = createColorProperty(property, elProperty);  
            property.elColorPicker = elColor[COLOR_ELEMENTS.COLOR_PICKER];
            property.elNumberR = elColor[COLOR_ELEMENTS.RED_NUMBER];
            property.elNumberG = elColor[COLOR_ELEMENTS.GREEN_NUMBER];
            property.elNumberB = elColor[COLOR_ELEMENTS.BLUE_NUMBER]; 
            break;
        }
        case 'vec3rgb': {
            let elVec3 = createVec3rgbProperty(property, elProperty);  
            property.elNumberR = elVec3[VECTOR_ELEMENTS.X_NUMBER];
            property.elNumberG = elVec3[VECTOR_ELEMENTS.Y_NUMBER];
            property.elNumberB = elVec3[VECTOR_ELEMENTS.Z_NUMBER];
            break;
        }        
        case 'dropdown': {
            property.elInput = createDropdownProperty(property, propertyID, elProperty);
            break;
        }
        case 'textarea': {
            property.elInput = createTextareaProperty(property, elProperty);
            break;
        }
        case 'multipleZonesSelection': {
            property.elInput = createZonesSelection(property, elProperty);
            break;            
        }
        case 'icon': {
            property.elSpan = createIconProperty(property, elProperty);
            break;
        }
        case 'texture': {
            let elTexture = createTextureProperty(property, elProperty);
            property.elImage = elTexture[TEXTURE_ELEMENTS.IMAGE];
            property.elInput = elTexture[TEXTURE_ELEMENTS.TEXT_INPUT];
            break;
        }
        case 'buttons': {
            property.elProperty = createButtonsProperty(property, elProperty);
            break;
        }
        case 'dynamic-multiselect': {
            property.elDivOptions = createDynamicMultiselectProperty(property, elProperty);
            break;
        }
        case 'placeholder':
        case 'sub-header': {
            break;
        }
        default: {
            console.log("EntityProperties - Unknown property type " + 
                        propertyType + " set to property " + propertyID);
            break;
        }
    }

    return property;
}


/**
 * PROPERTY-SPECIFIC CALLBACKS
 */
 
function parentIDChanged() {
    if (currentSelections.length === 1 && currentSelections[0].properties.type === "Material") {
        requestMaterialTarget();
    }
}


/**
 * BUTTON CALLBACKS
 */

function rescaleDimensions() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "rescaleDimensions",
        percentage: parseFloat(document.getElementById("property-scale").value)
    }));
}

function moveSelectionToGrid() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "moveSelectionToGrid"
    }));
}

function moveAllToGrid() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "moveAllToGrid"
    }));
}

function resetToNaturalDimensions() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "resetToNaturalDimensions"
    }));
}

function reloadModelURL() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "reloadModelURL"
    }));
}

function reloadScripts() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "reloadClientScripts"
    }));
}

function reloadServerScripts() {
    // invalidate the current status (so that same-same updates can still be observed visually)
    document.getElementById("property-serverScripts-status").innerText = PENDING_SCRIPT_STATUS;
        EventBridge.emitWebEvent(JSON.stringify({
        type: "action",
        action: "reloadServerScripts"
    }));
}

function copySkyboxURLToAmbientURL() {
    let skyboxURL = getPropertyInputElement("skybox.url").value;
    getPropertyInputElement("ambientLight.ambientURL").value = skyboxURL;
    updateProperty("ambientLight.ambientURL", skyboxURL, false);
}


/**
 * USER DATA FUNCTIONS
 */

function clearUserData() {
    let elUserData = getPropertyInputElement("userData");
    deleteJSONEditor();
    elUserData.value = "";
    showUserDataTextArea();
    showNewJSONEditorButton();
    hideSaveUserDataButton();
    updateProperty('userData', elUserData.value, false);
}

function newJSONEditor() {
    getPropertyInputElement("userData").classList.remove('multi-diff');
    deleteJSONEditor();
    createJSONEditor();
    let data = {};
    setEditorJSON(data);
    hideUserDataTextArea();
    hideNewJSONEditorButton();
    showSaveUserDataButton();
}

/**
 * @param {Set.<string>} [entityIDsToUpdate] Entity IDs to update userData for.
 */
function saveUserData(entityIDsToUpdate) {
    saveJSONUserData(true, entityIDsToUpdate);
}

function setJSONError(property, isError) {
    $("#property-"+ property + "-editor").toggleClass('error', isError);
    let $propertyUserDataEditorStatus = $("#property-"+ property + "-editorStatus");
    $propertyUserDataEditorStatus.css('display', isError ? 'block' : 'none');
    $propertyUserDataEditorStatus.text(isError ? 'Invalid JSON code - look for red X in your code' : '');
}

/**
 * @param {boolean} noUpdate - don't update the UI, but do send a property update.
 * @param {Set.<string>} [entityIDsToUpdate] - Entity IDs to update userData for.
 */
function setUserDataFromEditor(noUpdate, entityIDsToUpdate) {
    let errorFound = false;
    try {
        editor.get();
    } catch (e) {
        errorFound = true;
    }

    setJSONError('userData', errorFound);

    if (errorFound) {
        return;
    }

    let text = editor.getText();
    if (noUpdate) {
        EventBridge.emitWebEvent(
            JSON.stringify({
                ids: [...entityIDsToUpdate],
                type: "saveUserData",
                properties: {
                    userData: text
                }
            })
        );
    } else {
        updateProperty('userData', text, false);
    }
}

let editor = null;

function createJSONEditor() {
    let container = document.getElementById("property-userData-editor");
    let options = {
        search: false,
        mode: 'tree',
        modes: ['code', 'tree'],
        name: 'userData',
        onError: function(e) {
            alert('JSON editor:' + e);
        },
        onChange: function() {
            let currentJSONString = editor.getText();

            if (currentJSONString === '{"":""}') {
                return;
            }
            $('#property-userData-button-save').attr('disabled', false);
        }
    };
    editor = new JSONEditor(container, options);
}

function showSaveUserDataButton() {
    $('#property-userData-button-save').show();
}

function hideSaveUserDataButton() {
    $('#property-userData-button-save').hide();
}

function disableSaveUserDataButton() {
    $('#property-userData-button-save').attr('disabled', true);
}

function showNewJSONEditorButton() {
    $('#property-userData-button-edit').show();
}

function hideNewJSONEditorButton() {
    $('#property-userData-button-edit').hide();
}

function showUserDataTextArea() {
    $('#property-userData').show();
}

function hideUserDataTextArea() {
    $('#property-userData').hide();
}

function hideUserDataSaved() {
    $('#property-userData-saved').hide();
}

function setEditorJSON(json) {
    editor.set(json);
    if (editor.hasOwnProperty('expandAll')) {
        editor.expandAll();
    }
}

function deleteJSONEditor() {
    if (editor !== null) {
        setJSONError('userData', false);
        editor.destroy();
        editor = null;
    }
}

let savedJSONTimer = null;

/**
 * @param {boolean} noUpdate - don't update the UI, but do send a property update.
 * @param {Set.<string>} [entityIDsToUpdate] Entity IDs to update userData for
 */
function saveJSONUserData(noUpdate, entityIDsToUpdate) {
    setUserDataFromEditor(noUpdate, entityIDsToUpdate ? entityIDsToUpdate : selectedEntityIDs);
    $('#property-userData-saved').show();
    $('#property-userData-button-save').attr('disabled', true);
    if (savedJSONTimer !== null) {
        clearTimeout(savedJSONTimer);
    }
    savedJSONTimer = setTimeout(function() {
        hideUserDataSaved();
    }, EDITOR_TIMEOUT_DURATION);
}


/**
 * MATERIAL DATA FUNCTIONS
 */

function clearMaterialData() {
    let elMaterialData = getPropertyInputElement("materialData");
    deleteJSONMaterialEditor();
    elMaterialData.value = "";
    showMaterialDataTextArea();
    showNewJSONMaterialEditorButton();
    hideSaveMaterialDataButton();
    updateProperty('materialData', elMaterialData.value, false);
}

function newJSONMaterialEditor() {
    getPropertyInputElement("materialData").classList.remove('multi-diff');
    deleteJSONMaterialEditor();
    createJSONMaterialEditor();
    let data = {};
    setMaterialEditorJSON(data);
    hideMaterialDataTextArea();
    hideNewJSONMaterialEditorButton();
    showSaveMaterialDataButton();
}

function saveMaterialData() {
    saveJSONMaterialData(true);
}

/**
 * @param {boolean} noUpdate - don't update the UI, but do send a property update.
 * @param {Set.<string>} [entityIDsToUpdate] - Entity IDs to update materialData for.
 */
function setMaterialDataFromEditor(noUpdate, entityIDsToUpdate) {
    let errorFound = false;
    try {
        materialEditor.get();
    } catch (e) {
        errorFound = true;
    }

    setJSONError('materialData', errorFound);

    if (errorFound) {
        return;
    }
    let text = materialEditor.getText();
    if (noUpdate) {
        EventBridge.emitWebEvent(
            JSON.stringify({
                ids: [...entityIDsToUpdate],
                type: "saveMaterialData",
                properties: {
                    materialData: text
                }
            })
        );
    } else {
        updateProperty('materialData', text, false);
    }
}

let materialEditor = null;

function createJSONMaterialEditor() {
    let container = document.getElementById("property-materialData-editor");
    let options = {
        search: false,
        mode: 'tree',
        modes: ['code', 'tree'],
        name: 'materialData',
        onError: function(e) {
            alert('JSON editor:' + e);
        },
        onChange: function() {
            let currentJSONString = materialEditor.getText();

            if (currentJSONString === '{"":""}') {
                return;
            }
            $('#property-materialData-button-save').attr('disabled', false);
        }
    };
    materialEditor = new JSONEditor(container, options);
}

function showSaveMaterialDataButton() {
    $('#property-materialData-button-save').show();
}

function hideSaveMaterialDataButton() {
    $('#property-materialData-button-save').hide();
}

function disableSaveMaterialDataButton() {
    $('#property-materialData-button-save').attr('disabled', true);
}

function showNewJSONMaterialEditorButton() {
    $('#property-materialData-button-edit').show();
}

function hideNewJSONMaterialEditorButton() {
    $('#property-materialData-button-edit').hide();
}

function showMaterialDataTextArea() {
    $('#property-materialData').show();
}

function hideMaterialDataTextArea() {
    $('#property-materialData').hide();
}

function hideMaterialDataSaved() {
    $('#property-materialData-saved').hide();
}

function setMaterialEditorJSON(json) {
    materialEditor.set(json);
    if (materialEditor.hasOwnProperty('expandAll')) {
        materialEditor.expandAll();
    }
}

function deleteJSONMaterialEditor() {
    if (materialEditor !== null) {
        setJSONError('materialData', false);
        materialEditor.destroy();
        materialEditor = null;
    }
}

let savedMaterialJSONTimer = null;

/**
 * @param {boolean} noUpdate - don't update the UI, but do send a property update.
 * @param {Set.<string>} [entityIDsToUpdate] - Entity IDs to update materialData for.
 */
function saveJSONMaterialData(noUpdate, entityIDsToUpdate) {
    setMaterialDataFromEditor(noUpdate, entityIDsToUpdate ? entityIDsToUpdate : selectedEntityIDs);
    $('#property-materialData-saved').show();
    $('#property-materialData-button-save').attr('disabled', true);
    if (savedMaterialJSONTimer !== null) {
        clearTimeout(savedMaterialJSONTimer);
    }
    savedMaterialJSONTimer = setTimeout(function() {
        hideMaterialDataSaved();
    }, EDITOR_TIMEOUT_DURATION);
}

function bindAllNonJSONEditorElements() {
    let inputs = $('input');
    let i;
    for (i = 0; i < inputs.length; ++i) {
        let input = inputs[i];
        let field = $(input);
        // TODO FIXME: (JSHint) Functions declared within loops referencing 
        //             an outer scoped variable may lead to confusing semantics.
        field.on('focus', function(e) {
            if (e.target.id === "property-userData-button-edit" || e.target.id === "property-userData-button-clear" || 
                e.target.id === "property-materialData-button-edit" || e.target.id === "property-materialData-button-clear") {
                return;
            }
            if ($('#property-userData-editor').css('height') !== "0px") {
                saveUserData();
            }
            if ($('#property-materialData-editor').css('height') !== "0px") {
                saveMaterialData();
            }
        });
    }
}


/**
 * DROPDOWN FUNCTIONS
 */

function setDropdownText(dropdown) {
    let lis = dropdown.parentNode.getElementsByTagName("li");
    let text = "";
    for (let i = 0; i < lis.length; ++i) {
        if (String(lis[i].getAttribute("value")) === String(dropdown.value)) {
            text = lis[i].textContent;
        }
    }
    dropdown.firstChild.textContent = text;
}

function toggleDropdown(event) {
    let element = event.target;
    if (element.nodeName !== "DT") {
        element = element.parentNode;
    }
    element = element.parentNode;
    let isDropped = element.getAttribute("dropped");
    element.setAttribute("dropped", isDropped !== "true" ? "true" : "false");
}

function closeAllDropdowns() {
    let elDropdowns = document.querySelectorAll("div.dropdown > dl");
    for (let i = 0; i < elDropdowns.length; ++i) {
        elDropdowns[i].setAttribute('dropped', 'false');
    }
}

function setDropdownValue(event) {
    let dt = event.target.parentNode.parentNode.previousSibling.previousSibling;
    dt.value = event.target.getAttribute("value");
    dt.firstChild.textContent = event.target.textContent;

    dt.parentNode.setAttribute("dropped", "false");

    let evt = document.createEvent("HTMLEvents");
    evt.initEvent("change", true, true);
    dt.dispatchEvent(evt);
}


/**
 * TEXTAREA FUNCTIONS
 */

function setTextareaScrolling(element) {
    let isScrolling = element.scrollHeight > element.offsetHeight;
    element.setAttribute("scrolling", isScrolling ? "true" : "false");
}

/**
 * ZONE SELECTOR FUNCTIONS
 */

function enableAllMultipleZoneSelector() {
    let allMultiZoneSelectors = document.querySelectorAll(".hiddenMultiZonesSelection");
    let i, propId;
    for (i = 0; i < allMultiZoneSelectors.length; i++) {
        propId = allMultiZoneSelectors[i].id;
        displaySelectedZones(propId, true);
    }
} 

function disableAllMultipleZoneSelector() {
    let allMultiZoneSelectors = document.querySelectorAll(".hiddenMultiZonesSelection");
    let i, propId;
    for (i = 0; i < allMultiZoneSelectors.length; i++) {
        propId = allMultiZoneSelectors[i].id;
        displaySelectedZones(propId, false);
    }
} 

function requestZoneList() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: "zoneListRequest"
    }));
}

function addZoneToZonesSelection(propertyId) {
    let hiddenField = document.getElementById(propertyId);
    if (JSON.stringify(hiddenField.value) === '"undefined"') {
        hiddenField.value = "[]";
    }
    let selectedZones = JSON.parse(hiddenField.value);
    let zoneToAdd = document.getElementById("zones-select-" + propertyId).value;
    if (!selectedZones.includes(zoneToAdd)) {
        selectedZones.push(zoneToAdd);
    }
    hiddenField.value = JSON.stringify(selectedZones);
    displaySelectedZones(propertyId, true);
    let propertyName = propertyId.replace("property-", "");
    updateProperty(propertyName, selectedZones, false);
}

function removeZoneFromZonesSelection(propertyId, zoneId) {
    let hiddenField = document.getElementById(propertyId);
    if (JSON.stringify(hiddenField.value) === '"undefined"') {
        hiddenField.value = "[]";
    }
    let selectedZones = JSON.parse(hiddenField.value);
    let index = selectedZones.indexOf(zoneId);
    if (index > -1) {
      selectedZones.splice(index, 1);
    }
    hiddenField.value = JSON.stringify(selectedZones);
    displaySelectedZones(propertyId, true);
    let propertyName = propertyId.replace("property-", "");
    updateProperty(propertyName, selectedZones, false);
}

function displaySelectedZones(propertyId, isEditable) {
    let i,j, name, listedZoneInner, hiddenData, isMultiple;
    hiddenData = document.getElementById(propertyId).value;
    if (JSON.stringify(hiddenData) === '"undefined"') {
        isMultiple = true;
        hiddenData = "[]";
    } else {
        isMultiple = false;  
    }
    let selectedZones = JSON.parse(hiddenData);
    listedZoneInner = "<table>";
    if (selectedZones.length === 0) {
        if (!isMultiple) {
            listedZoneInner += "<tr><td class='zoneItem'>&nbsp;</td><td>&nbsp;</td></tr>";
        } else {
            listedZoneInner += "<tr><td class='zoneItem'>[ WARNING: Any changes will apply to all. ]</td><td>&nbsp;</td></tr>";
        }
    } else {
        for (i = 0; i < selectedZones.length; i++) {
            name = "{ERROR: NOT FOUND}";
            for (j = 0; j < zonesList.length; j++) {
                if (selectedZones[i] === zonesList[j].id) {
                    if (zonesList[j].name !== "") {
                        name = zonesList[j].name;
                    } else {
                        name = zonesList[j].id;
                    }
                    break;
                }
            }
            if (isEditable) {
                listedZoneInner += "<tr><td class='zoneItem'>" + name + "</td><td><a href='#' onClick='removeZoneFromZonesSelection(" + '"' + propertyId + '"' + ", " + '"' + selectedZones[i] + '"' + ");' >";
                listedZoneInner += "<img src='../../../html/css/img/remove_icon.png'></a></td></tr>";
            } else {
                listedZoneInner += "<tr><td class='zoneItem'>" + name + "</td><td>&nbsp;</td></tr>";
            }
        }
    }
    listedZoneInner += "</table>";
    document.getElementById("selected-zones-" + propertyId).innerHTML = listedZoneInner; 
    if (isEditable) {
        document.getElementById("multiZoneSelTools-" + propertyId).style.display = "block";
    } else {
        document.getElementById("multiZoneSelTools-" + propertyId).style.display = "none";
    }
}

function createZonesSelection(property, elProperty) {
    let elementID = property.elementID;
    requestZoneList();
    elProperty.className = "multipleZonesSelection";
    let elInput = document.createElement('input');
    elInput.setAttribute("id", elementID);
    elInput.setAttribute("type", "hidden");
    elInput.className = "hiddenMultiZonesSelection";

    let elZonesSelector = document.createElement('div');
    elZonesSelector.setAttribute("id", "zones-selector-" + elementID);

    let elMultiDiff = document.createElement('span');
    elMultiDiff.className = "multi-diff";

    elProperty.appendChild(elInput);
    elProperty.appendChild(elZonesSelector);
    elProperty.appendChild(elMultiDiff);

    return elInput;
}

function setZonesSelectionData(element, isEditable) {
    let zoneSelectorContainer = document.getElementById("zones-selector-" + element.id);
    let zoneSelector = "<div class='multiZoneSelToolbar' id='multiZoneSelTools-" + element.id + "'><select class = 'zoneSelect' id='zones-select-" + element.id + "' >";
    let i, name;
    for (i = 0; i < zonesList.length; i++) {
        if (zonesList[i].name === "") {
            name = zonesList[i].id;
        } else {
            name = zonesList[i].name;
        }
        zoneSelector += "<option value='" + zonesList[i].id + "'>" + name + "</option>";
    }   
    zoneSelector += "</select>&nbsp;<a href='#' id='zones-select-add-" + element.id + "' onClick='addZoneToZonesSelection(" + '"' + element.id + '"' + ");' >";
    zoneSelector += "<img style='vertical-align:top' src='../../../html/css/img/add_icon.png'></a></div>";
    zoneSelector += "<div class='selected-zone-container' id='selected-zones-" + element.id + "'></div>";
    zoneSelectorContainer.innerHTML = zoneSelector;
    displaySelectedZones(element.id, isEditable);
}

function updateAllZoneSelect() {
    let allZoneSelects = document.querySelectorAll(".zoneSelect");
    let i, j, name, propId;
    for (i = 0; i < allZoneSelects.length; i++) {
        allZoneSelects[i].options.length = 0;
        for (j = 0; j < zonesList.length; j++) {
            if (zonesList[j].name === "") {
                name = zonesList[j].id;
            } else {
                name = zonesList[j].name;
            }
            allZoneSelects[i].options[j] = new Option(name, zonesList[j].id, false , false);
        }
        propId = allZoneSelects[i].id.replace("zones-select-", "");
        if (document.getElementById("multiZoneSelTools-" + propId).style.display === "block") {
            displaySelectedZones(propId, true);
        } else {
            displaySelectedZones(propId, false);
        }
    }
}

/**
 * MATERIAL TARGET FUNCTIONS
 */

function requestMaterialTarget() {
    EventBridge.emitWebEvent(JSON.stringify({
        type: 'materialTargetRequest',
        entityID: getFirstSelectedID(),
    }));
}

function setMaterialTargetData(materialTargetData) {
    let elDivOptions = getPropertyInputElement("parentMaterialName");
    resetDynamicMultiselectProperty(elDivOptions);

    if (materialTargetData === undefined) {
        return;
    }

    elDivOptions.firstChild.style.display = "none"; // hide "No Options" text
    elDivOptions.parentNode.lastChild.style.display = null; // show Select/Clear all buttons

    let numMeshes = materialTargetData.numMeshes;
    for (let i = 0; i < numMeshes; ++i) {
        addMaterialTarget(elDivOptions, i, false);
    }

    let materialNames = materialTargetData.materialNames;
    let materialNamesAdded = [];
    for (let i = 0; i < materialNames.length; ++i) {
        let materialName = materialNames[i];
        if (materialNamesAdded.indexOf(materialName) === -1) {
            addMaterialTarget(elDivOptions, materialName, true);
            materialNamesAdded.push(materialName);
        }
    }

    materialTargetPropertyUpdate(elDivOptions.propertyValue);
}

function addMaterialTarget(elDivOptions, targetID, isMaterialName) {
    let elementID = elDivOptions.getAttribute("id");
    elementID += isMaterialName ? "-material-" : "-mesh-";
    elementID += targetID;

    let elDiv = document.createElement('div');
    elDiv.className = "materialTargetDiv";
    elDiv.onclick = onToggleMaterialTarget;
    elDivOptions.appendChild(elDiv);

    let elInput = document.createElement('input');
    elInput.className = "materialTargetInput";
    elInput.setAttribute("type", "checkbox");
    elInput.setAttribute("id", elementID);
    elInput.setAttribute("targetID", targetID);
    elInput.setAttribute("isMaterialName", isMaterialName);
    elDiv.appendChild(elInput);

    let elLabel = document.createElement('label');
    elLabel.setAttribute("for", elementID);
    elLabel.innerText = isMaterialName ? "Material " + targetID : "Mesh Index " + targetID;
    elDiv.appendChild(elLabel);

    return elDiv;
}

function onToggleMaterialTarget(event) {
    let elTarget = event.target;
    if (elTarget instanceof HTMLInputElement) {
        sendMaterialTargetProperty();
    }
    event.stopPropagation();
}

function setAllMaterialTargetInputs(checked) {
    let elDivOptions = getPropertyInputElement("parentMaterialName");   
    let elInputs = elDivOptions.getElementsByClassName("materialTargetInput");
    for (let i = 0; i < elInputs.length; ++i) {
        elInputs[i].checked = checked;
    }
}

function selectAllMaterialTarget() {
    setAllMaterialTargetInputs(true);
    sendMaterialTargetProperty();
}

function clearAllMaterialTarget() {
    setAllMaterialTargetInputs(false);
    sendMaterialTargetProperty();
}

function sendMaterialTargetProperty() {
    let elDivOptions = getPropertyInputElement("parentMaterialName");   
    let elInputs = elDivOptions.getElementsByClassName("materialTargetInput");

    let materialTargetList = [];
    for (let i = 0; i < elInputs.length; ++i) {
        let elInput = elInputs[i];
        if (elInput.checked) {
            let targetID = elInput.getAttribute("targetID");
            if (elInput.getAttribute("isMaterialName") === "true") {
                materialTargetList.push(MATERIAL_PREFIX_STRING + targetID);
            } else {
                materialTargetList.push(targetID);
            }
        }
    }

    let propertyValue = materialTargetList.join(",");
    if (propertyValue.length > 1) {
        propertyValue = "[" + propertyValue + "]";
    }

    updateProperty("parentMaterialName", propertyValue, false);
}

function materialTargetPropertyUpdate(propertyValue) {
    let elDivOptions = getPropertyInputElement("parentMaterialName");
    let elInputs = elDivOptions.getElementsByClassName("materialTargetInput");

    if (propertyValue.startsWith('[')) {
        propertyValue = propertyValue.substring(1, propertyValue.length);
    }
    if (propertyValue.endsWith(']')) {
        propertyValue = propertyValue.substring(0, propertyValue.length - 1);
    }

    let materialTargets = propertyValue.split(",");
    for (let i = 0; i < elInputs.length; ++i) {
        let elInput = elInputs[i];
        let targetID = elInput.getAttribute("targetID");
        let materialTargetName = targetID;
        if (elInput.getAttribute("isMaterialName") === "true") {
            materialTargetName = MATERIAL_PREFIX_STRING + targetID;
        }
        elInput.checked = materialTargets.indexOf(materialTargetName) >= 0;
    }

    elDivOptions.propertyValue = propertyValue;
}

function roundAndFixNumber(number, propertyData) {
    let result = number;
    if (propertyData.round !== undefined) {
        result = Math.round(result * propertyData.round) / propertyData.round;
    }
    if (propertyData.decimals !== undefined) {
        return result.toFixed(propertyData.decimals)
    }
    return result;
}

function applyInputNumberPropertyModifiers(number, propertyData) {
    const multiplier = propertyData.multiplier !== undefined ? propertyData.multiplier : 1;
    return roundAndFixNumber(number / multiplier, propertyData);
}

function applyOutputNumberPropertyModifiers(number, propertyData) {
    const multiplier = propertyData.multiplier !== undefined ? propertyData.multiplier : 1;
    return roundAndFixNumber(number * multiplier, propertyData);
}

const areSetsEqual = (a, b) => a.size === b.size && [...a].every(value => b.has(value));


function handleEntitySelectionUpdate(selections, isPropertiesToolUpdate) {
    const previouslySelectedEntityIDs = selectedEntityIDs;
    currentSelections = selections;
    selectedEntityIDs = new Set(selections.map(selection => selection.id));
    const multipleSelections = currentSelections.length > 1;
    const hasSelectedEntityChanged = !areSetsEqual(selectedEntityIDs, previouslySelectedEntityIDs);
    
    requestZoneList();
    
    if (selections.length === 0) {
        deleteJSONEditor();
        deleteJSONMaterialEditor();

        resetProperties();
        showGroupsForType("None");

        let elIcon = properties.type.elSpan;
        elIcon.innerText = NO_SELECTION;
        elIcon.style.display = 'inline-block';

        getPropertyInputElement("userData").value = "";
        showUserDataTextArea();
        showSaveUserDataButton();
        showNewJSONEditorButton();

        getPropertyInputElement("materialData").value = "";
        showMaterialDataTextArea();
        showSaveMaterialDataButton();
        showNewJSONMaterialEditorButton();

        disableProperties();
    } else {
        if (!isPropertiesToolUpdate && !hasSelectedEntityChanged && document.hasFocus()) {
            // in case the selection has not changed and we still have focus on the properties page,
            // we will ignore the event.
            return;
        }

        if (hasSelectedEntityChanged) {
            if (!multipleSelections) {
                resetServerScriptStatus();
            }
        }

        const doSelectElement = !hasSelectedEntityChanged;

        // Get unique entity types, and convert the types Sphere and Box to Shape
        const shapeTypes = ["Sphere", "Box"];
        const entityTypes = [...new Set(currentSelections.map(a =>
            shapeTypes.includes(a.properties.type) ? "Shape" : a.properties.type))];

        const shownGroups = getGroupsForTypes(entityTypes);
        showGroupsForTypes(entityTypes);
        showOnTheSamePage(entityTypes);

        const lockedMultiValue = getMultiplePropertyValue('locked');

        if (lockedMultiValue.isMultiDiffValue || lockedMultiValue.value) {
            disableProperties();
            getPropertyInputElement('locked').removeAttribute('disabled');
        } else {
            enableProperties();
            disableSaveUserDataButton();
            disableSaveMaterialDataButton();
        }

        const certificateIDMultiValue = getMultiplePropertyValue('certificateID');
        const hasCertifiedInSelection = certificateIDMultiValue.isMultiDiffValue || certificateIDMultiValue.value !== "";

        Object.entries(properties).forEach(function([propertyID, property]) {
            const propertyData = property.data;
            const propertyName = property.name;
            let propertyMultiValue = getMultiplePropertyValue(propertyName);
            let isMultiDiffValue = propertyMultiValue.isMultiDiffValue;
            let propertyValue = propertyMultiValue.value;

            if (propertyData.selectionVisibility !== undefined) {
                let visibility = propertyData.selectionVisibility;
                let propertyVisible = true;
                if (!multipleSelections) {
                    propertyVisible = isFlagSet(visibility, PROPERTY_SELECTION_VISIBILITY.SINGLE_SELECTION);
                } else if (isMultiDiffValue) {
                    propertyVisible = isFlagSet(visibility, PROPERTY_SELECTION_VISIBILITY.MULTI_DIFF_SELECTIONS);
                } else {
                    propertyVisible = isFlagSet(visibility, PROPERTY_SELECTION_VISIBILITY.MULTIPLE_SELECTIONS);
                }
                setPropertyVisibility(property, propertyVisible);
            }

            const isSubProperty = propertyData.subPropertyOf !== undefined;
            if (propertyValue === undefined && !isMultiDiffValue && !isSubProperty) {
                return;
            }

            if (!shownGroups.includes(property.group_id)) {
                const WANT_DEBUG_SHOW_HIDDEN_FROM_GROUPS = false;
                if (WANT_DEBUG_SHOW_HIDDEN_FROM_GROUPS) {
                    console.log("Skipping property " + property.data.label + " [" + property.name +
                        "] from hidden group " + property.group_id);
                }
                return;
            }

            if (propertyData.hideIfCertified && hasCertifiedInSelection) {
                propertyValue = "** Certified **";
                property.elInput.disabled = true;
            }

            if (propertyName === "type") {
                propertyValue = entityTypes.length > 1 ?  "Multiple" : propertyMultiValue.values[0];
            }

            switch (propertyData.type) {
                case 'string': {
                    if (isMultiDiffValue) {
                        if (propertyData.readOnly && propertyData.multiDisplayMode
                            && propertyData.multiDisplayMode === PROPERTY_MULTI_DISPLAY_MODE.COMMA_SEPARATED_VALUES) {
                            property.elInput.value = propertyMultiValue.values.join(", ");
                        } else {
                            property.elInput.classList.add('multi-diff');
                            property.elInput.value = "";
                        }
                    } else {
                        property.elInput.classList.remove('multi-diff');
                        property.elInput.value = propertyValue;
                    }
                    break;
                }
                case 'bool': {
                    const inverse = propertyData.inverse !== undefined ? propertyData.inverse : false;
                    if (isSubProperty) {
                        let subPropertyMultiValue = getMultiplePropertyValue(propertyData.subPropertyOf);
                        let propertyValue = subPropertyMultiValue.value;
                        isMultiDiffValue = subPropertyMultiValue.isMultiDiffValue;
                        if (isMultiDiffValue) {
                            let detailedSubProperty = getDetailedSubPropertyMPVDiff(subPropertyMultiValue, propertyName);
                            property.elInput.checked = detailedSubProperty.isChecked;
                            property.elInput.classList.toggle('multi-diff', detailedSubProperty.isMultiDiff);
                        } else {
                            let subProperties = propertyValue.split(",");
                            let subPropertyValue = subProperties.indexOf(propertyName) > -1;
                            property.elInput.checked = inverse ? !subPropertyValue : subPropertyValue;
                            property.elInput.classList.remove('multi-diff');
                        }

                    } else {
                        if (isMultiDiffValue) {
                            property.elInput.checked = false;
                        } else {
                            property.elInput.checked = inverse ? !propertyValue : propertyValue;
                        }
                        property.elInput.classList.toggle('multi-diff', isMultiDiffValue);
                    }

                    break;
                }
                case 'number': {
                    property.elInput.value = isMultiDiffValue ? "" : propertyValue;
                    property.elInput.classList.toggle('multi-diff', isMultiDiffValue);
                    break;
                }
                case 'number-draggable': {
                    let detailedNumberDiff = getDetailedNumberMPVDiff(propertyMultiValue, propertyData);
                    property.elNumber.setValue(detailedNumberDiff.averagePerPropertyComponent[0], detailedNumberDiff.propertyComponentDiff[0]);
                    break;
                }
                case 'rect': {
                    let detailedNumberDiff = getDetailedNumberMPVDiff(propertyMultiValue, propertyData);
                    property.elNumberX.setValue(detailedNumberDiff.averagePerPropertyComponent.x, detailedNumberDiff.propertyComponentDiff.x);
                    property.elNumberY.setValue(detailedNumberDiff.averagePerPropertyComponent.y, detailedNumberDiff.propertyComponentDiff.y);
                    property.elNumberWidth.setValue(detailedNumberDiff.averagePerPropertyComponent.width, detailedNumberDiff.propertyComponentDiff.width);
                    property.elNumberHeight.setValue(detailedNumberDiff.averagePerPropertyComponent.height, detailedNumberDiff.propertyComponentDiff.height);
                    break;
                }
                case 'vec3':
                case 'vec2': {
                    let detailedNumberDiff = getDetailedNumberMPVDiff(propertyMultiValue, propertyData);
                    property.elNumberX.setValue(detailedNumberDiff.averagePerPropertyComponent.x, detailedNumberDiff.propertyComponentDiff.x);
                    property.elNumberY.setValue(detailedNumberDiff.averagePerPropertyComponent.y, detailedNumberDiff.propertyComponentDiff.y);
                    if (property.elNumberZ !== undefined) {
                        property.elNumberZ.setValue(detailedNumberDiff.averagePerPropertyComponent.z, detailedNumberDiff.propertyComponentDiff.z);
                    }
                    break;
                }
                case 'color': {
                    let displayColor = propertyMultiValue.isMultiDiffValue ? propertyMultiValue.values[0] : propertyValue;
                    property.elColorPicker.style.backgroundColor = "rgb(" + displayColor.red + "," +
                        displayColor.green + "," +
                        displayColor.blue + ")";
                    property.elColorPicker.classList.toggle('multi-diff', propertyMultiValue.isMultiDiffValue);

                    if (hasSelectedEntityChanged && $(property.elColorPicker).attr('active') === 'true') {
                        // Set the color picker inactive before setting the color,
                        // otherwise an update will be sent directly after setting it here.
                        $(property.elColorPicker).attr('active', 'false');
                        colorPickers['#' + property.elementID].colpickSetColor({
                            "r": displayColor.red,
                            "g": displayColor.green,
                            "b": displayColor.blue
                        });
                        $(property.elColorPicker).attr('active', 'true');
                    }

                    property.elNumberR.setValue(displayColor.red);
                    property.elNumberG.setValue(displayColor.green);
                    property.elNumberB.setValue(displayColor.blue);
                    break;
                }
                case 'vec3rgb': {
                    let detailedNumberDiff = getDetailedNumberMPVDiff(propertyMultiValue, propertyData);
                    property.elNumberR.setValue(detailedNumberDiff.averagePerPropertyComponent.red, detailedNumberDiff.propertyComponentDiff.red);
                    property.elNumberG.setValue(detailedNumberDiff.averagePerPropertyComponent.green, detailedNumberDiff.propertyComponentDiff.green);
                    property.elNumberB.setValue(detailedNumberDiff.averagePerPropertyComponent.blue, detailedNumberDiff.propertyComponentDiff.blue);
                    break;
                }         
                case 'dropdown': {
                    property.elInput.classList.toggle('multi-diff', isMultiDiffValue);
                    property.elInput.value = isMultiDiffValue ? "" : propertyValue;
                    setDropdownText(property.elInput);
                    break;
                }
                case 'textarea': {
                    property.elInput.value = propertyValue;
                    setTextareaScrolling(property.elInput);
                    break;
                }
                case 'multipleZonesSelection': {
                    property.elInput.value =  JSON.stringify(propertyValue);
                    if (lockedMultiValue.isMultiDiffValue || lockedMultiValue.value) {
                        setZonesSelectionData(property.elInput, false);
                    } else {
                        setZonesSelectionData(property.elInput, true);
                    }
                    break;
                }
                case 'icon': {
                    property.elSpan.innerHTML = propertyData.icons[propertyValue];
                    property.elSpan.style.display = "inline-block";
                    break;
                }
                case 'texture': {
                    property.elInput.value = isMultiDiffValue ? "" : propertyValue;
                    property.elInput.classList.toggle('multi-diff', isMultiDiffValue);
                    if (isMultiDiffValue) {
                        property.elInput.setMultipleValues();
                    } else {
                        property.elInput.imageLoad(property.elInput.value);
                    }
                    break;
                }
                case 'dynamic-multiselect': {
                    if (!isMultiDiffValue && property.data.propertyUpdate) {
                        property.data.propertyUpdate(propertyValue);
                    }
                    break;
                }
            }

            let showPropertyRules = property.showPropertyRules;
            if (showPropertyRules !== undefined) {
                for (let propertyToShow in showPropertyRules) {
                    let showIfThisPropertyValue = showPropertyRules[propertyToShow];
                    let show = String(propertyValue) === String(showIfThisPropertyValue);
                    showPropertyElement(propertyToShow, show);
                }
            }
        });

        updateVisibleSpaceModeProperties();

        let userDataMultiValue = getMultiplePropertyValue("userData");
        let userDataTextArea = getPropertyInputElement("userData");
        let json = null;
        if (!userDataMultiValue.isMultiDiffValue) {
            try {
                json = JSON.parse(userDataMultiValue.value);
            } catch (e) {

            }
        }
        if (json !== null && !lockedMultiValue.isMultiDiffValue && !lockedMultiValue.value) {
            if (editor === null) {
                createJSONEditor();
            }
            userDataTextArea.classList.remove('multi-diff');
            setEditorJSON(json);
            showSaveUserDataButton();
            hideUserDataTextArea();
            hideNewJSONEditorButton();
            hideUserDataSaved();
        } else {
            // normal text
            deleteJSONEditor();
            userDataTextArea.classList.toggle('multi-diff', userDataMultiValue.isMultiDiffValue);
            userDataTextArea.value = userDataMultiValue.isMultiDiffValue ? "" : userDataMultiValue.value;

            showUserDataTextArea();
            showNewJSONEditorButton();
            hideSaveUserDataButton();
            hideUserDataSaved();
        }

        let materialDataMultiValue = getMultiplePropertyValue("materialData");
        let materialDataTextArea = getPropertyInputElement("materialData");
        let materialJson = null;
        if (!materialDataMultiValue.isMultiDiffValue) {
            try {
                materialJson = JSON.parse(materialDataMultiValue.value);
            } catch (e) {

            }
        }
        if (materialJson !== null && !lockedMultiValue.isMultiDiffValue && !lockedMultiValue.value) {
            if (materialEditor === null) {
                createJSONMaterialEditor();
            }
            materialDataTextArea.classList.remove('multi-diff');
            setMaterialEditorJSON(materialJson);
            showSaveMaterialDataButton();
            hideMaterialDataTextArea();
            hideNewJSONMaterialEditorButton();
            hideMaterialDataSaved();
        } else {
            // normal text
            deleteJSONMaterialEditor();
            materialDataTextArea.classList.toggle('multi-diff', materialDataMultiValue.isMultiDiffValue);
            materialDataTextArea.value = materialDataMultiValue.isMultiDiffValue ? "" :  materialDataMultiValue.value;
            showMaterialDataTextArea();
            showNewJSONMaterialEditorButton();
            hideSaveMaterialDataButton();
            hideMaterialDataSaved();
        }

        if (hasSelectedEntityChanged && selections.length === 1 && entityTypes[0] === "Material") {
            requestMaterialTarget();
        }

        let activeElement = document.activeElement;
        if (doSelectElement && typeof activeElement.select !== "undefined") {
            activeElement.select();
        }
    }
}

function loaded() {
    openEventBridge(function() {
        let elPropertiesList = document.getElementById("properties-pages");
        let elTabs = document.getElementById("tabs");

        GROUPS.forEach(function(group) {
            let elGroup;

            elGroup = document.createElement('div');
            elGroup.className = 'section ' + "major";
            elGroup.setAttribute("id", "properties-" + group.id);
            elPropertiesList.appendChild(elGroup);

            if (group.label !== undefined) {
                let elLegend = document.createElement('div');
                elLegend.className = "tab-section-header";
                elLegend.appendChild(createElementFromHTML(`<div class="labelTabHeader">${group.label}</div>`));
                elGroup.appendChild(elLegend);
                elTabs.appendChild(createElementFromHTML('<button id="tab-'+ group.id +'" onclick="showPage(' + "'" + group.id  + "'" + ');"><img src="tabs/'+ group.id +'.png"></button>'));
            }

            group.properties.forEach(function(propertyData) {
                let propertyType = propertyData.type;
                let propertyID = propertyData.propertyID;
                let propertyName = propertyData.propertyName !== undefined ? propertyData.propertyName : propertyID;
                let propertySpaceMode = propertyData.spaceMode !== undefined ? propertyData.spaceMode : PROPERTY_SPACE_MODE.ALL;
                let propertyElementID = "property-" + propertyID;
                propertyElementID = propertyElementID.replace('.', '-');

                let elContainer, elLabel;

                if (propertyData.replaceID === undefined) {
                    // Create subheader, or create new property and append it.
                    if (propertyType === "sub-header") {
                        elContainer = createElementFromHTML(
                            `<div class="sub-section-header legend">${propertyData.label}</div>`);
                    } else {
                        elContainer = document.createElement('div');
                        elContainer.setAttribute("id", "div-" + propertyElementID);
                        elContainer.className = 'property container';
                    }

                    if (group.twoColumn && propertyData.column !== undefined && propertyData.column !== -1) {
                        let columnName = group.id + "column" + propertyData.column;
                        let elColumn = document.getElementById(columnName);
                        if (!elColumn) {
                            let columnDivName = group.id + "columnDiv";
                            let elColumnDiv = document.getElementById(columnDivName);
                            if (!elColumnDiv) {
                                elColumnDiv = document.createElement('div');
                                elColumnDiv.className = "two-column";
                                elColumnDiv.setAttribute("id", group.id + "columnDiv");
                                elGroup.appendChild(elColumnDiv);
                            }
                            elColumn = document.createElement('fieldset');
                            elColumn.className = "column";
                            elColumn.setAttribute("id", columnName);
                            elColumnDiv.appendChild(elColumn);
                        }
                        elColumn.appendChild(elContainer);
                    } else {
                        elGroup.appendChild(elContainer);
                    }

                    let labelText = propertyData.label !== undefined ? propertyData.label : "";
                    let className = '';
                    if (propertyData.indentedLabel || propertyData.showPropertyRule !== undefined) {
                        className = 'indented';
                    }
                    elLabel = createElementFromHTML(
                        `<label><span class="${className}">${labelText}</span></label>`);
                    elContainer.appendChild(elLabel);
                } else {
                    elContainer = document.getElementById(propertyData.replaceID);
                }

                if (elLabel) {
                    createAppTooltip.registerTooltipElement(elLabel.childNodes[0], propertyID, propertyName);
                }

                let elProperty = createElementFromHTML('<div style="width: 100%;"></div>');
                elContainer.appendChild(elProperty);

                if (propertyType === 'triple') {
                    elProperty.className = 'flex-row';
                    for (let i = 0; i < propertyData.properties.length; ++i) {
                        let innerPropertyData = propertyData.properties[i];

                        let elWrapper = createElementFromHTML('<div class="triple-item"></div>');
                        elProperty.appendChild(elWrapper);

                        let propertyID = innerPropertyData.propertyID;               
                        let propertyName = innerPropertyData.propertyName !== undefined ? innerPropertyData.propertyName : propertyID;
                        let propertyElementID = "property-" + propertyID;
                        propertyElementID = propertyElementID.replace('.', '-');

                        let property = createProperty(innerPropertyData, propertyElementID, propertyName, propertyID, elWrapper);
                        property.isParticleProperty = group.id.includes("particles");
                        property.elContainer = elContainer;
                        property.spaceMode = propertySpaceMode;
                        property.group_id = group.id;

                        let elLabel = createElementFromHTML(`<div class="triple-label">${innerPropertyData.label}</div>`);
                        createAppTooltip.registerTooltipElement(elLabel, propertyID, propertyName);

                        elWrapper.appendChild(elLabel);
                        
                        if (property.type !== 'placeholder') {
                            properties[propertyID] = property;
                        }
                        if (innerPropertyData.type === 'number' || innerPropertyData.type === 'number-draggable') {
                            propertyRangeRequests.push(propertyID);
                        }
                    }
                } else {
                    let property = createProperty(propertyData, propertyElementID, propertyName, propertyID, elProperty);
                    property.isParticleProperty = group.id.includes("particles");
                    property.elContainer = elContainer;
                    property.spaceMode = propertySpaceMode;
                    property.group_id = group.id;

                    if (property.type !== 'placeholder') {
                        properties[propertyID] = property;
                    }
                    if (propertyData.type === 'number' || propertyData.type === 'number-draggable' || 
                        propertyData.type === 'vec2' || propertyData.type === 'vec3' ||
                        propertyData.type === 'rect' || propertyData.type === 'vec3rgb') {
                        propertyRangeRequests.push(propertyID);
                    }

                    let showPropertyRule = propertyData.showPropertyRule;
                    if (showPropertyRule !== undefined) {
                        let dependentProperty = Object.keys(showPropertyRule)[0];
                        let dependentPropertyValue = showPropertyRule[dependentProperty];
                        if (properties[dependentProperty] === undefined) {
                            properties[dependentProperty] = {};
                        }
                        if (properties[dependentProperty].showPropertyRules === undefined) {
                            properties[dependentProperty].showPropertyRules = {};
                        }
                        properties[dependentProperty].showPropertyRules[propertyID] = dependentPropertyValue;
                    }
                }
            });

            elGroups[group.id] = elGroup;
        });

        updateVisibleSpaceModeProperties();

        if (window.EventBridge !== undefined) {
            EventBridge.scriptEventReceived.connect(function(data) {
                data = JSON.parse(data);
                if (data.type === "server_script_status" && selectedEntityIDs.size === 1) {
                    let elServerScriptError = document.getElementById("property-serverScripts-error");
                    let elServerScriptStatus = document.getElementById("property-serverScripts-status");
                    elServerScriptError.value = data.errorInfo;
                    // If we just set elServerScriptError's display to block or none, we still end up with
                    // it's parent contributing 21px bottom padding even when elServerScriptError is display:none.
                    // So set it's parent to block or none
                    elServerScriptError.parentElement.style.display = data.errorInfo ? "block" : "none";
                    if (data.statusRetrieved === false) {
                        elServerScriptStatus.innerText = "Failed to retrieve status";
                    } else if (data.isRunning) {
                        elServerScriptStatus.innerText = ENTITY_SCRIPT_STATUS[data.status] || data.status;
                    } else {
                        elServerScriptStatus.innerText = NOT_RUNNING_SCRIPT_STATUS;
                    }
                } else if (data.type === "update" && data.selections) {
                    if (data.spaceMode !== undefined) {
                        currentSpaceMode = data.spaceMode === "local" ? PROPERTY_SPACE_MODE.LOCAL : PROPERTY_SPACE_MODE.WORLD;
                    }
                    handleEntitySelectionUpdate(data.selections, data.isPropertiesToolUpdate);
                } else if (data.type === 'tooltipsReply') {
                    createAppTooltip.setIsEnabled(!data.hmdActive);
                    createAppTooltip.setTooltipData(data.tooltips);
                } else if (data.type === 'hmdActiveChanged') {
                    createAppTooltip.setIsEnabled(!data.hmdActive);
                } else if (data.type === 'setSpaceMode') {
                    currentSpaceMode = data.spaceMode === "local" ? PROPERTY_SPACE_MODE.LOCAL : PROPERTY_SPACE_MODE.WORLD;
                    updateVisibleSpaceModeProperties();
                } else if (data.type === 'propertyRangeReply') {
                    let propertyRanges = data.propertyRanges;
                    for (let property in propertyRanges) {
                        let propertyRange = propertyRanges[property];
                        if (propertyRange !== undefined) {
                            let propertyData = properties[property].data;
                            let multiplier = propertyData.multiplier;
                            if (propertyData.min === undefined && propertyRange.minimum !== "") {
                                propertyData.min = propertyRange.minimum;
                                if (multiplier !== undefined) {
                                    propertyData.min /= multiplier;
                                }
                            }
                            if (propertyData.max === undefined && propertyRange.maximum !== "") {
                                propertyData.max = propertyRange.maximum;
                                if (multiplier !== undefined) {
                                    propertyData.max /= multiplier;
                                }
                            }
                            switch (propertyData.type) {
                                case 'number':
                                    updateNumberMinMax(properties[property]);
                                    break;
                                case 'number-draggable':
                                    updateNumberDraggableMinMax(properties[property]);
                                    break;
                                case 'vec3':
                                case 'vec2':
                                    updateVectorMinMax(properties[property]);
                                    break;
                                case 'vec3rgb':
                                    updateVectorMinMax(properties[property]);
                                    break;                                    
                                case 'rect':
                                    updateRectMinMax(properties[property]);
                                    break;
                            }
                        }
                    }
                } else if (data.type === 'materialTargetReply') {
                    if (data.entityID === getFirstSelectedID()) {
                        setMaterialTargetData(data.materialTargetData);
                    }
                } else if (data.type === 'zoneListRequest') {
                    zonesList = data.zones;
                    updateAllZoneSelect();
                }
            });

            // Request tooltips and property ranges as soon as we can process a reply:
            EventBridge.emitWebEvent(JSON.stringify({ type: 'tooltipsRequest' }));
            EventBridge.emitWebEvent(JSON.stringify({ type: 'propertyRangeRequest', properties: propertyRangeRequests }));
        }

        // Server Script Status
        let elServerScriptStatusOuter = document.getElementById('div-property-serverScriptStatus');
        let elServerScriptStatusContainer = document.getElementById('div-property-serverScriptStatus').childNodes[1];
        let serverScriptStatusElementID = 'property-serverScripts-status';
        createAppTooltip.registerTooltipElement(elServerScriptStatusOuter.childNodes[0], "serverScriptsStatus");
        let elServerScriptStatus = document.createElement('div');
        elServerScriptStatus.setAttribute("id", serverScriptStatusElementID);
        elServerScriptStatusContainer.appendChild(elServerScriptStatus);

        // Server Script Error
        let elServerScripts = getPropertyInputElement("serverScripts");
        let elDiv = document.createElement('div');
        elDiv.className = "property";
        let elServerScriptError = document.createElement('textarea');
        let serverScriptErrorElementID = 'property-serverScripts-error';
        elServerScriptError.setAttribute("id", serverScriptErrorElementID);
        elDiv.appendChild(elServerScriptError);
        elServerScriptStatusContainer.appendChild(elDiv);

        let elScript = getPropertyInputElement("script");
        elScript.parentNode.className = "url refresh";
        elServerScripts.parentNode.className = "url refresh";

        // User Data
        let userDataProperty = properties["userData"];
        let elUserData = userDataProperty.elInput;
        let userDataElementID = userDataProperty.elementID;
        elDiv = elUserData.parentNode;
        let elUserDataEditor = document.createElement('div');
        elUserDataEditor.setAttribute("id", userDataElementID + "-editor");
        let elUserDataEditorStatus = document.createElement('div');
        elUserDataEditorStatus.setAttribute("id", userDataElementID + "-editorStatus");
        let elUserDataSaved = document.createElement('span');
        elUserDataSaved.setAttribute("id", userDataElementID + "-saved");
        elUserDataSaved.innerText = "Saved!";
        elDiv.childNodes[JSON_EDITOR_ROW_DIV_INDEX].appendChild(elUserDataSaved);
        elDiv.insertBefore(elUserDataEditor, elUserData);
        elDiv.insertBefore(elUserDataEditorStatus, elUserData);

        // Material Data
        let materialDataProperty = properties["materialData"];
        let elMaterialData = materialDataProperty.elInput;
        let materialDataElementID = materialDataProperty.elementID;
        elDiv = elMaterialData.parentNode;
        let elMaterialDataEditor = document.createElement('div');
        elMaterialDataEditor.setAttribute("id", materialDataElementID + "-editor");
        let elMaterialDataEditorStatus = document.createElement('div');
        elMaterialDataEditorStatus.setAttribute("id", materialDataElementID + "-editorStatus");
        let elMaterialDataSaved = document.createElement('span');
        elMaterialDataSaved.setAttribute("id", materialDataElementID + "-saved");
        elMaterialDataSaved.innerText = "Saved!";
        elDiv.childNodes[JSON_EDITOR_ROW_DIV_INDEX].appendChild(elMaterialDataSaved);
        elDiv.insertBefore(elMaterialDataEditor, elMaterialData);
        elDiv.insertBefore(elMaterialDataEditorStatus, elMaterialData);

        // Textarea scrollbars
        let elTextareas = document.getElementsByTagName("TEXTAREA");

        let textareaOnChangeEvent = function(event) {
            setTextareaScrolling(event.target);
        };

        for (let textAreaIndex = 0, numTextAreas = elTextareas.length; textAreaIndex < numTextAreas; ++textAreaIndex) {
            let curTextAreaElement = elTextareas[textAreaIndex];
            setTextareaScrolling(curTextAreaElement);
            curTextAreaElement.addEventListener("input", textareaOnChangeEvent, false);
            curTextAreaElement.addEventListener("change", textareaOnChangeEvent, false);
            /* FIXME: Detect and update textarea scrolling attribute on resize. Unfortunately textarea doesn't have a resize
            event; mouseup is a partial stand-in but doesn't handle resizing if mouse moves outside textarea rectangle. */
            curTextAreaElement.addEventListener("mouseup", textareaOnChangeEvent, false);
        }

        // Dropdowns
        // For each dropdown the following replacement is created in place of the original dropdown...
        // Structure created:
        //  <dl dropped="true/false">
        //      <dt name="?" id="?" value="?"><span>display text</span><span>carat</span></dt>
        //      <dd>
        //          <ul>
        //              <li value="??>display text</li>
        //              <li>...</li>
        //          </ul>
        //      </dd>
        //  </dl>
        let elDropdowns = document.getElementsByTagName("select");
        for (let dropDownIndex = 0; dropDownIndex < elDropdowns.length; ++dropDownIndex) {
            let elDropdown = elDropdowns[dropDownIndex];
            let options = elDropdown.getElementsByTagName("option");
            let selectedOption = 0;
            for (let optionIndex = 0; optionIndex < options.length; ++optionIndex) {
                if (options[optionIndex].getAttribute("selected") === "selected") {
                    selectedOption = optionIndex;
                    break;
                }
            }
            let div = elDropdown.parentNode;

            let dl = document.createElement("dl");
            div.appendChild(dl);

            let dt = document.createElement("dt");
            dt.name = elDropdown.name;
            dt.id = elDropdown.id;
            dt.addEventListener("click", toggleDropdown, true);
            dl.appendChild(dt);

            let elMultiDiff = document.createElement('span');
            elMultiDiff.className = "multi-diff";
            dl.appendChild(elMultiDiff);

            let span = document.createElement("span");
            span.setAttribute("value", options[selectedOption].value);
            span.textContent = options[selectedOption].firstChild.textContent;
            dt.appendChild(span);

            let spanCaratDown = document.createElement("span");
            spanCaratDown.textContent = "5"; // caratDn
            dt.appendChild(spanCaratDown);

            let dd = document.createElement("dd");
            dl.appendChild(dd);

            let ul = document.createElement("ul");
            dd.appendChild(ul);

            for (let listOptionIndex = 0; listOptionIndex < options.length; ++listOptionIndex) {
                let li = document.createElement("li");
                li.setAttribute("value", options[listOptionIndex].value);
                li.textContent = options[listOptionIndex].firstChild.textContent;
                li.addEventListener("click", setDropdownValue);
                ul.appendChild(li);
            }

            let propertyID = elDropdown.getAttribute("propertyID");
            let property = properties[propertyID];
            property.elInput = dt;
            dt.addEventListener('change', createEmitTextPropertyUpdateFunction(property));
        }

        document.addEventListener('click', function(ev) { closeAllDropdowns() }, true);
        
        elDropdowns = document.getElementsByTagName("select");
        while (elDropdowns.length > 0) {
            let el = elDropdowns[0];
            el.parentNode.removeChild(el);
            elDropdowns = document.getElementsByTagName("select");
        }

        const KEY_CODES = {
            BACKSPACE: 8,
            DELETE: 46
        };

        document.addEventListener("keyup", function (keyUpEvent) {
            const FILTERED_NODE_NAMES = ["INPUT", "TEXTAREA"];
            if (FILTERED_NODE_NAMES.includes(keyUpEvent.target.nodeName)) {
                return;
            }

            if (elUserDataEditor.contains(keyUpEvent.target) || elMaterialDataEditor.contains(keyUpEvent.target)) {
                return;
            }

            let {code, key, keyCode, altKey, ctrlKey, metaKey, shiftKey} = keyUpEvent;

            let controlKey = window.navigator.platform.startsWith("Mac") ? metaKey : ctrlKey;

            let keyCodeString;
            switch (keyCode) {
                case KEY_CODES.DELETE:
                    keyCodeString = "Delete";
                    break;
                case KEY_CODES.BACKSPACE:
                    keyCodeString = "Backspace";
                    break;
                default:
                    keyCodeString = String.fromCharCode(keyUpEvent.keyCode);
                    break;
            }

            EventBridge.emitWebEvent(JSON.stringify({
                type: 'keyUpEvent',
                keyUpEvent: {
                    code,
                    key,
                    keyCode,
                    keyCodeString,
                    altKey,
                    controlKey,
                    shiftKey,
                }
            }));
        }, false);

        window.onblur = function() {
            // Fake a change event
            let ev = document.createEvent("HTMLEvents");
            ev.initEvent("change", true, true);
            document.activeElement.dispatchEvent(ev);
        };

        // For input and textarea elements, select all of the text on focus
        let els = document.querySelectorAll("input, textarea");
        for (let i = 0; i < els.length; ++i) {
            els[i].onfocus = function (e) {
                e.target.select();
            };
        }

        bindAllNonJSONEditorElements();

        showGroupsForType("None");
        showPage("base");
        resetProperties();
        disableProperties();

    });

    augmentSpinButtons();
    disableDragDrop();

    // Disable right-click context menu which is not visible in the HMD and makes it seem like the app has locked
    document.addEventListener("contextmenu", function(event) {
        event.preventDefault();
    }, false);

    setTimeout(function() {
        EventBridge.emitWebEvent(JSON.stringify({ type: 'propertiesPageReady' }));
    }, 1000);
}

function showOnTheSamePage(entityType) {
    let numberOfTypes = entityType.length;
    let matchingType = 0;
    for (let i = 0; i < numberOfTypes; i++) {
        if (GROUPS_PER_TYPE[entityType[i]].includes(currentTab)) {
            matchingType = matchingType + 1;
        }
    }
    if (matchingType !== numberOfTypes) {
        currentTab = "base";
    }
    showPage(currentTab);
}

function showPage(id) {
    currentTab = id;
    Object.entries(elGroups).forEach(([groupKey, elGroup]) => {
        if (groupKey === id) {
            elGroup.style.display = "block";
            document.getElementById("tab-" + groupKey).style.backgroundColor = "#2E2E2E";
        } else {
            elGroup.style.display = "none";
            document.getElementById("tab-" + groupKey).style.backgroundColor = "#404040";
        }
    });
}
