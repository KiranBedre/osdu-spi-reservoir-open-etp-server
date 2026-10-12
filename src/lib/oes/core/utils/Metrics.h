// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

#pragma once

#include <exception>
#include <map>
#include <memory>
#include <string>

#include <oes/core/utils/export.h>
#include <oes/core/utils/Singleton.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <prometheus/summary.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>
#include <oes/core/utils/config_leave_3rdparty.h>


using namespace prometheus;

namespace oes::core {

    struct MetricNotFound: public std::exception {
       const char* what() const throw() override {
            return "Metric not registered";
        }
    };


    class OES_CORE_UTILS_EXPORT Metrics : public PhoenixSingleton<Metrics> {
        public:
            // TODO add persistant registry
            Metrics();
            ~Metrics() override = default;

            std::shared_ptr<Registry> getRegistry();

            std::string getSerializedRegistry();

            Counter& getCounter(const std::string& name);

            void setCounter(const std::string& name, Counter& counter);

            Summary& getSummary(const std::string& name);

            void setSummary(const std::string& name, Summary& summary);

            Gauge& getGauge(const std::string& name);

            void setGauge(const std::string& name, prometheus::Gauge& gauge);

            Histogram& getHistogram(const std::string& name);

            void setHistogram(const std::string& name, Histogram& histogram);

        private:
            std::shared_ptr<Registry> registry_;
            std::map<std::string, Counter&> counters_;
            std::map<std::string, Summary&> summaries_;
            std::map<std::string, Gauge&> gauges_;
            std::map<std::string, Histogram&> histograms_;
    };
} // namespace oes::core