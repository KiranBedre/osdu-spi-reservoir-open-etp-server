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

#ifndef OES_CORE_UTILS_SYSTEM_H
#define OES_CORE_UTILS_SYSTEM_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/String.h>

#include <cstdint> // for int64_t

namespace oes {
namespace core {

/****************************************************************************/

class OES_CORE_UTILS_EXPORT System {
public:
    static void setEnv(const String& name, const String& value);
    static String getEnv(const String& name);
    static bool getBoolEnv(const String& name, bool value_on_invalid = false);
    static String getUserName();
    static String getHostName();

    static void getSearchPath(StringList& path, const String& name);
    static void splitSearchPath(StringList& path, const String& value);
    static String joinSearchPath(const StringList& path);

    static void sleep(int milliseconds);

    static double getRealTime();
    static double getUserTime();

    struct OES_CORE_UTILS_EXPORT MemoryUsed {
        MemoryUsed();

        int64_t peak_bytes;
        int64_t virtual_bytes;
        int64_t resident_bytes;

        std::string peakMB() const;
    };
    static MemoryUsed getMemoryUsage();
};

/****************************************************************************/

} // namespace core
} // namespace oes

#endif

