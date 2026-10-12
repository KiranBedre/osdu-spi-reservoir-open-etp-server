// ============================================================================
// Copyright 2012-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/TestConfig.h>

#include <sstream>
#include <fstream>
#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>
#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>

#ifdef BOOST_TEST_DYN_LINK
//  Boost test framework for dynamic linkage
#include <boost/test/framework.hpp>
#else
// Boost test framework for static linkage
#include <boost/test/included/unit_test.hpp>
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/execution_monitor.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/tree/traverse.hpp>
#include <boost/test/tree/visitor.hpp>
#if defined(OES_DEBUG_BUILD) && defined(_WIN32)
    #define VC_EXTRALEAN
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h> // For OutputDebugStringA
#endif
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/core/AssertTestTools.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/fwd.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Otel.h>

#if WIN32
#include <oes/core/utils/Utf8.h>
#define FORCE_UTF8_ENCODING
#endif


namespace but = boost::unit_test;
namespace bpo = boost::program_options;
namespace bio = boost::iostreams;

using namespace oes::core;

namespace {

/*****************************************************************************/

// Warning: args' lifetime must exceeds returned vector's lifetime!
std::vector<char*> toArgv(const std::vector<std::string>& args) {
    std::vector<char*> argv(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        argv[i] = const_cast<char*>(args[i].c_str());
    }
    return argv;
}

/*****************************************************************************/

using TestSuiteStack = std::vector<const but::test_suite*>;

using TestTreeVisitorFunc = std::function<
    void(const TestSuiteStack& tss, const but::test_case* tc)
>;

struct ForwardingTestTreeVisitor : but::test_tree_visitor {
    explicit ForwardingTestTreeVisitor(const TestTreeVisitorFunc& visitor) : visitor_(visitor) {}

    TestTreeVisitorFunc visitor_;
    TestSuiteStack tss_;

    // test tree visitor interface
    void visit(const but::test_case& tc) override {
        visitor_(tss_, &tc);
    }
    bool test_suite_start(const but::test_suite& ts) override {
        tss_.push_back(&ts);
        visitor_(tss_, nullptr);
        return true;
    }
    void test_suite_finish(const but::test_suite& ts) override {
        OES_RELEASE_UNUSED(ts);
        assert(tss_.back() == &ts);
        tss_.pop_back();
    }
};

/*****************************************************************************/

    /*! \internal \class ExecutionObserver
     * Observes execution of unit tests and provides a short end report that list the failures.
     */
    class ExecutionObserver : public but::test_observer {
    public:
        explicit ExecutionObserver(std::ostream& output_stream)
            :count_(0)
            ,output_stream_(output_stream)
        {}

        /*! Callback called when a test starts.
         * Stores its name and the number of declared failures, using for instance
         * BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES.
         * By default, 0 failure is expected for a test case.
         */
        void test_unit_start( but::test_unit const& ut) override {
            UnitTest cur;
            cur.name_ = ut.p_name;
            cur.logged_ = false;
            cur.failure_countdown_ = ut.p_expected_failures;
            current_tests_.push_back(cur);
        }

        /*! Callback called when a unit test is ended.
         * If the number of excepted failure is not reached, considers the test as failed to
         * force the analysis.
         */
        void test_unit_finish( but::test_unit const&, unsigned long /* elapsed */ ) override {
            if(current_tests_.empty()) {
                return;
            }
            if(current_failure_countdown() > 0) {
                storeCurrentTestFailure("Number of expected failure not reached");
            }
            current_tests_.pop_back();
        }

        /*! Callback called when a test has been executed.
         * If \p passed is false, decreases the number of expected failures for this test.
         * If no more failure is expected, stores this test in the final report.
         */
        virtual void assertion_result( bool passed ) {
            if(!passed) {
                decrease_failure();
                if(current_failure_countdown() < 0) {
                    storeCurrentTestFailure();
                }
            }
        }

