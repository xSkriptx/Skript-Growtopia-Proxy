#pragma once
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <windows.h>
#include <mutex>

namespace core {
    class HostsManager {
    public:
        HostsManager() : hosts_path("C:\\Windows\\System32\\drivers\\etc\\hosts") {}

        bool add_entries_if_needed(const std::vector<std::string>& entries) {
            return modify_hosts_file(entries, true);
        }

        bool remove_entries(const std::vector<std::string>& entries) {
            return modify_hosts_file(entries, false);
        }

    private:
        bool modify_hosts_file(const std::vector<std::string>& entries, bool add) {
            static std::mutex hosts_mutex;
            std::lock_guard<std::mutex> lock(hosts_mutex);
            
            std::ifstream in(hosts_path);
            if (!in.is_open()) {
                return false;
            }

            std::vector<std::string> lines;
            std::string line;
            while (std::getline(in, line)) {
                lines.push_back(line);
            }
            in.close();

            bool modified = false;
            if (add) {
                for (const auto& entry : entries) {
                    if (std::find(lines.begin(), lines.end(), entry) == lines.end()) {
                        lines.push_back(entry);
                        modified = true;
                    }
                }
            } else {
                for (auto it = lines.begin(); it != lines.end(); ) {
                    if (std::find(entries.begin(), entries.end(), *it) != entries.end()) {
                        it = lines.erase(it);
                        modified = true;
                    } else {
                        ++it;
                    }
                }
            }

            if (!modified) return true;

            std::ofstream out(hosts_path);
            if (!out.is_open()) {
                return false;
            }

            for (const auto& l : lines) {
                out << l << "\n";
            }
            out.close();

            return true;
        }

        std::string hosts_path;
    };
}