//
//  ScriptsModel.h
//  interface/src
//
//  Created by Ryan Huffman on 05/12/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptsModel_h
#define hifi_ScriptsModel_h

#include <QAbstractItemModel>
#include <QDir>
#include <QNetworkReply>
#include <QFileSystemWatcher>
#include <QtCore/QSharedPointer>

class TreeNodeFolder;
class ScriptValue;
using ScriptValuePointer = QSharedPointer<ScriptValue>;

enum ScriptOrigin {
    SCRIPT_ORIGIN_LOCAL,
    SCRIPT_ORIGIN_DEFAULT
};

enum TreeNodeType {
    TREE_NODE_TYPE_SCRIPT,
    TREE_NODE_TYPE_FOLDER
};

class TreeNodeBase {
public:
    TreeNodeFolder* getParent() const { return _parent; }
    void setParent(TreeNodeFolder* parent) { _parent = parent; }
    TreeNodeType getType() { return _type; }
    const QString& getName() { return _name; };
    virtual ~TreeNodeBase() = default;

private:
    TreeNodeFolder* _parent;
    TreeNodeType _type;

protected:
    QString _name;
    TreeNodeBase(TreeNodeFolder* parent, const QString& name, TreeNodeType type);
};

class TreeNodeScript : public TreeNodeBase {
public:
    TreeNodeScript(const QString& localPath, const QString& fullPath, ScriptOrigin origin);
    const QString& getLocalPath() { return _localPath; }
    const QString& getFullPath() { return _fullPath; };
    ScriptOrigin getOrigin() { return _origin; };

private:
    QString _localPath;
    QString _fullPath;
    ScriptOrigin _origin;
};

class TreeNodeFolder : public TreeNodeBase {
public:
    TreeNodeFolder(const QString& foldername, TreeNodeFolder* parent);
};

/*@jsdoc
 * Information on the scripts that are in the default scripts directory of the Interface installation. This is provided as a 
 * property of {@link ScriptDiscoveryService}.
 *
 * <p>The information provided reflects the subdirectory structure. Methods and signals are per QT's 
 * <a href="http://doc.qt.io/qt-5/qabstractitemmodel.html">QAbstractItemModel</a> class, with the following details:</p>
 * <ul>
 *   <li>A single column of data: <code>columnCount(index)</code> returns <code>1</code>. </li>
 *   <li>Data is provided for the following roles:
 *     <table>
 *       <thead>
 *         <tr><th>Role</th><th>Value</th><th>Description</th></tr>
 *       </thead>
 *       <tbody>
 *         <tr><td>Display</td><td><code>0</code></td><td>The directory or script file name.</td></tr>
 *         <tr><td>Path</td><td><code>256</code></td><td>The path and filename of the data item if it is a script, 
 *         <code>undefined</code> if it is a directory.</td></tr>
 *       </tbody>
 *     </table>
 *   </li>
 *   <li>Use <code>null</code> for the root directory's index.</li>
 * </ul>
 *
 * @class ScriptsModel
 * @hideconstructor
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @example <caption>List the first 2 levels of the scripts directory.</caption>
 * var MAX_DIRECTORY_LEVEL = 1;
 * var DISPLAY_ROLE = 0;
 * var PATH_ROLE = 256;
 * 
 * function printDirectory(parentIndex, directoryLevel, indent) {
 *     var numRows = ScriptDiscoveryService.scriptsModel.rowCount(parentIndex);
 *     for (var i = 0; i < numRows; i++) {
 *         var rowIndex = ScriptDiscoveryService.scriptsModel.index(i, 0, parentIndex);
 * 
 *         var name = ScriptDiscoveryService.scriptsModel.data(rowIndex, DISPLAY_ROLE);
 *         var hasChildren = ScriptDiscoveryService.scriptsModel.hasChildren(rowIndex);
 *         var path = hasChildren ? "" : ScriptDiscoveryService.scriptsModel.data(rowIndex, PATH_ROLE);
 * 
 *         print(indent + "- " + name + (hasChildren ? "" : " - " + path));
 * 
 *         if (hasChildren && directoryLevel < MAX_DIRECTORY_LEVEL) {
 *             printDirectory(rowIndex, directoryLevel + 1, indent + "    ");
 *         }
 *     }
 * }
 * 
 * print("Scripts:");
 * printDirectory(null, 0, "");  // null index for the root directory.
 */
class ScriptsModel : public QAbstractItemModel {
    Q_OBJECT
public:
    ScriptsModel(QObject* parent = NULL);
    ~ScriptsModel();

    // No JSDoc because the particulars of the parent class is provided in the @class description.
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;

    // No JSDoc because the particulars of the parent class is provided in the @class description.
    QModelIndex parent(const QModelIndex& child) const override;

    // No JSDoc because the particulars of the parent class is provided in the @class description.
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // No JSDoc because the particulars of the parent class is provided in the @class description.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    // No JSDoc because the particulars of the parent class is provided in the @class description.
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    // Not exposed in the API because no conversion between TreeNodeBase and ScriptValuePointer is provided.
    TreeNodeBase* getTreeNodeFromIndex(const QModelIndex& index) const;

    // Not exposed in the API because no conversion between TreeNodeBase and ScriptValuePointer is provided.
    QList<TreeNodeBase*> getFolderNodes(TreeNodeFolder* parent) const;

    enum Role {
        ScriptPath = Qt::UserRole,
    };

protected slots:

    /*@jsdoc
     * @function ScriptsModel.updateScriptsLocation
     * @param {string} newPath - New path.
     * @deprecated This method is deprecated and will be removed from the API.
     */
    void updateScriptsLocation(const QString& newPath);

    /*@jsdoc
     * @function ScriptsModel.downloadFinished
     * @deprecated This method is deprecated and will be removed from the API.
     */
    void downloadFinished();

    /*@jsdoc
     * @function ScriptsModel.reloadLocalFiles
     * @deprecated This method is deprecated and will be removed from the API.
     */
    void reloadLocalFiles();

    /*@jsdoc
     * @function ScriptsModel.reloadDefaultFiles
     * @deprecated This method is deprecated and will be removed from the API.
     */
    void reloadDefaultFiles();

protected:
    void requestDefaultFiles(QString marker = QString());
    bool parseXML(QByteArray xmlFile);
    void rebuildTree();

private:
    friend class ScriptEngines;
    bool _loadingScripts;
    QDir _localDirectory;
    QFileSystemWatcher _fsWatcher;
    QList<TreeNodeBase*> _treeNodes;
};

#endif // hifi_ScriptsModel_h
