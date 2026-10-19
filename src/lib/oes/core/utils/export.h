// ============================================================================
// Copyright 2008-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_EXPORT_H
#define OES_CORE_UTILS_EXPORT_H

#include <oes/core/utils/config.h>

#if ! defined(OES_CORE_UTILS_EXPORT)
#   if defined(_WIN32)
#       if defined(oes_core_utils_EXPORTS)
#           define OES_CORE_UTILS_EXPORT __declspec(dllexport)
#       else
#           define OES_CORE_UTILS_EXPORT __declspec(dllimport)
#       endif
#   else
#       define OES_CORE_UTILS_EXPORT
#   endif
#endif

#endif

