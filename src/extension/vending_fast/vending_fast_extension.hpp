#pragma once
#include "../extension.hpp"
#include "../../core/core.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../command_handler/vendfast_command.hpp"
#include <string>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <chrono>

namespace extension::vending_fast {

class VendingFastExtension final : public IExtension {
    core::Core* core_;
    static bool s_confirming_buy;

public:
    PROVIDE_EXT_UID(0x56454E44); 

    explicit VendingFastExtension(core::Core* core) : core_(core) {}
    ~VendingFastExtension() override = default;

    void init() override {
        spdlog::trace("VendingFastExtension initializing...");
        
        
        core_->get_event_dispatcher().appendListener(
            core::EventType::Packet,
            [this](const core::EventPacket& evt) {
                if (evt.from != core::EventFrom::FromServer) {
                    return;
                }
                handle_server_packet(evt);
            }
        );
        
        
        core_->get_event_dispatcher().appendListener(
            core::EventType::Message,
            [this](const core::EventMessage& evt) {
                if (evt.from != core::EventFrom::FromClient) {
                    return;
                }
                handle_client_message(evt);
            }
        );
        
        spdlog::trace("VendingFastExtension initialized");
    }

    void free() override {
        delete this;
    }

private:
    void handle_server_packet(const core::EventPacket& evt) {
        const auto& game_packet = evt.get_packet();
        
        
        if (game_packet.type == packet::PACKET_CALL_FUNCTION && game_packet.flags.extended) {
            spdlog::info("Received PACKET_CALL_FUNCTION");
        }
        
        
        if (game_packet.type != packet::PACKET_CALL_FUNCTION) {
            return;
        }
        
        if (!game_packet.flags.extended) {
            return;
        }
        
        
        const auto& ext_data = evt.get_ext_data();
        if (ext_data.empty()) {
            spdlog::warn("ext_data is empty!");
            return;
        }
        
        spdlog::info("Parsing variant, ext_data size: {}", ext_data.size());
        
        try {
            ByteStream<std::uint16_t> reader(const_cast<std::byte*>(ext_data.data()), ext_data.size());
            
            
            uint8_t param_count;
            if (!reader.read(param_count)) {
                spdlog::error("Failed to read param_count");
                return;
            }
            spdlog::info("param_count = {}", param_count);
            
            if (param_count < 2) {
                spdlog::info("Not enough parameters");
                return;
            }
            
            
            uint8_t index1;
            if (!reader.read(index1)) {
                spdlog::error("Failed to read index1");
                return;
            }
            
            uint8_t type1;
            if (!reader.read(type1)) {
                spdlog::error("Failed to read type1");
                return;
            }
            spdlog::info("type1 = {}", type1);
            
            
            if (type1 != 2) {
                spdlog::info("type1 is not string (expected 2, got {})", type1);
                return;
            }
            
            uint32_t str_len;
            if (!reader.read(str_len)) {
                spdlog::error("Failed to read str_len");
                return;
            }
            spdlog::info("str_len = {}", str_len);
            
            std::string function_name(str_len, '\0');
            if (!reader.read_data((std::byte*)function_name.data(), str_len)) {
                spdlog::error("Failed to read function_name");
                return;
            }
            
            spdlog::info("Variant function: {}", function_name);
            
            
            if (function_name != "OnDialogRequest") {
                return;
            }
            
            spdlog::info("Detected OnDialogRequest variant!");
            
            
            uint8_t index2;
            if (!reader.read(index2)) {
                spdlog::error("Failed to read index2");
                return;
            }
            
            uint8_t type2;
            if (!reader.read(type2)) {
                spdlog::error("Failed to read type2");
                return;
            }
            
            if (type2 != 2) {
                spdlog::info("type2 is not string (expected 2, got {})", type2);
                return;
            }
            
            uint32_t dialog_len;
            if (!reader.read(dialog_len)) {
                spdlog::error("Failed to read dialog_len");
                return;
            }
            spdlog::info("dialog_len = {}", dialog_len);
            
            std::string dialog(dialog_len, '\0');
            if (!reader.read_data((std::byte*)dialog.data(), dialog_len)) {
                spdlog::error("Failed to read dialog data");
                return;
            }
            
            spdlog::info("Successfully parsed dialog!");
            
            
            handle_vending_dialogs(evt, dialog);
            
        } catch (const std::exception& e) {
            spdlog::error("Exception during parsing: {}", e.what());
            return;
        }
    }
    
