// ============================================================================
// Copyright 2010-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <iostream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#if defined(_WIN32)
    #define VC_EXTRALEAN
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/ptrace.h>
#endif
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>


namespace oes {
    namespace core {

        namespace {
            void defaultAssertionHandler(const AssertionInfo& info) {
                // Default assertion policy is to:
                // - Send a fatal error message
                // - abort in debug build
                OES_ERROR(
                    "{} ({}). \nIn file: {}\nIn function: {}\nAt line: {}",
                    info.message, info.condition, info.file, info.function, info.line
                );

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

        /*! \struct AssertionInfo
         * \brief Describes the context of an assertion/check failure
         * \ingroup oes_core_utils_assertion
         */

        /*! \enum AssertionInfo::Type
         * \brief Type of assertion failure (check/assert)
         *
         * \var AssertionInfo::Type AssertionInfo::AssertFailed
         * When using OES_ASSERT
         *
         * \var AssertionInfo::Type AssertionInfo::DebugAssertFailed
         * When using OES_DEBUG_ASSERT
         */

        /*! \var AssertionInfo::Type AssertionInfo::type
         * \brief Type of assertion failure (check/require/assert)
         */

        /*! \var String AssertionInfo::message
         * \brief A message (if any) associated with the assertion
         */

        /*! \var String AssertionInfo::condition
         * \brief The condition (in its textual form) that triggered the assertion failure
         */

        /*! \var String AssertionInfo::file
         * \brief The file where the assertion failed (as provided by compiler intrinsic __FILE__)
         */

        /*! \var String AssertionInfo::function
         * \brief The function where the assertion failed (as provided by compiler intrinsic __FUNCTION__)
         */

        /*! \var int AssertionInfo::line
         * \brief The line where the assertion failed (as provided by compiler intrinsic __LINE__)
         */

        /*! \typedef boost::function<void(const AssertionInfo&)> AssertionHandler
         * \brief Type of the function object used to handle assertion failure
         * \ingroup oes_core_utils_assertion
         * \see getAssertionHandler setAssertionHandler
         */

        /*! \brief Retrieves the current assertion handler
         * \ingroup oes_core_utils_assertion
         * \details
         * The assertion handler is invoked whenever an assertion failed, with relevant
         * argument describing current assertion failure context. Most typically, it is
         * automatically invoked as part of OES_ASSERT and OES_EXPENSIVE_ASSERT
         * macros.
         * \return Current assertion handler (or a a null function object if not set)
         */
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

        /*! \typedef boost::function<bool()> BreakpointHandler
         * \brief Type of the function object used to handle breakpoint invocation
         * \ingroup oes_core_utils_assertion
         * \details
         * A breakpoint handler should return \c true if client may proceed and
         * trigger a breakpoint, \c false otherwise (breakpoint disabled).
         * \see getBreakpointHandler setBreakpointHandler
         */

        /*! \brief Retrieves the current breakpoint handler
         * \ingroup oes_core_utils_assertion
         * \details
         * The breakpoint handler is invoked right before a breakpoint is triggered
         * programmatically using OES_BREAK_INTERNAL() macro.
         * \return Current breakpoint handler (or a default handler if not set, see note)
         * \note A default handler is created and returned when this function is invoked
         * for the first time. The default handler works as follows:
         * - In all release builds, returns false (i.e. do not break)
         * - In Windows debug builds, returns true (i.e. do break)
         * - In Linux debug builds, returns true if a debugger is attached (i.e. do break),
         * false otherwise (i.e. do not break).
         */
        const BreakpointHandler getBreakpointHandler() {
            return getBreakpointHandlerRef();
        }

        /*! \brief Sets a new breakpoint handler
         * \ingroup oes_core_utils_assertion
         * \param[in] handler A new breakpoint handler to use (may be a null function object)
         */
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

/*! \defgroup oes_core_utils_assertion Assertion handling and macros
 * \ingroup oes_core_utils
 * \warning Read the following carefully as OES assertion behavior may differ
 * from other code bases.
 *
 * \par Macros
 * Two macros are available:
 * \li OES_ASSERT should be used whenever possible to check and enforce
 * invariants, pre-conditions and post-conditions. This type of assertion
 * is ALWAYS enabled and include a fallback action.
 * \li OES_DEBUG_ASSERT should be used when it is potentially computationally-expensive
 * to evaluate the condition or should be enforced in debug mode only. This type of
 * assertion is enabled in debug mode ONLY and does not include a fallback action.
 *
 * \par Macros and compilation mode
 * Not all macros are created equal, beware of side-effects in the evaluation of
 * the condition.
 * \li OES_ASSERT is ALWAYS enabled, the condition is always evaluated regardless of
 * compilation mode (debug/release/...)
 * \li OES_DEBUG_ASSERT is ONLY enabled in debug mode, the condition is therefore
 * NOT evaluated in any other mode.
 *
 * \par Behavior in DEBUG mode:
 * \li The behavior when an assertion fails is to check if breakpoints are enabled,
 * breaking if needed, to call the assertion handler and finally to ABORT (REGARDLESS
 * of any fallback action).
 * \li The default breakpoint policy is to always break in Windows debug builds and break
 * only if a debugger is attached in Linux debug builds.
 *
 * \par Behavior in RELEASE mode:
 * \li The behavior when an assertion fails is to call the assertion handler and execute
 * the fallback action provided as input.
 *
 * \par Assertion and breakpoint handlers
 * The default behavior can be changed as follows:
 * \li setBreakpointHandler() can be used to provide a new breakpoint handling strategy.
 * The breakpoint is triggered only if the breakpoint handler exists and returns \c true.
 * \li setAssertionHandler() can be used to provide a new assertion handling strategy.
 * The assertion handler is called whenever the expression evaluated by OES_ASSERT
 * or OES_DEBUG_ASSERT() fails.
 */

/* \def OES_ASSERT(condition,action)
 * \brief Macro used to check that a condition is met
 * \ingroup oes_core_utils_assertion
 * \details
 * \li If breakpoints are enabled (i.e. the breakpoint handler exists and returns
 * \c true, \see getBreakpointHandler), a breakpoint is triggered before proceeding.
 * \li If an assertion handler exists (\see getAssertionHandler), an assertion context is
 * created (\see AssertionInfo) and the assertion handler is invoked.
 * \li In DEBUG builds, execution flow is stops here and the program aborts. In RELEASE
 * builds, the execution continues starting with the fallback action provided as input.
 *
 * \note This type of assertion is always enabled (in release mode as well as any other
 * mode). \p condition is always evaluated.
 */

/* \def OES_DEBUG_ASSERT(condition)
 * \brief Macro used to enforce (in DEBUG mode only) that a required condition is met
 * \ingroup oes_core_utils_assertion
 * \details
 * \li If breakpoints are enabled (meaning that the breakpoint handler returns \c true,
 * \see getBreakpointHandler), a breakpoint is triggered before proceeding.
 * \li If an assertion handler exists (\see getAssertionHandler), an assertion context is
 * created (\see AssertionInfo) and the handler is invoked.
 * \li The execution flow stops here and the program aborts.
 *
 * \warning This type of assertion is only enabled in debug mode, \p condition
 * is NOT evaluated in non-debug mode.
 */

/* \def OES_ASSERT_MSG(condition, oss_args, action)
* \brief Macro used to check that a condition is met, and send a message when it is not the case.
* \ingroup oes_core_utils_assertion
* \details
* \li If breakpoints are enabled (i.e. the breakpoint handler exists and returns
* \c true, \see getBreakpointHandler), a breakpoint is triggered before proceeding.
* \li If an assertion handler exists (\see getAssertionHandler), an assertion context is
* created (\see AssertionInfo) and the assertion handler is invoked.
* \li In DEBUG builds, execution flow stops here and the program aborts. In RELEASE
* builds, the execution continues starting with the fallback action provided as input.
*
* \note This type of assertion is always enabled (in release mode as well as any other
* mode). \p condition is always evaluated.
*/

/*! \internal
 * \def OES_DO_NOTHING_STATEMENT
 * \brief DO NOT USE - Implementation macro defining the "do nothing" statement
 * \ingroup oes_core_utils_assertion
 * \details
 * This is required to implement correctly most macros and prevent un-wanted expansion
 * when macro is empty or nested within control statements (e.g. if).
 * Forces client to terminate macro calls with a semi-colon
 */

/*! \internal
 * \def OES_BREAK_INTERNAL
 * \brief DO NOT USE - Implementation macro defining a breakpoint invocation
 * \ingroup oes_core_utils_assertion
 * \par Supported platforms and OS
 * \li Windows using Microsoft (cl) or Intel (icc) compilers
 * \li Linux using GCC on Intel (g++) or Intel (icc) compilers
 */
