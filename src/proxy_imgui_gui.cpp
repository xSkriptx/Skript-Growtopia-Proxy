



#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"
#include "../third_party/imgui/backends/imgui_impl_win32.h"
#include "../third_party/imgui/backends/imgui_impl_dx9.h"

#include "extension/command_handler/dropfast_command.hpp"
#include "extension/command_handler/trashfast_command.hpp"
#include "extension/command_handler/vendfast_command.hpp"
#include "extension/command_handler/join_command.hpp"
#include "extension/command_handler/moddetect_command.hpp"   
#include "extension/command_handler/autocollect_command.hpp"
#include "utils/lua_manager.hpp"
#include "utils/lua_bridge.hpp"
#include "utils/world_manager.hpp"
#include "utils/player_tracker.hpp"
#include "extension/item_finder/item_finder.hpp"  

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <cmath>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <fstream> 
#include <string_view>

#pragma comment(lib, "d3d9.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);


bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


static LPDIRECT3D9           g_pD3D       = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice = nullptr;
static D3DPRESENT_PARAMETERS g_d3dpp      = {};
static HWND                  g_hWnd       = nullptr;
static const int             g_hdrH       = 24;   


struct LogEntry {
    std::string text;
    ImVec4      color;
};
static std::deque<LogEntry> g_log;


static char g_gui_path_x_str[16] = "0";
static char g_gui_path_y_str[16] = "0";
static std::mutex           g_log_mtx;
static const size_t         LOG_MAX = 2000;


struct PacketEntry {
    std::string direction; 
    std::string data;
    std::string timestamp;
};
static std::deque<PacketEntry> g_packets;
static std::mutex              g_pkt_mtx;
static const size_t            PKT_MAX = 500;
static bool                    g_pkt_enabled = true;


static std::function<bool(const std::string&, std::string&)> g_lua_runner;
static std::function<void(const std::string&)>               g_cmd_runner;
static std::function<void()>                                 g_connect_fn;
static std::function<void()>                                 g_disconnect_fn;
static std::function<bool()>                                 g_is_connected_fn;
static std::function<std::string(const std::string&)>        g_proxy_get_fn;
static std::function<void(const std::string&,const std::string&)> g_proxy_set_fn;


static char        g_lua_buf[8192] =
    "-- Skript Proxy | Executor\n";
static std::string g_lua_output;
static bool        g_lua_scroll_bottom = false;
static float       g_lua_editor_scroll_y = 0.0f;

static int CountLines(const char* s) {
    if (!s || !*s) return 1;
    int lines = 1;
    for (const char* p = s; *p; ++p) {
        if (*p == '\n') ++lines;
    }
    return lines;
}

static bool ContainsIcase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}



static std::string StripGTColorCodes(const std::string& in) {
    if (in.empty()) {
        return {};
    }

    std::string out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '`') {
            
            if (i + 1 < in.size()) {
                ++i;
            }
            continue;
        }
        out.push_back(c);
    }

    return out;
}


void AppendLog(const std::string& line);
static void ShowPlayersDialog();




static void ShowPlayersDialog() {
    auto all = utils::PlayerTracker::get_instance().get_all_players();
    std::string content;
    for (auto& kv : all) {
        if (!content.empty()) content += "\\n"; 
        content += StripGTColorCodes(kv.second.name);
    }
    if (content.empty()) content = "(no players)";

    
    std::string lua_content;
    for (char c : content) {
        if (c == '\\' || c == '"') lua_content += '\\';
        lua_content += c;
    }

    std::string lua_code = "MessageBox(\"Players in World:\", \"" + lua_content + "\")";
    if (g_lua_runner) {
        std::string err;
        g_lua_runner(lua_code, err);
        if (!err.empty()) {
            AppendLog(std::string("[GUI] Lua error: ") + err);
        }
    }
}


static char g_join_world[64] = "START";


static bool g_collapsed    = false;
static RECT g_saved_rect   = {};

static bool g_show_proxy = false;
static std::vector<std::string> g_proxy_lines;

static void LoadProxyDialog() {
    g_proxy_lines.clear();
    std::ifstream f("C:\\Users\\11User\\proxyupdates\\realproxy\\Marv Dialog.txt");
    if (!f.is_open()) return;
    std::string line;
    int count = 0;
    while (std::getline(f, line) && count < 40) {
        g_proxy_lines.push_back(line);
        if (line.empty()) break; 
        count++;
    }
    g_show_proxy = true;
}




void SetLuaRunner(std::function<bool(const std::string&, std::string&)> fn) { g_lua_runner = fn; }
void SetCommandRunner(std::function<void(const std::string&)> fn) {
    
    g_cmd_runner = [fn](const std::string& cmd){
        if (cmd.rfind("/proxy", 0) == 0) {
            LoadProxyDialog();
        }
        fn(cmd);
    };
}

void SetConnectionCallbacks(
    std::function<void()> connect_fn,
    std::function<void()> disconnect_fn,
    std::function<bool()> is_connected_fn)
{
    g_connect_fn      = connect_fn;
    g_disconnect_fn   = disconnect_fn;
    g_is_connected_fn = is_connected_fn;
}

void SetProxyConfigCallbacks(
    std::function<std::string(const std::string&)>             get_fn,
    std::function<void(const std::string&, const std::string&)> set_fn)
{
    g_proxy_get_fn = get_fn;
    g_proxy_set_fn = set_fn;
}

