#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_helper.hpp"

namespace command {

class CommandBase {
public:
    CommandBase(
        const std::vector<std::string>& aliases,
        const std::vector<std::string>& parameters,
        const std::string& description,
        size_t min_args = 0
    );

    virtual ~CommandBase() = default;

    virtual void execute(client::Client* client, const std::vector<std::string>& args) = 0;

    [[nodiscard]] const std::vector<std::string>& get_aliases() const { return aliases_; }
    [[nodiscard]] const std::vector<std::string>& get_parameters() const { return parameters_; }
    [[nodiscard]] const std::string& get_description() const { return description_; }
    [[nodiscard]] size_t get_min_args() const { return min_args_; }
    [[nodiscard]] virtual bool has_permission(player::Player* player) const { return true; }

    
    virtual std::unique_ptr<CommandBase> clone() const = 0;

private:
    std::vector<std::string> aliases_;
    std::vector<std::string> parameters_;
    std::string description_;
    size_t min_args_;
};

} 