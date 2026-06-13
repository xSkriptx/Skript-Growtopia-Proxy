
#include "autocollect_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/inventory_manager.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace command {

core::Core*              AutoCollectCommand::s_core     = nullptr;
std::atomic<bool>        AutoCollectCommand::s_running  {false};
std::atomic<uint64_t>    AutoCollectCommand::s_generation{0};



static void send_console(player::Player* p, const std::string& msg) {
    if (!p) return;
    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(msg);
    std::vector<std::byte> ext = var.serialize();
    packet::GameUpdatePacket pkt{};
    pkt.type           = packet::PACKET_CALL_FUNCTION;
    pkt.net_id         = -1;
    pkt.flags.extended = 1;
    pkt.data_size      = static_cast<uint32_t>(ext.size());
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    bs.write_data(ext.data(), ext.size());
    p->send_packet(bs.get_data(), 0);
}





void AutoCollectCommand::send_collect_packet(player::Player* to_server, uint32_t uid, float x, float y) {
    if (!to_server) return;

    uint8_t buf[60] = {};
    int off = 0;
    auto w1  = [&](uint8_t  v){ buf[off++] = v; };
    auto w4u = [&](uint32_t v){ memcpy(buf+off,&v,4); off+=4; };
    auto w4i = [&](int32_t  v){ memcpy(buf+off,&v,4); off+=4; };
    auto w4f = [&](float    v){ memcpy(buf+off,&v,4); off+=4; };

    w1(11);    
    w1(0);     
    w1(0);     
    w1(0);     
    
    
    int netid = 0;
    if (AutoCollectCommand::s_core) {
        netid = AutoCollectCommand::s_core->get_config().get<int>("player.netid");
    }

    w4i(netid); 
    w4i(0);    
    w4u(0);    
    w4f(0.f);  
    w4u(uid);  
    w4f(x);    
    w4f(y);    
    w4f(0.f);  
    w4f(0.f);  
    w4f(0.f);  
    w4i(0);    
    w4i(0);    
    w4u(0);    

    
    std::vector<std::byte> wire(60);
    uint32_t msg = 4;
    memcpy(wire.data(),    &msg, 4);
    memcpy(wire.data()+4,  buf, 56);

    to_server->send_packet(wire, 0);
    spdlog::info("[AutoCollect] Network Send: Collect UID {} (Item ID unknown) using NetID {}", uid, netid);
}



void AutoCollectCommand::notify_item_drop(float x, float y) {}

AutoCollectCommand::AutoCollectCommand() : CommandBase(
    {"autocollect", "ac"}, {}, "Toggle auto-collect dropped items", 0
) {}

std::unique_ptr<CommandBase> AutoCollectCommand::clone() const {
    return std::make_unique<AutoCollectCommand>(*this);
}

void AutoCollectCommand::set_core(core::Core* core) { s_core = core; }

void AutoCollectCommand::stop() {
    s_running  = false;
    s_generation.fetch_add(1);
}

void AutoCollectCommand::execute(client::Client* , const std::vector<std::string>& ) {
    if (!s_core) return;

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;

    
    if (s_running.load()) {
        s_running = false;
        s_generation.fetch_add(1);
        send_console(server->get_player(), "`0[ `bSkriptProxy `0] `4AutoCollect stopped.");
        return;
    }

    s_running = true;
    const uint64_t gen = ++s_generation;
    send_console(server->get_player(), "`0[ `bSkriptProxy `0] `2AutoCollect started.");
    std::thread([gen]() { run_autocollect(gen); }).detach();
}



