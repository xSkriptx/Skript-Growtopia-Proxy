#pragma once
#include "../extension.hpp"
#include "../../core/core.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../command_handler/autosurg_command.hpp"
#include <string>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>
#include <algorithm>

namespace extension::autosurg {

class AutoSurgExtension final : public IExtension {
    core::Core* core_;
    std::string last_dialog_name_;
    std::string last_tilex_;
    std::string last_tiley_;
    std::string last_dialog_content_;  
    std::chrono::steady_clock::time_point last_tool_click_time_{};
    std::string last_tool_sent_;
    int tool_click_delay_ms_ = 700;

public:
    PROVIDE_EXT_UID(0x4155544F); 

    explicit AutoSurgExtension(core::Core* core) : core_(core) {}
    ~AutoSurgExtension() override = default;

    void init() override {
        spdlog::trace("AutoSurgExtension initializing...");
        
        
        core_->get_event_dispatcher().appendListener(
            core::EventType::Packet,
            [this](const core::EventPacket& evt) {
                if (evt.from != core::EventFrom::FromServer) {
                    return;
                }
                handle_server_packet(evt);
            }
        );
        
        spdlog::trace("AutoSurgExtension initialized");
    }

    void free() override {
        delete this;
    }

private:
    std::string with_click_suffix(const std::string& tool_id) const {
        
        if (tool_id.size() >= 5) {
            return tool_id;
        }
        if (tool_id.empty()) {
            return tool_id;
        }
        return tool_id + tool_id.back();
    }

    std::string base_tool_id(const std::string& tool_click_id) const {
        if (tool_click_id.size() >= 4) {
            return tool_click_id.substr(0, 4);
        }
        return tool_click_id;
    }

    std::string tool_name_from_id(const std::string& id) const {
        if (id == "1258") return "Sponge";
        if (id == "1260") return "Scalpel";
        if (id == "1262") return "Anesthetic";
        if (id == "1264") return "Antiseptic";
        if (id == "1266") return "Antibiotic";
        if (id == "1268") return "Splint";
        if (id == "1270") return "Stitches";
        if (id == "1296") return "Fix it";
        if (id == "4308") return "Pins";
        if (id == "4310") return "Transfusion";
        if (id == "4312") return "Defibrillator";
        if (id == "4314") return "Clamp";
        if (id == "4316") return "Ultrasound";
        if (id == "4318") return "Lab kit";
        return "Unknown tool";
    }

    std::string tool_purpose_from_id(const std::string& id, const std::string& dialog) const {
        if (id == "1262") return "sedating patient";
        if (id == "4312") return "restarting heart";
        if (id == "4310") return "stabilizing pulse";
        if (id == "1266") return "treating high temperature";
        if (id == "4318") return "running lab diagnosis";
        if (id == "4316") return "ultrasound diagnosis";
        if (id == "4314") return "stopping blood loss";
        if (id == "1270") return "closing wounds";
        if (id == "1296") return "repairing incisions";
        if (id == "1258") return "cleaning visibility";
        if (id == "1268") return "fixing broken bones";
        if (id == "4308") return "pinning shattered bones";
        if (id == "1260") {
            if (dialog.find("shattered") != std::string::npos) return "treating shattered bones";
            return "surgical fallback";
        }
        if (id == "1264") return "disinfecting wound";
        return "progressing surgery";
    }

    void send_text_overlay(const std::string& msg) {
        if (!core_) return;

        player::Player* target = nullptr;
        if (auto* server = core_->get_server(); server && server->get_player()) {
            target = server->get_player();
        } else if (core_->get_client() && core_->get_client()->get_player()) {
            target = core_->get_client()->get_player();
        }
        if (!target) return;

        packet::Variant var{};
        var.add("OnTextOverlay");
        var.add(msg);

        std::vector<std::byte> ext_data = var.serialize();
        packet::GameUpdatePacket pkt{};
        pkt.type = packet::PACKET_CALL_FUNCTION;
        pkt.net_id = -1;
        pkt.flags.extended = 1;
        pkt.data_size = static_cast<uint32_t>(ext_data.size());

        ByteStream<std::uint16_t> bs{};
        bs.write(packet::NET_MESSAGE_GAME_PACKET);
        bs.write(pkt);
        bs.write_data(ext_data.data(), ext_data.size());
        target->send_packet(bs.get_data(), 0);
    }

