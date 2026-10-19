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

#include <iostream>
#include <memory>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/program_options/options_description.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/config.h> // for nullptr
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Otel.h>
#include <oes/eml/OpenETPServerConfig.h>

#include "EmlAppContext.h"


namespace po = boost::program_options;
using namespace oes::apps::eml;

namespace {

std::weak_ptr<EmlAppContext> global_ctx__;

void handlePscAssertion(const oes::core::AssertionInfo& info) {
    std::shared_ptr<EmlAppContext> ctx_ptr(global_ctx__.lock());
    OES_UNUSED(ctx_ptr);
    OES_ERROR(
        "{} ({}): {}: {}", info.file, info.line, info.message, info.condition
        );
}

void addCustomFormatterFromEnv(const oes::eml::openETPServerConfig& app_config) {
    auto customFormatter = std::make_shared<oes::core::TextMessageFormatter>(app_config.logFormat());
    auto customMessageHandler = std::make_shared<oes::core::ConsoleMessageHandler>();

    customMessageHandler->setFormatter(customFormatter);
    oes::core::Logger::instance().disconnectHandlers();
    oes::core::Logger::instance().connectHandler(customMessageHandler);
}

void addCustomSeverityFilterFromEnv(const oes::eml::openETPServerConfig& app_config) {
    std::string logMinSeverityString = app_config.logMinSeverity();
    std::string logMaxSeverityString = app_config.logMaxSeverity();
    oes::core::Message::Severity logMinSeverity;
    oes::core::Message::Severity logMaxSeverity;

    if (!logMinSeverityString.empty()) {
        logMinSeverity = oes::core::Message::severityFromString(logMinSeverityString);
    } else {
        logMinSeverity = oes::core::Message::Severity::first_log_severity;
    }

    if (!logMaxSeverityString.empty()) {
        logMaxSeverity = oes::core::Message::severityFromString(logMaxSeverityString);
    } else {
        logMaxSeverity = oes::core::Message::Severity::last_log_severity;
    }

    oes::core::Logger::instance().setSeverityFilter(logMinSeverity,logMaxSeverity);
}

} // namespace

/*!
 * \brief Command-Line program for Data-Exchange Resqml.
 *
 * \param argc the number of command line arguments.
 * \param argv the command line arguments, including the executed program name/path.
 * \return 0 on success; non-zero otherwise.
 */
