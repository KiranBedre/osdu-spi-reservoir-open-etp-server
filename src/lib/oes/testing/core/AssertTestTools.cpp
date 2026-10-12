// ============================================================================
// Copyright 2012-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <exception>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/bind/bind.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/execution_monitor.hpp>
#include <boost/test/debug.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#ifdef PGDM_NO_CPP11_LEGACY_SUPPORT
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
    #define VC_EXTRALEAN
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/ptrace.h>
#endif
#endif
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/core/AssertTestTools.h>

#include <oes/core/utils/Logger.h>


namespace but = boost::unit_test;

namespace {

/*****************************************************************************/

struct UnitTestAssertionHandler : public oes::testing::AbstractAssertionHandler {
    bool abortTestOnAssertion;
    bool breakOnAssertion;

    UnitTestAssertionHandler(
    ) : abortTestOnAssertion(true)
      , breakOnAssertion(false)
    {
#if defined(OES_DEBUG_BUILD)
        // No point to continue, as std::abort will be called anyway
        abortTestOnAssertion = true;
        // - Boost.Test traps the breakpoint signal on windows,
        //   so we only break when the debugger is effectively attached.
        // - Not doing so will let Boost.Test print a not-so-explicit
        //   "breakpoint encountered" message and stop
        //   (as opposed to the one printed by handleAssertion)
        breakOnAssertion = boost::debug::under_debugger();
#endif
    }

    ~UnitTestAssertionHandler() override {
    }

    void handleAssertion(const oes::core::AssertionInfo& info) override {
        but::unit_test_log << but::log::begin(info.file, info.line);
        but::unit_test_log << but::log_fatal_errors;
        but::unit_test_log << info.message << " (" << info.condition
            << "), in function " << info.function;
        but::unit_test_log << but::log::end();

#if (BOOST_VERSION < 106000)
        but::framework::assertion_result (false); // Always fails the test
#else
        but::framework::assertion_result (boost::unit_test::assertion_result (false)); // Always fails the test
#endif
        if( abortTestOnAssertion ) {
            but::framework::test_unit_aborted(but::framework::current_test_case());
#if (__cplusplus >= 201703L) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201703L))
            if (std::uncaught_exceptions() == 0) {
#else
            if (!std::uncaught_exception()) {
#endif
                throw boost::execution_aborted();
            } else {
                but::unit_test_log << "boost::execution_aborted() not thrown; unwinding";
                but::unit_test_log << but::log::end();
            }
        }
    }

    bool handleBreakpoint() override {
        return breakOnAssertion;
    }
};

/*****************************************************************************/

struct IgnoreAssertionHandler : public oes::testing::AbstractAssertionHandler {
    int& actualCount_;
    std::vector<oes::core::AssertionInfo>* p_asserts_;

    IgnoreAssertionHandler(
        int& actualCount, std::vector<oes::core::AssertionInfo>* p_asserts
    ) : actualCount_(actualCount)
      , p_asserts_(p_asserts)
    {
    }

    ~IgnoreAssertionHandler() override {
    }

    void handleAssertion(const oes::core::AssertionInfo& info) override {
        ++actualCount_;
        if (p_asserts_) {
            p_asserts_->push_back(info);
        }
    }

