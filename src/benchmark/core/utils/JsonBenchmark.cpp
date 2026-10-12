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

#include <sstream>
#include <cstring>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <benchmark/benchmark.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Json.h>


static void Core_JsonWrite(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::ostringstream oss;
        oes::core::JSONWriter json(oss);

        json.push_object()
                .key("test_string").string("Hello world!")
                .key("test_integer").number(100)
                .key("test_float").number(3.141592654)
                .key("test_bool").json_bool(true)
                .key("test_null").json_null()
                .key("test_array").push_array()
                    .string("Hello world!")
                    .number(100)
                    .number(3.141592654)
                    .json_true()
                    .json_false()
                    .json_null()
                .pop_array()
            .pop_object();

        benchmark::DoNotOptimize(oss.str());
    }
}
BENCHMARK(Core_JsonWrite);


static void Core_JsonParseArray(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::vector<std::string> vec;
        const char* s = R"([ "The quick brown fox jumps over the lazy dog", "Fox dwarves chop my talking quiz job", 100, 200, 300, 3.14159, true, false ])";

        if (!oes::core::json_parse(s, std::strlen(s), vec)) {
            state.SkipWithError("Unable to parse JSON");
            break;
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(Core_JsonParseArray);
