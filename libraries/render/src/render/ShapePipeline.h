//
//  ShapePipeline.h
//  render/src/render
//
//  Created by Zach Pomerantz on 12/31/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_ShapePipeline_h
#define hifi_render_ShapePipeline_h

#include <unordered_set>

#include <gpu/Batch.h>
#include <graphics/Material.h>

#include "Args.h"

namespace render {
class Item;
class ShapePlumber;

class ShapeKey {
public:
    enum FlagBit {
        MATERIAL = 0,
        TRANSLUCENT,
        LIGHTMAP,
        TANGENTS,
        UNLIT,
        DEFORMED,
        DUAL_QUAT_SKINNED,
        DEPTH_BIAS,
        WIREFRAME,
        FADE,
        CULL_FACE_NONE, // if neither of these are set, we're CULL_FACE_BACK
        CULL_FACE_FRONT,

        OWN_PIPELINE,
        INVALID,

        CUSTOM_0,
        CUSTOM_1,
        CUSTOM_2,
        CUSTOM_3,
        CUSTOM_4,
        CUSTOM_5,
        CUSTOM_6,
        CUSTOM_7,

        NUM_FLAGS, // Not a valid flag

        CUSTOM_MASK = (0xFF << CUSTOM_0),

    };
    using Flags = std::bitset<NUM_FLAGS>;

    Flags _flags;

    ShapeKey() : _flags{ 0 } {}
    ShapeKey(const Flags& flags) : _flags{flags} {}

    friend ShapeKey operator&(const ShapeKey& _Left, const ShapeKey& _Right) { return ShapeKey(_Left._flags & _Right._flags); }
    friend ShapeKey operator|(const ShapeKey& _Left, const ShapeKey& _Right) { return ShapeKey(_Left._flags | _Right._flags); }
    friend ShapeKey operator^(const ShapeKey& _Left, const ShapeKey& _Right) { return ShapeKey(_Left._flags ^ _Right._flags); }

    class Builder {
    public:
        Builder() {}
        Builder(ShapeKey key) : _flags{key._flags} {}

        ShapeKey build() const { return ShapeKey{_flags}; }

        Builder& withMaterial() { _flags.set(MATERIAL); return (*this); }
        Builder& withTranslucent() { _flags.set(TRANSLUCENT); return (*this); }
        Builder& withLightMap() { _flags.set(LIGHTMAP); return (*this); }
        Builder& withTangents() { _flags.set(TANGENTS); return (*this); }
        Builder& withUnlit() { _flags.set(UNLIT); return (*this); }
        Builder& withDeformed() { _flags.set(DEFORMED); return (*this); }
        Builder& withDualQuatSkinned() { _flags.set(DUAL_QUAT_SKINNED); return (*this); }
        Builder& withDepthBias() { _flags.set(DEPTH_BIAS); return (*this); }
        Builder& withWireframe() { _flags.set(WIREFRAME); return (*this); }
        Builder& withFade() { _flags.set(FADE); return (*this); }

        Builder& withoutCullFace() { return withCullFaceMode(graphics::MaterialKey::CullFaceMode::CULL_NONE); }
        Builder& withCullFaceMode(graphics::MaterialKey::CullFaceMode cullFaceMode) {
            switch (cullFaceMode) {
                case graphics::MaterialKey::CullFaceMode::CULL_NONE:
                    _flags.set(CULL_FACE_NONE);
                    _flags.reset(CULL_FACE_FRONT);
                    break;
                case graphics::MaterialKey::CullFaceMode::CULL_FRONT:
                    _flags.reset(CULL_FACE_NONE);
                    _flags.set(CULL_FACE_FRONT);
                    break;
                case graphics::MaterialKey::CullFaceMode::CULL_BACK:
                    _flags.reset(CULL_FACE_NONE);
                    _flags.reset(CULL_FACE_FRONT);
                    break;
                default:
                    break;
            }
            return (*this);
        }

