#pragma once
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include "strenc.hpp"

namespace utils {

class APIClient {
public:
    struct LockeSighting {
        std::string world_name;
        std::string time_str;
    };

private:
    static EncStr enc_license_key_;

    struct VendingItem {
        uint32_t item_id;
        std::string item_name;
        int price;
        int x;
        int y;
    };

    static std::vector<VendingItem> vending_batch_;
    static std::string current_world_;
    static std::mutex vending_mutex_;
    static std::atomic<bool> worker_running_;
    static std::thread worker_thread_;
    static std::mutex online_mutex_;
    static std::atomic<bool> online_connected_;
    static std::string online_username_;
    static std::string online_world_;
    static std::string online_session_id_;

    static std::string get_api_url() {
        return "";
    }

    static std::string gen_session_id() {
        return "";
    }

    static void send_online_ping(bool) {}

    static void upload_worker() {}

    static void flush_vending_batch() {}

public:
    static void init() {}

    static void shutdown() {
        enc_license_key_.wipe();
    }

    static void set_license_key(const std::string& key) {
        enc_license_key_.store(key);
    }

    static void set_online_connected(bool, const std::string& = "", const std::string& = "") {}

    static void set_online_world(const std::string&) {}

    static void upload_vending_data(
        const std::string&,
        const std::string&,
        uint32_t,
        const std::string&,
        int,
        int,
        int,
        int)
    {}

    static bool upload_proxy_log(
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&)
    {
        return true;
    }

    static bool upload_locke_data(const std::string& world_name) {
        if (world_name.empty()) return false;
        try {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            std::string timestamp = ss.str();

            std::ofstream file("locke_sightings.dat", std::ios::app);
            if (file.is_open()) {
                file << "[" << timestamp << "] " << world_name << "\n";
                file.close();
                return true;
            }
        } catch (...) {}
        return false;
    }

    static std::vector<LockeSighting> search_locke_data(const std::string& world_query = "") {
        std::vector<LockeSighting> out;
        try {
            std::ifstream file("locke_sightings.dat");
            if (!file.is_open()) return out;

            auto to_lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return s;
            };
            std::string q_l = to_lower(world_query);

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] != '[') continue;
                size_t close_bracket = line.find(']');
                if (close_bracket == std::string::npos) continue;

                std::string time_str = line.substr(1, close_bracket - 1);
                std::string world_name = line.substr(close_bracket + 2);
                
                while (!world_name.empty() && std::isspace(static_cast<unsigned char>(world_name.back()))) {
                    world_name.pop_back();
                }

                std::string w_l = to_lower(world_name);
                if (world_query.empty() || w_l.find(q_l) != std::string::npos) {
                    LockeSighting s{};
                    s.world_name = world_name;
                    s.time_str = time_str;
                    out.push_back(s);
                }
            }
            file.close();
            std::reverse(out.begin(), out.end());
        } catch (...) {}
        return out;
    }
};

inline EncStr      APIClient::enc_license_key_;
inline std::vector<APIClient::VendingItem> APIClient::vending_batch_;
inline std::string APIClient::current_world_;
inline std::mutex  APIClient::vending_mutex_;
inline std::atomic<bool> APIClient::worker_running_{false};
inline std::thread APIClient::worker_thread_;
inline std::mutex APIClient::online_mutex_;
inline std::atomic<bool> APIClient::online_connected_{false};
inline std::string APIClient::online_username_;
inline std::string APIClient::online_world_;
inline std::string APIClient::online_session_id_;

}
