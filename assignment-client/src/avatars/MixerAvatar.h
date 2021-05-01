//
//  MixerAvatar.h
//  assignment-client/src/avatars
//
//  Created by Simon Walton Feb 2019.
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// Avatar class for use within the avatar mixer - includes avatar-verification state.

#ifndef hifi_MixerAvatar_h
#define hifi_MixerAvatar_h

#include <AvatarData.h>

class ResourceRequest;

class MixerAvatar : public AvatarData {
    Q_OBJECT
public:
    inline MixerAvatar() {}

    bool getNeedsHeroCheck() const { return _needsHeroCheck; }
    void setNeedsHeroCheck(bool needsHeroCheck = true) { _needsHeroCheck = needsHeroCheck; }

    bool needsIdentityUpdate() const { return _needsIdentityUpdate; }
    void setNeedsIdentityUpdate(bool value = true) { _needsIdentityUpdate = value; }

    bool isInScreenshareZone() const { return _inScreenshareZone; }
    void setInScreenshareZone(bool value = true) { _inScreenshareZone = value; }
    const QUuid& getScreenshareZone() const { return _screenshareZone; }
    void setScreenshareZone(QUuid zone) { _screenshareZone = zone; }

private:
    bool _needsHeroCheck { false };
    bool _needsIdentityUpdate { false };
    bool _inScreenshareZone { false };
    QUuid _screenshareZone;
};

using MixerAvatarSharedPointer = std::shared_ptr<MixerAvatar>;

#endif  // hifi_MixerAvatar_h