    int get_effective_tool_delay_ms() const {
        
        
        int configured = 0;
        if (core_) {
            configured = core_->get_config().get<int>("autosurg.tool_delay_ms");
        }
        if (configured <= 0) {
            configured = tool_click_delay_ms_;
        }
        return std::clamp(configured, 200, 2000);
    }

    
    std::string get_tool_id_by_name(const std::string& tool_name) {
        if (tool_name.find("Sponge") != std::string::npos) return "1258";
        if (tool_name.find("Scalpel") != std::string::npos) return "1260";
        if (tool_name.find("Anesthetic") != std::string::npos) return "1262";
        if (tool_name.find("Antiseptic") != std::string::npos) return "1264";
        if (tool_name.find("Antibiotic") != std::string::npos) return "1266";
        if (tool_name.find("Splint") != std::string::npos) return "1268";
        if (tool_name.find("Stitches") != std::string::npos) return "1270";
        if (tool_name.find("Fix it") != std::string::npos) return "1296";
        if (tool_name.find("Pins") != std::string::npos) return "4308";
        if (tool_name.find("Transfusion") != std::string::npos) return "4310";
        if (tool_name.find("Defibrillator") != std::string::npos) return "4312";
        if (tool_name.find("Clamp") != std::string::npos) return "4314";
        if (tool_name.find("Ultrasound") != std::string::npos) return "4316";
        if (tool_name.find("Lab kit") != std::string::npos || tool_name.find("Lab Kit") != std::string::npos) return "4318";
        return "";
    }
    
    
    
    
    std::vector<std::string> get_available_tools(const std::string& dialog) {
        std::vector<std::string> tools;
        
        
        std::vector<std::string> possible_tools = {
            "1258", "1260", "1262", "1264", "1266", "1268", "1270", "1296",
            "4308", "4310", "4312", "4314", "4316", "4318"
        };
        
        
        for (const auto& tool_id : possible_tools) {
            std::string search_str = "tool" + tool_id;
            if (dialog.find(search_str) != std::string::npos) {
                tools.push_back(tool_id);
                spdlog::debug("AutoSurg: Found '{}' in dialog", search_str);
            }
        }
        
        return tools;
    }

