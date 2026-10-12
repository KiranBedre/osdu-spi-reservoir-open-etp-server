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

// WARNING: please, keep this file without any OES or 3rd-party includes

/*!
 * \file oes/core/utils/config_leave_3rdparty.h
 * \brief Include this file AFTER any 3rd-party includes to pop disabled warnings
 */

// To prevent improper use of enter/leave 3rdparty headers
// Any change of logic here should be duplicated in the other
// header config_enter_3rdparty.h
#ifndef OES_CORE_UTILS_CONFIG_ENTER_3RDPARTY_H
    #error "Enter/leave 3rd-party header mismatch - You probably missed an include of config_enter_3rdparty.h"
#endif
#undef OES_CORE_UTILS_CONFIG_ENTER_3RDPARTY_H

#if defined(_WIN32)

    #pragma warning(pop)

    // warning C4505: '<...>' : unreferenced local function has been removed
    #pragma warning(disable :4505)

    #if defined(_SCL_SECURE_NO_WARNINGS_UNDEFINE)
        #undef _SCL_SECURE_NO_WARNINGS
    #endif

#elif defined(__INTEL_COMPILER)

    #pragma warning(pop)

#elif defined(__clang__)

    #pragma GCC diagnostic pop

#elif defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406

    // NOTE: diagnostic pragmas are only supported from GCC 4.2
    // NOTE: GCC earlier than 4.6 cannot push/pop the warning state

    #pragma GCC diagnostic pop

#endif

