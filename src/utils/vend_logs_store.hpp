#pragma once
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace utils {

struct VendLogEntry {
    std::string time_str;
    std::string message;
};

class VendLogsStore {
public:
    static void try_add(const std::string& message) {
        if (!is_vend_purchase_message(message)) return;

        std::lock_guard<std::mutex> lock(mutex_());
        auto& logs = logs_();
        logs.push_back({now_local_time(), message});
        constexpr size_t kMaxLogs = 300;
        if (logs.size() > kMaxLogs) {
            logs.erase(logs.begin(), logs.begin() + (logs.size() - kMaxLogs));
        }
    }

    static std::vector<VendLogEntry> snapshot() {
        std::lock_guard<std::mutex> lock(mutex_());
        return logs_();
    }

    static size_t size() {
        std::lock_guard<std::mutex> lock(mutex_());
        return logs_().size();
    }

private:
    static bool contains_ci(std::string haystack, std::string needle) {
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(needle) != std::string::npos;
    }

    static bool is_vend_purchase_message(const std::string& msg) {
        
        
        return contains_ci(msg, " bought ") && contains_ci(msg, " for ") &&
               (contains_ci(msg, "world lock") || contains_ci(msg, "diamond lock") || contains_ci(msg, "blue gem lock"));
    }

    static std::string now_local_time() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%H:%M:%S");
        return oss.str();
    }

    static std::vector<VendLogEntry>& logs_() {
        static std::vector<VendLogEntry> logs;
        return logs;
    }

    static std::mutex& mutex_() {
        static std::mutex m;
        return m;
    }
};

} 