        Builder& withOwnPipeline() { _flags.set(OWN_PIPELINE); return (*this); }
        Builder& invalidate() { _flags.set(INVALID); return (*this); }

        Builder& withCustom(uint8_t custom) {  _flags &= (~CUSTOM_MASK); _flags |= (custom << CUSTOM_0); return (*this); }
        
        static const ShapeKey ownPipeline() { return Builder().withOwnPipeline(); }
        static const ShapeKey invalid() { return Builder().invalidate(); }

    protected:
        friend class ShapeKey;
        Flags _flags{0};
    };
    ShapeKey(const Builder& builder) : ShapeKey{builder._flags} {}

    class Filter {
    public:
        Filter(Flags flags, Flags mask) : _flags{flags}, _mask{mask} {}
        Filter(const ShapeKey& key) : _flags{ key._flags } { _mask.set(); }

        // Build a standard filter (will always exclude OWN_PIPELINE, INVALID)
        class Builder {
        public:
            Builder();

            Filter build() const { return Filter(_flags, _mask); }

            Builder& withMaterial() { _flags.set(MATERIAL); _mask.set(MATERIAL); return (*this); }
            Builder& withoutMaterial() { _flags.reset(MATERIAL); _mask.set(MATERIAL); return (*this); }

            Builder& withTranslucent() { _flags.set(TRANSLUCENT); _mask.set(TRANSLUCENT); return (*this); }
            Builder& withOpaque() { _flags.reset(TRANSLUCENT); _mask.set(TRANSLUCENT); return (*this); }

            Builder& withLightMap() { _flags.set(LIGHTMAP); _mask.set(LIGHTMAP); return (*this); }
            Builder& withoutLightMap() { _flags.reset(LIGHTMAP); _mask.set(LIGHTMAP); return (*this); }

            Builder& withTangents() { _flags.set(TANGENTS); _mask.set(TANGENTS); return (*this); }
            Builder& withoutTangents() { _flags.reset(TANGENTS); _mask.set(TANGENTS); return (*this); }

            Builder& withUnlit() { _flags.set(UNLIT); _mask.set(UNLIT); return (*this); }
            Builder& withoutUnlit() { _flags.reset(UNLIT); _mask.set(UNLIT); return (*this); }

            Builder& withDeformed() { _flags.set(DEFORMED); _mask.set(DEFORMED); return (*this); }
            Builder& withoutDeformed() { _flags.reset(DEFORMED); _mask.set(DEFORMED); return (*this); }

            Builder& withDualQuatSkinned() { _flags.set(DUAL_QUAT_SKINNED); _mask.set(DUAL_QUAT_SKINNED); return (*this); }
            Builder& withoutDualQuatSkinned() { _flags.reset(DUAL_QUAT_SKINNED); _mask.set(DUAL_QUAT_SKINNED); return (*this); }

            Builder& withDepthBias() { _flags.set(DEPTH_BIAS); _mask.set(DEPTH_BIAS); return (*this); }
            Builder& withoutDepthBias() { _flags.reset(DEPTH_BIAS); _mask.set(DEPTH_BIAS); return (*this); }

            Builder& withWireframe() { _flags.set(WIREFRAME); _mask.set(WIREFRAME); return (*this); }
            Builder& withoutWireframe() { _flags.reset(WIREFRAME); _mask.set(WIREFRAME); return (*this); }

            Builder& withCullFaceMode(graphics::MaterialKey::CullFaceMode cullFaceMode) {
                switch (cullFaceMode) {
                    case graphics::MaterialKey::CullFaceMode::CULL_NONE:
                        _flags.set(CULL_FACE_NONE);
                        _flags.reset(CULL_FACE_FRONT);
                        break;
                    case graphics::MaterialKey::CullFaceMode::CULL_FRONT:
                        _flags.reset(CULL_FACE_NONE);
                        _flags.set(CULL_FACE_FRONT);
                        break;
                    case graphics::MaterialKey::CullFaceMode::CULL_BACK:
                        _flags.reset(CULL_FACE_NONE);
                        _flags.reset(CULL_FACE_FRONT);
                        break;
                    default:
                        break;
                }
                _mask.set(CULL_FACE_NONE);
                _mask.set(CULL_FACE_FRONT);
                return (*this);
            }

