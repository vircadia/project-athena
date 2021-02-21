//
//  Query.h
//  interface/src/gpu
//
//  Created by Niraj Venkat on 7/7/2015.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_gpu_Query_h
#define hifi_gpu_Query_h

#include <assert.h>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <SimpleMovingAverage.h>

#include "Format.h"

namespace gpu {

    class Batch;

    class Query {
    public:
        using Handler = std::function<void(const Query&)>;

        Query(const Handler& returnHandler, const std::string& name = "gpu::query");
        ~Query();

        double getGPUElapsedTime() const;
        double getBatchElapsedTime() const;

        const std::string& getName() const { return _name; }

        // Only for gpu::Context
        const GPUObjectPointer gpuObject {};
        void triggerReturnHandler(uint64_t queryResult, uint64_t batchElapsedTime);
    protected:
        Handler _returnHandler;

        const std::string _name;
        uint64_t _queryResult { 0 };
        uint64_t _usecBatchElapsedTime { 0 };

        friend class Serializer;
        friend class Deserializer;
    };
    
    typedef std::shared_ptr<Query> QueryPointer;
    typedef std::vector< QueryPointer > Queries;


    // gpu RangeTimer is just returning an estimate of the time taken by a chunck of work delimited by the 
    // begin and end calls repeated for several times.
    // The result is always a late average of the time spent for that same task a few cycles ago.
    class RangeTimer {
    public:
        RangeTimer(const std::string& name);
        void begin(gpu::Batch& batch);
        void end(gpu::Batch& batch);
        
        double getGPUAverage() const;
        double getBatchAverage() const;

        const std::string& name() const { return _name; }

    protected:
        
        static const int QUERY_QUEUE_SIZE { 4 };

        const std::string _name;
        gpu::Queries _timerQueries;
        int _headIndex = -1;
        int _tailIndex = -1;

        MovingAverage<double, QUERY_QUEUE_SIZE * 2> _movingAverageGPU;
        MovingAverage<double, QUERY_QUEUE_SIZE * 2> _movingAverageBatch;

        int rangeIndex(int index) const { return (index % QUERY_QUEUE_SIZE); }
    };
    
    using RangeTimerPointer = std::shared_ptr<RangeTimer>;
    
};

#endif
