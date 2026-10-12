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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/process/io.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "ConsoleUtils.h"

#include <oes/core/utils/System.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>

#include <oes/testing/core/ScopedTimer.h>
#include <oes/testing/core/TestMain.h>


using namespace oes::core;
namespace but = boost::unit_test;

//****************************************************************************

namespace {

using namespace oes::testing::eml;

int millis() {
    auto tt = System::getRealTime();
    auto sec = std::floor(tt);
    const int millis = static_cast<int>((tt - sec) * 1000.0);
    return millis;
}

std::vector<std::string> toStringVector(const OESEmlRunner::ArgList& args) {
    std::vector<std::string> result;
    for (const auto& arg : args) {
        result.push_back(arg);
    }
    return result;
}

std::vector<std::string> mergeArguments(
    const OESEmlCredentials& creds,
    const std::vector<std::string>& arguments
) {
    OES_UNUSED(creds);
    std::vector<std::string> ext_arguments;
    ext_arguments = arguments;
    return ext_arguments;
}

std::string joinArguments(const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        return std::string(); // for EmlAppTestsNC/test_empty_cli basically
    }

    std::ostringstream oss;
    for (const auto& arg : arguments) {
        if (arg.empty()) {
            oss << '"' << '"' << ' ';
            continue;
        }
        if (arg.find_first_of(" \t\n'\"\\", 0) != std::string::npos) {
            // needs special quoting/escaping.  will be decoded by
            // po::split_unix() in oes_eml's main.cpp interactive_loop()
            oss << '"';
            for (char c : arg) {
                switch (c) {
                case '\t':  oss << '\\' << 't'; break;
                case '\n':  oss << '\\' << 'n'; break;
                case '\\':  oss << '\\' << '\\'; break;
                case '"':   oss << '\\' << '"'; break;
                case '\'':  oss << '\\' << '\''; break;
                default:    oss << c; break;
                }
            }
            oss << '"' << ' ';
        } else {
            oss << arg << ' ';
        }
    }

    auto cmdline = oss.str();
    BOOST_REQUIRE(!cmdline.empty());
    cmdline.resize(cmdline.size() - 1);
    return cmdline;
}

} // namespace

