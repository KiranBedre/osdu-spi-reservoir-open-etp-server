// ============================================================================
// Copyright 2014-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_TESTING_CORE_ASSERT_TEST_TOOLS_H
#define OES_TESTING_CORE_ASSERT_TEST_TOOLS_H

#include <oes/testing/core/export.h>
#include <oes/core/utils/std_fwd.h>

#ifndef PGDM_NO_CPP11_LEGACY_SUPPORT
#include <oes/core/utils/Assert.h>
#endif

#include <vector>
#include <functional>


#ifdef PGDM_NO_CPP11_LEGACY_SUPPORT
typedef std::string String;
// Copied from Assert.h
namespace oes {
namespace core {
        
struct OES_TESTING_CORE_EXPORT AssertionInfo {
    enum Type { Unknown, AssertFailed, DebugAssertFailed };
    Type type;
    String message;
    String condition;
    String file;
    String function;
    int line;

    AssertionInfo(
        Type t, const String& m, const String& c, const String& f, const String& fn, int l) :
        type(t), message(m), condition(c), file(f), function(fn), line(l) {
    }
    AssertionInfo():
        type(Unknown), line(-1) {
    }
};

typedef boost::function<bool()> BreakpointHandler;
OES_TESTING_CORE_EXPORT const BreakpointHandler getBreakpointHandler();
OES_TESTING_CORE_EXPORT void setBreakpointHandler(const BreakpointHandler& handler);

typedef boost::function<void(const AssertionInfo&)> AssertionHandler;
OES_TESTING_CORE_EXPORT const AssertionHandler getAssertionHandler();
OES_TESTING_CORE_EXPORT void setAssertionHandler(const AssertionHandler& handler);

namespace detail {
    // To avoid compilation warning(s) when evaluating assertion
    inline bool is_true(bool condition) {
        return condition;
    }

    OES_TESTING_CORE_EXPORT bool isDebuggerAttached();
    OES_TESTING_CORE_EXPORT bool shouldBreak();
    OES_TESTING_CORE_EXPORT void handleAssertionFailure(
        AssertionInfo::Type type,
        const char* condition,
        const char* message,
        const char* file,
        const char* function,
        int line);
} // End namespace detail

} // End namespace core
} // End namespace oes

#endif

/*****************************************************************************/


namespace oes {
namespace testing {

/*****************************************************************************/

#ifdef PGDM_NO_CPP11_LEGACY_SUPPORT
typedef boost::function<
    bool(size_t, const std::string* left_expected, const std::string* right_actual)
> CustomLineComparator;
#else
using CustomLineComparator = std::function<
    bool(size_t, const std::string* left_expected, const std::string* right_actual)
>;
#endif

OES_TESTING_CORE_EXPORT bool compareMultiLineText(
    const std::string& expected,
    const std::string& actual,
    std::string& diff_like_output_msg,
    CustomLineComparator comparator = nullptr
);

#define OES_CHECK_SAME_MULTILINE_TEXT_IF(expected, actual, line_custom_equals_predicate) \
    { \
        std::string msg; \
        BOOST_CHECK_MESSAGE( \
            ::oes::testing::compareMultiLineText( \
                expected, actual, msg, line_custom_equals_predicate\
            ), msg \
        ); \
    }

#define OES_CHECK_SAME_MULTILINE_TEXT(expected, actual) \
    OES_CHECK_SAME_MULTILINE_TEXT_IF(expected, actual, nullptr)

#define OES_REQUIRE_SAME_MULTILINE_TEXT_IF(expected, actual, line_custom_equals_predicate) \
    { \
        std::string msg; \
        BOOST_REQUIRE_MESSAGE( \
            ::oes::testing::compareMultiLineText( \
                expected, actual, msg, line_custom_equals_predicate\
            ), msg \
        ); \
    }

#define OES_REQUIRE_SAME_MULTILINE_TEXT(expected, actual) \
    OES_REQUIRE_SAME_MULTILINE_TEXT_IF(expected, actual, nullptr)

/*****************************************************************************/

OES_FWD_DECL_SHARED_PTRS(AbstractAssertionHandler);

OES_TESTING_CORE_EXPORT void pushUnitTestHandler();
OES_TESTING_CORE_EXPORT void pushAssertionHandler(
    const AbstractAssertionHandler_ptr& handler
);
OES_TESTING_CORE_EXPORT AbstractAssertionHandler_ptr popAssertionHandler();
OES_TESTING_CORE_EXPORT void clearAssertionHandlers();

/*****************************************************************************/

class OES_TESTING_CORE_EXPORT AbstractAssertionHandler {
public:
    virtual ~AbstractAssertionHandler();

private:
    virtual void handleAssertion(const oes::core::AssertionInfo& info) = 0;
    virtual bool handleBreakpoint() = 0;

private:
    void install();
    friend OES_TESTING_CORE_EXPORT void pushAssertionHandler(const AbstractAssertionHandler_ptr& handler);
    friend OES_TESTING_CORE_EXPORT AbstractAssertionHandler_ptr popAssertionHandler();
};

/*****************************************************************************/

class OES_TESTING_CORE_EXPORT ScopedExpectedAssert {
public:
    explicit ScopedExpectedAssert(int expectedCount = 1, bool recordAsserts = false);
    ScopedExpectedAssert(
        int expectedDebugCount, int expectedReleaseCount,
        bool recordAsserts = false
    );
    ~ScopedExpectedAssert();

    const std::vector<oes::core::AssertionInfo>& recordedAsserts() const;

    // no copy/assignment
    ScopedExpectedAssert(const ScopedExpectedAssert&) = delete;
    ScopedExpectedAssert& operator=(const ScopedExpectedAssert&) = delete;

private:
    int actualCount_;
    const int expectedCount_;

    std::vector<oes::core::AssertionInfo> asserts_;
};

/*****************************************************************************/

} // namespace testing
} // namespace oes

#endif
