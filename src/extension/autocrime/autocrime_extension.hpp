#pragma once
#include "../extension.hpp"
#include "../../core/core.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../command_handler/autocrime_command.hpp"
#include <string>
#include <vector>
#include <set>
#include <chrono>
#include <thread>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace extension::autocrime {

class AutoCrimeExtension final : public IExtension {
    core::Core* core_;
    std::string tilex_;
    std::string tiley_;

    std::string villain_;
    std::string liq_ = "no";
    std::string ban_ = "no";
    int step_ = 0;

    int c1_ = 2298;
    int c2_ = 2308;
    int c3_ = 2320;
    int c4_ = 2324;
    int c5_ = 2332;

    int use1_ = 0;
    int use2_ = 0;
    int use3_ = 0;
    int use4_ = 0;
    int use5_ = 0;
    std::vector<int> available_cards_;
    std::chrono::steady_clock::time_point last_action_time_{};
    int action_delay_ms_ = 650;

    std::string last_setup_key_;
    std::chrono::steady_clock::time_point last_setup_time_{};
    int setup_attempt_ = 0;

public:
    PROVIDE_EXT_UID(0x41555443); 

    explicit AutoCrimeExtension(core::Core* core) : core_(core) {}
    ~AutoCrimeExtension() override = default;

    void init() override {
        core_->get_event_dispatcher().appendListener(
            core::EventType::Packet,
            [this](const core::EventPacket& evt) {
                if (evt.from != core::EventFrom::FromServer) {
                    return;
                }
                handle_server_packet(evt);
            }
        );
    }

    void free() override {
        delete this;
    }

private:
    static bool has(const std::string& s, const std::string& needle) {
        return s.find(needle) != std::string::npos;
    }

    static bool has_card(const std::string& dialog, int id) {
        return has(dialog, "c" + std::to_string(id));
    }

    static std::vector<int> extract_available_cards(const std::string& dialog) {
        std::vector<int> out;
        const std::string needle = "add_checkicon|c";
        size_t pos = 0;
        while ((pos = dialog.find(needle, pos)) != std::string::npos) {
            size_t start = pos + needle.size();
            size_t end = start;
            while (end < dialog.size() && std::isdigit(static_cast<unsigned char>(dialog[end]))) {
                ++end;
            }
            if (end > start) {
                out.push_back(std::stoi(dialog.substr(start, end - start)));
            }
            pos = end;
        }
        return out;
    }

    static int extract_first_clickable_battle_card(const std::string& dialog) {
        const std::string needle = "add_button_with_icon|c";
        size_t pos = dialog.find(needle);
        if (pos == std::string::npos) return 0;
        size_t start = pos + needle.size();
        size_t end = start;
        while (end < dialog.size() && std::isdigit(static_cast<unsigned char>(dialog[end]))) {
            ++end;
        }
        if (end == start) return 0;
        return std::stoi(dialog.substr(start, end - start));
    }

    static std::string dup_last_digit(const std::string& id) {
        if (id.empty()) return id;
        return id + id.back();
    }

    int get_effective_action_delay_ms() const {
        int configured = 0;
        if (core_) {
            configured = core_->get_config().get<int>("autocrime.action_delay_ms");
        }
        if (configured <= 0) {
            configured = action_delay_ms_;
        }
        return std::clamp(configured, 200, 2500);
    }

