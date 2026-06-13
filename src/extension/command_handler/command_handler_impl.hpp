#include "bank_command.hpp"
#include "bank_withdraw_command.hpp"
#include "bank_check_command.hpp"
#pragma once
#include "command_handler.hpp"
#include "warp_command.hpp"
#include "run_command.hpp"
#include "door_command.hpp"
#include "doorbf_command.hpp"
#include "doorid_command.hpp"
#include "dat_command.hpp"
#include "spam_command.hpp"
#include "name_command.hpp"
#include "ping_command.hpp"
#include "gui_command.hpp"
#include "title_command.hpp"
#include "cleartitle_command.hpp"
#include "mentor_command.hpp"
#include "warn_command.hpp"
#include "fakemaint_command.hpp"
#include "join_command.hpp"
#include "growscan_command.hpp"
#include "gsbeta_command.hpp"
#include "inventory_command.hpp"
#include "dropall_command.hpp"
#include "autocomp_command.hpp"
#include "autocollect_command.hpp"
#include "dropfast_command.hpp"
#include "drop_currency_command.hpp"
#include "trashfast_command.hpp"
#include "banfire_command.hpp"
#include "autosurg_command.hpp"
#include "autocrime_command.hpp"
#include "admin_command.hpp"
#include "find_command.hpp"
#include "vendloc_command.hpp"
#include "vendlogs_command.hpp"
#include "buy_command.hpp"
#include "fillgbc_command.hpp"
#include "dbox_command.hpp"
#include "eventmenu_command.hpp"
#include "mailclaim_command.hpp"
#include "weather_command.hpp"
#include "clothes_command.hpp"
#include "clearclothes_command.hpp"
#include "vendfast_command.hpp"
#include "vendsafe_command.hpp"
#include "banall_command.hpp"
#include "pullall_command.hpp"
#include "ignorecsn_command.hpp"
#include "ignorecsnchat_command.hpp"
#include "host_command.hpp"
#include "wrench_command.hpp"
#include "moddetect_command.hpp"
#include "lockefind_command.hpp"
#include "locketest001_command.hpp"
#include "proxy_command.hpp"
#include "position_command.hpp"
#include "devicecheck_command.hpp"
#include "dropat_command.hpp"
#include "utility_commands.hpp"
#include "immune_command.hpp"
#include "../../core/core.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/tank_packet.hpp"
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <fmt/format.h>

namespace extension::command_handler {

class CommandHandlerExtension final : public ICommandHandlerExtension {
    core::Core* core_;
    std::unordered_map<std::string, std::unique_ptr<command::CommandBase>> commands_;
    
public:
    explicit CommandHandlerExtension(core::Core* core) : core_{core} {
    }

    ~CommandHandlerExtension() override = default;

