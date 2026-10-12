// ============================================================================
// Copyright 2008-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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


/*! \file oes/core/utils/Logger.h
 * \brief Message logging framework
 */

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/RangeUtils.hpp>

#include <cstdio>
#include <algorithm>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/any.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>

#if defined(_WIN32)
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <oes/core/utils/config_leave_3rdparty.h>

namespace bpo = boost::program_options;

/***************************************************************************/

namespace oes {
    namespace core {
    const TextMessageFormatter ConsoleMessageHandler::defaultFormatter_;

    struct Message::SeverityMeta
    {
        Severity severity;
        std::string_view name;
        std::string_view prefix;
    };

    namespace
    {
        constexpr Message::SeverityMeta SeverityMetadata[] =
        {
            { Message::Severity::debug, "debug", "Debug" },
            { Message::Severity::verbose, "verbose", "Verbose" },
            { Message::Severity::info, "info", "Info" },
            { Message::Severity::warning, "warning", "Warning" },
            { Message::Severity::alert, "alert", "Alert" },
            { Message::Severity::error, "error", "Error" },
            { Message::Severity::critical, "critical", "Critical" },
            { Message::Severity::fatal, "fatal", "Fatal" },
            { Message::Severity::status, "status", "Status" },
            { Message::Severity::notice, "notice", "Notice" },
            { Message::Severity::cursor_notice, "cursor_notice", "Notice" }
        };
        constexpr Message::SeverityMeta UnknownSeverityMetadata = { Message::Severity::unknown, "unknown", "Unknown" };

        const Message::SeverityMeta& getSeverityMeta(Message::Severity severity)
        {
            const auto it = std::find_if(std::cbegin(SeverityMetadata), std::cend(SeverityMetadata), [severity](const Message::SeverityMeta& meta) {
                return meta.severity == severity;
            });
            return (it == std::cend(SeverityMetadata)) ? UnknownSeverityMetadata : *it;
        }

        struct DebugConsoleMessageHandler : ConsoleMessageHandler {
            void print(const String& txt) override {
#if defined(_WIN32)
              if( !txt.empty() ) {
                  const String::size_type i_last = txt.size () - 1;

                  //Enforce the proper formatting: each messages shall appear in the separate line.
                  //Those messages that have a trailing '\n' (patterned as "...\n" or "...\0\n") are
                  //passed as they are, '\n' is appended to the others.
                  if( '\n' != txt [i_last] && !('\0' == txt [i_last] && '\n' ==  txt [i_last - 1]) ) {
                      //The message does not have a leading '\n', so append a one.
                      String formatted_txt = txt;
                      formatted_txt.push_back('\n');
                      OutputDebugStringA(formatted_txt.c_str());
                  } else {
                      OutputDebugStringA(txt.c_str());
                  }
              }
#else
                OES_UNUSED(txt);
#endif
           }
        };

    } // namespace

        /*! \class Message oes/core/utils/Logger.h
         * \brief Base class of messages.
         * \ingroup oes_core_utils_logging
         */

        /*!
         * \brief Base message constructor
         */
        Message::Message(Severity severity, String message)
            : severityMeta_(getSeverityMeta(severity))
            , time_(std::chrono::time_point_cast<Time::duration>(std::chrono::system_clock::now()))
            , message_(std::move(message))
        { }

        Message::Message(Severity severity, MessagePromise messagePromise)
            : severityMeta_(getSeverityMeta(severity))
            , time_(std::chrono::time_point_cast<Time::duration>(std::chrono::system_clock::now()))
            , messagePromise_(std::move(messagePromise))
        { }

        /*!
         * Returns the string that prefix this type of message.
         */
        std::string_view Message::prefix() const noexcept {
            return severityMeta_.prefix;
        }

        /*!
         * Returns a string representation of the current time
         */
        String Message::time_string() const {
            const auto ms = time_ - std::chrono::floor<std::chrono::seconds>(time_);
            return fmt::format("{:%F %T}.{:03}", time_, ms.count());
        }

        /*!
         * Returns the formatted message.
         */
        const String& Message::message() const {
            if (messagePromise_)
            {
                message_ = messagePromise_();
                messagePromise_ = nullptr;
            }
            return message_;
        }

