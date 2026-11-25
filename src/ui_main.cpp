// CardputerZero device entry point (LVGL + Linux framebuffer/evdev).
//
// This is the executable packaged as `cardmesh` in the arm64 .deb. It has
// NOT been run against real CardputerZero hardware or the online emulator —
// see docs/DEVICE_BUILD.md for exactly what is and isn't verified.

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "lvgl.h"

#include "Version.h"
#include "map/TileFetcher.h"
#include "storage/Settings.h"
#include "ui/BootScreen.h"
#include "ui/DashboardModel.h"
#include "ui/DashboardScreen.h"
#include "ui/EvdevKeyboard.h"
#include "ui/HomeScreen.h"
#include "ui/LoraMessagesModel.h"
#include "ui/LoraMessagesScreen.h"
#include "ui/MapModel.h"
#include "ui/MapScreen.h"
#include "ui/MeshCoreMessagesModel.h"
#include "ui/MeshCoreMessagesScreen.h"
#include "ui/MeshCoreWorker.h"
#include "ui/MeshMonitorMessagesModel.h"
#include "ui/MeshMonitorMessagesScreen.h"
#include "ui/MeshtasticWorker.h"
#include "ui/NetworkWorker.h"
#include "ui/SettingsScreen.h"
#include "update/UpdateWorker.h"
#include "mesh/ChannelKey.h"
#include "mesh/LoraRegions.h"
#include "mesh/MeshPacketCodec.h"