    void init() override {
        
        command::AdminCommand::set_core(core_);
        command::FindCommand::set_core(core_);
        command::VendLocCommand::set_core(core_);
        command::VendTPCommand::set_core(core_);
        command::VendLogsCommand::set_core(core_);
        command::WeatherCommand::set_core(core_);
        command::ClothesCommand::set_core(core_);
        command::VendFastCommand::set_core(core_);
        command::VendSafeCommand::set_core(core_);
        command::JoinCommand::set_core(core_);
        command::BanallCommand::set_core(core_);
        command::PullallCommand::set_core(core_);
        command::IgnoreCSNCommand::set_core(core_);
        command::IgnoreCSNChatCommand::set_core(core_);
        command::HostCommand::set_core(core_);
        command::WrenchCommand::set_core(core_);
        command::ModDetectCommand::set_core(core_);
        command::LockeFindCommand::set_core(core_);
        command::LockeTest001Command::set_core(core_);
        command::ProxyCommand::set_core(core_);
        command::InfoCommand::set_core(core_);
        command::DoorIDCommand::set_core(core_);
        command::DoorBFCommand::set_core(core_);
        command::DatCommand::set_core(core_);
        command::SpamCommand::set_core(core_);
        command::DropAllCommand::set_core(core_);
        command::AutoCompCommand::set_core(core_);
        command::AutoCollectCommand::set_core(core_);
        command::BuyCommand::set_core(core_);
        command::FillGBCCommand::set_core(core_);
        command::DboxCommand::set_core(core_);
        command::DropFastCommand::set_core(core_);
        command::TrashFastCommand::set_core(core_);
        command::DropWLCommand::set_core(core_);
        command::DropDLCommand::set_core(core_);
        command::DropBGLCommand::set_core(core_);
        command::GhostCharCommand::set_core(core_);
        command::VisualDropCommand::set_core(core_);
        command::BanFireCommand::set_core(core_);
        command::AutoSurgCommand::set_core(core_);
        command::AutoCrimeCommand::set_core(core_);
        command::PositionCommand::set_core(core_);
        command::DeviceCheckCommand::set_core(core_);
        command::TeleportPosCommand::set_core(core_);
        command::BackCommand::set_core(core_);
        command::DropAtCommand::set_core(core_);
        command::FindPathCommand::set_core(core_);
        command::RunCommand::set_core(core_);
        command::PlayerTPCommand::set_core(core_);
        command::FlagCommand::set_core(core_);
        command::InvisCommand::set_core(core_);

        command::BankCommand::set_core(core_);
        command::BankWithdrawCommand::set_core(core_);
        register_command(std::make_unique<command::BankCommand>());
        register_command(std::make_unique<command::BankWithdrawCommand>());
        register_command(std::make_unique<command::BankCheckCommand>());
        command::SmCommand::set_core(core_);
        command::FreezeCommand::set_core(core_);
        command::HitVFXCommand::set_core(core_);
        command::AuctionBidCommand::set_core(core_);
        command::UbiclubCommand::set_core(core_);
        command::WorldLockStorageCommand::set_core(core_);
        command::EventCommand::set_core(core_);
        command::MstateCommand::set_core(core_);
        command::BetaCommand::set_core(core_);
        command::InitWorldCommand::set_core(core_);
        command::GemsCommand::set_core(core_);
        command::BuxCommand::set_core(core_);
        command::BubbleCommand::set_core(core_);
        command::OverlayCommand::set_core(core_);
        command::ZoomCommand::set_core(core_);
        command::RainbowCommand::set_core(core_);
        command::InfinityCommand::set_core(core_);
        command::DragonCommand::set_core(core_);
        command::RiftWingsCommand::set_core(core_);
        command::SetWeatherCommand::set_core(core_);
        command::SetPosCommand::set_core(core_);
        command::ConsoleMessageCommand::set_core(core_);
        command::DialogCommand::set_core(core_);
        command::TradeCommand::set_core(core_);
        command::ActionCommand::set_core(core_);
        command::AntiGravityCommand::set_core(core_);
        command::AntiPunchCommand::set_core(core_);
        command::TitleIconCommand::set_core(core_);
        command::VisionCommand::set_core(core_);
        command::SpawnBGLsCommand::set_core(core_);
        command::ChestCommand::set_core(core_);
        command::RemaCommand::set_core(core_);
        command::ImmuneCommand::set_core(core_);

        
        command::SkinCommand::set_core(core_);
        command::DeathAnimCommand::set_core(core_);
        command::RespawnAnimCommand::set_core(core_);
        command::ParticleCommand::set_core(core_);
        command::ItemEffectCommand::set_core(core_);
        command::BannerCommand::set_core(core_);
        command::DisguiseCommand::set_core(core_);
        command::PaintballCommand::set_core(core_);
        command::BroadcastCommand::set_core(core_);
        command::RoleSkinCommand::set_core(core_);
        command::LevelUpCommand::set_core(core_);
        command::RawVariantCommand::set_core(core_);
        command::PlayersInfoCommand::set_core(core_);
        
        
        register_command(std::make_unique<command::WarpCommand>());
        register_command(std::make_unique<command::RunCommand>());
        register_command(std::make_unique<command::DoorCommand>());
        register_command(std::make_unique<command::DoorBFCommand>());
        register_command(std::make_unique<command::DoorIDCommand>());
        register_command(std::make_unique<command::DatCommand>());
        register_command(std::make_unique<command::SpamCommand>());
        register_command(std::make_unique<command::SpamTextCommand>());
        register_command(std::make_unique<command::SpamDelayCommand>());
        register_command(std::make_unique<command::NameCommand>());
        register_command(std::make_unique<command::PingCommand>());
        register_command(std::make_unique<command::GuiCommand>());
        register_command(std::make_unique<command::TitleCommand>());
        register_command(std::make_unique<command::G4GCommand>());
        register_command(std::make_unique<command::MaxLevelCommand>());
        register_command(std::make_unique<command::DrCommand>());
        register_command(std::make_unique<command::MentorCommand>());
        register_command(std::make_unique<command::ClearTitleCommand>());
        register_command(std::make_unique<command::WarnCommand>());
        register_command(std::make_unique<command::FakeMaintCommand>());
        register_command(std::make_unique<command::JoinCommand>());
        register_command(std::make_unique<command::GrowScanCommand>());
        register_command(std::make_unique<command::GsBetaCommand>());
        register_command(std::make_unique<command::InventoryCommand>());
        register_command(std::make_unique<command::DropAllCommand>());
        register_command(std::make_unique<command::AutoCompCommand>());
        register_command(std::make_unique<command::AutoCollectCommand>());
        register_command(std::make_unique<command::DropFastCommand>());
        register_command(std::make_unique<command::DropWLCommand>());
        register_command(std::make_unique<command::DropDLCommand>());
        register_command(std::make_unique<command::DropBGLCommand>());
        register_command(std::make_unique<command::GhostCharCommand>());
        register_command(std::make_unique<command::VisualDropCommand>());
        register_command(std::make_unique<command::FillGBCCommand>());
        register_command(std::make_unique<command::DboxCommand>());
        register_command(std::make_unique<command::TrashFastCommand>());
        register_command(std::make_unique<command::BanFireCommand>());
        register_command(std::make_unique<command::AutoSurgCommand>());
        register_command(std::make_unique<command::AutoCrimeCommand>());
        register_command(std::make_unique<command::AdminCommand>());
        register_command(std::make_unique<command::FindCommand>());
        register_command(std::make_unique<command::VendLocCommand>());
        register_command(std::make_unique<command::VendTPCommand>());
        register_command(std::make_unique<command::VendLogsCommand>());
        register_command(std::make_unique<command::BuyCommand>());
        register_command(std::make_unique<command::WeatherCommand>());
        register_command(std::make_unique<command::PositionCommand>());
        register_command(std::make_unique<command::DeviceCheckCommand>());
        register_command(std::make_unique<command::TeleportPosCommand>());
        register_command(std::make_unique<command::BackCommand>());
        register_command(std::make_unique<command::DropAtCommand>());
        register_command(std::make_unique<command::FindPathCommand>());
        register_command(std::make_unique<command::PlayerTPCommand>());
        register_command(std::make_unique<command::FlagCommand>());
        register_command(std::make_unique<command::InvisCommand>());
        register_command(std::make_unique<command::SmCommand>());
        register_command(std::make_unique<command::FreezeCommand>());
        register_command(std::make_unique<command::HitVFXCommand>());
        register_command(std::make_unique<command::AuctionBidCommand>());
        register_command(std::make_unique<command::UbiclubCommand>());
        register_command(std::make_unique<command::WorldLockStorageCommand>());
        register_command(std::make_unique<command::EventCommand>());
        register_command(std::make_unique<command::MstateCommand>());
        register_command(std::make_unique<command::BetaCommand>());
        register_command(std::make_unique<command::InitWorldCommand>());
        register_command(std::make_unique<command::GemsCommand>());
        register_command(std::make_unique<command::BuxCommand>());
        register_command(std::make_unique<command::BubbleCommand>());
        register_command(std::make_unique<command::OverlayCommand>());
        register_command(std::make_unique<command::ZoomCommand>());
        register_command(std::make_unique<command::RainbowCommand>());
        register_command(std::make_unique<command::InfinityCommand>());
        register_command(std::make_unique<command::DragonCommand>());
        register_command(std::make_unique<command::RiftWingsCommand>());
        register_command(std::make_unique<command::SetWeatherCommand>());
        register_command(std::make_unique<command::SetPosCommand>());
        register_command(std::make_unique<command::ConsoleMessageCommand>());
        register_command(std::make_unique<command::DialogCommand>());
        register_command(std::make_unique<command::TradeCommand>());
        register_command(std::make_unique<command::ActionCommand>());
        register_command(std::make_unique<command::AntiGravityCommand>());
        register_command(std::make_unique<command::AntiPunchCommand>());
        register_command(std::make_unique<command::TitleIconCommand>());
        register_command(std::make_unique<command::VisionCommand>());
        register_command(std::make_unique<command::SpawnBGLsCommand>());
        register_command(std::make_unique<command::ChestCommand>());
        register_command(std::make_unique<command::RemaCommand>());
        register_command(std::make_unique<command::ClothesCommand>());
        register_command(std::make_unique<command::ClearClothesCommand>());
        register_command(std::make_unique<command::VendFastCommand>());
        register_command(std::make_unique<command::VendSafeCommand>());
        register_command(std::make_unique<command::BanallCommand>());
        register_command(std::make_unique<command::PullallCommand>());
        register_command(std::make_unique<command::IgnoreCSNCommand>());
        register_command(std::make_unique<command::IgnoreCSNChatCommand>());
        register_command(std::make_unique<command::HostCommand>());
        register_command(std::make_unique<command::WrenchCommand>());
        register_command(std::make_unique<command::ModDetectCommand>());
        register_command(std::make_unique<command::LockeFindCommand>());
        register_command(std::make_unique<command::LockeTest001Command>());
        register_command(std::make_unique<command::ProxyCommand>());
        register_command(std::make_unique<command::InfoCommand>());
        register_command(std::make_unique<command::ImmuneCommand>());
        register_command(std::make_unique<command::EventMenuCommand>());
        register_command(std::make_unique<command::MailClaimCommand>());

        
        register_command(std::make_unique<command::SkinCommand>());
        register_command(std::make_unique<command::DeathAnimCommand>());
        register_command(std::make_unique<command::RespawnAnimCommand>());
        register_command(std::make_unique<command::ParticleCommand>());
        register_command(std::make_unique<command::ItemEffectCommand>());
        register_command(std::make_unique<command::BannerCommand>());
        register_command(std::make_unique<command::DisguiseCommand>());
        register_command(std::make_unique<command::PaintballCommand>());
        register_command(std::make_unique<command::BroadcastCommand>());
        register_command(std::make_unique<command::RoleSkinCommand>());
        register_command(std::make_unique<command::LevelUpCommand>());
        register_command(std::make_unique<command::RawVariantCommand>());
        register_command(std::make_unique<command::PlayersInfoCommand>());

        spdlog::trace("Registered {} commands", commands_.size());

        core_->get_event_dispatcher().prependListener(
            core::EventType::Message,
            [this](const core::EventMessage& event) {
                handle_chat_message(event);
            }
        );

        core_->get_event_dispatcher().prependListener(
            core::EventType::Packet,
            [this](const core::EventPacket& event) {
                handle_dialog_response(event);
            }
        );

        spdlog::trace("Command Handler extension loaded with GUI support");
    }

