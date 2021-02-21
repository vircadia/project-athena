//
//  Created by Bradley Austin Davis on 2016/05/15
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GLPipeline.h"

#include "GLShader.h"
#include "GLState.h"
#include "GLBackend.h"

using namespace gpu;
using namespace gpu::gl;

GLPipeline* GLPipeline::sync(GLBackend& backend, const Pipeline& pipeline) {
    GLPipeline* object = Backend::getGPUObject<GLPipeline>(pipeline);

    // If GPU object already created then good
    if (object) {
        return object;
    }

    // No object allocated yet, let's see if it's worth it...
    const auto& shader = pipeline.getProgram();

    // If this pipeline's shader has already failed to compile, don't try again
    if (shader->compilationHasFailed()) {
        return nullptr;
    }

    GLShader* programObject = GLShader::sync(backend, *shader);
    if (programObject == nullptr) {
        shader->setCompilationHasFailed(true);
        return nullptr;
    }

    const auto& state = pipeline.getState();
    GLState* stateObject = GLState::sync(*state);
    if (stateObject == nullptr) {
        return nullptr;
    }

    // Program and state are valid, we can create the pipeline object
    if (!object) {
        object = new GLPipeline();
        Backend::setGPUObject(pipeline, object);
    }

    object->_program = programObject;
    object->_state = stateObject;

    return object;
}