        /*!
         * Sets the formatted text of the message.
         * \param[in] message the formatted text of the message
         */
        void Message::setMessage(String message) {
            message_ = std::move(message);
            messagePromise_ = nullptr;
        }

        /*!
         * \fn std::ostream& operator << (std::ostream& os, const Message& message);
         * Prints message \a message to output stream \a os.
         * Printing is done using a StdFormatter
         */

        /*! \brief Converts an input Message::Severity to its string representation
         * \param[in] severity The severity to be converted to string
         * \return The string representation of a severity, lower-case
         */
        std::string_view Message::severityToString(Severity severity) {
            return getSeverityMeta(severity).name;
        }

        /*! \brief Converts an input String to its Message::Severity equivalent
         * \details
         * - Returns Message::unknown if the input string does not represent
         * any severity level (e.g. "not_a_severity" returns Message::unknown)
         * - The input \p severity is processed in a case-insensitive manner
         * (e.g. "info" and "INFO" both returns Message::info).
         * \param[in] severity An input string representing a message severity
         * (among status|debug|verbose|info|notice|warning|error|alert|critical|fatal).
         */
        Message::Severity Message::severityFromString(const String& severity)
        {
            const auto it = std::find_if(std::cbegin(SeverityMetadata), std::cend(SeverityMetadata), [severity](const SeverityMeta& meta) {
                return std::equal(cbegin(severity), cend(severity), meta.name.cbegin(), meta.name.cend(), [](const char lhs, const char rhs) {
                    return std::tolower(lhs) == std::tolower(rhs);
                });
            });
            return (it == std::cend(SeverityMetadata)) ? Severity::unknown : it->severity;
        }

        Message::Severity Message::severity() const noexcept {
            return severityMeta_.severity;
        }

        /****************************************************************************/

        /*! \class NotificationMessagesBufferEvent oes/core/utils/Logger.h
        * \brief Buffer event for notice messages.
        *
        * Sent to turn on or off the buffering for messages sent to the notification widget.
        *
        * When buffering is turned on, the following notification messages are appended to the end 
        * of the existing notice buffer and not displayed.
        * and displayed as a whole in the notification widget.
        *
        * When buffering is turned off, the next notice message sent will clear
        * all the previously buffered messages.  Note that, turning off buffering
        * does <b>not</b> clear the current notice messages (i.e. the buffer).
        * Clearing of the buffer occurs when the next, non-buffered message is sent.
        *
        * \remarks The preferred way of utilizing buffering for notice messages
        * is via the NotificationMessagesScopedBuffering class.  Unless absolutely
        * necessary, developers are discouraged to send PrintPrettyBufferEvents
        * directly.
        *
        * \see NotificationMessagesScopedBuffering
        *
        * \ingroup oes_core_utils_logging
        */

        /*!
        * Constructs a notice buffer event that will turn the buffering on.
        * \param[in] on Only used to distinguish the two constructors.
        * Has no effect if the buffering was already on.
        */
        NotificationMessagesBufferEvent::NotificationMessagesBufferEvent(const turn_buffering_on_tag& on) :
            Message(Severity::unknown, String()),
            turn_buffering_on_(true) {
            OES_UNUSED(on);
        }

        /*!
        * Constructs a notice buffer event that will turn the buffering on.
        * \param[in] off Only used to distinguish the two constructors.
        * Has no effect if the buffering was already off.
        */
        NotificationMessagesBufferEvent::NotificationMessagesBufferEvent(const turn_buffering_off_tag& off) :
            Message(Severity::unknown, String()),
            turn_buffering_on_(false) {
            OES_UNUSED(off);
        }

        /*!
        * Returns true if this NoticeBufferEvent is to turn on the buffering of
        * the notice messages when received by the appropriate event consumer.
        * If false, the event is to turn off the buffering.
        */
        bool NotificationMessagesBufferEvent::turn_buffering_on() const {
            return turn_buffering_on_;
        }

        /****************************************************************************/

