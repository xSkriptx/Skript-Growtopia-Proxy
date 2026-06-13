#pragma once
#include "../extension.hpp"
#include "../../core/core.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/inventory_manager.hpp"
#include "../../packet/packet_variant.hpp"
#include "../command_handler/vendsafe_command.hpp"
#include "../command_handler/vendfast_command.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>

namespace extension::vendsafe {

class VendSafeExtension final : public IExtension {
    core::Core* core_;
    bool waiting_confirm_ = false;
    std::string last_tilex_;
    std::string last_tiley_;
    std::string last_expectitem_;
    std::string last_expectprice_;
    int last_buycount_ = 1;

public:
    PROVIDE_EXT_UID(0x56534146); 
    explicit VendSafeExtension(core::Core* core) : core_(core) {}
    ~VendSafeExtension() override = default;

    void init() override {
        core_->get_event_dispatcher().appendListener(
            core::EventType::Packet,
            [this](const core::EventPacket& evt) {
                if (evt.from != core::EventFrom::FromServer) return;
                handle_server_packet(evt);
            }
        );
        spdlog::trace("VendSafeExtension initialized");
    }

    void free() override {
        delete this;
    }

private:
    static bool is_active() {
        
        
        
        return command::VendSafeCommand::is_enabled() || command::VendFastCommand::is_buy_mode_enabled();
    }

    static std::string extract_value(const std::string& src, const std::string& key) {
        size_t pos = src.find(key);
        if (pos == std::string::npos) return "";
        size_t start = pos + key.length();
        size_t end1 = src.find("|", start);
        size_t end2 = src.find("\n", start);
        size_t end = std::string::npos;
        if (end1 != std::string::npos && end2 != std::string::npos) end = std::min(end1, end2);
        else if (end1 != std::string::npos) end = end1;
        else if (end2 != std::string::npos) end = end2;
        if (end == std::string::npos || end <= start) return "";
        return src.substr(start, end - start);
    }

    static std::string extract_price(const std::string& dialog) {
        size_t pos = dialog.find("expectprice|");
        if (pos == std::string::npos) return "";
        size_t start = pos + 12;
        size_t end = start;
        while (end < dialog.size() && (std::isdigit(static_cast<unsigned char>(dialog[end])) || dialog[end] == '-')) {
            end++;
        }
        return dialog.substr(start, end - start);
    }

    static int extract_stock(const std::string& dialog) {
        size_t total_pos = dialog.find("contains a total of ");
        if (total_pos == std::string::npos) return 0;
        size_t num_start = total_pos + 20;
        size_t num_end = num_start;
        while (num_end < dialog.size() && std::isdigit(static_cast<unsigned char>(dialog[num_end]))) {
            num_end++;
        }
        if (num_end <= num_start) return 0;
        try {
            return std::stoi(dialog.substr(num_start, num_end - num_start));
        } catch (...) {
            return 0;
        }
    }