    void throttle_action() {
        auto now = std::chrono::steady_clock::now();
        const int delay_ms = get_effective_action_delay_ms();
        if (last_action_time_.time_since_epoch().count() != 0) {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_action_time_
            ).count();
            if (elapsed_ms < delay_ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms - static_cast<int>(elapsed_ms)));
            }
        }
        last_action_time_ = std::chrono::steady_clock::now();
    }

    static std::string extract_embed(const std::string& dialog, const std::string& key) {
        const std::string marker = "embed_data|" + key + "|";
        size_t pos = dialog.find(marker);
        size_t start = 0;
        if (pos != std::string::npos) {
            start = pos + marker.size();
        } else {
            const std::string marker2 = key + "|";
            pos = dialog.find(marker2);
            if (pos == std::string::npos) return "";
            start = pos + marker2.size();
        }
        size_t end = dialog.find('\n', start);
        if (end == std::string::npos) end = dialog.find('|', start);
        if (end == std::string::npos) end = dialog.size();
        return dialog.substr(start, end - start);
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

    void send_crime_dialog_return(const std::string& payload) {
        if (!core_ || !core_->get_client() || !core_->get_client()->get_player()) {
            return;
        }
        throttle_action();
        ByteStream<std::uint16_t> bs{};
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(payload, false);
        core_->get_client()->get_player()->send_packet(bs.get_data(), 0);
    }

    void send_open_battle(bool use_title_keys, bool use_battle_button) {
        const std::string pos_x_key = use_title_keys ? "titlex|" : "tilex|";
        const std::string pos_y_key = use_title_keys ? "titley|" : "tiley|";
        const std::string battle_button = use_battle_button ? "Battle!" : "button_ok";
        std::set<int> selected{c1_, c2_, c3_, c4_, c5_};

        const std::string payload = std::string(
            "action|dialog_return\n"
            "dialog_name|crime_edit\n"
            ) + pos_x_key + tilex_ + "|\n"
            + pos_y_key + tiley_ + "|\n"
            "state|0||\n"
            "buttonClicked|" + battle_button + "\n";

        std::string with_cards = payload;
        if (!available_cards_.empty()) {
            for (int id : available_cards_) {
                with_cards += "c" + std::to_string(id) + "|" + (selected.count(id) ? "1" : "0") + "\n";
            }
        } else {
            
            with_cards += "c" + std::to_string(c1_) + "|1\n";
            with_cards += "c" + std::to_string(c2_) + "|1\n";
            with_cards += "c" + std::to_string(c3_) + "|1\n";
            with_cards += "c" + std::to_string(c4_) + "|1\n";
            with_cards += "c" + std::to_string(c5_) + "|1\n";
        }
        send_crime_dialog_return(with_cards);
    }

    void send_play_card_or_pass(const std::string& card_or_passturn) {
        send_text_overlay("`9Please wait.");
        std::string click = "passturn";
        if (card_or_passturn != "passturn") {
            
            click = "c" + dup_last_digit(card_or_passturn);
        }
        const std::string payload = std::string(
            "action|dialog_return\n"
            "dialog_name|crime_edit\n"
            "tilex|") + tilex_ + "|\n"
            "tiley|" + tiley_ + "|\n"
            "state|1||\n"
            "buttonClicked|" + click;
        send_crime_dialog_return(payload);
    }

    void configure_for_villain(const std::string& dialog) {
        
        if (has(dialog, "Crime in Progress")) {
            c1_ = 2298; c2_ = 2308; c3_ = 2320; c4_ = 2324; c5_ = 2332;
        }

        if (has(dialog, "Jimmy Snow")) {
            villain_ = "Jimmy Snow";
            use1_ = c5_; use2_ = c4_; use3_ = c3_;
        } else if (has(dialog, "Big Bertha")) {
            villain_ = "Big Bertha";
            use1_ = c1_; use2_ = c4_; use3_ = c3_;
        } else if (has(dialog, "Shockinator")) {
            villain_ = "Shockinator";
            use1_ = c4_; use2_ = c3_; use3_ = c1_;
        } else if (has(dialog, "The Firebug")) {
            villain_ = "The Firebug";
            use1_ = c2_; use2_ = c1_; use3_ = c5_;
        } else if (has(dialog, "Generic Thug") || has(dialog, "Kat 5")) {
            villain_ = "Generic Thug";
            use1_ = c4_; use2_ = c3_; use3_ = c2_;
        } else if (has(dialog, "Professor Pummel")) {
            villain_ = "Professor Pummel";
            c1_ = 2296; c2_ = 2298; c3_ = 2300; c4_ = 2320; c5_ = 2324;
            use1_ = c3_; use2_ = c2_; use3_ = c1_;
        } else if (has(dialog, "Z. Everett Koop")) {
            villain_ = "Z. Everett Koop";
            c1_ = 2296; c2_ = 2298; c3_ = 2300; c4_ = 2320; c5_ = 2324;
            use1_ = c3_; use2_ = c1_; use3_ = c2_;
        } else if (has(dialog, "Dr. Destructo")) {
            villain_ = "Dr. Destructo";
            c1_ = 2298; c2_ = 2308; c3_ = 2310; c4_ = 2314; c5_ = 2316;
            step_ = 0;
            use1_ = c1_; use2_ = c3_; use3_ = c5_;
        } else if (has(dialog, "Dragon Hand") || has(dialog, "Almighty Seth")) {
            villain_ = "Dragon Hand";
            c1_ = 2308; c2_ = 2312; c3_ = 2314; c4_ = 2326; c5_ = 2328;
            use1_ = c5_; use2_ = c4_; use3_ = 6969;
        } else if (has(dialog, "Devil Ham")) {
            villain_ = "Devil Ham";
            c1_ = 2298; c2_ = 2332; c3_ = 2334; c4_ = 2336; c5_ = 2338;
            use1_ = c1_; use2_ = c4_; use3_ = c5_; use4_ = c3_; use5_ = c2_;
            liq_ = "no";
            step_ = 1;
        } else if (has(dialog, "Ms. Terry")) {
            villain_ = "Ms. Terry";
            c1_ = 2294; c2_ = 2316; c3_ = 2322; c4_ = 2328; c5_ = 2338;
            use1_ = c1_; use2_ = c2_; use3_ = c3_; use4_ = c4_; use5_ = c5_;
            ban_ = "no";
        }
    }

    void handle_battle_dialog(const core::EventPacket& evt, const std::string& dialog) {
        if (has(dialog, "Devil Ham`%'s `2Crush` card `4melted")) {
            liq_ = "yes";
        }
        if (has(dialog, "Ms. Terry") && has(dialog, "Ban Hammer")) {
            ban_ = "yes";
        }

        tilex_ = extract_embed(dialog, "tilex");
        tiley_ = extract_embed(dialog, "tiley");
        available_cards_ = extract_available_cards(dialog);
        configure_for_villain(dialog);

        
        const std::string setup_key = tilex_ + ":" + tiley_ + ":" + villain_;
        const auto now = std::chrono::steady_clock::now();
        if (setup_key == last_setup_key_ &&
            last_setup_time_.time_since_epoch().count() != 0 &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_setup_time_).count() < 2500) {
            setup_attempt_++;
        } else {
            setup_attempt_ = 1;
        }
        last_setup_key_ = setup_key;
        last_setup_time_ = now;

        const bool use_title_keys = (setup_attempt_ >= 2);
        const bool use_battle_button = (setup_attempt_ >= 3);
        send_open_battle(use_title_keys, use_battle_button);
        evt.canceled = true;
    }

    std::string choose_for_devil_ham(const std::string& dialog) {
        if (liq_ == "no") {
            return has_card(dialog, use1_) ? std::to_string(use1_) : "passturn";
        }
        if (has_card(dialog, use2_) && step_ == 0) {
            step_ = 1;
            return std::to_string(use2_);
        }
        if (has_card(dialog, use3_)) return std::to_string(use3_);
        if (has_card(dialog, use4_)) return std::to_string(use4_);
        if (has_card(dialog, use5_)) return std::to_string(use5_);
        return "passturn";
    }

    std::string choose_for_ms_terry(const std::string& dialog) {
        auto play_pos = dialog.find("is going to play ");
        if (play_pos != std::string::npos && play_pos + 17 < dialog.size()) {
            const char d = dialog[play_pos + 17];
            if (d == '2') return std::to_string(use1_);
            if (d == '4') return std::to_string(use2_);
            if (d == '9') return std::to_string(use4_);
            if (d == '1') return std::to_string(use5_);
        }
        if (has_card(dialog, use3_)) return std::to_string(use3_);
        if (has_card(dialog, use5_) && ban_ == "yes") return std::to_string(use5_);
        if (has_card(dialog, use1_)) return std::to_string(use1_);
        if (has_card(dialog, use4_)) return std::to_string(use4_);
        if (has_card(dialog, use5_)) return std::to_string(use5_);
        if (has_card(dialog, use2_)) return std::to_string(use2_);
        return "passturn";
    }

    std::string choose_for_dr_destructo(const std::string& dialog) {
        if (has_card(dialog, use1_) && step_ == 0) {
            step_ = 1;
            return std::to_string(use1_);
        }
        if (has_card(dialog, use2_)) return std::to_string(use2_);
        if (has_card(dialog, use3_)) return std::to_string(use3_);
        return "passturn";
    }

    std::string choose_generic(const std::string& dialog) {
        if (use1_ > 0 && has_card(dialog, use1_)) return std::to_string(use1_);
        
        const int first = extract_first_clickable_battle_card(dialog);
        if (first > 0) return std::to_string(first);
        return "passturn";
    }

    void handle_fighting_dialog(const core::EventPacket& evt, const std::string& dialog) {
        
        if (has(dialog, "embed_data|state|3|") || has(dialog, "You are defeated")) {
            return;
        }

        std::string chosen = "passturn";
        if (has(dialog, "Devil Ham")) {
            chosen = choose_for_devil_ham(dialog);
        } else if (has(dialog, "Ms. Terry")) {
            chosen = choose_for_ms_terry(dialog);
        } else if (has(dialog, "Dr. Destructo")) {
            chosen = choose_for_dr_destructo(dialog);
        } else {
            chosen = choose_generic(dialog);
        }

        send_play_card_or_pass(chosen);
        evt.canceled = true;
    }

    void handle_server_packet(const core::EventPacket& evt) {
        if (!command::AutoCrimeCommand::is_enabled()) {
            return;
        }

        const auto& game_packet = evt.get_packet();
        if (game_packet.type != packet::PACKET_CALL_FUNCTION || !game_packet.flags.extended) {
            return;
        }

        const auto& ext_data = evt.get_ext_data();
        if (ext_data.empty()) {
            return;
        }

        packet::Variant variant{};
        if (!variant.deserialize(ext_data) || variant.size() < 2) {
            return;
        }

        const std::string fn = variant.get<std::string>(0);
        if (fn != "OnDialogRequest") {
            return;
        }

        const std::string dialog = variant.get<std::string>(1);
        if (has(dialog, "|Battle!|") || has(dialog, "Crime in Progress")) {
            handle_battle_dialog(evt, dialog);
            return;
        }
        if (has(dialog, "Fighting Crime")) {
            handle_fighting_dialog(evt, dialog);
            return;
        }
    }
};

} 
