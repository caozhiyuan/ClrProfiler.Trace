#ifndef CLR_PROFILER_LOGGING_H_
#define CLR_PROFILER_LOGGING_H_

#include "util.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

namespace trace {

    class CLogger : public Singleton<CLogger>
    {
        friend class Singleton<CLogger>;
    private:
        static WSTRING GetLogPath()
        {
            WSTRING log_path;
            auto home = GetEnvironmentValue(GetClrProfilerHome());
            if(!home.empty()) {
                log_path = home + PathSeparator + "logs"_W;
            }
            else {
                log_path = "logs"_W;
            }
            return log_path;
        }

        CLogger() {

            spdlog::set_error_handler([](const std::string& msg)
            {
                std::cerr << "Logger Handler: " << msg << std::endl;
            });

            spdlog::flush_every(std::chrono::seconds(5));

            const WSTRING log_path = GetLogPath();

            CheckDir(ToString(log_path).c_str());

            const auto log_name = log_path + PathSeparator + "trace"_W + ToWSTRING(std::to_string(GetPID())) + ".log"_W;
            m_fileout = spdlog::rotating_logger_mt("Logger", ToString(log_name), 1024 * 1024 * 10, 3);

            m_fileout->set_level(spdlog::level::info);

            m_fileout->set_pattern("[%Y-%m-%d %T.%e] [%l] [thread %t] %v");

            m_fileout->flush_on(spdlog::level::err);
        };

        ~CLogger()
        {
            spdlog::drop_all();
        };

    public:
        std::shared_ptr<spdlog::logger> m_fileout;
    };

#define Info( ... )                               \
    {                                                 \
        CLogger::Instance()->m_fileout->info(__VA_ARGS__);  \
    }

#define Warn( ... )                               \
    {                                                 \
        CLogger::Instance()->m_fileout->warn(__VA_ARGS__);  \
    }

#define Error( ... )                               \
    {                                                 \
        CLogger::Instance()->m_fileout->error(__VA_ARGS__);   \
    }
}  // namespace trace

#endif  // CLR_PROFILER_LOGGING_H_