        /*! Callback called when an unexpected assert is raised.
         * Decreases the number of expected failures for this test. If no more failure is
         * expected, stores this test in the final report.
         */
        void exception_caught( boost::execution_exception const& e ) override {
            if(current_tests_.empty()) {
                return;
            }
            decrease_failure();
            if(current_failure_countdown() < 0) {
                storeCurrentTestFailure(std::string(e.what().begin(), e.what().end()));
            }
        }

        /*! Prints the final report if some test executions have been failed.
         */
        void logFinalReport() {
            if(count_ != 0) {
                output_stream_ << "*** failure detail: " << std::endl;
                output_stream_ << final_report_.str() << std::endl;
            }
        }
    private:
        /*! Adds the current test in the final report (only once).
         */
        void storeCurrentTestFailure(const std::string& extra_info = std::string()) {
            if(current_tests_.size() <= 1) {
                return;
            }
            UnitTest& cur = current_tests_.back();
            if(cur.logged_) {
                // log only once
                return;
            }
            final_report_
                << "(" << ++count_ << ") \t: ";
            for(size_t i = 1/* skip "Master Test Suite"*/; i < current_tests_.size(); ++i) {
                final_report_ << (i>1? "/" : "") << current_tests_[i].name_;
            }
            final_report_ << " failed. " << extra_info << std::endl;
            cur.logged_ = true;
        }

        /*! Decreases the number of expected failures for the current tests and its parents.
         */
        void decrease_failure() {
            for(size_t i = 1/* skip "Master Test Suite"*/; i < current_tests_.size(); ++i) {
                --current_tests_[i].failure_countdown_;
            }
        }
        /*! \return the failure counter for the current test.
         * Positive number means some failures are still expected, negative
         * number means too many failures has been encountered.
         */
        int current_failure_countdown() {
            if(current_tests_.empty()) {
                return 0;
            }
            const UnitTest& cur = current_tests_.back();
            return cur.failure_countdown_;
        }

    private:
        std::ostringstream final_report_;
        size_t count_;
        std::ostream& output_stream_;
        struct UnitTest {
            std::string name_;
            bool logged_;
            int failure_countdown_;
        };
        std::vector<UnitTest> current_tests_;
    };

/*****************************************************************************/

#if defined(OES_DEBUG_BUILD) && defined(_WIN32)
    struct OutputDebugStringSink : bio::sink {
        std::streamsize write(const char* s, std::streamsize n) {
            const std::string str(s, n);
            OutputDebugStringA(str.c_str());
            return n;
        }
    };
    typedef bio::tee_device<std::ostream,OutputDebugStringSink> TeeDebugDevice;
    typedef bio::stream<TeeDebugDevice> TeeDebugStream;
#endif

    bpo::options_description& getOESOptionsDescription() {
        static bpo::options_description desc(
            "Usage: tests_exe [options] [--run_test=TestSuiteOrCase]\n"
            "or\n"
            "Usage: tests_exe [Boost.Test options] -- [OES or PG Logger options]\n"
            "\n"
            "Note: For help on Boost.Test itself, use -H.\n"
            "      For help on specific Boost.Test option, use trailing -- as below:\n"
            "      tests_exe --help=option_name --\n"
            "\n"
            "Examples:\n"
            "1) Run all Unit tests (but not Performance or Manual tests by default):\n"
            "   tests_exe\n"
            "2) Run test cases from a given test suite:\n"
            "   tests_exe --run_test=MyClassTests\n"
            "3) Run given test case from a given test suite:\n"
            "   tests_exe --run_test=MyClassTests/test_my_method\n"
            "4) Run given performance pseudo-test (skipped without --perf):\n"
            "   tests_exe --perf --run_test=MyClassTests/my_benchmark\n"
            "5) Run all performance pseudo-test with debug log-level\n"
            "   tests_exe -P -g\n"
            "\n"
            "OES options"
        );
        if( desc.options().empty() ) {
            desc.add_options()
                ("pause",               "Pause test executable before returning")

                ("no_dbg_console",      "Disable output in debugging console")

                // Keep older help_oes for backward-compatibility. Using just --help
                // should resolve to this one too, thanks to allow_guessing below.
                ("help_oes",           "Print OES-specific help")
                ("oes-help,h",         "Print OES-specific help")
                ("boost-help,H",        "Print Boost.Test-specific help")

                ("perf,P",              "Also run Performance tests (skipped by default)")
                ("manual,M",            "Also run Manual tests (skipped by default)")
                ("debug,g",             "Shorthand for -log_level debug")
                ("list-test-cases,C",   "List test cases in this executable and exit")
                ("list-test-suites,S",  "List test suites in this executable and exit")

                ("boost-log-level,l", bpo::value<std::string>(), "Boost.Test --log_level=; for values, use:\n"
                                                                 "test_exe --help=log_level --")
                ;
        }
        return desc;
    }