void AutoCollectCommand::run_autocollect(uint64_t generation) {
    constexpr auto  INTERVAL = std::chrono::milliseconds(500);
    constexpr float RADIUS   = 3200.0f; 

    spdlog::info("[AutoCollect] started gen={}", generation);
    
    
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> collection_times;

    while (s_running.load() && generation == s_generation.load()) {

        
        if (!s_core) break;
        player::Player* to_server = s_core->get_client()  ? s_core->get_client()->get_player()  : nullptr;
        player::Player* to_local  = s_core->get_server()  ? s_core->get_server()->get_player() : nullptr;

        if (!to_server || !to_local) {
            std::this_thread::sleep_for(INTERVAL);
            continue;
        }

        
        {
            auto wname = utils::WorldManager::get_instance().get_world_name();
            if (wname.empty() || wname == "EXIT") {
                std::this_thread::sleep_for(INTERVAL);
                continue;
            }
        }

        
        float bot_x = 0.f, bot_y = 0.f;
        {
            auto px = s_core->get_config().get<std::string>("player.position.x");
            auto py = s_core->get_config().get<std::string>("player.position.y");
            try { if (!px.empty()) bot_x = std::stof(px); } catch(...) {}
            try { if (!py.empty()) bot_y = std::stof(py); } catch(...) {}
        }

        
        auto& inv      = utils::InventoryManager::get_instance();
        uint32_t inv_size  = inv.get_inventory_size();
        auto     inv_snap  = inv.get_items_snapshot();
        size_t   item_cnt  = inv_snap.size();
        std::unordered_map<uint16_t,uint8_t> amounts;
        for (const auto& it : inv_snap) amounts[it.id] = it.amount;

        
        auto& wm = utils::WorldManager::get_instance();
        std::vector<world::DroppedItemInfo> snap_items = wm.get_items();
        std::vector<world::DroppedItemInfo> snap_live  = wm.get_live_objects();

        
        struct Cand { uint32_t uid; uint16_t id; float x,y,d2; };
        std::vector<Cand> cands;
        cands.reserve(64);
        std::unordered_set<uint32_t> seen;

        auto enqueue = [&](const world::DroppedItemInfo& item){
            if (seen.count(item.Uid)) return;
            float dx = bot_x - item.X, dy = bot_y - item.Y;
            float d2 = dx*dx + dy*dy;
            if (d2 <= RADIUS*RADIUS) {
                seen.insert(item.Uid);
                cands.push_back({item.Uid, item.ItemId, item.X, item.Y, d2});
            }
        };
        for (const auto& it : snap_items) enqueue(it);
        for (const auto& it : snap_live)  enqueue(it);

        if (cands.empty()) { std::this_thread::sleep_for(INTERVAL); continue; }

        
        std::sort(cands.begin(), cands.end(),
            [](const Cand& a, const Cand& b){ return a.d2 < b.d2; });

        
        int sent = 0;
        for (const auto& c : cands) {
            if (!s_running.load() || generation != s_generation.load()) break;

            
            bool can = false;
            auto it = amounts.find(c.id);
            if (it != amounts.end())
                can = (it->second < 200);
            else
                can = (inv_size == 0 || item_cnt < inv_size);

            if (!can) continue;

            
            auto now = std::chrono::steady_clock::now();
            auto last_it = collection_times.find(c.uid);
            if (last_it != collection_times.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_it->second).count();
                if (elapsed < 2) continue; 
            }

            
            player::Player* srv = s_core->get_client() ? s_core->get_client()->get_player() : nullptr;
            if (!srv) break;

            
            
            for (int brute = -10; brute <= 10; ++brute) {
                uint32_t b_uid = static_cast<uint32_t>(static_cast<int32_t>(c.uid) + brute);
                if (b_uid == 0) continue;
                send_collect_packet(srv, b_uid, c.x, c.y);
            }

            collection_times[c.uid] = now;
            
            int netid = s_core ? s_core->get_config().get<int>("player.netid") : 0;
            spdlog::info("[AutoCollect] BRUTE-FORCE SWEEP sent for uid={} (Range +/-10) id={} (NetID={})", 
                         c.uid, c.id, netid);

            
            

            
            player::Player* loc = s_core->get_server() ? s_core->get_server()->get_player() : nullptr;
            send_console(loc, fmt::format("`2[AC]`w Collecting item `5{}`` uid:`3{}``", c.id, c.uid));
            ++sent;
        }

        if (sent > 0)
            spdlog::info("[AutoCollect] tick: {} packets sent", sent);

        std::this_thread::sleep_for(INTERVAL);
    }

    spdlog::info("[AutoCollect] stopped gen={}", generation);
    s_running = false;
}

} 
