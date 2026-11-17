#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace cardmesh::storage {

struct AppSettings {
    std::string server;
    int port = 443;
    bool https = true;
    std::string apiToken;
    std::string preferredSource;

    // Defaults to OSM's own tile server, which has a usage policy
    // (https://operations.osmfoundation.org/policies/tiles/) unsuited to
    // heavy/widely-distributed use -- configurable here so a production
    // deployment can point at a self-hosted or commercial tile server
    // instead. {z}/{x}/{y} are substituted. See src/map/TileCache.h.
    std::string tileServerTemplate = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";

    // Whether CardMesh checks GitHub Releases for a newer version at
    // startup. Checking is always available on demand (the 'U' shortcut)
    // regardless of this setting -- it only controls the automatic
    // startup check. See src/update/UpdateChecker.h.
    bool autoUpdateCheck = true;

    // CardMesh's own direct-LoRa node identity (distinct from MeshMonitor's
    // own node -- MeshMonitor has no idea this second radio exists).
    // Generated once on first use and persisted here; 0 means "not yet
    // generated". See MeshtasticWorker's startup path.
    std::uint32_t loraNodeNum = 0;

    // Which screen the app lands on after the boot splash: false (default)
    // is the Home icon grid (Meshtastic/MeshCore/MeshMonitor tiles), true
    // is the Dashboard directly, matching CardMesh's pre-Home-screen
    // behavior. Toggled via the 'S' key from the Home screen; takes effect
    // on the next launch, not the current session. See src/ui_main.cpp.
    bool homeScreenIsDashboard = false;
};

// Persists connection configuration at ~/.config/cardmesh/config.json (mode 0600).
// Real MeshMonitor credentials live only in this file and must never be committed.
class Settings {
public:
    static std::string defaultConfigPath();
    static std::optional<AppSettings> load(const std::string& path = defaultConfigPath());
    static bool save(const AppSettings& settings, const std::string& path = defaultConfigPath());
};

}  // namespace cardmesh::storage
