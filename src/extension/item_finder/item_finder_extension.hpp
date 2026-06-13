#pragma once
#include "../extension.hpp"
#include "item_finder.hpp"
#include "../../proxy_imgui_gui.hpp" 
#include "../../core/core.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../command_handler/vendloc_command.hpp"
#include "../command_handler/clothes_command.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <unordered_map>
#include <thread>
#include <chrono>

namespace extension::item_finder {

class ItemFinderExtension final : public IExtension {
    core::Core* core_;
    std::unique_ptr<ItemDatabase> database_;
    std::string items_json_path_;
    
    
    std::unordered_map<uint32_t, std::string> player_searches_;
    std::unordered_map<uint32_t, std::string> player_search_types_;
    
public:
    PROVIDE_EXT_UID(0x4954454D); 

    explicit ItemFinderExtension(core::Core* core) 
        : core_{ core }
        , database_(std::make_unique<ItemDatabase>())
    {
        
        items_json_path_ = find_items_json();
    }
    
    ~ItemFinderExtension() override = default;

    
    ItemDatabase* get_database() const {
        return database_.get();
    }

    void init() override {
        
        if (!items_json_path_.empty() && database_->load_from_json(items_json_path_)) {
            spdlog::trace("Item database loaded successfully: {} items", database_->get_item_count());
            
            
            command::VendLocCommand::set_item_database(database_.get());
            command::VendTPCommand::set_item_database(database_.get());
            
            
            SetItemDatabase(database_.get());
        } else {
            spdlog::warn("Failed to load items database from: {}", items_json_path_);
            spdlog::warn("Item finder will not be available");
            return;
        }
        
        
        core_->get_event_dispatcher().appendListener(
            core::EventType::Message,
            [this](const core::EventMessage& evt) {
                if (evt.from != core::EventFrom::FromClient) {
                    return;
                }
                
                
                const auto& text_parse = evt.get_message();
                std::string action = text_parse.get("action");
                
                if (action == "input") {
                    std::string text = text_parse.get("text");
                    
                    
                    if (text.rfind("/find", 0) == 0) {
                        handle_find_command(evt, text);
                    }
                } else if (action == "dialog_return") {
                    std::string dialog_name = text_parse.get("dialog_name");
                    
                    
                    if (dialog_name == "itemfinder") {
                        handle_dialog_return(evt, text_parse);
                    }
                    else if (dialog_name == "itemdetail") {
                        handle_item_detail_return(evt, text_parse);
                    }
                }
            }
        );
        
        spdlog::trace("Item Finder extension loaded - Use /find to search items");
    }

    void free() override {
        delete this;
    }

private:
    std::string find_items_json() {
        
        std::vector<std::string> possible_paths = {
            "C:\\Users\\11User\\OneDrive\\Desktop\\botrdp\\dtm\\itemsdecoder\\decoded_items.json",
            "./resources/decoded_items.json",
            "./decoded_items.json",
            "../decoded_items.json"
        };
        
        for (const auto& path : possible_paths) {
            std::ifstream file(path);
            if (file.is_open()) {
                file.close();
                return path;
            }
        }
        return "";
    }
    
    void handle_find_command(const core::EventMessage& evt, const std::string& text) {
        
        evt.canceled = true;
        
        
        std::string query = "";
        if (text.length() > 6) { 
            query = text.substr(6);
        }
        
        
        show_item_finder_dialog(evt, query, "");
    }
    
    void show_item_finder_dialog(const core::EventMessage& evt, const std::string& query, const std::string& search_type = "all") {
        const player::Player& player = evt.get_player();
        
        
        uint32_t host = player.get_peer()->address.host;
        player_searches_[host] = query;
        player_search_types_[host] = search_type;
        
        
        auto results = database_->search_items(query, 20, search_type);
        
        
        std::string dialog_content = database_->build_dialog(query, results, search_type);
        
        try {
            packet::Variant variant{};
            variant.add("OnDialogRequest");
            variant.add(dialog_content);
            
            std::vector<std::byte> ext_data = variant.serialize();

            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = -1;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());

            const_cast<player::Player&>(player).send_packet(byte_stream.get_data(), 0);
            
            spdlog::info("Item finder dialog sent with query: '{}', type: '{}'", query, search_type);
        } catch (const std::exception& e) {
            spdlog::error("Failed to send item finder dialog: {}", e.what());
        }
    }
    
    void show_item_details_dialog(const core::EventMessage& evt, const ItemInfo* item, const std::string& return_query, const std::string& return_type) {
        if (!item) return;
        
        const player::Player& player = evt.get_player();
        
        
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`w" << item->name << "``|left|" << item->id << "|\n";
        dialog << "add_spacer|small|\n";
        
        dialog << "add_smalltext|`5Item ID: `w" << item->id << "``|\n";
        dialog << "add_smalltext|`5Rarity: `w" << static_cast<int>(item->rarity) << "``|\n";
        
        
        if (!item->info.empty() && item->info != "No info.") {
            dialog << "add_spacer|small|\n";
            dialog << "add_textbox|`5Info:``|left|\n";
            dialog << "add_smalltext|`o" << item->info << "``|\n";
        }
        
