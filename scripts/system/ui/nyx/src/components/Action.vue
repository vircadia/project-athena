<!-- 
//
//  Action.vue
//
//  Created by Kalila L. on Feb 15 2021.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
-->

<template>
    <div>
        <div v-if="action.type === 'button'">
            <v-divider></v-divider>
            <v-list-item
                @click="triggeredMenuItem(action.type, action.name)"
                width="100%"
            >
                <v-list-item-content>
                    <v-list-item-title v-text="action.name"></v-list-item-title>
                </v-list-item-content>
            </v-list-item>
        </div>
        <div v-if="action.type === 'slider'">
            <v-divider></v-divider>
            <div width="100%">
                <v-list-item-content>
                    <v-slider
                        v-on:change="sliderUpdated(action.type, action.name, $event)"
                        :label="action.name"
                        :color="action.color"
                        :value="action.initialValue"
                        :min="action.minValue"
                        :max="action.maxValue"
                        :step="action.step"
                    ></v-slider>
                </v-list-item-content>
            </div>
        </div>
        <div v-if="action.type === 'textField'">
            <v-divider></v-divider>
            <div width="100%">
                <v-list-item-content>
                    <v-text-field
                        :label="action.name"
                        :value="action.initialValue"
                        :hint="action.hint"
                        v-model="textFields[action.name]"
                    >
                        <v-tooltip slot="append" left>
                            <template v-slot:activator="{ on, attrs }">
                                <v-icon
                                    @click="textFieldUpdated(action.type, action.name)" 
                                    v-bind="attrs"
                                    v-on="on"
                                    color="green"
                                >
                                    mdi-check
                                </v-icon>
                            </template>
                            <span>Apply</span>
                        </v-tooltip>
                    </v-text-field>
                </v-list-item-content>
            </div>
        </div>
        <div v-if="action.type === 'colorPicker'">
            <v-divider></v-divider>
            <div width="100%">
                <v-list-item-content>
                    <v-color-picker
                        @update:color="colorPickerUpdated(action.type, action.name, $event)"
                        :value="action.initialColor"
                    ></v-color-picker>
                </v-list-item-content>
            </div>
        </div>
    </div>
</template>

<script>
export default {
    name: 'Action',
    components: {
    },
    props: ['action', 'triggeredEntity'],
    data: () => ({
        // Dynamic Menu Item Data
        textFields: {}
    }),
    computed: {
    }, 
    methods: {
        colorPickerUpdated: function (type, colorPicker, colorEvent) {
            var dataToSend = {
                'triggeredEntityID': this.triggeredEntity.id,
                data: {
                    'type': type,
                    'name': colorPicker,
                    'colors': colorEvent
                }
            }

            this.$emit('emit-framework-message', 'menu-item-triggered', dataToSend);
        },
        sliderUpdated: function (type, slider, sliderEvent) {
            var dataToSend = {
                'triggeredEntityID': this.triggeredEntity.id,
                data: {
                    'type': type,
                    'name': slider,
                    'value': sliderEvent
                }
            }

            this.$emit('emit-framework-message', 'menu-item-triggered', dataToSend);
        },
        textFieldUpdated: function (type, textField) {
            var dataToSend = {
                'triggeredEntityID': this.triggeredEntity.id,
                data: {
                    'type': type,
                    'name': textField,
                    'value': this.textFields[textField]
                }
            }

            this.$emit('emit-framework-message', 'menu-item-triggered', dataToSend);
        },
        triggeredMenuItem: function (type, menuItem) {
            var dataToSend = {
                'triggeredEntityID': this.triggeredEntity.id,
                data: {
                    'type': type,
                    'name': menuItem
                }
            }

            this.$emit('emit-framework-message', 'menu-item-triggered', dataToSend);
        }
    },
    watch: {
    },
    created: function () {
    }
}
</script>
