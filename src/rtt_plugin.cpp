// Road Traffic Tuner - X-Plane plugin layer (menu, windows, startup check).
// All file work lives in rtt_core.cpp.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPStandardWidgets.h"
#include "XPWidgets.h"

#include "rtt_core.h"

using namespace rtt;

namespace {

constexpr int kSliderSteps = 40;  // 0.00 .. 2.00
constexpr double kSliderStep = 0.05;

constexpr intptr_t kMenuOpen = 1;
constexpr intptr_t kMenuAbout = 2;

Paths g_paths;
std::array<bool, kNetworkCount> g_net_ok{};       // file readable, expected layout
std::array<LaneStats, kNetworkCount> g_base;      // default ratios (multiplier 1.0)
Multipliers g_mults = default_multipliers();
NetworkSet g_nets = all_networks();
bool g_settings_loaded = false;                   // pack settings applied to the UI
std::string g_action_msg;                         // result of the last button press
bool g_confirm_remove = false;

XPLMMenuID g_menu = nullptr;
XPWidgetID g_window = nullptr;
XPWidgetID g_about = nullptr;
XPWidgetID g_netbox[kNetworkCount] = {};
XPWidgetID g_slider[kFamilyCount] = {};
XPWidgetID g_value[kFamilyCount] = {};
XPWidgetID g_spacing[kFamilyCount] = {};
XPWidgetID g_status[3] = {};
XPWidgetID g_btn_build = nullptr, g_btn_remove = nullptr, g_btn_reset = nullptr;

const char* kAboutText[] = {
    "Road Traffic Tuner rebalances X-Plane 12's own road traffic between road types - for example,",
    "making residential streets mostly empty while main roads stay busy.",
    "",
    "WHAT IT DOES",
    "X-Plane's road files give every car lane a 'spawn ratio': roughly, cars per metre of road. Out of",
    "the box, a quiet residential street gets about the same density per lane as a busy arterial.",
    "Build pack creates the folder Custom Scenery/zzz_Road_Traffic_Tuner, containing copies of the road",
    "files you tick - roads.net (most of the world) and/or roads_EU.net (Europe) - in which only the",
    "spawn ratios are multiplied by your slider settings, plus the road objects and textures they need,",
    "and a library.txt telling X-Plane to use these copies instead of its defaults.",
    "",
    "WHAT IT DOES NOT DO",
    "It never modifies X-Plane's own files. It does not change the total number of cars (X-Plane's",
    "graphics settings control that), only how those cars are shared between road types. It does not",
    "add vehicles, change car models or speeds, or use live traffic data.",
    "",
    "OTHER SCENERY THAT REPLACES ROAD FILES",
    "Other Custom Scenery packs can also replace these road files (xRoads, for example). X-Plane uses",
    "only one version of each road file: the one from the pack highest in Custom Scenery/",
    "scenery_packs.ini. When Open... finds such packs, it lists them by name and road network. A pack",
    "listed above this one overrides your traffic settings for that network; to use this plugin's",
    "settings instead, move this pack's line above it in scenery_packs.ini or remove the other pack.",
    "",
    "USING IT",
    "Changes take effect the next time X-Plane starts. Remove pack (pressed twice) deletes the folder,",
    "returning X-Plane to default traffic after a restart. If an X-Plane update changes the road files,",
    "the plugin window opens at startup so you can rebuild the pack with your saved settings.",
};

void rtt_log(const std::string& s) {
    XPLMDebugString(("Road Traffic Tuner: " + s + "\n").c_str());
}

int to_pos(double m) {
    int p = static_cast<int>(m / kSliderStep + 0.5);
    return p < 0 ? 0 : (p > kSliderSteps ? kSliderSteps : p);
}

std::string fmt2(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "x%.2f", v);
    return buf;
}

std::string network_list(const NetworkSet& nets) {
    std::string s;
    for (int n = 0; n < kNetworkCount; ++n)
        if (nets[n]) s += (s.empty() ? "" : " + ") + std::string(kNetworks[n].label);
    return s;
}