    bpo::variables_map& getOESOptions() {
        static bpo::variables_map vm;
        return vm;
    }

    std::ostream& getOStream() {
#if defined(OES_DEBUG_BUILD) && defined(_WIN32) && (BOOST_VERSION < 106000)
        // On Windows debug build, when running under debugger, we pipe all log
        // messages into the debug output of Visual Studio.
        if( not getOESOptions().count("no_dbg_console")
            and IsDebuggerPresent()!=0 ) {
                static OutputDebugStringSink sink;
                static TeeDebugDevice device(*but::runtime_config::log_sink(), sink);
                static TeeDebugStream stream(device);
                return stream;
        } else {
            return std::cout;
        }
#else
        return std::cout;
#endif
    }

    bool doInitOESUnitTest(
        int argc, char** argv,
        bpo::variables_map& our_options,
        bool allow_unregistered,
        const std::vector<std::string>* dashdash_args = nullptr,
        std::vector<std::string>* oes_args = nullptr,
        std::vector<std::string>* other_args = nullptr
    ) {
        try {
            std::shared_ptr<bpo::command_line_parser> parser;
            if (dashdash_args) {
                // assume no prog name
                parser.reset(new bpo::command_line_parser(*dashdash_args));
            } else {
                // assumes argv[0] is the prog name
                parser.reset(new bpo::command_line_parser(argc, argv));
            }
            parser->options(getOESOptionsDescription());
            if (allow_unregistered) {
                parser->allow_unregistered(); // Don't fail on Boost.Test options
            }
            bpo::parsed_options options = parser->run();
            if (oes_args && allow_unregistered) {
                assert(other_args);
                for (const auto& opt : options.options) {
                    if (opt.string_key == "oes-log-level" ||
                        opt.string_key == "boost-log-level"
                    ) {
                        // Skip these tokens, which are rewritten differently later by caller
                        continue;
                    }
                    const bool is_other_arg = opt.unregistered || opt.position_key >= 0;
                    std::vector<std::string>* vec = is_other_arg? other_args: oes_args;
                    for (const std::string& token : opt.original_tokens) {
                        vec->emplace_back(token);
                    }
                }
            }
            bpo::store(options, our_options);
            bpo::notify(our_options);
        } catch (const std::exception& ex) {
            OES_ERROR("Unable to run OES unit tests: {}", ex.what());
            return false;
        } catch (...) {
            OES_ERROR("Unable to run OES unit tests: Unknown exception");
            return false;
        }

        return true;
    }

    bool initOESUnitTest() {
        const bool rc = doInitOESUnitTest(
            // Note: Boost.Test already trimmed its own arguments
            but::framework::master_test_suite().argc,
            but::framework::master_test_suite().argv,
            getOESOptions(), false
        );

        but::unit_test_log.set_stream(getOStream());

        // default unit test assertion handler
        oes::testing::pushUnitTestHandler();

        return rc;
    }

    int argc_cache = 0;
    char** argv_cache = nullptr;

/*****************************************************************************/

std::string print_count(size_t count, const char* label) {
    switch (count) {
    case  0: return fmt::format("No {}s", label);
    case  1: return fmt::format("{} {}", count, label);
    default: return fmt::format("{} {}s", count, label);
    }
}

} // namespace