namespace {

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

// Milliseconds since process start, for diagnostic logs -- lets a reader
// tell "the UI thread is still ticking, just nothing changed" apart from
// "the UI thread stopped advancing entirely" by comparing gaps between
// consecutive log lines, including the main-loop heartbeat below.
long long logMs() {
    // A raw steady_clock reading (no per-TU "start" epoch subtracted) is a
    // single, process-wide monotonic timeline shared by ui_main.cpp,
    // MeshtasticWorker.cpp, and MeshCoreWorker.cpp -- subtracting an
    // independent "first call in this translation unit" epoch per file (the
    // earlier version of this helper) silently produced three unrelated
    // clocks, which made cross-file [nav]/[worker] gaps meaningless.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// See MeshtasticWorker.cpp's identical helper -- lets a reader confirm every
// [nav]/heartbeat line below shares the same tag (the main thread), so a
// stall showing up here, rather than only in [worker] lines, means the main
// loop itself is blocked, not just a worker's background thread.
std::string threadTag() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

enum class ActiveScreen { Home, Dashboard, Map, Messages, RelayMessages, MeshCore, Settings };

const char* activeScreenName(ActiveScreen screen) {
    switch (screen) {
        case ActiveScreen::Home: return "Home";
        case ActiveScreen::Dashboard: return "Dashboard";
        case ActiveScreen::Map: return "Map";
        case ActiveScreen::Messages: return "Messages";
        case ActiveScreen::RelayMessages: return "RelayMessages";
        case ActiveScreen::MeshCore: return "MeshCore";
        case ActiveScreen::Settings: return "Settings";
    }
    return "?";
}

}  // namespace

int main() {
    auto settings = cardmesh::storage::Settings::load();
    if (!settings.has_value()) {
        // First run: no config file yet. Exiting here (the previous
        // behavior) is a launch-blocking bug confirmed on real hardware
        // (GitHub issue #5) -- the launcher shows its own "loading" dialog
        // while this process starts, the process prints to stderr (which
        // the launcher never surfaces to the user) and exits in well under
        // a second, so it looks exactly like "the app doesn't launch" with
        // no visible error. There is no first-run setup screen, so fall
        // back to defaults and let the user configure the MeshMonitor
        // server afterward from the in-app Settings screen -- direct
        // Meshtastic/MeshCore over the onboard LoRa hat doesn't need a
        // MeshMonitor server at all, so the app is fully usable without one
        // configured yet. Best-effort persisted so the file exists for the
        // Settings screen to edit; failure to save isn't fatal here.
        settings = cardmesh::storage::AppSettings{};
        cardmesh::storage::Settings::save(*settings);
    }

    lv_init();
    lv_lodepng_init();  // needed by DashboardScreen's imgfont and MapScreen's tiles

    lv_display_t* display = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(display, envOr("CARDMESH_FB_DEVICE", "/dev/fb0").c_str());

    // /dev/input/cardputer-zero-internal is a stable udev symlink to the
    // device's built-in keyboard (confirmed on real hardware to be
    // /dev/input/event3, which is not a stable index to hardcode directly).
    cardmesh::ui::EvdevKeyboard keyboard(
        envOr("CARDMESH_INPUT_DEVICE", "/dev/input/cardputer-zero-internal"));
    if (!keyboard.isOpen()) {
        std::cerr << "Warning: could not open input device; global shortcuts will not work.\n";
    }

    const std::filesystem::path stateDir =
        std::filesystem::path(std::getenv("HOME") != nullptr ? std::getenv("HOME") : ".") /
        ".local" / "state" / "cardmesh";
    std::error_code ec;
    std::filesystem::create_directories(stateDir / "tiles", ec);

    cardmesh::map::TileFetcher tileFetcher(cardmesh::map::TileCache(
        (stateDir / "tiles").string(), settings->tileServerTemplate,
        "CardMesh/" + std::string(cardmesh::kVersion) + " (+https://github.com/" +
            std::string(cardmesh::kGitHubRepo) + ")"));
    tileFetcher.start();

    cardmesh::storage::AppSettings mutableSettings = *settings;

    cardmesh::ui::DashboardModel model;
    cardmesh::ui::MapModel mapModel;
    cardmesh::ui::LoraMessagesModel loraMessages;
    cardmesh::ui::MeshMonitorMessagesModel relayMessagesModel;
    cardmesh::ui::MeshCoreMessagesModel meshCoreMessagesModel;
    cardmesh::ui::BootScreen boot;
    cardmesh::ui::HomeScreen home;
    cardmesh::ui::SettingsScreen settingsScreen;
    cardmesh::ui::DashboardScreen dashboard(model);
    cardmesh::ui::MapScreen map(mapModel, tileFetcher);
    cardmesh::ui::LoraMessagesScreen messages(loraMessages);
    cardmesh::ui::MeshMonitorMessagesScreen relayMessages(relayMessagesModel);
    cardmesh::ui::MeshCoreMessagesScreen meshCoreMessages(meshCoreMessagesModel);

    // Independent of NetworkWorker/MeshMonitor by design -- both mesh paths
    // (REST-relayed and direct over the onboard LoRa hat) run concurrently.
    cardmesh::ui::MeshtasticWorker meshtasticWorker(mutableSettings, loraMessages, mapModel);
    meshtasticWorker.start();

    // MeshCoreWorker shares the single physical LoRa radio with
    // MeshtasticWorker (incompatible RF configs, so only one may own it at
    // a time) -- it starts only when the MeshCore screen is opened, and the
    // Dashboard loop below swaps ownership on entry/exit of that screen.
    cardmesh::ui::MeshCoreWorker meshCoreWorker(mutableSettings, meshCoreMessagesModel);

    cardmesh::ui::NetworkWorker worker(*settings, model, mapModel, relayMessagesModel);
    // Feeds MeshMonitor's own channel list (and each channel's base64 PSK,
    // where exposed) into MeshtasticWorker so direct-LoRa decoding isn't
    // limited to the public MediumFast/default-PSK channel. A channel with
    // no usable key (resolveChannelKey returns empty) is skipped. The
    // Primary channel's stored name is often empty -- MeshMonitor convention
    // mirrors real Meshtastic firmware here -- so it's substituted with
    // "MediumFast", this network's actual preset (see MeshtasticCrypto.h).
    worker.onChannelsUpdated = [&meshtasticWorker](const std::vector<cardmesh::models::Channel>& channels) {
        std::vector<cardmesh::mesh::KnownChannel> known;
        for (const auto& channel : channels) {
            auto key = cardmesh::mesh::resolveChannelKey(channel.psk);
            if (key.empty()) {
                continue;
            }
            cardmesh::mesh::KnownChannel entry;
            entry.name = channel.name.empty() ? "MediumFast" : channel.name;
            entry.key = std::move(key);
            known.push_back(std::move(entry));
        }
        meshtasticWorker.setChannels(std::move(known));
    };
    worker.start();

    cardmesh::update::UpdateModel updateModel;
    cardmesh::update::UpdateWorker updateWorker(updateModel, cardmesh::kGitHubRepo);
    updateWorker.start();
    if (mutableSettings.autoUpdateCheck) {
        updateWorker.checkNow();
    }

    // Fixed for the whole session: the 'S' key only changes the persisted
    // setting for the *next* launch, not the currently-running navigation
    // tree, so there's no need to re-check mutableSettings mid-session.
    const bool homeIsIconGrid = !mutableSettings.homeScreenIsDashboard;

    lv_screen_load(boot.screenObj());
    {
        const auto bootStart = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - bootStart < std::chrono::milliseconds(1500)) {
            const std::uint32_t sleepMs = lv_timer_handler();
            constexpr std::uint32_t kMaxSleepMs = 50;
            const std::uint32_t clampedSleepMs =
                (sleepMs > 0 && sleepMs <= kMaxSleepMs) ? sleepMs : kMaxSleepMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(clampedSleepMs));
        }
    }

    ActiveScreen active = homeIsIconGrid ? ActiveScreen::Home : ActiveScreen::Dashboard;
    lv_screen_load(homeIsIconGrid ? home.screenObj() : dashboard.screenObj());
    std::fprintf(stderr, "[nav %6lldms t=%s] pid=%d boot -> %s\n", logMs(), threadTag().c_str(),
                 static_cast<int>(::getpid()), activeScreenName(active));

    // Centralizes the `active = ActiveScreen::X` assignment used throughout
    // this file's key-dispatch logic so every transition is logged the same
    // way (pid included, so a relaunch after a crash shows up as a new pid
    // rather than being confused with a continued process; timestamped so a
    // gap between this and the next heartbeat line shows whether the main
    // loop itself stalled, vs. just not changing screens).
    auto setActive = [&](ActiveScreen next) {
        std::fprintf(stderr, "[nav %6lldms t=%s] pid=%d %s -> %s\n", logMs(), threadTag().c_str(),
                     static_cast<int>(::getpid()), activeScreenName(active), activeScreenName(next));
        active = next;
    };

    // A short ESC/HOME tap can't tell itself apart from the first instant
    // of a long hold (see EvdevKeyboard.cpp), so rather than guess, it shows
    // this reminder instead of navigating -- the actual exit-to-system-Home
    // on a multi-second hold is handled by the compositor/launcher, outside
    // CardMesh entirely. Matches the CardputerZero app store's own wording.
    // lv_layer_top() stays on top across every lv_screen_load() call below,
    // so one object here works for every screen.
    lv_obj_t* homeHintToast = lv_label_create(lv_layer_top());
    lv_label_set_text(homeHintToast, "Hold ESC for 3s to exit.");
    lv_obj_set_style_bg_color(homeHintToast, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(homeHintToast, LV_OPA_80, 0);
    lv_obj_set_style_text_color(homeHintToast, lv_color_white(), 0);
    lv_obj_set_style_pad_all(homeHintToast, 6, 0);
    lv_obj_set_style_radius(homeHintToast, 4, 0);
    lv_obj_align(homeHintToast, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(homeHintToast, LV_OBJ_FLAG_HIDDEN);
    std::chrono::steady_clock::time_point homeHintHideAt{};

    // Which channel each direct-LoRa screen currently sends on; cycled with
    // Left/Right when not composing (see PanLeft/PanRight below).
    // MeshMonitor-relayed sends use NetworkWorker's own primary-channel
    // detection instead (see NetworkWorker::pollOnce), so RelayMessages has
    // no channel picker.
    std::string selectedMeshtasticChannel = "MediumFast";
    std::string selectedMeshCoreChannel = "Public";

    auto cycleChannel = [](std::vector<std::string> names, const std::string& current, int direction) {
        if (names.empty()) {
            return current;
        }
        auto it = std::find(names.begin(), names.end(), current);
        std::size_t index = (it != names.end()) ? static_cast<std::size_t>(it - names.begin()) : 0;
        index = (index + names.size() + static_cast<std::size_t>(direction)) % names.size();
        return names[index];
    };

    // Shared by both the Home screen's tiles and the Dashboard's own
    // direct-protocol hotkeys ('C'/'K') -- idempotent, so it's safe to call
    // regardless of which worker was already running.
    auto enterMeshtastic = [&]() {
        std::fprintf(stderr, "[nav %6lldms t=%s] enterMeshtastic() enter\n", logMs(), threadTag().c_str());
        if (meshCoreWorker.isRunning()) {
            meshCoreWorker.stop();
        }
        if (!meshtasticWorker.isRunning()) {
            meshtasticWorker.start();
        }
        messages.setChannelName(selectedMeshtasticChannel);
        setActive(ActiveScreen::Messages);
        lv_screen_load(messages.screenObj());
        std::fprintf(stderr, "[nav %6lldms t=%s] enterMeshtastic() return\n", logMs(), threadTag().c_str());
    };
    auto enterMeshCore = [&]() {
        std::fprintf(stderr, "[nav %6lldms t=%s] enterMeshCore() enter\n", logMs(), threadTag().c_str());
        if (meshtasticWorker.isRunning()) {
            meshtasticWorker.stop();
        }
        if (!meshCoreWorker.isRunning()) {
            meshCoreWorker.start();
        }
        meshCoreMessages.setChannelName(selectedMeshCoreChannel);
        setActive(ActiveScreen::MeshCore);
        lv_screen_load(meshCoreMessages.screenObj());
        std::fprintf(stderr, "[nav %6lldms t=%s] enterMeshCore() return\n", logMs(), threadTag().c_str());
    };
    // Shared by the global 'U' shortcut and the Settings screen's "Update"
    // row, so the two can't drift apart.
    auto triggerUpdateCheckOrInstall = [&]() {
        bool available = false;
        {
            std::lock_guard<std::mutex> lock(updateModel.mutex);
            available = updateModel.checked && updateModel.updateAvailable;
        }
        if (available) {
            updateWorker.installNow();
        } else {
            updateWorker.checkNow();
        }
    };

    auto enterSettings = [&]() {
        settingsScreen.setVersionText(cardmesh::kVersion);
        settingsScreen.setMeshtasticStatusText(meshtasticWorker.isRunning() ? "Active" : "Inactive");
        settingsScreen.setMeshCoreStatusText(meshCoreWorker.isRunning() ? "Active" : "Inactive");
        settingsScreen.renderValues(mutableSettings);
        setActive(ActiveScreen::Settings);
        lv_screen_load(settingsScreen.screenObj());
    };

    // Applies a text/int field edit (toggles are flipped immediately
    // elsewhere, never routed through here) and persists it right away,
    // same as every other settings change in this file.
    auto commitSettingsField = [&](cardmesh::ui::SettingsField field, const std::string& value) {
        switch (field) {
            case cardmesh::ui::SettingsField::Server:
                mutableSettings.server = value;
                break;
            case cardmesh::ui::SettingsField::Port:
                try {
                    mutableSettings.port = std::stoi(value);
                } catch (...) {
                    // invalid input (empty, non-numeric) -- keep the old port
                }
                break;
            case cardmesh::ui::SettingsField::Token:
                mutableSettings.apiToken = value;
                break;
            case cardmesh::ui::SettingsField::TileServer:
                mutableSettings.tileServerTemplate = value;
                break;
            default:
                break;  // Https/AutoUpdate/HomeIsDashboard are toggles, never reach here
        }
        cardmesh::storage::Settings::save(mutableSettings);
        settingsScreen.renderValues(mutableSettings);
    };

    // Composing is handled separately from the GlobalKey shortcut switch
    // below: while composing, every printable key is message text, not a
    // shortcut (typing 'm' must not open the Map). See EvdevKeyboard::poll
    // vs. pollText. Shared across both message screens -- `active` says
    // which one, since only one can be composing at a time.
    bool composing = false;
    std::string composeBuffer;
    // Which Settings field is being edited, meaningful only while
    // `composing` is true and `active == ActiveScreen::Settings`.
    cardmesh::ui::SettingsField editingSettingsField = cardmesh::ui::SettingsField::Server;
    // Last updateModel.statusMessage rendered into the Settings screen's
    // Update row -- lets that per-tick poll skip re-rendering when nothing
    // changed, same pattern as DashboardScreen's own lastUpdateStatusLine_.
    std::string lastSettingsUpdateStatus;

    // Continuous once-a-second logging isn't appropriate for normal
    // production operation (disk writes forever, log spam) -- opt in with
    // CARDMESH_DEBUG_HEARTBEAT=1 when diagnosing a hang. The event-driven
    // [nav]/[worker] logs above stay on unconditionally since they only
    // fire on real transitions and are cheap and useful for crash triage.
    const bool debugHeartbeat = envOr("CARDMESH_DEBUG_HEARTBEAT", "") == "1";

    bool running = true;
    long long lastHeartbeatMs = logMs();
    while (running) {
        // A gap between consecutive heartbeat lines wider than the sleep
        // bound below means the main thread itself stalled somewhere in
        // this iteration (lv_timer_handler(), a key handler, or the
        // Dashboard/Map/Messages refresh calls) -- as opposed to a worker
        // thread being slow, which wouldn't affect this loop at all since
        // workers run on their own std::thread.
        if (debugHeartbeat) {
            const long long heartbeatNowMs = logMs();
            if (heartbeatNowMs - lastHeartbeatMs >= 1000) {
                std::fprintf(stderr, "[heartbeat %6lldms t=%s] pid=%d active=%s\n", heartbeatNowMs, threadTag().c_str(),
                             static_cast<int>(::getpid()), activeScreenName(active));
                lastHeartbeatMs = heartbeatNowMs;
            }
        }

        const std::uint32_t sleepMs = lv_timer_handler();

        {
            std::lock_guard<std::mutex> lock(updateModel.mutex);
            std::lock_guard<std::mutex> modelLock(model.mutex);
            model.updateStatusLine = updateModel.statusMessage;
        }

        if (active == ActiveScreen::Home) {
            bool meshMonitorConnected;
            {
                std::lock_guard<std::mutex> lock(model.mutex);
                meshMonitorConnected = model.connected;
            }
            home.refresh(meshtasticWorker.isRunning(), meshCoreWorker.isRunning(), meshMonitorConnected);
        } else if (active == ActiveScreen::Dashboard) {
            dashboard.refresh();
        } else if (active == ActiveScreen::Map) {
            map.refresh();
        } else if (active == ActiveScreen::Messages) {
            messages.refresh();
        } else if (active == ActiveScreen::MeshCore) {
            meshCoreMessages.refresh();
        } else if (active == ActiveScreen::Settings) {
            // Everything else is static until the user edits/toggles it,
            // which already re-renders explicitly -- but the Update row
            // reflects updateModel, which changes asynchronously on its own
            // worker thread (see UpdateWorker::run), so it needs polling
            // same as Dashboard's updateStatusLine above.
            std::string statusMessage;
            {
                std::lock_guard<std::mutex> lock(updateModel.mutex);
                statusMessage = updateModel.statusMessage;
            }
            if (statusMessage != lastSettingsUpdateStatus) {
                lastSettingsUpdateStatus = statusMessage;
                settingsScreen.setUpdateStatusText(statusMessage.empty() ? "Not checked yet" : statusMessage);
                settingsScreen.renderValues(mutableSettings);
            }
        } else {
            relayMessages.refresh();
        }

        if (composing) {
            // Deliberately not a `continue` -- it would skip the sleep_for
            // at the bottom of this loop and busy-spin the CPU while typing.
            const cardmesh::ui::TextInputEvent textEvent = keyboard.pollText();
            if (!textEvent.isEmpty()) {
                if (textEvent.cancelPressed) {
                    composing = false;
                    composeBuffer.clear();
                    if (active == ActiveScreen::Messages) messages.setComposing(false);
                    else if (active == ActiveScreen::MeshCore) meshCoreMessages.setComposing(false);
                    else if (active == ActiveScreen::Settings) settingsScreen.setEditing(false);
                    else relayMessages.setComposing(false);
                } else if (textEvent.enterPressed) {
                    if (active == ActiveScreen::Settings) {
                        commitSettingsField(editingSettingsField, composeBuffer);
                        settingsScreen.setEditing(false);
                    } else {
                        if (!composeBuffer.empty()) {
                            if (active == ActiveScreen::Messages) {
                                meshtasticWorker.sendText(selectedMeshtasticChannel, composeBuffer);
                            } else if (active == ActiveScreen::MeshCore) {
                                meshCoreWorker.sendText(selectedMeshCoreChannel, composeBuffer);
                            } else {
                                worker.queueSendMessage(composeBuffer);
                            }
                        }
                        if (active == ActiveScreen::Messages) messages.setComposing(false);
                        else if (active == ActiveScreen::MeshCore) meshCoreMessages.setComposing(false);
                        else relayMessages.setComposing(false);
                    }
                    composing = false;
                    composeBuffer.clear();
                } else {
                    if (textEvent.backspace && !composeBuffer.empty()) {
                        composeBuffer.pop_back();
                    }
                    composeBuffer += textEvent.chars;
                    if (active == ActiveScreen::Messages) messages.setComposeText(composeBuffer);
                    else if (active == ActiveScreen::MeshCore) meshCoreMessages.setComposeText(composeBuffer);
                    else if (active == ActiveScreen::Settings) settingsScreen.setEditingText(composeBuffer);
                    else relayMessages.setComposeText(composeBuffer);
                }
            }
        } else {
        switch (keyboard.poll()) {
            case cardmesh::ui::GlobalKey::Refresh:
                worker.requestRefreshNow();
                break;
            case cardmesh::ui::GlobalKey::Quit:
                running = false;
                break;
            case cardmesh::ui::GlobalKey::ToggleMap:
                if (active == ActiveScreen::Dashboard) {
                    setActive(ActiveScreen::Map);
                    lv_screen_load(map.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::ToggleRelayMessages:
                if (active == ActiveScreen::Dashboard) {
                    setActive(ActiveScreen::RelayMessages);
                    lv_screen_load(relayMessages.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::ToggleMeshCore:
                if (active == ActiveScreen::Dashboard) {
                    enterMeshCore();
                }
                break;
            case cardmesh::ui::GlobalKey::StartCompose:
                if (active == ActiveScreen::Home) {
                    switch (home.selected()) {
                        case cardmesh::ui::HomeTile::Meshtastic:
                            enterMeshtastic();
                            break;
                        case cardmesh::ui::HomeTile::MeshCore:
                            enterMeshCore();
                            break;
                        case cardmesh::ui::HomeTile::MeshMonitor:
                            setActive(ActiveScreen::Dashboard);
                            lv_screen_load(dashboard.screenObj());
                            break;
                        case cardmesh::ui::HomeTile::Settings:
                            enterSettings();
                            break;
                    }
                } else if (active == ActiveScreen::Settings) {
                    const cardmesh::ui::SettingsField field = settingsScreen.selected();
                    const cardmesh::ui::SettingsFieldKind kind = cardmesh::ui::SettingsScreen::kindOf(field);
                    if (kind == cardmesh::ui::SettingsFieldKind::Toggle) {
                        switch (field) {
                            case cardmesh::ui::SettingsField::Https:
                                mutableSettings.https = !mutableSettings.https;
                                break;
                            case cardmesh::ui::SettingsField::AutoUpdate:
                                mutableSettings.autoUpdateCheck = !mutableSettings.autoUpdateCheck;
                                break;
                            case cardmesh::ui::SettingsField::HomeIsDashboard:
                                mutableSettings.homeScreenIsDashboard = !mutableSettings.homeScreenIsDashboard;
                                break;
                            default:
                                break;
                        }
                        cardmesh::storage::Settings::save(mutableSettings);
                        settingsScreen.renderValues(mutableSettings);
                    } else if (kind == cardmesh::ui::SettingsFieldKind::Cycle) {
                        if (field == cardmesh::ui::SettingsField::LoraRegion) {
                            mutableSettings.loraRegion = cardmesh::mesh::nextLoraRegion(mutableSettings.loraRegion);
                        }
                        cardmesh::storage::Settings::save(mutableSettings);
                        settingsScreen.renderValues(mutableSettings);
                    } else if (kind == cardmesh::ui::SettingsFieldKind::Action) {
                        triggerUpdateCheckOrInstall();
                    } else if (kind == cardmesh::ui::SettingsFieldKind::Info) {
                        // Version is display-only -- nothing to do.
                    } else {
                        editingSettingsField = field;
                        composeBuffer = cardmesh::ui::SettingsScreen::editableValue(mutableSettings, field);
                        composing = true;
                        settingsScreen.setEditing(true);
                        settingsScreen.setEditingText(composeBuffer);
                    }
                } else if (active == ActiveScreen::Messages || active == ActiveScreen::RelayMessages ||
                    active == ActiveScreen::MeshCore) {
                    composing = true;
                    composeBuffer.clear();
                    if (active == ActiveScreen::Messages) {
                        messages.setComposing(true);
                        messages.setComposeText("");
                    } else if (active == ActiveScreen::MeshCore) {
                        meshCoreMessages.setComposing(true);
                        meshCoreMessages.setComposeText("");
                    } else {
                        relayMessages.setComposing(true);
                        relayMessages.setComposeText("");
                    }
                }
                break;
            case cardmesh::ui::GlobalKey::Back:
                if (active == ActiveScreen::Map || active == ActiveScreen::Messages ||
                    active == ActiveScreen::RelayMessages || active == ActiveScreen::MeshCore ||
                    active == ActiveScreen::Settings) {
                    if (active == ActiveScreen::MeshCore) {
                        // Hand the radio back to direct Meshtastic.
                        meshCoreWorker.stop();
                        meshtasticWorker.start();
                    }
                    // Map/RelayMessages are only reachable from the
                    // Dashboard regardless of what's "home" (see
                    // ToggleMap/ToggleRelayMessages above), so they always
                    // return there; Messages/MeshCore/Settings return to
                    // whichever screen is actually home.
                    if ((active == ActiveScreen::Map || active == ActiveScreen::RelayMessages) ||
                        !homeIsIconGrid) {
                        setActive(ActiveScreen::Dashboard);
                        lv_screen_load(dashboard.screenObj());
                    } else {
                        setActive(ActiveScreen::Home);
                        lv_screen_load(home.screenObj());
                    }
                } else if (active == ActiveScreen::Dashboard && homeIsIconGrid) {
                    setActive(ActiveScreen::Home);
                    lv_screen_load(home.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::PanUp:
                if (active == ActiveScreen::Map) map.panUp();
                else if (active == ActiveScreen::Settings) settingsScreen.moveUp();
                break;
            case cardmesh::ui::GlobalKey::PanDown:
                if (active == ActiveScreen::Map) map.panDown();
                else if (active == ActiveScreen::Settings) settingsScreen.moveDown();
                break;
            case cardmesh::ui::GlobalKey::PanLeft:
                if (active == ActiveScreen::Map) {
                    map.panLeft();
                } else if (active == ActiveScreen::Home) {
                    home.moveLeft();
                } else if (active == ActiveScreen::Messages) {
                    selectedMeshtasticChannel = cycleChannel(meshtasticWorker.channelNames(), selectedMeshtasticChannel, -1);
                    messages.setChannelName(selectedMeshtasticChannel);
                } else if (active == ActiveScreen::MeshCore) {
                    selectedMeshCoreChannel = cycleChannel(meshCoreWorker.channelNames(), selectedMeshCoreChannel, -1);
                    meshCoreMessages.setChannelName(selectedMeshCoreChannel);
                }
                break;
            case cardmesh::ui::GlobalKey::PanRight:
                if (active == ActiveScreen::Map) {
                    map.panRight();
                } else if (active == ActiveScreen::Home) {
                    home.moveRight();
                } else if (active == ActiveScreen::Messages) {
                    selectedMeshtasticChannel = cycleChannel(meshtasticWorker.channelNames(), selectedMeshtasticChannel, 1);
                    messages.setChannelName(selectedMeshtasticChannel);
                } else if (active == ActiveScreen::MeshCore) {
                    selectedMeshCoreChannel = cycleChannel(meshCoreWorker.channelNames(), selectedMeshCoreChannel, 1);
                    meshCoreMessages.setChannelName(selectedMeshCoreChannel);
                }
                break;
            case cardmesh::ui::GlobalKey::ZoomIn:
                if (active == ActiveScreen::Map) map.zoomIn();
                break;
            case cardmesh::ui::GlobalKey::ZoomOut:
                if (active == ActiveScreen::Map) map.zoomOut();
                break;
            case cardmesh::ui::GlobalKey::CheckForUpdate:
                triggerUpdateCheckOrInstall();
                break;
            case cardmesh::ui::GlobalKey::ToggleAutoUpdate:
                mutableSettings.autoUpdateCheck = !mutableSettings.autoUpdateCheck;
                cardmesh::storage::Settings::save(mutableSettings);
                {
                    std::lock_guard<std::mutex> lock(updateModel.mutex);
                    updateModel.statusMessage =
                        mutableSettings.autoUpdateCheck ? "Auto-update: ON" : "Auto-update: OFF";
                }
                break;
            case cardmesh::ui::GlobalKey::OpenSettings:
                if (active == ActiveScreen::Home || active == ActiveScreen::Dashboard) {
                    enterSettings();
                }
                break;
            case cardmesh::ui::GlobalKey::GoHome:
                // Works from anywhere, independent of whichever screen is
                // "home" at boot -- unlike Back, this always lands on the
                // icon grid itself, not the Dashboard.
                if (active != ActiveScreen::Home) {
                    if (active == ActiveScreen::MeshCore) {
                        // Hand the radio back to direct Meshtastic, same as Back.
                        meshCoreWorker.stop();
                        meshtasticWorker.start();
                    }
                    setActive(ActiveScreen::Home);
                    lv_screen_load(home.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::ShowHomeHint:
                lv_obj_clear_flag(homeHintToast, LV_OBJ_FLAG_HIDDEN);
                homeHintHideAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                break;
            case cardmesh::ui::GlobalKey::None:
                break;
        }
        }

        if (!lv_obj_has_flag(homeHintToast, LV_OBJ_FLAG_HIDDEN) &&
            std::chrono::steady_clock::now() >= homeHintHideAt) {
            lv_obj_add_flag(homeHintToast, LV_OBJ_FLAG_HIDDEN);
        }
        // lv_timer_handler() can return LV_NO_TIMER_READY (UINT32_MAX) when no
        // LVGL timer has pending work; treating that as a literal sleep
        // duration would block the loop (and keyboard/network polling) for
        // ~49 days. Cap the delay so this loop keeps running regardless.
        constexpr std::uint32_t kMaxSleepMs = 50;
        const std::uint32_t clampedSleepMs =
            (sleepMs > 0 && sleepMs <= kMaxSleepMs) ? sleepMs : kMaxSleepMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(clampedSleepMs));
    }

    tileFetcher.stop();
    worker.stop();
    meshtasticWorker.stop();
    meshCoreWorker.stop();
    updateWorker.stop();
    return EXIT_SUCCESS;
}
