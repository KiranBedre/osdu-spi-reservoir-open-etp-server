/*-----------------------------------------------------------------------
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"; you may not use this file except in compliance
with the License.  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
-----------------------------------------------------------------------*/

#include "ContentType.h"

#include <cassert>
#include <cstring>
#include <sstream>
#include <tuple>

namespace oes::common::epc {

ContentType::ContentType(
    bool isAssociatedToAnExtension,
    std::string contentType,
    std::string extensionOrPartName
) : contentTypeString_(std::move(contentType))
  , extensionOrPartName_(std::move(extensionOrPartName))
  , isAssociatedToAnExtension_(isAssociatedToAnExtension)
{
    const char first = *extensionOrPartName_.c_str(); // \0 if empty

    if (!isAssociatedToAnExtension) {
        // Add the first character as slash if we forgot to add it
        if (first != '/') {
            extensionOrPartName_.insert(0, 1, '/'); // prepend a slash
        }
    } else {
        // delete star and dot from extension if we forgot not to give them for an extension
        if (first == '*' || first == '.') {
            extensionOrPartName_.erase(0, 1); // drop the leading char
        }
    }
}

const std::string& ContentType::getContentTypeString() const {
    return contentTypeString_;
}

const std::string& ContentType::getExtensionOrPartName() const {
    return extensionOrPartName_;
}

void ContentType::setContentTypeString(std::string ctString) {
    contentTypeString_ = std::move(ctString);
}

void ContentType::setExtensionOrPartName(std::string extOrPartName) {
    extensionOrPartName_ = std::move(extOrPartName);
}

std::string ContentType::toString() const {
    std::ostringstream oss;
    print(oss);
    return oss.str();
}

std::ostream& ContentType::print(std::ostream& os) const {
    if (isAssociatedToAnExtension_) {
        return os << "<Default Extension=\"" << extensionOrPartName_
            << "\" ContentType=\"" << contentTypeString_ << "\" />";
    } else {
        return os << "<Override PartName=\"" << extensionOrPartName_
            << "\" ContentType=\"" << contentTypeString_ + "\"/>";
    }
}

bool ContentType::equals(const ContentType& rhs) const {
    const auto& lhs = *this;
    return
        std::tie(lhs.isAssociatedToAnExtension_, lhs.contentTypeString_, lhs.extensionOrPartName_) ==
        std::tie(rhs.isAssociatedToAnExtension_, rhs.contentTypeString_, rhs.extensionOrPartName_)
    ;
}

bool ContentType::parse(
    const std::string& contentType,
    std::string& ml, std::string& format, std::string& version, std::string& type
) {
    const char* s = contentType.c_str();

    // format is: application/x-$ML+$FORMAT;version=$VERSION;type=$TYPE
    const char prefix[] = "application/x-";
    const size_t prefix_len = sizeof(prefix) - 1;
    if (0 != std::strncmp(s, prefix, prefix_len)) {
        return false;
    }
    s += prefix_len;

    const char* plus = std::strchr(s, '+');
    if (!plus) {
        return false;
    }
    ml.assign(s, (plus - s));
    s = plus + 1;

    const char* semi = std::strchr(s, ';');
    if (!semi) {
        return false;
    }
    format.assign(s, (semi - s));
    s = semi;

    const char vkey[] = ";version=";
    const size_t vkey_len = sizeof(vkey) - 1;
    if (0 != std::strncmp(s, vkey, vkey_len)) {
        return false;
    }
    s += vkey_len;

    semi = std::strchr(s, ';');
    if (!semi) {
        return false;
    }
    version.assign(s, (semi - s));
    s = semi;

    const char tkey[] = ";type=";
    const size_t tkey_len = sizeof(tkey) - 1;
    if (0 != std::strncmp(s, tkey, tkey_len)) {
        return false;
    }
    s += tkey_len;
    const char* end = &contentType.back() + 1;
    type.assign(s, (end - s));
    assert((s + type.size()) == end);
    return true;
}

} // namespace oes::common::epc
