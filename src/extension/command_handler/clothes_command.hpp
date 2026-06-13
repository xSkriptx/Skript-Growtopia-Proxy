#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class ClothesCommand : public CommandBase {
public:
    ClothesCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static void set_pending_item(int item_id, int clothing_type);

private:
    static void send_clothing_change(client::Client* client);
};

} 