namespace oes::testing::eml {

//****************************************************************************

bool OESEmlCredentials::valid() const {
    return !username.empty();
}

bool OESEmlCredentials::equals(const OESEmlCredentials& rhs) const {
    return
        username     == rhs.username     &&
        passwd       == rhs.passwd       &&
        database     == rhs.database     &&
        ds_title     == rhs.ds_title     &&
        active_group == rhs.active_group
    ;
}

//****************************************************************************

class OESEmlRunner::UtObserver : public but::test_observer {
public:
    void test_unit_start(const but::test_unit& /*ut*/) override {
        test_ok_ = true;
    }

#if BOOST_VERSION >= 106300
    void assertion_result(but::assertion_result passed) override {
        if (passed == but::AR_FAILED) {
            test_ok_ = false;
        }
    }
#else
    virtual void assertion_result(bool passed) override {
        if (!passed) {
            test_ok_ = false;
        }
    }
#endif
    bool test_ok_ = true;
};

//****************************************************************************

/*!
 * \brief Gets the default timeout for OESEmlRunner.
 *
 * If the forked oes_eml used but the runner has not existed
 * after the given timeout, it is automatically terminated forcibly.
 *
 * \return 20 seconds in release builds; 1 minute in debug builds.
 */
int OESEmlRunner::defaultTimeout() {
    return releaseTimeout(20 * one_second);
}

/*!
 * \brief Gets debug and release timeout for OESEmlRunner, given the release one.
 *
 * Assumes debug builds are a constant-factor 3x slower than release builds.
 *
 * \param timeout_msecs the timeout value for release builds, in milliseconds.
 * \return timeout_msecs for release builds; 3*timeout_msecs for debug builds.
 * \note use the one_second and one_minute constants, to convey proper "time units".
 */
int OESEmlRunner::releaseTimeout(int timeout_msecs) {
#if defined (OES_DEBUG_BUILD)
    return timeout_msecs * 3; // assume Debug 3x slower than release
#else
    return timeout_msecs;
#endif
}

/*!
 * \brief Helper class to run oes_eml in a sub-process, capturing output and errors.
 *
 * \param timeout_msecs (Optional; defaults to 10s) the timeout for this runner.
 *
 * \warning if this runner is reused to start several processes, sequentially,
 *     the timeout applies to this runner, and not each started process.
 */
OESEmlRunner::OESEmlRunner(
    int timeout_msecs
) : openETPServer_path_(OPEN_ETP_SERVER_PATH)
  , timeout_msecs_(timeout_msecs)
  , process_(nullptr)
  , output_observer_thread_(nullptr)
  , process_stdin_(nullptr)
  , process_stdout_(nullptr)
  , process_stderr_(nullptr)
  , ut_observer_(nullptr)
  
{
    if (Logger::instance().minSeverityFilter() > Message::Severity::debug) {
        ut_observer_ = new UtObserver;
        but::framework::register_observer(*ut_observer_);
    }
}

namespace {

void splitLines(const std::string& buffer, std::vector<std::string>& lines) {
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    std::string temp;
    while ((pos = buffer.find('\n', prev)) != std::string::npos)
    {
        temp = buffer.substr(prev, pos - prev);
        if (!temp.empty() && temp.back() == '\r' ){
            temp.pop_back();
        }
        if (!temp.empty()) {
            lines.push_back(temp);
        }
        prev = pos + 1;
    }

    // To get the last substring (or only, if delimiter is not found)
    if (!buffer.substr(prev).empty()) {
        lines.push_back(buffer.substr(prev));
    }
};

String logActualExpected(
    const std::vector<std::string>& gotLines,
    const std::vector<std::string>& wantLines
) {
    std::stringstream s;
    s << fmt::format("\n= Have: {}\n", gotLines.size());
    for (const auto& g : gotLines) {
        s << fmt::format("{}\n", g);
    }
    s << fmt::format("\n= Wanted: {}\n", wantLines.size());
    for (const auto& w : wantLines) {
        s << fmt::format("{}\n", w);
    }
    return s.str();
}
std::vector<std::string> trimLogging(
    const std::vector<std::string>& rawGotLines
) {
    // remove logging info before comparison
    std::vector<std::string> trimmedLines;
    for (const auto& rg : rawGotLines) {
        // [2022-Nov-24 17:21:50]
        size_t found = rg.find(']');
        if (found == std::string::npos) {
            trimmedLines.push_back(rg);
            continue;
        }
        auto f = rg.substr(found);
        // Warning: or Debug: etc.
        found = f.find(": ");
        if (found == std::string::npos) {
            trimmedLines.push_back(f);
            continue;
        }
        if(f.size() < found + 2){
            trimmedLines.push_back(f.substr(found + 1));
            continue;
        }
        trimmedLines.push_back(f.substr(found + 2));
    }
    return trimmedLines;
}

} // namespace

OESEmlRunner::~OESEmlRunner() {
    // in case test failed, and logger is setup to hide debug message,
    // force outputting those debug message
    if (ut_observer_) {
        but::framework::deregister_observer(*ut_observer_);
        if (!ut_observer_->test_ok_) {
            // Doesn't work for the console, so output all as Warning
            //         auto last_severity = Logger::instance().minSeverityFilter();
            //         Logger::instance().setSeverityFilter(Message::debug);
            OES_WARNING("EML> openETPServer {}", last_parameters_);
            if (!output_.empty() && !error_.empty()) {
                OES_WARNING("The following output is sorted by cout/cerr:");
            }
            for (const auto& line : output_) {
                OES_WARNING("OUT>{}", line);
            }
            for (const auto& line : error_) {
                OES_WARNING("ERR>{}", line);
            }
        }
        delete ut_observer_;
    }

    // Avoid QProcess: Destroyed while process is still running.
    if (process_ != nullptr) {
        process_->terminate();
        process_->wait();
        if (output_observer_thread_->joinable())
        {
            output_observer_thread_->join();
        }
        delete process_;
        process_ = nullptr;
        delete output_observer_thread_;
        output_observer_thread_ = nullptr;
    }
}

bool OESEmlRunner::isRunning() const {
    return process_ != nullptr && process_->running();
}

bool OESEmlRunner::start(const ArgList& args) {
    return doStartAndWait(toStringVector(args), 0, true);
}

bool OESEmlRunner::start(const OESEmlCredentials& creds, const ArgList& args) {
    return doStartAndWait(creds, toStringVector(args), 0, true);
}

bool OESEmlRunner::startAndWait(const ArgList& args) {
    return doStartAndWait(toStringVector(args), 0);
}

bool OESEmlRunner::startAndWaitExpecting(int expected_return, const ArgList& args) {
    return doStartAndWait(toStringVector(args), expected_return);
}

bool OESEmlRunner::startAndWait(const OESEmlCredentials& creds, const ArgList& args) {
    return doStartAndWait(creds, toStringVector(args), 0);
}

bool OESEmlRunner::startAndWaitExpecting(
    int expected_return, const OESEmlCredentials& creds, const ArgList& args
) {
    return doStartAndWait(creds, toStringVector(args), expected_return);
}

bool OESEmlRunner::waitForFinished(int time_out_sec) {
    if (process_ == nullptr) {
        return true;
    }
    try {
        return process_->wait_for(std::chrono::seconds(time_out_sec));
    } catch (const boost::process::process_error&) {
        return true;
    }
}

void OESEmlRunner::kill() {
    if (process_ != nullptr) {
        process_->terminate();
        process_->wait();
        output_observer_thread_->join();
    }
}

bool OESEmlRunner::runScript(const std::vector<ArgList>& cmds_args) {
    OESEmlCredentials no_creds;
    return runScript(no_creds, cmds_args);
}

bool OESEmlRunner::runScript(
    const OESEmlCredentials& creds, const std::vector<ArgList>& cmds_args
) {
    for (const auto& args : cmds_args) {
        if (!startAndWait(creds, args)) {
            return false;
        }
        BOOST_REQUIRE(noErrors());
    }
    return true;
}

bool OESEmlRunner::doStartAndWait(
    const OESEmlCredentials& creds,
    const std::vector<std::string>& arguments,
    int expected_return,
    bool wait_for_started
) {
    const auto& merged_args = mergeArguments(creds, arguments);
    return doStartAndWait(merged_args, expected_return, wait_for_started);
}

bool OESEmlRunner::doStart(const std::vector<std::string>& arguments) {
    outBuf_.clear();
    errBuf_.clear();
    output_.clear();
    error_.clear();

    // BUG: in some build environments (where the test executables are not
    // located in the same directory as the regular executables), the
    // first attempt to execute oes_eml always fail, even if oes_eml can
    // be found in the PATH, and even if we supply the full path to
    // oes_eml!!!
    //
    // In such environments, a workaround consists in:
    // - running "oes_eml" without leading path, which will fail
    // - retrying with the full path to oes_eml, which succeeds
    //
    // This is a dirty workaround, but theres nothing better so far
    // TODO: find a real fix

    // By default, execute with a "short" name.
    // If this didn't work the first time, the absolute path that *did*
    // work, will be recorded in this variable, for subsequent executions.

    // Log the exact command-line we're about to execute
    // Should quote args with inner-spaces?
    const std::string& args_str = joinArguments(arguments);
    OES_DEBUG("EML.{:03}> {} {}", millis(), openETPServer_path_, args_str);
    last_parameters_ = args_str;

    process_stdin_ = new boost::process::opstream();
    process_stdout_ = new boost::process::ipstream();
    process_stderr_ = new boost::process::ipstream();
    process_ = new boost::process::child(
        openETPServer_path_ + " " + args_str,
        boost::process::std_in < *process_stdin_,
        boost::process::std_out > *process_stdout_,
        boost::process::std_err > *process_stderr_
    );
    output_observer_thread_ = new std::thread(&OESEmlRunner::listen_to_output, this);
    // TODO: this may be different than waiting for process to start.
    return process_->valid() && process_->joinable();
}

bool OESEmlRunner::listen_to_output() {
    std::string line;
    while (this->isRunning()) {
        if (!this->process_stdout_->eof()) {
            line.clear();
            std::getline(*(this->process_stdout_), line);
            line += "\n";
            outBuf_.append(line);
            handleRead(line, true);
        }

        // Dynamic reading from stderr stream causes deadlock on Windows
        // ToDo: retest after upgrade Boost lib
#ifndef WIN32
        if (!this->process_stderr_->eof()) {
            line.clear();
            std::getline(*(this->process_stderr_), line);
            line += "\n";
            errBuf_.append(line);
            handleRead(line, false);
        }
#endif
    }
    if (this->process_stdout_ != nullptr) {
        while (this->process_stdout_->good()) {
            line.clear();
            std::getline(*(this->process_stdout_), line);

            line += "\n";
            outBuf_.append(line);
            handleRead(line, true);
        }
    }
    if (this->process_stderr_ != nullptr) {
        while (this->process_stderr_->good()){
            line.clear();
            std::getline(*(this->process_stderr_), line);
            line += "\n";
            errBuf_.append(line);
            handleRead(line, false);
        }
    }
    return 0;
}

bool OESEmlRunner::doStartAndWait(
    const std::vector<std::string>& arguments, int expected_return, bool wait_for_started
) {
    const auto start = oes::testing::TimerStamp::now();

    bool result = true;
    int process_exit_code = 0;

    const bool started = doStart(arguments);
    BOOST_REQUIRE_MESSAGE(started, "Process failed to start");

    if (wait_for_started) {
        return started;
    }
    if (process_ == nullptr) {
        return false;
    }
    if (process_->running()) {
        try {
            result = process_->wait_for(std::chrono::milliseconds(timeout_msecs_));
        } catch (const boost::process::process_error&) {
            result = true;
        }
    } else {
        result = true;
    }
    if (result) {
        output_observer_thread_->join();
    } else {
        this->kill();
    }
    process_exit_code = result? process_->exit_code(): 99;

    oes::testing::TimerDuration duration(start);
    std::stringstream ss;
    ss << duration;
    OES_DEBUG(
        "EML.{:03}: {}, rc={} (want {}), in {}, timeout@{}s"
        , millis(), (result ? "ok" : "KO"), process_exit_code, expected_return
        , ss.str(), (timeout_msecs_ / one_second));

    if (expected_return == 0 && expected_return != process_exit_code) {
        // force output of error message from oes_eml, to help debugging
        OES_ERROR("oes_eml raised errors:\n{}", errBuf_);
    }
    BOOST_REQUIRE_EQUAL(expected_return, process_exit_code);
    BOOST_REQUIRE_MESSAGE(result, "Timed out, errored out, or already finished;"
        " after " << duration << " (timeout@" << (timeout_msecs_/one_second) << "s)"
        "; while running\noes_eml " << joinArguments(arguments)
        << "\nin test " << boost::unit_test::framework::current_test_case().p_name);

    splitStreams();
    return true;
}

void OESEmlRunner::splitStreams() {
    // Because a single line of output can be broken across several
    // onReadReadOut()/Err() callbacks, we split-to-lines at the end only.

    splitLines(outBuf_, output_);
    splitLines(errBuf_, error_);
}

void OESEmlRunner::handleRead(const std::string& input, bool isStdout) {
    OES_DEBUG(
        "{}.{:03}: ({} chars)\n{}{}"
            , (isStdout ? "OUT" : "ERR")
            , millis()
            , input.length()
            , input
            , ((!input.empty() && input.back() == '\n') ? "" : "<NO NEWLINE>")
    );
}

/*!
 * \brief Determines whether running oes_eml generated no output on stderr.
 * \return true if no messages printed on stderr; false otherwise.
 */
bool OESEmlRunner::noErrors() const {
    return error_.empty();
}

/*!
 * \brief Determines whether running oes_eml generated an output on stderr.
 * \return true if at least 1 message is printed on stderr; false otherwise.
 */
bool OESEmlRunner::hasErrors() const {
    return !error_.empty();
}

/*!
 * \brief Determines whether running oes_eml generated no output on stdout.
 * \return true if no messages printed on stdout; false otherwise.
 */
bool OESEmlRunner::noOutput() const {
    return output_.empty();
}

/*!
 * \brief Convenience equivalent to lineMatches(error_, tostd::string(regexp)).
 *
 * \param regexp the regular expression (boost::regex syntax).
 * \return true if any error line matches \a regexp; false otherwise.
 * \sa lineMatches for details.
 */
bool OESEmlRunner::errorLineMatches(const std::string& regexp) const {
    return lineMatches(error_, regexp);
}

/*!
 * \brief Convenience equivalent to lineMatches(error_, ...).
 *
 * \param regexp the regular expression (boost::regex syntax), provided
 *     in pieces, joined (without a separator), and converted to std::string.
 * \return true if any error line matches \a regexp; false otherwise.
 * \sa lineMatches for details.
 */
bool OESEmlRunner::errorLineMatches(const std::vector<std::string>& regexp) const {
    return lineMatches(error_, boost::algorithm::join(regexp, ""));
}

/*!
 * \brief Convenience equivalent to lineMatches(output_, tostd::string(regexp)).
 *
 * \param regexp the regular expression (boost::regex syntax).
 * \return true if any output line matches \a regexp; false otherwise.
 * \sa lineMatches for details.
 */
bool OESEmlRunner::outputLineMatches(const std::string& regexp) const {
    return lineMatches(output_, regexp);
}

/*!
 * \brief Convenience equivalent to lineMatches(output_, ...).
 *
 * \param regexp regexp the regular expression (boost::regex syntax), provided
 *     in pieces, joined (without a separator), and converted to std::string.
 * \return true if any error line matches \a regexp; false otherwise.
 * \sa lineMatches for details.
 */
bool OESEmlRunner::outputLineMatches(const std::vector<std::string>& regexp) const {
    return lineMatches(output_, boost::algorithm::join(regexp, ""));
}

namespace {

boost::regex toRegexp(const std::string& regexp) {
    // TODO: check this !
    try {
        boost::regex re(regexp);
        return re;
    } catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Invalid RegExp: " << regexp);
    }
    return boost::regex();
}

bool lineMatch(const std::string& line, const boost::regex& regexp) {
    // trim the line of leading/trailing whitespace, mostly to get rid
    // of trailing \r on Windows, to have portable ^ and $ anchoring.
    // TODO: remove the trimmed(), should not be required anymore
    return boost::regex_match(boost::algorithm::trim_copy(line), regexp);
}

} // namespace