        if (item->clothing_type > 0) {
            std::string clothing_name;
            switch (item->clothing_type) {
                case 1: clothing_name = "Hair"; break;
                case 2: clothing_name = "Shirt"; break;
                case 3: clothing_name = "Pants"; break;
                case 4: clothing_name = "Feet"; break;
                case 5: clothing_name = "Face"; break;
                case 6: clothing_name = "Hand"; break;
                case 7: clothing_name = "Back"; break;
                case 8: clothing_name = "Mask"; break;
                case 9: clothing_name = "Necklace"; break;
                case 10: clothing_name = "Ances"; break;
                default: clothing_name = "Clothing"; break;
            }
            dialog << "add_smalltext|`5Clothing Type: `w" << clothing_name << "``|\n";
        }
        
        dialog << "add_spacer|small|\n";
        
        
        dialog << "add_text_input_with_info|return_query||" << return_query << "|\n";
        dialog << "add_text_input_with_info|return_type||" << return_type << "|\n";
        
        dialog << "add_button|back_to_search|`2Back to Search``|\n";
        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|itemdetail|Close||\n";
        
        try {
            packet::Variant variant{};
            variant.add("OnDialogRequest");
            variant.add(dialog.str());
            
            std::vector<std::byte> ext_data = variant.serialize();

            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = -1;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());

            const_cast<player::Player&>(player).send_packet(byte_stream.get_data(), 0);
            
            spdlog::info("Item details dialog sent for item: {}", item->name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to send item details dialog: {}", e.what());
        }
    }
    
    void handle_dialog_return(const core::EventMessage& evt, const TextParse& text_parse) {
        evt.canceled = true;
        
        const player::Player& player = evt.get_player();
        uint32_t host = player.get_peer()->address.host;
        std::string button_clicked = text_parse.get("buttonClicked");
        
        spdlog::info("Item finder button clicked: '{}'", button_clicked);
        
        
        std::string search_query = text_parse.get("itemfind_search");
        
        
        std::string search_type = player_search_types_[host];
        
        
        if (button_clicked == "search_items") {
            spdlog::info("User selected: Search Items Only");
            show_item_finder_dialog(evt, "", "items");  
        }
        else if (button_clicked == "search_all") {
            spdlog::info("User selected: Search Items & Seeds");
            show_item_finder_dialog(evt, "", "all");  
        }
        else if (button_clicked == "search_seeds") {
            spdlog::info("User selected: Search Seeds Only");
            show_item_finder_dialog(evt, "", "seeds");  
        }
        
        else if (button_clicked == "itemfinder" || button_clicked == "" || button_clicked == "Search") {
            
            if (search_type.empty()) search_type = "all";
            
            if (!search_query.empty()) {
                spdlog::info("Searching for: '{}'", search_query);
                show_item_finder_dialog(evt, search_query, search_type);
            } else {
                show_item_finder_dialog(evt, "", search_type);
            }
        } 
        else if (button_clicked == "itemfind_close" || button_clicked == "Close") {
            
            spdlog::info("Item finder closed");
        }
        
        else if (button_clicked.find("viewdetail_") == 0) {
            try {
                
                std::string item_id_str = button_clicked.substr(11); 
                int item_id = std::stoi(item_id_str);
                
                const ItemInfo* item = database_->get_item_by_id(item_id);
                if (item) {
                    
                    show_item_details_dialog(evt, item, search_query, search_type);
                    spdlog::info("Showed details GUI for item: {} (ID: {})", item->name, item->id);
                } else {
                    spdlog::warn("Item not found: ID {}", item_id);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error showing item details: {}", e.what());
            }
        }
        
        else if (button_clicked.find("wear_") == 0) {
            try {
                
                std::string item_id_str = button_clicked.substr(5); 
                int item_id = std::stoi(item_id_str);
                
                
                const ItemInfo* item = database_->get_item_by_id(item_id);
                int clothing_type = item ? item->clothing_type : 6; 
                
                
                
                static std::unordered_map<int, int> local_slots_tracker;
                bool is_replacement = (local_slots_tracker.find(clothing_type) != local_slots_tracker.end());
                int old_item = is_replacement ? local_slots_tracker[clothing_type] : 0;
                local_slots_tracker[clothing_type] = item_id;
                
                
                command::ClothesCommand::set_pending_item(item_id, clothing_type);
                
                spdlog::info("Saved visual item ID: {}, clothing_type: {}. Use /clothes to apply.", item_id, clothing_type);
                
                
                auto* mutable_player = const_cast<player::Player*>(&player);
                
                if (is_replacement) {
                    utils::PacketUtils::send_chat_message(mutable_player, 
                        fmt::format("`4Replaced item {} `4with `2{} `4in this slot! Type `5/clothes `4to apply.", old_item, item_id));
                } else {
                    utils::PacketUtils::send_chat_message(mutable_player, 
                        fmt::format("`2Item {} added! Type `5/clothes `2to wear your outfit.", item_id));
                }
                
            } catch (const std::exception& e) {
                spdlog::error("Error saving wear item: {}", e.what());
            }
        }
        else {
            
            spdlog::warn("Unknown button in item finder: '{}'", button_clicked);
            show_item_finder_dialog(evt, search_query, search_type);
        }
    }
    
    void handle_item_detail_return(const core::EventMessage& evt, const TextParse& text_parse) {
        evt.canceled = true;
        
        std::string button_clicked = text_parse.get("buttonClicked");
        std::string return_query = text_parse.get("return_query");
        std::string return_type = text_parse.get("return_type");
        
        if (button_clicked == "back_to_search") {
            
            show_item_finder_dialog(evt, return_query, return_type);
            spdlog::info("Returned to search from item details");
        }
    }
};

} 
