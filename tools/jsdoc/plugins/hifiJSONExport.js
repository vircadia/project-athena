exports.handlers = {
    processingComplete: function(e) {
        const pathTools = require('path');
        var outputFolder;
        var doclets = e.doclets.map(doclet => Object.assign({}, doclet));

        var argv = process.argv;
        for (var i = 0; i < argv.length; i++) {
            if (argv[i] === '-d') {
                outputFolder = argv[i+1];
                break;
            }
        }

        if (!outputFolder) {
            console.log("Output folder not found, specify it with -d");
            process.exit(1);
        }

        const fs = require('fs');
        if (!fs.existsSync(outputFolder)) {
            fs.mkdirSync(outputFolder);
        }
        doclets.map(doclet => {delete doclet.meta; delete doclet.comment});
        fs.writeFile(pathTools.join(outputFolder, "hifiJSDoc.json"), JSON.stringify(doclets, null, 4), function(err) {
            if (err) {
                return console.log(err);
            }

            console.log("The Vircadia JSDoc JSON was saved into " + outputFolder + "!");
        });
    }
};