/*!
 * \brief Determines whether any line matches a regular expression.
 *
 * \param lines the lines to match. Typically output_ or error_.
 * \param regexp the regexp the regular expression (boost::regex syntax).
 * \return true if a line from \a lines matches \a regexp; false otherwise.
 *
 * \note trims (using std::string::trimmed()) the lines of leading/trailing
 *     whitespace before checking against the \a regexp.
 *     Avoids Windows/Linux line termination issues.
 *
 * \note To ensure the line matches exactly \a regexp, and not merely contains
 *     some text that matches, anchor the regular expression with a leading
 *     caret (^) and trailing dollar ($), to also match the line's start/end.
 */
bool OESEmlRunner::lineMatches(const std::vector<std::string>& rawLines, const std::string& regexp) const {
    // remove logging info before comparison
    std::vector<std::string> lines = trimLogging(rawLines);
    boost::regex re(toRegexp(regexp));
    for (const auto& line : lines) {
        if (lineMatch(line, re)) {
            return true;
        }
    }
    OES_DEBUG(logActualExpected(lines, {regexp}));
    return false;
}

bool OESEmlRunner::errorHasLines(
    const std::vector<std::string>& wantLines, bool in_that_order
) const {
    return hasLines(error_, wantLines, in_that_order, false);
}

