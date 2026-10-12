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
//

#include <map>
#include <memory>
#include <string>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/serializer.h>
#include <prometheus/text_serializer.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>


using namespace prometheus;

namespace oes::core {
    Metrics::Metrics() :
        registry_(std::make_shared<Registry>()) {
    }

    std::shared_ptr<Registry> Metrics::getRegistry() {
        return registry_;
    }

    std::string Metrics::getSerializedRegistry(){
        auto serializer = std::unique_ptr<Serializer>{new TextSerializer()};
        return serializer->Serialize(registry_->Collect());
    }

    Counter& Metrics::getCounter(const std::string& name) {
        auto it = counters_.find(name);
        if (it != counters_.end()) {
            return it->second;
        } else {
            throw MetricNotFound();
        }
    }

    void Metrics::setCounter(const std::string& name, Counter& counter) {
        if (counters_.find(name) == counters_.end()) {
            counters_.insert({name, counter});
        } else {
            OES_WARNING("Ignoring new counter: '{}' already registered", name);
        }
    }

    Summary& Metrics::getSummary(const std::string& name) {
        auto it = summaries_.find(name);
        if (it != summaries_.end()) {
            return it->second;
        } else {
            throw MetricNotFound();
        }
    }

    void Metrics::setSummary(const std::string& name, Summary& summary) {
        if (summaries_.find(name) == summaries_.end()) {
            summaries_.insert({name, summary});
        } else {
            OES_WARNING("Ignoring new summary: '{}' already registered", name);
        }
    }

    Gauge& Metrics::getGauge(const std::string& name) {
        auto it = gauges_.find(name);
        if (it != gauges_.end()) {
            return it->second;
        }
        throw MetricNotFound();
    }

    void Metrics::setGauge(const std::string& name, Gauge& gauge) {
        if (gauges_.find(name) == gauges_.end()) {
            gauges_.emplace(name, gauge);
        } else {
            OES_WARNING("Ignoring new gauge: '{}' already registered", name);
        }
    }

    Histogram& Metrics::getHistogram(const std::string& name) {
        auto it = histograms_.find(name);
        if (it != histograms_.end()) {
            return it->second;
        }
        throw MetricNotFound();
    }

    void Metrics::setHistogram(const std::string& name, Histogram& histogram) {
        if (histograms_.find(name) == histograms_.end()) {
            histograms_.insert({name, histogram});
        } else {
            OES_WARNING("Ignoring new histogram: '{}' already registered", name);
        }
    }
}