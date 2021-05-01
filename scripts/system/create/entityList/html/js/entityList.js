//  entityList.js
//
//  Created by Ryan Huffman on 19 Nov 2014
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

const ASCENDING_SORT = 1;
const DESCENDING_SORT = -1;
const ASCENDING_STRING = "&#x25B4;";
const DESCENDING_STRING = "&#x25BE;";
const BYTES_PER_MEGABYTE = 1024 * 1024;
const COLLAPSE_EXTRA_INFO = "E";
const EXPAND_EXTRA_INFO = "D";
const FILTER_IN_VIEW_ATTRIBUTE = "pressed";
const WINDOW_NONVARIABLE_HEIGHT = 180;
const EMPTY_ENTITY_ID = "0";
const MAX_LENGTH_RADIUS = 9;
const MINIMUM_COLUMN_WIDTH = 24;
const SCROLLBAR_WIDTH = 20;
const RESIZER_WIDTH = 13; //Must be the number of COLUMNS - 1.
const DELTA_X_MOVE_COLUMNS_THRESHOLD = 2;
const DELTA_X_COLUMN_SWAP_POSITION = 5;

function decimalMegabytes(number) {
    return number ? (number / BYTES_PER_MEGABYTE).toFixed(1) : "";
}

function displayIfNonZero(number) {
    return number ? number : "";
}

function getFilename(url) {
    let urlParts = url.split("/");
    return urlParts[urlParts.length - 1];
}

const COLUMNS = {
    type: {
        columnHeader: "Type",
        propertyID: "type",
        initialWidth: 0.16,
        initiallyShown: true,
        alwaysShown: true,
        defaultSortOrder: ASCENDING_SORT,
    },
    parentState: {
        columnHeader: "A",
        vglyph: true,
        dropdownLabel: "Hierarchy",
        propertyID: "parentState",
        initialWidth: 0.08,
        defaultSortOrder: DESCENDING_SORT,
    },     
    name: {
        columnHeader: "Name",
        propertyID: "name",
        initialWidth: 0.34,
        initiallyShown: true,
        alwaysShown: true,
        defaultSortOrder: ASCENDING_SORT,
    },
    url: {
        columnHeader: "File",
        dropdownLabel: "File",
        propertyID: "url",
        initialWidth: 0.34,
        initiallyShown: true,
        defaultSortOrder: ASCENDING_SORT,
    },
    locked: {
        columnHeader: "&#xe006;",
        glyph: true,
        propertyID: "locked",
        initialWidth: 0.08,
        initiallyShown: true,
        alwaysShown: true,
        defaultSortOrder: DESCENDING_SORT,
    },
    visible: {
        columnHeader: "&#xe007;",
        glyph: true,
        propertyID: "visible",
        initialWidth: 0.08,
        initiallyShown: true,
        alwaysShown: true,
        defaultSortOrder: DESCENDING_SORT,
    },
    verticesCount: {
        columnHeader: "Verts",
        dropdownLabel: "Vertices",
        propertyID: "verticesCount",
        initialWidth: 0.08,
        defaultSortOrder: DESCENDING_SORT,
    },
    texturesCount: {
        columnHeader: "Texts",
        dropdownLabel: "Textures",
        propertyID: "texturesCount",
        initialWidth: 0.08,
        defaultSortOrder: DESCENDING_SORT,
    },
    texturesSize: {
        columnHeader: "Text MB",
        dropdownLabel: "Texture Size",
        propertyID: "texturesSize",
        initialWidth: 0.10,
        format: decimalMegabytes,
        defaultSortOrder: DESCENDING_SORT,
    },
    hasTransparent: {
        columnHeader: "&#xe00b;",
        glyph: true,
        dropdownLabel: "Transparency",
        propertyID: "hasTransparent",
        initialWidth: 0.04,
        defaultSortOrder: DESCENDING_SORT,
    },
    isBaked: {
        columnHeader: "&#xe01a;",
        glyph: true,
        dropdownLabel: "Baked",
        propertyID: "isBaked",
        initialWidth: 0.08,
        defaultSortOrder: DESCENDING_SORT,
    },
    drawCalls: {
        columnHeader: "Draws",
        dropdownLabel: "Draws",
        propertyID: "drawCalls",
        initialWidth: 0.08,
        defaultSortOrder: DESCENDING_SORT,
    },
    hasScript: {
        columnHeader: "k",
        glyph: true,
        dropdownLabel: "Script",
        propertyID: "hasScript",
        initialWidth: 0.06,
        defaultSortOrder: DESCENDING_SORT,
    },
    created: {
        columnHeader: "Created (UTC)",
        dropdownLabel: "Creation Date",
        propertyID: "created",
        initialWidth: 0.38,
        defaultSortOrder: DESCENDING_SORT,
    },
    lastEdited: {
        columnHeader: "Modified (UTC)",
        dropdownLabel: "Modification Date",
        propertyID: "lastEdited",
        initialWidth: 0.38,
        defaultSortOrder: DESCENDING_SORT,
    },
    urlWithPath: {
        columnHeader: "URL",
        dropdownLabel: "URL",
        propertyID: "urlWithPath",
        initialWidth: 0.54,
        initiallyShown: false,
        defaultSortOrder: ASCENDING_SORT,
    },
};

const FILTER_TYPES = [
    "Shape",
    "Model",
    "Image",
    "Light",
    "Zone",
    "Web",
    "Material",
    "ParticleEffect",
    "PolyLine",
    "PolyVox",
    "Text",
    "Grid",
];

const DOUBLE_CLICK_TIMEOUT = 300; // ms
const RENAME_COOLDOWN = 400; // ms

// List of all entities
let entities = [];
// List of all entities, indexed by Entity ID
let entitiesByID = {};
// The filtered and sorted list of entities passed to ListView
let visibleEntities = [];
// List of all entities that are currently selected
let selectedEntities = [];

let entityList = null; // The ListView

let hmdMultiSelectMode = false; 

let lastSelectedEntity;
/**
 * @type EntityListContextMenu
 */
let entityListContextMenu = null;

let currentSortColumnID = "type";
let currentSortOrder = ASCENDING_SORT;
let elSortOrders = {};
let typeFilters = [];
let isFilterInView = false;

let columns = [];
let columnsByID = {};
let lastResizeEvent = null;
let resizeColumnIndex = 0;
let elTargetTh = null;
let elTargetSpan = null;
let targetColumnIndex = 0;
let lastColumnSwapPosition = -1;
let initialThEvent = null;
let renameTimeout = null;
let renameLastBlur = null;
let renameLastEntityID = null;
let isRenameFieldBeingMoved = false;
let elFilterTypeInputs = {};

let elEntityTable,
    elEntityTableHeader,
    elEntityTableBody,
    elEntityTableScroll,
    elEntityTableHeaderRow,
    elRefresh,
    elToggleLocked,
    elToggleVisible,
    elActionsMenu,
    elSelectionMenu,
    elTransformMenu,
    elToolsMenu,
    elMenuBackgroundOverlay,
    elHmdMultiSelect,
    elHmdCopy,
    elHmdCut,
    elHmdPaste,
    elHmdDuplicate,   
    elUndo,
    elRedo,
    elParent,
    elUnparent,    
    elDelete,
    elRotateAsTheNextClickedSurface,
    elQuickRotate90x,
    elQuickRotate90y,
    elQuickRotate90z,
    elMoveEntitySelectionToAvatar,
    elSelectAll,
    elSelectInverse,
    elSelectNone,
    elSelectAllInBox,
    elSelectAllTouchingBox,
    elSelectParent,
    elSelectTopParent,
    elAddChildrenToSelection,
    elSelectFamily,
    elSelectTopFamily,
    elTeleportToEntity,
    elSetCameraFocusToSelection,
    elToggleLocalWorldMode,
    elExportSelectedEntities,
    elImportEntitiesFromFile,
    elImportEntitiesFromUrl,
    elGridActivator,
    elSnapToGridActivator,
    elSnapToGridActivatorCaption,
    elAlignGridToSelection,
    elAlignGridToAvatar,
    elBrokenURLReport,
    elFilterTypeMultiselectBox,
    elFilterTypeText,
    elFilterTypeOptions,
    elFilterTypeOptionsButtons,
    elFilterTypeSelectAll,
    elFilterTypeClearAll,
    elFilterSearch,
    elFilterInView,
    elFilterRadius,
    elExport,
    elSelectedEntitiesCount,
    elVisibleEntitiesCount,
    elNoEntitiesMessage,
    elColumnsMultiselectBox,
    elColumnsOptions,
    elToggleSpaceMode,
    elRenameInput;

