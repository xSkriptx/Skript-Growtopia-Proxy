#pragma once
#include <httplib.h>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <regex>
#include <chrono>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#include "web_server.hpp"
#include "../../utils/strenc.hpp"
#include "../../utils/local_cert.hpp"
#include "../../client/client.hpp"
#include "../../core/core.hpp"
#include "../../core/logger.hpp"
#include "../../utils/network.hpp"
#include "../../utils/text_parse.hpp"

namespace extension::web_server {
class WebServerExtension final : public IWebServerExtension {
    core::Core* core_;
    httplib::SSLServer server_;

    std::string address_;
    uint16_t port_;
    std::string installed_thumbprint_;

public:
    explicit WebServerExtension(core::Core* core)
        : core_{ core }
        , server_{ ensure_local_cert(), "./resources/key.pem" }
        , port_{ 65535 }
    {
    }

    ~WebServerExtension() override
    {
        server_.stop();
        remove_root_certificate();
    }

    void init() override
    {
        core_->get_event_dispatcher().prependListener(
            core::EventType::Connection,
            [&](const core::EventConnection& evt)
            {
                if (evt.from != core::EventFrom::FromClient) {
                    return;
                }

                
                if (evt.get_player().get_peer()->address.host != 16777343) {
                    spdlog::info("Security alert: External connection attempt blocked");
                    return;
                }

                if (address_.empty() || port_ == 65535) {
                    return;
                }

                std::ignore = core_->get_client()->connect(address_, port_);
                evt.canceled = true;

                address_.clear();
                port_ = 65535;
            }
        );

        
        check_ca_certificate();
        install_root_certificate("./resources/cert.pem");

        
        server_.set_logger([](const httplib::Request& req, const httplib::Response& res)
        {
            std::string method_color = "\033[1;36m"; 
            std::string status_color = res.status >= 400 ? "\033[1;31m" : "\033[1;32m"; 
            
            spdlog::info("HTTP {} {}{}\033[0m -> {}{}\033[0m", 
                method_color, req.method, req.path,
                status_color, res.status);
        });

        server_.set_error_handler([](const httplib::Request&, httplib::Response& res)
        {
            nlohmann::json error_response = {
                {"status", "error"},
                {"code", res.status},
                {"message", httplib::status_message(res.status)},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
            
            res.set_content(error_response.dump(), "application/json");
        });

        server_.set_exception_handler([](const httplib::Request&, httplib::Response& res, const std::exception_ptr& ep)
        {
            res.status = 500;
            
            nlohmann::json error_response = {
                {"status", "error"},
                {"code", 500},
                {"message", "Internal server error"},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };

            try {
                std::rethrow_exception(ep);
            }
            catch (std::exception &e) {
                error_response["details"] = e.what();
            }
            catch (...) {
                error_response["details"] = "Unknown exception occurred";
            }

            res.set_content(error_response.dump(), "application/json");
        });

        if (!server_.bind_to_port("0.0.0.0", 443)) {
            spdlog::info("HTTPS server failed to bind to port 443");
            return;
        }

        spdlog::trace("HTTPS server initialized on port 443");
        std::thread{ &WebServerExtension::listen_internal, this }.detach();
    }

    void free() override
    {
        delete this;
    }

private:
    void check_ca_certificate() {
        std::string ca_cert_path = "./resources/cacert.pem";
        if (std::filesystem::exists(ca_cert_path)) {
            std::ifstream file(ca_cert_path);
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            if (content.size() > 100) {
                spdlog::trace("CA certificate found ({} bytes): {}", content.size(), ca_cert_path);
            } else {
                spdlog::warn("CA certificate file is too small ({} bytes). Download fresh copy.", content.size());
            }
        } else {
            spdlog::warn("CA certificate not found: {}", ca_cert_path);
        }
    }

    // The new client rejects untrusted HTTPS certs, so the local cert must be in the
    // Windows Root store. Idempotent (-f). Needs admin, which the hosts edit already requires.
    static bool run_certutil(std::string cmd)
    {
#ifdef _WIN32
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};

        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            return false;
        }

        WaitForSingleObject(pi.hProcess, 15000);
        DWORD exit_code = 1;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exit_code == 0;
#else
        (void)cmd;
        return false;
#endif
    }

    void install_root_certificate(const std::string& path)
    {
        const std::string thumb = utils::local_cert::thumbprint(path);
        if (thumb.empty()) {
            spdlog::error("Cannot read {}; the game will reject the proxy HTTPS certificate", path);
            return;
        }

        if (run_certutil("certutil -addstore -f ROOT \"" + path + "\"")) {
            installed_thumbprint_ = thumb;
            spdlog::info("Local HTTPS certificate installed in Windows Root store");
        }
        else {
            spdlog::error("certutil failed; run the proxy as Administrator");
        }
    }

    void remove_root_certificate()
    {
        if (installed_thumbprint_.empty()) {
            return;
        }

        if (run_certutil("certutil -delstore ROOT " + installed_thumbprint_)) {
            spdlog::info("Local HTTPS certificate removed from Windows Root store");
        }
        installed_thumbprint_.clear();
    }

    // Runs before server_ is constructed so the SSL server finds its files.
    static const char* ensure_local_cert()
    {
        utils::local_cert::ensure("./resources/cert.pem", "./resources/key.pem");
        return "./resources/cert.pem";
    }