    bool handleBreakpoint() override {
        return false;
    }

private: // no copy/assignment
    IgnoreAssertionHandler(const IgnoreAssertionHandler&);
    IgnoreAssertionHandler& operator=(const IgnoreAssertionHandler&);
};

/*****************************************************************************/

oes::core::AssertionHandler default_assert_handler_;
oes::core::BreakpointHandler default_dp_handler_;

std::vector<oes::testing::AbstractAssertionHandler_ptr> handler_stack_;

/*****************************************************************************/

} // namespace

namespace oes {
namespace testing {

/*****************************************************************************/

/*!
 * \brief Compares two strings, expected to be equal, generating diff-like
 *     report on differences.
 *
 * Mostly useful in unit test when comparing large multi-line text,
 * which are very hard to visually diff when output by Boost.Test \em normally.
 *
 * Not meant to be used directly, but instead via the macros
 * OES_CHECK_SAME_MULTILINE_TEXT and OES_REQUIRE_SAME_MULTILINE_TEXT.
 *
 * The diff-like report uses a simple format, where identical lines are output
 * with a prefix of spaces only, while differing lines are output for both
 * \a expected and \a actual, on top of each other to easily spot differences,
 * with "L <<<  " and "R >>>  " prefixes, respectively.
 *
 * \param expected the left-hand-side \em expected text.
 * \param actual the right-hand-side text actually computed and to be tested.
 * \param[out] diff_like_output_msg variable to hold the diff-like report.
 * \param comparator (Optional; default to none) boolean predicate which is
 *     forwarded, when provided, the 0-based line number and differing lines,
 *     to allow custom heuristic (e.g. regular expression) to determine when
 *     two lines are considered different. When it returns false, the lines are
 *     not-equal. Not called on identical lines. 
 * \return true when expected == actual, in which case \a diff_like_output_msg
 *     is left unchanged; false otherwise, with \a diff_like_output_msg assigned
 *     a diff-like by-line report of differences.
 *
 * \note this is \b not a actual diff, but a line-by-line report of differences.
 */
bool compareMultiLineText(
    const std::string& expected, const std::string& actual,
    std::string& diff_like_output_msg, CustomLineComparator comparator
) {
    if (expected == actual) {
        diff_like_output_msg.clear();
        return true;
    }

    std::vector<std::string> split_expected;
    boost::split(split_expected, expected, boost::algorithm::is_any_of("\n"));
    std::vector<std::string> split_actual;
    boost::split(split_actual, actual, boost::algorithm::is_any_of("\n"));

    std::ostringstream oss;
    oss << '\n';
    bool same = true;
    const size_t min_size = std::min(split_actual.size(), split_expected.size());
    for (size_t i = 0; i < min_size; ++i) {
        const std::string& tmp_expected = split_expected[i];
        const std::string& tmp_actual = split_actual[i];

        if (tmp_expected == tmp_actual) {
            oss << "    " << tmp_expected << '\n';
        } else if (!comparator || !comparator(i, &tmp_expected, &tmp_actual)) {
            same = false;
            // Show differences in whitespace at the end, which is invisible otherwise.
            oss << "L <<" << tmp_expected << ">>\n";
            oss << "R >>" << tmp_actual << "<<\n";
        }
    }

    if (split_actual.size() > split_expected.size()) {
        for (size_t i = min_size; i < split_actual.size(); ++i) {
            const std::string& tmp_actual = split_actual[i];

            if (!comparator || !comparator(i, nullptr, &tmp_actual)) {
                same = false;
                oss << "R ++" << tmp_actual << "++\n";
            }
        }
    }

    if (split_expected.size() > split_actual.size()) {
        for (size_t i = min_size; i < split_expected.size(); ++i) {
            const std::string& tmp_expected = split_expected[i];
            if (!comparator || !comparator(i, &tmp_expected, nullptr)) {
                same = false;
                oss << "L ++" << split_expected[i] << "++\n";
            }
        }
    }

    if (!same) {
        diff_like_output_msg = oss.str();
    } else {
        diff_like_output_msg.clear();
    }

    return same;
}

/*****************************************************************************/

/*!
 * \brief Installs the default Unit-Test assertion handler.
 *
 * This handlers breaks the code only if a debugger is attached to the current
 * process, and fails the current test if any assertion is raised by the tested
 * code. Use ScopedExpectedAssert to explicitly and temporarily disable this
 * automatic test failing on assertions to test code boundary conditions that
 * do raise assertions, on purpose.
 */
void pushUnitTestHandler() {
    AbstractAssertionHandler_ptr handler(new UnitTestAssertionHandler);
    pushAssertionHandler(handler);
}

/*!
 * \brief Installs a new custom assertion handler.
 *
 * \param handler the handler to be installed. Caller typically does not hold
 *     on to the handler, which will naturally be deleted when popped from the
 *     handler stack.
 *
 * \warning Must be balanced with a call to popAssertionHandler().
 */
void pushAssertionHandler(const AbstractAssertionHandler_ptr& handler) {
    assert(handler);
    if (handler_stack_.empty()) {
        default_assert_handler_ = oes::core::getAssertionHandler();
        default_dp_handler_ = oes::core::getBreakpointHandler();
    }
    handler_stack_.push_back(handler);
    handler->install();
}

/*!
 * \brief Uninstalls the last pushed custom assertion handler.
 *
 * \return the uninstalled handler, should the caller need to examine its
 *     state (by dynamic casting to a known derived class of course).
 */
AbstractAssertionHandler_ptr popAssertionHandler() {
    assert(!handler_stack_.empty());
    AbstractAssertionHandler_ptr back = handler_stack_.back();
    handler_stack_.pop_back();
    if (!handler_stack_.empty()) {
        handler_stack_.back()->install(); // install re-activated handler
    } else {
        clearAssertionHandlers();
    }
    return back;
}

/*!
 * \brief Uninstalls all handlers, and restore the default assertion and
 *     breakpoint handler functions.
 */
void clearAssertionHandlers() {
    handler_stack_.clear();
    if (default_assert_handler_) {
        oes::core::setAssertionHandler(default_assert_handler_);
    }
    if (default_dp_handler_) {
        oes::core::setBreakpointHandler(default_dp_handler_);
    }
}

/*****************************************************************************/

/*! \class AbstractAssertionHandler oes/testing/core/AssertTestTools.h
 * \brief Base class for custom assertion handlers, to push/pop on handler stack.
 *
 * Derived class must implement the handleAssertion() and handleBreakpoint()
 * virtual methods.
 *
 * \ingroup oes_testing_core
 */

/*!
 * \brief Deletes this handler.
 */
AbstractAssertionHandler::~AbstractAssertionHandler() {
}

/*!
 * \internal
 * \brief Installs this handler assertion and breakpoint handler functions.
 */
void AbstractAssertionHandler::install() {
    oes::core::setAssertionHandler(
        boost::bind(&AbstractAssertionHandler::handleAssertion, this, boost::placeholders::_1)
    );
    oes::core::setBreakpointHandler(
        boost::bind(&AbstractAssertionHandler::handleBreakpoint, this)
    );
}

/*!
 * \fn virtual void AbstractAssertionHandler::handleAssertion(const oes::core::AssertionInfo& info) = 0
 * \brief Callback receiving detailed information about triggered assertions.
 * \param info
 */

/*!
 * \fn virtual bool AbstractAssertionHandler::handleBreakpoint() = 0
 * \brief Callback receiving notification of an about-to-be triggered breakpoint.
 * \return true if the breakpoint should be triggered; false otherwise.
 */

/*****************************************************************************/

/*! \class ScopedExpectedAssert oes/testing/core/AssertTestTools.h
 * \brief Asserts assertions are being triggered, as expected, but test code
 *     that feeds invalid arguments or inputs to some tested code.
 *
 * \ingroup oes_testing_core
 */

/*!
 * \brief Instantiates ScopedExpectedAssert instance.
 *
 * Pushes a new assertion handler on the handler stack, which will not fail
 * the test is an assertion is triggered while this instance is \em alive.
 * Records how many assertions were effectively observed, and possibly detailed
 * information about each assertions if requested.
 *
 * \param expectedCount (Optional; default to 1) how many asserts we are
 *     supposed to ignore. Observing more or less than this \a expectedCount
 *     will force a test failure.
 * \param recordAsserts (Optional; default to false) whether to actually record
 *     each ignored assertions' details, to allow double-checking they really
 *     were the one(s) the client code expected to trigger.
 *
 * \sa pushAssertionHandler
 */
ScopedExpectedAssert::ScopedExpectedAssert(
    int expectedCount, bool recordAsserts
) : actualCount_(0)
  , expectedCount_(expectedCount)
{
    oes::testing::AbstractAssertionHandler_ptr handler(
        new IgnoreAssertionHandler(
            actualCount_,
            recordAsserts? &asserts_: nullptr
        )
    );
    oes::testing::pushAssertionHandler(handler);
}

/*!
 * \brief Instantiates ScopedExpectedAssert instance.
 *
 * Pushes a new assertion handler on the handler stack, which will not fail
 * the test is an assertion is triggered while this instance is \em alive.
 * Records how many assertions were effectively observed, and possibly detailed
 * information about each assertions if requested.
 *
 * \param expectedDebugCount how many asserts (including OES_DEBUG_ASSERT) we are
 *     supposed to ignore. Observing more or less than this \a expectedDebugCount
 *     will force a test failure.
 * \param expectedReleaseCount how many asserts (excluding OES_DEBUG_ASSERT) we 
 *     are supposed to ignore. Observing more or less than this \a expectedReleaseCount
 *     will force a test failure.
 * \param recordAsserts (Optional; default to false) whether to actually record
 *     each ignored assertions' details, to allow double-checking they really
 *     were the one(s) the client code expected to trigger.
 *
 * \sa pushAssertionHandler
 */
ScopedExpectedAssert::ScopedExpectedAssert(
    int expectedDebugCount,
    int expectedReleaseCount,
    bool recordAsserts
) : actualCount_(0)
#if defined (OES_DEBUG_BUILD)
, expectedCount_(expectedDebugCount)
#else
, expectedCount_(expectedReleaseCount)
#endif
{
#if defined (OES_DEBUG_BUILD)
    OES_UNUSED(expectedReleaseCount);
#else
    OES_UNUSED(expectedDebugCount);
#endif
    oes::testing::AbstractAssertionHandler_ptr handler(
        new IgnoreAssertionHandler(
            actualCount_,
            recordAsserts? &asserts_: nullptr
        )
    );
    oes::testing::pushAssertionHandler(handler);
}

/*!
 * \brief Instantiates ScopedExpectedAssert instance.
 *
 * \sa popAssertionHandler
 */
ScopedExpectedAssert::~ScopedExpectedAssert() {
    oes::testing::popAssertionHandler();

    if (actualCount_ != expectedCount_) {
        std::ostringstream oss;
        oss << "Expected " << expectedCount_ << " asserts, but got only " << actualCount_;
        BOOST_FAIL(oss.str());
    }
}

/*!
 * \brief Gets the ignored assertions, if any.
 *
 * \return detailed assertion information, if their recording was requested.
 */
const std::vector<oes::core::AssertionInfo>& ScopedExpectedAssert::recordedAsserts() const {
    return asserts_;
}

/*****************************************************************************/

} // namespace testing
} // namespace oes


/*****************************************************************************/

#ifdef PGDM_NO_CPP11_LEGACY_SUPPORT
// Copied from Assert.cpp
namespace oes {
    namespace core {