            Builder& withFade() { _flags.set(FADE); _mask.set(FADE); return (*this); }
            Builder& withoutFade() { _flags.reset(FADE); _mask.set(FADE); return (*this); }

            Builder& withCustom(uint8_t custom) { _flags &= (~CUSTOM_MASK); _flags |= (custom << CUSTOM_0); _mask |= (CUSTOM_MASK); return (*this); }
            Builder& withoutCustom() { _flags &= (~CUSTOM_MASK);  _mask |= (CUSTOM_MASK); return (*this); }

        protected:
            friend class Filter;
            Flags _flags{0};
            Flags _mask{0};
        };
        Filter(const Filter::Builder& builder) : Filter(builder._flags, builder._mask) {}
        ShapeKey key() const { return ShapeKey(_flags); }
    protected:
        friend class ShapePlumber;
        Flags _flags{0};
        Flags _mask{0};
    };

    bool useMaterial() const { return _flags[MATERIAL]; }
    bool hasLightMap() const { return _flags[LIGHTMAP]; }
    bool hasTangents() const { return _flags[TANGENTS]; }
    bool isUnlit() const { return _flags[UNLIT]; }
    bool isTranslucent() const { return _flags[TRANSLUCENT]; }
    bool isDeformed() const { return _flags[DEFORMED]; }
    bool isDualQuatSkinned() const { return _flags[DUAL_QUAT_SKINNED]; }
    bool isDepthBiased() const { return _flags[DEPTH_BIAS]; }
    bool isWireframe() const { return _flags[WIREFRAME]; }
    bool isCullFace() const { return !_flags[CULL_FACE_NONE] && !_flags[CULL_FACE_FRONT]; }
    bool isFaded() const { return _flags[FADE]; }

    bool hasOwnPipeline() const { return _flags[OWN_PIPELINE]; }
    bool isValid() const { return !_flags[INVALID]; }

    uint8_t getCustom() const { return (_flags.to_ulong() & CUSTOM_MASK) >> CUSTOM_0; }
    bool isCustom() const { return (_flags.to_ulong() & CUSTOM_MASK); }

    // Comparator for use in stl containers
    class Hash {
    public:
        size_t operator() (const ShapeKey& key) const {
            return std::hash<ShapeKey::Flags>()(key._flags);
        }
    };

    // Comparator for use in stl containers
    class KeyEqual {
    public:
        bool operator()(const ShapeKey& lhs, const ShapeKey& rhs) const { return lhs._flags == rhs._flags; }
    };
};

inline QDebug operator<<(QDebug debug, const ShapeKey& key) {
    if (key.isValid()) {
        if (key.hasOwnPipeline()) {
            debug << "[ShapeKey: OWN_PIPELINE]";
        } else {
            debug << "[ShapeKey:"
                << "useMaterial:" << key.useMaterial()
                << "hasLightmap:" << key.hasLightMap()
                << "hasTangents:" << key.hasTangents()
                << "isUnlit:" << key.isUnlit()
                << "isTranslucent:" << key.isTranslucent()
                << "isDeformed:" << key.isDeformed()
                << "isDualQuatSkinned:" << key.isDualQuatSkinned()
                << "isDepthBiased:" << key.isDepthBiased()
                << "isWireframe:" << key.isWireframe()
                << "isCullFace:" << key.isCullFace()
                << "isFaded:" << key.isFaded()
                << "]";
        }
    } else {
        debug << "[ShapeKey: INVALID]";
    }
    return debug;
}

// Rendering abstraction over gpu::Pipeline and map locations
// Meta-information (pipeline and locations) to render a shape
class ShapePipeline {
public:
    class Locations {
    public:
        bool albedoTextureUnit{ false };
        bool normalTextureUnit{ false };
        bool roughnessTextureUnit{ false };
        bool metallicTextureUnit{ false };
        bool emissiveTextureUnit{ false };
        bool occlusionTextureUnit{ false };
        bool lightingModelBufferUnit{ false };
        bool skinClusterBufferUnit{ false };
        bool materialBufferUnit{ false };
        bool keyLightBufferUnit{ false };
        bool lightBufferUnit{ false };
        bool lightAmbientBufferUnit{ false };
        bool lightAmbientMapUnit{ false };
        bool deferredFrameTransformBufferUnit{ false };
        bool fadeMaskTextureUnit{ false };
        bool fadeParameterBufferUnit{ false };
        bool fadeObjectParameterBufferUnit{ false };
        bool hazeParameterBufferUnit{ false };
        bool lightClusterGridBufferUnit{ false };
        bool lightClusterContentBufferUnit{ false };
        bool lightClusterFrustumBufferUnit{ false };
    };
    using LocationsPointer = std::shared_ptr<Locations>;

