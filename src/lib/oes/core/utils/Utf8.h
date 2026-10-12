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

#ifndef OES_CORE_UTILS_UTF8_H
#define OES_CORE_UTILS_UTF8_H

#include <oes/core/utils/export.h>

#ifdef __cplusplus
#include <string>

namespace oes {
namespace core {
namespace utf8 {

OES_CORE_UTILS_EXPORT std::wstring fromUtf8ToWstr(const std::string& utf8_string);
OES_CORE_UTILS_EXPORT std::string fromWstrToUtf8(const std::wstring& wide_string);

} // namespace utf8
} // namespace core
} // namespace oes

#endif // __cplusplus
#endif // OES_CORE_UTILS_UTF8_H