        /*! \class NotificationMessagesScopedBuffering oes/core/utils/Logger.h
        * \ingroup oes_core_utils_logging
        * \brief Convenience class to turn on and off print pretty buffering.
        *
        * NotificationMessagesScopedBuffering turns on buffering for notification messages upon
        * construction and turns off buffering upon destruction by sending
        * the necessary NotificationMessagesBufferEvent to the appropriate event consumer.
        *
        * In other words, this RAII-based convenience class ties the lifetime of
        * print pretty buffering to its own lifetime.  When an instance of this class
        * goes out of scope and thus destructed, print pretty buffering is
        * automatically turned off.
        *
        * \remarks The preferred way of utilizing buffering for notification messages
        * is via the NotificationMessagesScopedBuffering class. NotificationMessagesBufferEvent
        * should not be sent directly.
        *
        * \see NotificationMessagesBufferEvent
        */


        /*!
        * Default constructor.
        * Turns on buffering by sending a NotificationMessagesBufferEvent.
        */
        NotificationMessagesScopedBuffering::NotificationMessagesScopedBuffering() {
            NotificationMessagesBufferEvent message{ NotificationMessagesBufferEvent::turn_buffering_on_tag() };
            for (const MessageHandler_ptr& handler : Logger::instance().handlers_) {
                handler->receive(message);
            }
        }

        /*!
        * Destructor.
        * Turns off buffering by sending a NotificationMessagesBufferEvent.
        */
        NotificationMessagesScopedBuffering::~NotificationMessagesScopedBuffering() {
            NotificationMessagesBufferEvent message{ NotificationMessagesBufferEvent::turn_buffering_off_tag() };
            for (const MessageHandler_ptr& handler : Logger::instance().handlers_) {
                handler->receive(message);
            }
        }

        /****************************************************************************/

        /*! \class MessageHandler oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         *
         * \brief MessageHandler provides a mean for redirecting messages sent
         * to Logger streams
         * \details (eg: update a widget appearance, write
         * to * a pipe, ...).
         *
         * Users who want to capture messages and redirect them at their
         * convenience should create a derived class and redefine function
         * receive().
         */

        /*! \brief Base class constructor 
         * \param useSeverityFilter true to apply the severity filter from the Logger. This
         * parameter is true by default. 
         */
        MessageHandler::MessageHandler(bool useSeverityFilter) {
            if (useSeverityFilter) {
                if (const auto& filter = oes::core::Logger::instance().severityFilter()) {
                    connectFilter(filter);
                }
            }
        }

        /*! \brief Destroys the message handler */
        MessageHandler::~MessageHandler() = default;

        /*!
         * \brief Processes a message
         * \details
         * This function is called when a new message has arrived on the stream
         * The handler is connected to(see Logger::connectHandler).
         */
        void MessageHandler::receive(const Message& message) {
            if (isFiltered(message)) {
                return;
            }
            handleMessage(message);
        }

        /*!
         * \fn MessageHandler::handleMessage(const Message&)
         * \brief Called to process the message if it is not filtered
         */

        /*! \brief Installs a new message filter message filtering.
         * \param[in] filter The new filter to install
         */
        void MessageHandler::connectFilter(const ConstMessageFilter_ptr& filter) {
            OES_ASSERT(filter, return;);
            filters_.push_back(filter);
        }

        /*! \brief Removes a message filter
         * \param[in] filter The filter to be removed (may not be null), the filter is not deleted
         * \see connectFilter(ConstMessageFilter_ptr)
         * \return true if the filter was removed
         */
        bool MessageHandler::disconnectFilter(const ConstMessageFilter_ptr& filter) {
            OES_ASSERT(filter, return false;);
            if (filters_.empty()) {
                return false;
            }
            auto itr = std::remove(filters_.begin(), filters_.end(), filter);
            if (itr == filters_.end()) {
                return false;
            }
            filters_.erase(itr, filters_.end());
            return true;
        }

        /*! \brief Removes all filters
         * \see connectFilter(ConstMessageFilter_ptr)
         */
        void MessageHandler::disconnectFilters() {
            filters_.clear();
        }

        /*!
         * \return true if the message is filtered
         * \details Alls filters are given the opportunity to filter
         */
        bool MessageHandler::isFiltered(const Message& message) {
            bool discarded = false;
            for (const ConstMessageFilter_ptr& filter : filters_) {
                // filter cannot be null. See assert in connectFilter()
                if (filter->filter(message)) {
                    discarded = true;
                }
            }
            return discarded;
        }

