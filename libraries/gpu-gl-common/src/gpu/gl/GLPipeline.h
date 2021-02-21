//
//  Created by Bradley Austin Davis on 2016/05/15
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_gl_GLPipeline_h
#define hifi_gpu_gl_GLPipeline_h

#include "GLShared.h"

namespace gpu { namespace gl {

class GLPipeline : public GPUObject {
public:
    static GLPipeline* sync(GLBackend& backend, const Pipeline& pipeline);

    GLShader* _program { nullptr };
    GLState* _state { nullptr };
};

} }


#endif
