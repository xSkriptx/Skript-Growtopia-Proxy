#include "doorbf_tree.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <mutex>
#include <unordered_set>

namespace command::doorbf_tree {

namespace {

std::vector<std::string> g_saved_custom_ids;
std::mutex g_saved_custom_ids_mutex;
bool g_loaded_from_disk = false;
constexpr const char* kDoorbfIdsFile = "doorbf_ids.txt";

bool is_number_token(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::string to_lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> expand_custom_tokens(const std::vector<std::string>& args, size_t start_idx, int max_items) {
    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(max_items));
    std::unordered_set<std::string> seen;

    auto push_unique = [&](std::string id) {
        auto not_space = [](unsigned char c) { return !std::isspace(c); };
        id.erase(id.begin(), std::find_if(id.begin(), id.end(), not_space));
        id.erase(std::find_if(id.rbegin(), id.rend(), not_space).base(), id.end());
        if (id.empty() || static_cast<int>(out.size()) >= max_items) return;

        const std::string lower = to_lower_copy(id);
        
        if (lower == "\\n" || lower == "\\r" || lower == "\\t" || lower == "\\r\\n") return;

        
        std::string filtered;
        filtered.reserve(id.size());
        for (unsigned char c : id) {
            if (std::isalnum(c) || c == '_' || c == '-') {
                filtered.push_back(static_cast<char>(c));
            }
        }
        if (filtered.empty()) return;
        if (seen.insert(filtered).second) out.push_back(filtered);
    };

    for (size_t i = start_idx; i < args.size() && static_cast<int>(out.size()) < max_items; ++i) {
        const std::string token = args[i];
        const size_t dash = token.find('-');
        if (dash != std::string::npos) {
            const std::string left = token.substr(0, dash);
            const std::string right = token.substr(dash + 1);
            if (is_number_token(left) && is_number_token(right)) {
                int a = std::stoi(left);
                int b = std::stoi(right);
                if (a <= b) {
                    b = std::min(b, a + 200000);
                    for (int n = a; n <= b && static_cast<int>(out.size()) < max_items; ++n) {
                        push_unique(std::to_string(n));
                    }
                    continue;
                }
            }
        }
        push_unique(token);
    }
    return out;
}

std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

void ensure_loaded_locked() {
    if (g_loaded_from_disk) return;
    g_loaded_from_disk = true;

    std::ifstream in(kDoorbfIdsFile);
    if (!in.is_open()) return;

    std::unordered_set<std::string> seen;
    for (std::string line; std::getline(in, line);) {
        line = trim_copy(line);
        if (line.empty()) continue;
        if (seen.insert(line).second) {
            g_saved_custom_ids.push_back(line);
        }
    }
}

void save_to_disk_locked() {
    std::ofstream out(kDoorbfIdsFile, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& id : g_saved_custom_ids) {
        out << id << "\n";
    }
}

} 

InputResult parse_input(const std::vector<std::string>& args, int default_max_attempts) {
    InputResult result{};
    result.max_attempts = default_max_attempts;

    
    
    size_t token_start = 1;

    if (token_start < args.size()) {
        const std::string mode = to_lower_copy(args[token_start]);
        if (mode == "?help") {
            result.show_help = true;
            return result;
        }
        if (mode == "?list") {
            result.show_list = true;
            return result;
        }
        if (mode == "?clearlist") {
            result.clear_list = true;
            return result;
        }

        result.custom_ids = expand_custom_tokens(args, token_start, result.max_attempts);
        if (result.custom_ids.empty()) {
            result.error = "DoorBF: no valid ids parsed from your list.";
            return result;
        }
        result.has_custom_ids = true;
    }

    return result;
}

void save_custom_ids(const std::vector<std::string>& ids) {
    std::lock_guard<std::mutex> lock(g_saved_custom_ids_mutex);
    ensure_loaded_locked();
    g_saved_custom_ids = ids;
    save_to_disk_locked();
}

void clear_custom_ids() {
    std::lock_guard<std::mutex> lock(g_saved_custom_ids_mutex);
    ensure_loaded_locked();
    g_saved_custom_ids.clear();
    save_to_disk_locked();
}

std::vector<std::string> load_custom_ids(int max_items) {
    std::lock_guard<std::mutex> lock(g_saved_custom_ids_mutex);
    ensure_loaded_locked();
    std::vector<std::string> out = g_saved_custom_ids;
    if (max_items > 0 && static_cast<int>(out.size()) > max_items) {
        out.resize(static_cast<size_t>(max_items));
    }
    return out;
}

size_t saved_custom_count() {
    std::lock_guard<std::mutex> lock(g_saved_custom_ids_mutex);
    ensure_loaded_locked();
    return g_saved_custom_ids.size();
}

} 
