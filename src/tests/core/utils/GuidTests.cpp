// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

/*
 * Test on Guid
 */

#include <oes/core/utils/Guid.hpp>

#include <sstream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/unordered_map.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>
#include <oes/testing/core/ScopedTimer.h>
#include <oes/testing/core/TestMain.h>


using namespace oes::core;
using namespace oes::testing;

namespace {

BOOST_AUTO_TEST_SUITE(GuidTests)

        BOOST_AUTO_TEST_CASE(test_constructor) {
    // default ctor
    Guid uid1;
    BOOST_CHECK(!uid1.isValid());
    BOOST_CHECK(static_cast<Guid::size_type>(uid1.end() - uid1.begin()) == uid1.size());
    uid1 = make_guid(); // assignment
    BOOST_CHECK(uid1.isValid());
    Guid uid2(Guid::make());
    BOOST_CHECK(uid1 != uid2);
    // copy
    uid2 = uid1;
    BOOST_CHECK(uid1 == uid2);
    // copy ctor
    Guid uid3(uid1);
    BOOST_CHECK(uid3.isValid());
    BOOST_CHECK(uid1 == uid3);
    // stl like ctor
    Guid uid4(uid2.begin(), uid2.end());
    BOOST_CHECK(uid4.isValid());
    BOOST_CHECK(uid4 == uid2);
    // static invalid
    BOOST_CHECK(!Guid::invalid().isValid());
}

BOOST_AUTO_TEST_CASE(test_equal_operator) {
    Guid uid1;
    Guid uid2(uid1);
    Guid uid3(uid1);
    uid3.begin()[0] = uid3.begin()[0] == 0 ? 255 : uid3.begin()[0] - 1;
    Guid uid4(uid1);
    uid4.begin()[15] = uid4.begin()[15] == 0 ? 255 : uid3.begin()[15] - 1;
    BOOST_CHECK(uid1 == uid2);
    BOOST_CHECK(!(uid1 != uid2));
    BOOST_CHECK(uid3 != uid1);
    BOOST_CHECK(uid4 != uid1);
}

BOOST_AUTO_TEST_CASE(test_swap) {
    Guid uid1(Guid::make());
    Guid uid2(uid1);
    Guid uid3(Guid::make());
    Guid uid4(uid3);
    BOOST_CHECK(uid2 != uid4);
    swap(uid2, uid4);
    BOOST_CHECK(uid3 == uid2);
    BOOST_CHECK(uid1 == uid4);
}

BOOST_AUTO_TEST_CASE(test_reset) {
    Guid uid1(Guid::make());
    BOOST_CHECK(uid1.isValid());
    uid1.reset();
    BOOST_CHECK(!uid1.isValid());
    uid1.reset(); // again
    BOOST_CHECK(!uid1.isValid());
}

BOOST_AUTO_TEST_CASE(test_comparison_operators) {
    Guid uid1(Guid::make());
    Guid uid2(uid1);
    Guid uid3(Guid::make());

    // strict weak ordering
    BOOST_CHECK(uid1 == uid2);
    BOOST_CHECK(!(uid1 < uid2));
    BOOST_CHECK(!(uid1 > uid2));

    BOOST_CHECK(uid1 <= uid2);
    BOOST_CHECK(uid1 >= uid2);
    if (uid1 < uid3) {
        BOOST_CHECK(uid1 != uid3);
        BOOST_CHECK(!(uid1 > uid2));
        swap(uid1, uid3);
    }
    BOOST_CHECK(uid1 > uid3);
    BOOST_CHECK(uid3 < uid1);
    BOOST_CHECK(!(uid1 < uid3));
    BOOST_CHECK(!(uid3 > uid1));
    BOOST_CHECK(uid1 >= uid3);
    BOOST_CHECK(uid3 <= uid1);
    swap(uid1, uid3);
    BOOST_CHECK(uid1 < uid3);
    BOOST_CHECK(uid3 > uid1);
}

BOOST_AUTO_TEST_CASE(test_string_conversion) {
    // valid case
    Guid uid1("542a857f39a301b27322a27383f995f5");
    BOOST_CHECK(uid1.isValid());
    BOOST_CHECK(uid1.begin()[0]==84);
    BOOST_CHECK(uid1.begin()[1]==42);
    BOOST_CHECK(uid1.begin()[15]==245);
    // valid xml case
    Guid uid2("542a857f-39a3-01b2-7322-a27383f995f5");
    BOOST_CHECK(uid2.isValid());
    BOOST_CHECK(uid1 == uid2);
    // invalid empty case
    Guid uid3("");
    BOOST_CHECK(!uid3.isValid());
    BOOST_CHECK(Guid::invalid() == uid3);
    BOOST_CHECK(Guid::parse("") == uid3);
    // invalid case
    BOOST_CHECK(!Guid("").isValid());
    BOOST_CHECK(!Guid::parse("542a857f39a301b27322a27383f995f").isValid()); // wrong length
    BOOST_CHECK(!Guid::parse("x42a857f39a301b27322a27383f995f5").isValid()); // not hexa
    BOOST_CHECK(!Guid::parse("x4-2a857f39a-301b2-7322a27383-f995f5").isValid()); // wrong dash

    const char* legacy_format = "542a857f-39a301b2-7322-a27383f995f5";
    BOOST_CHECK(!Guid::parse(legacy_format).isValid()); // not parsed by default
    Guid uid4; // but can be parsed in lax-mode, via reparse()
    BOOST_CHECK(uid4.reparse(legacy_format, strlen(legacy_format), true));
    BOOST_CHECK(uid4.isValid());
    std::string gouid(legacy_format);
    gouid.erase(8, 1);
    gouid.erase(16, 1);
    gouid.erase(20, 1);
    BOOST_CHECK(gouid.find('-') == std::string::npos);
    BOOST_CHECK(uid4 == Guid::parse(gouid));
}

BOOST_AUTO_TEST_CASE(test_string_output) {
    const unsigned char uui[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 33, 66, 95, 226, 255};
    Guid uid1(uui, uui + sizeof(uui));
    BOOST_CHECK_EQUAL(uid1.toStringNoDash(), "0102030405060708090a1021425fe2ff");
    BOOST_CHECK_EQUAL(uid1.toXmlString(), "01020304-0506-0708-090a-1021425fe2ff");

    BOOST_CHECK_EQUAL(Guid::invalid().toStringNoDash(), "00000000000000000000000000000000");
    BOOST_CHECK_EQUAL(Guid::invalid().toXmlString(), "00000000-0000-0000-0000-000000000000");
}

BOOST_AUTO_TEST_CASE(test_streaming) {
    const unsigned char uui[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 33, 66, 95, 226, 255};
    Guid uid1(uui, uui + sizeof(uui));
    std::ostringstream oss1;
    oss1 << uid1;
    BOOST_CHECK_EQUAL(oss1.str(), "01020304-0506-0708-090a-1021425fe2ff");
    std::ostringstream oss2;
    oss2 << Guid::invalid();
    BOOST_CHECK_EQUAL(oss2.str(), "00000000-0000-0000-0000-000000000000");
}

BOOST_AUTO_TEST_CASE(test_containers_interop) {
    const size_t count = 1000; // increase count for qualitative "perf" test

    std::vector<Guid> guids(count);
    std::generate(guids.begin(), guids.end(), &make_guid);

    // tests randomness of generator (somewhat) and op<()
    std::sort(guids.begin(), guids.end());
    std::vector<Guid>::iterator new_end = std::unique(guids.begin(), guids.end());
    guids.resize(std::distance(guids.begin(), new_end));
    BOOST_CHECK(guids.size() == count);

    // tests randomness of generator (somewhat) and op<()
    std::set<Guid> guid_set(guids.begin(), guids.end());
    BOOST_CHECK(guid_set.size() == count);

    // tests hash_value()
    boost::unordered_set<Guid> guid_hashset(guids.begin(), guids.end());
    BOOST_CHECK(guid_hashset.size() == count);

    // tests hash_value()
    std::unordered_set<Guid> guid_stduset(guids.begin(), guids.end());
    BOOST_CHECK(guid_stduset.size() == count);

    // tests find-able in ordered, hashed, and sorted containers
    for (const Guid& guid : guids) {
        BOOST_CHECK(guid_set.find(guid) != guid_set.end());
        BOOST_CHECK(guid_hashset.find(guid) != guid_hashset.end());
        BOOST_CHECK(guid_stduset.find(guid) != guid_stduset.end());
        BOOST_CHECK(std::binary_search(guids.begin(), guids.end(), guid));
    }
}

BOOST_AUTO_TEST_CASE(test_make_deterministic) {
    const std::string input = "PropertyType/urn:resqml:oes.com:Porosity";

    // Legacy code from DX's PropertyTypesImpl.cpp, to double-check
    // backward compatibility of make_deterministic();
    boost::uuids::string_generator sgen;
    boost::uuids::uuid dns_namespace_uuid = sgen("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    boost::uuids::name_generator gen(dns_namespace_uuid);
    std::stringstream ss; ss << gen(input);
    const std::string guid_str = ss.str();

    // Should be identical. And calling it twice should return the same result.
    BOOST_CHECK_EQUAL(guid_str, Guid::make_deterministic(Guid::dns_namespace(), input).toXmlString());
    BOOST_CHECK_EQUAL(guid_str, Guid::make_deterministic(Guid::dns_namespace(), input).toXmlString());

    // Using different namespace should yield different Guids for the same input
    std::vector<Guid> guids({
        Guid::make_deterministic(Guid::dns_namespace(), input),
        Guid::make_deterministic(Guid::url_namespace(), input),
        Guid::make_deterministic(Guid::oid_namespace(), input),
        Guid::make_deterministic(Guid::x500_namespace(), input),
        Guid::make_deterministic(Guid::oes_namespace(), input)
    });
    std::unordered_set<Guid> guid_set(guids.begin(), guids.end());
    BOOST_CHECK_EQUAL(guids.size(), guid_set.size());

    // Default namespace is the oes one
    BOOST_CHECK_EQUAL(
        Guid::make_deterministic(input),
        Guid::make_deterministic(Guid::oes_namespace(), input)
    );

    // Passing a guid-string as "namespace" uses that guid as-is as namespace
    const Guid guid_ns = Guid::parse(guid_str);
    BOOST_CHECK(guid_ns.isValid());
//  BOOST_CHECK_EQUAL((bool)guid_ns, guid_ns.isValid()); // explicit op bool()
    BOOST_CHECK_EQUAL(
        Guid::make_deterministic(guid_str, input),
        Guid::make_deterministic(guid_ns, input)
    );

    // While passing arbitrary test for the NS yields a different result
    const std::string text_ns = "not an NS";
    const Guid guid_with_text_ns = Guid::make_deterministic(text_ns, input);
    BOOST_CHECK(guid_with_text_ns != Guid::make_deterministic(guid_ns, input));

    // And that the result is still deterministic
    BOOST_CHECK_EQUAL(
        guid_with_text_ns, Guid::make_deterministic(text_ns, input)
    );

    // And it's based on the oes namespace for the namespace
    BOOST_CHECK_EQUAL(
        guid_with_text_ns,
        Guid::make_deterministic(
            Guid::make_deterministic(text_ns), input
        )
    );
}

BOOST_AUTO_TEST_CASE(bench_generator_perf_in_parallel) {
    OES_TEST_CHECK_PERF_ENABLE();

    const int count = 16*1000*1000;
    std::string message = "generate ";
    message += std::to_string(count);
    message += " guids";

    // test performance on single CPU
    {
        ScopedTimer timer(message, true);
        for (int i = 0; i < count; i++) {
            Guid::make();
        }
    }

    // test performance on 4 CPUs
    {
        ScopedTimer timer(message + " in parallel on 4 CPUs", true);
#pragma omp parallel for schedule(static) default(shared) num_threads(4)
        for (int i = 0; i < count; i++) {
            Guid::make();
        }
    }

    // test uniqueness of generated guids
    std::vector<Guid> guids(count);
#pragma omp parallel for schedule(static) default(shared) num_threads(4)
    for (int i = 0; i < count; i++) {
        guids[i] = Guid::make();
    }
    std::sort(guids.begin(), guids.end());
    std::vector<Guid>::iterator new_end = std::unique(guids.begin(), guids.end());
    guids.resize(std::distance(guids.begin(), new_end));
    BOOST_CHECK(guids.size() == count);
}

BOOST_AUTO_TEST_CASE(test_generator_in_parallel) {
    // generate guids in parallel
    const int count = 16*1000;
    std::vector<Guid> guids(count);
#pragma omp parallel for schedule(static) default(shared) num_threads(4)
    for (int i = 0; i < count; i++) {
        guids[i] = Guid::make();
    }
    // tests uniqueness of generated guids
    std::sort(guids.begin(), guids.end());
    std::vector<Guid>::iterator new_end = std::unique(guids.begin(), guids.end());
    guids.resize(std::distance(guids.begin(), new_end));
    BOOST_CHECK(guids.size() == count);
}

template <typename Map>
size_t insert_then_lookup(const char* map_type, Map map, const std::vector<std::string>& guids) {
    std::ostringstream oss_insert;
    oss_insert << "Insert " << guids.size() << " GUIDs into " << map_type;
    std::ostringstream oss_lookup;
    oss_lookup << "Lookup " << guids.size() << " GUIDs from " << map_type;
    std::string value = "value";
    {
        ScopedTimer timer(oss_insert.str(), true);
        for (size_t i = 0; i < guids.size(); i++) {
            map[guids[i]] = value;
        }
    }
    size_t found = 0;
    {
        ScopedTimer timer(oss_lookup.str(), true);
        for (size_t i = 0; i < guids.size(); i++) {
            if (map.find(guids[i]) != map.end()) {
                ++found;
            }
        }
    }
    return found;
}

BOOST_AUTO_TEST_CASE(bench_perf_map_insert_lookup) {
    OES_TEST_CHECK_PERF_ENABLE();
    // Prepare vector of keys
    const size_t count = 250*1000;
    std::vector<std::string> guids(count);
    const std::string prefix = "obj_WellBoreInterpretation_";
    for (size_t i = 0; i < count; i++) {
        guids[i] = prefix + Guid::make().toXmlString();
    }
    {
        std::map<std::string, std::string> map;
        size_t found = insert_then_lookup("std::map", map, guids);
        BOOST_CHECK_EQUAL(found, count);
    }
    {
        std::unordered_map<std::string, std::string> map(count);
        size_t found = insert_then_lookup("std::unordered_map", map, guids);
        BOOST_CHECK_EQUAL(found, count);
    }
    {
        std::unordered_map<std::string, std::string> map;
        size_t found = insert_then_lookup("std::unordered_map no size", map, guids);
        BOOST_CHECK_EQUAL(found, count);
    }
    {
        boost::unordered_map<std::string, std::string> map(count);
        size_t found = insert_then_lookup("boost::unordered_map", map, guids);
        BOOST_CHECK_EQUAL(found, count);
    }
    {
        boost::unordered_map<std::string, std::string> map;
        size_t found = insert_then_lookup("boost::unordered_map no size", map, guids);
        BOOST_CHECK_EQUAL(found, count);
    }
    {
        boost::container::flat_map<std::string, std::string> map;
        std::ostringstream oss_insert;
        const char* map_type = "boost::flat_map";
        oss_insert << "Insert " << guids.size() << " GUIDs into " << map_type;
        std::string value = "value";
        {
            ScopedTimer timer(oss_insert.str(), true);
            typedef std::pair<std::string, std::string> Entry;
            std::vector<Entry> entry_vec;
            entry_vec.reserve(count);
            for (const auto& guid : guids) {
                entry_vec.emplace_back(guid, value);
            }
            std::sort(entry_vec.begin(), entry_vec.end(), [](const Entry& lhs, const Entry& rhs) {
                return lhs.first < rhs.first;
            });
            entry_vec.erase(std::unique(entry_vec.begin(), entry_vec.end(), [](const Entry& lhs, const Entry& rhs) {
                return lhs.first == rhs.first;
            }), entry_vec.end());
            for (auto& entry : entry_vec) {
                map.insert(entry);
            }
        }
        std::ostringstream oss_lookup;
        oss_lookup << "Lookup " << guids.size() << " GUIDs from " << map_type;
        size_t found = 0;
        {
            ScopedTimer timer(oss_lookup.str(), true);
            for (const auto& guid : guids) {
                if (map.find(guid) != map.end()) {
                    ++found;
                }
            }
        }
        BOOST_CHECK_EQUAL(found, count);
        // Unsorted insertion very slow for > 25K
        //insert_then_lookup("boost::flat_map", map, guids);
    }
}

BOOST_AUTO_TEST_CASE(bench_parsing) {
    const int count = oes::testing::perfValues(100, 1000 * 1000);

    std::vector<std::string> nod_guid_strs; // NO-Dash
    std::vector<std::string> xml_guid_strs; // 4 dashes
    std::vector<std::string> sku_guid_strs; // legacy 3 dashes format
    nod_guid_strs.reserve(count);
    xml_guid_strs.reserve(count);
    sku_guid_strs.reserve(count);
    for (int i = 0; i < count; ++i) {
        Guid guid = Guid::make();
        nod_guid_strs.emplace_back(guid.toStringNoDash());
        xml_guid_strs.emplace_back(guid.toXmlString());
        sku_guid_strs.emplace_back(guid.toXmlString().erase(13, 1));
    }

    std::vector<Guid> xml_guids;
    std::vector<Guid> nod_guids;
    std::vector<Guid> sku_guids;
    xml_guids.reserve(count);
    nod_guids.reserve(count);
    sku_guids.reserve(count);

    {
        oes::testing::ScopedTimer timer("Parsing No-Dash guids");
        for (const std::string& guid_str : nod_guid_strs) {
            nod_guids.emplace_back(Guid::parse(guid_str));
        }
    }
    {
        oes::testing::ScopedTimer timer("Parsing 4-Dashes XML guids");
        for (const std::string& guid_str : xml_guid_strs) {
            xml_guids.emplace_back(Guid::parse(guid_str));
        }
    }
    int errors = 0;
    {
        oes::testing::ScopedTimer timer("Parsing 3-Dashes legacy guids");
        for (const std::string& guid_str : sku_guid_strs) {
            sku_guids.emplace_back();
            if (!sku_guids.back().reparse(guid_str.c_str(), guid_str.length(), true)) {
                ++errors;
            }
        }
    }
    {
        // simulate the impact of forcing having a std::string to parse
        oes::testing::ScopedTimer timer("Parsing 3-Dashes legacy guids (substr)");
        for (const std::string& guid_str : sku_guid_strs) {
            sku_guids.emplace_back();
            std::string guid_copy = guid_str.substr(0, guid_str.length());
            if (!sku_guids.back().reparse(guid_copy.c_str(), guid_copy.length(), true)) {
                ++errors;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        if (nod_guids[i] != xml_guids[i]) {
            ++errors;
        }
        if (nod_guids[i] != sku_guids[i]) {
            ++errors;
        }
    }
    BOOST_REQUIRE_EQUAL(errors, 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