        /*! \brief Assigns a new message formatter
         * message formatting.
         * \details
         * - Default implementation does not support message formatting, it is
         * up to concrete classes to accept and make use of the new formatter.
         * \param[in] formatter The new formatter, may be null to remove formatting
         * \return true if the handler supports message formatting and accepts the
         * new formatter, false otherwise.
         */
        bool MessageHandler::setFormatter(ConstMessageFormatter_ptr formatter) {
            return !formatter;
        }

        /****************************************************************************/

        /*! \class MessageFilter oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         * \brief Base class for message filters.
         */

        /*! \fn virtual bool MessageFilter::filter(const Message& message) const
         * \brief Performs message filtering
         * \details
         * Note that the filter may choose to alter message itself if
         * applicable, message is not provided as a const object.
         * \retval true If the message is being filtered, the message should NOT be
         * forwarded to handlers (case of global filters) or should NOT be printed
         * (case of local filters).
         * \retval false If the message is not filtered, the message should be
         * forwarded to handlers (case of global filters) or should be printed
         * (case of local filters).
         * \param[in] message the message sent to the Logger
         */

        MessageFilter::~MessageFilter() = default;

        /****************************************************************************/

        /*! \class MessageContext oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         * \brief Base class for message contexts.
         * \details
         * Message contexts allow to add contextual information to a message
         * before it is sent to the message handlers. For example,
         * MessageContext%s are useful to add information about the included
         * file stack in case of parsing errors. This information is generally
         * not available to the code that sends the error, but the parser can
         * install a MessageContext in the Logger that provides this
         * information.
         */

        /*! \fn virtual void MessageContext::apply(const Message& message) const
         * \brief Applies a context to a message
         * \details
         * This function must replace the message formatted text by adding
         * information to the message.
         * \param[in] message the message sent to the Logger
         * \see Message::setFormat()
         */

        MessageContext::~MessageContext() = default;

        /****************************************************************************/

        /*! \class MessageFormatter oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         * \brief Base class for message formatters.
         */

        /*! \fn MessageHandler::MessageHandler()
         * Base class constructor.
         */

        /*!
         * Destroys the message formatter
         */
        MessageFormatter::~MessageFormatter() = default;

        /*! \fn MessageFormatter::format(const Message& message) const
         * \brief Formats the string to the desired format and returns the
         * resulting string
         */

        /*! \class XmlMessageFormatter oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         * \brief Formats messages to XML format
         */

        /*!
         * \brief Format the \a message using XML format.
         */
        String XmlMessageFormatter::format(const Message& m) const {

            std::stringstream s;
            s << "<" << m.prefix() << ">\n";
            s << "  <time>" << m.time_string() << "</time>\n";
            s << "  <file>" << m.function() << "</file>\n";
            s << "  <lineNumber>" << m.lineNumber() << "</lineNumber>\n";
            s << "  <function>" << m.function() << "</function>\n";
            s << "  <depth>" << m.depth() << "</depth>\n";
            s << "  <text>" << m.message() << "</text>\n";
            s << "</" << m.prefix() << ">\n";

            // Another alternative could be:
            // s << "<message type=\"" << m.prefix() << "\">\n";
            // ...
            // s << "</message>\n";

            return s.str();
        }

        /*! \class TextMessageFormatter oes/core/utils/Logger.h
         * \ingroup oes_core_utils_logging
         * \brief Formats messages to Text format
         */

        /*!
         * \brief Creates a new text formatter.
         *
         * The messages passed to the formatter are formatted using the
         * format specification \a format.
         *
         * Ordinary characters placed in the format string are copied without
         * conversion. Conversion specifications are introduced by a '%'
         * character, and terminated by a conversion specifier character, and
         * are replaced in s as follows:
         *
         * - %P: message prefix (Fatal, Critical, ...)
         * - %T: the current date and time in the form YYY-MM-DD HH:MM:SS
         * - %F: The file file that sent the message
         * - %L: The line number in the file
         * - %f: The function in the file (if available)
         * - %M: The message text
         * - %%: expands to the single character '%'
         * - any other characters are printed as is.
         *
         * If \a format is empty, the default format is taken instead: [%T] %P: %M
         */
        TextMessageFormatter::TextMessageFormatter(const String& format) :
            format_(format)
        {
            if( format_.empty() ) {
                // Reset to the default format:
                format_ = "[%T] %P: %M";
            }
        }