namespace oes {
namespace testing {

/*****************************************************************************/

/*! \def OES_TEST_MAIN()
 * \brief Defines the augmented Boost.Test main provided by TestMain
 * \ingroup oes_testing_core
 * \details Incompatible with BOOST_TEST_MODULE macro, you must not use
 * the built-in Boost.Test main.
 */

/*****************************************************************************/

/*!
 * \brief Gets the test case name of the current Boost.Test unit test.
 *
 * Can be useful to create test-case-specific temporary files or directories.
 *
 * \return "my_case", even is that test is part of test suite "MySuite".
 * \note Does \b NOT include the test suite name, so \b NOT a \em full name.
 * \sa test_suite_name()
 */
std::string test_case_name() {
    return boost::unit_test::framework::current_test_case().p_name.get();
}

/*!
 * \brief Gets the test suite name of the current Boost.Test unit test.
 *
 * Can be useful to create test-suite-specific temporary files or directories.
 *
 * \return "MySuite" if the current test case is part of user-defined suite,
 *     or "Master Test Suite" otherwise.
 * \note Returns the \em direct-parent suite name. In rare cases, suite can
 *     be nested arbitrarily, and this utility gets only the direct parent.
 */
std::string test_suite_name() {
    namespace but = boost::unit_test;
    const auto& tcase = but::framework::current_test_case();
    // should probably go up the ancestry, since suites can nest,
    // but in practice we don't do it in our own tests.
    const auto& tsuite = but::framework::get<but::test_suite>(tcase.p_parent_id);
    return tsuite.p_name.get();
}

/*****************************************************************************/

/*! \class TestMain oes/testing/core/TestMain.h
 * \brief Provides main for unit-tests based on Boost.Test
 * \ingroup oes_testing_core
 */

/*! \brief Main function
 * \details Used to implement main in test executable (see OES_TEST_MAIN)
 * - Uses and augment the built-in Boost.Test main routine boost::unit_test::unit_test_main
 * - Adds an additional --pause option, pausing execution after running the tests
 * (meant for developers to check easily test summary).
 * - Adds an additional logger on windows debug (unless --no_dbg_console is one),
 * printing all log information to VS debug console while retaining the same information
 * in normal log.
 */
int TestMain::main(int argc, char* argv[]) {
    argc_cache = argc;
    argv_cache = argv;

    // Otel Config
    auto app_config = oes::testing::TestConfig::make();
    oes::core::OtelSpan::registerOtelConfig(app_config);

    // Parse CLI args a first time, using only our options (not Boost.Test ones)
    // for some early initialization (or not, see --no-pg-logger) before Boost.Test
    bpo::variables_map our_options;
    std::vector<std::string> oes_args;
    std::vector<std::string> other_args;

    // Boost.Test v3 (Boost >= 1.59) supports the Unix-standard --
    // to introduce custom argument(s) that Boost.Test does not understand.
    int dashdash_idx = -1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == '\0') {
            dashdash_idx = i; break;
        }
    }
    const bool has_dashdash = (dashdash_idx != -1);
    if (has_dashdash) {
        assert(dashdash_idx < argc);
        if (const int argc2 = argc - (dashdash_idx + 1)) {
            char** argv2 = argv + (dashdash_idx + 1);
            for (int i = 0; i < argc2; ++i) {
                oes_args.emplace_back(argv2[i]);
            }
        }

        // init ourselves only from args after --, all expected to be *our* args,
        // so we don't allow unregistered args
        if (!doInitOESUnitTest(0, nullptr, our_options, false, &oes_args)) {
            return 1;
        }
    } else {
        if (!doInitOESUnitTest(argc, argv, our_options, true, nullptr, &oes_args, &other_args)) {
            return 1;
        }
        if (our_options.count("boost-log-level")) {
            const std::string& level = our_options["boost-log-level"].as<std::string>();
            other_args.emplace_back("--log_level=" + level);
        }
    }

    if (our_options.count("oes-help") || our_options.count("help_oes")) {
        but::results_reporter::get_stream()
            << std::endl << getOESOptionsDescription() << std::endl;
        return 0;
    }

    if (our_options.count("boost-help")) {
        ExecutionObserver reporter(getOStream());
        but::framework::register_observer(reporter);
        char* help_argv[2] = { argv[0], const_cast<char*>("--help") };
        const int result = but::unit_test_main(nullptr, 2, help_argv);
        reporter.logFinalReport();
        return result;
    }

