//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Filter.h"

#include <QtCore/QObject>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <SharedUtil.h>

#include "filters/ClampFilter.h"
#include "filters/ConstrainToIntegerFilter.h"
#include "filters/ConstrainToPositiveIntegerFilter.h"
#include "filters/DeadZoneFilter.h"
#include "filters/HysteresisFilter.h"
#include "filters/InvertFilter.h"
#include "filters/NotFilter.h"
#include "filters/PulseFilter.h"
#include "filters/ScaleFilter.h"
#include "filters/TranslateFilter.h"
#include "filters/TransformFilter.h"
#include "filters/PostTransformFilter.h"
#include "filters/RotateFilter.h"
#include "filters/LowVelocityFilter.h"
#include "filters/ExponentialSmoothingFilter.h"
#include "filters/AccelerationLimiterFilter.h"

using namespace controller;

Filter::Factory Filter::_factory;

REGISTER_FILTER_CLASS_INSTANCE(ClampFilter, "clamp")
REGISTER_FILTER_CLASS_INSTANCE(ConstrainToIntegerFilter, "constrainToInteger")
REGISTER_FILTER_CLASS_INSTANCE(ConstrainToPositiveIntegerFilter, "constrainToPositiveInteger")
REGISTER_FILTER_CLASS_INSTANCE(DeadZoneFilter, "deadZone")
REGISTER_FILTER_CLASS_INSTANCE(HysteresisFilter, "hysteresis")
REGISTER_FILTER_CLASS_INSTANCE(InvertFilter, "invert")
REGISTER_FILTER_CLASS_INSTANCE(NotFilter, "logicalNot")
REGISTER_FILTER_CLASS_INSTANCE(ScaleFilter, "scale")
REGISTER_FILTER_CLASS_INSTANCE(PulseFilter, "pulse")
REGISTER_FILTER_CLASS_INSTANCE(TranslateFilter, "translate")
REGISTER_FILTER_CLASS_INSTANCE(TransformFilter, "transform")
REGISTER_FILTER_CLASS_INSTANCE(PostTransformFilter, "postTransform")
REGISTER_FILTER_CLASS_INSTANCE(RotateFilter, "rotate")
REGISTER_FILTER_CLASS_INSTANCE(LowVelocityFilter, "lowVelocity")
REGISTER_FILTER_CLASS_INSTANCE(ExponentialSmoothingFilter, "exponentialSmoothing")
REGISTER_FILTER_CLASS_INSTANCE(AccelerationLimiterFilter, "accelerationLimiter")

const QString JSON_FILTER_TYPE = QStringLiteral("type");
const QString JSON_FILTER_PARAMS = QStringLiteral("params");


Filter::Pointer Filter::parse(const QJsonValue& json) {
    Filter::Pointer filter;
    if (json.isString()) {
        filter = Filter::getFactory().create(json.toString());
    } else if (json.isObject()) {
        QJsonObject jsonObj = json.toObject();
        // The filter is an object, now let s check for type and potential arguments
        auto filterType = jsonObj[JSON_FILTER_TYPE];
        filter = Filter::getFactory().create(filterType.toString());
        if (filter) {
            QJsonValue params = jsonObj;
            if (jsonObj.contains(JSON_FILTER_PARAMS)) {
                params = jsonObj[JSON_FILTER_PARAMS];
            }
            if (!filter->parseParameters(params)) {
                qWarning() << "Unable to parse filter parameters " << params;
                return Filter::Pointer();
            }
        }
    }
    return filter;
}

bool Filter::parseSingleFloatParameter(const QJsonValue& parameters, const QString& name, float& output) {
    if (parameters.isDouble()) {
        output = parameters.toDouble();
        return true;
    } else if (parameters.isArray()) {
        auto arrayParameters = parameters.toArray();
        if (arrayParameters.size() > 1) {
            output = arrayParameters[0].toDouble();
            return true;
        }
    } else if (parameters.isObject()) {
        auto objectParameters = parameters.toObject();
        if (objectParameters.contains(name)) {
            output = objectParameters[name].toDouble();
            return true;
        }
    }
    return false;
}

bool Filter::parseVec3Parameter(const QJsonValue& parameters, glm::vec3& output) {
    if (parameters.isDouble()) {
        output = glm::vec3(parameters.toDouble());
        return true;
    } else if (parameters.isArray()) {
        auto arrayParameters = parameters.toArray();
        if (arrayParameters.size() == 3) {
            output = glm::vec3(arrayParameters[0].toDouble(),
                               arrayParameters[1].toDouble(),
                               arrayParameters[2].toDouble());
            return true;
        }
    } else if (parameters.isObject()) {
        auto objectParameters = parameters.toObject();
        if (objectParameters.contains("x") && objectParameters.contains("y") && objectParameters.contains("z")) {
            output = glm::vec3(objectParameters["x"].toDouble(),
                               objectParameters["y"].toDouble(),
                               objectParameters["z"].toDouble());
            return true;
        }
    }
    return false;
}