int main(int argc, char* argv[]) {

#ifdef OES_DEBUG_BUILD
    // For early startup logging, before ctx is created.
    for (int i = 0; i < argc; ++i) {
        using namespace std;
        if (0 == strcmp("--debug-main", argv[i])) {
            OES_DEBUG("Entered main(), static initializations done.");
            const int w = static_cast<int>(log10(argc)) + 1;
            for (int j = 0; j < argc; ++j) {
                OES_DEBUG("ARG#{:0{}} <<{}>>", j, w, argv[j]);
            }
            break;
        }
    }
#endif

    oes::core::cachedTzOffset();

    const auto tw = EmlAppContext::terminalWidth();
    po::options_description generic_desc("" /*"Global Options"*/, tw);
    generic_desc.add_options()
        ("help,h",                  "Display this information")
        ("version,V",               "Display version information")
        ("timer",                   "Display real/user time of command action")
        ("output-file,o", txtArg(), "File to write dumps/text to")
        ("config",   txtArgOpt(),   "Path to the json environment configuration files")
#ifdef OES_DEBUG_BUILD
        ("nobps",                   "No breakpoints")
#endif
    ;

    options_description logger_desc("Logging Options", tw);
    oes::core::Logger::instance().add_options(logger_desc);

    options_description hidden_desc("Hidden Options", tw);
    hidden_desc.add_options()
("command",         txtArg(),   "Command to execute")
("cmdargs", po::value<UnparsedArgs>(), "Arguments for command")
#ifdef OES_DEBUG_BUILD
("debug-main", "Low-level main() logging for startup debugging")
#endif
;

    po::options_description all_desc("All Options", tw);
    all_desc.add(generic_desc).add(logger_desc).add(hidden_desc);
    

    const EmlAppNamedCommandVec commands = {
        { "hdf5",   &hdf5_main,   "Tools to analyze HDF5 files" },
        { "server", &server_main, "ETP 1.2 server" },
        { "probe",  &probe_main,  "HTTP query and ETP ping ETP servers" },
        { "space",  &space_main,  "ETP 1.2 client accessing ETP Dataspaces" }
    };

    std::ostringstream oss;
    for (const EmlAppNamedCommand& entry : commands) {
        oss << fmt::format("  {:<8} {}", entry.name, entry.desc) << std::endl;
    }
    oss << "\nGlobal Options"; // trick for prettier output.
    const std::string& command_list = oss.str();

    po::options_description visible_desc(
        "\nUsage: openETPServer [global-options] <command> --action [options] [args]\n\n"
        "Type 'openETPServer <command> -h' for help on a specific command.\n\n"
        "Available Commands:\n" + command_list
    );
    visible_desc.add(generic_desc);
    visible_desc.add(logger_desc);
        
    std::stringstream visible_desc_buffer;
    visible_desc_buffer << visible_desc << std::endl;

    // from http://stackoverflow.com/questions/15541498
    // see also https://gist.github.com/randomphrase/10801888
    po::positional_options_description pos;
    pos .add("command", 1)
        .add("cmdargs", -1);

    variables_map global_args;
    try {
        parsed_options parsed = po::command_line_parser(argc, argv)
            .options(all_desc)
            .positional(pos)
            .allow_unregistered()
            .run();

        po::store(parsed, global_args);

        if (global_args.size() == 0) {
            OES_INFO(visible_desc_buffer.str());
            return 0;
        }

        std::string config_file_path;
        auto it = global_args.find("config");
        if(it!=global_args.end()){
            config_file_path = it->second.as<std::string>();
        } 
        auto app_config = oes::eml::openETPServerConfig::make(config_file_path);
        oes::core::OtelSpan::registerOtelConfig(app_config);
        oes::core::setAssertionHandler(handlePscAssertion);
        addCustomSeverityFilterFromEnv(*app_config);
        addCustomFormatterFromEnv(*app_config);

        oes::core::Logger::instance().configure(global_args);

        std::shared_ptr<EmlAppContext> ctx_ptr(new EmlAppContext(global_args, app_config));
        const EmlAppContext& ctx(*ctx_ptr); // here
        global_ctx__ = ctx_ptr;

        if (global_args.count("help") && ctx.command.empty()) {
            // global help
            OES_INFO(visible_desc_buffer.str());
            return 0;
        }

        if (global_args.count("version")) {
            OES_INFO(
                "(Energistics Markup Languages (EML) Command Line Utility. v1 Beta.)"
                );
            // TODO(unknown): show OCI version, HDF5 version, etc...
            return 0;
        }

        if (ctx.command.empty()) {
            OES_ERROR("openETPServer: Missing command");
            return 1;
        }

#ifdef OES_DEBUG_BUILD
        if (ctx.hasArg("nobps")) {
            oes::core::setBreakpointHandler([]() {
                return false; // should not break
            });
        }
#endif

        const int rc = ctx_ptr->dispatchCommand(parsed, commands);

        if (rc == -1) {
            OES_ERROR("openETPServer: Unknown command: {}\n\n{}", ctx.command
                , visible_desc_buffer.str());
        }

#ifdef OES_DEBUG_BUILD
        else if (rc != 0) {
            OES_ERROR("openETPServer: Non-zero return code: {}", rc);
        } else {
            OES_INFO("openETPServer: done.");
        }
#endif

        return rc;
    } catch (const std::exception& ex) {
        OES_ERROR("openETPServer: {}\n\n{}", ex.what(), visible_desc_buffer.str());
        return 3; // keep in sync with tryCall
    } catch (...) {
        OES_ERROR("openETPServer: Unknown Error");
        return 4; // keep in sync with tryCall
    }
}
