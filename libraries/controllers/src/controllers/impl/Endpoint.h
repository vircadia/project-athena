//
//  Created by Bradley Austin Davis 2015/10/09
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_Controllers_Endpoint_h
#define hifi_Controllers_Endpoint_h

#include <list>
#include <memory>
#include <functional>

#include <QtCore/QObject>

#include "../AxisValue.h"
#include "../Input.h"
#include "../Pose.h"

namespace controller {
    /*
    * Encapsulates a particular input / output,
    * i.e. Hydra.Button0, Standard.X, Action.Yaw
    */
    class Endpoint : public QObject {
        Q_OBJECT;
    public:
        using Pointer = std::shared_ptr<Endpoint>;
        using List = std::list<Pointer>;
        using Pair = std::pair<Pointer, Pointer>;
        using ReadLambda = std::function<float()>;
        using WriteLambda = std::function<void(float)>;

        Endpoint(const Input& input) : _input(input) {}
        virtual AxisValue value() { return peek(); }
        virtual AxisValue peek() const = 0;
        virtual void apply(AxisValue value, const Pointer& source) = 0;
        virtual Pose peekPose() const { return Pose(); };
        virtual Pose pose() { return peekPose(); }
        virtual void apply(const Pose& value, const Pointer& source) {}
        virtual bool isPose() const { return _input.isPose(); }
        virtual bool writeable() const { return true; }
        virtual bool readable() const { return true; }
        virtual void reset() { }

        const Input& getInput() { return _input;  }

    protected:
        Input _input;
    };

    class LambdaEndpoint : public Endpoint {
    public:
        using Endpoint::apply;
        LambdaEndpoint(ReadLambda readLambda, WriteLambda writeLambda = [](float) {})
            : Endpoint(Input::INVALID_INPUT), _readLambda(readLambda), _writeLambda(writeLambda) { }

        virtual AxisValue peek() const override { return AxisValue(_readLambda(), 0); }
        virtual void apply(AxisValue value, const Pointer& source) override { _writeLambda(value.value); }

    private:
        ReadLambda _readLambda;
        WriteLambda _writeLambda;
    };

    extern Endpoint::WriteLambda DEFAULT_WRITE_LAMBDA;

    class LambdaRefEndpoint : public Endpoint {
    public:
        using Endpoint::apply;
        LambdaRefEndpoint(const ReadLambda& readLambda, const WriteLambda& writeLambda = DEFAULT_WRITE_LAMBDA)
            : Endpoint(Input::INVALID_INPUT), _readLambda(readLambda), _writeLambda(writeLambda) {
        }

        virtual AxisValue peek() const override { return AxisValue(_readLambda(), 0); }
        virtual void apply(AxisValue value, const Pointer& source) override { _writeLambda(value.value); }

    private:
        const ReadLambda& _readLambda;
        const WriteLambda& _writeLambda;
    };


    class VirtualEndpoint : public Endpoint {
    public:
        VirtualEndpoint(const Input& id = Input::INVALID_INPUT)
            : Endpoint(id) {
        }

        virtual AxisValue peek() const override { return _currentValue; }
        virtual void apply(AxisValue value, const Pointer& source) override { _currentValue = value; }

        virtual Pose peekPose() const override { return _currentPose; }
        virtual void apply(const Pose& value, const Pointer& source) override {
            _currentPose = value;
        }
    protected:
        AxisValue _currentValue { 0.0f, 0, false };
        Pose _currentPose {};
    };

}

#endif
