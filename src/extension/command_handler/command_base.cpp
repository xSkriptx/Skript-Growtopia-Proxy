#include "command_base.hpp"

namespace command {

CommandBase::CommandBase(
    const std::vector<std::string>& aliases,
    const std::vector<std::string>& parameters,
    const std::string& description,
    size_t min_args
) : aliases_(aliases), parameters_(parameters), description_(description), min_args_(min_args) {
}

} 