bool OESEmlRunner::outputHasLines(
    const std::vector<std::string>& wantLines, bool in_that_order
) const {
    return hasLines(output_, wantLines, in_that_order, false);
}

bool OESEmlRunner::outputLinesMatch(
    const std::vector<std::string>& regexps
) const {
    return hasLines(output_, regexps, true/*order*/, true);
}

bool OESEmlRunner::hasLines(
    const std::vector<std::string>& rawGotLines,
    const std::vector<std::string>& wantLines,
    bool in_that_order,
    bool as_regexp
) const {
    BOOST_REQUIRE(wantLines.size() > 0);
    // regexp + any order not implemented
    BOOST_REQUIRE(!as_regexp || in_that_order);

    // remove logging info before comparison
    std::vector<std::string> gotLines = trimLogging(rawGotLines);

    unsigned int i = 0;
    if (as_regexp) {
        boost::regex re(toRegexp(wantLines[0]));
        for (; i < gotLines.size(); i++) {
            if (lineMatch(gotLines[i], re)) {
                break;
            }
        }
    } else {
        for (; i < gotLines.size(); i++) {
            if (gotLines[i] == wantLines[0]) {
                break;
            }
        }
    }
    if (i == gotLines.size()) {
        //OES_DEBUG("No line matches 1st {}: wanted: {}, have: {}", (as_regexp ? "REGX" : "LINE"), wantLines[0], gotLines[0]);
        OES_DEBUG(logActualExpected(gotLines, wantLines));
        return false;
    }
    if (in_that_order) {
        if ((gotLines.size() - i) < wantLines.size()) {
            OES_DEBUG(
                "Not enough lines ({} - {}) to match Regexes ({})"
                , gotLines.size(), i, wantLines.size()
            );
            OES_DEBUG(logActualExpected(gotLines, wantLines));
            return false; // missing lines for sure
        }
        for (unsigned int j = 0; j < wantLines.size(); ++j, ++i) {
            if (as_regexp) {
                boost::regex re(toRegexp(wantLines[j]));
                if (!lineMatch(gotLines[i], re)) {
                    OES_DEBUG(
                        "Line#{} does not match Regex#{}\nLINE: {}\nREGX: {}"
                        , i, j, gotLines[i], wantLines[j]
                    );
                    //remove
                    OES_DEBUG(logActualExpected(gotLines, wantLines));
                    return false;
                }
            } else {
                if (gotLines[i] != wantLines[j]) {
                    OES_DEBUG(
                        "Line#{} does not match Line#{}\nLINE: {}\nLINE: {}"
                        , i, j, gotLines[i], wantLines[j]
                    );
                    //remove
                    OES_DEBUG(logActualExpected(gotLines, wantLines));
                    return false;
                }
            }
        }
    } else {
        std::vector<std::string> sortedLines = gotLines;
        std::sort(sortedLines.begin(), sortedLines.end());
        for (const auto& wantLine : wantLines) {
            auto found = std::binary_search(sortedLines.begin(), sortedLines.end(), wantLine);
            if (!found) {
                OES_DEBUG(logActualExpected(sortedLines, wantLines));
                return false;
            }
        }
    }
    return true;
}

//****************************************************************************

OESEmlRunnerInputPasswd::OESEmlRunnerInputPasswd(
    const std::string& passwd, const std::string& prompt, int timeout_msecs
) : OESEmlRunner(timeout_msecs)
  , passwd_(passwd)
  , prompt_(prompt)
{
}

OESEmlRunnerInputPasswd::OESEmlRunnerInputPasswd(
    const std::string& passwd, const std::string& prompt,
    const std::string& repeat_prompt, int timeout_msecs
) : OESEmlRunner(timeout_msecs)
  , passwd_(passwd)
  , prompt_(prompt)
  , repeat_(repeat_prompt)
{
}

void OESEmlRunnerInputPasswd::handleRead(const std::string& input, bool isStdout) {
    if (isStdout && (
            input.find(prompt_) != std::string::npos ||
            (!repeat_.empty() && (input.find(repeat_) != std::string::npos))
        )
    ) {
        (*process_stdin_) << passwd_.data() << std::endl;
    }
    OESEmlRunner::handleRead(input, isStdout);
}

//*****************************************************************************

} // namespace oes::testing::eml
