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

#include <memory>
#include <iostream>
#include <prometheus/summary.h>
#include <typeinfo>
#include <oes/core/utils/Metrics.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <oes/core/utils/config_leave_3rdparty.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace oes::core;

prometheus::Counter& prepareCounter(const std::string& name) {
    auto registry = Metrics::instance().getRegistry();
    auto& counter_fam = prometheus::BuildCounter()
                            .Name(name + "_fam")
                            .Help("Family Counter")
                            .Register(*registry);
    auto& test_counter = counter_fam.Add({{"counter", name}});
    return test_counter;
}

prometheus::Summary& prepareSummary(const std::string& name) {
    auto registry = Metrics::instance().getRegistry();
    auto& summary_fam = prometheus::BuildSummary()
                             .Name(name + "_fam")
                             .Help("Family Summary")
                             .Register(*registry);
    auto& test_summary = summary_fam.Add(
                            {}, Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    return test_summary;
}

BOOST_AUTO_TEST_SUITE(MetricsTests)

BOOST_AUTO_TEST_CASE(test_get_registry) {
    auto registry = Metrics::instance().getRegistry();
    BOOST_CHECK(registry);
}

BOOST_AUTO_TEST_CASE(test_set_get_counter_success) {
    auto& test_counter = prepareCounter("test_counter");
    Metrics::instance().setCounter("test_counter", test_counter);

    auto& expected = Metrics::instance().getCounter("test_counter");
    BOOST_CHECK_EQUAL(&test_counter, &expected);
}

BOOST_AUTO_TEST_CASE(test_get_counter_failed) {
    BOOST_CHECK_THROW(Metrics::instance().getCounter("no_test_counter"), oes::core::MetricNotFound);
}

BOOST_AUTO_TEST_CASE(test_set_counter_multiple_ignored) {
    auto& test_counter = prepareCounter("test_counter");
    Metrics::instance().setCounter("test_counter", test_counter);
    auto& test_counter_2 = prepareCounter("test_counter_2");

    Metrics::instance().setCounter("test_counter", test_counter_2);

    auto& expected = Metrics::instance().getCounter("test_counter");
    BOOST_CHECK_EQUAL(&test_counter, &expected);
}

BOOST_AUTO_TEST_CASE(test_set_get_summary_success) {
    auto& test_summary = prepareSummary("test_summary");
    Metrics::instance().setSummary("test_summary", test_summary);

    auto& expected = Metrics::instance().getSummary("test_summary");
    BOOST_CHECK_EQUAL(&test_summary, &expected);
}

BOOST_AUTO_TEST_CASE(test_get_summary_failed) {
    BOOST_CHECK_THROW(Metrics::instance().getSummary("no_test_summary"), oes::core::MetricNotFound);
}

BOOST_AUTO_TEST_CASE(test_set_summary_multiple_ignored) {
    auto& test_summary = prepareSummary("test_summary");
    Metrics::instance().setSummary("test_summary", test_summary);
    auto& test_summary_2 = prepareSummary("test_summary_2");

    Metrics::instance().setSummary("test_summary", test_summary_2);

    auto& expected = Metrics::instance().getSummary("test_summary");
    BOOST_CHECK_EQUAL(&test_summary, &expected);
}

BOOST_AUTO_TEST_CASE(test_get_serialized_registry) {
    auto& test_counter = prepareCounter("test_counter");
    Metrics::instance().setCounter("test_counter", test_counter);
    auto& test_counter_2 = prepareCounter("test_counter_2");
    Metrics::instance().setCounter("test_counter_2", test_counter_2);
    
    auto serializedRegistry = Metrics::instance().getSerializedRegistry();
    std::vector<std::string> lines;
    boost::algorithm::split(lines, serializedRegistry, boost::is_any_of("\n"));

    BOOST_CHECK_EQUAL(lines[0], "# HELP test_counter_fam Family Counter"); 
    BOOST_CHECK_EQUAL(lines[1], "# TYPE test_counter_fam counter"); 
    BOOST_CHECK(boost::algorithm::contains(lines[2], "test_counter_fam{counter=\"test_counter\"}")); 
    BOOST_CHECK_EQUAL(lines[3], "# HELP test_counter_2_fam Family Counter");
    BOOST_CHECK_EQUAL(lines[4], "# TYPE test_counter_2_fam counter");
    BOOST_CHECK(boost::algorithm::contains(lines[5], "test_counter_2_fam{counter=\"test_counter_2\"}")); 
}

BOOST_AUTO_TEST_SUITE_END()

}