    static std::string to_lower_copy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::string strip_gt_color_codes(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '`') {
                if (i + 1 < input.size()) {
                    ++i;
                }
                continue;
            }
            out.push_back(input[i]);
        }
        return out;
    }

    enum class LockCurrency {
        WorldLock,
        DiamondLock,
        BlueGemLock
    };

    static std::optional<LockCurrency> detect_lock_currency(const std::string& dialog) {
        const std::string clean = to_lower_copy(strip_gt_color_codes(dialog));
        if (clean.find("blue gem lock") != std::string::npos) return LockCurrency::BlueGemLock;
        if (clean.find("diamond lock") != std::string::npos) return LockCurrency::DiamondLock;
        if (clean.find("world lock") != std::string::npos) return LockCurrency::WorldLock;
        return std::nullopt;
    }

    static int extract_price_amount(const std::string& dialog) {
        const std::string clean = strip_gt_color_codes(dialog);
        std::smatch m;
        
        static const std::regex for_lock_re(R"(for\s+(\d+)\s+(?:World|Diamond|Blue Gem)\s+Lock)", std::regex::icase);
        if (std::regex_search(clean, m, for_lock_re) && m.size() > 1) {
            try { return std::max(1, std::stoi(m[1].str())); } catch (...) {}
        }
        
        static const std::regex cost_re(R"(For\s+a\s+cost\s+of[^0-9]*([0-9]+))", std::regex::icase);
        if (std::regex_search(clean, m, cost_re) && m.size() > 1) {
            try { return std::max(1, std::stoi(m[1].str())); } catch (...) {}
        }
        return 1;
    }

    static int compute_max_affordable_buy(const std::string& dialog, int stock_limit) {
        auto currency_opt = detect_lock_currency(dialog);
        if (!currency_opt.has_value()) {
            return std::clamp(stock_limit, 1, 999);
        }

        const int price_per_item = std::max(1, extract_price_amount(dialog));
        auto& inv = utils::InventoryManager::get_instance();
        const int wl = static_cast<int>(inv.get_item_count(242));
        const int dl = static_cast<int>(inv.get_item_count(1796));
        const int bgl = static_cast<int>(inv.get_item_count(7188));

        long long affordable = 0;
        switch (*currency_opt) {
            case LockCurrency::WorldLock: {
                const long long total_wl = static_cast<long long>(wl) +
                                           static_cast<long long>(dl) * 100LL +
                                           static_cast<long long>(bgl) * 10000LL;
                affordable = total_wl / price_per_item;
                break;
            }
            case LockCurrency::DiamondLock: {
                const long long total_dl = static_cast<long long>(dl) +
                                           static_cast<long long>(bgl) * 100LL +
                                           static_cast<long long>(wl) / 100LL;
                affordable = total_dl / price_per_item;
                break;
            }
            case LockCurrency::BlueGemLock: {
                const long long total_bgl = static_cast<long long>(bgl) +
                                            static_cast<long long>(dl) / 100LL +
                                            static_cast<long long>(wl) / 10000LL;
                affordable = total_bgl / price_per_item;
                break;
            }
        }

        const int result = static_cast<int>(std::clamp<long long>(affordable, 1LL, 999LL));
        return std::min(stock_limit, result);
    }

    static int encode_buycount_x10(int item_amount) {
        const int safe_items = std::clamp(item_amount, 1, 999);
        return safe_items * 10;
    }

    void send_dialog_return(const std::string& payload) {
        if (!core_ || !core_->get_client() || !core_->get_client()->get_player()) return;
        ByteStream<std::uint16_t> bs{};
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(payload, false);
        core_->get_client()->get_player()->send_packet(bs.get_data(), 0);
    }

    void send_safe_buy_request(const std::string& tilex, const std::string& tiley,
                               const std::string& expectitem, const std::string& expectprice,
                               int buycount) {
        std::ostringstream packet;
        packet << "action|dialog_return\n"
               << "dialog_name|vending\n"
               << "tilex|" << tilex << "|\n"
               << "tiley|" << tiley << "|\n"
               << "expectprice|" << expectprice << "|\n"
               << "expectitem|" << expectitem << "|\n"
               << "buycount|" << buycount;
        send_dialog_return(packet.str());
    }

    void send_buy_confirm(const std::string& tilex, const std::string& tiley,
                          const std::string& expectitem, const std::string& expectprice,
                          const std::string& buycount) {
        std::ostringstream packet;
        packet << "action|dialog_return\n"
               << "dialog_name|vending\n"
               << "tilex|" << tilex << "|\n"
               << "tiley|" << tiley << "|\n"
               << "verify|1|\n"
               << "buycount|" << buycount << "|\n"
               << "expectprice|" << expectprice << "|\n"
               << "expectitem|" << expectitem << "|";
        send_dialog_return(packet.str());
    }

    void handle_server_packet(const core::EventPacket& evt) {
        const auto& gp = evt.get_packet();
        if (gp.type != packet::PACKET_CALL_FUNCTION || !gp.flags.extended) return;

        
        const auto& ext = evt.get_ext_data();
        if (ext.empty()) return;
        ByteStream<std::uint16_t> reader(const_cast<std::byte*>(ext.data()), ext.size());

        uint8_t param_count = 0;
        if (!reader.read(param_count) || param_count < 2) return;

        uint8_t idx0 = 0, type0 = 0;
        uint32_t fn_len = 0;
        if (!reader.read(idx0) || !reader.read(type0) || type0 != 2 || !reader.read(fn_len)) return;

        std::string fn(fn_len, '\0');
        if (!reader.read_data((std::byte*)fn.data(), fn_len)) return;

        if (fn == "OnTalkBubble" && is_active()) {
            
            uint8_t idx1 = 0, type1 = 0;
            if (!reader.read(idx1) || !reader.read(type1)) return;
            uint32_t dummy = 0;
            if (type1 == 3 || type1 == 4 || type1 == 5) {
                if (!reader.read(dummy)) return;
            } else {
                return;
            }

            uint8_t idx2 = 0, type2 = 0;
            uint32_t txt_len = 0;
            if (!reader.read(idx2) || !reader.read(type2) || type2 != 2 || !reader.read(txt_len)) return;

            std::string text(txt_len, '\0');
            if (!reader.read_data((std::byte*)text.data(), txt_len)) return;

            if (text.find("can't hold that many locks") != std::string::npos && !last_tilex_.empty()) {
                send_safe_buy_request(last_tilex_, last_tiley_, last_expectitem_, last_expectprice_, encode_buycount_x10(1));
                if (core_ && core_->get_server() && core_->get_server()->get_player()) {
                    utils::PacketUtils::send_chat_message(core_->get_server()->get_player(),
                        "`6VendSafe:`o server rejected count, retrying with 1");
                }
            }
            return;
        }

        if (fn != "OnDialogRequest" || !is_active()) return;

        uint8_t idx1 = 0, type1 = 0;
        uint32_t dialog_len = 0;
        if (!reader.read(idx1) || !reader.read(type1) || type1 != 2 || !reader.read(dialog_len)) return;

        std::string dialog(dialog_len, '\0');
        if (!reader.read_data((std::byte*)dialog.data(), dialog_len)) return;

        spdlog::info("VendSafe: OnDialogRequest len={}", dialog_len);

        if (dialog.find("end_dialog|vending|Close|Buy|") != std::string::npos) {
            const std::string tilex = extract_value(dialog, "embed_data|tilex|");
            const std::string tiley = extract_value(dialog, "embed_data|tiley|");
            const std::string expectitem = extract_value(dialog, "embed_data|expectitem|");
            const std::string expectprice = extract_price(dialog);
            if (tilex.empty() || tiley.empty() || expectitem.empty() || expectprice.empty()) {
                spdlog::warn("VendSafe: Missing buy params tilex='{}' tiley='{}' item='{}' price='{}'",
                    tilex, tiley, expectitem, expectprice);
                return;
            }

            int stock = extract_stock(dialog);
            if (stock <= 0) stock = 1;
            int safe_buy_items = compute_max_affordable_buy(dialog, std::clamp(stock, 1, 999));
            safe_buy_items = std::clamp(safe_buy_items, 1, 999);
            const int safe_buy_encoded = encode_buycount_x10(safe_buy_items);

            last_tilex_ = tilex;
            last_tiley_ = tiley;
            last_expectitem_ = expectitem;
            last_expectprice_ = expectprice;
            last_buycount_ = safe_buy_encoded;
            waiting_confirm_ = true;

            send_safe_buy_request(tilex, tiley, expectitem, expectprice, safe_buy_encoded);
            evt.canceled = true;
            return;
        }

        if (dialog.find("end_dialog|vending|Cancel|OK|") != std::string::npos && waiting_confirm_) {
            std::string tilex = extract_value(dialog, "embed_data|tilex|");
            std::string tiley = extract_value(dialog, "embed_data|tiley|");
            std::string expectitem = extract_value(dialog, "embed_data|expectitem|");
            std::string buycount = extract_value(dialog, "embed_data|buycount|");
            std::string expectprice = extract_price(dialog);

            if (tilex.empty()) tilex = last_tilex_;
            if (tiley.empty()) tiley = last_tiley_;
            if (expectitem.empty()) expectitem = last_expectitem_;
            if (expectprice.empty()) expectprice = last_expectprice_;
            if (buycount.empty()) buycount = std::to_string(last_buycount_);

            if (!tilex.empty() && !tiley.empty() && !expectitem.empty() && !expectprice.empty()) {
                send_buy_confirm(tilex, tiley, expectitem, expectprice, buycount);
                evt.canceled = true;
            }
            waiting_confirm_ = false;
            return;
        }
    }
};

} 
