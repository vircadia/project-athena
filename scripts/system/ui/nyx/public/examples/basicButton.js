(function () {
    var NyxAlpha1 = Script.require('../nyx-helpers.js?ds3dsa545');

    var _entityID;

    function onEntityMenuActionTriggered(triggeredEntityID, command, data) {
        if (data.name === 'Create Cube' && triggeredEntityID === _entityID) {
            Entities.addEntity({
                type: "Box",
                position: Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0, y: 0, z: -5 })),
                rotation: MyAvatar.orientation,
                dimensions: { x: 0.5, y: 0.5, z: 0.5 },
                lifetime: 300  // Delete after 5 minutes.
            });
        }
    }

    this.preload = function (entityID) {
        _entityID = entityID;
        
        NyxAlpha1.registerWithEntityMenu(entityID, [
            {
                type: 'button',
                name: 'Create Cube'
            }
        ]);

        NyxAlpha1.entityMenuActionTriggered.connect(_entityID, onEntityMenuActionTriggered);
    };

    this.unload = function () {
        NyxAlpha1.entityMenuActionTriggered.disconnect(_entityID, onEntityMenuActionTriggered);
    };

});