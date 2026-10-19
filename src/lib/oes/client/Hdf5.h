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

#ifndef OES_CLIENT_HDF5_H
#define OES_CLIENT_HDF5_H

// FIXME: (windows)
// - Hdf5 relies on off_t which is not defined when __STDC__ is on (only
// _off_t is in sys/types.h).
// - Hdf5 was compiled with sizeof(off_t)==sizeof(long) and there is some
// code that relies on this assumption.
// - So there is potentially both a re-definition issue ("redefinition; different
// basic types") and size issue.

#if defined(_WIN32) && defined(__STDC__)
    #include <sys/types.h>
    #define off_t _off_t
#endif

#include <oes/core/utils/config_enter_3rdparty.h>
#include <H5Cpp.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#if defined(_WIN32) && defined(__STDC__)
    #undef off_t
#endif

#endif