    using BatchSetter = std::function<void(const ShapePipeline&, gpu::Batch&, render::Args*)>;

    using ItemSetter = std::function<void(const ShapePipeline&, render::Args*, const render::Item&)>;

    ShapePipeline(const gpu::PipelinePointer& pipeline, const LocationsPointer& locations, const BatchSetter& batchSetter = nullptr, const ItemSetter& itemSetter = nullptr) :
        pipeline(pipeline),
        locations(locations),
        _batchSetter(batchSetter),
        _itemSetter(itemSetter) {}

    // Normally, a pipeline is accessed through pickPipeline. If it needs to be set manually,
    // after calling setPipeline this method should be called to prepare the pipeline with default buffers.
    void prepare(gpu::Batch& batch, Args* args);

    gpu::PipelinePointer pipeline;
    std::shared_ptr<Locations> locations;

    void prepareShapeItem(Args* args, const ShapeKey& key, const Item& shape);

protected:
    friend class ShapePlumber;

    BatchSetter _batchSetter;
    ItemSetter _itemSetter;
public:
    using CustomKey = uint8_t;
    using CustomFactory = std::function<std::shared_ptr<ShapePipeline> (const ShapePlumber& plumber, const ShapeKey& key, RenderArgs* args)>;
    using CustomFactoryMap = std::map<CustomKey, CustomFactory>;

    static CustomFactoryMap _globalCustomFactoryMap;

    static CustomKey registerCustomShapePipelineFactory(CustomFactory factory);

};
using ShapePipelinePointer = std::shared_ptr<ShapePipeline>;

class ShapePlumber {
public:
    using Key = ShapeKey;
    using Filter = Key::Filter;
    using Pipeline = ShapePipeline;
    using PipelinePointer = ShapePipelinePointer;
    using PipelineMap = std::unordered_map<ShapeKey, PipelinePointer, ShapeKey::Hash, ShapeKey::KeyEqual>;
    using Slot = int32_t;
    using Locations = Pipeline::Locations;
    using LocationsPointer = Pipeline::LocationsPointer;
    using BatchSetter = Pipeline::BatchSetter;
    using ItemSetter = Pipeline::ItemSetter;

    void addPipeline(const Key& key, const gpu::ShaderPointer& program, const gpu::StatePointer& state,
        BatchSetter batchSetter = nullptr, ItemSetter itemSetter = nullptr);
    void addPipeline(const Filter& filter, const gpu::ShaderPointer& program, const gpu::StatePointer& state,
        BatchSetter batchSetter = nullptr, ItemSetter itemSetter = nullptr);

    const PipelinePointer pickPipeline(RenderArgs* args, const Key& key) const;

protected:
    void addPipelineHelper(const Filter& filter, Key key, int bit, const PipelinePointer& pipeline) const;
    mutable PipelineMap _pipelineMap;

private:
    mutable std::unordered_set<Key, Key::Hash, Key::KeyEqual> _missingKeys;
};


using ShapePlumberPointer = std::shared_ptr<ShapePlumber>;

}

#endif // hifi_render_ShapePipeline_h