// Load the default road files once per window opening, for spacing and checks.
void load_base() {
    for (int n = 0; n < kNetworkCount; ++n) {
        std::string text;
        g_base[n] = LaneStats{};
        g_net_ok[n] = false;
        if (!read_file(g_paths.net_file(Network(n)), text)) {
            rtt_log("could not read " + g_paths.net_file(Network(n)).u8string());
            continue;
        }
        scale_roads_net(text, Multipliers{1, 1, 1, 1}, &g_base[n]);
        g_net_ok[n] = validate(g_base[n]).empty();
        if (!g_net_ok[n]) rtt_log(std::string(kNetworks[n].file) + " has an unexpected layout");
    }
}

void update_row(int i) {
    if (!g_window) return;
    XPSetWidgetDescriptor(g_value[i], fmt2(g_mults[i]).c_str());
    std::vector<std::pair<double, double>> lanes;
    for (int n = 0; n < kNetworkCount; ++n)
        if (g_nets[n] && g_net_ok[n])
            for (const auto& l : g_base[n][i]) lanes.emplace_back(l.first, l.first * g_mults[i]);
    std::string sp = lanes.empty() ? std::string()
                                   : typical_spacing(lanes, false) + "  ->  " +
                                         typical_spacing(lanes, true);
    XPSetWidgetDescriptor(g_spacing[i], sp.c_str());
}

void update_rows() {
    for (int i = 0; i < kFamilyCount; ++i) update_row(i);
}

void set_status() {
    if (!g_window) return;
    std::string l1, l2;
    std::string bad;
    for (int n = 0; n < kNetworkCount; ++n)
        if (g_nets[n] && !g_net_ok[n]) bad += (bad.empty() ? "" : ", ") + std::string(kNetworks[n].file);

    PackSettings s;
    bool installed = pack_installed(g_paths);
    bool have_settings = installed && load_settings(g_paths, s);
    if (!bad.empty()) {
        l1 = "Cannot use X-Plane's " + bad + " - see Log.txt.";
    } else if (have_settings && !stale_networks(g_paths, s).empty()) {
        l1 = "X-Plane's road files changed since the pack was built: press Build pack.";
    } else if (have_settings) {
        l1 = "Pack installed, replacing: " + network_list(s.networks) + ".";
    } else if (installed) {
        l1 = "Pack folder found (Custom Scenery/" + std::string(kPackName) + ").";
    } else {
        l1 = "No pack installed - X-Plane is using its default traffic.";
    }

    auto conflicts = find_conflicts(g_paths);
    if (conflicts.empty()) {
        l2 = "No other packs replace the road files.";
    } else {
        l2 = "Also replacing road files: ";
        for (size_t i = 0; i < conflicts.size(); ++i)
            l2 += (i ? ", " : "") + conflicts[i].pack + " (" +
                  kNetworks[conflicts[i].network].label + ")";
        if (l2.size() > 110) l2 = l2.substr(0, 107) + "...";
    }
    XPSetWidgetDescriptor(g_status[0], l1.c_str());
    XPSetWidgetDescriptor(g_status[1], l2.c_str());
    XPSetWidgetDescriptor(g_status[2], g_action_msg.c_str());
}

void sync_controls() {
    if (!g_window) return;
    for (int i = 0; i < kFamilyCount; ++i)
        XPSetWidgetProperty(g_slider[i], xpProperty_ScrollBarSliderPosition, to_pos(g_mults[i]));
    for (int n = 0; n < kNetworkCount; ++n)
        XPSetWidgetProperty(g_netbox[n], xpProperty_ButtonState, g_nets[n] ? 1 : 0);
    update_rows();
}

// Apply the installed pack's settings to the controls (once per session).
void load_pack_settings_once() {
    if (g_settings_loaded) return;
    g_settings_loaded = true;
    PackSettings s;
    if (pack_installed(g_paths) && load_settings(g_paths, s)) {
        g_mults = s.mults;
        g_nets = s.networks;
    }
}

