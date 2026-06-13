#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class WeatherCommand : public CommandBase {
public:
    WeatherCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static void send_weather_packet(client::Client* client, int weather_id);
};

} 
