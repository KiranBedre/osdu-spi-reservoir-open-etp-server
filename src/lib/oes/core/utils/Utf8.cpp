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
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1

#include <oes/core/utils/Utf8.h>

#include <string.h>
#include <wchar.h>

#include <codecvt>
#include <locale>

namespace oes {
namespace core {

namespace utf8 {
/*!
 * \brief Converts an UTF-8 encoded string to a wide-string.
 * \param utf8_string the UTF-8 encoded string to convert
 */
std::wstring fromUtf8ToWstr(const std::string& utf8_string) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.from_bytes(utf8_string);
}

/*!
 * \brief Converts a wide-string to an UTF-8 encoded string.
 * \param wide_string the string to convert
 */
std::string fromWstrToUtf8(const std::wstring& wide_string) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(wide_string);
}
} // namespace utf8

} // namespace core
} // namespace oes
