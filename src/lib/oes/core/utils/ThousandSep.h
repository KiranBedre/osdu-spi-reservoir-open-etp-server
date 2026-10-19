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

#ifndef OES_CORE_UTILS_THOUSANDSEP_H
#define OES_CORE_UTILS_THOUSANDSEP_H

#include <locale>
#include <ostream>
#include <sstream>

/*! \file oes/core/utils/ThousandSep.h
* \brief Print integers with thousand, million, billion, etc... comma separators.
* Header-only and \em inline on purpose.
*/

namespace oes {
namespace core {

//============================================================================

/*!
 * \internal
 * \class CommaThousandSeparatorFacet oes/core/utils/ThousandSep.h
 * \brief Special facet to force US thousand separator (i.e. comma).
 * \ingroup oes_core_utils
 */
struct CommaThousandSeparatorFacet : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\003"; }
};

//============================================================================

/*!
 * \class ScopedCommaThousandSeparator oes/core/utils/ThousandSep.h
 * \brief RAII utility to enable comma as a thousand-separator on a stream.
 * \ingroup oes_core_utils
 */
struct ScopedCommaThousandSeparator {
    std::ostream& os_;
    std::locale prev_locale_;

    explicit ScopedCommaThousandSeparator(
        std::ostream& os
    ) : os_(os)
      , prev_locale_(os.getloc())
    {
        os.imbue(std::locale(prev_locale_, new CommaThousandSeparatorFacet));
    }

    ~ScopedCommaThousandSeparator() {
        os_.imbue(prev_locale_);
    }

    ScopedCommaThousandSeparator(const ScopedCommaThousandSeparator&) = delete;
    ScopedCommaThousandSeparator& operator=(const ScopedCommaThousandSeparator&) = delete;
};

//============================================================================

/*!
 * \brief Converts an integer to a string, with thousand comma separator.
 *
 * Convenience \em printing a single integer using ScopedCommaThousandSeparator.
 *
 * Especially useful to read large number in the millions or billions,
 * for human-friendly display in command-line applications.
 *
 * Prefer using ScopedCommaThousandSeparator directly, when needing to
 * output many numbers, rather than using this convenience for a single number.
 *
 * \param value the number to convert.
 * \return the number string, for example, 7 or 1,007 or 2,345,678
 */
inline std::string thou(size_t value) {
    std::ostringstream oss;
    ScopedCommaThousandSeparator thousandSep(oss);
    oss << value;
    return oss.str();
}

//============================================================================

} // namespace oes
} // namespace core

#endif
