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

#include <cassert>
#include <iostream>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#  include <Windows.h>
#  include <io.h>
#  define isatty(h) _isatty(h)
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include "EmlAppContext.h"
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/InputByteParser.h>


namespace po = boost::program_options;
using oes::apps::eml::EmlAppContext;

using std::endl;

namespace {

// http://stackoverflow.com/questions/6240950
std::ostream cnull(nullptr);

// See also http://stackoverflow.com/questions/16935908
//      and http://stackoverflow.com/questions/6812224
std::pair<unsigned, unsigned> getTerminalLineAndColumnSizes() {
    std::pair<unsigned, unsigned> result{ 0, 0 };

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (0 != GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        result.first  = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
        result.second = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
    }
#else
    struct winsize w;
    if (-1 != ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) {
        result.first = w.ws_row;
        result.second = w.ws_col;
    }
#endif

    if (result.first == 0) {
        result.first  = 24;
    } else {
        result.first  = std::min(std::max(result.first,   4u), 256u); // clamp( 4, 256)
    }

    if (result.second == 0) {
        result.second = po::options_description::m_default_line_length;
    } else {
        // Call succeeded. But values may be out of range.
        // Redirected output on Linux yields (6, 0) for example.
        result.second = std::min(std::max(result.second, 64u), 160u); // clamp(64, 160)
    }

    return result;
}

} // namespace

// FIXME: lots of copy/paste

