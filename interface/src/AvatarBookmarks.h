//
//  AvatarBookmarks.h
//  interface/src
//
//  Created by Triplelexx on 23/03/17.
//  Copyright 2017 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AvatarBookmarks_h
#define hifi_AvatarBookmarks_h

#include <DependencyManager.h>
#include "Bookmarks.h"

/*@jsdoc 
 * The <code>AvatarBookmarks</code> API provides facilities for working with avatar bookmarks ("favorites" in the Avatar app). 
 * An avatar bookmark associates a name with an avatar model, scale, and avatar entities (wearables).
 *
 * @namespace AvatarBookmarks
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 */

class AvatarBookmarks: public Bookmarks, public  Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    AvatarBookmarks();
    void setupMenus(Menu* menubar, MenuWrapper* menu) override {};

    /*@jsdoc
     * Gets the details of an avatar bookmark.
     * @function AvatarBookmarks.getBookmark
     * @param {string} bookmarkName - The name of the avatar bookmark (case sensitive).
     * @returns {AvatarBookmarks.BookmarkData|{}} The bookmark data if the bookmark exists, <code>{}</code> if it doesn't.
     */
    Q_INVOKABLE QVariantMap getBookmark(const QString& bookmarkName);

public slots:
    /*@jsdoc 
     * Adds a new (or updates an existing) avatar bookmark with your current avatar model, scale, and avatar entities.
     * @function AvatarBookmarks.addBookmark
     * @param {string} bookmarkName - The name of the avatar bookmark (case sensitive).
     * @param {string} [avatarIcon=""] - A URL to an icon for the avatar bookmark.
     * @example <caption>Add a new avatar bookmark and report the bookmark data.</caption>
     * var bookmarkName = "New Bookmark";
     * AvatarBookmarks.addBookmark(bookmarkName);
     * var bookmarkData = AvatarBookmarks.getBookmark(bookmarkName);
     * print("Bookmark data: " + JSON.stringify(bookmarkData));
     */
    void addBookmark(const QString& bookmarkName, const QString& avatarIcon = "");

    /*@jsdoc
     * Updates an existing bookmark with your current avatar model, scale, and wearables. No action is taken if the bookmark 
     * doesn't exist.
     * @function AvatarBookmarks.saveBookmark
     * @param {string} bookmarkName - The name of the avatar bookmark (case sensitive).
     * @param {string} [avatarIcon=""] - A URL to an icon for the avatar bookmark.
     */
    void saveBookmark(const QString& bookmarkName, const QString& avatarIcon = "");

    /*@jsdoc
     * Loads an avatar bookmark, setting your avatar model, scale, and avatar entities (or attachments if an old bookmark) to 
     * those in the bookmark.
     * @function AvatarBookmarks.loadBookmark
     * @param {string} bookmarkName - The name of the avatar bookmark to load (case sensitive).
     */
    void loadBookmark(const QString& bookmarkName);

    /*@jsdoc
     * Deletes an avatar bookmark.
     * @function AvatarBookmarks.removeBookmark
     * @param {string} bookmarkName - The name of the avatar bookmark to delete (case sensitive).
     */
    void removeBookmark(const QString& bookmarkName);

    /*@jsdoc
     * Updates the avatar entities and their properties. Current avatar entities not included in the list provided are deleted.
     * @function AvatarBookmarks.updateAvatarEntities
     * @param {MyAvatar.AvatarEntityData[]} avatarEntities - The avatar entity IDs and properties.
     * @deprecated This function is deprecated and will be removed. Use the {@link MyAvatar} API instead.
     */
    void updateAvatarEntities(const QVariantList& avatarEntities);

    /*@jsdoc
     * Gets the details of all avatar bookmarks.
     * @function AvatarBookmarks.getBookmarks
     * @returns {Object<string,AvatarBookmarks.BookmarkData>} The current avatar bookmarks in an object where the keys are the 
     *     bookmark names and the values are the bookmark details.
     * @example <caption>List the names and URLs of all the avatar bookmarks.</caption>
     * var bookmarks = AvatarBookmarks.getBookmarks();
     * print("Avatar bookmarks:");
     * for (var key in bookmarks) {
     *     print("- " + key + " " + bookmarks[key].avatarUrl);
     * };
     */
    QVariantMap getBookmarks() { return _bookmarks; }

signals:
    /*@jsdoc
     * Triggered when an avatar bookmark is loaded, setting your avatar model, scale, and avatar entities (or attachments if an 
     * old bookmark) to those in the bookmark.
     * @function AvatarBookmarks.bookmarkLoaded
     * @param {string} bookmarkName - The name of the avatar bookmark loaded.
     * @returns {Signal}
     */
    void bookmarkLoaded(const QString& bookmarkName);

    /*@jsdoc
     * Triggered when an avatar bookmark is deleted.
     * @function AvatarBookmarks.bookmarkDeleted
     * @param {string} bookmarkName - The name of the avatar bookmark deleted.
     * @returns {Signal}
     * @example <caption>Report when a bookmark is deleted.</caption>
     * AvatarBookmarks.bookmarkDeleted.connect(function (bookmarkName) {
     *     print("Bookmark deleted: " + bookmarkName);
     * });
     */
    void bookmarkDeleted(const QString& bookmarkName);

    /*@jsdoc
     * Triggered when a new avatar bookmark is added or an existing avatar bookmark is updated, using 
     * {@link AvatarBookmarks.addBookmark|addBookmark}.
     * @function AvatarBookmarks.bookmarkAdded
     * @param {string} bookmarkName - The name of the avatar bookmark added or updated.
     * @returns {Signal}
     */
    void bookmarkAdded(const QString& bookmarkName);

protected:
    void addBookmarkToMenu(Menu* menubar, const QString& name, const QVariant& bookmark) override {};
    void readFromFile() override;
    QVariantMap getAvatarDataToBookmark(const QString avatarIcon);

protected slots: 
    /*@jsdoc
     * Performs no action.
     * @function AvatarBookmarks.deleteBookmark
     * @deprecated This function is deprecated and will be removed.
     */
    void deleteBookmark() override;

private:
    const QString AVATARBOOKMARKS_FILENAME = "avatarbookmarks.json";
    const QString ENTRY_AVATAR_URL = "avatarUrl";
    const QString ENTRY_AVATAR_ICON = "avatarIcon";
    const QString ENTRY_AVATAR_ATTACHMENTS = "attachments";
    const QString ENTRY_AVATAR_ENTITIES = "avatarEntites";
    const QString ENTRY_AVATAR_SCALE = "avatarScale";
    const QString ENTRY_VERSION = "version";

    const int AVATAR_BOOKMARK_VERSION = 4;
};

#endif // hifi_AvatarBookmarks_h