void do_build() {
    auto r = build_pack(g_paths, g_mults, g_nets);
    if (!r.ok) {
        g_action_msg = "Build failed - see Log.txt.";
        rtt_log("build failed: " + r.error);
        if (r.error == "No road network selected.") g_action_msg = "Tick at least one road network.";
        return;
    }
    g_action_msg = "Built (" + std::to_string(r.files_copied) +
                   " files copied). Restart X-Plane to apply.";
    if (!r.missing.empty())
        g_action_msg = "Built, but " + std::to_string(r.missing.size()) +
                       " files were missing - see Log.txt. Restart to apply.";
    rtt_log("built pack for " + network_list(g_nets) + ": " +
            std::to_string(r.files_copied) + " files copied");
    for (const auto& m : r.missing) rtt_log("missing file: " + m);
}

void do_remove() {
    if (!pack_installed(g_paths)) {
        g_action_msg = "No pack to remove.";
        return;
    }
    if (!g_confirm_remove) {
        g_confirm_remove = true;
        g_action_msg = "Press Remove pack again to confirm.";
        return;
    }
    g_confirm_remove = false;
    std::string err;
    if (remove_pack(g_paths, err)) {
        g_action_msg = "Pack removed. Restart X-Plane for default traffic.";
        rtt_log("pack removed");
    } else {
        g_action_msg = "Remove failed - see Log.txt.";
        rtt_log(err);
    }
}

int window_handler(XPWidgetMessage msg, XPWidgetID, intptr_t p1, intptr_t p2) {
    if (msg == xpMessage_CloseButtonPushed) {
        XPHideWidget(g_window);
        return 1;
    }
    XPWidgetID w = reinterpret_cast<XPWidgetID>(p1);
    if (msg == xpMsg_ScrollBarSliderPositionChanged) {
        for (int i = 0; i < kFamilyCount; ++i) {
            if (w == g_slider[i]) {
                int pos = static_cast<int>(
                    XPGetWidgetProperty(g_slider[i], xpProperty_ScrollBarSliderPosition, nullptr));
                g_mults[i] = pos * kSliderStep;
                update_row(i);
                return 1;
            }
        }
    }
    if (msg == xpMsg_ButtonStateChanged) {
        for (int n = 0; n < kNetworkCount; ++n) {
            if (w == g_netbox[n]) {
                g_nets[n] = p2 != 0;
                g_confirm_remove = false;
                update_rows();
                set_status();
                return 1;
            }
        }
    }
    if (msg == xpMsg_PushButtonPressed) {
        if (w != g_btn_remove) g_confirm_remove = false;
        if (w == g_btn_build) {
            do_build();
        } else if (w == g_btn_remove) {
            do_remove();
        } else if (w == g_btn_reset) {
            g_mults = default_multipliers();
            g_nets = all_networks();
            sync_controls();
            g_action_msg = "Reset to defaults (press Build pack to apply).";
        } else {
            return 0;
        }
        set_status();
        return 1;
    }
    return 0;
}

int about_handler(XPWidgetMessage msg, XPWidgetID, intptr_t, intptr_t) {
    if (msg == xpMessage_CloseButtonPushed) {
        XPHideWidget(g_about);
        return 1;
    }
    return 0;
}

XPWidgetID caption(int l, int t, int r, int b, const char* text, XPWidgetID parent) {
    return XPCreateWidget(l, t, r, b, 1, text, 0, parent, xpWidgetClass_Caption);
}

XPWidgetID button(int l, int t, int r, int b, const char* text, XPWidgetID parent) {
    XPWidgetID w = XPCreateWidget(l, t, r, b, 1, text, 0, parent, xpWidgetClass_Button);
    XPSetWidgetProperty(w, xpProperty_ButtonType, xpPushButton);
    XPSetWidgetProperty(w, xpProperty_ButtonBehavior, xpButtonBehaviorPushButton);
    return w;
}

XPWidgetID checkbox(int l, int t, XPWidgetID parent) {
    XPWidgetID w = XPCreateWidget(l, t, l + 16, t - 16, 1, "", 0, parent, xpWidgetClass_Button);
    XPSetWidgetProperty(w, xpProperty_ButtonType, xpRadioButton);
    XPSetWidgetProperty(w, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox);
    return w;
}

