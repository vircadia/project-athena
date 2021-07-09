<!-- 
//
//  App.vue
//
//  Created by Kalila L. on Oct 6 2020.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
// 
//  TODO: Create module, make it an iterator and improve definitions for adding new menu items.
//
-->

<template>
    <v-app>
        <v-app-bar
            app
        >
            <v-toolbar-title 
                v-if="triggeredEntity.name"
            >
                {{ triggeredEntity.name }}
            </v-toolbar-title>
            <v-toolbar-title 
                v-if="!triggeredEntity.name"
            >
                Unnamed
            </v-toolbar-title>
            
            <v-spacer></v-spacer>
            
            <v-btn
                small
                fab
                color="red"
                @click="closeEntityMenu();"
            >
                <v-icon>mdi-close-thick</v-icon>
            </v-btn>

            <template v-slot:extension>
                <v-spacer></v-spacer>

                <v-tooltip bottom>
                    <template v-slot:activator="{ on }">
                        <v-btn
                            small
                            fab
                            color="primary"
                            @click="triggerParentToEntity();"
                            class="mr-3"
                            v-on="on"
                        >
                            <v-icon>mdi-account-child</v-icon>
                        </v-btn>
                    </template>
                    <span>Parent self to entity</span>
                </v-tooltip>

                <v-tooltip bottom>
                    <template v-slot:activator="{ on }">
                        <v-btn
                            small
                            fab
                            color="primary"
                            @click="triggerSitOnEntity();"
                            class="mr-3"
                            v-on="on"
                        >
                            <v-icon>mdi-sofa-single</v-icon>
                        </v-btn>
                    </template>
                    <span>Sit on entity</span>
                </v-tooltip>

                <v-tooltip bottom>
                    <template v-slot:activator="{ on }">
                        <v-btn
                            small
                            fab
                            color="green"
                            @click="tabs = 2"
                            v-on="on"
                        >
                            <v-icon>mdi-cog</v-icon>
                        </v-btn>
                    </template>
                    <span>Settings</span>
                </v-tooltip>

                <v-spacer></v-spacer>
            </template>
        </v-app-bar>
        <v-main>
            <v-container
                class="fill-height"
                fluid
            >
                <v-card
                    height="100%"
                    width="100%"
                    class="pa-2"
                >
                    <v-tabs-items 
                        v-model="tabs"
                        height="100%"
                        width="100%"
                    >
                        <v-tab-item>
                            <v-list
                                v-if="registeredEntityMenus[triggeredEntity.id]"
                            >
                                <v-list>
                                    <Action
                                        v-for="item in registeredEntityMenus[triggeredEntity.id].actions"
                                        :key="item.name"
                                        @emit-framework-message="sendFrameworkMessage" 
                                        :triggeredEntity="triggeredEntity"
                                        :action="item"
                                    ></Action>
                                </v-list>
                            </v-list>
                            <div 
                                v-else-if="!registeredEntityMenus[triggeredEntity.id]"
                                class="text-center"
                            >
                                <v-list-item-subtitle
                                    class="text-h6"
                                >
                                    No Actions Available
                                </v-list-item-subtitle>
                            </div>
                        </v-tab-item>
                        <v-tab-item>
                            <!-- This needs to move to its own component. -->
                            <v-card
                                class="mx-auto"
                                height="100%"
                                width="100%"
                            >
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Entity Type
                                        </div>
                                        <v-list-item-subtitle>{{ triggeredEntity.type }}</v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Entity Host Type
                                        </div>
                                        <v-list-item-subtitle>{{ triggeredEntity.entityHostType }}</v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Last Edited By
                                        </div>
                                        <v-list-item-subtitle>{{ triggeredEntity.lastEditedByName }}</v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Description
                                        </div>
                                        <v-list-item-subtitle>{{ triggeredEntity.description }}</v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Position (Vec3)
                                        </div>
                                        <v-list-item-subtitle>
                                            <span class="red--text">x: </span>{{ triggeredEntity.position.x }}<br/>
                                            <span class="green--text">y: </span>{{ triggeredEntity.position.y }}<br/>
                                            <span class="blue--text">z: </span>{{ triggeredEntity.position.z }}
                                        </v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                                <v-list-item two-line>
                                    <v-list-item-content>
                                        <div class="overline">
                                            Rotation (Quat)
                                        </div>
                                        <v-list-item-subtitle>
                                            <span class="red--text">x: </span>{{ triggeredEntity.rotation.x }}<br/>
                                            <span class="green--text">y: </span>{{ triggeredEntity.rotation.y }}<br/>
                                            <span class="blue--text">z: </span>{{ triggeredEntity.rotation.z }}<br/>
                                            <span>w: </span>{{ triggeredEntity.rotation.w }}
                                        </v-list-item-subtitle>
                                    </v-list-item-content>
                                </v-list-item>
                            </v-card>
                        </v-tab-item>
                        <v-tab-item>
                            <!-- This needs to move to its own component. -->
                            <v-card
                                class="mx-auto"
                                height="100%"
                                width="100%"
                            >
                                <v-card-text
                                    class="pt-0"
                                >
                                    <v-card-title>
                                        Trigger Entity Menu
                                    </v-card-title>
                                    <v-divider></v-divider>
                                    <v-row>
                                        <v-card-title
                                            class="subtitle-1"
                                        >
                                            <v-checkbox
                                                v-model="settings.entityMenu.useMouseTriggers"
                                                hide-details
                                                class="shrink mr-2 mt-0"
                                            ></v-checkbox>
                                            Mouse / Controller
                                        </v-card-title>
                                    </v-row>
                                    <v-row>
                                        <v-combobox
                                            v-model="settings.entityMenu.selectedMouseTriggers"
                                            :items="settings.entityMenu.possibleMouseTriggers"
                                            :disabled="!settings.entityMenu.useMouseTriggers"
                                            label="Triggered by"
                                            multiple
                                            outlined
                                            dense
                                        ></v-combobox>
                                    </v-row>
                                    <v-row>
                                        <v-combobox
                                            v-model="settings.entityMenu.selectedMouseModifiers"
                                            :items="settings.entityMenu.possibleMouseModifiers"
                                            :disabled="true || !settings.entityMenu.useMouseTriggers"
                                            label="Modifiers"
                                            multiple
                                            outlined
                                            dense
                                        ></v-combobox>
                                    </v-row>

                                    <v-divider></v-divider>

                                    <v-card-title>
                                        Sit Offsets
                                    </v-card-title>
                                    <v-divider></v-divider>
                                    <v-row>
                                        <v-text-field
                                            label="x"
                                            v-model="settings.sit.offset.position.x"
                                            type="number"
                                        ></v-text-field>
                                        <v-text-field
                                            label="y"
                                            v-model="settings.sit.offset.position.y"
                                            type="number"
                                        ></v-text-field>
                                        <v-text-field
                                            label="z"
                                            v-model="settings.sit.offset.position.z"
                                            type="number"
                                        ></v-text-field>
                                    </v-row>
                                </v-card-text>
                            </v-card>
                        </v-tab-item>
                    </v-tabs-items>
                </v-card>
            </v-container>
        </v-main>
        <v-bottom-navigation
            app
        >
            <v-tabs
                v-model="tabs"
                fixed-tabs
            >
                <v-tabs-slider></v-tabs-slider>
                <v-tab
                    class="primary--text"
                >
                    <v-icon class="mr-2">mdi-format-list-bulleted</v-icon>
                    Actions
                </v-tab>
                <v-tab
                    class="primary--text"
                >
                    <v-icon class="mr-2">mdi-cube</v-icon>
                    Properties
                </v-tab>
                <v-tab
                    class="primary--text"
                    style="display: none;"
                >
                    <v-icon>mdi-cog</v-icon>
                </v-tab>
            </v-tabs>
        </v-bottom-navigation>
    </v-app>