        /*!
         * \brief Formats a message to text format
         * \details
         * Formats the \a message using Text format according to the format
         * specification passed in the class constructor.
         */
        String TextMessageFormatter::format(const Message& message) const {
            std::stringstream s;
            bool gotPercent = false;

            for( String::const_iterator p = format_.begin(); p != format_.end(); ++p ) {
                if( gotPercent ) {
                    gotPercent = false;
                    switch(*p) {
                        case '%':
                            s << '%';
                            break;
                        case 'T':
                            s << message.time_string();
                            break;
                        case 'P':
                            s << message.prefix();
                            break;
                        case 'F':
                            s << message.file();
                            break;
                        case 'L':
                            s << message.lineNumber();
                            break;
                        case 'f':
                            s << message.function();
                            break;
                        case 'D':
                            s << message.depth();
                            break;
                        case 'M':
                            s << message.message();
                            break;
                        default:
                            s << '%' << *p;
                            break;
                    }
                } else {
                    if( *p == '%' ) {
                        gotPercent = true;
                        continue;
                    } else {
                        s << *p;
                    }
                }
            }

            return s.str();
        }

        /****************************************************************************/

        /*! \class Logger oes/core/utils/Logger.h
         * \ingroup oes_core_utils oes_core_utils_logging
         * \brief Prints a message to a message stream.
         * \details
         * - By default, the logger comes with a console handler (sending messages
         * to standard output) and a debug console handler (sending messages to
         * the debugging console) in windows debug builds.
         * - To customize this behavior, users can attach one or more message handlers to
         * the message stream: handlers will receive the message in the order of their
         * registration.
         */

        /*!\brief logger message send observer
         * \details
         * Enables to notify the PG Logger
         */
        Logger::MessageSendObserver Logger::messageSendObserver_;

        /*!\class Logger::ProgramOptions oes/core/utils/Logger.h
         * \brief Defines the oes::core::Logger program options.
         * \details The program options 
         * are used for the oes::core::Logger configuration. The program options names defined here
         *  may be shared by master logger implementation. Currently it is shared by the PGLog, but can possibly
         * be shared by some other one.
         */

        /*!
         * \brief The name of the command line argument used for setting the logging level.
         * \details This option name is exported and may be shared by the master loggers options.
         */
        const char* Logger::ProgramOptions::logSeverity_ = "log_level";

        /*!
         * \brief The shared implementation of the boost program_options custom validation
         *     for program option of the oes::core::Message::Severity type.
         *
         * \param values
         * \return the valid severity
         *
         * \exception boost::program_options::validation_error if the token does not correspondents
         *     to the valid value.
         */
        Message::Severity Logger::ProgramOptions::validateSeverity(const std::vector<std::string>& values) {
            const std::string& severity_name = boost::program_options::validators::get_single_string(values);

            // The severity value token is case insensitive,
            // the severity name expected by the Message::severityFromString is case insensitive.
            const Message::Severity severity = Message::severityFromString(severity_name);
            if (Message::Severity::unknown == severity) {
                //Unexpected severity value
                throw bpo::validation_error(
                    bpo::validation_error::invalid_option_value, "unexpected severity token - ", severity_name
                );
            }

            return severity;
        }

        /*!
         * \brief the boost::program_options overloaded validate function for Message::Severity enum.
         *
         * This function is called by the boost infrastructure and converts the string token
         * that correspondents with the log severity option to the valid Message::Severity value.
         *
         * \exception boost::program_options::validation_error if the token does not correspondents
         * to the valid value.
         */
        void validate(boost::any& v, const std::vector<std::string>& values, Message::Severity*, int) {
            bpo::validators::check_first_occurrence(v);
            const Message::Severity severity = Logger::ProgramOptions::validateSeverity(values);
            v = boost::any(severity);
        }

        Logger::Logger() :
            filters_(),
            contexts_(),
            handlers_(),
            pgLoggerFormatter_ ("%M")
        {
            connectHandler(createConsoleHandler());
#if defined(OES_DEBUG_BUILD) && defined(_WIN32)
            connectHandler(createDebugConsoleHandler());
#endif
            setSeverityFilter(Message::Severity::debug);
        }