        namespace {
            void defaultAssertionHandler(const AssertionInfo& info) {
                // Default assertion policy is to:
                // - Send a fatal error message
                // - abort in debug build
                OES_ERROR("{} ({}) \n In file: {}\nIn function: {}\nAt line: {}"
                    , info.message, info.condition, info.file, info.function, info.line);
                

#if defined (OES_DEBUG_BUILD)
                std::abort();
#endif
            }

            AssertionHandler& getAssertionHandlerRef() {
                static AssertionHandler handler = defaultAssertionHandler;
                return handler;
            }

            bool defaultBreakpointHandler() {
                // Default breakpoint policy is to:
                // - Never break in release builds
                // - Always break in Windows debug builds
                // - Conditionally break in Linux debug builds
#if defined (OES_DEBUG_BUILD)
    #if defined(_WIN32)
                return true;
    #else
                return  detail::isDebuggerAttached();
    #endif
#else
                return false;
#endif
            }

            BreakpointHandler& getBreakpointHandlerRef() {
                static BreakpointHandler handler = defaultBreakpointHandler;
                return handler;
            }
        }

        const AssertionHandler getAssertionHandler() {
            return getAssertionHandlerRef();
        }

        /*! \brief  Sets a new assertion handler
         * \ingroup oes_core_utils_assertion
         * \param[in] handler A new assertion handler to use (may be a null function object)
         */
        void setAssertionHandler(const AssertionHandler& handler) {
            getAssertionHandlerRef() = handler;
        }

