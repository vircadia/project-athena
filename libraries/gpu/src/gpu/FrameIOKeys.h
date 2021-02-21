//
//  Created by Bradley Austin Davis on 2018/10/14
//  Copyright 2013-2018 High Fidelity, Inc.
//  Copyright 2020 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_gpu_FrameIOKeys_h
#define hifi_gpu_FrameIOKeys_h

namespace gpu { namespace keys {


constexpr const char*  binary = "binary";
constexpr const char*  L00 = "L00";
constexpr const char*  L1m1 = "L1m1";
constexpr const char*  L10 = "L10";
constexpr const char*  L11 = "L11";
constexpr const char*  L2m2 = "L2m2";
constexpr const char*  L2m1 = "L2m1";
constexpr const char*  L20 = "L20";
constexpr const char*  L21 = "L21";
constexpr const char*  L22 = "L22";

constexpr const char*  eyeProjections = "eyeProjections";
constexpr const char*  eyeViews = "eyeViews";
constexpr const char*  alphaToCoverageEnable = "alphaToCoverageEnable";
constexpr const char*  antialisedLineEnable = "antialisedLineEnable";
constexpr const char*  attributes = "attributes";
constexpr const char*  batches = "batches";
constexpr const char*  blendFunction = "blendFunction";
constexpr const char*  borderColor = "borderColor";
constexpr const char*  bufferMask = "bufferMask";
constexpr const char*  buffers = "buffers";
constexpr const char*  capturedTextures = "capturedTextures";
constexpr const char*  channel = "channel";
constexpr const char*  chunk = "chunk";
constexpr const char*  colorAttachments = "colorAttachments";
constexpr const char*  colorWriteMask = "colorWriteMask";
constexpr const char*  commands = "commands";
constexpr const char*  comparisonFunction = "comparisonFunction";
constexpr const char*  cullMode = "cullMode";
constexpr const char*  data = "data";
constexpr const char*  depth = "depth";
constexpr const char*  depthBias = "depthBias";
constexpr const char*  depthBiasSlopeScale = "depthBiasSlopeScale";
constexpr const char*  depthClampEnable = "depthClampEnable";
constexpr const char*  depthStencilAttachment = "depthStencilAttachment";
constexpr const char*  depthTest = "depthTest";
constexpr const char*  drawCallInfos = "drawCallInfos";
constexpr const char*  drawcallUniform = "drawcallUniform";
constexpr const char*  drawcallUniformReset = "drawcallUniformReset";
constexpr const char*  element = "element";
constexpr const char*  fillMode = "fillMode";
constexpr const char*  filter = "filter";
constexpr const char*  formats = "formats";
constexpr const char*  frameIndex = "frameIndex";
constexpr const char*  framebuffer = "framebuffer";
constexpr const char*  framebuffers = "framebuffers";
constexpr const char*  frequency = "frequency";
constexpr const char*  frontFaceClockwise = "frontFaceClockwise";
constexpr const char*  height = "height";
constexpr const char*  id = "id";
constexpr const char*  ktxFile = "ktxFile";
constexpr const char*  layers = "layers";
constexpr const char*  maxAnisotropy = "maxAnisotropy";
constexpr const char*  maxMip = "maxMip";
constexpr const char*  minMip = "minMip";
constexpr const char*  mipOffset = "mipOffset";
constexpr const char*  mips = "mips";
constexpr const char*  multisampleEnable = "multisampleEnable";
constexpr const char*  name = "name";
constexpr const char*  namedData = "namedData";
constexpr const char*  names = "names";
constexpr const char*  objects = "objects";
constexpr const char*  offset = "offset";
constexpr const char*  pipelines = "pipelines";
constexpr const char*  pose = "pose";
constexpr const char*  profileRanges = "profileRanges";
constexpr const char*  program = "program";
constexpr const char*  programs = "programs";
constexpr const char*  isJitterOnProjectionEnabled = "isJitterOnProjectionEnabled";
constexpr const char*  queries = "queries";
constexpr const char*  sampleCount = "sampleCount";
constexpr const char*  sampleMask = "sampleMask";
constexpr const char*  sampler = "sampler";
constexpr const char*  samples = "samples";
constexpr const char*  scissorEnable = "scissorEnable";
constexpr const char*  shaders = "shaders";
constexpr const char*  size = "size";
constexpr const char*  skybox = "skybox";
constexpr const char*  slot = "slot";
constexpr const char*  source = "source";
constexpr const char*  state = "state";
constexpr const char*  stencilActivation = "stencilActivation";
constexpr const char*  stencilTestBack = "stencilTestBack";
constexpr const char*  stencilTestFront = "stencilTestFront";
constexpr const char*  stereo = "stereo";
constexpr const char*  subresource = "subresource";
constexpr const char*  swapchains = "swapchains";
constexpr const char*  texelFormat = "texelFormat";
constexpr const char*  texture = "texture";
constexpr const char*  textureTables = "textureTables";
constexpr const char*  textures = "textures";
constexpr const char*  transforms = "transforms";
constexpr const char*  type = "type";
constexpr const char*  usageType = "usageType";
constexpr const char*  view = "view";
constexpr const char*  width = "width";
constexpr const char*  wrapModeU = "wrapModeU";
constexpr const char*  wrapModeV = "wrapModeV";
constexpr const char*  wrapModeW = "wrapModeW";


constexpr const char*  backWriteMask = "backWriteMask";
constexpr const char*  frontWriteMask = "frontWriteMask";
constexpr const char*  reference = "reference";
constexpr const char*  readMask = "readMask";
constexpr const char*  failOp = "failOp";
constexpr const char*  depthFailOp = "depthFailOp";
constexpr const char*  passOp = "passOp";
constexpr const char*  enabled = "enabled";
constexpr const char*  blend = "blend";
constexpr const char*  flags = "flags";
constexpr const char*  writeMask = "writeMask";
constexpr const char*  function = "function";
constexpr const char*  sourceColor = "sourceColor";
constexpr const char*  sourceAlpha = "sourceAlpha";
constexpr const char*  destColor = "destColor";
constexpr const char*  destAlpha = "destAlpha";
constexpr const char*  opColor = "opColor";
constexpr const char*  opAlpha = "opAlpha";
constexpr const char*  enable = "enable";
constexpr const char*  contextDisable = "contextDisable";

constexpr const char*  COMMAND_NAMES[] = {
    "draw",
    "drawIndexed",
    "drawInstanced",
    "drawIndexedInstanced",
    "multiDrawIndirect",
    "multiDrawIndexedIndirect",

    "setInputFormat",
    "setInputBuffer",
    "setIndexBuffer",
    "setIndirectBuffer",

    "setModelTransform",
    "setViewTransform",
    "setProjectionTransform",
    "setProjectionJitterEnabled",
    "setProjectionJitterSequence",
    "setProjectionJitterScale",
    "setViewportTransform",
    "setDepthRangeTransform",

    "saveViewProjectionTransform",
    "setSavedViewProjectionTransform",
    "copySavedViewProjectionTransformToBuffer",

    "setPipeline",
    "setStateBlendFactor",
    "setStateScissorRect",

    "setUniformBuffer",
    "setResourceBuffer",
    "setResourceTexture",
    "setResourceTextureTable",
    "setResourceFramebufferSwapChainTexture",

    "setFramebuffer",
    "setFramebufferSwapChain",
    "clearFramebuffer",
    "blit",
    "generateTextureMips",
    "generateTextureMipsWithPipeline",

    "advance",

    "beginQuery",
    "endQuery",
    "getQuery",

    "resetStages",

    "disableContextViewCorrection",
    "restoreContextViewCorrection",

    "disableContextStereo",
    "restoreContextStereo",

    "runLambda",

    "startNamedCall",
    "stopNamedCall",

    "glUniform1i",
    "glUniform1f",
    "glUniform2f",
    "glUniform3f",
    "glUniform4f",
    "glUniform3fv",
    "glUniform4fv",
    "glUniform4iv",
    "glUniformMatrix3fv",
    "glUniformMatrix4fv",

    "glColor4f",

    "pushProfileRange",
    "popProfileRange",
};

template<class T, size_t N>
constexpr size_t array_size(T (&)[N]) { return N; }

static_assert(array_size(COMMAND_NAMES) == Batch::Command::NUM_COMMANDS, "Command array sizes must match");
}}  // namespace gpu::keys

#endif
