(function () {
    var NyxAlpha1 = Script.require('../nyx-helpers.js?12dsadsddsadsdfafdasdassaddseras3');

    var _entityID;
    
    function onEntityMenuActionTriggered(triggeredEntityID, command, data) {
        if (data.name === 'TESTFIELD' && triggeredEntityID === _entityID) {
            Entities.editEntity(_entityID, { 
                text: data.value
            });
        }
    }

    this.preload = function (entityID) {
        _entityID = entityID;

        var initialProps = Entities.getEntityProperties(_entityID, ['text']);
        
        NyxAlpha1.registerWithEntityMenu(_entityID, [
            {
                type: 'textField',
                name: 'TESTFIELD',
                hint: 'just smile!',
                initialValue: initialProps.text
            }
        ]);
        NyxAlpha1.entityMenuActionTriggered.connect(_entityID, onEntityMenuActionTriggered);
    };

    this.unload = function () {
        NyxAlpha1.entityMenuActionTriggered.connect(_entityID, onEntityMenuActionTriggered);
    };

});