        Logger::~Logger() = default;

        /*! \brief Add logger related options to program options
        *  \details Add log_level to command line options, and deprecated options quiet, debug and verbose for backward compatibility
         * \par desc options to add to
         * \retval desc parameter with added option
         */
        boost::program_options::options_description Logger::add_options(boost::program_options::options_description& desc) const{
            desc.add_options()
                (ProgramOptions::logSeverity_, boost::program_options::value<std::string>(),
                    "Logging level (debug|verbose|info|warning|error|alert|critical|fatal|status|notice)"
                )
                ("verbose,v",               "Deprecated(use log_level instead): Output information messages")
                ("debug",                   "Deprecated(use log_level instead): Output debug messages, overwrite verbose")
                ("quiet,q",                 "Deprecated(use log_level instead): Do not output any log below critical, overwrite verbose and debug");
            return desc;
        }

        /*! \brief Performs a basic configuration of the logger from command line arguments
         * \details Consolidates some basic configuration of the logger from command line
         * \par Recognized logging options:
         * - log_level, installing a global message filter with the said level. The default
         * value is 'debug' in debug builds and 'info' in release builds.
         * help option is not supported, as the help shall be supported on the application level.
         * - Also add deprecated options quiet, debug and verbose for backward compatibility
         */
        void Logger::configure(const boost::program_options::variables_map& options) {
            Message::Severity severity_default = Message::Severity::info;
            auto it = options.find(ProgramOptions::logSeverity_);
            if(it!=options.end()){
                severity_default = Message::severityFromString(it->second.as<std::string>());
            }else if( options.find("quiet") != options.end() ){
                severity_default = oes::core::Message::Severity::critical;
            }else if( options.find("debug") != options.end() ){
                severity_default = oes::core::Message::Severity::debug;
            }else if( options.find("verbose") != options.end() ){
                severity_default = oes::core::Message::Severity::verbose;
            }
            setSeverityFilter(severity_default);
        }

        /*!
         * \brief Updates or sets the minimum severity filter.
         *
         * \param minimumSeverity the minimum logging level a message must have (inclusive)
         * \param maximumSeverity (Optional; defaults to max_severity, use last_log_severity to filter
         *     the messages specific to the UI) the maximum logging
         *     level a message may have (inclusive).
         *
         * \details By default, the severity filter is driven by command line
         *    option with -log_level. 
         *    This filter is NOT global. However, by default it is placed on all
         *    MessageHandlers unless requested otherwise in the handler constructor. If a severity filter
         *    has been placed on a handler it will be replaced.
         */
        void Logger::setSeverityFilter(
            Message::Severity minimumSeverity,
            Message::Severity maximumSeverity
        ) {
            ConstMessageFilter_ptr newFilter = createSeverityFilter(minimumSeverity, maximumSeverity);
            if (severity_filter_) {
                disconnectFilter(severity_filter_);
                // if it has been placed on existing message handlers, replace it
                for (const MessageHandler_ptr& handler : handlers_) {
                    if (handler->disconnectFilter(severity_filter_)) {
                        handler->connectFilter(newFilter);
                        if (severity_filter_ == nullptr) {
                            break;
                        }
                    }
                }
            }
            connectFilter(newFilter);
            severity_filter_ = newFilter;
        }

        /*!
         * \return the severity filter (controlled by -log_level) if any
         */
        ConstMessageFilter_ptr Logger::severityFilter() const {
            return severity_filter_;
        }

        /*!
         * \brief Gets the current minimum logging level of the severity filter.
         *
         * If no severity filter is installed, returns Message::unknown.
         * \sa setSeverityFilter
         */
        Message::Severity Logger::minSeverityFilter() const {
            if (severity_filter_) {
                const auto severity_filter =
                    std::dynamic_pointer_cast<const BySeverityMessageFilter>(severity_filter_);
                OES_ASSERT(severity_filter, return Message::Severity::unknown;);
                return severity_filter->minimumSeverity_;
            }
            return Message::Severity::unknown;
        }

