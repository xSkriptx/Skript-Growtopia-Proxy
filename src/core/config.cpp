#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config.hpp"

namespace core {
static const std::map<std::string, ConfigStorage> config_defaults{
    { "server.port", 16999 },
    { "server.address", "www.growtopia1.com" },
    { "client.game_version", "5.21" },
    { "client.protocol", 312 },
    { "client.dnsServer", "cloudflare" },
    { "extension.ignore", std::vector<std::string>{ "0xdeadbeef" } },
    { "log.printMessage", true },
    { "log.printGameUpdatePacket", false },
    { "log.printVariant", true },
    { "filter.rate_limit", 300 },
    { "filter.allow_burst", true },
    { "display.visual_name", "" },
    { "display.title.g4g", false },
    { "display.title.maxlevel", false },
    { "display.title.dr", false },
    { "display.title.mentor", false },
    { "display.show_ping", false },
    
    { "proxy.enabled",  false },
    { "proxy.host",     std::string("") },
    { "proxy.port",     static_cast<unsigned int>(1080) },
    { "proxy.username", std::string("") },
    { "proxy.password", std::string("") },
};

Config::Config()
{
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (std::ifstream ifs{ "config.json" }; ifs.good()) {
        spdlog::info("Loading config file \"config.json\"...");

        nlohmann::json j{};
        ifs >> j;

        for (const auto& [key, value] : j.items()) {
            if (value.is_number_unsigned()) {
                config_[key] = value.get<unsigned int>();
            }
            else if (value.is_number_integer()) {
                config_[key] = value.get<int>();
            }
            else if (value.is_string()) {
                config_[key] = value.get<std::string>();
            }
            else if (value.is_boolean()) {
                config_[key] = value.get<bool>();
            }
            else if (value.is_array()) {
                if (!value.empty() && value[0].is_string()) {
                    config_[key] = value.get<std::vector<std::string>>();
                }
                else {
                    throw std::runtime_error{ "Invalid configuration array value type" };
                }
            }
            else {
                throw std::runtime_error{ "Invalid configuration value type" };
            }
        }
    }

    bool save_defaults{ false };
    for (const auto& [key, value] : config_defaults) {
        if (config_.contains(key)) {
            continue;
        }

        spdlog::warn("Configuration key \"{}\" is missing, setting default value", key);
        config_[key] = value;
        save_defaults = true;
    }

    if (save_defaults) {
        nlohmann::json j{};
        for (const auto& [key, value] : config_) {
            std::visit([&]<typename U>(U val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, unsigned int>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    j[key] = val;
                }
            }, value);
        }

        std::ofstream ofs{ "config.json" };
        ofs << std::setw(4) << j << std::endl;
    }

    spdlog::info("Configuration loaded successfully");
}

void Config::save()
{
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    try {
        nlohmann::json j{};
        for (const auto& [key, value] : config_) {
            std::visit([&]<typename U>(U val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, unsigned int>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    j[key] = val;
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    j[key] = val;
                }
            }, value);
        }

        std::ofstream ofs{ "config.json" };
        ofs << std::setw(4) << j << std::endl;
        spdlog::trace("Configuration saved to config.json");
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
    }
}
}
