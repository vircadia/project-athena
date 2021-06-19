import QtQuick 2.9

ListModel {
    id: model
    function extractMarketId(avatarUrl) {

        var guidRegexp = '([A-Z0-9]{8}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{12})';

        var regexp = new RegExp(guidRegexp,["i"]);
        var match = regexp.exec(avatarUrl);
        if (match !== null) {
            return match[1];
        }

        return '';
    }

    function makeMarketItemUrl(avatarUrl) {
        var marketItemUrl = "https://highfidelity.com/marketplace/items/%marketId%"
            .split('%marketId%').join(extractMarketId(avatarUrl));

        return marketItemUrl;
    }
	
    function makeMarketThumbnailUrl(marketId) {
        var avatarThumbnailUrl = "https://hifi-metaverse.s3-us-west-1.amazonaws.com/marketplace/previews/%marketId%/large/hifi-mp-%marketId%.jpg"
            .split('%marketId%').join(marketId);
            
        return avatarThumbnailUrl;
    }
	
    function trimFileExtension(url) {
        var trimmedUrl = url.substring(0, (url.indexOf("#") === -1) ? url.length : url.indexOf("#"));
        trimmedUrl = trimmedUrl.substring(0, (trimmedUrl.indexOf("?") === -1) ? trimmedUrl.length : trimmedUrl.indexOf("?"));
        trimmedUrl = trimmedUrl.substring(0, trimmedUrl.lastIndexOf("."));

        return trimmedUrl;
    }
	
    function makeThumbnailUrl(avatarUrl) {
        var marketId = extractMarketId(avatarUrl);
        if (marketId !== '') {
            return makeMarketThumbnailUrl(marketId);
        }
        
        var avatarThumbnailFileUrl = trimFileExtension(avatarUrl) + ".jpg";
        
        return avatarThumbnailFileUrl;
    }

    function makeAvatarObject(avatar, avatarName) {
        var avatarThumbnailUrl;
        
        console.log("icon..." + avatar.avatarUrl);
        console.log("icon..." + avatar.thumbnailUrl);
        if (!avatar.avatarIcon) {
            avatarThumbnailUrl = makeThumbnailUrl(avatar.avatarUrl);
        } else {
            avatarThumbnailUrl = avatar.avatarIcon;
        }

        return {
            'name' : avatarName,
            'avatarScale' : avatar.avatarScale,
            'thumbnailUrl' : avatarThumbnailUrl,
            'avatarUrl' : avatar.avatarUrl,
            'wearables' : avatar.avatarEntites ? avatar.avatarEntites : [],
            'entry' : avatar,
            'getMoreAvatars' : false
        };

    }

    function addAvatarEntry(avatar, avatarName) {
        var avatarEntry = makeAvatarObject(avatar, avatarName);
        append(avatarEntry);

        return allAvatars.count - 1;
    }

    function populate(bookmarks) {
        clear();
        for (var avatarName in bookmarks) {
            var avatar = bookmarks[avatarName];
            var avatarEntry = makeAvatarObject(avatar, avatarName);

            append(avatarEntry);
        }
    }

    function arraysAreEqual(a1, a2, comparer) {
        if (Array.isArray(a1) && Array.isArray(a2)) {
            if (a1.length !== a2.length) {
                return false;
            }

            for (var i = 0; i < a1.length; ++i) {
                if (!comparer(a1[i], a2[i])) {
                    return false;
                }
            }
        } else if (Array.isArray(a1)) {
            return a1.length === 0;
        } else if (Array.isArray(a2)) {
            return a2.length === 0;
        }

        return true;
    }

    function modelsAreEqual(m1, m2, comparer) {
        if (m1.count !== m2.count) {
            return false;
        }

        for (var i = 0; i < m1.count; ++i) {
            var e1 = m1.get(i);

            var allDifferent = true;

            // it turns out order of wearables can randomly change so make position-independent comparison here
            for (var j = 0; j < m2.count; ++j) {
                var e2 = m2.get(j);

                if (comparer(e1, e2)) {
                    allDifferent = false;
                    break;
                }
            }

            if (allDifferent) {
                return false;
            }
        }

        return true;
    }

    function compareNumericObjects(o1, o2) {
        if (o1 === undefined && o2 !== undefined) {
            return false;
        }
        if (o1 !== undefined && o2 === undefined) {
            return false;
        }

        for (var prop in o1) {
            if (o1.hasOwnProperty(prop) && o2.hasOwnProperty(prop)) {
                var v1 = o1[prop];
                var v2 = o2[prop];


                if (v1 !== v2 && Math.round(v1 * 500) != Math.round(v2 * 500)) {
                    return false;
                }
            }
        }

        return true;
    }

    function compareObjects(o1, o2, props, arrayProp) {
        for (var i = 0; i < props.length; ++i) {
            var prop = props[i];
            var propertyName = prop.propertyName;
            var comparer = prop.comparer;

            var o1Value = arrayProp ? o1[arrayProp][propertyName] : o1[propertyName];
            var o2Value = arrayProp ? o2[arrayProp][propertyName] : o2[propertyName];

            if (comparer) {
                if (comparer(o1Value, o2Value) === false) {
                    return false;
                }
            } else {
                if (JSON.stringify(o1Value) !== JSON.stringify(o2Value)) {
                    return false;
                }
            }
        }

        return true;
    }

    function compareWearables(w1, w2) {
        return compareObjects(w1, w2, [{'propertyName' : 'modelURL'},
                                       {'propertyName' : 'parentJointIndex'},
                                       {'propertyName' : 'marketplaceID'},
                                       {'propertyName' : 'itemName'},
                                       {'propertyName' : 'script'},
                                       {'propertyName' : 'relayParentJoints'},
                                       {'propertyName' : 'localPosition', 'comparer' : compareNumericObjects},
                                       {'propertyName' : 'localRotationAngles', 'comparer' : compareNumericObjects},
                                       {'propertyName' : 'dimensions', 'comparer' : compareNumericObjects}], 'properties')
    }

    function findAvatarIndexByValue(avatar) {

        var index = -1;

        // 2DO: find better way of determining selected avatar in bookmarks
        for (var i = 0; i < allAvatars.count; ++i) {
            var thesame = true;
            var bookmarkedAvatar = allAvatars.get(i);

            if (bookmarkedAvatar.avatarUrl !== avatar.avatarUrl) {
                continue;
            }

            if (bookmarkedAvatar.avatarScale !== avatar.avatarScale) {
                continue;
            }

            if (!modelsAreEqual(bookmarkedAvatar.wearables, avatar.wearables, compareWearables)) {
                continue;
            }

            if (thesame) {
                index = i;
                break;
            }
        }

        return index;
    }

    function findAvatarIndex(avatarName) {
        for (var i = 0; i < count; ++i) {
            if (get(i).name === avatarName) {
                return i;
            }
        }
        return -1;
    }

    function findAvatar(avatarName) {
        var avatarIndex = findAvatarIndex(avatarName);
        if (avatarIndex === -1) {
            return undefined;
        }

        return get(avatarIndex);
    }

}