    void free() override {
        commands_.clear();
        delete this;
    }

    void register_command(std::unique_ptr<command::CommandBase> command) {
        for (const auto& alias : command->get_aliases()) {
            commands_[alias] = command->clone();
        }
    }

    bool execute_command(player::Player* player, const std::string& command_line) {
        
        std::string trimmed_line = trim(command_line);
        
        spdlog::info("[COMMAND] Input: '{}' (len={})", command_line, command_line.length());
        spdlog::info("[COMMAND] After trim: '{}' (len={})", trimmed_line, trimmed_line.length());
        
        if (trimmed_line.empty()) {
            spdlog::warn("[COMMAND] Empty command after trim");
            return false;
        }
        
        
        
        size_t space_pos = trimmed_line.find(' ');
        std::string raw_command_name;
        std::string args_string;
        
        if (space_pos != std::string::npos) {
            
            raw_command_name = trimmed_line.substr(0, space_pos);
            args_string = trimmed_line.substr(space_pos + 1);
        } else {
            
            raw_command_name = trimmed_line;
            args_string = "";
        }
        
        
        
        std::string command_name = raw_command_name;
        auto it = commands_.find(command_name);
        
        if (it == commands_.end()) {
            
            
            std::string best_match;
            for (const auto& [registered_cmd, _] : commands_) {
                
                if (raw_command_name.rfind(registered_cmd, 0) == 0) {
                    
                    if (registered_cmd.length() > best_match.length()) {
                        best_match = registered_cmd;
                    }
                }
            }
            
            if (!best_match.empty()) {
                
                command_name = best_match;
                std::string extra_chars = raw_command_name.substr(best_match.length());
                if (!extra_chars.empty()) {
                    
                    args_string = args_string.empty() ? extra_chars : (extra_chars + " " + args_string);
                    spdlog::info("[COMMAND] Keyboard buffer detected: '{}' matched to '{}', extra chars: '{}'", 
                                 raw_command_name, command_name, extra_chars);
                }
                it = commands_.find(command_name);
            }
        }
        
        spdlog::info("[COMMAND] Command name: '{}'", command_name);
        spdlog::info("[COMMAND] Args string: '{}'", args_string);
        
        
        std::vector<std::string> args;
        
        
        args.push_back(command_name);
        
        if (!args_string.empty()) {
            auto parsed_args = parse_command(args_string);
            
            args.insert(args.end(), parsed_args.begin(), parsed_args.end());
        }
        
        spdlog::info("[COMMAND] Total args (including command name): {}", args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            spdlog::info("[COMMAND]   arg[{}] = '{}'", i, args[i]);
        }
        
        if (it == commands_.end()) {
            spdlog::warn("[COMMAND] Command '{}' not found in {} registered commands", command_name, commands_.size());
            return false;
        }

        spdlog::info("[COMMAND] Executing command: '{}'", command_name);
        auto& command = it->second;
        
        if (!command->has_permission(player)) {
            utils::PacketUtils::send_chat_message(player, "`4You don't have permission to use this command.");
            return true;
        }

        if (args.size() < command->get_min_args()) {
            std::string usage = fmt::format("`4Usage: /{} {}", command_name, 
                fmt::join(command->get_parameters(), " "));
            utils::PacketUtils::send_chat_message(player, usage);
            return true;
        }

        try {
            client::Client* client = core_->get_client();
            
            if (auto* title_cmd = dynamic_cast<command::TitleCommand*>(command.get())) {
                title_cmd->execute_with_core(client, args, core_);
            } else if (auto* g4g_cmd = dynamic_cast<command::G4GCommand*>(command.get())) {
                g4g_cmd->execute_with_core(client, args, core_);
            } else if (auto* maxlv_cmd = dynamic_cast<command::MaxLevelCommand*>(command.get())) {
                maxlv_cmd->execute_with_core(client, args, core_);
            } else if (auto* dr_cmd = dynamic_cast<command::DrCommand*>(command.get())) {
                dr_cmd->execute_with_core(client, args, core_);
            } else if (auto* clear_cmd = dynamic_cast<command::ClearTitleCommand*>(command.get())) {
                clear_cmd->execute_with_core(client, args, core_);
            } else if (auto* mentor_cmd = dynamic_cast<command::MentorCommand*>(command.get())) {
                mentor_cmd->execute_with_core(client, args, core_);
            } else if (auto* growscan_cmd = dynamic_cast<command::GrowScanCommand*>(command.get())) {
                spdlog::info("[CMD DEBUG] Executing GrowScan command");
                growscan_cmd->execute_with_core(client, args, core_);
            } else if (auto* gsbeta_cmd = dynamic_cast<command::GsBetaCommand*>(command.get())) {
                gsbeta_cmd->execute_with_core(client, args, core_);
            } else if (auto* warn_cmd = dynamic_cast<command::WarnCommand*>(command.get())) {
                warn_cmd->execute_with_core(client, args, core_);
            } else if (auto* fakemaint_cmd = dynamic_cast<command::FakeMaintCommand*>(command.get())) {
                fakemaint_cmd->execute_with_core(client, args, core_);
            } else if (auto* join_cmd = dynamic_cast<command::JoinCommand*>(command.get())) {
                command->execute(client, args);
            } else if (auto* name_cmd = dynamic_cast<command::NameCommand*>(command.get())) {
                name_cmd->execute_with_core(client, args, core_);
            } else if (auto* ping_cmd = dynamic_cast<command::PingCommand*>(command.get())) {
                ping_cmd->execute_with_core(client, args, core_);
            } else {
                command->execute(client, args);
            }
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Command execution failed: {}", e.what());
            utils::PacketUtils::send_chat_message(player, "`4Command execution failed.");
            return true;
        }
    }

