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

#pragma once
#include <oes/core/utils/export.h>
#include <oes/core/utils/fmt_ext.h>
#include <oes/core/utils/fwd.h>
#include <oes/core/utils/TypeTraits.h>
#include <oes/core/utils/Singleton.h>
#include <oes/core/utils/String.h>

#include <iostream>
#include <chrono>
#include <vector>
#include <cstdarg>
#include <functional>


namespace boost {
    namespace program_options {
        class options_description;
        class variables_map;
    }
    class any;
}

namespace oes {
    namespace core {

        class OES_CORE_UTILS_EXPORT Message {
        public:
            enum class Severity {
                unknown = 0,
                debug,
                verbose,
                info,
                warning,
                alert,
                error,
                critical,
                fatal,
                status,
                notice,
                cursor_notice,
                first_log_severity = debug,
                last_log_severity = fatal,
                min_severity = debug,
                max_severity = cursor_notice
            };

            struct SeverityMeta;
            using MessagePromise = std::function<String()>;

            Message(Severity severity, String message);
            Message(Severity severity, MessagePromise messagePromise);

            static std::string_view severityToString(Severity severity);
            static Severity severityFromString(const String& severity);

            using Time = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

            Severity severity() const noexcept;
            std::string_view prefix() const noexcept;

            const String& message() const;
            void setMessage(String message);

            const Time& time() const noexcept { return time_; }
            String time_string() const;

            std::string_view function() const noexcept { return function_; }
            void setFunction(std::string_view function) noexcept { function_ = function; }

            std::string_view file() const noexcept { return file_; }
            void setFile(std::string_view file) noexcept { file_ = file; }

            size_t lineNumber() const noexcept { return lineNumber_; }
            void setLineNumber(size_t lineNumber) noexcept { lineNumber_ = lineNumber; }

            size_t depth() const noexcept { return depth_; }
            void setDepth(size_t depth) noexcept { depth_ = depth; }

        private:
            const SeverityMeta& severityMeta_;
            Time time_;
            std::string_view function_;
            std::string_view file_;
            size_t lineNumber_ = 0;
            size_t depth_ = 0;
            mutable String message_;
            mutable MessagePromise messagePromise_;
        };

        class OES_CORE_UTILS_EXPORT NotificationMessagesBufferEvent : public Message {
        public:
            struct turn_buffering_on_tag { };
            struct turn_buffering_off_tag { };

        public:
            explicit NotificationMessagesBufferEvent(const turn_buffering_on_tag&);
            explicit NotificationMessagesBufferEvent(const turn_buffering_off_tag&);

            bool turn_buffering_on() const;

            NotificationMessagesBufferEvent(const NotificationMessagesBufferEvent&) = delete;     // noncopyable
            NotificationMessagesBufferEvent& operator=(const NotificationMessagesBufferEvent&) = delete;

        private:
            const bool turn_buffering_on_;
        };

        /***************************************************************************/

        class OES_CORE_UTILS_EXPORT NotificationMessagesScopedBuffering {
        public:
            NotificationMessagesScopedBuffering();
            ~NotificationMessagesScopedBuffering();
        };

        /***************************************************************************/

        class OES_CORE_UTILS_EXPORT MessageFormatter {
        public:
            MessageFormatter(const MessageFormatter&) = delete;     // noncopyable
            MessageFormatter& operator=(const MessageFormatter&) = delete;
            virtual ~MessageFormatter();

            virtual String format(const Message& message) const = 0;
        protected:
            MessageFormatter() = default;
        };

        class OES_CORE_UTILS_EXPORT XmlMessageFormatter : public MessageFormatter {
        public:
            XmlMessageFormatter() {}
            String format(const Message& message) const override;
        };

        class OES_CORE_UTILS_EXPORT TextMessageFormatter : public MessageFormatter {
        public:
            explicit TextMessageFormatter(const String& format = String());
            String format(const Message& message) const override;
        private:
            String format_;
        };

        /***************************************************************************/

        class OES_CORE_UTILS_EXPORT MessageFilter {
        public:
            MessageFilter(const MessageFilter&) = delete;     // noncopyable
            MessageFilter& operator=(const MessageFilter&) = delete;
            virtual ~MessageFilter();

