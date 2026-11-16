// CardputerZero device entry point (LVGL + Linux framebuffer/evdev).
//
// This is the executable packaged as `cardmesh` in the arm64 .deb. It has
// NOT been run against real CardputerZero hardware or the online emulator —
// see docs/DEVICE_BUILD.md for exactly what is and isn't verified.

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

#include "lvgl.h"

#include "Version.h"
#include "map/TileFetcher.h"
#include "storage/Settings.h"
#include "ui/DashboardModel.h"
#include "ui/DashboardScreen.h"
#include "ui/EvdevKeyboard.h"
#include "ui/LoraMessagesModel.h"
#include "ui/LoraMessagesScreen.h"
#include "ui/MapModel.h"
#include "ui/MapScreen.h"
#include "ui/MeshtasticWorker.h"
#include "ui/NetworkWorker.h"
#include "update/UpdateWorker.h"
#include "mesh/ChannelKey.h"
#include "mesh/MeshPacketCodec.h"

namespace {

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

enum class ActiveScreen { Dashboard, Map, Messages };

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
    cardmesh::ui::DashboardScreen dashboard(model);
    cardmesh::ui::MapScreen map(mapModel, tileFetcher);
    cardmesh::ui::LoraMessagesScreen messages(loraMessages);

    // Independent of NetworkWorker/MeshMonitor by design -- both mesh paths
    // (REST-relayed and direct over the onboard LoRa hat) run concurrently.
    cardmesh::ui::MeshtasticWorker meshtasticWorker(mutableSettings, loraMessages, mapModel);
    meshtasticWorker.start();

    cardmesh::ui::NetworkWorker worker(*settings, model, mapModel);
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

    ActiveScreen active = ActiveScreen::Dashboard;
    lv_screen_load(dashboard.screenObj());

    // Composing is handled separately from the GlobalKey shortcut switch
    // below: while composing, every printable key is message text, not a
    // shortcut (typing 'm' must not open the Map). See EvdevKeyboard::poll
    // vs. pollText.
    //
    // MVP scope: always sends on the public "MediumFast" channel -- there's
    // no channel picker yet, so a real per-channel PSK learned via
    // onChannelsUpdated above is only used for *decoding* incoming traffic,
    // not for choosing where outgoing messages go.
    bool composing = false;
    std::string composeBuffer;

    bool running = true;
    while (running) {
        const std::uint32_t sleepMs = lv_timer_handler();

        {
            std::lock_guard<std::mutex> lock(updateModel.mutex);
            std::lock_guard<std::mutex> modelLock(model.mutex);
            model.updateStatusLine = updateModel.statusMessage;
        }

        if (active == ActiveScreen::Dashboard) {
            dashboard.refresh();
        } else if (active == ActiveScreen::Map) {
            map.refresh();
        } else {
            messages.refresh();
        }

        if (composing) {
            // Deliberately not a `continue` -- it would skip the sleep_for
            // at the bottom of this loop and busy-spin the CPU while typing.
            const cardmesh::ui::TextInputEvent textEvent = keyboard.pollText();
            if (!textEvent.isEmpty()) {
                if (textEvent.cancelPressed) {
                    composing = false;
                    composeBuffer.clear();
                    messages.setComposing(false);
                } else if (textEvent.enterPressed) {
                    if (!composeBuffer.empty()) {
                        meshtasticWorker.sendText("MediumFast", composeBuffer);
                    }
                    composing = false;
                    composeBuffer.clear();
                    messages.setComposing(false);
                } else {
                    if (textEvent.backspace && !composeBuffer.empty()) {
                        composeBuffer.pop_back();
                    }
                    composeBuffer += textEvent.chars;
                    messages.setComposeText(composeBuffer);
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
            case cardmesh::ui::GlobalKey::ToggleMessages:
                if (active == ActiveScreen::Dashboard) {
                    active = ActiveScreen::Messages;
                    lv_screen_load(messages.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::StartCompose:
                if (active == ActiveScreen::Messages) {
                    composing = true;
                    composeBuffer.clear();
                    messages.setComposing(true);
                    messages.setComposeText("");
                }
                break;
            case cardmesh::ui::GlobalKey::Back:
                if (active == ActiveScreen::Map || active == ActiveScreen::Messages) {
                    active = ActiveScreen::Dashboard;
                    lv_screen_load(dashboard.screenObj());
                }
                break;
            case cardmesh::ui::GlobalKey::PanUp:
                if (active == ActiveScreen::Map) map.panUp();
                break;
            case cardmesh::ui::GlobalKey::PanDown:
                if (active == ActiveScreen::Map) map.panDown();
                break;
            case cardmesh::ui::GlobalKey::PanLeft:
                if (active == ActiveScreen::Map) map.panLeft();
                break;
            case cardmesh::ui::GlobalKey::PanRight:
                if (active == ActiveScreen::Map) map.panRight();
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
    updateWorker.stop();
    return EXIT_SUCCESS;
}
