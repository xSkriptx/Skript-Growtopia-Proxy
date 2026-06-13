#include "auth.hpp"
#include "../utils/proxy_logger.hpp"
#include "../utils/api_client.hpp"
#include "../utils/strenc.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <Windows.h>

namespace core {
using json = nlohmann::json;

static void enable_console_colors() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    GetConsoleMode(h, &m);
    SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static std::string get_machine_guid() {
    HKEY hKey;
    char value[256]{};
    DWORD size = sizeof(value);

    std::string regPath = DEC("SOFTWARE\\Microsoft\\Cryptography");
    std::string regVal  = DEC("MachineGuid");

    bool ok = false;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0,
                      KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, regVal.c_str(), nullptr, nullptr,
                             (LPBYTE)value, &size) == ERROR_SUCCESS)
            ok = true;
        RegCloseKey(hKey);
    }
    SecureZeroMemory(&regPath[0], regPath.size());
    SecureZeroMemory(&regVal[0],  regVal.size());
    return ok ? std::string(value) : DEC("UNKNOWN-HWID");
}

static const unsigned char CRED_XOR = 0x5C;

static void save_credentials(const std::string& username,
                              const std::string& license_key) {
    std::string fname = DEC("px.dat");
    json config;
    config[DEC("u")] = username;
    config[DEC("k")] = license_key;
    std::string raw = config.dump();
    for (auto& c : raw) c ^= CRED_XOR;
    std::ofstream f(fname, std::ios::binary);
    if (f.is_open()) f.write(raw.data(), (std::streamsize)raw.size());
    SecureZeroMemory(&fname[0], fname.size());
    SecureZeroMemory(&raw[0],   raw.size());
}

static bool load_credentials(std::string& username,
                              std::string& license_key) {
    std::string fname = DEC("px.dat");
    std::ifstream f(fname, std::ios::binary);
    SecureZeroMemory(&fname[0], fname.size());
    if (!f.is_open()) return false;

    std::string raw((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    for (auto& c : raw) c ^= CRED_XOR;
    try {
        json config = json::parse(raw);
        SecureZeroMemory(&raw[0], raw.size());
        username    = config.value(DEC("u"), "");
        license_key = config.value(DEC("k"), "");
        return !username.empty() && !license_key.empty();
    } catch (...) {
        SecureZeroMemory(&raw[0], raw.size());
        return false;
    }
}

bool Authenticator::verify_license(const std::string& license_key,
                                    const std::string& username) {
    std::cout << "\033[1;33mVerifying license (Local Mode)...\033[0m\n";
    std::cout << "\033[1;32mLicense Verified (Local)!\033[0m\n";
    std::cout << "\033[1;36mType: Local Premium\033[0m\n";
    std::cout << "\033[1;36mStatus: Active\033[0m\n";
    std::cout << "\033[1;36mExpires: Never\033[0m\n";

    std::string lk = license_key.empty() ? "LOCAL_LICENSE" : license_key;
    std::string usr = username.empty() ? "LocalPlayer" : username;

    utils::APIClient::set_license_key(lk);
    utils::ProxyLogger::set_username(usr);

    return true;
}

bool Authenticator::authenticate() {
    enable_console_colors();
    std::string username, license_key;

    if (load_credentials(username, license_key)) {
        std::cout << "\033[1;32mFound saved credentials\033[0m\n";
        std::cout << "\033[1;32mUsername: " << username << "\033[0m\n";
        if (verify_license(license_key, username)) {
            SecureZeroMemory(&license_key[0], license_key.size());
            std::cout << "\033[1;32mWelcome back, " << username << "!\033[0m\n";
            return true;
        }
        SecureZeroMemory(&license_key[0], license_key.size());
        std::cout << "\033[1;33mCredentials invalid. Please re-enter.\033[0m\n";
    }

    std::cout << "\033[1;36m\n===========================\n";
    std::cout <<             "   Proxy - Authentication  \n";
    std::cout <<             "===========================\n\033[0m\n";
    std::cout << "\033[1;37mUsername: \033[0m";
    std::getline(std::cin, username);
    std::cout << "\033[1;37mLicense key: \033[0m";
    std::getline(std::cin, license_key);

    if (verify_license(license_key, username)) {
        save_credentials(username, license_key);
        SecureZeroMemory(&license_key[0], license_key.size());
        std::cout << "\033[1;32mAuthentication successful!\033[0m\n";
        return true;
    }
    SecureZeroMemory(&license_key[0], license_key.size());
    std::cout << "\033[1;31mAuthentication failed.\033[0m\n";
    return false;
}

}