    void send_gui_to_player(player::Player* player, const std::string& dialog_type = "main") {
        if (!player) return;

        try {
            std::string dialog_data;
            
            if (dialog_type == "proxy") {
                dialog_data = 
                    "set_default_color|`o\n"
                    "add_label_with_icon|big|`wSkript Proxy Commands``|left|5016\n"
                    "add_spacer|small\n"
                    "add_textbox|Available Proxy Commands:|left\n"
                    "add_spacer|small\n"
                    "add_button|warp_gui|`2Warp to World``\n"
                    "add_button|name_gui|`5Change Name``\n"
                    "add_button|title_gui|`3Title Selection``\n"
                    "add_button|drop_gui|`6Drop Items``\n"  
                    "add_button|info_gui|`9Proxy Info``\n"
                    "add_spacer|small\n"
                    "add_quick_exit\n"
                    "end_dialog|proxy_gui|Cancel|Okay";
            } 
            else if (dialog_type == "info") {
                dialog_data = 
                    "set_default_color|`o\n"
                    "add_label_with_icon|big|`wProxy Information``|left|758\n"
                    "add_spacer|small\n"
                    "add_textbox|Skript Proxy v2.0|left\n"
                    "add_textbox|Connection Issues Fixed|left\n"
                    "add_textbox|Features:|left\n"
                    "add_textbox|- Zoom Mod & JP Flag|left\n"
                    "add_textbox|- World Warping|left\n"
                    "add_textbox|- Name Changing|left\n"
                    "add_textbox|- Title Selection|left\n"
                    "add_textbox|- Item Dropping|left\n"  
                    "add_textbox|- GUI Interface|left\n"
                    "add_spacer|small\n"
                    "add_button|back|`5Back to Main``\n"
                    "add_quick_exit\n"
                    "end_dialog|info_gui|Close|Okay";
            }
            else if (dialog_type == "title") {
                dialog_data = 
                    "set_default_color|`o\n"
                    "add_label_with_icon|big|`wTitle Manager``|left|5016|\n"
                    "add_spacer|small|\n"
                    "add_textbox|`oSelect a title below. Titles can be combined!``|left|\n"
                    "add_spacer|small|\n"
                    
                    "add_label_with_icon|small|`2G4G Title``|left|11304|\n"
                    "add_smalltext|`oShows the green [G4G] badge next to your name``|\n"
                    "add_button|g4g|`2Apply G4G``|\n"
                    "add_spacer|small|\n"
                    
                    "add_label_with_icon|small|`5Max Level Title``|left|11302|\n"
                    "add_smalltext|`oShows the purple [125] level badge``|\n"
                    "add_button|maxlv|`5Apply Max Level``|\n"
                    "add_spacer|small|\n"
                    
                    "add_label_with_icon|small|`9Dr. Title``|left|11300|\n"
                    "add_smalltext|`oAdds 'Dr.' prefix to your name with doctor badge``|\n"
                    "add_button|dr|`9Apply Dr.``|\n"
                    "add_spacer|small|\n"
                    
                    "add_label_with_icon|small|`6Mentor Title``|left|11298|\n"
                    "add_smalltext|`oShows mentor badge with your name``|\n"
                    "add_button|mentor|`6Apply Mentor``|\n"
                    "add_spacer|small|\n"
                    
                    "add_label_with_icon|small|`4Clear All``|left|758|\n"
                    "add_smalltext|`oRemove all titles and reset to original name``|\n"
                    "add_button|cleartitle|`4Clear Titles``|\n"
                    "add_spacer|small|\n"
                    
                    "add_textbox|`w💡 Tip: You can use multiple title commands to combine them!``|left|\n"
                    "add_quick_exit|\n"
                    "end_dialog|title_gui|Close||";
            }
            else { 
                dialog_data = 
                    "set_default_color|`o\n"
                    "add_label_with_icon|big|`wSkript Proxy v2.0``|left|5016\n"
                    "add_spacer|small\n"
                    "add_textbox|Welcome to Skript Proxy! Select a feature:|left\n"
                    "add_spacer|small\n"
                    "add_text_input|world_name|World Name:| |30\n"
                    "add_spacer|small\n"
                    "add_button|warp|`2Warp to World``\n"
                    "add_button|name_change|`5Change Name``\n"
                    "add_button|title|`3Title Selection``\n"
                    "add_button|drop|`6Drop Items``\n"  
                    "add_button|proxy|`9Proxy Commands``\n"
                    "add_button|info|`6Proxy Info``\n"
                    "add_spacer|small\n"
                    "add_quick_exit\n"
                    "end_dialog|skript_gui|Cancel|Okay";
            }

            packet::Variant variant{};
            variant.add("OnDialogRequest");
            variant.add(dialog_data);
            
            std::vector<std::byte> ext_data = variant.serialize();

            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = -1;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());

            player->send_packet(byte_stream.get_data(), 0);
            
            spdlog::info("GUI interface '{}' displayed for player {}", dialog_type, player->get_peer()->connectID);

        } catch (const std::exception& e) {
            spdlog::error("Failed to send GUI interface: {}", e.what());
        }
    }

private:
    void handle_chat_message(const core::EventMessage& event) {
        if (event.from != core::EventFrom::FromClient) {
            return;
        }

        TextParse text_parse(event.get_message().get_raw());
        
        std::string action = clean_string(text_parse.get("action"));
        std::string message = clean_string(text_parse.get("text"));
        
        
        if (message.empty() && !action.empty() && action[0] == '/') {
            message = action;
        }
        
        
        if (action == "dialog_return") {
            std::string dialog_name = text_parse.get("dialog_name");
            std::string button_clicked = text_parse.get("buttonClicked");
            
            if (dialog_name == "title_gui") {
                handle_title_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked);
                event.canceled = true;
                return;
            }
            else if (dialog_name == "growscan_menu" || dialog_name == "growscan_results") {
                handle_growscan_response(const_cast<player::Player*>(&event.get_player()), button_clicked, dialog_name);
                event.canceled = true;
                return;
            }
            else if (dialog_name == "gsbeta_menu" || dialog_name == "gsbeta_results") {
                handle_gsbeta_response(const_cast<player::Player*>(&event.get_player()), button_clicked, dialog_name);
                event.canceled = true;
                return;
            }
            else if (dialog_name == "find_items" || dialog_name == "vending_locations") {
                
                auto* server = core_->get_server();
                if (server && server->get_player()) {
                    command::FindCommand::handle_dialog_click(server->get_player(), button_clicked);
                }
                event.canceled = true;
                return;
            }
            else if (dialog_name == "vendloc_search") {
                
                std::string button_clicked = text_parse.get("buttonClicked");
                auto* server = core_->get_server();
                if (server && server->get_player()) {
                    
                    if (!button_clicked.empty() && button_clicked.rfind("viewworlds_", 0) == 0) {
                        command::VendLocCommand::handle_dialog_click(server->get_player(), button_clicked);
                    } else {
                        
                        std::string search_query = text_parse.get("vend_search");
                        command::VendLocCommand::handle_dialog_return(server->get_player(), search_query);
                    }
                }
                event.canceled = true;
                return;
            }
            else if (dialog_name == "vendloc_locations") {
                
                auto* server = core_->get_server();
                if (server && server->get_player()) {
                    command::VendLocCommand::handle_dialog_click(server->get_player(), button_clicked);
                }
                event.canceled = true;
                return;
            }
            else if (dialog_name == "buy_dialog") {
                
                std::string store_id = text_parse.get("store_id");
                std::string count_str = text_parse.get("count");
                
                if (!store_id.empty()) {
                    int count = 1;
                    try {
                        if (!count_str.empty()) {
                            count = std::stoi(count_str);
                        }
                    } catch (...) {
                        count = 1;
                    }
                    
                    auto* client = core_->get_client();
                    if (client && client->get_player()) {
                        command::BuyCommand::send_buy_packet(client, store_id, count);
                    }
                }
                event.canceled = true;
                return;
            }
            else if (dialog_name == "host_settings") {
                
                
                command::HostCommand::apply_dialog_settings(text_parse);
                event.canceled = true;
                return;
            }
            else if (dialog_name == "wrench_settings") {
                command::WrenchCommand::apply_dialog_settings(text_parse);
                event.canceled = true;
                return;
            }
        }
        
        
        std::string command_source = message;
        if (command_source.empty() || command_source.size() <= 1) {
            
            
            if (!action.empty() && action.size() > 1 && action[0] == '/') {
                command_source = action;
                spdlog::info("[CMD DEBUG] Text field empty/short, using action field: '{}'", command_source);
            }
        }
        