void AppendLog(const std::string& line) {
    ImVec4 col = ImVec4(0.80f, 0.90f, 1.00f, 1.0f); 
    if (line.find("[ERR]")  != std::string::npos ||
        line.find("error")  != std::string::npos ||
        line.find("Error")  != std::string::npos)
        col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);      
    else if (line.find("[OK]")   != std::string::npos ||
             line.find("ENABL")  != std::string::npos)
        col = ImVec4(0.30f, 0.95f, 0.50f, 1.0f);     
    else if (line.find("[WARN]") != std::string::npos ||
             line.find("warn")   != std::string::npos)
        col = ImVec4(1.0f, 0.75f, 0.20f, 1.0f);      
    else if (line.find("[LUA]")  != std::string::npos)
        col = ImVec4(0.60f, 0.85f, 1.00f, 1.0f);     

    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (g_log.size() >= LOG_MAX) g_log.pop_front();
    g_log.push_back({ line, col });
}

void AppendPacket(const std::string& direction, const std::string& data) {
    if (!g_pkt_enabled) return;
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char ts[16];
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm_buf);

    {
        std::lock_guard<std::mutex> lk(g_pkt_mtx);
        if (g_packets.size() >= PKT_MAX) g_packets.pop_front();
        g_packets.push_back({ direction, data, ts });
    }

    
    if (data.find("[TEXT]") != std::string::npos ||
        data.find("CALL_FUNCTION") != std::string::npos) {
        bool cs = (direction.size() > 0 && direction[0] == 'C');
        ImVec4 col = cs
            ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f)
            : ImVec4(0.55f, 0.75f, 1.00f, 1.0f);
        std::lock_guard<std::mutex> lk(g_log_mtx);
        if (g_log.size() < LOG_MAX)
            g_log.push_back({ "[PKT] " + direction + "  " + data, col });
    }
}

void SetWorldName(const std::string& name) {  (void)name; }
void SetPlayerName(const std::string& name) { (void)name; }





static void ApplySkriptTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 5.0f;
    s.GrabRounding      = 4.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 5.0f;
    s.TabRounding       = 5.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.ItemSpacing       = ImVec2(8, 6);
    s.FramePadding      = ImVec2(8, 5);
    s.WindowPadding     = ImVec2(12, 10);

    auto* c = s.Colors;
    
    c[ImGuiCol_WindowBg]          = ImVec4(0.037f, 0.051f, 0.118f, 0.97f); 
    c[ImGuiCol_ChildBg]           = ImVec4(0.050f, 0.070f, 0.150f, 0.90f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.055f, 0.075f, 0.160f, 0.96f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.025f, 0.037f, 0.090f, 0.95f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.060f, 0.110f, 0.240f, 0.90f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.080f, 0.150f, 0.320f, 0.95f);
    
    c[ImGuiCol_Tab]               = ImVec4(0.055f, 0.100f, 0.240f, 0.85f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.120f, 0.220f, 0.520f, 0.90f);
    c[ImGuiCol_TabActive]         = ImVec4(0.100f, 0.200f, 0.550f, 1.00f);
    c[ImGuiCol_TabUnfocused]      = ImVec4(0.040f, 0.080f, 0.190f, 0.80f);
    c[ImGuiCol_TabUnfocusedActive]= ImVec4(0.070f, 0.140f, 0.360f, 0.90f);
    
    c[ImGuiCol_Header]            = ImVec4(0.090f, 0.180f, 0.430f, 0.80f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.130f, 0.270f, 0.560f, 0.90f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.150f, 0.310f, 0.650f, 1.00f);
    
    c[ImGuiCol_Button]            = ImVec4(0.090f, 0.190f, 0.470f, 0.90f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.140f, 0.290f, 0.640f, 1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.070f, 0.140f, 0.360f, 1.00f);
    
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.020f, 0.030f, 0.080f, 0.80f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.100f, 0.210f, 0.500f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.150f,0.300f, 0.640f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]=ImVec4(0.180f, 0.360f, 0.750f, 1.00f);
    
    c[ImGuiCol_Separator]         = ImVec4(0.090f, 0.190f, 0.450f, 0.70f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.140f, 0.290f, 0.640f, 0.90f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.180f, 0.360f, 0.750f, 1.00f);
    c[ImGuiCol_Border]            = ImVec4(0.090f, 0.180f, 0.430f, 0.60f);
    
    c[ImGuiCol_TitleBg]           = ImVec4(0.030f, 0.050f, 0.130f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.060f, 0.110f, 0.270f, 1.00f);
    
    c[ImGuiCol_Text]              = ImVec4(0.82f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.40f, 0.55f, 0.75f, 1.00f);
    
    c[ImGuiCol_CheckMark]         = ImVec4(0.25f, 0.85f, 0.45f, 1.00f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.15f, 0.40f, 0.85f, 0.90f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.20f, 0.55f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.10f, 0.25f, 0.60f, 0.50f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.15f, 0.38f, 0.80f, 0.80f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.20f, 0.50f, 1.00f, 1.00f);
    
    c[ImGuiCol_PlotLines]         = ImVec4(0.35f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogram]     = ImVec4(0.20f, 0.55f, 1.00f, 0.90f);
}


static void Badge(const char* text, ImVec4 col) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}


static bool AccentButton(const char* label, ImVec2 sz = ImVec2(0,0)) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.09f,0.20f,0.50f,0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f,0.32f,0.72f,1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f,0.13f,0.35f,1.00f));
    bool r = ImGui::Button(label, sz);
    ImGui::PopStyleColor(3);
    return r;
}