            virtual bool filter(const Message& message) const = 0;
        protected:
            MessageFilter() = default;
        };

        typedef std::vector<ConstMessageFilter_ptr> MessageFilterList;

        /***************************************************************************/

        struct OES_CORE_UTILS_EXPORT BySeverityMessageFilter : MessageFilter {
            BySeverityMessageFilter(
                Message::Severity mins, Message::Severity maxs
            ) : MessageFilter()
              , minimumSeverity_(mins)
              , maximumSeverity_(maxs)
            {
            }

            bool filter(const Message& message) const override {
                const auto s = message.severity();
                if (minimumSeverity_ <= s && s <= maximumSeverity_) {
                    return false; // Not filtered
                }
                return true; // Filtered
            }

            const Message::Severity minimumSeverity_;
            const Message::Severity maximumSeverity_;
        };

        /***************************************************************************/

        class OES_CORE_UTILS_EXPORT MessageContext {
        public:
            MessageContext(const MessageContext&) = delete;     // noncopyable
            MessageContext& operator=(const MessageContext&) = delete;
            virtual ~MessageContext();

            virtual void apply(Message& message) const = 0;
        protected:
            MessageContext() = default;
        };

        typedef std::vector<ConstMessageContext_ptr> MessageContextList;

        /***************************************************************************/

        class OES_CORE_UTILS_EXPORT MessageHandler {
        public:
            MessageHandler(const MessageHandler&) = delete;     // noncopyable
            MessageHandler& operator=(const MessageHandler&) = delete;
            virtual ~MessageHandler();

            void receive(const Message& message);
            void connectFilter(const ConstMessageFilter_ptr& filter);
            bool disconnectFilter(const ConstMessageFilter_ptr& filter);
            void disconnectFilters();
            bool isFiltered(const Message& message);

            virtual bool setFormatter(ConstMessageFormatter_ptr formatter);
        protected:
            explicit MessageHandler(bool useSeverityFilter = true);
            virtual void handleMessage(const Message& message) = 0;
        private:
            MessageFilterList filters_;
        };

        typedef std::vector<MessageHandler_ptr> MessageHandlerList;

        /***************************************************************************/
        
        /*!
         * \brief Forwards the log messages to the console
         * \details UI messages are discarded
         */
        struct OES_CORE_UTILS_EXPORT ConsoleMessageHandler : MessageHandler {
            ConsoleMessageHandler() : MessageHandler(false), formatter_() {
                //The Console only cares for log messages: discard UI message
                // connectFilter(std::make_shared<BySeverityMessageFilter>(Message::first_log_severity, Message::last_log_severity));
            }
            virtual void print(const String& txt) {
                std::cout << txt << std::endl;
            }
            void handleMessage(const Message& message) override {
                const String txt = formatter_ ? formatter_->format(message) : defaultFormatter_.format(message);
                print(txt);
            }
            bool setFormatter(ConstMessageFormatter_ptr formatter) override {
                formatter_ = formatter;
                return true;
            }
            static const TextMessageFormatter defaultFormatter_;
            ConstMessageFormatter_ptr formatter_;
        };

        /****************************************************************************/

        class OES_CORE_UTILS_EXPORT Logger : public Singleton<Logger> {
        public:
            Logger();
            ~Logger() override;

            boost::program_options::options_description add_options(boost::program_options::options_description& desc) const;
            void configure(const boost::program_options::variables_map& options);

            void send(Message::Severity severity, std::string_view file, const int lineNumber, std::string_view function, String message);
            void send(Message::Severity severity, std::string_view file, const int lineNumber, std::string_view function, Message::MessagePromise messagePromise);
            void send(Message& message);