    std::string determine_tool(const std::string& dialog) {
        
        auto available_tools = get_available_tools(dialog);
        
        spdlog::debug("AutoSurg: Total clickable tools found: {}", available_tools.size());
        for (const auto& t : available_tools) {
            spdlog::debug("  - tool{} (clickable button exists)", t);
        }
        
        
        if (available_tools.size() == 1) {
            spdlog::debug("AutoSurg: Only 1 clickable tool available, using tool{}", available_tools[0]);
            return with_click_suffix(available_tools[0]);
        }
        
        
        if (available_tools.empty()) {
            spdlog::error("AutoSurg: NO CLICKABLE TOOLS FOUND!");
            return "";
        }
        
        
        
        
        if (dialog.find("The patient has not been diagnosed.") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4316") {
                    spdlog::debug("AutoSurg: Patient not diagnosed, using Ultrasound");
                    return "43166";
                }
            }
            spdlog::warn("AutoSurg: Patient not diagnosed but Ultrasound is unavailable");
        }
        
        
        if (dialog.find("`4The patient wakes up!") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1262") {
                    spdlog::debug("AutoSurg: Patient waking up, using Anesthetic");
                    return "12622";
                }
            }
        }
        
        if (dialog.find("`4The patient screams and flails!") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1262") {
                    spdlog::debug("AutoSurg: Patient screaming, using Anesthetic");
                    return "12622";
                }
            }
        }
        
        if (dialog.find("Status: `4Heart stopped") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4312") {
                    spdlog::debug("AutoSurg: Heart stopped, using Defibrillator");
                    return "43122";
                }
            }
        }
        
        if (dialog.find("Status: `6Coming to") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1262") {
                    spdlog::debug("AutoSurg: Patient coming to, using Anesthetic");
                    return "12622";
                }
            }
        }
        
        
        if (dialog.find("hyperactive") != std::string::npos || dialog.find("Hyperactive") != std::string::npos ||
            dialog.find("`9The patient is hyperactive") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1262") {
                    spdlog::debug("AutoSurg: Patient hyperactive, using Anesthetic");
                    return "12622";
                }
            }
        }
        
        
        if (dialog.find("Pulse: `4") != std::string::npos || dialog.find("Pulse: `6") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4310") {
                    spdlog::debug("AutoSurg: Low pulse, using Transfusion");
                    return "43100";
                }
            }
        }
        
        
        if (dialog.find("Temp: `4") != std::string::npos || dialog.find("Temp: `6") != std::string::npos ||
            dialog.find("Temp: `3") != std::string::npos || dialog.find("Temp: `b") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1266") {
                    spdlog::debug("AutoSurg: High temp, using Antibiotic");
                    return "12666";
                }
            }
            
            for (const auto& t : available_tools) {
                if (t == "4318") {
                    spdlog::debug("AutoSurg: High temp, using Lab kit");
                    return "43188";
                }
            }
        }
        
        
        if (dialog.find("Patient is losing blood") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4314") {
                    spdlog::debug("AutoSurg: Bleeding, using Clamp");
                    return "43144";
                }
            }
            for (const auto& t : available_tools) {
                if (t == "1270") {
                    spdlog::debug("AutoSurg: Bleeding, using Stitches");
                    return "12700";
                }
            }
        }
        
        if (dialog.find("Patient is `6losing blood!") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4314") {
                    spdlog::debug("AutoSurg: Losing blood, using Clamp");
                    return "43144";
                }
            }
            for (const auto& t : available_tools) {
                if (t == "1270") {
                    spdlog::debug("AutoSurg: Losing blood, using Stitches");
                    return "12700";
                }
            }
        }
        
        
        if (dialog.find("tool1296") != std::string::npos && dialog.find("tool1270") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1270") {
                    spdlog::debug("AutoSurg: Found Fix it + Stitches, prioritizing Stitches");
                    return "12700";
                }
            }
        }

        
        if (dialog.find("Incisions: `") != std::string::npos && dialog.find("Incisions: `0") == std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1296") {
                    spdlog::debug("AutoSurg: Need to fix incisions, using Fix it");
                    return "12966";
                }
            }
        }
        
        
        if (dialog.find("The patient has not been diagnosed") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4316") {
                    spdlog::debug("AutoSurg: Need diagnosis, using Ultrasound");
                    return "43166";
                }
            }
        }
        
        
        if (dialog.find("Status: `4Awake") != std::string::npos ||
            dialog.find("Status: `3Awake") != std::string::npos ||
            dialog.find("Status: `bAwake") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1262") {
                    spdlog::debug("AutoSurg: Patient awake, using Anesthetic");
                    return "12622";
                }
            }
        }
        
        
        if (dialog.find("`4You can't see") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1258") {
                    spdlog::debug("AutoSurg: Can't see, using Sponge");
                    return "12588";
                }
            }
        }
        
        
        if (dialog.find("shattered") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "4308") {
                    spdlog::debug("AutoSurg: Shattered bones, using Pins");
                    return "43088";
                }
            }
            for (const auto& t : available_tools) {
                if (t == "1260") {
                    spdlog::debug("AutoSurg: Shattered bones, using Scalpel");
                    return "12600";
                }
            }
        }
        
        if (dialog.find("broken") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1268") {
                    spdlog::debug("AutoSurg: Broken bones, using Splint");
                    return "12688";
                }
            }
        }

        if (dialog.find("Patient broke his arm.") != std::string::npos ||
            dialog.find("Patient broke his leg.") != std::string::npos) {
            for (const auto& t : available_tools) {
                if (t == "1270") {
                    spdlog::debug("AutoSurg: Patient broke limb, using Stitches");
                    return "12700";
                }
            }
        }
        
        
        for (const auto& t : available_tools) {
            if (t == "1260") {
                spdlog::debug("AutoSurg: Using Scalpel as fallback");
                return "12600";
            }
        }

        spdlog::debug("AutoSurg: Using first available tool as last resort: tool{}", available_tools[0]);
        return with_click_suffix(available_tools[0]);
    }

    void send_tool_packet(const core::EventPacket& evt, const std::string& tool) {
        if (!core_->get_client() || !core_->get_client()->get_player()) {
            spdlog::error("AutoSurg: No client player");
            return;
        }

        if (tool.empty()) {
            spdlog::error("AutoSurg: Tool ID is empty!");
            return;
        }

        
        evt.canceled = true;

        auto now = std::chrono::steady_clock::now();
        const int effective_delay_ms = get_effective_tool_delay_ms();
        if (last_tool_click_time_.time_since_epoch().count() != 0) {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_tool_click_time_
            ).count();

            if (elapsed_ms < effective_delay_ms) {
                const auto wait_ms = effective_delay_ms - static_cast<int>(elapsed_ms);
                spdlog::debug("AutoSurg: Throttling tool clicks, waiting {}ms", wait_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            }
        }

        spdlog::debug("AutoSurg: Sending click for tool{}", tool);
        
        
        std::ostringstream packet_str;
        packet_str << "action|dialog_return\n"
                   << "dialog_name|surgery\n"
                   << "buttonClicked|tool" << tool;
        
        std::string packet_data = packet_str.str();
        spdlog::debug("AutoSurg: Tool packet data:\n{}", packet_data);
        
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(packet_data, false);
        
        core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
        last_tool_click_time_ = std::chrono::steady_clock::now();
        last_tool_sent_ = tool;

        const std::string id = base_tool_id(tool);
        const std::string tool_name = tool_name_from_id(id);
        const std::string purpose = tool_purpose_from_id(id, last_dialog_content_);
        send_text_overlay(fmt::format("Using {} - {}", tool_name, purpose));
        
        spdlog::debug("AutoSurg: Tool click sent and dialog canceled!");
    }

    void handle_server_packet(const core::EventPacket& evt) {
        const auto& game_packet = evt.get_packet();

        
        if (game_packet.type != packet::PACKET_CALL_FUNCTION) {
            return;
        }
        
        if (!game_packet.flags.extended) {
            return;
        }
        
        
        const auto& ext_data = evt.get_ext_data();
        if (ext_data.empty()) {
            return;
        }

        try {
            ByteStream<std::uint16_t> reader(const_cast<std::byte*>(ext_data.data()), ext_data.size());
            
            
            uint8_t param_count;
            if (!reader.read(param_count) || param_count < 2) {
                return;
            }
            
            
            uint8_t index1;
            if (!reader.read(index1)) {
                return;
            }
            
            uint8_t type1;
            if (!reader.read(type1)) {
                return;
            }
            
            if (type1 != 2) {  
                return;
            }
            
            uint32_t str_len;
            if (!reader.read(str_len)) {
                return;
            }
            
            std::string func_name(str_len, '\0');
            if (!reader.read_data((std::byte*)func_name.data(), str_len)) {
                return;
            }
            
            
            if (func_name != "OnDialogRequest") {
                return;
            }
            
            spdlog::debug("AutoSurg: Detected OnDialogRequest variant!");
            
            
            uint8_t index2;
            if (!reader.read(index2)) {
                return;
            }
            
            uint8_t type2;
            if (!reader.read(type2)) {
                return;
            }
            
            if (type2 != 2) {  
                return;
            }
            
            uint32_t dialog_len;
            if (!reader.read(dialog_len)) {
                return;
            }
            
            std::string dialog(dialog_len, '\0');
            if (!reader.read_data((std::byte*)dialog.data(), dialog_len)) {
                return;
            }
            
            last_dialog_content_ = dialog;
            
            spdlog::debug("AutoSurg: Dialog length: {}, first 100 chars: {}", 
                        dialog_len, dialog.substr(0, 100));
            
            
            size_t tilex_pos = dialog.find("embed_data|tilex|");
            if (tilex_pos != std::string::npos) {
                tilex_pos += 17;  
                size_t tilex_end = dialog.find("\n", tilex_pos);
                if (tilex_end == std::string::npos) {
                    tilex_end = dialog.find("|", tilex_pos);
                }
                last_tilex_ = dialog.substr(tilex_pos, tilex_end - tilex_pos);
            }
            
            size_t tiley_pos = dialog.find("embed_data|tiley|");
            if (tiley_pos != std::string::npos) {
                tiley_pos += 17;  
                size_t tiley_end = dialog.find("\n", tiley_pos);
                if (tiley_end == std::string::npos) {
                    tiley_end = dialog.find("|", tiley_pos);
                }
                last_tiley_ = dialog.substr(tiley_pos, tiley_end - tiley_pos);
            }
            
            
            if (dialog.find("end_dialog|surge|") != std::string::npos) {
                
                if (!command::AutoSurgCommand::is_enabled()) {
                    spdlog::debug("AutoSurg: Confirmation dialog detected but AutoSurg is DISABLED, opening normally");
                    return;
                }
                
                spdlog::debug("AutoSurg: DETECTED CONFIRMATION DIALOG (surge)!");
                
                
                std::ostringstream packet_str;
                packet_str << "action|dialog_return\n"
                           << "dialog_name|surge\n";
                
                if (!last_tilex_.empty()) {
                    packet_str << "tilex|" << last_tilex_ << "\n";
                }
                if (!last_tiley_.empty()) {
                    packet_str << "tiley|" << last_tiley_ << "\n";
                }
                
                packet_str << "buttonClicked|Okay!";
                
                std::string packet_data = packet_str.str();
                spdlog::debug("AutoSurg: Confirming surgery with packet:\n{}", packet_data);
                
                ByteStream<std::uint16_t> byte_stream{};
                byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
                byte_stream.write(packet_data, false);
                
                core_->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
                
                spdlog::debug("AutoSurg: Sent surgery confirmation!");
                evt.canceled = true;
                return;
            }
            
            
            if (dialog.find("end_dialog|surgery") != std::string::npos) {
                
                if (!command::AutoSurgCommand::is_enabled()) {
                    spdlog::debug("AutoSurg: Surgery dialog detected but AutoSurg is DISABLED, opening normally");
                    return;
                }
                
                spdlog::debug("AutoSurg: DETECTED ACTUAL SURGERY DIALOG (surgery)!");
                
                
                last_dialog_name_ = "surgery";
                
                
                std::string tool_to_use = determine_tool(dialog);
                
                if (tool_to_use.empty()) {
                    spdlog::warn("AutoSurg: No suitable tool found, opening dialog normally");
                    return;
                }
                
                
                spdlog::debug("AutoSurg: Selected tool: {}, sending click packet", tool_to_use);
                send_tool_packet(evt, tool_to_use);
                return;
            }
            
            
            spdlog::debug("AutoSurg: Not a surgery dialog (neither surge nor surgery)");
            
        } catch (const std::exception& e) {
            spdlog::error("AutoSurg: Exception in handle_server_packet: {}", e.what());
        }
    }
};

} 
