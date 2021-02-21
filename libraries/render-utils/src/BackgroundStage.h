//
//  BackgroundStage.h

//  Created by Sam Gateau on 5/9/2017.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_utils_BackgroundStage_h
#define hifi_render_utils_BackgroundStage_h

#include <graphics/Stage.h>
#include <set>
#include <unordered_map>
#include <render/IndexedContainer.h>
#include <render/Stage.h>
#include "HazeStage.h"

#include "LightingModel.h"


// Background stage to set up background-related rendering tasks
class BackgroundStage : public render::Stage {
public:
    static std::string _stageName;
    static const std::string& getName() { return _stageName; }

    using Index = render::indexed_container::Index;
    static const Index INVALID_INDEX;
    static bool isIndexInvalid(Index index) { return index == INVALID_INDEX; }
    
    using BackgroundPointer = graphics::SunSkyStagePointer;
    using Backgrounds = render::indexed_container::IndexedPointerVector<graphics::SunSkyStage>;
    using BackgroundMap = std::unordered_map<BackgroundPointer, Index>;

    using BackgroundIndices = std::vector<Index>;


    Index findBackground(const BackgroundPointer& background) const;
    Index addBackground(const BackgroundPointer& background);

    BackgroundPointer removeBackground(Index index);
    
    bool checkBackgroundId(Index index) const { return _backgrounds.checkIndex(index); }

    Index getNumBackgrounds() const { return _backgrounds.getNumElements(); }
    Index getNumFreeBackgrounds() const { return _backgrounds.getNumFreeIndices(); }
    Index getNumAllocatedBackgrounds() const { return _backgrounds.getNumAllocatedIndices(); }

    BackgroundPointer getBackground(Index backgroundId) const {
        return _backgrounds.get(backgroundId);
    }

    Backgrounds _backgrounds;
    BackgroundMap _backgroundMap;

    class Frame {
    public:
        Frame() {}
        
        void clear() { _backgrounds.clear(); }

        void pushBackground(BackgroundStage::Index index) { _backgrounds.emplace_back(index); }

        BackgroundStage::BackgroundIndices _backgrounds;
    };
    using FramePointer = std::shared_ptr<Frame>;
    
    Frame _currentFrame;
};
using BackgroundStagePointer = std::shared_ptr<BackgroundStage>;

class BackgroundStageSetup {
public:
    using JobModel = render::Job::Model<BackgroundStageSetup>;

    BackgroundStageSetup();
    void run(const render::RenderContextPointer& renderContext);
};

class DrawBackgroundStage {
public:
    using Inputs = render::VaryingSet3<LightingModelPointer, BackgroundStage::FramePointer, HazeStage::FramePointer>;
    using JobModel = render::Job::ModelI<DrawBackgroundStage, Inputs>;

    DrawBackgroundStage(uint transformSlot) : _transformSlot(transformSlot) {}

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

private:
    uint _transformSlot;
};

#endif