    bool validate_server_response(const httplib::Result& response)
    {
        if (!response) {
            spdlog::info("Network error: {}", httplib::to_string(response.error()));
            return false;
        }

        if (response->status != 200) {
            spdlog::info("Server responded with status: {}", response->status);
            return false;
        }

        return true;
    }

    
    std::unique_ptr<httplib::SSLClient> create_ssl_client(const std::string& host) {
        auto cli = std::make_unique<httplib::SSLClient>(host);
        cli->set_connection_timeout(10);
        cli->set_read_timeout(30);
        cli->set_write_timeout(10);
        
        
        std::string ca_cert_path = "./resources/cacert.pem";
        if (std::filesystem::exists(ca_cert_path)) {
            cli->set_ca_cert_path(ca_cert_path);
            spdlog::debug("Using CA certificate: {}", ca_cert_path);
        }
        
        
        cli->enable_server_certificate_verification(true);
        
        return cli;
    }

    std::string resolve_domain_name(const std::string& domain_name)
    {
        std::string host = core_->get_config().get("client.dnsServer") == "cloudflare" 
            ? "cloudflare-dns.com" 
            : "dns.google";
        
        std::string path = core_->get_config().get("client.dnsServer") == "cloudflare" 
            ? "/dns-query" 
            : "/resolve";

        httplib::Headers headers = {
            { "Accept", "application/dns-json" },
            { "User-Agent", "SkriptProxy/2.0" }
        };

        auto cli = create_ssl_client(host);
        
        auto res = cli->Get(path + "?name=" + domain_name + "&type=A", headers);
        
        if (!validate_server_response(res)) {
            return "";
        }

        try {
            auto j = nlohmann::json::parse(res->body);
            if (j["Status"] != 0) {
                return "";
            }
            
            return j["Answer"].back()["data"].get<std::string>();
        }
        catch (...) {
            return "";
        }
    }

    void listen_internal()
    {
        
        server_.Post("/growtopia/server_data.php", [&](
            const httplib::Request& req,
            httplib::Response& res
        ) {
            
            res.set_header("X-Content-Type-Options", "nosniff");
            res.set_header("X-Frame-Options", "DENY");
            res.set_header("X-XSS-Protection", "1; mode=block");
            res.set_header("Strict-Transport-Security", "max-age=31536000");

            std::string target_server = resolve_domain_name(
                core_->get_config().get("server.address")
            );

            if (target_server.empty()) {
                res.status = 502;
                res.set_content("Failed to resolve server address", "text/plain");
                return true;
            }

            spdlog::info("Connecting to Growtopia server: {}", target_server);
            
            bool connection_success = false;
            std::string response_body;
            
            
            {
                auto cli = create_ssl_client(target_server);
                auto result = cli->Post("/growtopia/server_data.php", 
                    {{ "User-Agent", req.get_header_value("User-Agent") },
                     { "Host", core_->get_config().get("server.address") },
                     { "Content-Type", "application/x-www-form-urlencoded" }},
                    req.body, "application/x-www-form-urlencoded");
                
                if (validate_server_response(result)) {
                    connection_success = true;
                    response_body = result->body;
                    spdlog::info("Successfully connected using SSL verification");
                } else {
                    spdlog::warn("SSL verification failed, trying without verification...");
                }
            }
            
            
            if (!connection_success) {
                spdlog::warn("Trying without SSL verification...");
                httplib::SSLClient cli{ target_server };
                cli.set_connection_timeout(5);
                cli.set_read_timeout(15);
                cli.enable_server_certificate_verification(false);  
                
                auto result = cli.Post("/growtopia/server_data.php", 
                    {{ "User-Agent", req.get_header_value("User-Agent") },
                     { "Host", core_->get_config().get("server.address") },
                     { "Content-Type", "application/x-www-form-urlencoded" }},
                    req.body, "application/x-www-form-urlencoded");
                
                if (validate_server_response(result)) {
                    connection_success = true;
                    response_body = result->body;
                    spdlog::warn("Connected without SSL verification (INSECURE)");
                }
            }
            
            if (!connection_success) {
                res.status = 502;
                res.set_content("Cannot connect to Growtopia server", "text/plain");
                spdlog::error("All connection attempts failed to {}", target_server);
                return true;
            }

            try {
                TextParse text_parse{ response_body };
                if (text_parse.empty()) {
                    res.status = 502;
                    res.set_content("Invalid server response", "text/plain");
                    return true;
                }

                
                address_ = text_parse.get("server");
                port_ = std::stoi(text_parse.get("port"));

                
                text_parse.set("server", { "127.0.0.1" });
                text_parse.set("port", { 
                    std::to_string(core_->get_config().get<unsigned int>("server.port")) 
                });
                text_parse.set("type2", { "1" });

                res.set_content(text_parse.get_raw(), "text/html");
                spdlog::info("Successfully proxied request to Growtopia");
                return true;
            }
            catch (const std::exception& e) {
                spdlog::info("Error processing server response: {}", e.what());
                res.status = 500;
                res.set_content("Internal server error", "text/plain");
                return true;
            }
            catch (...) {
                spdlog::info("Unknown error processing server response");
                res.status = 500;
                res.set_content("Internal server error", "text/plain");
                return true;
            }
        });

        
        server_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            nlohmann::json health_status = {
                {"status", "healthy"},
                {"service", "skript-proxy"},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
            res.set_content(health_status.dump(), "application/json");
        });

        
        server_.Get("/logs", [](const httplib::Request&, httplib::Response& res) {
            auto logs = core::Logger::read_log_file();
            nlohmann::json log_data = {
                {"logs", logs},
                {"count", logs.size()}
            };
            res.set_content(log_data.dump(), "application/json");
        });

        spdlog::trace("HTTP server endpoints registered");
        server_.listen_after_bind();
    }
};
}
