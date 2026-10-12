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

#include <oes/eml/OpenETPServerConfig.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/program_options.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <string>
#include <vector>
#include <fstream>
#include <utility>

namespace oes::apps::eml {

using boost::program_options::options_description;
using boost::program_options::positional_options_description;
using boost::program_options::parsed_options;
using boost::program_options::variables_map;

/****************************************************************************/

struct EmlAppContext;
using UnparsedArgs = std::vector<std::string>;

using EmlAppCommand = int (*)(EmlAppContext&, const UnparsedArgs&);
struct EmlAppNamedCommand { const char* name; EmlAppCommand command; const char* desc; };
using EmlAppNamedCommandVec = std::vector<EmlAppNamedCommand>;

using EmlAppAction = bool (*)(const EmlAppContext&);
struct EmlAppNamedAction { const char* name; EmlAppAction action; };

struct ActionFailed {}; // Error message already printed, just "abort" current command

/****************************************************************************/

struct EmlAppContext {
    variables_map& args;

    const bool help;
    const bool timer;

    const std::string command;

    const unsigned terminalWidth_;
    oes::eml::openETPServerConfig_ptr server_config_;

public:
    explicit EmlAppContext(variables_map& args, oes::eml::openETPServerConfig_ptr server_config);

    std::ostream& out() const;
    std::ostream& err() const;
    std::ostream& msg() const;
    std::ostream& dbg() const;

    bool hasArg(const std::string& name) const;
    std::string stringArg(const std::string& name) const;
    const oes::eml::openETPServerConfig& appConfig() const;
    oes::eml::openETPServerConfig_ptr appConfig_ptr() const;

    const std::string& action() const;

    int reparse(
        const options_description& cmd_desc,
        const std::vector<std::string>& unparsed_args
    );
    int reparse(
        const options_description& cmd_desc,
        const positional_options_description& pos_desc,
        const std::vector<std::string>& unparsed_args
    );

    int dispatchCommand(
        const parsed_options& parsed, const EmlAppNamedCommandVec& commands
    );
    int dispatchAction(const EmlAppNamedAction actions[], size_t action_count);
    int timeCall(const EmlAppNamedAction& action);
    int tryCall(const EmlAppNamedAction& action);

    template <size_t N>
    int dispatchActions(const EmlAppNamedAction (&actions)[N]) {
        return dispatchAction(actions, N);
    }

    static unsigned terminalWidth();

private: // no copy/assignment
    EmlAppContext(const EmlAppContext&) = delete;
    EmlAppContext& operator=(const EmlAppContext&) = delete;

private:
    std::string action_;
    mutable std::ofstream ofs_;
};

/****************************************************************************/

int hdf5_main   (EmlAppContext& ctx, const UnparsedArgs& unparsed_args);
int server_main (EmlAppContext& ctx, const UnparsedArgs& unparsed_args);
int space_main  (EmlAppContext& ctx, const UnparsedArgs& unparsed_args);
int probe_main  (EmlAppContext& ctx, const UnparsedArgs& unparsed_args);

/****************************************************************************/

#if (BOOST_VERSION >= 105900) && (BOOST_VERSION <= 106400)
template <typename T>
struct greedy_typed_value : public boost::program_options::typed_value<T> {
    explicit greedy_typed_value() : boost::program_options::typed_value<T>(nullptr) {}
    bool adjacent_tokens_only() const override { return false; }
    unsigned max_tokens() const override { return 1; }
};
template <typename T> boost::program_options::typed_value<T>* greedy_value() {
    return new greedy_typed_value<T>();
}
#else
template <typename T> boost::program_options::typed_value<T>* greedy_value() {
    return boost::program_options::value<T>();
}
#endif

/****************************************************************************/

inline boost::program_options::typed_value<std::string>* txtArg() {
    return boost::program_options::value<std::string>();
}

inline boost::program_options::typed_value<std::string>* txtArg(const std::string& default_value) {
    return txtArg()->default_value(default_value);
}

inline boost::program_options::typed_value<std::string>* txtArgOpt(const std::string& implicit_value = "") {
    return greedy_value<std::string>()->implicit_value(implicit_value);
}

inline boost::program_options::typed_value<int>* intArg() {
    return boost::program_options::value<int>();
}

inline boost::program_options::typed_value<int>* intArg(int default_value) {
    return boost::program_options::value<int>()->default_value(default_value);
}

/****************************************************************************/

std::string outputFilename(
    const EmlAppContext& ctx, const std::string& input,
    const char* dash_suffix = nullptr, const char* extension = nullptr
);

size_t parseByteSize(const std::string& spec);
std::pair<size_t, size_t> parseByteSizes(const std::string& spec);

/****************************************************************************/

}  // namespace oes::apps::eml
