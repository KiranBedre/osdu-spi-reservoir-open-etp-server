// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#include <oes/core/utils/config_enter_3rdparty.h>
#include <benchmark/benchmark.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>
#include "BenchmarkConfig.h"


int main(int argc, char** argv)
{
    // Check prerequisites
    // ============================================================================
    // Can't run in the multi-partition mode
    if (oes::benchmark::BenchmarkConfig::make()->partitionMode() != std::string("single")) {
        std::cout << "OES Benchmark can't run in the multi-partition mode, please set RDMS_DATA_PARTITION_MODE=single" << std::endl;
        return -1;
    }

    // Disable OES logging
    auto& logger = oes::core::Logger::instance();
    logger.setSeverityFilter(oes::core::Message::Severity::max_severity, oes::core::Message::Severity::max_severity);
    logger.disconnectHandlers();

    // The following code is extracted from BENCHMARK_MAIN (please sync it when update the library)
    // ============================================================================
    char arg0_default[] = "benchmark";
    char* args_default = arg0_default;
    if (!argv) {

        argc = 1;
        argv = &args_default;
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