namespace oes {
namespace apps {
namespace eml {

/****************************************************************************/

#ifdef OES_DEBUG_BUILD
bool test_parseByteSize() {
    auto assertOK = [](size_t expected, const char* spec) {
        assert(expected == oes::core::parseByteSize(spec, 2147483647));
    };
    assertOK(1, "1");
    assertOK(1, "1B");
    assertOK(1000, "1KB");
    assertOK(1024, "1KiB");
    assertOK(15*1000*1000, "15MB");
    assertOK(static_cast<size_t>(1.5*1000*1000), "1.5MB");
    assertOK(static_cast<size_t>(1.5*1024*1024), "1.5MiB");

    auto assertKO = [](const char* expected_err_fragment, const char* spec) {
        try {
            oes::core::parseByteSize(spec, 2147483647);
            assert(false);
        } catch (const std::runtime_error& ex) {
            const std::string err(ex.what());
            assert(err.find(expected_err_fragment) != std::string::npos);
        }
    };
    assertKO("Expected leading number", "foo");
    assertKO("Must end with B", "1K");
    assertKO("Invalid uom", "1FooB");
    assertKO("Invalid uom", "1FB");
    assertKO("Size too large", "3GB");
    assertKO("Size too large", "1TB");

    return true;
}
static const bool ok = test_parseByteSize();
#endif

std::pair<size_t, size_t> parseByteSizes(const std::string& spec) {
    std::vector<std::string> sizes;
    if (spec.empty()) {
        return { 0, 0 };
    }

    boost::split(
        sizes, spec, boost::algorithm::is_any_of("\t ,;|"),
        boost::algorithm::token_compress_on
    );

    size_t zsize = 0; // max compressed size
    size_t usize = 0; // max uncompressed size

    switch (sizes.size()) {
    case  1:
        zsize = oes::core::parseByteSize(sizes[0], 2147483647 /*2GiB*/);
        usize = 10 * zsize; // default to 10x the specified max-compressed-size
        break;
    case  2:
        zsize = oes::core::parseByteSize(sizes[0], 2147483647 /*2GiB*/);
        usize = oes::core::parseByteSize(sizes[1], 2147483647 /*2GiB*/);
        break;
    default:
        throw std::runtime_error(
            fmt::format("Expected 1 or 2 sizes, not {}", sizes.size())
        );
    }

    return { zsize, usize };
}

/****************************************************************************/

EmlAppContext::EmlAppContext(
    variables_map& args,
    oes::eml::openETPServerConfig_ptr server_config
) : args(args)
  , help(args.count("help") > 0)
  , timer(args.count("timer") > 0)
  , command(!args["command"].empty()? args["command"].as<std::string>(): std::string())
  , terminalWidth_(terminalWidth())
  , server_config_(server_config)
{
}

std::ostream& EmlAppContext::out() const {
    const std::string& filename = stringArg("output-file");
    if (!filename.empty()) {
        if (!ofs_.is_open()) {
            ofs_.open(filename.c_str());
        }
        if (ofs_.good()) {
            return ofs_;
        }
        OES_ERROR("openETPServer: Cannot open file: {}", filename);
        // fall thru
    }
    return oes::core::Logger::instance().minSeverityFilter() >= oes::core::Message::Severity::critical? cnull: std::cout;
}

std::ostream& EmlAppContext::err() const {
    return oes::core::Logger::instance().minSeverityFilter() >= oes::core::Message::Severity::critical? cnull: std::cerr;
}

std::ostream& EmlAppContext::msg() const {
    return oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::verbose? out() : cnull;
}

std::ostream& EmlAppContext::dbg() const {
    return oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug ? out() : cnull;
}

bool EmlAppContext::hasArg(const std::string& name) const {
    return args.count(name) > 0;
}

std::string EmlAppContext::stringArg(const std::string& name) const {
    const po::variable_value& value = args[name];
    return value.empty()? std::string(): value.as<std::string>();
}

const oes::eml::openETPServerConfig& EmlAppContext::appConfig() const {
    assert(server_config_);
    return *server_config_;
}

oes::eml::openETPServerConfig_ptr EmlAppContext::appConfig_ptr() const {
    assert(server_config_);
    return server_config_;
}

/*!
 * \brief Augments global options with command-specific options.
 *
 * \param cmd_desc the command-specific options.
 * \param unparsed_args the unparsed arguments to parse against \a cmd_desc.
 * \return 0 on success; -1 on success, but return immediately success (0);
 *     positive error code otherwise.
 */
int EmlAppContext::reparse(
    const options_description& cmd_desc,
    const std::vector<std::string>& unparsed_args
) {
    try {
        po::store(
            po::command_line_parser(unparsed_args)
            .options(cmd_desc)
            .run(),
            args
        );
    }
    catch (const std::exception& ex) {
        std::stringstream ex_buffer(std::string(ex.what()) + "\n");
        ex_buffer << cmd_desc;
        OES_ERROR("openETPServer: {}", ex_buffer.str());
        return 1; // error
    }

    if (args.size() == 0) {
        std::stringstream cmd_desc_buffer;
        cmd_desc_buffer << cmd_desc;
        OES_WARNING(cmd_desc_buffer.str());
        return -1; // success, but caller should return 0
    }

    if (help) {
        std::stringstream cmd_desc_buffer;
        cmd_desc_buffer << cmd_desc;
        OES_INFO(cmd_desc_buffer.str());
        return -1; // success, but caller should return 0
    }

    return 0; // success
}

/*!
 * \brief Augments global options with command-specific options.
 *
 * \param cmd_desc the command-specific options.
 * \param pos_desc the command-specific positional options (option with no name)
 * \param unparsed_args the unparsed arguments to parse against \a cmd_desc.
 * \return 0 on success; -1 on success, but return immediately success (0);
 *     positive error code otherwise.
 */
int EmlAppContext::reparse(
    const options_description& cmd_desc,
    const positional_options_description& pos_desc,
    const std::vector<std::string>& unparsed_args
) {
    try {
        po::store(
            po::command_line_parser(unparsed_args)
            .options(cmd_desc)
            .positional(pos_desc)
            .run(),
            args
        );
    }
    catch (const std::exception& ex) {
        std::stringstream ex_buffer(std::string(ex.what()) + "\n");
        ex_buffer << cmd_desc;
        OES_ERROR("openETPServer: {}", ex_buffer.str());
        return 1; // error
    }

    if (args.size() == 0) {
        std::stringstream cmd_desc_buffer;
        cmd_desc_buffer << cmd_desc;
        OES_WARNING(cmd_desc_buffer.str());
        return -1; // success, but caller should return 0
    }

    if (help) {
        std::stringstream cmd_desc_buffer;
        cmd_desc_buffer << cmd_desc;
        OES_INFO(cmd_desc_buffer.str());
        return -1; // success, but caller should return 0
    }

    return 0; // success
}

int EmlAppContext::dispatchCommand(
    const parsed_options& parsed, const EmlAppNamedCommandVec& commands
) {
    OES_ASSERT(!commands.empty(), return 1;);

    for (const EmlAppNamedCommand& entry : commands) {
        if (command == entry.name) {
            std::vector<std::string> command_args = po::collect_unrecognized(
                parsed.options, po::include_positional
            );

            // Remove the command itself from command_args.
            // Note that other unknown options can precede it.
            const size_t before_size = command_args.size();
            OES_ASSERT(before_size >= 1, /*do nothing*/;);
            for (size_t i = 0; i < command_args.size(); ++i) {
                if (command_args[i] == command) {
                    command_args.erase(command_args.begin() + i);
                    break;
                }
            }

            return entry.command(*this, command_args);
        }
    }

    return -1; // no match. Unknown command?
}

int EmlAppContext::dispatchAction(const EmlAppNamedAction actions[], size_t action_count) {
    OES_ASSERT(actions && action_count, return 1;);

    size_t pop_count = 0;
    const EmlAppNamedAction* p_action = nullptr;
    for (size_t i = 0; i < action_count; ++i) {
        const EmlAppNamedAction& action = actions[i];
        if (hasArg(action.name)) {
            ++pop_count;
            p_action = &action;
        }
    }
    switch (pop_count) {
    case  0:
        OES_ERROR("No action specified.");
        return 1;
    case  1:
        break;
    default:
        OES_ERROR("Too many actions specified.");
        return 2;
    }

    assert(p_action);
    return timeCall(*p_action);
}

const std::string& EmlAppContext::action() const {
    OES_ASSERT(!action_.empty(), ;);
    return action_;
}

int EmlAppContext::timeCall(const EmlAppNamedAction& entry) {
    const double user_start = oes::core::System::getUserTime();
    const double real_start = oes::core::System::getRealTime();

    const int rc = tryCall(entry);

    if (timer) {
        const double user = oes::core::System::getUserTime() - user_start;
        const double real = oes::core::System::getRealTime() - real_start;
        const std::string timing = fmt::format("{:.3f}s (user: {:.3f}s)", real, user);
        OES_WARNING("{} {} {} in {} {} peak memory."
            , command, (entry.name? " ": ""), (entry.name? entry.name: "")
            , timing, oes::core::System::getMemoryUsage().peakMB());
    }

    return rc;
}

int EmlAppContext::tryCall(const EmlAppNamedAction& entry) {
    try {
        if (entry.name) {
            action_ = entry.name;
        } else { // validate and compare commands have no action
            action_ = command;
        }
        return entry.action(*this)? 0: 1;
    } catch (ActionFailed) {
        return 1;
    } catch (const std::exception& ex) {
        OES_ERROR("openETPServer: {}", ex.what());
        return 3; // keep in sync with main()
    } catch (...) {
        OES_ERROR("openETPServer: Unknown Error");
        return 4; // keep in sync with main()
    }
}

unsigned EmlAppContext::terminalWidth() {
    return getTerminalLineAndColumnSizes().second;
}

/****************************************************************************/

} // namespace eml
} // namespace apps
} // namespace oes