    void handle_vending_dialogs(const core::EventPacket& evt, const std::string& dialog) {
        spdlog::info("Detected OnDialogRequest");
        spdlog::info("Dialog content (first 200 chars): {}", dialog.substr(0, std::min((size_t)200, dialog.size())));
        
        
        spdlog::info("VendFast: Empty mode: {}", command::VendFastCommand::is_empty_mode_enabled());
        spdlog::info("VendFast: Add mode: {}", command::VendFastCommand::is_add_mode_enabled());
        spdlog::info("VendFast: Buy mode: {}", command::VendFastCommand::is_buy_mode_enabled());
        
        
        if (dialog.find("end_dialog|vending|Close|Update|") != std::string::npos) {
            spdlog::info("VendFast: Detected vending Update dialog");
            
            
            if (command::VendFastCommand::is_empty_mode_enabled()) {
                spdlog::info("VendFast: Empty mode is enabled, handling...");
                handle_empty_mode(evt, dialog);
                return;
            }
            
            
            if (command::VendFastCommand::is_add_mode_enabled()) {
                spdlog::info("VendFast: Add mode is enabled, handling...");
                handle_add_mode(evt, dialog);
                return;
            }
        }
        
        
        if (dialog.find("end_dialog|vending|Close|Buy|") != std::string::npos) {
            spdlog::info("VendFast: Detected vending Buy dialog");
            
            if (command::VendFastCommand::is_buy_mode_enabled()) {
                spdlog::info("VendFast: Buy mode is enabled, handling...");
                handle_buy_mode_step1(evt, dialog);
                return;
            }
        }
        
        
        if (dialog.find("end_dialog|vending|Cancel|OK|") != std::string::npos) {
            spdlog::info("VendFast: Detected vending confirmation dialog");
            
            if (command::VendFastCommand::is_buy_mode_enabled()) {
                spdlog::info("VendFast: Buy mode is enabled, confirming...");
                handle_buy_mode_step2(evt, dialog);
                return;
            }
        }
        
        spdlog::info("VendFast: No matching mode or dialog pattern");
    }
    
    void handle_empty_mode(const core::EventPacket& evt, const std::string& dialog) {
        
        std::string tilex = extract_value(dialog, "embed_data|tilex|");
        std::string tiley = extract_value(dialog, "embed_data|tiley|");
        
        if (tilex.empty() || tiley.empty()) {
            spdlog::warn("VendFast: Could not extract coordinates");
            return;
        }
        
        spdlog::info("VendFast: Auto-emptying vending at ({}, {})", tilex, tiley);
        
        
        std::ostringstream packet_str;
        packet_str << "action|dialog_return\n"
                   << "dialog_name|vending\n"
                   << "tilex|" << tilex << "|\n"
                   << "tiley|" << tiley << "|\n"
                   << "buttonClicked|pullstock ";  
        
        std::string packet_data = packet_str.str();
        spdlog::info("VendFast: Sending empty packet (exact buy format):\n{}", packet_data);
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(packet_data, false);
        
        if (core_->get_client() && core_->get_client()->get_player()) {
            core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
            spdlog::info("VendFast: Sent empty packet");
        }
        
        evt.canceled = true;
    }
    
