//
//  ProceduralSkybox.h
//  libraries/procedural/src/procedural
//
//  Created by Sam Gateau on 9/21/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_ProceduralSkybox_h
#define hifi_ProceduralSkybox_h

#include <graphics/Skybox.h>

#include "Procedural.h"

class ProceduralSkybox: public graphics::Skybox {
public:
    ProceduralSkybox(uint64_t created = 0);
    
    void parse(const QString& userData) { _procedural.setProceduralData(ProceduralData::parse(userData)); }

    bool empty() override;
    void clear() override;

    void render(gpu::Batch& batch, const ViewFrustum& frustum, bool forward, uint transformSlot) const override;
    static void render(gpu::Batch& batch, const ViewFrustum& frustum, const ProceduralSkybox& skybox, bool forward, uint transformSlot);

    uint64_t getCreated() const { return _created; }

protected:
    mutable Procedural _procedural;
    uint64_t _created;
};
typedef std::shared_ptr< ProceduralSkybox > ProceduralSkyboxPointer;

#endif