            template<typename TFormat, typename ...TArgs, std::enable_if_t<std::is_convertible_v<TFormat, std::string_view>, int> = 0>
            void sendFormatted(Message::Severity severity, std::string_view file, const int lineNumber, std::string_view function, TFormat&& message, TArgs&&... formatArgs)
            {
                constexpr auto hasFormatFn = fmt::has_fmt_format_v<TFormat>;
                constexpr auto hasFormatArgs = sizeof...(formatArgs) != 0;

                // Logging as plain text or formatted message depending on the availability of the `fmt` library.
                // (!) Please include `fmt` library BEFORE this logger.
                static_assert(!hasFormatArgs || hasFormatFn, "Logging messages with formatting is only allowed with the `fmt` library (did you forget to include it?)");
                if constexpr (hasFormatArgs && hasFormatFn)
                {
                    try
                    {
                        // Using lazy formatting to prevent losing performance on filtered messages
                        send(severity, file, lineNumber, function, [&message , &formatArgs...]() -> String {
                            using namespace fmt;
                            return format(std::forward<TFormat>(message), std::forward<TArgs>(formatArgs)...);
                        });
                    }
                    catch (const std::exception& ex)
                    {
                        send(Message::Severity::error, file, lineNumber, function, String("Invalid log format, ") + ex.what() + ": " + message);
                    }
                    catch (...) {
                        send(Message::Severity::error, file, lineNumber, function, String("Invalid log format: ") + message);
                    }
                }
                else {
                    send(severity, file, lineNumber, function, std::forward<TFormat>(message));
                }
            }

            void connectFilter(const ConstMessageFilter_ptr& filter);
            bool disconnectFilter(const ConstMessageFilter_ptr& filter);
            void disconnectFilters();

            void connectHandler(const MessageHandler_ptr& handler);
            void disconnectHandler(const MessageHandler_ptr& handler);
            void disconnectHandlers();

            void pushContext(const ConstMessageContext_ptr& context);
            void popContext();
            void clearContexts();

            // Built-in filter(s)
            static ConstMessageFilter_ptr createSeverityFilter(
                Message::Severity minimumSeverity,
                Message::Severity maximumSeverity = Message::Severity::max_severity
            );
            void setSeverityFilter(
                Message::Severity minimumSeverity,
                Message::Severity maximumSeverity = Message::Severity::max_severity
            );
            Message::Severity minSeverityFilter() const;
            Message::Severity maxSeverityFilter() const;
            ConstMessageFilter_ptr severityFilter() const;

            // Built-in handler(s)
            static MessageHandler_ptr createConsoleHandler();
            static MessageHandler_ptr createDebugConsoleHandler();

            MessageHandler_ptr consoleHandler() const;
            MessageHandler_ptr debugConsoleHandler() const;

            typedef std::function<void (
                Message::Severity,
                std::string_view,   // subsystem (e.g. "SharedComponents")
                std::string_view,   // file
                std::string_view,   // function
                int,                // line
                std::string_view    // message
            )> MessageSendObserver;

            static void connectMessageSendObserver(MessageSendObserver);

            struct OES_CORE_UTILS_EXPORT ProgramOptions {
                static Message::Severity validateSeverity(const std::vector<std::string>&);
                static const char* logSeverity_;
            };

        private:
            friend class NotificationMessagesScopedBuffering;
            ConstMessageFilter_ptr severity_filter_;
            MessageFilterList filters_;
            MessageContextList contexts_;
            MessageHandlerList handlers_;
            const TextMessageFormatter pgLoggerFormatter_;

            static MessageSendObserver messageSendObserver_;
        };

#define OES_FATAL(...)           oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::fatal, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_CRITICAL(...)        oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::critical, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_ALERT(...)           oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::alert, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_ERROR(...)           oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::error, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_WARNING(...)         oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::warning, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_NOTICE(...)          oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::notice, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_CURSOR_NOTICE(...)   oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::cursor_notice, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_INFO(...)            oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::info, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_VERBOSE(...)         oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::verbose, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_DEBUG(...)           oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::debug, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define OES_STATUS(...)          oes::core::Logger::instance().sendFormatted(oes::core::Message::Severity::status, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

        /****************************************************************************/

        inline std::ostream& operator << (std::ostream& os, const Message& message) {
            return os << message.message();
        }

        inline void Logger::connectMessageSendObserver (MessageSendObserver messageSendObserver) {
            messageSendObserver_ = std::move(messageSendObserver);
        }

    } // namespace core
} // namespace oes
