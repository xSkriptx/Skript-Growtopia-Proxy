# C++ High-Performance Network Interceptor & Lua Scripting Proxy

A modular, multi-threaded C++ network interception proxy and scripting sandbox. The application intercepts local game client connection requests via transparent SSL/DNS redirection, intercepts raw network packets, and exposes system hooks to an embedded **LuaJIT** scripting engine.

---

> [!IMPORTANT]
> ### 🛑 Educational Purposes Only
> This software is developed solely as an educational research prototype in network security, protocol reverse-engineering, and embedded systems design. It is designed to demonstrate local packet filtering, Win32 desktop overlays, and scripting integrations. 
> 
> **It is not intended to be used for unauthorized game automation, cheating, or exploiting on public servers.** The author does not condone, support, or assume any responsibility for misuse of this tool in online games.

---

## Key Features

- **Transparent SSL/DNS Redirection**: Intercepts HTTPS server list lookups (`server_data.php`) and dynamically routes traffic through a local custom proxy.
- **Asynchronous Protocol Engine**: Operates on a multi-threaded event loop, processing client and server game packet queues via **ENet**.
- **Embedded LuaJIT Core**: Embeds a high-performance Lua scripting compiler to inject packets, query client state, and execute real-time automation.
- **ImGui Desktop UI**: Implements a graphical control interface using **Dear ImGui** and DirectX 9/Win32.
- **Offline & Privacy Preserving**: Bypasses cloud licensing, remote database logging, and telemetry server connections. All logs, vending scans, and Locke trackers run 100% locally.

---

## Tech Stack & Architecture

- **Core**: C++23 (MSVC)
- **Dependency Management**: Conan Package Manager
- **Build System**: CMake (minimum version 3.24)
- **Network Stack**: ENet (UDP connection loop), cpp-httplib (HTTP/HTTPS tunnels)
- **Scripting Engine**: LuaJIT 2.1
- **UI Engine**: Dear ImGui (DirectX 9 / Win32 integration)
- **Logging & Events**: spdlog, eventpp dispatcher

---

## Getting Started

### Prerequisites
- Windows OS
- CMake (version 3.24 or later)
- Visual Studio 2022 (MSVC compiler with C++23 support)
- Conan Package Manager installed and configured in PATH

### Build Instructions

1. Configure the project using CMake:
   ```powershell
   cmake -B build
   ```
   *Note: CMake will automatically call Conan to retrieve all external library dependencies (fmt, glm, spdlog, magic_enum, luajit).*

2. Build the executable in Release mode:
   ```powershell
   cmake --build build --config Release
   ```

3. Run the compiled binary:
   ```powershell
   .\build\src\Release\Skript.exe
   ```

---

## Lua Scripting API Documentation

The embedded scripting system runs `.lua` files placed in the `scripts/` folder. It exposes the following native API to automate player actions, analyze maps, and inject network packets:

### Native Globals

#### 1. System & Threads
- `log(text)`: Appends a text string to the local debug log file.
- `Sleep(ms)`: Yields thread execution for the specified milliseconds.
- `RunThread(function)`: Spawns and executes a Lua function in a separate, detached system thread.

#### 2. Network Transmission (Outbound to Server)
- `SendPacket(type, raw_data)`: Sends a generic text/game message packet to the server (type `2` or `3`).
- `SendPacketRaw(gup_table)`: Injects a raw `GameUpdatePacket` structure to the server.
- `SendChat(text)`: Simulates sending chat input. Checks event dispatcher lists and submits the message.
- `Warp(world_name)`: Submits a join/warp packet to connect to the target world.
- `Collect(item_id)`: Sends item sweep-collect packets for the target item ID, brute-forcing dropped UIDs locally.

#### 3. Network Transmission (Inbound to Client)
- `SendPacketToClient(type, raw_data)`: Routes a generic text/game message packet to the local client.
- `SendPacketRawClient(gup_table)`: Routes a raw `GameUpdatePacket` structure to the local client.
- `SendVarlist(varlist_table)`: Sends a serialized variant list (such as `OnDialogRequest`) directly to the client.
- `MessageBox(title, content)`: Displays a custom visual popup dialog box inside the client window.

#### 4. Client State & Inventory
- `GetLocal()`: Returns a table representing the local player state.
  ```lua
  local player = GetLocal()
  print("Name: " .. player.name)
  print("Position: (" .. player.tile_x .. ", " .. player.tile_y .. ")")
  ```
- `GetPlayers()`: Returns an indexed list of all active players in the current world.
- `GetInventory()`: Returns an array snapshot of inventory slots (`{id, count}`).
- `GetItemCount(item_id)`: Queries the quantity of a specific item in the local inventory.

#### 5. Map & Tile Analysis
- `GetWorldName()`: Returns the name of the current world.
- `GetObjects()`: Returns all dropped items currently located on the map.
- `GetTile(x, y)`: Returns metadata representing the tile block at map coordinate `(x, y)` (`{fg, bg, pos_x, pos_y, ready, water, fire}`).
- `GetTiles()`: Returns an array of all map tile blocks.
- `FindPath(x, y)`: Calculates pathfinding routes and walks the player to coordinate `(x, y)`.

#### 6. Networking Callback Event Hooks
- `AddCallback(name, event, function)`: Registers an event listener function.
- `RemoveCallback(name)`: Unregisters a callback by name.
- `RemoveCallbacks()`: Unregisters all callback handlers.

##### Supported Events:
- `"OnVarlist"`: Triggers when the server submits a variant list (e.g. `OnDialogRequest`, `OnConsoleMessage`).
- `"OnPacket"`: Triggers when the client submits an outgoing generic text packet.
- `"OnRawPacket"`: Triggers on outgoing raw `GameUpdatePacket` items from the client.
- `"OnIncomingRawPacket"`: Triggers on incoming raw `GameUpdatePacket` items from the server.

---

### Lua Script Examples

#### 1. Basic Outbound Packet Injection
```lua
-- Warp to a world and log connection
RunThread(function()
    log("Warping to world START...")
    Warp("START")
    Sleep(3000)
    SendChat("Hello from Lua!")
end)
```

#### 2. Event Interception Callback
```lua
-- Block specified incoming chat packets
AddCallback("my_filter", "OnVarlist", function(varlist, netid)
    if varlist[0] == "OnTalkBubble" then
        local sender_id = varlist[1]
        local message = varlist[2]
        log("TalkBubble from netid " .. sender_id .. ": " .. message)
        
        if string.find(message, "CSN") then
            log("Blocked chat event message containing keyword")
            return true -- Return true to cancel packet propagation
        end
    end
    return false
end)
```
