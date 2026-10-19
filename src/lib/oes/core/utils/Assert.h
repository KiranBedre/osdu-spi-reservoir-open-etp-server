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

#ifndef OES_CORE_UTILS_ASSERT_H
#define OES_CORE_UTILS_ASSERT_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/String.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/current_function.hpp>
#include <boost/function.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
    namespace core {

        struct OES_CORE_UTILS_EXPORT AssertionInfo {
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
        OES_CORE_UTILS_EXPORT const BreakpointHandler getBreakpointHandler();
        OES_CORE_UTILS_EXPORT void setBreakpointHandler(const BreakpointHandler& handler);

        typedef boost::function<void(const AssertionInfo&)> AssertionHandler;
        OES_CORE_UTILS_EXPORT const AssertionHandler getAssertionHandler();
        OES_CORE_UTILS_EXPORT void setAssertionHandler(const AssertionHandler& handler);
        
        namespace detail {
            // To avoid compilation warning(s) when evaluating assertion
            inline bool is_true(bool condition) {
                return condition;
            }

            OES_CORE_UTILS_EXPORT bool isDebuggerAttached();
            OES_CORE_UTILS_EXPORT bool shouldBreak();
            OES_CORE_UTILS_EXPORT void handleAssertionFailure(
                AssertionInfo::Type type,
                const char* condition,
                const char* message,
                const char* file,
                const char* function,
                int line);
        } // End namespace detail

    } // End namespace core
} // End namespace oes

#define OES_DO_NOTHING_STATEMENT ((void)0)


// Definition of OES_BREAK_INTERNAL

// MSVC or Intel C++ compilers
#if defined(_MSC_VER) || defined( __INTEL_COMPILER)
    #define OES_BREAK_INTERNAL()\
        __debugbreak()
// GNU GCC on Intel 32 & 64 platforms
#elif defined( __GNUC__ ) && ( defined(__i386__) || defined(__x86_64__) )
    #define OES_BREAK_INTERNAL()\
        __asm__ __volatile__ ( "int $3" )
// Unknown platforms or compilers
#else
    #define OES_BREAK_INTERNAL()\
        OES_DO_NOTHING_STATEMENT
#endif

// Definition of OES_ASSERT_MSG

#define OES_ASSERT_MSG(condition,oss_args,action)\
    if (not oes::core::detail::is_true(bool(condition)) ) {\
        if (oes::core::detail::shouldBreak()) {\
            OES_BREAK_INTERNAL();\
        }\
        std::ostringstream oss;\
        oss << "Assertion failed: " << oss_args;\
        const std::string msg = oss.str();\
        oes::core::detail::handleAssertionFailure(\
            oes::core::AssertionInfo::AssertFailed, msg.c_str(),\
             #condition,\
            __FILE__,\
            BOOST_CURRENT_FUNCTION,\
            __LINE__\
        );\
        action\
    } else OES_DO_NOTHING_STATEMENT


// Definition of OES_ASSERT_INTERNAL

#define OES_ASSERT_INTERNAL(condition,type,msg,action)\
    if( not oes::core::detail::is_true(bool(condition)) ) {\
        if( oes::core::detail::shouldBreak() ) {\
            OES_BREAK_INTERNAL();\
        }\
        oes::core::detail::handleAssertionFailure(\
            type,\
            msg,\
            #condition,\
            __FILE__,\
            BOOST_CURRENT_FUNCTION,\
            __LINE__\
        );\
        action\
    } else OES_DO_NOTHING_STATEMENT

// Definition of OES_ASSERT

#define OES_ASSERT(condition,action)\
    OES_ASSERT_INTERNAL(condition, oes::core::AssertionInfo::AssertFailed,\
        "Assertion failed",\
        action)

// Definition of OES_DEBUG_ASSERT

#if defined (OES_DEBUG_BUILD)
    #define OES_DEBUG_ASSERT(condition)\
        OES_ASSERT_INTERNAL(condition, oes::core::AssertionInfo::DebugAssertFailed,\
        "Debug assertion failed",\
        OES_DO_NOTHING_STATEMENT;)
#else
    #define OES_DEBUG_ASSERT(condition)\
        OES_DO_NOTHING_STATEMENT
#endif

#endif
