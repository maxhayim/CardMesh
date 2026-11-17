// CardputerZero device entry point (LVGL + Linux framebuffer/evdev).
//
// This is the executable packaged as `cardmesh` in the arm64 .deb. It has
// NOT been run against real CardputerZero hardware or the online emulator —
// see docs/DEVICE_BUILD.md for exactly what is and isn't verified.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
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
#include "mesh/MeshPacketCodec.h"

namespace {

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

enum class ActiveScreen { Home, Dashboard, Map, Messages, RelayMessages, MeshCore, Settings };

}  // namespace

int main() {
    const auto settings = cardmesh::storage::Settings::load();
    if (!settings.has_value()) {
        std::cerr << "No configuration found at " << cardmesh::storage::Settings::defaultConfigPath()
                  << "\n";
        std::cerr << "First-run setup UI is not implemented yet; write the config file manually "
                     "(see README) and relaunch.\n";
        return EXIT_FAILURE;
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
    cardmesh::ui::MeshCoreWorker meshCoreWorker(meshCoreMessagesModel);

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
        if (meshCoreWorker.isRunning()) {
            meshCoreWorker.stop();
        }
        if (!meshtasticWorker.isRunning()) {
            meshtasticWorker.start();
        }
        messages.setChannelName(selectedMeshtasticChannel);
        active = ActiveScreen::Messages;
        lv_screen_load(messages.screenObj());
    };
    auto enterMeshCore = [&]() {
        if (meshtasticWorker.isRunning()) {
            meshtasticWorker.stop();
        }
        if (!meshCoreWorker.isRunning()) {
            meshCoreWorker.start();
        }
        meshCoreMessages.setChannelName(selectedMeshCoreChannel);
        active = ActiveScreen::MeshCore;
        lv_screen_load(meshCoreMessages.screenObj());
    };
    auto enterSettings = [&]() {
        settingsScreen.renderValues(mutableSettings);
        active = ActiveScreen::Settings;
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

    bool running = true;
    while (running) {
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
            // Static until the user edits/toggles something, which already
            // re-renders explicitly -- no per-tick polling needed.
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
                    active = ActiveScreen::Map;
                    lv_screen_load(map.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::ToggleRelayMessages:
                if (active == ActiveScreen::Dashboard) {
                    active = ActiveScreen::RelayMessages;
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
                            active = ActiveScreen::Dashboard;
                            lv_screen_load(dashboard.screenObj());
                            break;
                        case cardmesh::ui::HomeTile::Settings:
                            enterSettings();
                            break;
                    }
                } else if (active == ActiveScreen::Settings) {
                    const cardmesh::ui::SettingsField field = settingsScreen.selected();
                    if (cardmesh::ui::SettingsScreen::kindOf(field) == cardmesh::ui::SettingsFieldKind::Toggle) {
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
                        active = ActiveScreen::Dashboard;
                        lv_screen_load(dashboard.screenObj());
                    } else {
                        active = ActiveScreen::Home;
                        lv_screen_load(home.screenObj());
                    }
                } else if (active == ActiveScreen::Dashboard && homeIsIconGrid) {
                    active = ActiveScreen::Home;
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
            case cardmesh::ui::GlobalKey::CheckForUpdate: {
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
                break;
            }
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
                    active = ActiveScreen::Home;
                    lv_screen_load(home.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::None:
                break;
        }
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
