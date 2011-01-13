
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "benchmarksuite.h"

// appleseed.foundation headers.
#include "foundation/platform/thread.h"
#include "foundation/platform/timer.h"
#include "foundation/platform/types.h"
#include "foundation/utility/benchmark/benchmarkresult.h"
#include "foundation/utility/benchmark/ibenchmarkcase.h"
#include "foundation/utility/benchmark/ibenchmarkcasefactory.h"
#include "foundation/utility/benchmark/timingresult.h"
#include "foundation/utility/filter.h"
#include "foundation/utility/stopwatch.h"

// Standard headers.
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace foundation
{

//
// BenchmarkSuite class implementation.
//

namespace
{
    // An empty benchmark case used for measuring the overhead of calling IBenchmarkCase::run().
    struct EmptyBenchmarkCase
      : public IBenchmarkCase
    {
        // Return the name of the benchmark case.
        virtual const char* get_name() const
        {
            return "Empty";
        }

        // Run the benchmark case.
        virtual void run()
        {
        }
    };
}

struct BenchmarkSuite::Impl
{
    typedef Stopwatch<X86Timer> StopwatchType;

    string                          m_name;
    vector<IBenchmarkCaseFactory*>  m_factories;

    static double measure_runtime_seconds(
        IBenchmarkCase*         benchmark,
        StopwatchType&          stopwatch,
        const size_t            iteration_count)
    {
        stopwatch.start();

        for (size_t i = 0; i < iteration_count; ++i)
            benchmark->run();

        stopwatch.measure();

        return stopwatch.get_seconds();
    }

    static double measure_runtime_ticks(
        IBenchmarkCase*         benchmark,
        StopwatchType&          stopwatch,
        const size_t            iteration_count)
    {
        stopwatch.start();

        for (size_t i = 0; i < iteration_count; ++i)
            benchmark->run();

        stopwatch.measure();

        return static_cast<double>(stopwatch.get_ticks());
    }

    template <typename MeasurementFunction>
    static double measure_runtime(
        IBenchmarkCase*         benchmark,
        StopwatchType&          stopwatch,
        MeasurementFunction&    measurement_function,
        const size_t            iteration_count,
        const size_t            measurement_count)
    {
        double lowest_runtime = numeric_limits<double>::max();

        for (size_t i = 0; i < measurement_count; ++i)
        {
            const double runtime =
                measurement_function(
                    benchmark,
                    stopwatch,
                    iteration_count);

            lowest_runtime = min(lowest_runtime, runtime);
        }

        return lowest_runtime;
    }

    struct BenchmarkParams
    {
        size_t  m_iteration_count;
        size_t  m_measurement_count;
    };

    // Estimate benchmarking parameters.
    static void estimate_benchmark_params(
        IBenchmarkCase*         benchmark,
        StopwatchType&          stopwatch,
        BenchmarkParams&        params)
    {
        const size_t InitialIterationCount = 100;
        const size_t InitialMeasurementCount = 100;
        const double TargetMeasurementRuntime = 1.0e-4;     // seconds
        const double TargetTotalRuntime = 0.2;              // seconds

        // Measure the runtime for this initial number of iterations.
        const double runtime =
            measure_runtime(
                benchmark,
                stopwatch,
                measure_runtime_seconds,
                InitialIterationCount,
                InitialMeasurementCount);

        // Compute the number of iterations.
        params.m_iteration_count =
            static_cast<size_t>(InitialIterationCount * (TargetMeasurementRuntime / runtime));
        params.m_iteration_count = max<size_t>(1, params.m_iteration_count);

        // Compute the number of measurements.
        params.m_measurement_count =
            static_cast<size_t>(TargetTotalRuntime / TargetMeasurementRuntime);
        params.m_measurement_count = max<size_t>(1, params.m_measurement_count);
    }

    static double measure_iteration_runtime(
        IBenchmarkCase*         benchmark,
        StopwatchType&          stopwatch,
        const BenchmarkParams&  params)
    {
        return
            measure_runtime(
                benchmark,
                stopwatch,
                measure_runtime_ticks,
                params.m_iteration_count,
                params.m_measurement_count)
            / params.m_iteration_count;
    }

    // Measure and return the overhead (in ticks) of running an empty benchmark case.
    static double measure_call_overhead(
        StopwatchType&          stopwatch,
        const BenchmarkParams&  params)
    {
        auto_ptr<IBenchmarkCase> empty_case(new EmptyBenchmarkCase());
        return measure_iteration_runtime(empty_case.get(), stopwatch, params);
    }
};

// Constructor.
BenchmarkSuite::BenchmarkSuite(const char* name)
  : impl(new Impl())
{
    assert(name);
    impl->m_name = name;
}

// Destructor.
BenchmarkSuite::~BenchmarkSuite()
{
    delete impl;
}

// Return the name of the test suite.
const char* BenchmarkSuite::get_name() const
{
    return impl->m_name.c_str();
}

// Register a benchmark case (via its factory function).
void BenchmarkSuite::register_case(IBenchmarkCaseFactory* factory)
{
    assert(factory);
    impl->m_factories.push_back(factory);
}

// Run all the registered benchmark cases.
void BenchmarkSuite::run(BenchmarkResult& suite_result) const
{
    LetThroughFilter filter;
    run(filter, suite_result);
}

// Run those benchmark cases whose name pass a given filter.
void BenchmarkSuite::run(
    const IFilter&      filter,
    BenchmarkResult&    suite_result) const
{
    BenchmarkingThreadContext benchmarking_context;

    // Tell the listeners that a benchmark suite is about to be executed.
    suite_result.begin_suite(*this);

    for (size_t i = 0; i < impl->m_factories.size(); ++i)
    {
        IBenchmarkCaseFactory* factory = impl->m_factories[i];

        // Skip benchmark cases that aren't let through by the filter.
        if (!filter.accepts(factory->get_name()))
            continue;

        // Instantiate the benchmark case.
        auto_ptr<IBenchmarkCase> benchmark(factory->create());

        // Recreate the stopwatch (and the underlying timer) for every benchmark
        // case, since the CPU frequency will fluctuate quite a bit depending on
        // the CPU load.  We need an up-to-date frequency estimation in order to
        // compute accurate call rates.
        Impl::StopwatchType stopwatch(100000);

        // Tell the listeners that a benchmark case is about to be executed.
        suite_result.begin_case(*this, *benchmark.get());

        try
        {
            suite_result.signal_case_execution();

            // Estimate benchmarking parameters.
            Impl::BenchmarkParams params;
            Impl::estimate_benchmark_params(
                benchmark.get(),
                stopwatch,
                params);

            // Measure the overhead of calling IBenchmarkCase::run().
            const double overhead =
                Impl::measure_call_overhead(stopwatch, params);

            // Run the benchmark case.
            const double execution_time =
                Impl::measure_iteration_runtime(
                    benchmark.get(),
                    stopwatch,
                    params);

            // Gather the timing results.
            TimingResult timing_result;
            timing_result.m_iteration_count = params.m_iteration_count;
            timing_result.m_measurement_count = params.m_measurement_count;
            timing_result.m_frequency = static_cast<double>(stopwatch.get_timer().frequency());
            timing_result.m_ticks = execution_time > overhead ? execution_time - overhead : 0.0;

            // Post the timing result.
            suite_result.write(
                *this,
                *benchmark.get(),
                __FILE__,
                __LINE__,
                timing_result);
        }
        catch (const exception& e)
        {
            suite_result.write(
                *this,
                *benchmark.get(),
                __FILE__,
                __LINE__,
                "An unexpected exception was caught: %s.",
                e.what());

            suite_result.signal_case_failure();
        }
        catch (...)
        {
            suite_result.write(
                *this,
                *benchmark.get(),
                __FILE__,
                __LINE__,
                "An unexpected exception was caught.");

            suite_result.signal_case_failure();
        }

        // Tell the listeners that the benchmark case execution has ended.
        suite_result.end_case(*this, *benchmark.get());
    }

    // Tell the listeners that the benchmark suite execution has ended.
    suite_result.end_suite(*this);
}

}   // namespace foundation
