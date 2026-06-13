#pragma once




#include <string>
#include <functional>

namespace extension { namespace item_finder { class ItemDatabase; } }



void RunImGuiApp();



void SetLuaRunner(std::function<bool(const std::string&, std::string&)> fn);

void SetCommandRunner(std::function<void(const std::string&)> fn);




void SetConnectionCallbacks(
    std::function<void()>   connect_fn,
    std::function<void()>   disconnect_fn,
    std::function<bool()>   is_connected_fn);



void SetProxyConfigCallbacks(
    std::function<std::string(const std::string&)>        get_fn,
    std::function<void(const std::string&, const std::string&)> set_fn);



void AppendLog(const std::string& line);


void AppendPacket(const std::string& direction, const std::string& data);


void SetItemDatabase(class extension::item_finder::ItemDatabase* db);