        if (command_source.size() > 1 && command_source[0] == '/') {
            
            std::string raw_bytes;
            for (size_t i = 0; i < command_source.length() && i < 50; ++i) {
                raw_bytes += fmt::format("[{}:0x{:02X}] ", command_source[i], (unsigned char)command_source[i]);
            }
            spdlog::info("[CHAT] Raw bytes: {}", raw_bytes);
            
            std::string command_text = trim(command_source.substr(1));  
            
            spdlog::info("[CHAT] Raw command from client: '{}' (len={})", command_source, command_source.length());
            spdlog::info("[CHAT] After substr(1) and trim: '{}' (len={})", command_text, command_text.length());
            
            if (command_text == "gui" || command_text == "menu" || command_text == "interface") {
                send_gui_to_player(const_cast<player::Player*>(&event.get_player()), "main");
                event.canceled = true;
                return;
            }
            else if (command_text == "info") {
                send_gui_to_player(const_cast<player::Player*>(&event.get_player()), "info");
                event.canceled = true;
                return;
            }
            
            else if (command_text == "drop" || command_text.find("drop ") == 0) {
                if (execute_command(const_cast<player::Player*>(&event.get_player()), command_text)) {
                    event.canceled = true;
                }
                return;
            }
            
            if (execute_command(const_cast<player::Player*>(&event.get_player()), command_text)) {
                spdlog::info("[CHAT] Command executed and canceled");
                event.canceled = true;
            } else {
                spdlog::info("[CHAT] Command not handled by proxy, letting server handle it");
            }
        }
        
        
        else if (!command_source.empty() && action == "input") {
            std::string plain_text = trim(command_source);
            if (!plain_text.empty()) {
                std::string lowered = plain_text;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                const bool is_doorbf =
                    (lowered == "doorbf") ||
                    (lowered.rfind("doorbf ", 0) == 0);

                if (is_doorbf) {
                    spdlog::info("[DEBUG-CMD] doorbf without slash: '{}'", plain_text);
                    if (execute_command(const_cast<player::Player*>(&event.get_player()), plain_text)) {
                        spdlog::info("[DEBUG-CMD] doorbf executed and canceled");
                        event.canceled = true;
                    }
                }
            }
        }
    }

    void handle_dialog_response(const core::EventPacket& event) {
        const auto& game_packet = event.get_packet();
        const auto& ext_data = event.get_ext_data();

        
        if (game_packet.type == packet::PACKET_TILE_CHANGE_REQUEST ||
            game_packet.type == packet::PACKET_TILE_ACTIVATE_REQUEST) {
            const packet::TankUpdatePacket* tank = nullptr;
            if (ext_data.size() >= sizeof(packet::TankUpdatePacket)) {
                tank = reinterpret_cast<const packet::TankUpdatePacket*>(ext_data.data());
            } else {
                tank = reinterpret_cast<const packet::TankUpdatePacket*>(&game_packet);
            }
            client::Client* client = core_->get_client();
            if (client && command::FindPathCommand::handle_shift_click(client, tank->int_x, tank->int_y)) {
                const_cast<core::EventPacket&>(event).canceled = true;
                return;
            }
            
        }

        if (event.from != core::EventFrom::FromClient) {
            return;
        }

        if (game_packet.type != packet::PACKET_CALL_FUNCTION) {
            return;
        }

        if (ext_data.empty()) {
            return;
        }

        try {
            packet::Variant variant{};
            if (!variant.deserialize(ext_data)) {
                return;
            }

            if (variant.size() >= 2 && variant.get<std::string>(0) == "OnDialogReturn") {
                std::string dialog_name = variant.get<std::string>(1);
                std::string dialog_data = variant.get<std::string>(2);
                TextParse text_parse{dialog_data};
                
                std::string button_clicked = text_parse.get("buttonClicked");
                std::string world_name = text_parse.get("world_name");
                
                spdlog::info("[DIALOG] Received dialog return - name: '{}', button_clicked: '{}', data_size: {}", 
                           dialog_name, button_clicked, dialog_data.size());
                spdlog::info("[DIALOG] Raw dialog data: '{}'", dialog_data);
                
                if (dialog_name == "skript_gui") {
                    handle_main_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked, world_name);
                }
                else if (dialog_name == "proxy_gui") {
                    handle_proxy_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked);
                }
                else if (dialog_name == "info_gui") {
                    handle_info_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked);
                }
                else if (dialog_name == "name_change") {
                    handle_name_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked, text_parse);
                }
                else if (dialog_name == "title_gui") {
                    handle_title_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked);
                }
                else if (dialog_name == "host_settings") {
                    command::HostCommand::apply_dialog_settings(text_parse);
                }
                else if (dialog_name == "wrench_settings") {
                    command::WrenchCommand::apply_dialog_settings(text_parse);
                }
                else if (dialog_name == "drop_gui") {
                    handle_drop_gui_response(const_cast<player::Player*>(&event.get_player()), button_clicked, text_parse);
                }
                else if (dialog_name == "growscan_menu" || dialog_name == "growscan_results") {
                    handle_growscan_response(const_cast<player::Player*>(&event.get_player()), button_clicked, dialog_name);
                }
                
                
                event.canceled = true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error handling dialog response: {}", e.what());
        }
    }

    void handle_main_gui_response(player::Player* player, const std::string& button_clicked, const std::string& world_name) {
        if (button_clicked == "warp") {
            if (!world_name.empty()) {
                std::vector<std::string> warp_args{world_name};
                if (auto it = commands_.find("warp"); it != commands_.end()) {
                    client::Client* client = core_->get_client();
                    it->second->execute(client, warp_args);
                }
            } else {
                utils::PacketUtils::send_chat_message(player, "`4Please enter a world name first!");
            }
        }
        else if (button_clicked == "name_change") {
            send_name_change_dialog(player);
        }
        else if (button_clicked == "title") {
            send_gui_to_player(player, "title");
        }
        else if (button_clicked == "drop") {
            send_drop_dialog(player);
        }
        else if (button_clicked == "proxy") {
            send_gui_to_player(player, "proxy");
        }
        else if (button_clicked == "info") {
            send_gui_to_player(player, "info");
        }
    }

    void handle_proxy_gui_response(player::Player* player, const std::string& button_clicked) {
        if (button_clicked == "warp_gui") {
            send_gui_to_player(player, "main");
        }
        else if (button_clicked == "name_gui") {
            send_name_change_dialog(player);
        }
        else if (button_clicked == "title_gui") {
            send_gui_to_player(player, "title");
        }
        else if (button_clicked == "drop_gui") {
            send_drop_dialog(player);
        }
        else if (button_clicked == "info_gui") {
            send_gui_to_player(player, "info");
        }
    }

    void handle_info_gui_response(player::Player* player, const std::string& button_clicked) {
        if (button_clicked == "back") {
            send_gui_to_player(player, "proxy");
        }
    }

    void handle_name_gui_response(player::Player* player, const std::string& button_clicked, TextParse& text_parse) {
        if (button_clicked == "change_name") {
            std::string new_name = text_parse.get("new_name");
            if (!new_name.empty()) {
                std::vector<std::string> name_args{new_name};
                if (auto it = commands_.find("name"); it != commands_.end()) {
                    client::Client* client = core_->get_client();
                    it->second->execute(client, name_args);
                }
            } else {
                utils::PacketUtils::send_chat_message(player, "`4Please enter a name first!");
            }
        }
    }

    void handle_title_gui_response(player::Player* player, const std::string& button_clicked) {
        if (!player) return;

        spdlog::info("Title GUI button clicked: '{}'", button_clicked);

        try {
            client::Client* client = core_->get_client();
            std::vector<std::string> empty_args;
            
            if (button_clicked == "g4g") {
                spdlog::info("Applying G4G title via GUI");
                if (auto it = commands_.find("g4g"); it != commands_.end()) {
                    if (auto* g4g_cmd = dynamic_cast<command::G4GCommand*>(it->second.get())) {
                        g4g_cmd->execute_with_core(client, empty_args, core_);
                    } else {
                        spdlog::error("G4G command found but cast failed");
                    }
                } else {
                    spdlog::error("G4G command not found in commands map");
                }
            }
            else if (button_clicked == "maxlv") {
                spdlog::info("Applying MaxLevel title via GUI");
                if (auto it = commands_.find("maxlevel"); it != commands_.end()) {
                    if (auto* maxlv_cmd = dynamic_cast<command::MaxLevelCommand*>(it->second.get())) {
                        maxlv_cmd->execute_with_core(client, empty_args, core_);
                    }
                }
            }
            else if (button_clicked == "dr") {
                spdlog::info("Applying Dr. title via GUI");
                if (auto it = commands_.find("dr"); it != commands_.end()) {
                    if (auto* dr_cmd = dynamic_cast<command::DrCommand*>(it->second.get())) {
                        dr_cmd->execute_with_core(client, empty_args, core_);
                    }
                }
            }
            else if (button_clicked == "mentor") {
                spdlog::info("Applying Mentor title via GUI");
                if (auto it = commands_.find("mentor"); it != commands_.end()) {
                    if (auto* mentor_cmd = dynamic_cast<command::MentorCommand*>(it->second.get())) {
                        mentor_cmd->execute_with_core(client, empty_args, core_);
                    }
                }
            }
            else if (button_clicked == "cleartitle") {
                spdlog::info("Clearing titles via GUI");
                if (auto it = commands_.find("cleartitle"); it != commands_.end()) {
                    if (auto* clear_cmd = dynamic_cast<command::ClearTitleCommand*>(it->second.get())) {
                        clear_cmd->execute_with_core(client, empty_args, core_);
                    }
                }
            }
            else {
                spdlog::warn("Unknown button clicked in title GUI: '{}'", button_clicked);
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Error applying title: {}", e.what());
            utils::PacketUtils::send_chat_message(player, "`4Error applying title!");
        }
    }

    void handle_drop_gui_response(player::Player* player, const std::string& button_clicked, TextParse& text_parse) {
    if (button_clicked == "drop_item") {
        std::string item_id_str = text_parse.get("item_id");
        std::string count_str = text_parse.get("item_count");
        
        if (!item_id_str.empty() && !count_str.empty()) {
            try {
                uint32_t item_id = std::stoul(item_id_str);
                uint32_t count = std::stoul(count_str);
                
                
                if (item_id > 0 && count > 0 && count <= 200) {
                    
                    TextParse drop_parse{};
                    drop_parse.add("action", {"dialog_return"});
                    drop_parse.add("dialog_name", {"drop_item"});
                    drop_parse.add("itemID", {item_id_str});
                    drop_parse.add("count", {count_str});

                    ByteStream<std::uint16_t> byte_stream{};
                    byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
                    byte_stream.write(drop_parse.get_raw(), false);

                    player->send_packet(byte_stream.get_data(), 0);
                    
                    utils::PacketUtils::send_chat_message(player, 
                        fmt::format("`2Dropped {} of item ID: {}", count, item_id));
                    
                    spdlog::info("Fast drop from GUI - Item: {}, Count: {}", item_id, count);
                } else {
                    utils::PacketUtils::send_chat_message(player, 
                        "`4Invalid item ID or count (1-200 only)");
                }
            } catch (const std::exception& e) {
                utils::PacketUtils::send_chat_message(player, 
                    "`4Invalid numbers entered");
            }
        } else {
            utils::PacketUtils::send_chat_message(player, 
                "`4Please enter both item ID and count");
        }
    }
}

    void handle_growscan_response(player::Player* player, const std::string& button_clicked, const std::string& dialog_name) {
        if (!player) return;

        spdlog::info("GrowScan button clicked: '{}' in dialog '{}'", button_clicked, dialog_name);

        try {
            auto* server = core_->get_server();
            if (!server || !server->get_player()) {
                spdlog::error("GrowScan: No server available!");
                return;
            }

            
            if (auto it = commands_.find("growscan"); it != commands_.end()) {
                if (auto* gs_cmd = dynamic_cast<command::GrowScanCommand*>(it->second.get())) {
                    gs_cmd->handle_button_click(server->get_player(), button_clicked, core_);
                }
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Error in GrowScan: {}", e.what());
            auto* server = core_->get_server();
            if (server && server->get_player()) {
                utils::PacketUtils::send_chat_message(server->get_player(), "`4Error in GrowScan!");
            }
        }
    }

    void handle_gsbeta_response(player::Player* player, const std::string& button_clicked, const std::string& dialog_name) {
        if (!player) return;

        spdlog::info("GsBeta button clicked: '{}' in dialog '{}'", button_clicked, dialog_name);

        try {
            auto* server = core_->get_server();
            if (!server || !server->get_player()) {
                spdlog::error("GsBeta: No server available!");
                return;
            }

            if (auto it = commands_.find("gsbeta"); it != commands_.end()) {
                if (auto* cmd = dynamic_cast<command::GsBetaCommand*>(it->second.get())) {
                    cmd->handle_button_click(server->get_player(), button_clicked);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error in GsBeta: {}", e.what());
        }
    }

    void send_name_change_dialog(player::Player* player) {
        if (!player) return;

        try {
            std::string name_dialog = 
                "set_default_color|`o\n"
                "add_label_with_icon|big|`wChange Name``|left|758\n"
                "add_spacer|small\n"
                "add_textbox|Enter your new display name below:|left\n"
                "add_spacer|small\n"
                "add_text_input|new_name|New Name:| |20\n"
                "add_spacer|small\n"
                "add_button|change_name|`5Change Name``\n"
                "add_quick_exit\n"
                "end_dialog|name_change|Cancel|Okay";

            packet::Variant variant{};
            variant.add("OnDialogRequest");
            variant.add(name_dialog);
            
            std::vector<std::byte> ext_data = variant.serialize();

            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = -1;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());

            player->send_packet(byte_stream.get_data(), 0);
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to send name change dialog: {}", e.what());
        }
    }

    void send_drop_dialog(player::Player* player) {
        if (!player) return;

        try {
            std::string drop_dialog = 
                "set_default_color|`o\n"
                "add_label_with_icon|big|`wDrop Items``|left|758\n"
                "add_spacer|small\n"
                "add_textbox|Enter item details to drop:|left\n"
                "add_spacer|small\n"
                "add_text_input|item_id|Item ID:| |10\n"
                "add_text_input|item_count|Count:|1|5\n"
                "add_spacer|small\n"
                "add_textbox|Common Item IDs:|left\n"
                "add_textbox|`298`o - Diamond Lock|left\n"
                "add_textbox|`218`o - World Lock|left\n"
                "add_textbox|`224`o - Blue Gem|left\n"
                "add_textbox|`220`o - Purple Gem|left\n"
                "add_textbox|`210`o - Golden Boots|left\n"
                "add_spacer|small\n"
                "add_button|drop_item|`2Drop Item``\n"
                "add_quick_exit\n"
                "end_dialog|drop_gui|Cancel|Okay";

            packet::Variant variant{};
            variant.add("OnDialogRequest");
            variant.add(drop_dialog);
            
            std::vector<std::byte> ext_data = variant.serialize();

            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = -1;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());

            player->send_packet(byte_stream.get_data(), 0);
            
            spdlog::info("Drop dialog displayed for player");

        } catch (const std::exception& e) {
            spdlog::error("Failed to send drop dialog: {}", e.what());
        }
    }

    void apply_g4g_title(player::Player* player, uint32_t netid) {
        packet::Variant var1{};
        var1.add("OnNameChanged");
        var1.add("");
        
        std::vector<std::byte> ext_data1 = var1.serialize();
        packet::GameUpdatePacket pkt1{};
        pkt1.type = packet::PACKET_CALL_FUNCTION;
        pkt1.net_id = netid;
        pkt1.flags.extended = 1;
        pkt1.data_size = static_cast<uint32_t>(ext_data1.size());
        
        ByteStream<std::uint16_t> bs1{};
        bs1.write(packet::NET_MESSAGE_GAME_PACKET);
        bs1.write(pkt1);
        bs1.write_data(ext_data1.data(), ext_data1.size());
        player->send_packet(bs1.get_data(), 0);

        packet::Variant var2{};
        var2.add("OnCountryState");
        var2.add("id|donor");
        
        std::vector<std::byte> ext_data2 = var2.serialize();
        packet::GameUpdatePacket pkt2{};
        pkt2.type = packet::PACKET_CALL_FUNCTION;
        pkt2.net_id = netid;
        pkt2.flags.extended = 1;
        pkt2.data_size = static_cast<uint32_t>(ext_data2.size());
        
        ByteStream<std::uint16_t> bs2{};
        bs2.write(packet::NET_MESSAGE_GAME_PACKET);
        bs2.write(pkt2);
        bs2.write_data(ext_data2.data(), ext_data2.size());
        player->send_packet(bs2.get_data(), 0);

        spdlog::info("Applied G4G title via GUI");
    }

    void apply_maxlv_title(player::Player* player, uint32_t netid) {
        packet::Variant var1{};
        var1.add("OnNameChanged");
        var1.add("");
        
        std::vector<std::byte> ext_data1 = var1.serialize();
        packet::GameUpdatePacket pkt1{};
        pkt1.type = packet::PACKET_CALL_FUNCTION;
        pkt1.net_id = netid;
        pkt1.flags.extended = 1;
        pkt1.data_size = static_cast<uint32_t>(ext_data1.size());
        
        ByteStream<std::uint16_t> bs1{};
        bs1.write(packet::NET_MESSAGE_GAME_PACKET);
        bs1.write(pkt1);
        bs1.write_data(ext_data1.data(), ext_data1.size());
        player->send_packet(bs1.get_data(), 0);

        packet::Variant var2{};
        var2.add("OnCountryState");
        var2.add("id|maxLevel");
        
        std::vector<std::byte> ext_data2 = var2.serialize();
        packet::GameUpdatePacket pkt2{};
        pkt2.type = packet::PACKET_CALL_FUNCTION;
        pkt2.net_id = netid;
        pkt2.flags.extended = 1;
        pkt2.data_size = static_cast<uint32_t>(ext_data2.size());
        
        ByteStream<std::uint16_t> bs2{};
        bs2.write(packet::NET_MESSAGE_GAME_PACKET);
        bs2.write(pkt2);
        bs2.write_data(ext_data2.data(), ext_data2.size());
        player->send_packet(bs2.get_data(), 0);

        spdlog::info("Applied Max Level title via GUI");
    }

    void apply_dr_title(player::Player* player, uint32_t netid) {
        std::string player_name = "Player";
        
        packet::Variant var1{};
        var1.add("OnNameChanged");
        var1.add("Dr." + player_name);
        
        std::vector<std::byte> ext_data1 = var1.serialize();
        packet::GameUpdatePacket pkt1{};
        pkt1.type = packet::PACKET_CALL_FUNCTION;
        pkt1.net_id = netid;
        pkt1.flags.extended = 1;
        pkt1.data_size = static_cast<uint32_t>(ext_data1.size());
        
        ByteStream<std::uint16_t> bs1{};
        bs1.write(packet::NET_MESSAGE_GAME_PACKET);
        bs1.write(pkt1);
        bs1.write_data(ext_data1.data(), ext_data1.size());
        player->send_packet(bs1.get_data(), 0);

        packet::Variant var2{};
        var2.add("OnCountryState");
        var2.add("id|doctor");
        
        std::vector<std::byte> ext_data2 = var2.serialize();
        packet::GameUpdatePacket pkt2{};
        pkt2.type = packet::PACKET_CALL_FUNCTION;
        pkt2.net_id = netid;
        pkt2.flags.extended = 1;
        pkt2.data_size = static_cast<uint32_t>(ext_data2.size());
        
        ByteStream<std::uint16_t> bs2{};
        bs2.write(packet::NET_MESSAGE_GAME_PACKET);
        bs2.write(pkt2);
        bs2.write_data(ext_data2.data(), ext_data2.size());
        player->send_packet(bs2.get_data(), 0);

        spdlog::info("Applied Dr. title via GUI");
    }

    std::vector<std::string> parse_command(const std::string& message) {
        std::vector<std::string> args;
        std::string current_arg;
        bool in_quotes = false;
        
        for (char c : message) {
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ' ' && !in_quotes) {
                if (!current_arg.empty()) {
                    args.push_back(trim(current_arg));  
                    current_arg.clear();
                }
            } else {
                current_arg += c;
            }
        }
        
        if (!current_arg.empty()) {
            args.push_back(trim(current_arg));  
        }
        
        return args;
    }
    
    
    std::string clean_string(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        
        for (char c : str) {
            
            if ((c >= 32 && c <= 126) || c == '\t' || c == '\n' || c == '\r') {
                result += c;
            }
            
            else if (c == '\0') {
                break;
            }
            
        }
        
        return result;
    }
    
    
    std::string trim(const std::string& str) {
        
        std::string cleaned = clean_string(str);
        
        size_t first = cleaned.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = cleaned.find_last_not_of(" \t\n\r");
        return cleaned.substr(first, (last - first + 1));
    }
};

} 