    if (our_options.count("list-test-cases")) {
        int ts_count = 0; int tc_count = 0;
        ForwardingTestTreeVisitor visitor(
            [&ts_count, &tc_count]
            (const TestSuiteStack& tss, const but::test_case* tc) {
                if (tc) {
                    for (size_t i = 1 /* Skip Master Test Suite */; i < tss.size(); ++i) {
                        OES_INFO("{}/", tss[i]->p_name.value);
                    }
                    OES_INFO(tc->p_name.value);
                    ++tc_count;
                } else if (tss.size() > 1) { // Only Master Test Suite has == 1
                    ++ts_count;
                }
            }
        );
        const auto& master_ts = but::framework::master_test_suite();
#if (BOOST_VERSION >= 106000)
        but::traverse_test_tree(master_ts, visitor, true);
#else
        but::traverse_test_tree(master_ts, visitor);
#endif
        OES_INFO("\n{}, {}.", print_count(ts_count, "test suite"), print_count(tc_count, "test case"));
        return 0;
    }

    if (our_options.count("list-test-suites")) {
        int ts_count = 0; int tc_count = 0;
        ForwardingTestTreeVisitor visitor(
            [&ts_count, &tc_count]
            (const TestSuiteStack& tss, const but::test_case* tc) {
                if (!tc) {
                    if (tss.size() > 1) { // Only Master Test Suite has == 1
                        for (size_t i = 1 /* Skip Master Test Suite */; i < tss.size(); ++i) {
                            OES_INFO("{}/", tss[i]->p_name.value);
                        }
                        OES_INFO("");
                        ++ts_count;
                    }
                } else {
                    ++tc_count;
                }
            }
        );
        const auto& master_ts = but::framework::master_test_suite();
#if (BOOST_VERSION >= 106000)
        but::traverse_test_tree(master_ts, visitor, true);
#else
        but::traverse_test_tree(master_ts, visitor);
#endif
        OES_INFO("\n{}, {}.", print_count(ts_count, "test suite"), print_count(tc_count, "test case"));
        return 0;
    }

    if (our_options.count("perf")) {
        if (oes::testing::arePerformanceTestsEnabled()) {
            OES_INFO("Performance tests already enabled (OES_EXECUTE_PERF_TEST=true)");
        } else {
            OES_INFO("Enabling Performance tests");
            System::setEnv("OES_EXECUTE_PERF_TEST", "true");
        }
    }

    if (our_options.count("manual")) {
        if (oes::testing::areManualTestsEnabled()) {
            OES_INFO("Manual tests already enabled (OES_EXECUTE_MANUAL_TEST=true)");
        } else {
            OES_INFO("Enabling Manual tests (Please turn them into Unit tests)");
            System::setEnv("OES_EXECUTE_MANUAL_TEST", "true");
        }
    }

    std::vector<std::string> logger_args;
    std::vector<char*> logger_argv; // MUST be at top-level scope of TestMain

    // - Disabling by default the memory leak detection (to avoid the dump after each run),
    // use --detect_memory_leaks=1 to re-enable it explicitly.
    // - We do not terminate cleanly/explicitly all services upon access point deletion,
    // and a lot of data allocated (hence the spurious object dump).
   System::setEnv("BOOST_TEST_DETECT_MEMORY_LEAK", "0");

   std::vector<std::string> boost_test_args;
   std::vector<char*>       boost_test_argv;
   const bool use_raw_args = (has_dashdash || (oes_args.empty() && !our_options.count("boost-log-level")));
   if (!use_raw_args) {
       // rewrite the args Boost.Test will process, to put OES-specific args
       // after the -- so Boost.Test does not error-out on them.
       boost_test_args.emplace_back(argv[0]);
       boost_test_args.insert(boost_test_args.end(), other_args.begin(), other_args.end());
       if (!oes_args.empty()) {
           boost_test_args.emplace_back("--");
           boost_test_args.insert(boost_test_args.end(), oes_args.begin(), oes_args.end());
       }
       //OES_INFO("Rewrote command-line to:");
       //for (const auto& arg : boost_test_args) {
       //    OES_INFO("{} ", arg);
       //}
       //OES_INFO("");
       boost_test_argv = toArgv(boost_test_args);
   } else {
       //OES_INFO("Using command-line as-is:");
       //for (int i = 0; i < argc; ++i) {
       //    OES_INFO("{} ", argv[i]);
       //}
       //OES_INFO("");
   }

