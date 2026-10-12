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

#include <oes/testing/core/LoggerTestTools.h>

#include <oes/core/utils/Assert.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::core;

namespace {

struct FilterExpectedMessage : public MessageFilter {
    FilterExpectedMessage(
        Message::Severity severity,
        const String& message,
        int expectedCount
    ) : severity_(severity)
      , message_(message)
      , expression_(message)
      , actualCount_(0)
      , expectedCount_(expectedCount)
    {
    }

    bool filter(const Message& message) const override {
        if (message.severity() != severity_) {
            return false;
        }
        if (boost::regex_match(message.message(), expression_)) {
            ++actualCount_;
            return true;
        }
        return false;
    }

    ~FilterExpectedMessage() override {
        if (actualCount_ != expectedCount_) {
            std::ostringstream oss;
            oss << "Expected message \"" << message_ << "\" ("
                << expectedCount_ << "x, but got " << actualCount_ << "x)";
            // note: no BOOST_REQUIRE or BOOST_FAIL to avoid throwing 
            //    exception in dtor hence abort the application
            BOOST_CHECK_MESSAGE(false, oss.str());
        }
    }

private:
    Message::Severity severity_;
    std::string message_;
    boost::regex expression_;
    mutable int actualCount_;
    int expectedCount_;
};

} // namespace

namespace oes {
namespace testing {

/*! \class ScopedExpectedMessage oes/testing/core/LoggerTestTools.h
 * \brief Assert the Logger will receive excepted message.
 *
 * Use boost unit test to check the message has been received.
 *
 * \ingroup oes_testing_core
 */

/*!
 * \brief New instance of ScopedExpectedMessage.
 *
 * \param severity the expected severity.
 * \param regexp the expected message, as a regular expression.
 *     Does not include the (time, file and line) message prefix.
 * \param expectedCount (Optional; default to 1) how many times \a regex is
 *     supposed to match messages of the given \a severity
 */
ScopedExpectedMessage::ScopedExpectedMessage(
    Message::Severity severity,
    const String& regexp,
    int expectedCount
) : filter_(new FilterExpectedMessage(severity, regexp, expectedCount))
{
    Logger::instance().connectFilter(filter_);
}

/*!
 * \brief Calls BOOST_FAIL if message was not seen.
 */
ScopedExpectedMessage::~ScopedExpectedMessage() {
    Logger::instance().disconnectFilter(filter_);
}


/*****************************************************************************/

ScopedLoggerQuiet::ScopedLoggerQuiet(
) : min_severity_(Logger::instance().minSeverityFilter()) {
    Logger::instance().setSeverityFilter(Message::Severity::max_severity);
}

ScopedLoggerQuiet::~ScopedLoggerQuiet() {
    Logger::instance().setSeverityFilter(min_severity_);
}

} // namespace testing
} // namespace oes