struct Rect {
    int l, t, r, b;
    bool contains(int x, int y) const { return x >= l && x < r && y <= t && y > b; }
};

struct MonitorSearch {
    int x, y;
    bool found;
    Rect rect;
};

void on_monitor(int, int l, int t, int r, int b, void* ref) {
    auto* s = static_cast<MonitorSearch*>(ref);
    Rect m{l, t, r, b};
    if (!s->found && m.contains(s->x, s->y)) {
        s->found = true;
        s->rect = m;
    }
}

// Choose where a new window opens: just below the mouse (which is on the
// Plugins menu when opened from the menu), kept wholly on the monitor the
// mouse is on. Positions are in global desktop boxels. After this X-Plane
// leaves the window free, so the user can drag it anywhere.
void window_origin(int w, int h, int& L, int& T) {
    int mx = 0, my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);
    MonitorSearch s{mx, my, false, {}};
    XPLMGetAllMonitorBoundsGlobal(on_monitor, &s);
    Rect area = s.rect;
    if (!s.found) {  // windowed X-Plane, or mouse outside X-Plane: whole desktop
        XPLMGetScreenBoundsGlobal(&area.l, &area.t, &area.r, &area.b);
        if (!area.contains(mx, my)) {
            mx = area.l + 100 + w / 2;
            my = area.t - 60;
        }
    }
    const int margin = 20;
    L = mx - w / 2;
    T = my - 40;  // below the menu bar
    if (L + w > area.r - margin) L = area.r - margin - w;
    if (L < area.l + margin) L = area.l + margin;
    if (T > area.t - margin) T = area.t - margin;
    if (T - h < area.b + margin) T = area.b + margin + h;
}

void create_window() {
    const int W = 720, H = 400;
    int L, T;
    window_origin(W, H, L, T);
    const int R = L + W, B = T - H;

    g_window = XPCreateWidget(L, T, R, B, 0, "Road Traffic Tuner", 1, nullptr,
                              xpWidgetClass_MainWindow);
    XPSetWidgetProperty(g_window, xpProperty_MainWindowHasCloseBoxes, 1);
    XPAddWidgetCallback(g_window, window_handler);

    int y = T - 30;
    caption(L + 15, y, R - 15, y - 18,
            "Traffic on each kind of road (x1.00 = X-Plane default, x0 = none)", g_window);
    y -= 30;
    caption(L + 15, y, L + 85, y - 18, "Apply to:", g_window);
    g_netbox[kWorld] = checkbox(L + 90, y - 1, g_window);
    caption(L + 112, y, L + 360, y - 18, "Rest of the world (roads.net)", g_window);
    g_netbox[kEurope] = checkbox(L + 370, y - 1, g_window);
    caption(L + 392, y, R - 15, y - 18, "Europe (roads_EU.net)", g_window);
    y -= 32;
    for (int i = 0; i < kFamilyCount; ++i) {
        caption(L + 15, y, L + 95, y - 20, kFamilies[i].label, g_window);
        caption(L + 95, y, L + 280, y - 20, kFamilies[i].description, g_window);
        g_slider[i] = XPCreateWidget(L + 285, y - 2, L + 455, y - 18, 1, "", 0, g_window,
                                     xpWidgetClass_ScrollBar);
        XPSetWidgetProperty(g_slider[i], xpProperty_ScrollBarType, xpScrollBarTypeSlider);
        XPSetWidgetProperty(g_slider[i], xpProperty_ScrollBarMin, 0);
        XPSetWidgetProperty(g_slider[i], xpProperty_ScrollBarMax, kSliderSteps);
        XPSetWidgetProperty(g_slider[i], xpProperty_ScrollBarPageAmount, 2);
        g_value[i] = caption(L + 470, y, L + 525, y - 20, "", g_window);
        g_spacing[i] = caption(L + 540, y, R - 15, y - 20, "", g_window);
        y -= 28;
    }
    caption(L + 15, y, R - 15, y - 18,
            "Right column: typical spacing between cars in a lane (default -> new).", g_window);
    y -= 34;
    for (int i = 0; i < 3; ++i) {
        g_status[i] = caption(L + 15, y, R - 15, y - 18, "", g_window);
        y -= 20;
    }
    y -= 12;
    g_btn_build = button(L + 15, y, L + 135, y - 22, "Build pack", g_window);
    g_btn_remove = button(L + 145, y, L + 265, y - 22, "Remove pack", g_window);
    g_btn_reset = button(R - 155, y, R - 15, y - 22, "Reset to defaults", g_window);
}

