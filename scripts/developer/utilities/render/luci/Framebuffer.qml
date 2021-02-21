//
//  Framebuffer.qml
//
//  Created by Sam Gateau on 4/18/2019
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.7

import "../../lib/prop" as Prop

Column {
    anchors.left: parent.left
    anchors.right: parent.right 

    id: framebuffer

    property var config: Render.getConfig("RenderMainView.DebugDeferredBuffer")

    function setDebugMode(mode) {
        framebuffer.config.enabled = (mode != 0);
        framebuffer.config.mode = mode;
    }

    Prop.PropEnum {
        label: "Debug Buffer"
        object: config
        property: "mode"
       // valueVar: 0
        enums: [
            "Off",
            "Depth",
            "Albedo",
            "Normal",
            "Roughness",
            "Metallic",
            "Emissive",
            "Unlit",
            "Occlusion",
            "Lightmap",
            "Scattering",
            "Lighting",
            "Shadow Cascade 0",
            "Shadow Cascade 1",
            "Shadow Cascade 2",
            "Shadow Cascade 3",
            "Shadow Cascade Indices",
            "Linear Depth",
            "Half Linear Depth",
            "Half Normal",
            "Mid Curvature",
            "Mid Normal",
            "Low Curvature",
            "Low Normal",
            "Curvature Occlusion",
            "Debug Scattering",
            "Ambient Occlusion",
            "Ambient Occlusion Blurred",
            "Ambient Occlusion Normal",
            "Velocity",
            "Antialiasing Intensity",
            "Custom",
                ]

        valueVarSetter: function (mode) { framebuffer.setDebugMode(mode) }
    } 
}