</template>

<script>
// Components
import Action from './components/Action'

var vue_this;

function browserDevelopment() {
    if (typeof EventBridge !== 'undefined') {
        return false; // We are in Vircadia.
    } else {
        return true; // We are in the browser, probably for development purposes.
    }
}

if (!browserDevelopment()) {
    // eslint-disable-next-line
    EventBridge.scriptEventReceived.connect(function(receivedCommand) {
        receivedCommand = JSON.parse(receivedCommand);

        // We route the data based on the command given.
        if (receivedCommand.command === 'script-to-web-registered-entity-menus') {
            vue_this.updateRegisteredEntityMenus(receivedCommand.data);
        }
        
        if (receivedCommand.command === 'script-to-web-triggered-entity-info') {
            vue_this.updateTriggeredEntityInfo(receivedCommand.data);
        }
        
        if (receivedCommand.command === 'script-to-web-update-settings') {
            if (receivedCommand.data.settings === null || receivedCommand.data.settings === '') {
                return;
            }

            vue_this.updateSettings(receivedCommand.data);
        }
    });
}

export default {
    name: 'App',
    components: {
        Action
    },
    data: () => ({
        // Settings
        settings: {
            entityMenu: {
                useMouseTriggers: true,
                selectedMouseTriggers: [],
                possibleMouseTriggers: [
                    'Primary',
                    'Secondary',
                    'Tertiary'
                ],
                possibleMouseModifiers: [
                    'Shift',
                    'Meta',
                    'Control',
                    'Alt'
                ]
            },
            sit: {
                offset: {
                    position: {
                        x: 0,
                        y: 0,
                        z: 0
                    }
                }
            }
        },
        // General
        tabs: 0,
        // Entity Context Menu
        registeredEntityMenus: {
            "{768542d0-e962-49e3-94fb-85651d56f5ae}": {
                'actions': [
                    {
                        type: 'button',
                        name: 'Equip'
                    },
                    {
                        type: 'colorPicker',
                        name: 'Color Picker',
                        initialColor: {
                            r: '1',
                            g: '1',
                            b: '1',
                            a: '1'
                        }
                    },
                    {
                        type: 'slider',
                        name: 'Alpha',
                        step: 0.1,
                        color: 'yellow',
                        initialValue: '0.1',
                        minValue: 0,
                        maxValue: 1
                    },
                    {
                        type: 'button',
                        name: 'Unequip'
                    }
                ]
            }
        },
        triggeredEntity: {
            id: '{768542d0-e962-49e3-94fb-85651d56f5ae}',
            name: 'TESTTTTTTTT',
            type: 'test',
            lastEditedBy: 'test',
            lastEditedByName: 'test',
            description: 'This is a test description.',
            entityHostType: 'Domain',
            position: {
                'x': 1,
                'y': 1,
                'z': 1
            },
            rotation: {
                'x': 1,
                'y': 1,
                'z': 1,
                'w': 1
            },
        }
    }),
    computed: {
    }, 
    methods: {
        updateRegisteredEntityMenus: function (data) {
            // The data should already be parsed.
            // console.log("DATA RECEIVED ON UPDATE REGISTERED ENTITY MENU ITEMS:" + JSON.stringify(data));
            this.registeredEntityMenus = data.registeredEntityMenus;
            this.triggeredEntity = data.lastTriggeredEntityInfo;
        },
        updateTriggeredEntityInfo: function (data) {
            // The data should already be parsed.
            // console.log("DATA RECEIVED ON TRIGGERED ENTITY:" + JSON.stringify(data));
            this.triggeredEntity = data;
        },
        updateSettings: function (data) {
            this.settings = data.settings;
        },
        triggerSitOnEntity: function () {
            this.sendFrameworkMessage('sit-on-entity-triggered', {});
        },
        triggerParentToEntity: function () {
            this.sendFrameworkMessage('parent-to-entity-triggered', {});
        },
        triggerMoveModeOnEntity: function () {
            this.sendFrameworkMessage('entity-move-mode-triggered', {});
        },
        closeEntityMenu: function () {
            this.sendFrameworkMessage('close-entity-menu', '');
        },
        sendFrameworkMessage: function (command, data) {
            var JSONtoSend = {
                "command": command,
                "data": data
            };
            
            if (!browserDevelopment()) {
                // eslint-disable-next-line
                EventBridge.emitWebEvent(JSON.stringify(JSONtoSend));
            } else {
                // console.info(JSON.stringify(JSONtoSend));
            }
        }
    },
    watch: {
        settings: {
            deep: true,
            handler() {
                var dataToSend = {
                    settings: this.settings
                }

                this.sendFrameworkMessage('web-to-script-settings-changed', dataToSend);
            }
        }
    },
    created: function () {
        vue_this = this;

        this.$vuetify.theme.dark = 'true';
        this.sendFrameworkMessage('ready', '');
    }
}
</script>