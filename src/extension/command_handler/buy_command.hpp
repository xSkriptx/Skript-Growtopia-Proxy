#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class BuyCommand : public CommandBase {
public:
    BuyCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    
    static void send_buy_packet(client::Client* client, const std::string& store_id, int count = 1);
    static void set_core(core::Core* core);

private:
    static std::string get_store_id(const std::string& item_name);
};

} 