static bool DangerButton(const char* label, ImVec2 sz = ImVec2(0,0)) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f,0.07f,0.07f,0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f,0.12f,0.12f,1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f,0.04f,0.04f,1.00f));
    bool r = ImGui::Button(label, sz);
    ImGui::PopStyleColor(3);
    return r;
}


static void ToggleRow(const char* label, const char* desc, bool* val,
                      std::function<void()> on_change) {
    ImGui::PushID(label);
    bool v = *val;
    if (v) ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.25f,0.92f,0.45f,1.0f));
    else   ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.55f,0.55f,0.55f,1.0f));

    if (ImGui::Checkbox(label, &v)) {
        *val = v;
        on_change();
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.70f,0.90f,1.0f));
    ImGui::TextUnformatted(desc);
    ImGui::PopStyleColor();
    ImGui::PopID();
}




struct Particle { float angle, radius, speed, phase; };
static std::vector<Particle> g_particles;

static void DrawBackground(ImDrawList* dl, ImVec2 pos, ImVec2 sz, float t) {
    if (g_particles.empty()) {
        g_particles.reserve(35);
        srand(42);
        for (int i = 0; i < 35; i++)
            g_particles.push_back({
                i * 0.52f,
                140.f + (rand()%90),
                0.3f + 0.4f*(rand()/float(RAND_MAX)),
                rand()/float(RAND_MAX)
            });
    }
    ImVec2 ctr = { pos.x + sz.x*0.5f, pos.y + sz.y*0.5f };
    for (auto& p : g_particles) {
        float a  = p.angle + t * p.speed;
        float px = ctr.x + cosf(a + p.phase) * (p.radius + 22*fabsf(sinf(t*0.28f + p.phase)));
        float py = ctr.y + sinf(a - p.phase) * (p.radius*0.65f + 16*fabsf(cosf(t*0.35f - p.phase)));
        float r  = 10.f + 5.f*sinf(t*1.4f + p.phase);
        float br = 0.18f + 0.14f*sinf(a);
        float bg = 0.42f + 0.28f*cosf(a + 1.2f);
        dl->AddCircleFilled({px,py}, r,
            IM_COL32((int)(br*255), (int)(bg*255), 255, 55), 20);
    }
}




