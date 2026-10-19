// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/common/etp12/EmlUri.h>
#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::common::etp12;

using oes::core::Guid;

namespace {

EmlUri regexParse(const std::string& uri);
EmlUri regexParseFull(const std::string& uri);

BOOST_AUTO_TEST_SUITE(EmlUriTests)

BOOST_AUTO_TEST_CASE(test_empty) {
    EmlUri uri;
    BOOST_CHECK_EQUAL(bool(uri), false);

    EmlUri uri2;
    BOOST_CHECK_EQUAL(uri, uri2);
    BOOST_CHECK(!(uri < uri2));
    BOOST_CHECK(!(uri2 < uri));

    EmlUri space = EmlUri::make_space("foo");
    BOOST_CHECK_EQUAL(bool(space), true);

    BOOST_CHECK(uri != space);
    BOOST_CHECK(!(uri == space));
    BOOST_CHECK(uri < space); // empty before non-empty
}

BOOST_AUTO_TEST_CASE(test_parse_legacy) {
    EmlUri::ScopedLegacyFlags legacy;

    const Guid guid = Guid::make();
    const std::string uxml = guid.toXmlString();
    const std::string unod = guid.toStringNoDash();

    struct {
        std::string uri_;
        EmlUri expected_;
    } testcases[] {
        // eDATA_SPACE (3x DS paths)
        { "eml:///", EmlUri::make_space() },
        { "eml:///dataspace(foo)", EmlUri::make_space("foo") },
        { "eml:///dataspace(foo/bar)", EmlUri::make_space("foo/bar") },
        { "eml:///dataspace(foo/bar/baz)", EmlUri::make_space("foo/bar/baz") },
        { "eml:///dataspace(foo%20bar/baz)", EmlUri::make_space("foo bar/baz") },

        { "eml:///dataspace(Project(1))", EmlUri::make_space("Project(1)") },
        { "eml:///dataspace(Project(2))", EmlUri::make_space("Project(2)") },
        { "eml:///dataspace(Dataspace(2)/Project(1))", EmlUri::make_space("Dataspace(2)/Project(1)") },
        { "eml:///dataspace(Dataspace(3)/Project(2))", EmlUri::make_space("Dataspace(3)/Project(2)") },
        { "eml:///dataspace(App-User(1)/Dataspace(2)/Project(1))", EmlUri::make_space("App-User(1)/Dataspace(2)/Project(1)") },
        { "eml:///dataspace(App-User(2)/Dataspace(3)/Project(2))", EmlUri::make_space("App-User(2)/Dataspace(3)/Project(2)") },

        // eOBJ_TYPE (3x DS paths X 2x ML types)
        { "eml:///eml20.feat",    EmlUri::make_type("", "feat") },
        { "eml:///resqml20.feat", EmlUri::make_type("", "feat", EmlUri::eRESQML20) },
        { "eml:///resqml20.feat(", EmlUri() }, // extra opening-paren

        { "eml:///dataspace(foo)/eml20.feat",    EmlUri::make_type("foo", "feat") },
        { "eml:///dataspace(foo)/resqml20.feat", EmlUri::make_type("foo", "feat", EmlUri::eRESQML20) },
        { "eml:///dataspace(foo)/resqml20.feat(", EmlUri() }, // extra opening-paren

        { "eml:///dataspace(foo/bar)/eml20.feat",    EmlUri::make_type("foo/bar", "feat") },
        { "eml:///dataspace(foo/bar)/resqml20.feat", EmlUri::make_type("foo/bar", "feat", EmlUri::eRESQML20) },
        { "eml:///dataspace(foo/bar)/resqml20.feat(", EmlUri() }, // extra opening-paren

        // eOBJ_INSTANCE (3x DS paths X 2x ML types X 2x GUID formats)
        { "eml:///eml20.feat(" + uxml + ")",    EmlUri::make_obj("", "feat", guid) },
        { "eml:///eml20.feat(" + unod + ")",    EmlUri() }, // not accepted at this point (non-standard)
        { "eml:///resqml20.feat(" + uxml + ")", EmlUri::make_obj("", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///resqml20.feat(" + unod + ")", EmlUri() },
        { "eml:///resqml20.feat(" + uxml, EmlUri() }, // missing closing-parent

        { "eml:///dataspace(foo)/eml20.feat(" + uxml + ")",    EmlUri::make_obj("foo", "feat", guid) },
        { "eml:///dataspace(foo)/eml20.feat(" + unod + ")",    EmlUri() },
        { "eml:///dataspace(foo)/resqml20.feat(" + uxml + ")", EmlUri::make_obj("foo", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///dataspace(foo)/resqml20.feat(" + unod + ")", EmlUri() },
        { "eml:///dataspace(foo)/resqml20.feat(" + uxml, EmlUri() }, // missing closing-parent

        { "eml:///dataspace(foo/bar)/eml20.feat(" + uxml + ")",    EmlUri::make_obj("foo/bar", "feat", guid) },
        { "eml:///dataspace(foo/bar)/eml20.feat(" + unod + ")",    EmlUri() },
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + uxml + ")", EmlUri::make_obj("foo/bar", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + unod + ")", EmlUri() },
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + uxml + ",version1)", EmlUri::make_obj("foo/bar", "feat", guid, EmlUri::eRESQML20, "version1") },
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + uxml + ",version1", EmlUri() }, // missing closing-parent
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + uxml + "),version1", EmlUri() },
        { "eml:///dataspace(foo/bar)/resqml20.feat(" + unod + ",version1)", EmlUri() },

        // should not have uuid= since has no version
        { "eml:///dataspace(foo/bar)/eml20.feat(uuid=" + uxml + ")", EmlUri() },

        // should not have uuid= and version=, since using legacy (unquoted) space name
        { "eml:///dataspace(foo/bar)/eml20.feat(uuid=" + uxml + ",version='1')", EmlUri() },
    };
    for (const auto& tc : testcases) {
        EmlUri uri = EmlUri::parse(tc.uri_);
        BOOST_CHECK_EQUAL(uri, tc.expected_);

        if (tc.expected_.isValid()) {
            std::string str = uri.to_string();
            BOOST_CHECK_EQUAL(str, tc.uri_);

            const bool unchanged_uri =
                (uri.isRoot()) ||
                (uri.isType() && uri.defaultDataspace()) ||
                (uri.isObj()  && uri.defaultDataspace())
            ;

            {
                // Try re-parsing that legacy URI, with legacy-parse OFF
                // Should fail, except for a few URIs which are the same in OLD and NEW format.
                EmlUri::ScopedLegacyFlags legacy2(false, false);
                EmlUri uri2 = EmlUri::parse(tc.uri_);
                if (unchanged_uri) {
                    BOOST_CHECK_EQUAL(uri2, tc.expected_); // same as before
                } else {
                    BOOST_CHECK(!uri2.isValid());
                }
            }

            {
                // Check that when we turn OFF legacy-print, i.e. get
                // the official URI string, it differs from the original.
                EmlUri::ScopedLegacyFlags legacy2(true, false);
                std::string str2 = uri.to_string();
                if (unchanged_uri) {
                    // Only case where URI is the same
                    BOOST_CHECK_EQUAL(str2, tc.uri_);
                } else {
                    BOOST_CHECK_NE(str2, tc.uri_);
                }

                // And that when parsing that non-legacy (i.e. official) URI,
                // with legacy-parsing ON, we can still parse it OK. I.e.
                // parsing, when legacy is enabled, accepts BOTH forms.
                EmlUri uri2 = EmlUri::parse(str2);
                BOOST_CHECK_EQUAL(uri2, tc.expected_);
            }

            // exercize to_type(), and check it corresponds to the
            // type-part of to_string()
            if (tc.expected_.isType()) {
                EmlUri space = EmlUri::make_space(uri.dataspace_);
                std::string type;
                if (space.isRoot()) {
                    type = space.to_string() + uri.to_type();
                } else {
                    type = space.to_string() + '/' + uri.to_type();
                }
                EmlUri type_uri = EmlUri::parse(type);
                BOOST_CHECK_EQUAL(uri, type_uri);
            } else if (tc.expected_.isObj()) {
                EmlUri space = EmlUri::make_space(uri.dataspace_);
                std::string obj;
                if (space.isRoot()) {
                    obj = space.to_string() + uri.to_type();
                } else {
                    obj = space.to_string() + '/' + uri.to_type();
                }
                if (!uri.version_.empty()) {
                    obj += '(' + uri.guid_.toXmlString() + ',' + uri.version_ + ')';
                } else {
                    obj += '(' + uri.guid_.toXmlString() + ')';
                }
                EmlUri obj_uri = EmlUri::parse(obj);
                BOOST_CHECK_EQUAL(uri, obj_uri);
            } else if (uri.isSpace()) {
                try {
                    uri.to_type();
                    BOOST_FAIL("Expected std::runtime_error");
                } catch (const std::runtime_error& ex) {
                    std::string msg = ex.what();
                    BOOST_CHECK(msg.find("Expected ") == 0);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_parse_official) {
    EmlUri::ScopedLegacyFlags legacy(false, false);

    const Guid guid = Guid::make();
    const std::string uxml = guid.toXmlString();
    const std::string unod = guid.toStringNoDash();

    struct {
        std::string uri_;
        EmlUri expected_;
    } testcases[] {
        // eDATA_SPACE (3x DS paths)
        { "eml:///", EmlUri::make_space() },
        { "eml:///dataspace('foo')", EmlUri::make_space("foo") },
        { "eml:///dataspace('foo/bar')", EmlUri::make_space("foo/bar") },
        { "eml:///dataspace('foo/bar/baz')", EmlUri::make_space("foo/bar/baz") },
        { "eml:///dataspace('foo%20bar/baz')", EmlUri::make_space("foo bar/baz") },

        { "eml:///dataspace('Project(1)')", EmlUri::make_space("Project(1)") },
        { "eml:///dataspace('Project(2)')", EmlUri::make_space("Project(2)") },
        { "eml:///dataspace('Dataspace(2)/Project(1)')", EmlUri::make_space("Dataspace(2)/Project(1)") },
        { "eml:///dataspace('Dataspace(3)/Project(2)')", EmlUri::make_space("Dataspace(3)/Project(2)") },
        { "eml:///dataspace('App-User(1)/Dataspace(2)/Project(1)')", EmlUri::make_space("App-User(1)/Dataspace(2)/Project(1)") },
        { "eml:///dataspace('App-User(2)/Dataspace(3)/Project(2)')", EmlUri::make_space("App-User(2)/Dataspace(3)/Project(2)") },

        // NEW style URIs support unbalanced parens, since rely on closing single-quote
        { "eml:///dataspace('App-User2/Dataspace(3/Project(2')", EmlUri::make_space("App-User2/Dataspace(3/Project(2") },
        { "eml:///dataspace('App-User(2/Dataspace(3/Project(2')", EmlUri::make_space("App-User(2/Dataspace(3/Project(2") },
        { "eml:///dataspace('App-User(2/Dataspace(3)/Project(2')", EmlUri::make_space("App-User(2/Dataspace(3)/Project(2") },
        { "eml:///dataspace('App-User(2)/Dataspace(3)/Project(2')", EmlUri::make_space("App-User(2)/Dataspace(3)/Project(2") },
        { "eml:///dataspace('App-User(2)/Dataspace(3)/Project2)')", EmlUri::make_space("App-User(2)/Dataspace(3)/Project2)") },
        { "eml:///dataspace('App-User(2)/Dataspace3)/Project2)')", EmlUri::make_space("App-User(2)/Dataspace3)/Project2)") },
        { "eml:///dataspace('App-User2)/Dataspace3)/Project2)')", EmlUri::make_space("App-User2)/Dataspace3)/Project2)") },
        { "eml:///dataspace('App-User2)/Dataspace3)/Project2')", EmlUri::make_space("App-User2)/Dataspace3)/Project2") },
        { "eml:///dataspace('App-User2)/Dataspace3/Project2')", EmlUri::make_space("App-User2)/Dataspace3/Project2") },
        { "eml:///dataspace('App-User2/Dataspace3/Project2')", EmlUri::make_space("App-User2/Dataspace3/Project2") },

        // eOBJ_TYPE (3x DS paths X 2x ML types)
        { "eml:///eml20.feat",    EmlUri::make_type("", "feat") },
        { "eml:///resqml20.feat", EmlUri::make_type("", "feat", EmlUri::eRESQML20) },

        { "eml:///dataspace('foo')/eml20.feat",    EmlUri::make_type("foo", "feat") },
        { "eml:///dataspace('foo')/resqml20.feat", EmlUri::make_type("foo", "feat", EmlUri::eRESQML20) },

        { "eml:///dataspace('foo/bar')/eml20.feat",    EmlUri::make_type("foo/bar", "feat") },
        { "eml:///dataspace('foo/bar')/resqml20.feat", EmlUri::make_type("foo/bar", "feat", EmlUri::eRESQML20) },

        // eOBJ_INSTANCE (3x DS paths X 2x ML types X 2x GUID formats)
        { "eml:///eml20.feat(" + uxml + ")",    EmlUri::make_obj("", "feat", guid) },
        { "eml:///eml20.feat(" + unod + ")",    EmlUri() }, // not accepted at this point (non-standard)
        { "eml:///resqml20.feat(" + uxml + ")", EmlUri::make_obj("", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///resqml20.feat(" + unod + ")", EmlUri() },

        { "eml:///dataspace('foo')/eml20.feat(" + uxml + ")",    EmlUri::make_obj("foo", "feat", guid) },
        { "eml:///dataspace('foo')/eml20.feat(" + unod + ")",    EmlUri() },
        { "eml:///dataspace('foo')/resqml20.feat(" + uxml + ")", EmlUri::make_obj("foo", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///dataspace('foo')/resqml20.feat(" + unod + ")", EmlUri() },

        { "eml:///dataspace('foo/bar')/eml20.feat(" + uxml + ")",    EmlUri::make_obj("foo/bar", "feat", guid) },
        { "eml:///dataspace('foo/bar')/eml20.feat(" + unod + ")",    EmlUri() },
        { "eml:///dataspace('foo/bar')/resqml20.feat(" + uxml + ")", EmlUri::make_obj("foo/bar", "feat", guid, EmlUri::eRESQML20) },
        { "eml:///dataspace('foo/bar')/resqml20.feat(" + unod + ")", EmlUri() },
        { "eml:///dataspace('foo/bar')/resqml20.feat(uuid=" + uxml + ",version='1')", EmlUri::make_obj("foo/bar", "feat", guid, EmlUri::eRESQML20, "1") },
        { "eml:///dataspace('foo/bar')/resqml20.feat(uuid=" + uxml + ",version='1'", EmlUri() }, // missing closing paren
        { "eml:///dataspace('foo/bar')/resqml20.feat(uuid=" + uxml + "),version=1)", EmlUri() }, // missing version squotes
        { "eml:///dataspace('foo/bar')/resqml20.feat(uuid=" + unod + ",version='1')", EmlUri() }, // invalid UUID format
    };
    for (const auto& tc : testcases) {
        EmlUri uri = EmlUri::parse(tc.uri_);
        BOOST_CHECK_EQUAL(uri, tc.expected_);

        if (tc.expected_.isValid()) {
            std::string str = uri.to_string();
            BOOST_CHECK_EQUAL(str, tc.uri_);

            // exercize to_type(), and check it corresponds to the
            // type-part of to_string()
            if (tc.expected_.isType()) {
                EmlUri space = EmlUri::make_space(uri.dataspace_);
                std::string type;
                if (space.isRoot()) {
                    type = space.to_string() + uri.to_type();
                } else {
                    type = space.to_string() + '/' + uri.to_type();
                }
                EmlUri type_uri = EmlUri::parse(type);
                BOOST_CHECK_EQUAL(uri, type_uri);
            } else if (tc.expected_.isObj()) {
                EmlUri space = EmlUri::make_space(uri.dataspace_);
                std::string obj;
                if (space.isRoot()) {
                    obj = space.to_string() + uri.to_type();
                } else {
                    obj = space.to_string() + '/' + uri.to_type();
                }
                if (!uri.version_.empty()) {
                    obj += "(uuid=" + uri.guid_.toXmlString() + ",version='" + uri.version_ + "')";
                } else {
                    obj += '(' + uri.guid_.toXmlString() + ')';
                }
                EmlUri obj_uri = EmlUri::parse(obj);
                BOOST_CHECK_EQUAL(uri, obj_uri);
            } else if (uri.isSpace()) {
                try {
                    uri.to_type();
                    BOOST_FAIL("Expected std::runtime_error");
                } catch (const std::runtime_error& ex) {
                    std::string msg = ex.what();
                    BOOST_CHECK(msg.find("Expected ") == 0);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_parse_prodml23) {
    EmlUri::ScopedLegacyFlags legacy(false, false);
    const Guid guid = Guid::make();
    const std::string uri = "eml:///prodml23.TimeSeriesData(" + guid.toXmlString() + ")";

    const EmlUri parsed = EmlUri::parse(uri);
    BOOST_CHECK_EQUAL(parsed, EmlUri::make_obj("", "TimeSeriesData", guid, EmlUri::ePRODML23));
    BOOST_CHECK_EQUAL(parsed.to_string(), uri);
}

BOOST_AUTO_TEST_CASE(test_dataspace) {
    EmlUri foo = EmlUri::make_space("foo");
    EmlUri bar = EmlUri::make_space("bar");
    EmlUri foo2 = EmlUri::make_space("foo");
    EmlUri empty = EmlUri::make_space("");
    EmlUri composite2 = EmlUri::make_space("foo/bar");
    EmlUri composite3 = EmlUri::make_space("foo/bar/baz");

    BOOST_CHECK_EQUAL(foo.type_, EmlUri::eDATA_SPACE);
    BOOST_CHECK_EQUAL(foo.ml_, EmlUri::eNO_ML);
    BOOST_CHECK_EQUAL(bar.ml_, EmlUri::eNO_ML);

    BOOST_CHECK_EQUAL(foo, foo2);
    BOOST_CHECK(foo != bar);
    BOOST_CHECK(!(foo == bar));
    BOOST_CHECK(bar < foo);

    BOOST_CHECK_EQUAL(size_t(0), empty.dataspacePartCount());
    BOOST_CHECK_EQUAL(size_t(1), foo.dataspacePartCount());
    BOOST_CHECK_EQUAL(size_t(1), bar.dataspacePartCount());
    BOOST_CHECK_EQUAL(size_t(1), foo2.dataspacePartCount());
    BOOST_CHECK_EQUAL(size_t(2), composite2.dataspacePartCount());
    BOOST_CHECK_EQUAL(size_t(3), composite3.dataspacePartCount());

    BOOST_CHECK_EQUAL("", empty.dataspaceName());
    BOOST_CHECK_EQUAL("foo", foo.dataspaceName());
    BOOST_CHECK_EQUAL("bar", bar.dataspaceName());
    BOOST_CHECK_EQUAL("foo", foo2.dataspaceName());
    BOOST_CHECK_EQUAL("bar", composite2.dataspaceName());
    BOOST_CHECK_EQUAL("baz", composite3.dataspaceName());

    BOOST_CHECK_EQUAL("", empty.dataspaceFolder());
    BOOST_CHECK_EQUAL("", foo.dataspaceFolder());
    BOOST_CHECK_EQUAL("", bar.dataspaceFolder());
    BOOST_CHECK_EQUAL("", foo2.dataspaceFolder());
    BOOST_CHECK_EQUAL("foo", composite2.dataspaceFolder());
    BOOST_CHECK_EQUAL("foo/bar", composite3.dataspaceFolder());
}

BOOST_AUTO_TEST_CASE(test_obj_instance) {
    const Guid guid = Guid::parse("2a114208-a4b7-4c02-bb4d-3e15c68d1674");
    const EmlUri obj = EmlUri::make_obj("foo", "obj_bar", guid);
    const std::string obj_uri = obj.to_string();
    const std::string exp_uri = "eml:///dataspace('foo')/eml20.obj_bar(2a114208-a4b7-4c02-bb4d-3e15c68d1674)";
    BOOST_CHECK_EQUAL(obj_uri, exp_uri);

    const EmlUri obj2 = EmlUri::parse(obj_uri);
    BOOST_CHECK((bool)obj2);

    BOOST_CHECK_EQUAL(obj2.dataspace_, "foo");
    BOOST_CHECK_EQUAL(obj2.dataspace_, obj.dataspace_);

    BOOST_CHECK_EQUAL(obj2.ml_, EmlUri::eEML20);
    BOOST_CHECK_EQUAL(obj2.ml_, obj.ml_);

    BOOST_CHECK_EQUAL(obj2.obj_type_, "obj_bar");
    BOOST_CHECK_EQUAL(obj2.obj_type_, obj.obj_type_);

    BOOST_CHECK_EQUAL(obj2.guid_, guid);
    BOOST_CHECK_EQUAL(obj2.guid_, obj.guid_);

    BOOST_REQUIRE_EQUAL(obj2, obj); // same as above basically

    const std::string obj2_uri = obj2.to_string();
    BOOST_CHECK_EQUAL(obj2_uri, obj_uri);
    BOOST_CHECK_EQUAL(obj2_uri, exp_uri);

    // Same as EmlUri::parse(obj_uri), but for code-coverage
    const EmlUri obj3 = EmlUri::parse(obj_uri.c_str(), obj_uri.size());
    BOOST_CHECK_EQUAL(obj2, obj3);
    BOOST_CHECK_EQUAL(obj2.to_string(), obj3.to_string());
}

BOOST_AUTO_TEST_CASE(test_percent_encoding) {
    struct {
        const char* raw_;
        const char* enc_; // encoded. null means as-is, i.e. same as raw_
    } testcases[] {
        { "", nullptr },
        { "a", nullptr },
        { "A b", "A%20b" }, // spaces are encoded
        { "Ab", nullptr },
        { "0az", nullptr },
        { "%", "%25" },
        { "a/b", nullptr }, // Not "a%2fb" Normal? Part of reserved chars
        // Reserved characters. See https://en.wikipedia.org/wiki/Percent-encoding
        { "!#$&()*+,/:;=?@", nullptr },
        { "'[]", "%27%5b%5d" }, // FIXME: Why are those not encoded???
    };
    for (const auto& tc : testcases) {
        std::ostringstream oss;
        EmlUri::percentEncode(oss, tc.raw_);
        std::string actual = oss.str();

        std::string expected(tc.enc_? tc.enc_: tc.raw_);
        BOOST_CHECK_EQUAL(actual, expected);

        std::ostringstream oss2;
        EmlUri::percentDecode(oss2, actual);
        std::string decoded = oss2.str();
        BOOST_CHECK_EQUAL(decoded, tc.raw_);
    }
}

BOOST_AUTO_TEST_CASE(test_move_swap) {
    EmlUri ds1 = EmlUri::make_space("Long Enough to overflow SSO, really");
    const char* ds1_str = ds1.dataspace_.c_str();
    const std::string& ds1_uri = ds1.to_string();

    EmlUri ds2 = EmlUri::make_space("Short");
    const char* ds2_str = ds2.dataspace_.c_str();
    const std::string& ds2_uri = ds2.to_string();

    std::swap(ds1, ds2);

    const void* ds1_str_after = ds1.dataspace_.c_str();
    const std::string& ds1_uri_after = ds1.to_string();

    const void* ds2_str_after = ds2.dataspace_.c_str();
    const std::string& ds2_uri_after = ds2.to_string();

    BOOST_CHECK_EQUAL(ds1_uri, ds2_uri_after);
    BOOST_CHECK_EQUAL(ds2_uri, ds1_uri_after);

    // By pointer comparisons. Depends
    BOOST_CHECK_EQUAL(ds1_str, ds2_str_after); // buffer moved
#ifdef _WIN32
    /*
    ** Weird, fails on Linux. No SSO in GCC6's std::string???
    */
    BOOST_CHECK_NE(ds2_str, ds1_str_after); // copied / SSO
#else
    OES_UNUSED(ds2_str);
    OES_UNUSED(ds1_str_after);
#endif
}

BOOST_AUTO_TEST_CASE(test_child_creation) {
    const Guid uid = Guid::make();
    const EmlUri::ML ml = EmlUri::eRESQML20; // not the default

    EmlUri none;
    EmlUri root  = EmlUri::make_space();
    EmlUri space = EmlUri::make_space("foo");
    EmlUri type  = EmlUri::make_type("foo", "obj_Foo", ml);
    EmlUri obj   = EmlUri::make_obj("foo", "obj_Foo", uid, ml);

    BOOST_CHECK_EQUAL(space, root.child_space("foo"));
    BOOST_CHECK_EQUAL(type, space.child_type("obj_Foo",ml));
    BOOST_CHECK_EQUAL(obj, type.child_obj(uid));

    BOOST_CHECK_EQUAL(
        root.child_space("foo").child_type("obj_Foo",ml).child_obj(uid),
        obj
    );

    // for branch-coverage
    BOOST_CHECK_EQUAL(root.child_space("foo/bar"), space.child_space("bar"));

    BOOST_CHECK_THROW(none.child_space("foo"), std::runtime_error);
    BOOST_CHECK_THROW(obj.child_type("obj_Foo",ml), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(bench_parse_obj_instance) {
    EmlUri::ScopedLegacyFlags legacy; // RegExp require legacy URIs

    const int count = oes::testing::perfValues(100, 1000*1000);

    std::vector<std::string> uris;
    uris.reserve(count);
    for (int i = 0; i < count; ++i) {
        const EmlUri obj = EmlUri::make_obj("foo", "obj_bar", Guid::make(), EmlUri::eEML20, "v1");
        uris.emplace_back(obj.to_string());
    }

    std::vector<EmlUri> hand_emls;
    hand_emls.reserve(count);
    {
        oes::testing::ScopedTimer timer("Custom-parsing URIs");
        for (const std::string& uri : uris) {
            hand_emls.emplace_back(EmlUri::parse(uri));
        }
    }

    std::vector<EmlUri> regex_emls;
    regex_emls.reserve(count);
    {
        oes::testing::ScopedTimer timer("RegExp-parsing URIs");
        for (const std::string& uri : uris) {
            regex_emls.emplace_back(regexParse(uri));
        }
    }

    int errors = 0;
    for (int i = 0; i < count; ++i) {
        if (hand_emls[i] != regex_emls[i]) {
            ++errors;
        }
    }
    BOOST_REQUIRE_EQUAL(errors, 0);
}

BOOST_AUTO_TEST_CASE(bench_parse_different_uris) {
    EmlUri::ScopedLegacyFlags legacy; // RegExp require legacy URIs

    const int count = oes::testing::perfValues(1, 10);

    struct {
        int multiplier_;
        std::string uri_;
    } uri_templates[] {
        { 1, "eml:///" }, // 1 root
        { 1, "eml:/" }, // 1 root alternative
        { 10, "eml:///dataspace('foo')" }, // 10 projects
        { 10, "eml:/dataspace()" }, // 10 projects alternative
        /*
        ** Note sure who handles that on the RegExp-side
        { 1*10, "eml://foo/eml20" }, // 1 per-project, so x10
        */
        { 1*10*100, "eml:///dataspace('foo')/eml20.feat" }, // ~ 100 types in DX/RESQML
        { 1*10*100*100, "eml:///dataspace('foo')/eml20.feat(" + Guid::make().toXmlString() + ")" }, // ~ 100 instances per-type
        { 1*10*100*100, "eml:/dataspace('foo')/eml20.feat(" + Guid::make().toXmlString() + ")" }, // ~ 100 instances per-type
    };

    int uri_count = 0;
    for (auto& entry : uri_templates) {
        if ((count == 1) && (entry.multiplier_ > 100)) {
            // in regular "test-mode", avoid too many URIs
            entry.multiplier_ = 100;
        }
        uri_count += (count * entry.multiplier_);
    }

    std::vector<std::string> uris;
    uris.reserve(uri_count);
    for (const auto& entry : uri_templates) {
        for (int i = 0; i < (count * entry.multiplier_); ++i) {
            const EmlUri obj = EmlUri::parse(entry.uri_);
            uris.emplace_back(obj.to_string());
        }
    }

    std::vector<EmlUri> hand_emls;
    hand_emls.reserve(uri_count);
    {
        oes::testing::ScopedTimer timer("Custom-parsing URIs");
        for (const std::string& uri : uris) {
            hand_emls.emplace_back(EmlUri::parse(uri));
        }
    }

    std::vector<EmlUri> regex_emls;
    regex_emls.reserve(uri_count);
    {
        oes::testing::ScopedTimer timer("RegExp-parsing URIs");
        for (const std::string& uri : uris) {
            regex_emls.emplace_back(regexParseFull(uri));
        }
    }

    int errors = 0;
    for (int i = 0; i < uri_count; ++i) {
        if (hand_emls[i] != regex_emls[i]) {
            ++errors;
        }
    }
    BOOST_REQUIRE_EQUAL(errors, 0);
}

BOOST_AUTO_TEST_SUITE_END()

// From DiscoveryHandler, in etp/DiscoveryHandler.hpp
static boost::regex object_regex() {
    static boost::regex regex_(
        "^eml:(?://)?(?:/dataspace\\((?<dataspace>.*?)?\\))?(?:/(?<family>resqml|eml)(?<release>\\d{2})\\.(?<objectType>[^\\-]+)" //Path to object
        "(?:\\((?<uuid>[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})(?:\\,(?<version>\\w+))?\\))?)$"
    );
    return regex_;
}

// From DataSpaceDiscoveryHandler, in DataServer.cpp
static boost::regex dataspace_regex() {
    static boost::regex regex_(
        "^eml:(?://)?(?:/dataspace\\((?<dataspace>[^)]*)?\\))?$"
    );
    return regex_;
}

// From DxInterfaceTypeHandler, in DataServer.cpp
static boost::regex objtype_regex() {
    /*
    ** replacing non-capturing groups into capturing ones, for compatibility with EmlUri
    static boost::regex regex_("^eml://(.*?)/?(resqml20|eml20)/?([^/]+)?$");
    */
    static boost::regex regex_(
        "^eml:(?://)?(?:/dataspace\\((?<dataspace>.*?)?\\))?/(?<family>resqml|eml)(?<release>\\d{2})\\.(?<objectType>[^\\-]+)$"
    );
    return regex_;
}

#if 0 // unused for now, avoid GCC warning
// From DxDataSetHandler, in DataServer.cpp
static boost::regex extpart_regex() {
    /* ETP 1.0
    static boost::regex regex_("^eml://(.*?)/?resqml20/obj_EpcExternalPartReference\\(([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})\\)" //HDF5 proxy path
        "/(.*?)/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/(.*?)$"); //equivalent of path in hdf5
    */

    // ETP 1.2. Why no "path in hdf5" anymore?
    static boost::regex regex_("^eml://(.*?)/?(resqml20|eml20)/obj_EpcExternalPartReference\\(([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})\\)"); //HDF5 proxy path
        //"/(.*?)/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/(.*?)$" ); //equivalent of path in hdf5

    return regex_;
}
#endif//0

static bool split_dataspace_uri(const std::string& uri, std::string& dataspace) {
    boost::match_results<std::string::const_iterator> what;
    if (!boost::regex_search(uri, what, dataspace_regex())) {
        return false;
    }

    dataspace = std::string(what[1].first, what[1].second);
    return true;
}

static bool split_objtype_uri(const std::string& uri, std::string& dataspace, std::string& protocol, std::string& interfaceType) {
    boost::match_results<std::string::const_iterator> what;
    if (!boost::regex_search(uri, what, objtype_regex())) {
        return false;
    }

    dataspace = std::string(what[1].first, what[1].second);
    protocol = std::string(what[2].first, what[2].second) + std::string(what[3].first, what[3].second);
    interfaceType = std::string(what[4].first, what[4].second);
    return true;
}

/// Split uri into usable components.
static bool split_object_uri(const std::string& uri, std::string& dataspace, std::string& protocol, std::string& interfaceType, Guid& guid, std::string& version) {
    boost::match_results<std::string::const_iterator> what;
    if (!boost::regex_search(uri, what, object_regex())) {
        return false;
    }

    dataspace = std::string(what[1].first, what[1].second);
    protocol = std::string(what[2].first, what[2].second)+ std::string(what[3].first, what[3].second);
    interfaceType = std::string(what[4].first, what[4].second);
    guid = Guid::parse(std::string(what[5].first, what[5].second));
    version = std::string(what[6].first, what[6].second);
    return true;
}

EmlUri regexParseFull(const std::string& uri) {
    /*
    ** We're going to try these regexes in the same order they are registered in.
    **
    DxDataProvider::DxDataProvider() ... {
        register_discovery_handler(discovery_handler_ptr(new DataSpaceDiscoveryHandler(*this)));
        register_discovery_handler(discovery_handler_ptr(new DxInterfaceTypeHandler(*this)));
        register_discovery_handler(discovery_handler_ptr(new DxRelationshipHandler(*this)));
        register_data_type_handler(data_type_handler_ptr(new DxObjectHandler(*this)));
        register_data_array_handler(data_array_handler_ptr(new DxDataSetHandler(*this)));
    }
    */

    EmlUri eml;
    std::string ml;

    // WARNING: Regex supports only "single-part" ETP "data" space. No "folder".


    // Relationship is the same RE as object below. Weird...
    // I suspect a ETP 1.0 to ETP 1.2 porting issue...
    // Or perhaps not, not registered in the same "namespace": discovery vs data_type

    if (split_dataspace_uri(uri, eml.dataspace_)) {
        eml.type_ = EmlUri::eDATA_SPACE;
        return eml;
    }

    if (split_objtype_uri(uri, eml.dataspace_, ml, eml.obj_type_)) {
        eml.type_ = EmlUri::eDATA_SPACE;
        eml.ml_ = EmlUri::parseML(ml.c_str(), ml.size());
        return eml;
    }

    if (split_object_uri(uri, eml.dataspace_, ml, eml.obj_type_, eml.guid_, eml.version_)) {
        eml.type_ = EmlUri::eOBJ_INSTANCE;
        eml.ml_ = EmlUri::parseML(ml.c_str(), ml.size());
        return eml;
    }
    return EmlUri(); // invalid
}

EmlUri regexParse(const std::string& uri) {
    EmlUri eml;
    std::string ml;

    if (split_object_uri(uri, eml.dataspace_, ml, eml.obj_type_, eml.guid_, eml.version_)) {
        eml.type_ = EmlUri::eOBJ_INSTANCE;
        eml.ml_ = EmlUri::parseML(ml.c_str(), ml.size());
        return eml;
    }

    return EmlUri(); // invalid
}

} // End anonymous namespace