    void handle_add_mode(const core::EventPacket& evt, const std::string& dialog) {
        
        std::string tilex = extract_value(dialog, "embed_data|tilex|");
        std::string tiley = extract_value(dialog, "embed_data|tiley|");
        
        if (tilex.empty() || tiley.empty()) {
            spdlog::warn("VendFast: Could not extract coordinates");
            return;
        }
        
        spdlog::info("VendFast: Auto-adding stock at ({}, {})", tilex, tiley);
        
        
        std::ostringstream packet_str;
        packet_str << "action|dialog_return\n"
                   << "dialog_name|vending\n"
                   << "tilex|" << tilex << "|\n"
                   << "tiley|" << tiley << "|\n"
                   << "buttonClicked|addstock ";
        
        std::string packet_data = packet_str.str();
        spdlog::info("VendFast: Sending add stock packet:\n{}", packet_data);
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(packet_data, false);
        
        if (core_->get_client() && core_->get_client()->get_player()) {
            core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
            spdlog::info("VendFast: Sent add stock packet");
        }
        
        
        evt.canceled = true;
    }
    
    void handle_buy_mode_step1(const core::EventPacket& evt, const std::string& dialog) {
        
        std::string tilex = extract_value(dialog, "embed_data|tilex|");
        std::string tiley = extract_value(dialog, "embed_data|tiley|");
        std::string expectitem = extract_value(dialog, "embed_data|expectitem|");
        std::string expectprice_str = extract_price(dialog);
        
        if (tilex.empty() || tiley.empty() || expectitem.empty() || expectprice_str.empty()) {
            spdlog::warn("VendFast: Could not extract buy parameters");
            return;
        }
        
        
        int buy_count = 0;
        size_t total_pos = dialog.find("contains a total of ");
        if (total_pos != std::string::npos) {
            size_t num_start = total_pos + 20;
            size_t num_end = num_start;
            while (num_end < dialog.size() && isdigit(dialog[num_end])) {
                num_end++;
            }
            if (num_end > num_start) {
                std::string stock_str = dialog.substr(num_start, num_end - num_start);
                int total_stock = std::stoi(stock_str);
                
                
                buy_count = total_stock * 10;
                
                spdlog::info("VendFast: Detected stock: {}, buying with zero trick: {}", stock_str, buy_count);
            }
        }
        
        if (buy_count <= 0) {
            spdlog::warn("VendFast: Invalid buy count: {}", buy_count);
            return;
        }
        
        spdlog::info("VendFast: Buying {} items from vending", buy_count);
        evt.canceled = true;
        
        
        std::ostringstream packet_str;
        packet_str << "action|dialog_return\n"
                   << "dialog_name|vending\n"
                   << "tilex|" << tilex << "|\n"
                   << "tiley|" << tiley << "|\n"
                   << "expectprice|" << expectprice_str << "|\n"
                   << "expectitem|" << expectitem << "|\n"
                   << "buycount|" << buy_count;  
        
        std::string packet_data = packet_str.str();
        spdlog::info("VendFast: Sending buy request ({} chars):\n{}", packet_data.length(), packet_data);
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(packet_data, false);
        
        if (core_->get_client() && core_->get_client()->get_player()) {
            core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
            spdlog::info("VendFast: Buy request packet sent");
        }
    }
    
