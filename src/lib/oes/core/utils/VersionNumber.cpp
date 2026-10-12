// ============================================================================
// Copyright 2015-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/VersionNumber.h>
#include <oes/core/utils/String.h>

#include <regex>
#include <sstream>

/*!
 * \file oes/core/utils/VersionNumber.h
 * \ingroup oes_core_utils
 * \brief Provides a version number class.
 */

namespace oes {
namespace core {

/*!
 * \class VersionNumber core/utils/VersionNumber.h
 * \brief Version Numbering class
 * \details
 * Version numbers consist of two to four components: major, minor, change, and status level.
 * The major and minor components are required; the change and status components are optional.
 * First 3 components must be integers greater than or equal to 0, the status component is a status letter
 * (a, b, c or p) followed by status level integer.
 *
 * -status \c a indicates an alpha version,
 * -status \c b indicates a beta version
 * -status \c c indicates a candidate release
 * -status \c p indicates a patch to a release
 *
 * The format of the version number is as follows (optional components are shown in square brackets ([ and ]):
 * major.minor[.build[.revision]]
 *
 * A valid version number matches the following regular expression:
 *     (\\d+)?\.?(\\d+)?\.(\\d+)(([abcp])(\\d+))?
 *
 * That is, a valid version number is a dot('.') separated sequence of 2
 * to 3 integer components. A version number optionally accepts an
 * additional status which consists in a status indicator[abcp] followed
 * by a number.
 *
 * Note: A version number with 2 integers(such as 1.2 or 2.3) is always less
 * than a version number with 3 integers(such as 2.5.0 or 1.2.0).
 * \ingroup oes_core_utils
 */

/*!
 * Default VersionNumber constructor.
 * Builds an invalid 0.0.0 version.
 */
VersionNumber::VersionNumber() {
    components_[0] = 0;
    components_[1] = 0;
    components_[2] = 0;
    components_[3] = 0;
    status_ = invalid;
}

/*!
 * Creates new VersionNumber from explicit values.
 */
VersionNumber::VersionNumber(
    VersionComponent major,
    VersionComponent minor,
    VersionComponent change
) : status_(release) {
    components_[0] = major;
    components_[1] = minor;
    components_[2] = change;
    components_[3] = 0;
}

/*!
 * Creates a version number initialized with the given \p string.
 *
 * Major and minor version number must be separated by a dot ('.')
 * The change can be given after the major.minor information.
 * The status is optional an represented by a single letter (a, b, c or p) plus
 * a status level number.
 * Can decode string like: "1.0.1a1", "1.1p2", "2.0"
 */
VersionNumber::VersionNumber(const String& string) {
    components_[0] = 1;
    components_[1] = 0;
    components_[2] = 0;
    components_[3] = 0;
    status_ = release;

    // The following pattern have 6 capturing groups
    // Group 1 :(\d+): is decimal number for the Major version
    // Group 2 :(\d+): is decimal number for minor version
    // Group 3 :(\d+): is decimal number for change number
    // Group 4 :(([abcp])(\d+)) is the status group a letter plus a decimal number
    // Group 5 : ([abcp]) is the status letter a, b, c or p
    // Group 6 :(\d+) is status level
    std::regex re("(\\d+)?\\.?(\\d+)?\\.(\\d+)(([abcp])(\\d+))?");
    std::smatch matches;
    if (std::regex_match(string, matches, re)) {
        // Group 1 is the Major version
        std::istringstream(matches[1]) >> components_[0];
        // Group 2 is the minor version
        // Group 3 is either the minor or change number
        if (!matches[2].str().empty()) {
            std::istringstream(matches[2]) >> components_[1];
            std::istringstream(matches[3]) >> components_[2];
        }
        else {
            std::istringstream(matches[3]) >> components_[1];
            components_[2] = 0;
        }
        // Group 4 is the status group (letter + level)
        // Group 5 is the status letter a, b, c or p
        std::string status = matches[5];
        if (status == "a") status_ = alpha;
        else if (status == "b") status_ = beta;
        else if (status == "c") status_ = candidate;
        else if (status == "p") status_ = patch;
        else status_ = release;
        // Group 6 is status level
        std::istringstream(matches[6]) >> components_[3];
    }
    if ((components_[0] + components_[1] + components_[2]) == 0) {
        status_ = invalid;
    }
}

/*!
 * Copy constructor
 */
VersionNumber::VersionNumber(const VersionNumber& version) {
    components_[0] = version.components_[0];
    components_[1] = version.components_[1];
    components_[2] = version.components_[2];
    components_[3] = version.components_[3];
    status_ = version.status_;
}

/*!
 * Copy
 */
VersionNumber& VersionNumber::operator=(const VersionNumber& version){
    components_[0] = version.components_[0];
    components_[1] = version.components_[1];
    components_[2] = version.components_[2];
    components_[3] = version.components_[3];
    status_ = version.status_;
    return *this;
}

/*!
 * Parses and assigns.
 */
VersionNumber& VersionNumber::operator=(const oes::core::String& version) {
    *this = VersionNumber(version);
    return *this;
}

/*!
 * Returns the component at the specified index.
 * Index must be a positive value between 0 and max_components-1.
 */
VersionNumber::VersionComponent VersionNumber::getComponent(int index) const {
    return components_[index];
}

/*!
 * Returns the status of the version number.
 */
VersionNumber::VersionStatus VersionNumber::getStatus() const {
    return status_;
}

/*!
 * Returns the human readable form of the version number.
 */
oes::core::String VersionNumber::prettify() const {
    std::ostringstream oss;
    oss << components_[0] << "." << components_[1];

    if (components_[2] != 0) {
        oss << "." << components_[2];
    }
    switch (status_) {
    case invalid:   oss << " Invalid"; break;
    case alpha:     oss << " Alpha"; break;
    case beta:      oss << " Beta"; break;
    case candidate: oss << " Release Candidate"; break;
    case patch:     oss << " Patch"; break;
    case release:   break;
    }
    if (status_ != release && components_[3]!=0 ) {
        oss << " " << components_[3];
    }
    return oss.str();
}

/*!
 * Returns a short/serialized form of the version number.
 * It can be passed into VersionNumber constructor.
 */
oes::core::String VersionNumber::serialize() const {
    if (status_ == invalid) {
        return oes::core::String("Invalid");
    }

    std::ostringstream oss;
    oss << components_[0] << "." << components_[1];

    if (components_[2] != 0) {
        oss << "." << components_[2];
    }
    switch (status_) {
    case invalid:   oss << ""; break; // prevent compilation warnings
    case alpha:     oss << "a"; break;
    case beta:      oss << "b"; break;
    case candidate: oss << "c"; break;
    case patch:     oss << "p"; break;
    case release:   break;
    }
    if (status_ != release && components_[3]!=0 ) {
        oss << components_[3];
    }
    return oss.str();
}

/*!
 * \fn int compare(const VersionNumber& a, const VersionNumber& b)
 * Compares version numbers \a a and \a b and returns:
 * - 0 if the two versions are equal,
 * - a negative number if a < b,
 * - a positive number if a > b
 */


/*!
 * \fn bool operator ==(const VersionNumber& a, const VersionNumber& b)
 * \return true if versions are equal, false otherwise.
 */

/*!
 * \fn bool operator !=(const VersionNumber& a, const VersionNumber& b)
 * \return false if versions are equal, true otherwise.
 */

/*!
 * \fn bool operator <(const VersionNumber& a, const VersionNumber& b)
 * \return true if \p a is a smaller version than \p b, false otherwise.
 */

/*!
 * \fn bool operator <=(const VersionNumber& a, const VersionNumber& b)
 * \return true if \p a is a smaller version than \p b or if \p a equal \p b, false otherwise.
 */

/*!
 * \fn bool operator >(const VersionNumber& a, const VersionNumber& b)
 * \return true if \p a is a greater version than \p b, false otherwise.
 */

/*!
 * \fn bool operator >=(const VersionNumber& a, const VersionNumber& b)
 * \return true if \p a is a greater version than \p b or if \p a equal \p b, false otherwise.
 */

/*!
 * \fn VersionNumber& operator<<(VersionNumber& version, const oes::core::String& str)
 * Parses given string \p str and sets \p version attributes accordingly.
 * \return a reference on \p version
 */

/*!
 * \fn VersionNumber& operator>>(const oes::core::String& str, VersionNumber& version)
 * Parses given string \p str and sets \p version attributes accordingly.
 * \return a reference on \p version
 */

/*!
 * \fn const VersionNumber& operator>>(const VersionNumber& version, oes::core::String& str)
 * Prints the human readable form of \p version number in \p str.
 * \return a reference on \p version
 */

/*!
 * \fn const VersionNumber& operator<<(oes::core::String& str, const VersionNumber& version)
 * Prints the human readable form of \p version number in \p str.
 * \return a reference on \p version
 */

} // core
} // oes
