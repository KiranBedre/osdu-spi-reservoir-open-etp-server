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
#pragma once

#include "export.h"
#include <oes/postgresql/PhysicalName.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/process/child.hpp>
#include <boost/process/pipe.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <thread>

namespace oes::testing::eml {

const int one_second = 1000;
const int one_minute = 60 * one_second;

//****************************************************************************

struct OES_TESTING_EML_EXPORT OESEmlCredentials {
    oes::postgresql::relational::PhysicalName username;
    std::string passwd; // may be empty
    std::string database;
    std::string ds_title; // empty if no dataspace
    std::string active_group; // may be empty

    OESEmlCredentials() = default;

    bool valid() const;
    bool equals(const OESEmlCredentials& rhs) const;
};

inline bool operator==(const OESEmlCredentials& lhs, const OESEmlCredentials& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const OESEmlCredentials& lhs, const OESEmlCredentials& rhs) {
    return !lhs.equals(rhs);
}

//****************************************************************************

class OES_TESTING_EML_EXPORT OESEmlRunner {

public:
    struct StringWrapper : public std::string {
        StringWrapper(const std::string& str) : std::string(str) {} //cppcheck-suppress noExplicitConstructor
        StringWrapper(const char* str) : std::string(str) {} //cppcheck-suppress noExplicitConstructor
        // TODO: better to use sqlName, and in few location getRaw to validate
        //   oes_eml accept to be case unsensitive by default
        explicit StringWrapper(const oes::postgresql::relational::PhysicalName& str) : std::string(str.getRaw()) {}
    };
    using ArgList    = std::vector<StringWrapper>;
    using ArgListVec = std::vector<ArgList>;

public:
    static int defaultTimeout();
    static int releaseTimeout(int timeout_msecs);

    virtual ~OESEmlRunner();
    explicit OESEmlRunner(int timeout_msecs = defaultTimeout());

    OESEmlRunner(const OESEmlRunner&) = delete;
    OESEmlRunner& operator=(const OESEmlRunner&) = delete;

    bool isRunning() const;

    bool start(const ArgList& args);
    bool start(const OESEmlCredentials& creds, const ArgList& args);

    bool waitForFinished(int time_out_sec = 10);
    void kill();

    bool startAndWait(const ArgList& args);
    bool startAndWait(const OESEmlCredentials& creds, const ArgList& args);
    bool startAndWaitExpecting(int expected_return, const ArgList& args);
    bool startAndWaitExpecting(
        int expected_return, const OESEmlCredentials& creds, const ArgList& args
    );

    bool runScript(const ArgListVec& cmds_args);
    bool runScript(const OESEmlCredentials& creds, const ArgListVec& cmds_args);

    bool noErrors() const;
    bool hasErrors() const;
    bool noOutput() const;

    bool errorLineMatches(const std::string& regexp) const;
    bool errorLineMatches(const std::vector<std::string>& regexp) const;
    bool errorHasLines(
        const std::vector<std::string>& wantLines, bool in_that_order = true
    ) const;

    bool outputLineMatches(const std::string& regexp) const;
    bool outputLineMatches(const std::vector<std::string>& regexp) const;
    bool outputHasLines(
        const std::vector<std::string>& wantLines, bool in_that_order = true
    ) const;
    bool outputLinesMatch(
        const std::vector<std::string>& regexps/*, bool in_that_order = true*/
    ) const;

    bool lineMatches(const std::vector<std::string>& lines, const std::string& regexp) const;
    bool hasLines(
        const std::vector<std::string>& gotLines,
        const std::vector<std::string>& wantLines,
        bool in_that_order,
        bool as_regexp
    ) const;

private:
    // std::string-based
    bool doStart(
        const std::vector<std::string>& arguments
    );
    bool doStartAndWait(
        const std::vector<std::string>& arguments,
        int expected_return = 0,
        bool wait_for_started = false
    );
    bool doStartAndWait(
        const OESEmlCredentials& creds,
        const std::vector<std::string>& arguments,
        int expected_return = 0,
        bool wait_for_started = false
    );

    void splitStreams();
public:
    std::string openETPServer_path_;
    int timeout_msecs_;

    boost::process::child* process_;

    std::thread* output_observer_thread_;
    boost::process::opstream *process_stdin_;
    boost::process::ipstream *process_stdout_;
    boost::process::ipstream *process_stderr_;

    std::string outBuf_;
    std::string errBuf_;
    std::vector<std::string> output_;
    std::vector<std::string> error_;

    // for debugging
    std::string last_parameters_;
    class UtObserver;
    UtObserver* ut_observer_;
    //

    virtual void handleRead(const std::string& /*input*/, bool isStdout);

    bool listen_to_output();
};

//****************************************************************************

class OESEmlRunnerInputPasswd : public OESEmlRunner {
public:
    explicit OESEmlRunnerInputPasswd(
        const std::string& passwd, const std::string& prompt = "Password:",
        int timeout_msecs = 10000
    );

    OESEmlRunnerInputPasswd(
        const std::string& passwd, const std::string& prompt, const std::string& repeat_prompt,
        int timeout_msecs = 10000
    );

    void handleRead(const std::string& input, bool isStdout) override;

private:
    std::string passwd_;
    std::string prompt_;
    std::string repeat_;
};

//****************************************************************************

} // namespace oes::testing::eml