    ExecutionObserver reporter(getOStream());
    but::framework::register_observer(reporter);
    const int result = but::unit_test_main(
        &initOESUnitTest,
        use_raw_args? argc: static_cast<int>(boost_test_argv.size()),
        use_raw_args? argv: boost_test_argv.data()
    );
    clearAssertionHandlers();
    reporter.logFinalReport();

    // Gives developer(s) opportunity to read log before closing
    if( getOESOptions().count("pause") ) {
        OES_INFO("\n<RETURN> to continue...");
        std::string text;
        std::getline(std::cin, text);
    }

    return result;
}

int TestMain::argc() {
    return argc_cache;
}

char** TestMain::argv() {
    return argv_cache;
}

std::string TestMain::get_exe_path() {
    return argv_cache[0];
}

/*****************************************************************************/

/*!
 * \return true if performance tests must be executed, false otherwise
 * \note Gets OES_EXECUTE_PERF_TEST environment variable,
 *     must be set to "true" to enable performance tests.
 */
bool arePerformanceTestsEnabled() {
    const std::string& var = oes::testing::TestConfig::make()->oesExecutePerfTest();
    return var == "true";
}

/*!
 * \return true if manual tests must be executed, false otherwise
 * \note Gets OES_EXECUTE_MANUAL_TEST environment variable,
 *     must be set to true" to enable performance tests.
 */
bool areManualTestsEnabled() {
    const std::string& var = oes::testing::TestConfig::make()->oesExecuteManualTest();
    return var == "true";
}

/*****************************************************************************/

/*!
 * \brief Resolves an asset/resource under SCM for a test case.
 *
 * This method is used by unit tests to locate required test input files,
 * and provide full path name for test output files (collocated with inputs for now).
 *
 * Optionally supports a client-specific (and supplied) environment variable which
 * if provided is used to locate the resource file first, allowing for example to
 * run the test using an alternative version of the resource without having to check
 * it out first, or to run the test in an environment lacking the expected CMake
 * environment variables.
 *
 * For example, to locate the \em testLinePicking.epc input file to be found in
 * \em ${PWD}/data/resqml, or directly in ${RESQML_FILES} if
 * defined, use:
 * \code
 * const std::string& line_picking_input = oes::testing::resolveTestDataFile(
 *     "data/resqml", "testLinePicking.epc", true, "RESQML_FILES"
 * );
 * \endcode
 *
 * \param psc_relative_path the path within the SharedComnponents workspace the
 *     files are expected to be in. Must not be empty (i.e. no test resources in
 *     the root of SharedComponents), and must not contain Windows-specific
 *     backslashes, but only portable forward-slashes.
 * \param name The \em short name of the file, without any slashes, except at the
 *     end, indicating the input or output file is a folder (directory), not a file.
 * \param is_input_file whether this is an input file, thus required, or an
 *     output file, which is then deleted if it exists.
 * \param env_var_override (Optional; defaults to empty) The name of an environment
 *     variable, which if provided (i.e. non-empty), is used to locate the files
 *     directly under, ignoring \a psc_relative_path.
 * \return the resolved input or output file on success.
 * \throws Boost.Test exception failing the current test case on error,
 *     since uses BOOST_REQUIRE and BOOST_FAIL on failure.
 *
 * \warning As already mentioned above, if \a is_input_file is false, and a file exists
 *     with the resolved output file name, probably from a previous test run, the
 *     file is deleted first. And if \a name ends with a (forward) slash, indicating
 *     that the client expects a folder and not a file, an existing output folder will
 *     also be fully deleted (i.e emptied and deleted).
 */
