//
//  SimpleMeshProxy.h
//  libraries/model-networking/src/model-networking/
//
//  Created by Seth Alves on 2017-1-27.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SimpleMeshProxy_h
#define hifi_SimpleMeshProxy_h

#include <RegisteredMetaTypes.h>

class SimpleMeshProxy : public MeshProxy {
public:
    SimpleMeshProxy(const MeshPointer& mesh) : _mesh(mesh) { }

    MeshPointer getMeshPointer() const override;

    int getNumVertices() const override;

    glm::vec3 getPos(int index) const override;
    glm::vec3 getPos3(int index) const override { return getPos(index); } // deprecated

protected:
    const MeshPointer _mesh;
};

#endif // hifi_SimpleMeshProxy_h