        /*!
         * \brief Gets the current maximum logging level of the severity filter.
         *
         * If no severity filter is installed, returns Message::unknown.
         * \sa setSeverityFilter
         */
        Message::Severity Logger::maxSeverityFilter() const {
            if (severity_filter_) {
                auto severity_filter =
                    std::dynamic_pointer_cast<const BySeverityMessageFilter>(severity_filter_);
                OES_ASSERT(severity_filter, return Message::Severity::unknown;);
                return severity_filter->maximumSeverity_;
            }
            return Message::Severity::unknown;
        }

        /*! \brief Installs a new global message filter.
         * \details Messages filtered by a global filter never reach the MessageHandlers. 
         * Local filters can be set on individual handlers.
         * \param[in] filter The new filter to install
         */
        void Logger::connectFilter(const ConstMessageFilter_ptr& filter) {
            OES_ASSERT(filter, return;);
            filters_.push_back(filter);
        }

        /*! \brief Removes a global message filter
         * \details Messages filtered by a global filter never reach the MessageHandlers.
         * Local filters can be set on individual handlers
         * \param[in] filter The filter to be removed (may not be null)
         * \see connectFilter(ConstMessageFilter_ptr)
         * \return true if the filter was removed
         */
        bool Logger::disconnectFilter(const ConstMessageFilter_ptr& filter) {
            OES_ASSERT(filter, return false;);
            if (filters_.empty()) {
                return false;
            }
            auto itr = std::remove(filters_.begin(), filters_.end(), filter);
            if (itr == filters_.end()) {
                return false;
            }
            filters_.erase(itr,filters_.end());
            return true;
        }

        /*! \brief Removes all global filters
        * \see connectFilter(ConstMessageFilter_ptr)
        */
        void Logger::disconnectFilters() {
            filters_.clear();
        }

        /*!
         * \brief Pushes a new message context
         * \details
         * Message contexts are applied in reverse order from the last one to
         * the first one.
         * \param[in] context a message context
         */
        void Logger::pushContext(const ConstMessageContext_ptr& context) {
            OES_ASSERT(context, return;);
            contexts_.push_back(context);
        }

        /*!
         * \brief Removes the last message context
         */
        void Logger::popContext() {
            contexts_.pop_back();
        }

        /*!
         * \brief Clears all the message contexts
         */
        void Logger::clearContexts() {
            contexts_.clear();
        }

        /*!
         * \brief Attaches a new handler to the stream.
         */
        void Logger::connectHandler(const MessageHandler_ptr& handler) {
            OES_ASSERT(handler, return;);
            handlers_.push_back(handler);
        }

        /*!
         * \brief Removes the specified handler from the stream.
         */
        void Logger::disconnectHandler(const MessageHandler_ptr& handler) {
            OES_ASSERT(handler, return;);
            handlers_.erase(
                std::remove(handlers_.begin(), handlers_.end(), handler),
                handlers_.end()
            );
        }

        /*!
         * \brief Resets the message stream to its initial state (Removes all
         * handlers from the stream).
         */
        void Logger::disconnectHandlers() {
            handlers_.clear();
        }

        void Logger::send(Message::Severity severity, std::string_view file, const int lineNumber,
            std::string_view function, String message)
        {
            Message msg(severity, std::move(message));
            msg.setFile(file);
            msg.setLineNumber(lineNumber);
            msg.setFunction(function);
            send(msg);
        }

        void Logger::send(Message::Severity severity, std::string_view file, const int lineNumber,
            std::string_view function, Message::MessagePromise messagePromise)
        {
            Message msg(severity, std::move(messagePromise));
            msg.setFile(file);
            msg.setLineNumber(lineNumber);
            msg.setFunction(function);
            send(msg);
        }