bool Filter::parseMat4Parameter(const QJsonValue& parameters, glm::mat4& output) {
    if (parameters.isObject()) {
        auto objectParameters = parameters.toObject();


        if (objectParameters.contains("r0c0") &&
            objectParameters.contains("r1c0") &&
            objectParameters.contains("r2c0") &&
            objectParameters.contains("r3c0") &&
            objectParameters.contains("r0c1") &&
            objectParameters.contains("r1c1") &&
            objectParameters.contains("r2c1") &&
            objectParameters.contains("r3c1") &&
            objectParameters.contains("r0c2") &&
            objectParameters.contains("r1c2") &&
            objectParameters.contains("r2c2") &&
            objectParameters.contains("r3c2") &&
            objectParameters.contains("r0c3") &&
            objectParameters.contains("r1c3") &&
            objectParameters.contains("r2c3") &&
            objectParameters.contains("r3c3")) {

            output[0][0] = objectParameters["r0c0"].toDouble();
            output[0][1] = objectParameters["r1c0"].toDouble();
            output[0][2] = objectParameters["r2c0"].toDouble();
            output[0][3] = objectParameters["r3c0"].toDouble();
            output[1][0] = objectParameters["r0c1"].toDouble();
            output[1][1] = objectParameters["r1c1"].toDouble();
            output[1][2] = objectParameters["r2c1"].toDouble();
            output[1][3] = objectParameters["r3c1"].toDouble();
            output[2][0] = objectParameters["r0c2"].toDouble();
            output[2][1] = objectParameters["r1c2"].toDouble();
            output[2][2] = objectParameters["r2c2"].toDouble();
            output[2][3] = objectParameters["r3c2"].toDouble();
            output[3][0] = objectParameters["r0c3"].toDouble();
            output[3][1] = objectParameters["r1c3"].toDouble();
            output[3][2] = objectParameters["r2c3"].toDouble();
            output[3][3] = objectParameters["r3c3"].toDouble();

            return true;
        }
    }
    return false;
}

bool Filter::parseQuatParameter(const QJsonValue& parameters, glm::quat& output) {
    if (parameters.isObject()) {
        auto objectParameters = parameters.toObject();
        if (objectParameters.contains("w") &&
            objectParameters.contains("x") &&
            objectParameters.contains("y") &&
            objectParameters.contains("z")) {

            output = glm::quat(objectParameters["w"].toDouble(),
                               objectParameters["x"].toDouble(),
                               objectParameters["y"].toDouble(),
                               objectParameters["z"].toDouble());
            return true;
        }
    }
    return false;
}


#if 0

namespace controller {

    class LambdaFilter : public Filter {
    public:
        //    LambdaFilter() {}12
        LambdaFilter(Lambda f) : _function(f) {};

        virtual float apply(float value) const {
            return _function(value);
        }

        virtual bool parseParameters(const QJsonArray& parameters) { return true; }

        //        REGISTER_FILTER_CLASS(LambdaFilter);
    private:
        Lambda _function;
    };

    class ScriptFilter : public Filter {
    public:

    };



    //class EasingFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override;

    //private:
    //    QEasingCurve _curve;
    //};

    //// GLSL style filters
    //class StepFilter : public Filter {
    //public:
    //    StepFilter(float edge) : _edge(edge) {};
    //    virtual float apply(float value) const override;

    //private:
    //    const float _edge;
    //};

    //class PowFilter : public Filter {
    //public:
    //    PowFilter(float exponent) : _exponent(exponent) {};
    //    virtual float apply(float value) const override;

    //private:
    //    const float _exponent;
    //};

    //class AbsFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override;
    //};

    //class SignFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override;
    //};

    //class FloorFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override {
    //        return floor(newValue);
    //    }
    //};

    //class CeilFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override {
    //        return ceil(newValue);
    //    }
    //};

    //class FractFilter : public Filter {
    //public:
    //    virtual float apply(float value) const override {
    //        return fract(newValue);
    //    }
    //};

    //class MinFilter : public Filter {
    //public:
    //    MinFilter(float mine) : _min(min) {};

    //    virtual float apply(float value) const override {
    //        return glm::min(_min, newValue);
    //    }

    //private:
    //    const float _min;
    //};

    //class MaxFilter : public Filter {
    //public:
    //    MaxFilter(float max) : _max(max) {};
    //    virtual float apply(float newValue, float oldValue) override;
    //private:
    //    const float _max;
    //};
}
#endif