static void TabDebugger() {
    
    if (AccentButton("Clear##log")) {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        g_log.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu lines)", g_log.size());


    ImGui::Spacing();

    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
    if (ImGui::BeginChild("##log_child", ImVec2(0,0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        ImGuiListClipper clip;
        clip.Begin((int)g_log.size());
        while (clip.Step()) {
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                ImGui::PushStyleColor(ImGuiCol_Text, g_log[i].color);
                ImGui::TextUnformatted(g_log[i].text.c_str());
                ImGui::PopStyleColor();
            }
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}




static void TabLua() {
    
    if (AccentButton("  ▶  Run  ")) {
        std::string err;
        bool ok = false;
        if (g_lua_runner) {
            ok = g_lua_runner(g_lua_buf, err);
        } else {
            auto* bridge = lua::LuaManager::get();
            if (bridge) {
                err = bridge->execute_with_error(g_lua_buf);
                ok  = err.empty();
            } else {
                err = "Lua not initialised";
            }
        }
        
        int len = (int)strlen(g_lua_buf);
        g_lua_output += "── run (" + std::to_string(len) + " bytes) ──────────────────\n";
        if (ok)  g_lua_output += "[OK]  Script executed successfully\n";
        else     g_lua_output += "[ERR] " + err + "\n";
        g_lua_scroll_bottom = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Code"))   { memset(g_lua_buf, 0, sizeof(g_lua_buf)); }
    ImGui::SameLine();
    if (ImGui::Button("Clear Output")) { g_lua_output.clear(); }
    ImGui::SameLine();
    ImGui::TextDisabled("  Ctrl+Enter = run");

    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().y;
    float edH   = avail * 0.52f;
    float outH  = avail - edH - 30.f;

    
    {
        const int line_count = CountLines(g_lua_buf);
        int digits = 1;
        for (int v = line_count; v >= 10; v /= 10) ++digits;

        const float digit_w = ImGui::CalcTextSize("0").x;
        const float gutter_w = std::max(28.0f, digit_w * digits + 14.0f);

        if (ImGui::BeginTable("##lua_editor_tbl", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##ln", ImGuiTableColumnFlags_WidthFixed, gutter_w);
            ImGui::TableSetupColumn("##code", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.01f, 0.02f, 0.06f, 0.97f));
            if (ImGui::BeginChild("##lua_linenos", ImVec2(0, edH), true,
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::SetScrollY(g_lua_editor_scroll_y);
                for (int i = 1; i <= line_count; ++i) {
                    ImGui::TextDisabled("%*d", digits, i);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.03f, 0.08f, 0.97f));
            bool edited = ImGui::InputTextMultiline("##lua_code", g_lua_buf, sizeof(g_lua_buf),
                ImVec2(-1.f, edH),
                ImGuiInputTextFlags_AllowTabInput);
            (void)edited;
            ImGui::PopStyleColor();

            if (ImGuiInputTextState* st = ImGui::GetInputTextState(ImGui::GetID("##lua_code"))) {
                g_lua_editor_scroll_y = st->Scroll.y;
            }

            ImGui::EndTable();
        }
    }

    
    if (ImGui::IsItemFocused() &&
        ImGui::IsKeyDown(ImGuiKey_LeftCtrl) &&
        ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
        std::string err;
        bool ok = false;
        if (g_lua_runner) {
            ok = g_lua_runner(g_lua_buf, err);
        } else {
            auto* bridge = lua::LuaManager::get();
            if (bridge) { err = bridge->execute_with_error(g_lua_buf); ok = err.empty(); }
        }
        g_lua_output += ok ? "[OK]  Done\n" : "[ERR] " + err + "\n";
        g_lua_scroll_bottom = true;
    }

    
    ImGui::Spacing();
    ImGui::TextDisabled("Output:");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.03f, 0.07f, 0.95f));
    if (ImGui::BeginChild("##lua_out", ImVec2(-1.f, outH), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        
        const char* p = g_lua_output.c_str();
        while (*p) {
            const char* nl = strchr(p, '\n');
            int len2 = nl ? (int)(nl - p) : (int)strlen(p);
            std::string line(p, len2);

            ImVec4 col = ImVec4(0.80f, 0.90f, 1.00f, 1.0f);
            if (line.rfind("[ERR]", 0) == 0)   col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            else if (line.rfind("[OK]",  0) == 0) col = ImVec4(0.30f, 0.95f, 0.50f, 1.0f);
            else if (line.rfind("── ",  0) == 0) col = ImVec4(0.40f, 0.55f, 0.75f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();

            p = nl ? nl + 1 : p + len2;
            if (!nl) break;
        }
        if (g_lua_scroll_bottom) {
            ImGui::SetScrollHereY(1.0f);
            g_lua_scroll_bottom = false;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}




static void TabControls() {
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  Auto-Confirm Toggles");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    static bool df = false, tf = false, ve = false, va = false, vb = false;
    
    df = command::DropFastCommand::is_enabled();
    tf = command::TrashFastCommand::is_enabled();
    ve = command::VendFastCommand::is_empty_mode_enabled();
    va = command::VendFastCommand::is_add_mode_enabled();
    vb = command::VendFastCommand::is_buy_mode_enabled();

    ToggleRow("Drop Fast##chk",  " – auto-confirms drop dialogs",  &df,
        []{ command::DropFastCommand::toggle();
            AppendLog(std::string("[GUI] DropFast ") +
                (command::DropFastCommand::is_enabled() ? "ENABLED" : "DISABLED")); });

    ToggleRow("Trash Fast##chk", " – auto-confirms trash dialogs", &tf,
        []{ command::TrashFastCommand::toggle();
            AppendLog(std::string("[GUI] TrashFast ") +
                (command::TrashFastCommand::is_enabled() ? "ENABLED" : "DISABLED")); });

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  Vend Fast");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ToggleRow("Vend Empty##chk", " – auto-empty vend machine",    &ve,
        []{ command::VendFastCommand::toggle_empty_mode(); });

    ToggleRow("Vend Add##chk",   " – auto-add items to vend",     &va,
        []{ command::VendFastCommand::toggle_add_mode(); });

    ToggleRow("Vend Buy##chk",   " – auto-buy from vend",         &vb,
        []{ command::VendFastCommand::toggle_buy_mode(); });

    
    static bool vm = false;
    vm = command::ModDetectCommand::is_enabled();
    ToggleRow("ModDetect##chk",    " – monitor moderator spawns",   &vm,
        []{ if (g_cmd_runner) g_cmd_runner("/moddetect"); });

    
    static bool ac = false;
    ac = command::AutoCollectCommand::is_enabled();
    ToggleRow("AutoCollect##chk",  " – auto-collect dropped items", &ac,
        []{ if (g_cmd_runner) g_cmd_runner("/ac"); });

    ImGui::Spacing();
    ImGui::Spacing();

    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  Quick Actions");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##world_input", g_join_world, sizeof(g_join_world));
    ImGui::SameLine();
    if (AccentButton("Join World")) {
        
        if (g_cmd_runner && strlen(g_join_world) > 0) {
            g_cmd_runner(std::string("warp ") + g_join_world);
            AppendLog(std::string("[GUI] Warping to: ") + g_join_world);
        }
    }
    ImGui::SameLine();
    if (AccentButton("EXIT##exit")) {
        if (g_cmd_runner) g_cmd_runner("exit");
        AppendLog("[GUI] Warping to EXIT");
    }

    ImGui::Spacing();
    
    ImGui::PushItemWidth(50);
    ImGui::Text("Path:");
    ImGui::SameLine();
    ImGui::InputText("##ctrl_path_x", g_gui_path_x_str, sizeof(g_gui_path_x_str));
    ImGui::SameLine();
    ImGui::InputText("##ctrl_path_y", g_gui_path_y_str, sizeof(g_gui_path_y_str));
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        if (g_cmd_runner) {
            int x = atoi(g_gui_path_x_str);
            int y = atoi(g_gui_path_y_str);
            g_cmd_runner(fmt::format("/path {} {}", x, y));
        }
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    float bw = 120.f;
    if (AccentButton("Drop All", {bw,0})) {
        if (g_cmd_runner) g_cmd_runner("/dropall");
    }
    ImGui::SameLine();
    if (AccentButton("Pull All", {bw,0})) {
        if (g_cmd_runner) g_cmd_runner("/pullall");
    }
    ImGui::SameLine();
    if (DangerButton("Ban All", {bw,0})) {
        if (g_cmd_runner) g_cmd_runner("/banall");
    }

    ImGui::Spacing();
    if (AccentButton("Ping", {bw,0})) {
        if (g_cmd_runner) g_cmd_runner("/ping");
    }
    ImGui::SameLine();
    if (AccentButton("Who", {bw,0})) {
        
        ShowPlayersDialog();
    }

    ImGui::Spacing(); ImGui::Spacing();

    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  Proxy Commands");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    float pw = 118.f;
    if (AccentButton("dat##btn",   {pw,0})) { if (g_cmd_runner) g_cmd_runner("dat"); }
    ImGui::SameLine();
    if (AccentButton("info##btn",  {pw,0})) { if (g_cmd_runner) g_cmd_runner("info"); }
    ImGui::SameLine();
    if (AccentButton("ping##btn",  {pw,0})) { if (g_cmd_runner) g_cmd_runner("ping"); }

    ImGui::Spacing();

    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  Join Modes");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    static bool jm_pull = false, jm_kick = false, jm_ban = false;
    jm_pull = command::JoinCommand::is_pull_mode_enabled();
    jm_kick = command::JoinCommand::is_kick_mode_enabled();
    jm_ban  = command::JoinCommand::is_ban_mode_enabled();

    ToggleRow("Pull Mode##jm", " – auto-pull players on join", &jm_pull,
        []{ command::JoinCommand::toggle_pull_mode();
            AppendLog(std::string("[GUI] Pull Mode ") +
                (command::JoinCommand::is_pull_mode_enabled() ? "ENABLED" : "DISABLED")); });

    ToggleRow("Kick Mode##jm", " – auto-kick players on join", &jm_kick,
        []{ command::JoinCommand::toggle_kick_mode();
            AppendLog(std::string("[GUI] Kick Mode ") +
                (command::JoinCommand::is_kick_mode_enabled() ? "ENABLED" : "DISABLED")); });

    ToggleRow("Ban Mode##jm",  " – auto-ban players on join",  &jm_ban,
        []{ command::JoinCommand::toggle_ban_mode();
            AppendLog(std::string("[GUI] Ban Mode ") +
                (command::JoinCommand::is_ban_mode_enabled() ? "ENABLED" : "DISABLED")); });
}




static void TabWorld() {
    auto& wm = utils::WorldManager::get_instance();
    auto& pt = utils::PlayerTracker::get_instance();
    auto local = pt.get_local_player();

    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f,0.07f,0.18f,0.90f));
    if (ImGui::BeginChild("##world_card", ImVec2(-1, 110), true)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  World");
        ImGui::SameLine(120);
        if (wm.has_world()) {
            ImGui::TextColored(ImVec4(0.3f,0.95f,0.5f,1.f), "%s", wm.get_world_name().c_str());
        } else {
            ImGui::TextDisabled("(not in world)");
        }

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Size");
        ImGui::SameLine(120);
        ImGui::Text("%u x %u", wm.get_world_width(), wm.get_world_height());

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Tiles");
        ImGui::SameLine(120);
        ImGui::Text("%zu", wm.get_tiles().size());

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Dropped");
        ImGui::SameLine(120);
        ImGui::Text("%zu items", wm.get_items().size());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f,0.07f,0.18f,0.90f));
    if (ImGui::BeginChild("##player_card", ImVec2(-1, 100), true)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Player");
        ImGui::SameLine(120);
        if (local.name.empty()) {
            ImGui::TextColored(ImVec4(1.f,0.9f,0.5f,1.f), "%s", "(unknown)");
        } else {
            const std::string clean = StripGTColorCodes(local.name);
            ImGui::TextColored(ImVec4(1.f,0.9f,0.5f,1.f), "%s", clean.empty() ? local.name.c_str() : clean.c_str());
        }

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  NetID");
        ImGui::SameLine(120);
        ImGui::Text("%u", local.netID);

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Position");
        ImGui::SameLine(120);
        ImGui::Text("%.1f, %.1f", local.position.x, local.position.y);

        ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f), "  Country");
        ImGui::SameLine(120);
        ImGui::TextUnformatted(local.country.empty() ? "?" : local.country.c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    
    ImGui::Spacing();
    ImGui::TextDisabled("Dropped Items:");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
    if (ImGui::BeginChild("##items_child", ImVec2(0,0), false)) {
        auto& items = wm.get_items();
        if (ImGui::BeginTable("##items_tbl", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ID",     ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("X",      ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("Y",      ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clip;
            clip.Begin((int)items.size());
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                    auto& it = items[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u", it.ItemId);
                    ImGui::TableNextColumn(); ImGui::Text("%u", it.Amount);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", it.X);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", it.Y);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    
    ImGui::Spacing();
    ImGui::TextDisabled("Players in world:");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
    if (ImGui::BeginChild("##players_child", ImVec2(0,200), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        auto players_map = pt.get_all_players();
        std::vector<utils::PlayerTracker::PlayerInfo> players;
        players.reserve(players_map.size());
        for (auto& kv : players_map) players.push_back(kv.second);

        if (ImGui::BeginTable("##players_tbl", 6,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableSetupColumn("NetID", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("UserID",ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
            
            ImGui::TableSetupColumn("Tile X",     ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("Tile Y",     ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn("Country",ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clip;
            clip.Begin((int)players.size());
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                    const auto& p = players[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u", p.netID);
                    ImGui::TableNextColumn(); ImGui::Text("%u", p.userID);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(p.name.c_str());
                    {
                        
                        int tx = static_cast<int>(std::floor(p.position.x / 32.0f));
                        int ty = static_cast<int>(std::floor(p.position.y / 32.0f));
                        ImGui::TableNextColumn(); ImGui::Text("%d", tx);
                        ImGui::TableNextColumn(); ImGui::Text("%d", ty);
                    }
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(p.country.c_str());
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    
    if (wm.has_world() && wm.get_world_width() > 0) {
        ImGui::Spacing();
        ImGui::TextDisabled("World tiles (first 1000):");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
        if (ImGui::BeginChild("##tiles_child", ImVec2(0,200), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            auto& tiles = wm.get_tiles();
            if (ImGui::BeginTable("##tiles_tbl", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupScrollFreeze(0,1);
                ImGui::TableSetupColumn("Idx",   ImGuiTableColumnFlags_WidthFixed, 50.f);
                ImGui::TableSetupColumn("X",     ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableSetupColumn("Y",     ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableSetupColumn("Fg",    ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableSetupColumn("Bg",    ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableHeadersRow();
                ImGuiListClipper clip;
                clip.Begin((int)tiles.size());
                int max_show = 1000;
                int idx2=0;
                while (clip.Step()) {
                    for (int i = clip.DisplayStart; i < clip.DisplayEnd && idx2 < max_show; i++, idx2++) {
                        const auto& t = tiles[i];
                        uint32_t x = i % wm.get_world_width();
                        uint32_t y = i / wm.get_world_width();
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", i);
                        ImGui::TableNextColumn(); ImGui::Text("%u", x);
                        ImGui::TableNextColumn(); ImGui::Text("%u", y);
                        ImGui::TableNextColumn(); ImGui::Text("%u", t.Fg);
                        ImGui::TableNextColumn(); ImGui::Text("%u", t.Bg);
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        
    }
}



static extension::item_finder::ItemDatabase* g_item_db = nullptr;
void SetItemDatabase(extension::item_finder::ItemDatabase* db) { g_item_db = db; }

static void TabItemDB() {
    ImGui::TextDisabled("Use search to query decoded_items.json");
    static char g_item_search[128] = {};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##item_search", "name or ID...", g_item_search, sizeof(g_item_search));
    ImGui::SameLine();
    if (ImGui::Button("Clear##item_search")) { g_item_search[0] = '\0'; }
    ImGui::Spacing();
    if (!g_item_db) {
        ImGui::Text("Item DB not loaded");
        return;
    }
    std::string query(g_item_search);
    std::vector<extension::item_finder::ItemInfo> results;
    if (!query.empty()) {
        results = g_item_db->search_items(query, 200, "all");
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
    if (ImGui::BeginChild("##itemdb_child", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (query.empty()) {
            ImGui::Text("Type in the box above to start searching");
        } else if (results.empty()) {
            ImGui::Text("No items found for '%s'", query.c_str());
        } else {
            ImGuiListClipper clip;
            clip.Begin((int)results.size());
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                    auto& item = results[i];
                    ImGui::Separator();
                    ImGui::Text("[sprite: %s]", item.file_name.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.8f,0.9f,1.0f,1.0f), "%s (ID %d)", item.name.c_str(), item.id);
                    ImGui::Text("Type %d  Rarity %d", item.type, item.rarity);
                    ImGui::TextWrapped("Desc: %s", item.description.c_str());
                    if (!item.info.empty()) ImGui::TextWrapped("Info: %s", item.info.c_str());
                }
            }
            if (results.size() >= 200) {
                ImGui::Spacing();
                ImGui::TextDisabled("Showing first 200 results; refine search");
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}




static void TabPackets() {
    ImGui::Checkbox("Enable Packet Capture", &g_pkt_enabled);
    ImGui::SameLine();
    if (ImGui::Button("Clear##pkt")) {
        std::lock_guard<std::mutex> lk(g_pkt_mtx);
        g_packets.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu captured)", g_packets.size());
    ImGui::Spacing();

    static char g_pkt_filter[128] = {};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##pkt_filter", "Search packets (direction/time/data)...", g_pkt_filter, sizeof(g_pkt_filter));
    if (g_pkt_filter[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::Button("x##pkt_filter_clear")) {
            g_pkt_filter[0] = '\0';
        }
    }
    ImGui::Spacing();

    std::vector<PacketEntry> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_pkt_mtx);
        snapshot.assign(g_packets.begin(), g_packets.end());
    }

    std::vector<int> visible;
    visible.reserve(snapshot.size());
    std::string_view needle{ g_pkt_filter };
    for (int i = 0; i < (int)snapshot.size(); ++i) {
        const auto& p = snapshot[(size_t)i];
        if (needle.empty() ||
            ContainsIcase(p.timestamp, needle) ||
            ContainsIcase(p.direction, needle) ||
            ContainsIcase(p.data, needle)) {
            visible.push_back(i);
        }
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f,0.03f,0.08f,0.95f));
    if (ImGui::BeginChild("##pkt_child", ImVec2(0,0), false)) {
        if (ImGui::BeginTable("##pkt_tbl", 3,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed, 70.f);
            ImGui::TableSetupColumn("Dir",     ImGuiTableColumnFlags_WidthFixed, 50.f);
            ImGui::TableSetupColumn("Data",    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGuiListClipper clip;
            clip.Begin((int)visible.size());
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                    auto& p = snapshot[(size_t)visible[(size_t)i]];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", p.timestamp.c_str());
                    ImGui::TableNextColumn();
                    bool cs = (p.direction == "C\xe2\x86\x92S");
                    ImGui::TextColored(cs
                        ? ImVec4(0.3f,0.9f,0.5f,1.f)
                        : ImVec4(0.9f,0.5f,0.3f,1.f),
                        "%s", p.direction.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(p.data.c_str());
                }
            }
            
            if (needle.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}




static void TabProxy() {
    
    static char s5_host[128]  = {};
    static char s5_port[8]    = "1080";
    static char s5_user[64]   = {};
    static char s5_pass[64]   = {};
    static bool s5_enabled    = false;
    static bool loaded        = false;

    if (!loaded && g_proxy_get_fn) {
        auto v  = g_proxy_get_fn("proxy.enabled");
        auto h  = g_proxy_get_fn("proxy.host");
        auto p  = g_proxy_get_fn("proxy.port");
        auto u  = g_proxy_get_fn("proxy.username");
        auto pw = g_proxy_get_fn("proxy.password");
        s5_enabled = (v == "1" || v == "true");
        
        _snprintf_s(s5_host, sizeof(s5_host), _TRUNCATE, "%s", h.c_str());
        _snprintf_s(s5_port, sizeof(s5_port), _TRUNCATE, "%s", p.empty() ? "1080" : p.c_str());
        _snprintf_s(s5_user, sizeof(s5_user), _TRUNCATE, "%s", u.c_str());
        _snprintf_s(s5_pass, sizeof(s5_pass), _TRUNCATE, "%s", pw.c_str());
        loaded = true;
    }

    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  SOCKS5 Proxy");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Route GT server traffic through a SOCKS5 proxy.");
    ImGui::TextDisabled("Changes take effect on next connection.");
    ImGui::Spacing();

    bool changed = false;

    
    {
        bool v = s5_enabled;
        if (v) ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.25f,0.92f,0.45f,1.0f));
        else   ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.55f,0.55f,0.55f,1.0f));
        if (ImGui::Checkbox("Enable SOCKS5##s5", &v)) { s5_enabled = v; changed = true; }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    ImGui::BeginDisabled(!s5_enabled);

    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##s5host", "Host (e.g. 127.0.0.1)", s5_host, sizeof(s5_host));
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::InputTextWithHint("##s5port", "Port", s5_port, sizeof(s5_port));
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

    ImGui::Spacing();
    ImGui::TextDisabled("Optional credentials:");
    ImGui::SetNextItemWidth(145);
    ImGui::InputTextWithHint("##s5user", "Username", s5_user, sizeof(s5_user));
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(145);
    ImGui::InputText("##s5pass", s5_pass, sizeof(s5_pass), ImGuiInputTextFlags_Password);
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

    ImGui::EndDisabled();

    ImGui::Spacing();
    if (AccentButton("Save SOCKS5 Settings")) {
        changed = true;
    }

    if (changed && g_proxy_set_fn) {
        g_proxy_set_fn("proxy.enabled",  s5_enabled ? "true" : "false");
        g_proxy_set_fn("proxy.host",     s5_host);
        g_proxy_set_fn("proxy.port",     s5_port);
        g_proxy_set_fn("proxy.username", s5_user);
        g_proxy_set_fn("proxy.password", s5_pass);
        AppendLog(std::string("[GUI] SOCKS5 settings saved — enabled=") + (s5_enabled ? "YES" : "NO")
                  + " " + s5_host + ":" + s5_port);
        loaded = false; 
    }

    ImGui::Spacing(); ImGui::Spacing();

    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.80f,1.00f,1.0f));
    ImGui::TextUnformatted("  VPN");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("A VPN works at the OS level — connect your VPN client before"
                       " launching the proxy. No extra configuration is needed here.");
    ImGui::Spacing();
    ImGui::TextDisabled("Recommended free VPNs for GT:");
    ImGui::BulletText("Windscribe (10 GB/month free)");
    ImGui::BulletText("ProtonVPN (unlimited free tier)");
    ImGui::BulletText("Cloudflare WARP (free, fast)");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.f,0.75f,0.2f,1.f),
        "Tip: If GT can't connect, enable VPN first then restart Growtopia.");
}




void ProxyImGuiGUI(bool* p_open) {
    
    static std::string g_prev_world;
    auto& wm_check = utils::WorldManager::get_instance();
    auto& pt_check = utils::PlayerTracker::get_instance();
    if (wm_check.has_world()) {
        std::string name = wm_check.get_world_name();
        if (name != g_prev_world) {
            g_prev_world = name;
            
            {
                std::lock_guard<std::mutex> lk(g_log_mtx);
                g_log.clear();
            }
            {
                std::lock_guard<std::mutex> lk(g_pkt_mtx);
                g_packets.clear();
            }
            pt_check.clear();
        }
    } else if (!g_prev_world.empty()) {
        
        g_prev_world.clear();
        std::lock_guard<std::mutex> lk(g_log_mtx); g_log.clear();
        std::lock_guard<std::mutex> lk2(g_pkt_mtx); g_packets.clear();
        pt_check.clear();
    }

    
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::Begin("##main", p_open,
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    
    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsz  = ImGui::GetWindowSize();
    DrawBackground(ImGui::GetWindowDrawList(), wpos, wsz, (float)ImGui::GetTime());

    
    
    if (ImGui::Button(g_collapsed ? ">|" : "|<", ImVec2(24,0))) {
        g_collapsed = !g_collapsed;
        if (g_hWnd) {
            if (g_collapsed) {
                GetWindowRect(g_hWnd, &g_saved_rect);
                SetWindowPos(g_hWnd, nullptr,
                    g_saved_rect.left, g_saved_rect.top, 180, 32,
                    SWP_NOZORDER|SWP_NOACTIVATE);
            } else if (g_saved_rect.right) {
                int w = g_saved_rect.right  - g_saved_rect.left;
                int h = g_saved_rect.bottom - g_saved_rect.top;
                SetWindowPos(g_hWnd, nullptr,
                    g_saved_rect.left, g_saved_rect.top, w, h,
                    SWP_NOZORDER|SWP_NOACTIVATE);
            }
        }
    }
    ImGui::SameLine();
    
    if (ImGui::Button("_", ImVec2(24,0)) && g_hWnd) {
        ShowWindow(g_hWnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.75f,1.00f,1.0f));
    ImGui::Text("  Skript Proxy");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("v2.43");

    if (g_collapsed) { ImGui::End(); return; }

    
    if (g_show_proxy) ImGui::OpenPopup("Proxy Info");
    if (ImGui::BeginPopupModal("Proxy Info", &g_show_proxy, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (auto &l : g_proxy_lines) {
            ImGui::TextWrapped("%s", l.c_str());
        }
        if (ImGui::Button("Close")) g_show_proxy = false;
        ImGui::EndPopup();
    }

    ImGui::Separator();

    
    auto& pt = utils::PlayerTracker::get_instance();
    auto  lp = pt.get_local_player();
    auto& wm = utils::WorldManager::get_instance();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.72f,0.95f,1.0f));
    if (lp.name.empty()) {
        ImGui::Text("  %s", "Not connected");
    } else {
        const std::string clean = StripGTColorCodes(lp.name);
        ImGui::Text("  %s", clean.empty() ? lp.name.c_str() : clean.c_str());
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextColored(wm.has_world()
        ? ImVec4(0.3f,0.92f,0.5f,1.f)
        : ImVec4(0.5f,0.5f,0.5f,1.f),
        "%s", wm.has_world() ? wm.get_world_name().c_str() : "No world");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("DropFast: ");
    ImGui::SameLine();
    Badge(command::DropFastCommand::is_enabled()  ? "ON" : "off",
          command::DropFastCommand::is_enabled()
              ? ImVec4(0.3f,0.92f,0.5f,1.f) : ImVec4(0.5f,0.5f,0.5f,1.f));
    ImGui::SameLine();
    ImGui::TextDisabled("TrashFast: ");
    ImGui::SameLine();
    Badge(command::TrashFastCommand::is_enabled() ? "ON" : "off",
          command::TrashFastCommand::is_enabled()
              ? ImVec4(0.3f,0.92f,0.5f,1.f) : ImVec4(0.5f,0.5f,0.5f,1.f));

    
    {
        const bool connected = g_is_connected_fn ? g_is_connected_fn() : false;
        const float btn_w   = 88.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float win_w   = ImGui::GetIO().DisplaySize.x  
                              - ImGui::GetStyle().WindowPadding.x * 2.f;
        const float cursor_x = win_w - btn_w * 2.f - spacing;
        if (cursor_x > ImGui::GetCursorPosX() + 4.f)
            ImGui::SameLine(cursor_x);
        else
            ImGui::SameLine(); 

        if (!connected) {
            if (AccentButton("Connect", ImVec2(btn_w, 0))) {
                if (g_connect_fn) {
                    g_connect_fn();
                    AppendLog("[GUI] Connecting to server...");
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled();
            DangerButton("Disconnect", ImVec2(btn_w, 0));
            ImGui::EndDisabled();
        } else {
            ImGui::BeginDisabled();
            AccentButton("Connect", ImVec2(btn_w, 0));
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (DangerButton("Disconnect", ImVec2(btn_w, 0))) {
                if (g_disconnect_fn) {
                    g_disconnect_fn();
                    AppendLog("[GUI] Disconnecting from server...");
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("  Debugger  "))  { TabDebugger();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  Executor  "))   { TabLua();       ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  Controls  "))   { TabControls();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  World  "))      { TabWorld();     ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  Item DB  "))    { TabItemDB();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  Packets  "))    { TabPackets();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("  Proxy  "))      { TabProxy();     ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}





void RunImGuiApp() {
    WNDCLASSEX wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        _T("SkriptProxyImGui"), nullptr
    };
    RegisterClassEx(&wc);

    int ww = 860, wh = 560;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName, _T("Skript Proxy"),
        WS_POPUP | WS_VISIBLE,
        (sw-ww)/2, (sh-wh)/2, ww, wh,
        nullptr, nullptr, wc.hInstance, nullptr);
    g_hWnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    
    
    {
        ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\GOTHICB.TTF", 17.0f);
        if (!font) {
            font = io.Fonts->AddFontDefault();
        }
        io.FontDefault = font;
    }

    ApplySkriptTheme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    
    float alpha = 0.f;
    bool fading = true;

    bool show = true;
    MSG  msg  = {};

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        
        if (fading) {
            alpha += 0.07f;
            if (alpha >= 1.0f) { alpha = 1.0f; fading = false; }
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)(alpha * 235), LWA_ALPHA);
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ProxyImGuiGUI(&show);

        ImGui::EndFrame();

        
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
            D3DCOLOR_RGBA(9, 13, 30, 255), 1.0f, 0);

        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }

        HRESULT hr = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (hr == D3DERR_DEVICELOST &&
            g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();

        if (!show) break;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}



bool CreateDeviceD3D(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) return false;
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed               = TRUE;
    g_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
    return g_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) >= 0;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D)       { g_pD3D->Release();       g_pD3D       = nullptr; }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (g_pd3dDevice->Reset(&g_d3dpp) != D3DERR_INVALIDCALL)
        ImGui_ImplDX9_CreateDeviceObjects();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wP, lP)) return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wP != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth  = LOWORD(lP);
            g_d3dpp.BackBufferHeight = HIWORD(lP);
            ResetDevice();
        }
        return 0;
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lP), GET_Y_LPARAM(lP) };
        ScreenToClient(hWnd, &pt);
        if (pt.y < g_hdrH) {
            
            if (pt.x >= 60) 
                return HTCAPTION; 
        }
        break;
    }
    case WM_SYSCOMMAND:
        if ((wP & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wP, lP);
}