        /*!
         * \brief Sends the message to the logger.
         * \details Message is dispatched to all registered handlers unless filtered
         * by a global filter.
         * If no handlers are registered, the message is sent on the standard
         * output and to the debug window on Windows.
         * All filters are provided with the opportunity to filter the messages.
         */
        void Logger::send(Message& message) {
            // Check if the message is filtered globally

            bool discarded = false;
            for (const ConstMessageFilter_ptr& filter : filters_) {
                // filter cannot be null. See assert in connectFilter()
                if (filter->filter(message)) {
                    discarded = true;
                    // do not return early, a filter may need to 'see' a message even if a previously set
                    // filter discards it.
                }
            }
            if (discarded) {
                return;
            }

            // Apply the message contexts in reverse order

            for (const ConstMessageContext_ptr& context : reverse_of(contexts_)) {
                // Contexts cannot be null (see assertion in pushContext())
                context->apply(message);
            }

            // Dispatch messages to the handlers

            for (const MessageHandler_ptr& handler : handlers_) {
                // Handlers cannot be null (see assertion in connectHandler())
                handler->receive(message);
            }

            //Pass the messages to the device observer sink. The observer is set on the PG Logger initialization stage.
            //The current logger is only the facade, the logging is done by the PG Logger.
            //If the observer is not set the current logger is not functional.
            if (!messageSendObserver_ || message.severity() > Message::Severity::last_log_severity) {
                return;
            }
            oes::core::String text = pgLoggerFormatter_.format(message);
            const oes::core::String::size_type text_length = text.length();
            if (text_length >=1 && '\n' == text.at(text_length - 1)) {
                text.erase(text_length-1);
            }

            messageSendObserver_(
                message.severity(),
                "SharedComponents",
                message.file(),
                message.function(),
                static_cast<int>(message.lineNumber()),
                text
            );
        }

        /*!
         * \brief Creates a message filter based on a minimum severity
         *
         * The two highest levels, \em notice and \em status are special messages
         * routed to custom UI components typically, and may not be appropriate
         * to output in all contexts (e.g. unit tests). In such cases, using a
         * maximum severity of \em fatal, to filter out those notice and status messages.
         *
         * \param minimumSeverity the minimum severity level a message must have (inclusive)
         * \param maximumSeverity (Optional; defaults to max_severity, use last_log_severity to filter
         *     the messages specific to the UI) the maximum severity level a message may have (inclusive).
         * \return the newly created filter; called is responsible for setting it on the
         *     Logger singleton or a specific MessageHandler.
         */
        ConstMessageFilter_ptr Logger::createSeverityFilter(
            Message::Severity minimumSeverity,
            Message::Severity maximumSeverity
        ) {
            return std::make_shared<BySeverityMessageFilter>(minimumSeverity, maximumSeverity);
        }

        /*! \brief Creates a handler sending messages to std::cout
         * \details Uses by default a text formatter for its messages. The UI messages
         * (severity > last_log_severity) and info/debug are discarded.
         */
        MessageHandler_ptr Logger::createConsoleHandler() {
            return std::make_shared<ConsoleMessageHandler>();
        }

        /*! \brief Creates a handler sending messages to the debugger console (if any)
         * \details Currently a no-op handler on Linux or if debugger is not attached
         */
        MessageHandler_ptr Logger::createDebugConsoleHandler() {
            return std::make_shared<DebugConsoleMessageHandler>();
        }

        /*!
         * \brief Gets the console handler, if any.
         *
         * Allows to configure the console handler's filters or formatter,
         * without resorting to a global filter suppressing messages even before
         * handlers see them.
         *
         * Especially useful for unit tests, so Notice and Status messages are
         * not output to the console, yet test can still capture them via their
         * own MessageHandler.
         *
         * \return the (first) console handler found; otherwise null,
         *     if no console handler was added using createConsoleHandler()
         */
        MessageHandler_ptr Logger::consoleHandler() const {
            for (const auto& handler : handlers_) {
                if (dynamic_cast<ConsoleMessageHandler*>(handler.get())) {
                    return handler;
                }
            }
            return MessageHandler_ptr(); // not found
        }

        /*!
         * \brief Gets the console handler, if any.
         *
         * Allows to configure the console handler's filters or formatter,
         * without resorting to a global filter suppressing messages even before
         * handlers see them.
         *
         * Especially useful for unit tests, so Notice and Status messages are
         * not output to the console, yet test can still capture them via their
         * own MessageHandler.
         *
         * \return the (first) console handler found; otherwise null,
         *     if no console handler was added using createConsoleHandler()
         */
        MessageHandler_ptr Logger::debugConsoleHandler() const {
            for (const auto& handler : handlers_) {
                if (dynamic_cast<DebugConsoleMessageHandler*>(handler.get())) {
                    return handler;
                }
            }
            return MessageHandler_ptr(); // not found
        }

    } // namespace core
} // namespace oes