    void handle_buy_mode_step2(const core::EventPacket& evt, const std::string& dialog) {
        
        if (s_confirming_buy) {
            spdlog::info("VendFast: Already confirming, ignoring duplicate");
            evt.canceled = true;
            return;
        }
        
        s_confirming_buy = true;
        
        spdlog::info("VendFast: Buy confirmation dialog detected");
        
        
        std::string tilex = extract_value(dialog, "embed_data|tilex|");
        std::string tiley = extract_value(dialog, "embed_data|tiley|");
        std::string expectitem = extract_value(dialog, "embed_data|expectitem|");
        std::string buycount = extract_value(dialog, "embed_data|buycount|");
        
        
        std::string expectprice;
        size_t price_pos = dialog.find("expectprice|");
        if (price_pos != std::string::npos) {
            size_t start = price_pos + 12;
            size_t end = start;
            while (end < dialog.size() && (isdigit(dialog[end]) || dialog[end] == '-')) {
                end++;
            }
            expectprice = dialog.substr(start, end - start);
        }
        
        spdlog::info("VendFast: Extracted values - tilex: {}, tiley: {}, expectitem: {}, buycount: {}, expectprice: {}",
                    tilex, tiley, expectitem, buycount, expectprice);
        
        if (tilex.empty() || tiley.empty() || expectitem.empty() || buycount.empty() || expectprice.empty()) {
            spdlog::warn("VendFast: Could not extract confirmation parameters");
            s_confirming_buy = false;
            return;
        }
        
        
        int server_buycount = std::stoi(buycount);
        int original_buycount = server_buycount;
        
        
        
        
        
        
        spdlog::info("VendFast: Server returned buycount: {}, confirming with this value", buycount);
        
        spdlog::info("VendFast: Confirming buy of {} items at ({}, {})", buycount, tilex, tiley);
        evt.canceled = true;
        
        
        std::ostringstream packet_str;
        packet_str << "action|dialog_return\n"
                   << "dialog_name|vending\n"
                   << "tilex|" << tilex << "|\n"
                   << "tiley|" << tiley << "|\n"
                   << "verify|1|\n"
                   << "buycount|" << buycount << "|\n"
                   << "expectprice|" << expectprice << "|\n"
                   << "expectitem|" << expectitem << "|";
        
        std::string packet_data = packet_str.str();
        spdlog::info("VendFast: Sending confirmation:\n{}", packet_data);
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(packet_data, false);
        
        if (core_->get_client() && core_->get_client()->get_player()) {
            core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
            spdlog::info("VendFast: Sent confirmation with buycount {}", buycount);
        }
        
        
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            s_confirming_buy = false;
        }).detach();
    }
    
    std::string extract_value(const std::string& dialog, const std::string& key) {
        size_t pos = dialog.find(key);
        if (pos == std::string::npos) return "";
        
        size_t start = pos + key.length();
        
        
        size_t end1 = dialog.find("|", start);
        size_t end2 = dialog.find("\n", start);
        
        size_t end = std::string::npos;
        if (end1 != std::string::npos && end2 != std::string::npos) {
            end = std::min(end1, end2);
        } else if (end1 != std::string::npos) {
            end = end1;
        } else if (end2 != std::string::npos) {
            end = end2;
        }
        
        if (end == std::string::npos) return "";
        
        std::string result = dialog.substr(start, end - start);
        spdlog::info("VendFast: Extracted '{}' = '{}'", key, result);
        return result;
    }
    
    std::string extract_price(const std::string& dialog) {
        size_t pos = dialog.find("expectprice|");
        if (pos == std::string::npos) return "";
        
        size_t start = pos + 12;
        size_t end = start;
        while (end < dialog.size() && (isdigit(dialog[end]) || dialog[end] == '-')) {
            end++;
        }
        
        std::string price = dialog.substr(start, end - start);
        spdlog::info("VendFast: Extracted price = '{}'", price);
        return price;
    }
    
    void handle_client_message(const core::EventMessage& evt) {
        const auto& text_parse = evt.get_message();
        std::string action = text_parse.get("action");
        
        if (action != "dialog_return") {
            return;
        }
        
        std::string dialog_name = text_parse.get("dialog_name");
        if (dialog_name != "vendfast") {
            return;
        }
        
        std::string button = text_parse.get("buttonClicked");
        
        if (button == "toggle_empty") {
            command::VendFastCommand::toggle_empty_mode();
            evt.canceled = true;
            
            if (core_->get_client()) {
                command::VendFastCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
        else if (button == "toggle_add") {
            command::VendFastCommand::toggle_add_mode();
            evt.canceled = true;
            
            if (core_->get_client()) {
                command::VendFastCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
        else if (button == "toggle_buy") {
            command::VendFastCommand::toggle_buy_mode();
            evt.canceled = true;
            
            if (core_->get_client()) {
                command::VendFastCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
    }
};


bool VendingFastExtension::s_confirming_buy = false;

} 