std::string resolveTestDataFile(
    const std::string& psc_relative_path,
    const std::string& name,
    bool is_input_file,
    const std::string& env_var_override
) {
    namespace fs = std::filesystem;

    BOOST_REQUIRE(psc_relative_path.size() > 0);
    BOOST_REQUIRE(psc_relative_path.find('/') != std::string::npos);
    BOOST_REQUIRE(psc_relative_path.find('\\') == std::string::npos);
    BOOST_REQUIRE(psc_relative_path.front() != '/');
    BOOST_REQUIRE(psc_relative_path.back() != '/');

    BOOST_REQUIRE(name.size() > 0);
    BOOST_REQUIRE(name.find('\\') == std::string::npos);

    const bool expecting_folder = (name.back() == '/');
    if (expecting_folder) {
        BOOST_REQUIRE(name.find('/') == name.size() - 1); // only slash is at the end
    } else {
        BOOST_REQUIRE(name.find('/') == std::string::npos);
    }

    std::vector<fs::path> findPaths;
    if (!env_var_override.empty())
    {
        auto path = System::getEnv(env_var_override);
        if (!path.empty()) {
            // declare this env. var. to test against other files than the ones
            // checked in this file's directory.
            findPaths.emplace_back(std::move(path));
        }
    }
    // Find tests data either locally or in optional UNIT_TESTS_DATA_PATH env var.
    auto path = oes::testing::TestConfig::make()->unitTestsDataPath();
    if (!path.empty()) {
        findPaths.emplace_back(std::move(path));
    }
    findPaths.emplace_back(fs::current_path());
    findPaths.emplace_back(fs::current_path() / "tests");

    fs::path resolvedPath;
    for (const auto& rootPath : findPaths)
    {
#ifdef FORCE_UTF8_ENCODING
        const std::wstring relPath = utf8::fromUtf8ToWstr(psc_relative_path);
        const std::wstring fileName = utf8::fromUtf8ToWstr(name);
#else
        const std::string& relPath = psc_relative_path;
        const std::string& fileName = name;
#endif

        fs::path filePath = rootPath / relPath / fileName;
        filePath.make_preferred();

        if (is_input_file) {
            if (exists(filePath)) {
                if (expecting_folder) {
                    BOOST_REQUIRE_MESSAGE(fs::is_directory(filePath), "File not a folder: " << filePath);
                }
                else {
                    BOOST_REQUIRE_MESSAGE(fs::is_regular_file(filePath), "File not regular: " << filePath);
                }
                resolvedPath = filePath;
                break;
            }
        }
        else {
            if (exists(rootPath) && is_directory(rootPath)) {
                resolvedPath = filePath;
                break;
            }
        }
    }

    if (!resolvedPath.empty())
    {
        // Remove previous output file
        if (!is_input_file && exists(resolvedPath))
        {
            if (expecting_folder) {
                BOOST_REQUIRE_MESSAGE(fs::remove_all(resolvedPath), "Cannot remove folder: " << resolvedPath);
            }
            else {
                BOOST_REQUIRE_MESSAGE(fs::remove(resolvedPath), "Cannot remove file: " << resolvedPath);
            }
        }

#ifdef FORCE_UTF8_ENCODING
        return utf8::fromWstrToUtf8(resolvedPath.wstring());
#else
        return resolvedPath.string();
#endif
    }

    BOOST_FAIL(
        "Cannot locate test data " << (expecting_folder ? "folder": "file")
        << " '" << name << "' in $UNIT_TESTS_DATA_PATH/" << psc_relative_path
    );
    return ""; // unreachable
}

/*****************************************************************************/

bool readFile(const std::string& filename, std::vector<char>& buffer) {
    std::ifstream ifs(filename.c_str(), std::ifstream::binary | std::ifstream::in);
    if (!ifs.good()) {
        return false;
    }
    ifs.seekg(0, std::ios_base::end);
    int size = static_cast<int>(ifs.tellg());
    ifs.seekg(0, std::ios_base::beg);
    if (size == 0) {
        return true;
    }
    buffer.resize(size);
    ifs.read(buffer.data(), size);
    return ifs.good();
}

} // namespace testing
} // namespace oes