const ENABLE_PROFILING = false;
let profileIndent = "";
const PROFILE_NOOP = function(_name, fn, args) {
    fn.apply(this, args);
} ;
const PROFILE = !ENABLE_PROFILING ? PROFILE_NOOP : function(name, fn, args) {
    console.log("PROFILE-Web " + profileIndent + "(" + name + ") Begin");
    let previousIndent = profileIndent;
    profileIndent += '  ';
    let before = Date.now();
    fn.apply(this, args);
    let delta = Date.now() - before;
    profileIndent = previousIndent;
    console.log("PROFILE-Web " + profileIndent + "(" + name + ") End " + delta + "ms");
};

function loaded() {    
    openEventBridge(function() {

        var isColumnsSettingLoaded = false;
        
        elEntityTable = document.getElementById("entity-table");
        elEntityTableHeader = document.getElementById("entity-table-header");
        elEntityTableBody = document.getElementById("entity-table-body");
        elEntityTableScroll = document.getElementById("entity-table-scroll");
        elRefresh = document.getElementById("refresh");
        elToggleLocked = document.getElementById("locked");
        elToggleVisible = document.getElementById("visible");         
        elHmdMultiSelect = document.getElementById("hmdmultiselect");
        elActionsMenu = document.getElementById("actions");
        elSelectionMenu = document.getElementById("selection");
        elTransformMenu = document.getElementById("transform");
        elToolsMenu = document.getElementById("tools");
        elMenuBackgroundOverlay = document.getElementById("menuBackgroundOverlay");
        elHmdCopy = document.getElementById("hmdcopy");
        elHmdCut = document.getElementById("hmdcut");
        elHmdPaste = document.getElementById("hmdpaste");
        elHmdDuplicate = document.getElementById("hmdduplicate");        
        elUndo = document.getElementById("undo");
        elRedo = document.getElementById("redo");
        elParent = document.getElementById("parent");
        elUnparent = document.getElementById("unparent");
        elDelete = document.getElementById("delete");
        elRotateAsTheNextClickedSurface = document.getElementById("rotateAsTheNextClickedSurface");
        elQuickRotate90x = document.getElementById("quickRotate90x");
        elQuickRotate90y = document.getElementById("quickRotate90y");
        elQuickRotate90z = document.getElementById("quickRotate90z");
        elMoveEntitySelectionToAvatar = document.getElementById("moveEntitySelectionToAvatar"); 
        elSelectAll = document.getElementById("selectall");
        elSelectInverse = document.getElementById("selectinverse");
        elSelectNone = document.getElementById("selectnone");
        elSelectAllInBox = document.getElementById("selectallinbox");
        elSelectAllTouchingBox = document.getElementById("selectalltouchingbox");
        elSelectParent = document.getElementById("selectparent");
        elSelectTopParent = document.getElementById("selecttopparent");
        elAddChildrenToSelection = document.getElementById("addchildrentoselection");
        elSelectFamily = document.getElementById("selectfamily");
        elSelectTopFamily = document.getElementById("selecttopfamily");
        elTeleportToEntity = document.getElementById("teleport-to-entity");
        elSetCameraFocusToSelection = document.getElementById("setCameraFocusToSelection");
        elToggleLocalWorldMode = document.getElementById("toggleLocalWorldMode");
        elExportSelectedEntities = document.getElementById("exportSelectedEntities");
        elImportEntitiesFromFile = document.getElementById("importEntitiesFromFile");
        elImportEntitiesFromUrl = document.getElementById("importEntitiesFromUrl");
        elGridActivator = document.getElementById("gridActivator");
        elSnapToGridActivator = document.getElementById("snapToGridActivator");
        elSnapToGridActivatorCaption = document.getElementById("snapToGridActivatorCaption");
        elAlignGridToSelection = document.getElementById("alignGridToSelection");
        elAlignGridToAvatar = document.getElementById("alignGridToAvatar");
        elBrokenURLReport = document.getElementById("brokenURLReport");
        elFilterTypeMultiselectBox = document.getElementById("filter-type-multiselect-box");
        elFilterTypeText = document.getElementById("filter-type-text");
        elFilterTypeOptions = document.getElementById("filter-type-options");
        elFilterTypeOptionsButtons = document.getElementById("filter-type-options-buttons");
        elFilterTypeSelectAll = document.getElementById('filter-type-select-all');
        elFilterTypeClearAll = document.getElementById('filter-type-clear-all');
        elFilterSearch = document.getElementById("filter-search");
        elFilterInView = document.getElementById("filter-in-view");
        elFilterRadius = document.getElementById("filter-radius");
        elExport = document.getElementById("export");
        elSelectedEntitiesCount = document.getElementById("selected-entities-count");
        elVisibleEntitiesCount = document.getElementById("visible-entities-count");
        elNoEntitiesMessage = document.getElementById("no-entities");
        elColumnsMultiselectBox = document.getElementById("entity-table-columns-multiselect-box");
        elColumnsOptions = document.getElementById("entity-table-columns-options");
        elToggleSpaceMode = document.getElementById("toggle-space-mode");

        document.body.onclick = onBodyClick;
        elToggleLocked.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleLocked" }));
        };
        elToggleVisible.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleVisible" }));
        };
        elExport.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "export"}));
        };
        elHmdMultiSelect.onclick = function() {
            if (hmdMultiSelectMode) {
                elHmdMultiSelect.className = "vglyph";
                hmdMultiSelectMode = false;
            } else {
                elHmdMultiSelect.className = "white vglyph";
                hmdMultiSelectMode = true;
            }
            EventBridge.emitWebEvent(JSON.stringify({ type: "hmdMultiSelectMode", value: hmdMultiSelectMode }));
        };
        elActionsMenu.onclick = function() {
            document.getElementById("menuBackgroundOverlay").style.display = "block";
            document.getElementById("actions-menu").style.display = "block";
        };
        elSelectionMenu.onclick = function() {
            document.getElementById("menuBackgroundOverlay").style.display = "block";
            document.getElementById("selection-menu").style.display = "block";
        };
        elTransformMenu.onclick = function() {
            document.getElementById("menuBackgroundOverlay").style.display = "block";
            document.getElementById("transform-menu").style.display = "block";
        };        
        elToolsMenu.onclick = function() {
            document.getElementById("menuBackgroundOverlay").style.display = "block";
            document.getElementById("tools-menu").style.display = "block";
        };
        elMenuBackgroundOverlay.onclick = function() {
            closeAllEntityListMenu();
        };
        elHmdCopy.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "copy" }));
            closeAllEntityListMenu();
        };
        elHmdCut.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "cut" }));
            closeAllEntityListMenu();
        };
        elHmdPaste.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "paste" }));
            closeAllEntityListMenu();
        };
        elHmdDuplicate.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "duplicate" }));
            closeAllEntityListMenu();
        };
        elParent.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "parent" }));
            closeAllEntityListMenu();
        };
        elUnparent.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "unparent" }));
            closeAllEntityListMenu();
        };
        elUndo.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "undo" }));
            closeAllEntityListMenu();
        };
        elRedo.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "redo" }));
            closeAllEntityListMenu();
        };         
        elDelete.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "delete" }));
            closeAllEntityListMenu();
        };
        elRotateAsTheNextClickedSurface.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "rotateAsTheNextClickedSurface" }));
            closeAllEntityListMenu();
        };
        elQuickRotate90x.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "quickRotate90x" }));
            closeAllEntityListMenu();
        };
        elQuickRotate90y.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "quickRotate90y" }));
            closeAllEntityListMenu();
        };
        elQuickRotate90z.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "quickRotate90z" }));
            closeAllEntityListMenu();
        };
        elMoveEntitySelectionToAvatar.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "moveEntitySelectionToAvatar" }));
            closeAllEntityListMenu();
        };
        elSelectAll.onclick = function() {

            let visibleEntityIDs = visibleEntities.map(visibleEntity => visibleEntity.id);
            let selectionIncludesAllVisibleEntityIDs = visibleEntityIDs.every(visibleEntityID => {
                return selectedEntities.includes(visibleEntityID);
            });

            let selection = [];

            if (!selectionIncludesAllVisibleEntityIDs) {
                selection = visibleEntityIDs;
            }

            updateSelectedEntities(selection, false);

            EventBridge.emitWebEvent(JSON.stringify({
                "type": "selectionUpdate",
                "focus": false,
                "entityIds": selection
            }));

            closeAllEntityListMenu();
        };
        elSelectInverse.onclick = function() {
            let visibleEntityIDs = visibleEntities.map(visibleEntity => visibleEntity.id);
            let selectionIncludesAllVisibleEntityIDs = visibleEntityIDs.every(visibleEntityID => {
                return selectedEntities.includes(visibleEntityID);
            });

            let selection = [];

            if (!selectionIncludesAllVisibleEntityIDs) {
                visibleEntityIDs.forEach(function(id) {
                    if (!selectedEntities.includes(id)) {
                        selection.push(id);
                    }
                });
            }

            updateSelectedEntities(selection, false);

            EventBridge.emitWebEvent(JSON.stringify({
                "type": "selectionUpdate",
                "focus": false,
                "entityIds": selection
            }));
            
            closeAllEntityListMenu();
        };
        elSelectNone.onclick = function() {
            updateSelectedEntities([], false);
            EventBridge.emitWebEvent(JSON.stringify({
                "type": "selectionUpdate",
                "focus": false,
                "entityIds": []
            }));            
            closeAllEntityListMenu();
        };
        elSelectAllInBox.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectAllInBox" }));
            closeAllEntityListMenu();
        };
        elSelectAllTouchingBox.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectAllTouchingBox" }));
            closeAllEntityListMenu();
        };
        elSelectParent.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectParent" }));
            closeAllEntityListMenu();
        };
        elSelectTopParent.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectTopParent" }));
            closeAllEntityListMenu();
        };
        elAddChildrenToSelection.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "addChildrenToSelection" }));
            closeAllEntityListMenu();
        };
        elSelectFamily.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectFamily" }));
            closeAllEntityListMenu();
        };
        elSelectTopFamily.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "selectTopFamily" }));
            closeAllEntityListMenu();
        };
        elTeleportToEntity.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "teleportToEntity" }));
            closeAllEntityListMenu();
        };
        elSetCameraFocusToSelection.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "setCameraFocusToSelection" }));
            closeAllEntityListMenu();
        };
        elToggleLocalWorldMode.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleSpaceMode" }));
            closeAllEntityListMenu();
        };
        elExportSelectedEntities.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "export"}));
            closeAllEntityListMenu();
        };
        elImportEntitiesFromFile.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "importFromFile"}));
            closeAllEntityListMenu();
        };
        elImportEntitiesFromUrl.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "importFromUrl"}));
            closeAllEntityListMenu();
        };
        elGridActivator.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleGridVisibility" }));
            closeAllEntityListMenu();
        };
        elSnapToGridActivator.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleSnapToGrid" }));
            closeAllEntityListMenu();
        };
        elAlignGridToSelection.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "alignGridToSelection" }));
            closeAllEntityListMenu();
        };
        elAlignGridToAvatar.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "alignGridToAvatar" }));
            closeAllEntityListMenu();
        };
        elBrokenURLReport.onclick = function () {
            EventBridge.emitWebEvent(JSON.stringify({ type: "brokenURLReport" }));
            closeAllEntityListMenu();
        };
        elToggleSpaceMode.onclick = function() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "toggleSpaceMode" }));
        };
        elRefresh.onclick = refreshEntities;
        elFilterTypeMultiselectBox.onclick = onToggleTypeDropdown;
        elFilterTypeSelectAll.onclick = onSelectAllTypes;
        elFilterTypeClearAll.onclick = onClearAllTypes;
        elFilterSearch.onkeyup = refreshEntityList;
        elFilterSearch.onsearch = refreshEntityList;
        elFilterInView.onclick = onToggleFilterInView;
        elFilterRadius.onkeyup = onRadiusChange;
        elFilterRadius.onchange = onRadiusChange;
        elColumnsMultiselectBox.onclick = onToggleColumnsDropdown;
        
        // create filter type dropdown checkboxes with label and icon for each type
        for (let i = 0; i < FILTER_TYPES.length; ++i) {
            let type = FILTER_TYPES[i];
            let typeFilterID = "filter-type-" + type;
            
            let elDiv = document.createElement("div");
            elDiv.onclick = onToggleTypeFilter;
            elFilterTypeOptions.insertBefore(elDiv, elFilterTypeOptionsButtons);
            
            let elInput = document.createElement("input");
            elInput.setAttribute("type", "checkbox");
            elInput.setAttribute("id", typeFilterID);
            elInput.setAttribute("filterType", type);
            elInput.checked = true; // all types are checked initially
            elFilterTypeInputs[type] = elInput;
            elDiv.appendChild(elInput);
            
            let elLabel = document.createElement("label");
            elLabel.setAttribute("for", typeFilterID);
            elLabel.innerText = type;
            elDiv.appendChild(elLabel);
            
            let elSpan = document.createElement("span");
            elSpan.setAttribute("class", "typeIcon");
            elSpan.innerHTML = ENTITY_TYPE_ICON[type];

            elLabel.insertBefore(elSpan, elLabel.childNodes[0]);
            
            toggleTypeFilter(elInput, false); // add all types to the initial types filter
        }
        
        // create columns
        elHeaderTr = document.createElement("tr");
        elEntityTableHeader.appendChild(elHeaderTr);
        let columnIndex = 0;
        for (let columnID in COLUMNS) {
            let columnData = COLUMNS[columnID];
            
            let elTh = document.createElement("th");
            let thID = "entity-" + columnID;
            elTh.setAttribute("id", thID);
            elTh.setAttribute("columnIndex", columnIndex);
            elTh.setAttribute("columnID", columnID);
            if (columnData.glyph || columnData.vglyph) {
                let elGlyph = document.createElement("span");
                elGlyph.className = "glyph";
                if (columnData.vglyph) {
                    elGlyph.className = "vglyph";
                }
                elGlyph.innerHTML = columnData.columnHeader;
                elTh.appendChild(elGlyph);
            } else {
                elTh.innerText = columnData.columnHeader;
            }
            elTh.onmousedown = function(event) {
                if (event.target.nodeName === "TH") {
                    elTargetTh = event.target;
                    targetColumnIndex = parseInt(elTargetTh.getAttribute("columnIndex"));
                    lastColumnSwapPosition = event.clientX;
                } else if (event.target.nodeName === "SPAN") {
                    elTargetSpan = event.target;
                }
                initialThEvent = event;
            };

            let elResizer = document.createElement("span");
            elResizer.className = "resizer";
            elResizer.innerHTML = "&nbsp;";
            elResizer.onmousedown = onStartResize;
            elTh.appendChild(elResizer);

            let elSortOrder = document.createElement("span");
            elSortOrder.className = "sort-order";
            elTh.appendChild(elSortOrder);
            elHeaderTr.appendChild(elTh);
                        
            elSortOrders[columnID] = elSortOrder;
            
            // add column to columns dropdown if it is not set to be always shown
            if (columnData.alwaysShown !== true) { 
                let columnDropdownID = "entity-table-column-" + columnID;
                
                let elDiv = document.createElement("div");
                elDiv.onclick = onToggleColumn;
                elColumnsOptions.appendChild(elDiv);
                
                let elInput = document.createElement("input");
                elInput.setAttribute("type", "checkbox");
                elInput.setAttribute("id", columnDropdownID);
                elInput.setAttribute("columnID", columnID);
                elInput.checked = columnData.initiallyShown === true;
                elDiv.appendChild(elInput);
                
                let elLabel = document.createElement("label");
                elLabel.setAttribute("for", columnDropdownID);
                elLabel.innerText = columnData.dropdownLabel;
                elDiv.appendChild(elLabel);
            }
            
            let initialWidth = columnData.initiallyShown === true ? columnData.initialWidth : 0;
            columns.push({
                columnID: columnID,
                elTh: elTh,
                elResizer: elResizer,
                width: initialWidth,
                data: columnData
            });
            columnsByID[columnID] = columns[columnIndex];
            
            ++columnIndex;
        }

        elEntityTableHeaderRow = document.querySelectorAll("#entity-table thead th");

        entityList = new ListView(elEntityTableBody, elEntityTableScroll, elEntityTableHeaderRow, createRow, updateRow,
                                  clearRow, preRefresh, postRefresh, preRefresh, WINDOW_NONVARIABLE_HEIGHT);

        entityListContextMenu = new EntityListContextMenu();

        function startRenamingEntity(entityID) {
            renameLastEntityID = entityID;
            let entity = entitiesByID[entityID];
            if (!entity || entity.locked || !entity.elRow) {
                return;
            }

            let elCell = entity.elRow.childNodes[getColumnIndex("name")];
            elRenameInput = document.createElement("input");
            elRenameInput.setAttribute("class", "rename-entity");
            elRenameInput.value = entity.name;
            let ignoreClicks = function(event) {
                event.stopPropagation();
            };
            elRenameInput.onclick = ignoreClicks;
            elRenameInput.ondblclick = ignoreClicks;
            elRenameInput.onkeyup = function(keyEvent) {
                if (keyEvent.key === "Enter") {
                    elRenameInput.blur();
                }
            };

            elRenameInput.onblur = function(event) {
                if (isRenameFieldBeingMoved) {
                    return;
                }
                let value = elRenameInput.value;
                EventBridge.emitWebEvent(JSON.stringify({
                    type: 'rename',
                    entityID: entityID,
                    name: value
                }));
                entity.name = value;
                elRenameInput.parentElement.innerText = value;

                renameLastBlur = Date.now();
                elRenameInput = null;
            };

            elCell.innerHTML = "";
            elCell.appendChild(elRenameInput);

            elRenameInput.select();
        }

        function preRefresh() {
            // move the rename input to the body
            if (!isRenameFieldBeingMoved && elRenameInput) {
                isRenameFieldBeingMoved = true;
                document.body.appendChild(elRenameInput);
                // keep the focus
                elRenameInput.focus();
            }
        }

        function postRefresh() {
            if (!elRenameInput || !isRenameFieldBeingMoved) {
                return;
            }
            let entity = entitiesByID[renameLastEntityID];
            if (!entity || entity.locked || !entity.elRow) {
                return;
            }
            let elCell = entity.elRow.childNodes[getColumnIndex("name")];
            elCell.innerHTML = "";
            elCell.appendChild(elRenameInput);
            // keep the focus
            elRenameInput.focus();
            isRenameFieldBeingMoved = false;
        }

        entityListContextMenu.setOnSelectedCallback(function(optionName, selectedEntityID) {
            switch (optionName) {
                case "Cut":
                    EventBridge.emitWebEvent(JSON.stringify({ type: "cut" }));
                    break;
                case "Copy":
                    EventBridge.emitWebEvent(JSON.stringify({ type: "copy" }));
                    break;
                case "Paste":
                    EventBridge.emitWebEvent(JSON.stringify({ type: "paste" }));
                    break;
                case "Rename":
                    startRenamingEntity(selectedEntityID);
                    break;
                case "Duplicate":
                    EventBridge.emitWebEvent(JSON.stringify({ type: "duplicate" }));
                    break;
                case "Delete":
                    EventBridge.emitWebEvent(JSON.stringify({ type: "delete" }));
                    break;
            }
        });

        function onRowContextMenu(clickEvent) {
            if (elRenameInput) {
                // disallow the context menu from popping up while renaming
                return;
            }

            let entityID = this.dataset.entityID;

            if (!selectedEntities.includes(entityID)) {
                let selection = [entityID];
                updateSelectedEntities(selection);

                EventBridge.emitWebEvent(JSON.stringify({
                    type: "selectionUpdate",
                    focus: false,
                    entityIds: selection,
                }));
            }

            let enabledContextMenuItems = ["Copy", "Paste", "Duplicate"];
            if (entitiesByID[entityID] && !entitiesByID[entityID].locked) {
                enabledContextMenuItems.push("Cut");
                enabledContextMenuItems.push("Rename");
                enabledContextMenuItems.push("Delete");
            }

            entityListContextMenu.open(clickEvent, entityID, enabledContextMenuItems);
        }

        let clearRenameTimeout = () => {
            if (renameTimeout !== null) {
                window.clearTimeout(renameTimeout);
                renameTimeout = null;
            }
        };

        function onRowClicked(clickEvent) {
            let entityID = this.dataset.entityID;
            let selection = [entityID];
            let controlKey = window.navigator.platform.startsWith("Mac") ? clickEvent.metaKey : clickEvent.ctrlKey;

            if (controlKey || hmdMultiSelectMode) {
                let selectedIndex = selectedEntities.indexOf(entityID);
                if (selectedIndex >= 0) {
                    selection = [];
                    selection = selectedEntities.concat(selection);
                    selection.splice(selectedIndex, 1);
                } else {
                    selection = selectedEntities.concat(selection);
                }
            } else if (clickEvent.shiftKey && selectedEntities.length > 0) {
                let previousItemFound = -1;
                let clickedItemFound = -1;
                for (let i = 0, len = visibleEntities.length; i < len; ++i) {
                    let entity = visibleEntities[i];
                    if (clickedItemFound === -1 && entityID === entity.id) {
                        clickedItemFound = i;
                    } else if (previousItemFound === -1 && selectedEntities[0] === entity.id) {
                        previousItemFound = i;
                    }
                }
                if (previousItemFound !== -1 && clickedItemFound !== -1) {
                    selection = [];
                    let toItem = Math.max(previousItemFound, clickedItemFound);
                    for (let i = Math.min(previousItemFound, clickedItemFound); i <= toItem; i++) {
                        selection.push(visibleEntities[i].id);
                    }
                    if (previousItemFound > clickedItemFound) {
                        // always make sure that we add the items in the right order
                        selection.reverse();
                    }
                }
            } else if (!controlKey && !clickEvent.shiftKey && selectedEntities.length === 1) {
                // if reselecting the same entity then start renaming it
                if (selectedEntities[0] === entityID) {
                    if (renameLastBlur && renameLastEntityID === entityID && (Date.now() - renameLastBlur) < RENAME_COOLDOWN) {

                        return;
                    }
                    clearRenameTimeout();
                    renameTimeout = window.setTimeout(() => {
                        renameTimeout = null;
                        startRenamingEntity(entityID);
                    }, DOUBLE_CLICK_TIMEOUT);
                }
            }
            
            updateSelectedEntities(selection, false);

            EventBridge.emitWebEvent(JSON.stringify({
                type: "selectionUpdate",
                focus: false,
                entityIds: selection,
            }));
        }

        function onRowDoubleClicked() {
            clearRenameTimeout();

            let selection = [this.dataset.entityID];
            updateSelectedEntities(selection, false);

            EventBridge.emitWebEvent(JSON.stringify({
                type: "selectionUpdate",
                focus: true,
                entityIds: selection,
            }));
        }

        function updateEntityData(entityData) {
            entities = [];
            entitiesByID = {};
            visibleEntities.length = 0; // maintains itemData reference in ListView
            
            PROFILE("map-data", function() {
                entityData.forEach(function(entity) {
                    let type = entity.type;
                    let filename = getFilename(entity.url);

                    let entityData = {
                        id: entity.id,
                        name: entity.name,
                        type: type,
                        url: filename,
                        fullUrl: filename,
                        urlWithPath: entity.certificateID === "" ? entity.url : "<i>" + CERTIFIED_PLACEHOLDER + "</i>",
                        locked: entity.locked,
                        visible: entity.visible,
                        verticesCount: displayIfNonZero(entity.verticesCount),
                        texturesCount: displayIfNonZero(entity.texturesCount),
                        texturesSize: entity.texturesSize,
                        hasTransparent: entity.hasTransparent,
                        isBaked: entity.isBaked,
                        drawCalls: displayIfNonZero(entity.drawCalls),
                        hasScript: entity.hasScript,
                        parentState: entity.parentState,
                        created: entity.created,
                        lastEdited: entity.lastEdited,
                        elRow: null, // if this entity has a visible row element assigned to it
                        selected: false // if this entity is selected for edit regardless of having a visible row
                    };

                    entities.push(entityData);
                    entitiesByID[entityData.id] = entityData;
                });
            });
            
            refreshEntityList();
        }

        const isNullOrEmpty = function(value) {
            return value === undefined || value === null || value === "";
        };
        
        function refreshEntityList() {
            PROFILE("refresh-entity-list", function() {
                PROFILE("filter", function() {
                    let searchTerm = elFilterSearch.value.toLowerCase();
                    visibleEntities = entities.filter(function(e) {
                        let type = e.type === "Box" || e.type === "Sphere" ? "Shape" : e.type;
                        let typeFilter = typeFilters.indexOf(type) > -1;
                        let searchFilter = searchTerm === '' || (e.name.toLowerCase().indexOf(searchTerm) > -1 ||
                                                                 e.type.toLowerCase().indexOf(searchTerm) > -1 ||
                                                                 e.fullUrl.toLowerCase().indexOf(searchTerm) > -1 ||
                                                                 (e.urlWithPath.toLowerCase().indexOf(searchTerm) > -1 && 
                                                                 columnsByID["urlWithPath"].elTh.style.visibility === "visible") ||
                                                                 e.id.toLowerCase().indexOf(searchTerm) > -1);
                        return typeFilter && searchFilter;
                    });
                });

                PROFILE("sort", function() {
                    let isAscendingSort = currentSortOrder === ASCENDING_SORT;
                    let isDefaultSort = currentSortOrder === COLUMNS[currentSortColumnID].defaultSortOrder;
                    visibleEntities.sort((entityA, entityB) => {
                        /**
                         * If the default sort is ascending, empty should be considered largest.
                         * If the default sort is descending, empty should be considered smallest.
                         */
                        if (!isAscendingSort) {
                            [entityA, entityB] = [entityB, entityA];
                        }
                        let valueA = entityA[currentSortColumnID];
                        let valueB = entityB[currentSortColumnID];

                        if (valueA === valueB) {
                            return entityA.id < entityB.id ? -1 : 1;
                        }

                        if (isNullOrEmpty(valueA)) {
                            return (isDefaultSort ? 1 : -1) * (isAscendingSort ? 1 : -1);
                        }
                        if (isNullOrEmpty(valueB)) {
                            return (isDefaultSort ? -1 : 1) * (isAscendingSort ? 1 : -1);
                        }
                        if (typeof(valueA) === "string") {
                            return valueA.localeCompare(valueB);
                        }
                        return valueA < valueB ? -1 : 1;
                    });
                });

                PROFILE("update-dom", function() {
                    entityList.itemData = visibleEntities;
                    entityList.refresh();
                    updateColumnWidths();
                });
                
                refreshFooter();
                refreshNoEntitiesMessage();
            });
        }
        
        function removeEntities(deletedIDs) {
            // Loop from the back so we can pop items off while iterating
            
            // delete any entities matching deletedIDs list from entities and entitiesByID lists
            // if the entity had an associated row element then ensure row is unselected and clear it's entity
            for (let j = entities.length - 1; j >= 0; --j) {
                let id = entities[j].id;
                for (let i = 0, length = deletedIDs.length; i < length; ++i) {
                    if (id === deletedIDs[i]) {
                        let elRow = entities[j].elRow;
                        if (elRow) {
                            elRow.className = "";
                            elRow.dataset.entityID = EMPTY_ENTITY_ID;
                        }
                        entities.splice(j, 1);
                        delete entitiesByID[id];
                        break;
                    }
                }
            }
            
            // delete any entities matching deletedIDs list from selectedEntities list
            for (let j = selectedEntities.length - 1; j >= 0; --j) {
                let id = selectedEntities[j].id;
                for (let i = 0, length = deletedIDs.length; i < length; ++i) {
                    if (id === deletedIDs[i]) {
                        selectedEntities.splice(j, 1);
                        break;
                    }
                }
            }

            // delete any entities matching deletedIDs list from visibleEntities list
            // if this was a row that was above our current row offset (a hidden top row in the top buffer),
            // then decrease row offset accordingly
            let firstVisibleRow = entityList.getFirstVisibleRowIndex();
            for (let j = visibleEntities.length - 1; j >= 0; --j) {
                let id = visibleEntities[j].id;
                for (let i = 0, length = deletedIDs.length; i < length; ++i) {
                    if (id === deletedIDs[i]) {
                        if (j < firstVisibleRow && entityList.rowOffset > 0) {
                            entityList.rowOffset--;
                        }
                        visibleEntities.splice(j, 1);
                        break;
                    }
                }
            }
            
            entityList.refresh();
            
            refreshFooter();
            refreshNoEntitiesMessage();
        }
        
        function clearEntities() {
            // clear the associated entity ID from all visible row elements
            let firstVisibleRow = entityList.getFirstVisibleRowIndex();
            let lastVisibleRow = entityList.getLastVisibleRowIndex();
            for (let i = firstVisibleRow; i <= lastVisibleRow && i < visibleEntities.length; i++) {
                let entity = visibleEntities[i];
                entity.elRow.dataset.entityID = EMPTY_ENTITY_ID;
            }
            
            entities = [];
            entitiesByID = {};
            visibleEntities.length = 0; // maintains itemData reference in ListView
            
            entityList.resetToTop();
            entityList.clear();
            
            refreshFooter();
            refreshNoEntitiesMessage();
        }

        function setSortColumn(columnID) {
            PROFILE("set-sort-column", function() {
                if (currentSortColumnID === columnID) {
                    currentSortOrder *= -1;
                } else {
                    elSortOrders[currentSortColumnID].innerHTML = "";
                    currentSortColumnID = columnID;
                    currentSortOrder = COLUMNS[currentSortColumnID].defaultSortOrder;
                }
                refreshSortOrder();
                refreshEntityList();
            });
        }
        
        function refreshSortOrder() {
            elSortOrders[currentSortColumnID].innerHTML = currentSortOrder === ASCENDING_SORT ? ASCENDING_STRING : DESCENDING_STRING;
        }
        
        function refreshEntities() {
            EventBridge.emitWebEvent(JSON.stringify({ type: "refresh" }));
        }
        
        function refreshFooter() {
            elSelectedEntitiesCount.innerText = selectedEntities.length;
            elVisibleEntitiesCount.innerText = visibleEntities.length;
        }
        
        function refreshNoEntitiesMessage() {
            if (visibleEntities.length > 0) {
                elNoEntitiesMessage.style.display = "none";
            } else {
                elNoEntitiesMessage.style.display = "block";
            }
        }
        
        function updateSelectedEntities(selectedIDs, autoScroll) {
            let notFound = false;

            lastSelectedEntity = selectedIDs[selectedIDs.length - 1];

            // reset all currently selected entities and their rows first
            selectedEntities.forEach(function(id) {
                let entity = entitiesByID[id];
                if (entity !== undefined) {
                    entity.selected = false;
                    if (entity.elRow) {
                        entity.elRow.className = "";
                    }
                }
            });

            // then reset selected entities list with newly selected entities and set them selected
            selectedEntities = [];
            selectedIDs.forEach(function(id) {
                selectedEntities.push(id);
                let entity = entitiesByID[id];
                if (entity !== undefined) {
                    entity.selected = true;
                    if (entity.elRow) {
                        if (id === lastSelectedEntity) {
                            entity.elRow.className = "last-selected";
                        } else {
                            entity.elRow.className = "selected";
                        }
                    }
                } else {
                    notFound = true;
                }
            });

            if (autoScroll && selectedIDs.length > 0) {
                let firstItem = Number.MAX_VALUE;
                let lastItem = -1;
                let itemFound = false;
                visibleEntities.forEach(function(entity, index) {
                    if (selectedIDs.indexOf(entity.id) !== -1) {
                        if (firstItem > index) {
                            firstItem = index;
                        }
                        if (lastItem < index) {
                            lastItem = index;
                        }
                        itemFound = true;
                    }
                });
                if (itemFound) {
                    entityList.scrollToRow(firstItem, lastItem);
                }
            }

            elToggleSpaceMode.disabled = selectedIDs.length > 1;

            refreshFooter();

            return notFound;
        }
        
        function createRow() {
            let elRow = document.createElement("tr");
            columns.forEach(function(column) {
                let elRowColumn = document.createElement("td");
                elRowColumn.className = createColumnClassName(column.columnID);
                elRow.appendChild(elRowColumn);
            });
            elRow.oncontextmenu = onRowContextMenu;
            elRow.onclick = onRowClicked;
            elRow.ondblclick = onRowDoubleClicked;
            return elRow;
        }
        
        function updateRow(elRow, itemData) {
            // update all column texts and glyphs to this entity's data
            for (let i = 0; i < columns.length; ++i) {
                let column = columns[i];
                let elCell = elRow.childNodes[i];
                if (column.data.glyph) {
                    elCell.innerHTML = itemData[column.data.propertyID] ? column.data.columnHeader : null;
                } else if (column.data.vglyph) {
                    elCell.innerHTML = itemData[column.data.propertyID];
                } else {
                    let value = itemData[column.data.propertyID];
                    if (column.data.format) {
                        value = column.data.format(value);
                    }
                    elCell.innerHTML = value;
                }
                elCell.style = "min-width:" + column.widthPx + "px;" + "max-width:" + column.widthPx + "px;";
                elCell.className = createColumnClassName(column.columnID);
            }

            // if this entity was previously selected flag it's row as selected
            if (itemData.selected) {
                if (itemData.id === lastSelectedEntity) {
                    elRow.className = "last-selected";
                } else {
                    elRow.className = "selected";
                }
            } else {
                elRow.className = "";
            }

            // if this row previously had an associated entity ID that wasn't the new entity ID then clear
            // the ID from the row and the row element from the previous entity's data, then set the new 
            // entity ID to the row and the row element to the new entity's data
            let prevEntityID = elRow.dataset.entityID;
            let newEntityID = itemData.id;
            let validPrevItemID = prevEntityID !== undefined && prevEntityID !== EMPTY_ENTITY_ID;
            if (validPrevItemID && prevEntityID !== newEntityID && entitiesByID[prevEntityID].elRow === elRow) {
                elRow.dataset.entityID = EMPTY_ENTITY_ID;
                entitiesByID[prevEntityID].elRow = null;
            }
            if (!validPrevItemID || prevEntityID !== newEntityID) {
                elRow.dataset.entityID = newEntityID;
                entitiesByID[newEntityID].elRow = elRow;
            }
        }
        
        function clearRow(elRow) {
            // reset all texts and glyphs for each of the row's columns
            for (let i = 0; i < columns.length; ++i) {
                let cell = elRow.childNodes[i];
                if (columns[i].data.glyph) {
                    cell.innerHTML = "";
                } else {
                    cell.innerText = "";
                }
            }

            // clear the row from any associated entity
            let entityID = elRow.dataset.entityID;
            if (entityID && entitiesByID[entityID]) {
                entitiesByID[entityID].elRow = null;
            }
            
            // reset the row to hidden and clear the entity from the row
            elRow.className = '';
            elRow.dataset.entityID = EMPTY_ENTITY_ID;
        }
        
        function onToggleFilterInView() {
            isFilterInView = !isFilterInView;
            if (isFilterInView) {
                elFilterInView.setAttribute(FILTER_IN_VIEW_ATTRIBUTE, FILTER_IN_VIEW_ATTRIBUTE);
            } else {
                elFilterInView.removeAttribute(FILTER_IN_VIEW_ATTRIBUTE);
            }
            EventBridge.emitWebEvent(JSON.stringify({ type: "filterInView", filterInView: isFilterInView }));
            refreshEntities();
        }
        
        function onRadiusChange() {
            elFilterRadius.value = elFilterRadius.value.replace(/[^0-9]/g, "");
            elFilterRadius.value = Math.max(elFilterRadius.value, 0);
            EventBridge.emitWebEvent(JSON.stringify({ type: "radius", radius: elFilterRadius.value }));
            refreshEntities();
        }
        
        function getColumnIndex(columnID) {
            for (let i = 0; i < columns.length; ++i) {
                if (columns[i].columnID === columnID) {
                    return i;
                }
            }
            return -1;
        }
        
        function createColumnClassName(columnID) {
            let column = columnsByID[columnID];
            let visible = column.elTh.style.visibility !== "hidden";
            let className = column.data.glyph ? "glyph" : "";
            if (column.data.vglyph) {
                className = "vglyph";
            }
            className += visible ? "" : " hidden";
            return className;
        }
        
        function isColumnsDropdownVisible() {
            return elColumnsOptions.style.display === "block";
        }
        
        function toggleColumnsDropdown() {
            elColumnsOptions.style.display = isColumnsDropdownVisible() ? "none" : "block";
        }
        
        function onToggleColumnsDropdown(event) {
            toggleColumnsDropdown();
            if (isTypeDropdownVisible()) {
                toggleTypeDropdown();
            }
            event.stopPropagation();
        }
        
        function toggleColumn(elInput, refresh) {
            let columnID = elInput.getAttribute("columnID");
            let columnChecked = elInput.checked;
            
            if (columnChecked) {
                let widthNeeded = columnsByID[columnID].data.initialWidth;
                
                let numberVisibleColumns = 0;
                for (let i = 0; i < columns.length; ++i) {
                    let column = columns[i];
                    if (column.columnID === columnID) {
                        column.width = widthNeeded;
                    } else if (column.width > 0) {
                        ++numberVisibleColumns;
                    }
                }
                
                for (let i = 0; i < columns.length; ++i) {
                    let column = columns[i];
                    if (column.columnID !== columnID && column.width > 0) {
                        column.width -= column.width * widthNeeded;
                    }
                }
            } else {
                let widthLoss = 0;
                
                let numberVisibleColumns = 0;
                for (let i = 0; i < columns.length; ++i) {
                    let column = columns[i];
                    if (column.columnID === columnID) {
                        widthLoss = column.width;
                        column.width = 0;
                    } else if (column.width > 0) {
                        ++numberVisibleColumns;
                    }
                }
                
                for (let i = 0; i < columns.length; ++i) {
                    let column = columns[i];
                    if (column.columnID !== columnID && column.width > 0) {
                        let newTotalWidth = (1 - widthLoss);
                        column.width += (column.width / newTotalWidth) * widthLoss;
                    }
                }
            }
            
            updateColumnWidths();
        }
        
        function onToggleColumn(event) {
            let elTarget = event.target;
            if (elTarget instanceof HTMLInputElement) {
                toggleColumn(elTarget, true);
            }
            event.stopPropagation();
        }
        
        function isTypeDropdownVisible() {
            return elFilterTypeOptions.style.display === "block";
        }
        
        function toggleTypeDropdown() {
            elFilterTypeOptions.style.display = isTypeDropdownVisible() ? "none" : "block";
        }
        
        function onToggleTypeDropdown(event) {
            toggleTypeDropdown();
            if (isColumnsDropdownVisible()) {
                toggleColumnsDropdown();
            }
            event.stopPropagation();
        }
        
        function refreshTypeFilter(refreshList) {
            if (typeFilters.length === 0) {
                elFilterTypeText.innerText = "No Types";
            } else if (typeFilters.length === FILTER_TYPES.length) {
                elFilterTypeText.innerText = "All Types";
            } else {
                elFilterTypeText.innerText = "Types...";
            }
            
            if (refreshList) {
                refreshEntityList();
            }
        }
        
        function toggleTypeFilter(elInput, refreshList) {
            let type = elInput.getAttribute("filterType");
            let typeChecked = elInput.checked;
            
            let typeFilterIndex = typeFilters.indexOf(type);
            if (!typeChecked && typeFilterIndex > -1) {
                typeFilters.splice(typeFilterIndex, 1);
            } else if (typeChecked && typeFilterIndex === -1) {
                typeFilters.push(type);
            }
            
            refreshTypeFilter(refreshList);
        }
        
        function onToggleTypeFilter(event) {
            let elTarget = event.target;
            if (elTarget instanceof HTMLInputElement) {
                toggleTypeFilter(elTarget, true);
            }
            event.stopPropagation();
        }
        
        function onSelectAllTypes(event) {
            for (let type in elFilterTypeInputs) {
                elFilterTypeInputs[type].checked = true;
            }
            typeFilters = FILTER_TYPES;
            refreshTypeFilter(true);
            event.stopPropagation();
        }
        
        function onClearAllTypes(event) {
            for (let type in elFilterTypeInputs) {
                elFilterTypeInputs[type].checked = false;
            }
            typeFilters = [];
            refreshTypeFilter(true);
            event.stopPropagation();
        }
        
        function onBodyClick(event) {
            // if clicking anywhere outside of the multiselect dropdowns (since click event bubbled up to onBodyClick and
            // propagation wasn't stopped in the toggle type/column callbacks) and the dropdown is open then close it
            if (isTypeDropdownVisible()) {
                toggleTypeDropdown();
            }
            if (isColumnsDropdownVisible()) {
                toggleColumnsDropdown();
            }
        }
        
        function onStartResize(event) {
            lastResizeEvent = event;
            resizeColumnIndex = parseInt(this.parentNode.getAttribute("columnIndex"));
            event.stopPropagation();
        }
        
        function updateColumnWidths() {
            let fullWidth = elEntityTableBody.offsetWidth;
            let remainingWidth = fullWidth;
            let scrollbarVisible = elEntityTableScroll.scrollHeight > elEntityTableScroll.clientHeight;
            let resizerRight = scrollbarVisible ? SCROLLBAR_WIDTH - RESIZER_WIDTH/2 : -RESIZER_WIDTH/2;
            let visibleColumns = 0;
                        
            for (let i = columns.length - 1; i > 0; --i) {
                let column = columns[i];
                column.widthPx = Math.ceil(column.width * fullWidth);
                column.elTh.style = "min-width:" + column.widthPx + "px;" + "max-width:" + column.widthPx + "px;";
                let columnVisible = column.width > 0;
                column.elTh.style.visibility = columnVisible ? "visible" : "hidden";
                if (column.elResizer) {
                    column.elResizer.style = "right:" + resizerRight + "px;";
                    column.elResizer.style.visibility = columnVisible && visibleColumns > 0 ? "visible" : "hidden";
                }
                resizerRight += column.widthPx;
                remainingWidth -= column.widthPx;
                if (columnVisible) {
                    ++visibleColumns;
                }
            }
            
            // assign all remaining space to the first column
            let column = columns[0];
            column.widthPx = remainingWidth;
            column.width = remainingWidth / fullWidth;
            column.elTh.style = "min-width:" + column.widthPx + "px;" + "max-width:" + column.widthPx + "px;";
            let columnVisible = column.width > 0;
            column.elTh.style.visibility = columnVisible ? "visible" : "hidden";
            if (column.elResizer) {
                column.elResizer.style = "right:" + resizerRight + "px;";
                column.elResizer.style.visibility = columnVisible && visibleColumns > 0 ? "visible" : "hidden";
            }
            
            if (isColumnsSettingLoaded) {
                EventBridge.emitWebEvent(JSON.stringify({ type: "saveColumnsConfigSetting", columnsData: columns }));
            }
            
            entityList.refresh();
        }
        
        function swapColumns(columnAIndex, columnBIndex) {
            let columnA = columns[columnAIndex];
            let columnB = columns[columnBIndex];
            let columnATh = columns[columnAIndex].elTh;
            let columnBTh = columns[columnBIndex].elTh;
            let columnThParent = columnATh.parentNode;
            columnThParent.removeChild(columnBTh);
            columnThParent.insertBefore(columnBTh, columnATh);
            columnATh.setAttribute("columnIndex", columnBIndex);
            columnBTh.setAttribute("columnIndex", columnAIndex);
            columnA.elResizer.setAttribute("columnIndex", columnBIndex);
            columnB.elResizer.setAttribute("columnIndex", columnAIndex);
            
            for (let i = 0; i < visibleEntities.length; ++i) {
                let elRow = visibleEntities[i].elRow;
                if (elRow) {
                    let columnACell = elRow.childNodes[columnAIndex];
                    let columnBCell = elRow.childNodes[columnBIndex];
                    elRow.removeChild(columnBCell);
                    elRow.insertBefore(columnBCell, columnACell);
                }
            }
            
            columns[columnAIndex] = columnB;
            columns[columnBIndex] = columnA;
            
            updateColumnWidths();
        }
        
        document.onmousemove = function(event) {
            if (lastResizeEvent) {
                startTh = null;
                
                let column = columns[resizeColumnIndex];
                
                let nextColumnIndex = resizeColumnIndex + 1;
                let nextColumn = columns[nextColumnIndex];
                while (nextColumn.width === 0) {
                    nextColumn = columns[++nextColumnIndex];
                }

                let fullWidth = elEntityTableBody.offsetWidth;
                let dx = event.clientX - lastResizeEvent.clientX;
                let dPct = dx / fullWidth;
                
                let newColWidth = column.width + dPct;
                let newNextColWidth = nextColumn.width - dPct;
                
                if (newColWidth * fullWidth >= MINIMUM_COLUMN_WIDTH && newNextColWidth * fullWidth >= MINIMUM_COLUMN_WIDTH) {
                    column.width += dPct;
                    nextColumn.width -= dPct;
                    updateColumnWidths();
                    lastResizeEvent = event;
                }
            } else if (elTargetTh) {
                let dxFromInitial = event.clientX - initialThEvent.clientX;
                if (Math.abs(dxFromInitial) >= DELTA_X_MOVE_COLUMNS_THRESHOLD) {
                    elTargetTh.className = "dragging";
                }
                if (targetColumnIndex < columns.length - 1) {
                    let nextColumnIndex = targetColumnIndex + 1;
                    let nextColumnTh = columns[nextColumnIndex].elTh;
                    let nextColumnStartX = nextColumnTh.getBoundingClientRect().left;
                    if (event.clientX >= nextColumnStartX && event.clientX - lastColumnSwapPosition >= DELTA_X_COLUMN_SWAP_POSITION) {
                        swapColumns(targetColumnIndex, nextColumnIndex);
                        targetColumnIndex = nextColumnIndex;
                        lastColumnSwapPosition = event.clientX;
                    }
                }
                if (targetColumnIndex >= 1) {
                    let prevColumnIndex = targetColumnIndex - 1;
                    let prevColumnTh = columns[prevColumnIndex].elTh;
                    let prevColumnEndX = prevColumnTh.getBoundingClientRect().right;
                    if (event.clientX <= prevColumnEndX && lastColumnSwapPosition - event.clientX >= DELTA_X_COLUMN_SWAP_POSITION) {
                        swapColumns(prevColumnIndex, targetColumnIndex);
                        targetColumnIndex = prevColumnIndex;
                        lastColumnSwapPosition = event.clientX;
                    }
                }
            } else if (elTargetSpan) {
                let dxFromInitial = event.clientX - initialThEvent.clientX;
                if (Math.abs(dxFromInitial) >= DELTA_X_MOVE_COLUMNS_THRESHOLD) {
                    elTargetTh = elTargetSpan.parentNode;
                    elTargetTh.className = "dragging";
                    targetColumnIndex = parseInt(elTargetTh.getAttribute("columnIndex"));
                    lastColumnSwapPosition = event.clientX;
                    elTargetSpan = null;
                }
            }
        };
        
        document.onmouseup = function(event) {
            if (elTargetTh) {
                if (elTargetTh.className !== "dragging" && elTargetTh === event.target) {
                    let columnID = elTargetTh.getAttribute("columnID");
                    setSortColumn(columnID);
                }
                elTargetTh.className = "";
            } else if (elTargetSpan) {
                let columnID = elTargetSpan.parentNode.getAttribute("columnID");
                setSortColumn(columnID);
            }
            lastResizeEvent = null;
            elTargetTh = null;
            elTargetSpan = null;
            initialThEvent = null;
        };

        function setSpaceMode(spaceMode) {
            if (spaceMode === "local") {
                elToggleSpaceMode.className = "space-mode-local hifi-edit-button";
                elToggleSpaceMode.innerText = "Local";
            } else {
                elToggleSpaceMode.className = "space-mode-world hifi-edit-button";
                elToggleSpaceMode.innerText = "World";
            }
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

            if (controlKey && !shiftKey && !altKey && keyCodeString === "A") {
                let visibleEntityIDs = visibleEntities.map(visibleEntity => visibleEntity.id);
                let selectionIncludesAllVisibleEntityIDs = visibleEntityIDs.every(visibleEntityID => {
                    return selectedEntities.includes(visibleEntityID);
                });

                let selection = [];

                if (!selectionIncludesAllVisibleEntityIDs) {
                    selection = visibleEntityIDs;
                }

                updateSelectedEntities(selection);

                EventBridge.emitWebEvent(JSON.stringify({
                    type: "selectionUpdate",
                    focus: false,
                    entityIds: selection,
                }));

                return;
            }

            if (controlKey && !shiftKey && !altKey && keyCodeString === "I") {
                let visibleEntityIDs = visibleEntities.map(visibleEntity => visibleEntity.id);
                let selectionIncludesAllVisibleEntityIDs = visibleEntityIDs.every(visibleEntityID => {
                    return selectedEntities.includes(visibleEntityID);
                });

                let selection = [];

                if (!selectionIncludesAllVisibleEntityIDs) {
                    visibleEntityIDs.forEach(function(id) {
                        if (!selectedEntities.includes(id)) {
                            selection.push(id);
                        }
                    });
                }

                updateSelectedEntities(selection);

                EventBridge.emitWebEvent(JSON.stringify({
                    "type": "selectionUpdate",
                    "focus": false,
                    "entityIds": selection
                }));
                
                return;
            }

            EventBridge.emitWebEvent(JSON.stringify({
                type: "keyUpEvent",
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

        if (window.EventBridge !== undefined) {
            EventBridge.scriptEventReceived.connect(function(data) {
                
                data = JSON.parse(data);
                
                if (data.type === "clearEntityList") {
                    clearEntities();
                } else if (data.type === "selectionUpdate") {
                    let notFound = updateSelectedEntities(data.selectedIDs, true);
                    if (notFound) {
                        refreshEntities();
                    }
                } else if (data.type === "update" && data.selectedIDs !== undefined) {
                    PROFILE("update", function() {
                        let newEntities = data.entities;
                        if (newEntities) {
                            if (newEntities.length === 0) {
                                clearEntities();
                            } else {
                                updateEntityData(newEntities);
                                updateSelectedEntities(data.selectedIDs, true);
                            }
                        }
                        setSpaceMode(data.spaceMode);
                    });
                } else if (data.type === "removeEntities" && data.deletedIDs !== undefined && data.selectedIDs !== undefined) {
                    removeEntities(data.deletedIDs);
                    updateSelectedEntities(data.selectedIDs, true);
                } else if (data.type === "deleted" && data.ids) {
                    removeEntities(data.ids);
                } else if (data.type === "setSpaceMode") {
                    setSpaceMode(data.spaceMode);
                } else if (data.type === "setSnapToGrid") {
                    if (data.snap) { 
                        elSnapToGridActivatorCaption.innerHTML = "&#x2713; Deactivate Snap to Grid";
                    } else {
                        elSnapToGridActivatorCaption.innerHTML = "Activate Snap to Grid";
                    }
                } else if (data.type === "confirmHMDstate") {
                    if (data.isHmd) {
                        document.getElementById("hmdmultiselect").style.display = "inline";
                    } else {
                        document.getElementById("hmdmultiselect").style.display = "none";                      
                    }
                } else if (data.type === "loadedConfigSetting") {
                    if (typeof(data.defaultRadius) === "number") {
                        elFilterRadius.value = data.defaultRadius;
                        onRadiusChange();
                    }
                    if (data.columnsData !== "NO_DATA" && typeof(data.columnsData) === "object") {
                        var isValid = true;
                        var originalColumnIDs = [];
                        for (let originalColumnID in COLUMNS) {
                            originalColumnIDs.push(originalColumnID);
                        }                        
                        for (let columnSetupIndex in data.columnsData) {
                            var checkPresence = originalColumnIDs.indexOf(data.columnsData[columnSetupIndex].columnID);
                            if (checkPresence === -1) {
                                isValid = false;
                                break;
                            }
                        }
                        if (isValid) {
                            for (var columnIndex = 0; columnIndex < data.columnsData.length; columnIndex++) {
                                if (data.columnsData[columnIndex].data.alwaysShown !== true) {
                                    var columnDropdownID = "entity-table-column-" + data.columnsData[columnIndex].columnID;
                                    if (data.columnsData[columnIndex].width !== 0) {
                                        document.getElementById(columnDropdownID).checked = false;
                                        document.getElementById(columnDropdownID).click();
                                    } else {
                                        document.getElementById(columnDropdownID).checked = true;
                                        document.getElementById(columnDropdownID).click();
                                    }
                                }
                            }
                            for (columnIndex = 0; columnIndex < data.columnsData.length; columnIndex++) {
                                let currentColumnIndex = originalColumnIDs.indexOf(data.columnsData[columnIndex].columnID);
                                if (currentColumnIndex !== -1 && columnIndex !== currentColumnIndex) {
                                    for (var i = currentColumnIndex; i > columnIndex; i--) {
                                        swapColumns(i - 1, i);
                                        var swappedContent = originalColumnIDs[i - 1];  
                                        originalColumnIDs[i - 1] = originalColumnIDs[i];  
                                        originalColumnIDs[i] = swappedContent;                                        
                                    }
                                }
                            }
                        } else {
                            EventBridge.emitWebEvent(JSON.stringify({ type: "saveColumnsConfigSetting", columnsData: "" }));
                        }
                    }
                    isColumnsSettingLoaded = true;
                }
            });
        }

        refreshSortOrder();
        refreshEntities();
        
        window.addEventListener("resize", updateColumnWidths);
        
        EventBridge.emitWebEvent(JSON.stringify({ type: "loadConfigSetting" }));
    });
    
    augmentSpinButtons();
    disableDragDrop();

    document.addEventListener("contextmenu", function(event) {
        entityListContextMenu.close();

        // Disable default right-click context menu which is not visible in the HMD and makes it seem like the app has locked
        event.preventDefault();
    }, false);

    // close context menu when switching focus to another window
    $(window).blur(function() {
        entityListContextMenu.close();
        closeAllEntityListMenu();
    });
    
    function closeAllEntityListMenu() {
        document.getElementById("menuBackgroundOverlay").style.display = "none";
        document.getElementById("selection-menu").style.display = "none";
        document.getElementById("actions-menu").style.display = "none";
        document.getElementById("transform-menu").style.display = "none";
        document.getElementById("tools-menu").style.display = "none";
    }

}
