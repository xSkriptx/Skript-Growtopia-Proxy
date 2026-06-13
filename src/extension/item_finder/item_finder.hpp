#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <cctype>
#include <sstream>

namespace extension::item_finder {

struct ItemInfo {
    int id;
    std::string name;
    int type;
    int rarity;
    std::string file_name;
    int clothing_type;
    int properties;
    std::string description;
    std::string info; 
};

class ItemDatabase {
public:
    ItemDatabase() = default;
    
    bool load_from_json(const std::string& json_path) {
        try {
            std::ifstream file(json_path);
            if (!file.is_open()) {
                return false;
            }
            
            nlohmann::json j;
            file >> j;
            
            items_.clear();
            
            for (const auto& item_json : j["items"]) {
                ItemInfo item;
                item.id = item_json.value("id", -1);
                item.name = item_json.value("name", "Unknown");
                item.type = item_json.value("type", 0);
                item.rarity = item_json.value("rarity", 0);
                item.file_name = item_json.value("file_name", "");
                item.clothing_type = item_json.value("clothing_type", 0);
                item.properties = item_json.value("properties", 0);
                item.info = item_json.value("_unk10", ""); 
                
                
                item.description = build_description(item_json);
                
                items_.push_back(item);
            }
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    std::vector<ItemInfo> search_items(const std::string& query, int max_results = 20, const std::string& search_type = "all") const {
        std::vector<ItemInfo> results;
        std::string query_lower = to_lower(query);
        
        if (query_lower.empty()) {
            return results;
        }
        
        for (const auto& item : items_) {
            std::string name_lower = to_lower(item.name);
            
            
            bool type_match = true;
            if (search_type == "items") {
                
                
                type_match = (name_lower.find(" seed") == std::string::npos);
            } else if (search_type == "seeds") {
                
                
                type_match = (name_lower.find(" seed") != std::string::npos);
            }
            
            
            if (!type_match) continue;
            
            
            if (name_lower.find(query_lower) != std::string::npos ||
                std::to_string(item.id) == query) {
                results.push_back(item);
                
                if (results.size() >= max_results) {
                    break;
                }
            }
        }
        
        return results;
    }
    
    std::string build_dialog(const std::string& query, const std::vector<ItemInfo>& results, const std::string& search_type = "all") const {
        std::ostringstream dialog;
        
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wItem Finder``|left|1796|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        if (query.empty() && search_type.empty()) {
            dialog << "add_textbox|`oWhat would you like to search for?``|left|\n";
            dialog << "add_spacer|small|\n";
            
            
            dialog << "add_label_with_icon|big|`2Search Items Only``|left|242|\n";
            dialog << "add_smalltext|`oSearch for blocks, tools, and consumables``|\n";
            dialog << "add_button|search_items|`2Select Items Only``|\n";
            dialog << "add_spacer|small|\n";
            
            dialog << "add_label_with_icon|big|`5Search Items & Seeds``|left|5|\n";
            dialog << "add_smalltext|`oSearch for everything``|\n";
            dialog << "add_button|search_all|`5Select Items & Seeds``|\n";
            dialog << "add_spacer|small|\n";
            
            dialog << "add_label_with_icon|big|`9Search Seeds Only``|left|5|\n";
            dialog << "add_smalltext|`oSearch for seeds only``|\n";
            dialog << "add_button|search_seeds|`9Select Seeds Only``|\n";
            dialog << "add_spacer|small|\n";
            
            dialog << "add_quick_exit|\n";
            dialog << "end_dialog|itemfinder|Close||\n";
            return dialog.str();
        }
        
        
        std::string search_type_label;
        if (search_type == "items") search_type_label = " (Items Only)";
        else if (search_type == "seeds") search_type_label = " (Seeds Only)";
        else search_type_label = " (All)";
        
        dialog << "add_textbox|`5Search Type:" << search_type_label << "``|left|\n";
        dialog << "add_text_input|itemfind_search|Search:|" << query << "|30|\n";
        dialog << "add_spacer|small|\n";
        
        if (results.empty() && !query.empty()) {
            dialog << "add_textbox|`4No items found for: `w" << query << "``|\n";
            dialog << "add_spacer|small|\n";
        } else if (!query.empty()) {
            dialog << "add_textbox|`2Found " << results.size() << " items``|\n";
            dialog << "add_spacer|small|\n";
            
            
            for (const auto& item : results) {
                
                dialog << "add_label_with_icon|big|`w" << item.name << "``|left|" << item.id << "|\n";
                
                
                dialog << "add_smalltext|`o" << item.description << "``|\n";
                dialog << "add_smalltext|`5ID: " << item.id << " | Type: " << static_cast<int>(item.type) << " | Rarity: " << static_cast<int>(item.rarity) << "``|\n";
                
                
                dialog << "add_button|viewdetail_" << item.id << "|`2View Full Details``|\n";
                dialog << "add_button|wear_" << item.id << "|`9Wear Item``|\n";
                
                
                dialog << "add_spacer|small|\n";
            }
            
            if (results.size() >= 20) {
                dialog << "add_textbox|`4Showing first 20 results. Refine your search for more specific results.``|\n";
                dialog << "add_spacer|small|\n";
            }
        } else {
            dialog << "add_textbox|`oEnter an item name or ID to search``|\n";
            dialog << "add_textbox|`oExample: dirt, laser, 242``|\n";
        }
        
        dialog << "add_spacer|small|\n";
        dialog << "end_dialog|itemfinder|Close|Search|\n";
        
        return dialog.str();
    }
    
    const ItemInfo* get_item_by_id(int id) const {
        if (id >= 0 && id < items_.size()) {
            return &items_[id];
        }
        return nullptr;
    }
    
    size_t get_item_count() const {
        return items_.size();
    }

private:
    std::vector<ItemInfo> items_;
    
    static std::string to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }
    
    static std::string build_description(const nlohmann::json& item_json) {
        std::ostringstream desc;
        
        int type = item_json.value("type", 0);
        desc << "Type: ";
        switch (type) {
            case 0: desc << "Block"; break;
            case 1: desc << "Door"; break;
            case 2: desc << "Lock"; break;
            case 3: desc << "Gems"; break;
            case 4: desc << "Sign"; break;
            case 5: desc << "SFX Foreground"; break;
            case 6: desc << "Toggleable Foreground"; break;
            case 7: desc << "Main Door"; break;
            case 8: desc << "Platform"; break;
            case 9: desc << "Bedrock"; break;
            case 10: desc << "Lava"; break;
            case 11: desc << "Foreground"; break;
            case 12: desc << "Background"; break;
            case 13: desc << "Seed"; break;
            case 14: desc << "Clothing"; break;
            case 15: desc << "Animated Block"; break;
            case 16: desc << "SFX Background"; break;
            case 17: desc << "Toggleable Background"; break;
            case 18: desc << "Bouncy"; break;
            case 19: desc << "Checkpoint"; break;
            case 20: desc << "Gateway"; break;
            case 21: desc << "Treasure"; break;
            case 22: desc << "Deadly Block"; break;
            case 23: desc << "Trampoline"; break;
            case 24: desc << "Consumable"; break;
            default: desc << "Unknown"; break;
        }
        
        desc << " | Rarity: " << item_json.value("rarity", 0);
        
        
        
        return desc.str();
    }
};

} 