void create_about() {
    const int lines = static_cast<int>(sizeof kAboutText / sizeof kAboutText[0]);
    const int W = 760, H = 50 + lines * 16;
    int L, T;
    window_origin(W, H, L, T);
    g_about = XPCreateWidget(L, T, L + W, T - H, 0,
                             (std::string("About Road Traffic Tuner ") + kToolVersion).c_str(),
                             1, nullptr, xpWidgetClass_MainWindow);
    XPSetWidgetProperty(g_about, xpProperty_MainWindowHasCloseBoxes, 1);
    XPAddWidgetCallback(g_about, about_handler);
    int y = T - 30;
    for (int i = 0; i < lines; ++i) {
        caption(L + 15, y, L + W - 15, y - 15, kAboutText[i], g_about);
        y -= 16;
    }
}

void open_window() {
    if (!g_window) create_window();
    load_pack_settings_once();
    load_base();
    sync_controls();
    set_status();
    XPShowWidget(g_window);
    XPBringRootWidgetToFront(g_window);
}

void open_about() {
    if (!g_about) create_about();
    XPShowWidget(g_about);
    XPBringRootWidgetToFront(g_about);
}

void menu_handler(void*, void* item) {
    intptr_t id = reinterpret_cast<intptr_t>(item);
    if (id == kMenuOpen) open_window();
    else if (id == kMenuAbout) open_about();
}

// Runs once after X-Plane has started: offer a rebuild if the pack is stale.
float startup_check(float, float, int, void*) {
    PackSettings s;
    if (pack_installed(g_paths) && load_settings(g_paths, s)) {
        auto stale = stale_networks(g_paths, s);
        if (!stale.empty()) {
            for (auto n : stale)
                rtt_log(std::string(kNetworks[n].file) + " changed since the pack was built");
            g_action_msg = "Your saved settings are loaded - press Build pack to update.";
            open_window();
        }
    }
    return 0;  // do not call again
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    std::strcpy(outName, "Road Traffic Tuner");
    std::strcpy(outSig, "roadtraffictuner.plugin");
    std::strcpy(outDesc, "Rebalances X-Plane's road traffic between road types.");

    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    // Modern widget windows: positioned in global desktop coordinates (the same
    // ones window_origin() uses) and they follow X-Plane's UI size setting.
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);
    char root[2048] = {};
    XPLMGetSystemPath(root);
    g_paths.xplane_root = fs::u8path(root);  // X-Plane gives UTF-8 paths

    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Road Traffic Tuner", nullptr, 0);
    g_menu = XPLMCreateMenu("Road Traffic Tuner", XPLMFindPluginsMenu(), item,
                            menu_handler, nullptr);
    XPLMAppendMenuItem(g_menu, "Open...", reinterpret_cast<void*>(kMenuOpen), 0);
    XPLMAppendMenuItem(g_menu, "About...", reinterpret_cast<void*>(kMenuAbout), 0);
    rtt_log(std::string("version ") + kToolVersion + " started, X-Plane folder: " + root);
    return 1;
}

PLUGIN_API void XPluginStop() {
    if (g_window) XPDestroyWidget(g_window, 1);
    if (g_about) XPDestroyWidget(g_about, 1);
    g_window = g_about = nullptr;
    if (g_menu) XPLMDestroyMenu(g_menu);
    g_menu = nullptr;
}

PLUGIN_API int XPluginEnable() {
    XPLMRegisterFlightLoopCallback(startup_check, -1.0f, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable() {
    XPLMUnregisterFlightLoopCallback(startup_check, nullptr);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) {}