        const BreakpointHandler getBreakpointHandler() {
            return getBreakpointHandlerRef();
        }

        void setBreakpointHandler(const BreakpointHandler& handler) {
            getBreakpointHandlerRef() = handler;
        }

        namespace detail {
            bool shouldBreak() {
                const BreakpointHandler handler = getBreakpointHandler();
                return handler && handler();
            }

            void handleAssertionFailure(
                AssertionInfo::Type type,
                const char* condition,
                const char* message,
                const char* file,
                const char* function,
                int line) {
                const AssertionHandler handler = getAssertionHandler();
                if( handler ) {
                    const AssertionInfo info(
                        type,
                        condition ? condition : String(),
                        message   ? message   : String(),
                        file      ? file      : String(),
                        function  ? function  : String(),
                        line);
                    handler(info);
                }
            }

            bool isDebuggerAttached() {
#if defined(_WIN32)
                return (IsDebuggerPresent()!=0);
#else
                // From Topology's assert. This call to ptrace will return an
                // error if the current process is being traced (running with debugger
                // or debugger attached) and prevent program to crash if a breakpoint
                // is triggered while not under debugger.
                // Notorious side-effects: the ptrace function can only be called ONCE,
                // debugger cannot be attached afterwards.
                static const bool value = (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0);
                return value;
#endif
            }
        } // End namespace detail

    } // End namespace core
} // End namespace oes

#endif
