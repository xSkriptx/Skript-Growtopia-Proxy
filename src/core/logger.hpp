#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include <vector>
#include <mutex>

namespace core {
class Logger {
public:
    Logger() = default;
    ~Logger() { 
        if (logger_) {
            logger_->flush(); 
        }
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> create_console_sink()
    {
        auto console_sink{ std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
        
        
        console_sink->set_pattern("%^%v%$");
        
#ifdef GTPROXY_DEBUG
        console_sink->set_level(spdlog::level::trace);
#else
        console_sink->set_level(spdlog::level::debug);
#endif
        
        return console_sink;
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_file_sink()
    {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "proxy.log",
            1024 * 1024 * 2,
            4
        );
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] %v");
        return file_sink;
    }

    [[nodiscard]] static std::vector<std::string> read_log_file(int max_lines = 500) {
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);
        
        std::vector<std::string> logs;
        std::ifstream file("proxy.log");
        
        if (!file.is_open()) {
            return logs;
        }

        std::string line;
        while (std::getline(file, line)) {
            logs.push_back(line);
        }

        if (logs.size() > max_lines) {
            logs.erase(logs.begin(), logs.end() - max_lines);
        }

        return logs;
    }

    [[nodiscard]] std::shared_ptr<spdlog::logger> get_logger() const { return logger_; }
    void set_logger(std::shared_ptr<spdlog::logger> logger) { logger_ = std::move(logger); }

private:
    std::shared_ptr<spdlog::logger> logger_;
};
}
