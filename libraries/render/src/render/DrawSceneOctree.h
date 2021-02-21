//
//  DrawSceneOctree.h
//  render/src/render
//
//  Created by Sam Gateau on 1/25/16.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_DrawSceneOctree_h
#define hifi_render_DrawSceneOctree_h

#include <ViewFrustum.h>
#include <gpu/Batch.h>

#include "DrawTask.h"

namespace render {
    class DrawSceneOctreeConfig : public Job::Config {
        Q_OBJECT
        Q_PROPERTY(bool showVisibleCells READ getShowVisibleCells WRITE setShowVisibleCells NOTIFY dirty())
        Q_PROPERTY(bool showEmptyCells READ getShowEmptyCells WRITE setShowEmptyCells NOTIFY dirty())
        Q_PROPERTY(int numAllocatedCells READ getNumAllocatedCells)
        Q_PROPERTY(int numFreeCells READ getNumFreeCells)

    public:

        DrawSceneOctreeConfig() : Job::Config(false) {}

        int numAllocatedCells{ 0 };
        int numFreeCells{ 0 };

        int getNumAllocatedCells() const { return numAllocatedCells; }
        int getNumFreeCells() const { return numFreeCells; }

        bool showVisibleCells{ true };
        bool showEmptyCells{ false };

        bool getShowVisibleCells() { return showVisibleCells; }
        bool getShowEmptyCells() { return showEmptyCells; }

    public slots:
        void setShowVisibleCells(bool show) { showVisibleCells = show; emit dirty(); }
        void setShowEmptyCells(bool show) { showEmptyCells = show; emit dirty(); }

    signals:
        void dirty();
    };

    class DrawSceneOctree {
        gpu::PipelinePointer _drawCellBoundsPipeline;
        gpu::PipelinePointer _drawLODReticlePipeline;
        gpu::PipelinePointer _drawItemBoundPipeline;
        gpu::BufferPointer _cellBoundsBuffer;
        gpu::Stream::FormatPointer _cellBoundsFormat;

        bool _showVisibleCells; // initialized by Config
        bool _showEmptyCells; // initialized by Config

    public:
        using Config = DrawSceneOctreeConfig;
        using JobModel = Job::ModelI<DrawSceneOctree, ItemSpatialTree::ItemSelection, Config>;

        DrawSceneOctree(uint transformSlot) : _transformSlot(transformSlot) {}

        void configure(const Config& config);
        void run(const RenderContextPointer& renderContext, const ItemSpatialTree::ItemSelection& selection);

        const gpu::PipelinePointer getDrawCellBoundsPipeline();
        const gpu::PipelinePointer getDrawLODReticlePipeline();
        const gpu::PipelinePointer getDrawItemBoundPipeline();
    
    private:
        uint _transformSlot;
    };


    class DrawItemSelectionConfig : public Job::Config {
        Q_OBJECT
        Q_PROPERTY(bool showInsideItems READ getShowInsideItems WRITE setShowInsideItems NOTIFY dirty())
        Q_PROPERTY(bool showInsideSubcellItems READ getShowInsideSubcellItems WRITE setShowInsideSubcellItems NOTIFY dirty())
        Q_PROPERTY(bool showPartialItems READ getShowPartialItems WRITE setShowPartialItems NOTIFY dirty())
        Q_PROPERTY(bool showPartialSubcellItems READ getShowPartialSubcellItems WRITE setShowPartialSubcellItems NOTIFY dirty())
    public:

        DrawItemSelectionConfig() : Job::Config(false) {}

        bool showInsideItems{ true };
        bool showInsideSubcellItems{ true };
        bool showPartialItems{ true };
        bool showPartialSubcellItems{ true };

        bool getShowInsideItems() const { return showInsideItems; };
        bool getShowInsideSubcellItems() const { return showInsideSubcellItems; };
        bool getShowPartialItems() const { return showPartialItems; };
        bool getShowPartialSubcellItems() const { return showPartialSubcellItems; };

    public slots:
        void setShowInsideItems(bool show) { showInsideItems = show; emit dirty(); }
        void setShowInsideSubcellItems(bool show) { showInsideSubcellItems = show; emit dirty(); }
        void setShowPartialItems(bool show) { showPartialItems = show; emit dirty(); }
        void setShowPartialSubcellItems(bool show) { showPartialSubcellItems = show; emit dirty(); }

    signals:
        void dirty();
    };

    class DrawItemSelection {
        gpu::PipelinePointer _drawItemBoundPipeline;
        gpu::BufferPointer _boundsBufferInside;
        gpu::BufferPointer _boundsBufferInsideSubcell;
        gpu::BufferPointer _boundsBufferPartial;
        gpu::BufferPointer _boundsBufferPartialSubcell;

        bool _showInsideItems; // initialized by Config
        bool _showInsideSubcellItems; // initialized by Config
        bool _showPartialItems; // initialized by Config
        bool _showPartialSubcellItems; // initialized by Config

    public:
        using Config = DrawItemSelectionConfig;
        using JobModel = Job::ModelI<DrawItemSelection, ItemSpatialTree::ItemSelection, Config>;

        DrawItemSelection(uint transformSlot) : _transformSlot(transformSlot) {}

        void configure(const Config& config);
        void run(const RenderContextPointer& renderContext, const ItemSpatialTree::ItemSelection& selection);

        const gpu::PipelinePointer getDrawItemBoundPipeline();

    private:
        uint _transformSlot;
    };
}

#endif // hifi_render_DrawStatus_h
