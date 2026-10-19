// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_TESTING_CORE_LOGGER_TEST_TOOLS_H
#define OES_TESTING_CORE_LOGGER_TEST_TOOLS_H

#include <oes/testing/core/export.h>

#include <oes/core/utils/Logger.h>

namespace oes {
namespace testing {

class OES_TESTING_CORE_EXPORT ScopedExpectedMessage {
public:
    ScopedExpectedMessage(
        oes::core::Message::Severity severity,
        const oes::core::String& regexp,
        int expectedCount = 1
    );
    ~ScopedExpectedMessage();

private: // no copy/assignment
    ScopedExpectedMessage(const ScopedExpectedMessage&);
    ScopedExpectedMessage& operator=(const ScopedExpectedMessage&);

private:
    oes::core::MessageFilter_ptr filter_;
};


/*****************************************************************************/

class OES_TESTING_CORE_EXPORT ScopedLoggerQuiet {
public:
    ScopedLoggerQuiet();
    ~ScopedLoggerQuiet();

private:
    oes::core::Message::Severity min_severity_;
};

} // namespace testing
} // namespace oes

#endif
