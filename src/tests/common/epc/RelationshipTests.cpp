// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/common/epc/Relationship.h>

#include <algorithm>
#include <sstream>

#include <oes/testing/core/TestMain.h>

namespace {

using namespace oes::common;

//============================================================================

BOOST_AUTO_TEST_SUITE(EpcRelationshipTests)

BOOST_AUTO_TEST_CASE(test_ctors) {
    const epc::Relationship empty;

    // internal by default
    BOOST_CHECK(empty.relMode() == epc::RelMode::eInternal);
    BOOST_CHECK_EQUAL(empty.isInternalTarget(), true);
    BOOST_CHECK(empty.relType() == epc::RelType::eUnknownType);
    BOOST_CHECK_EQUAL(empty.getId().empty(), true);
    BOOST_CHECK_EQUAL(empty.getType().empty(), true);
    BOOST_CHECK_EQUAL(empty.getTarget().empty(), true);

    // compare to self
    BOOST_CHECK(empty == empty);
    BOOST_CHECK(!(empty != empty));
    BOOST_CHECK(!(empty < empty));

    // compare to another "empty" rel
    epc::Relationship rel;
    BOOST_CHECK(empty == rel);
    BOOST_CHECK(!(empty != rel));
    BOOST_CHECK(!(empty < rel));

    epc::Relationship rel_int("target", "type", "id", true);
    BOOST_CHECK(rel_int.relMode() == epc::RelMode::eInternal);
    BOOST_CHECK(rel_int.relType() == epc::RelType::eUnknownType);
    BOOST_CHECK_EQUAL(
        rel_int.toString(),
        "<Relationship "
            "Id=\"id\" "
            "Type=\"type\" "
            "Target=\"target\""
        "/>"
    );

    epc::Relationship rel_ext("target", "type", "id", false);
    BOOST_CHECK(rel_ext.relMode() == epc::RelMode::eExternal);
    BOOST_CHECK(rel_ext.relType() == epc::RelType::eUnknownType);
    BOOST_CHECK_EQUAL( // for branch-coverage in .toString()
        rel_ext.toString(),
        "<Relationship "
            "Id=\"id\" "
            "Type=\"type\" "
            "Target=\"target\" "
            "TargetMode=\"External\"" // notice additional attribute
        "/>"
    );

    epc::Relationship rel_obj("target", "http://schemas.energistics.org/package/2012/relationships/destinationObject", "id");
    BOOST_CHECK(rel_obj.relMode() == epc::RelMode::eInternal); // since default
    BOOST_CHECK(rel_obj.relType() == epc::RelType::eDestinationObject);
    BOOST_CHECK_EQUAL(rel_obj.getType(), "http://schemas.energistics.org/package/2012/relationships/destinationObject");
    BOOST_CHECK(rel_obj.getType().c_str() == epc::relationTypeAsString(epc::RelType::eDestinationObject).c_str());
}

BOOST_AUTO_TEST_CASE(test_members) {
    const epc::Relationship empty;
    epc::Relationship rel;

    // setting the mode
    rel.setIsExternalTarget();
    BOOST_CHECK(rel.relMode() == epc::RelMode::eExternal);
    BOOST_CHECK_EQUAL(rel.isInternalTarget(), false);

    rel.setIsInternalTarget();
    BOOST_CHECK(rel.relMode() == epc::RelMode::eInternal);
    BOOST_CHECK_EQUAL(rel.isInternalTarget(), true);

    rel.setMode(epc::RelMode::eExternal);
    BOOST_CHECK(rel.relMode() == epc::RelMode::eExternal);
    BOOST_CHECK_EQUAL(rel.isInternalTarget(), false);

    // compare to rel (differs in mode only)
    BOOST_CHECK(empty != rel);
    BOOST_CHECK(!(empty == rel));
    BOOST_CHECK(empty < rel);
    BOOST_CHECK(!(rel < empty));

    rel.setMode(epc::RelMode::eInternal);
    BOOST_CHECK(rel.relMode() == epc::RelMode::eInternal);
    BOOST_CHECK_EQUAL(rel.isInternalTarget(), true);

    // compare to rel again (mode back t default, so no diffs)
    BOOST_CHECK(empty == rel);
    BOOST_CHECK(!(empty != rel));
    BOOST_CHECK(!(empty < rel));

    // setting the type to built-in well-known one
    BOOST_CHECK(rel.relType() == epc::RelType::eUnknownType);
    rel.setType(epc::RelType::eDestinationObject);
    BOOST_CHECK(rel.relType() == epc::RelType::eDestinationObject);
    const auto& type = rel.getType();
    BOOST_CHECK_EQUAL(type.find("/destinationObject"), size_t(57));
    BOOST_CHECK_EQUAL(type, epc::relationTypeAsString(epc::RelType::eDestinationObject));
    // shares the same string pointer, i.e. the same memory
    BOOST_CHECK(type.c_str() == epc::relationTypeAsString(epc::RelType::eDestinationObject).c_str());

    // setting the type to arbitrary / custom one
    rel.setType("My custom type");
    BOOST_CHECK(rel.relType() == epc::RelType::eUnknownType);
    const auto& type2 = rel.getType();
    BOOST_CHECK(type2.c_str() != epc::relationTypeAsString(epc::RelType::eUnknownType).c_str());
    BOOST_CHECK_EQUAL(type2, "My custom type");

    rel.setType("My custom type2", 15);
    BOOST_CHECK(rel.relType() == epc::RelType::eUnknownType);
    const auto& type3 = rel.getType();
    BOOST_CHECK(type3.c_str() != epc::relationTypeAsString(epc::RelType::eUnknownType).c_str());
    BOOST_CHECK_EQUAL(type3, "My custom type2");

    // setting the target
    const char target_str[] = "obj_WellboreTrajectoryRepresentation_46509486-87c4-47bd-876b-f500fbecf01c";
    std::string target(target_str);
    const char*const c_target = target.c_str();
    rel.setTarget(std::move(target));
    const std::string& target_out = rel.getTarget(); // cppcheck-suppress mismatchingContainers
    BOOST_CHECK_EQUAL(target_out, target_str);
    BOOST_CHECK(target_out.c_str() != target_str);
    BOOST_CHECK(target_out.c_str() == c_target); // the memory block was moved around correctly

    // setting the Id
    rel.setId("foo bar");
    BOOST_CHECK_EQUAL(rel.getId(), "foo bar");

    BOOST_CHECK_EQUAL(
        rel.toString(),
        "<Relationship "
            "Id=\"foo bar\" "
            "Type=\"My custom type2\" "
            "Target=\"obj_WellboreTrajectoryRepresentation_46509486-87c4-47bd-876b-f500fbecf01c\""
        "/>"
    );
    std::ostringstream oss;
    rel.print(oss);
    BOOST_CHECK_EQUAL(oss.str(), rel.toString());

    const epc::Relationship copy(rel);
    BOOST_CHECK(rel != empty);
    BOOST_CHECK(empty != rel);
    BOOST_CHECK(rel == copy);
    BOOST_CHECK(copy == rel);

    epc::Relationship copy2(copy);
    BOOST_CHECK(copy == copy2);
    const char*const copy2_target = copy2.getTarget().c_str(); // cppcheck-suppress mismatchingContainers

    epc::Relationship moved(std::move(copy2));
    // cppcheck-suppress accessMoved
    BOOST_CHECK(copy != copy2); // since copy2 in moved-from state
    BOOST_CHECK(copy == moved); // since copy2 in moved-from state
    const char*const moved_target = moved.getTarget().c_str();
    BOOST_CHECK(copy2_target == moved_target); // string-buf moved around (since > than SBO)

    // Ignore or not Id
    BOOST_CHECK(rel == moved);
    BOOST_CHECK(rel.equals(moved));

    rel.setId("another id");
    BOOST_CHECK(rel != moved);
    BOOST_CHECK(!rel.equals(moved));
    BOOST_CHECK(rel.equals(moved, true)); // same, if we ignore the Id

    // We have two rels, which are identical except for their Ids
    // Using "normal" (full) equality, both are kept
    {
        std::vector<epc::Relationship> rels{ rel, empty, moved };
        std::sort(rels.begin(), rels.end());
        rels.erase(std::unique(rels.begin(), rels.end()), rels.end());
        BOOST_CHECK_EQUAL(size_t(3), rels.size());
    }
    // But when using op< and op== that ignore Ids, one is removed
    {
        std::vector<epc::Relationship> rels{ rel, empty, moved };
        epc::RelEqualsIgnoreId relEquals;
        epc::RelLessThanIgnoreId relLessThan;
        std::sort(rels.begin(), rels.end(), relLessThan);
        rels.erase(std::unique(rels.begin(), rels.end(), relEquals), rels.end());
        BOOST_REQUIRE_EQUAL(size_t(2), rels.size()); // got rid of one of them
        BOOST_CHECK(rels[0] == empty);
        BOOST_CHECK(rels[1].equals(rel, true));
        BOOST_CHECK(rels[1].equals(moved, true));
        BOOST_CHECK((rels[1] != rel) || (rels[1] != moved));
    }
}

// for code-coverage mostly
BOOST_AUTO_TEST_CASE(test_set_type_helpers) {
    epc::Relationship rel;

    rel.setDestinationObjectType();
    BOOST_CHECK(rel.relType() == epc::RelType::eDestinationObject);

    rel.setSourceObjectType();
    BOOST_CHECK(rel.relType() == epc::RelType::eSourceObject);

    rel.setDestinationMediaType();
    BOOST_CHECK(rel.relType() == epc::RelType::eDestinationMedia);

    rel.setSourceMediaType();
    BOOST_CHECK(rel.relType() == epc::RelType::eSourceMedia);

    rel.setChunkedPartType();
    BOOST_CHECK(rel.relType() == epc::RelType::eChunkedPart);

    rel.setExternalPartProxyToMlType();
    BOOST_CHECK(rel.relType() == epc::RelType::eExternalPartProxyToMl);

    rel.setMlToExternalPartProxyType();
    BOOST_CHECK(rel.relType() == epc::RelType::eMlToExternalPartProxy);

    rel.setExternalResourceType();
    BOOST_CHECK(rel.relType() == epc::RelType::eExternalResource);
}

BOOST_AUTO_TEST_CASE(test_rel_mode_round_trip) {
    for (const std::string& type_str : epc::allBuiltinRelationTypes()) {
        if (!type_str.empty()) {
            BOOST_CHECK_EQUAL(size_t(0), type_str.find("http://schemas.energistics.org/package/2012/relationships/"));
        }

        epc::RelType type = epc::relationTypeFromString(type_str);
        const std::string& type_str_out = epc::relationTypeAsString(type);
        BOOST_CHECK_EQUAL(type_str, type_str_out);
    }

    epc::RelType type = epc::relationTypeFromString("foo bar");
    BOOST_CHECK(type == epc::RelType::eUnknownType);

    // case-sensitive, notice D instead of d: DestinationObject
    epc::RelType type2 = epc::relationTypeFromString("http://schemas.energistics.org/package/2012/relationships/DestinationObject");
    BOOST_CHECK(type2 == epc::RelType::eUnknownType);
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
