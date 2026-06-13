#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace command::doorbf_tree {

struct InputResult {
    int max_attempts = 2500;
    bool show_help = false;
    bool show_list = false;
    bool clear_list = false;
    bool has_custom_ids = false;
    std::vector<std::string> custom_ids;
    std::string error;
};

InputResult parse_input(const std::vector<std::string>& args, int default_max_attempts = 2500);

void save_custom_ids(const std::vector<std::string>& ids);
void clear_custom_ids();
std::vector<std::string> load_custom_ids(int max_items);
size_t saved_custom_count();

} 
