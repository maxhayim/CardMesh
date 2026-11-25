#include "HardwareStatus.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace cardmesh::hardware {

namespace {

std::string readTrimmed(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    std::getline(file, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

}  // namespace

// Scanned by `type` rather than hardcoding the `bq27220-0` chip name
// confirmed on this hardware, so a different chip still works without a
// code change.
BatteryStatus readBatteryStatus() {
    const char* overrideDir = std::getenv("CARDMESH_POWER_SUPPLY_DIR");
    const std::string root = overrideDir != nullptr ? overrideDir : "/sys/class/power_supply";
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (readTrimmed(entry.path() / "type") != "Battery") {
            continue;
        }
        BatteryStatus status;
        const std::string capacityText = readTrimmed(entry.path() / "capacity");
        if (!capacityText.empty()) {
            try {
                status.percent = std::stoi(capacityText);
            } catch (...) {
                status.percent = -1;
            }
        }
        // Standard power_supply "status" values: Charging, Discharging,
        // Full, Not charging, Unknown. Only "Charging" itself drives the
        // charge-glyph -- "Full" is still physically plugged in, but
        // showing a charging bolt once it's topped off would be wrong.
        const std::string statusText = readTrimmed(entry.path() / "status");
        if (!statusText.empty()) {
            status.chargingKnown = true;
            status.charging = statusText == "Charging";
        }
        if (status.percent >= 0 || status.chargingKnown) {
            return status;
        }
    }
    return BatteryStatus{};
}

WifiState readWifiState() {
    const char* overridePath = std::getenv("CARDMESH_WIFI_OPERSTATE_PATH");
    const std::string path = overridePath != nullptr ? overridePath : "/sys/class/net/wlan0/operstate";
    // readTrimmed() returns "" both when the file is missing (no wlan0
    // interface at all -- genuinely unavailable hardware) and can't be
    // distinguished from a present-but-empty file, but operstate is never
    // actually empty when the interface exists, so this is unambiguous in
    // practice.
    const std::string state = readTrimmed(path);
    if (state.empty()) {
        return WifiState::Unavailable;
    }
    return state == "up" ? WifiState::Connected : WifiState::Disconnected;
}

// /proc/net/wireless's "link quality" field is scaled differently by
// different wifi drivers -- most report out of a 0-70 max (the historical
// prism/hostap convention that wireless-tools/NetworkManager still assume
// by default), a handful report 0-100 directly. This picks the common
// heuristic (scale by 70 unless the raw value already exceeds it) but has
// NOT been confirmed against the real CardputerZero's wifi chip/driver --
// treat the displayed percentage as approximate pending on-device
// confirmation. Returns -1 (not displayed) if the file is missing, the
// interface has no "wlan0:" line, or the line can't be parsed.
int readWifiSignalPercent() {
    const char* overridePath = std::getenv("CARDMESH_WIRELESS_PROC_PATH");
    const std::string path = overridePath != nullptr ? overridePath : "/proc/net/wireless";
    std::ifstream file(path);
    if (!file.is_open()) {
        return -1;
    }
    std::string line;
    std::getline(file, line);  // header line 1
    std::getline(file, line);  // header line 2
    while (std::getline(file, line)) {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string ifaceName = line.substr(0, colon);
        const std::size_t start = ifaceName.find_first_not_of(" \t");
        if (start == std::string::npos || ifaceName.substr(start) != "wlan0") {
            continue;
        }
        std::istringstream rest(line.substr(colon + 1));
        int statusHex = 0;
        double linkQuality = 0.0;
        rest >> std::hex >> statusHex >> std::dec >> linkQuality;
        if (!rest) {
            return -1;
        }
        double percent = linkQuality <= 70.0 ? (linkQuality / 70.0) * 100.0 : linkQuality;
        percent = std::max(0.0, std::min(100.0, percent));
        return static_cast<int>(percent + 0.5);
    }
    return -1;
}

// rfkill device numbering isn't stable across kernel/firmware versions, so
// this matches by name ("hci0", the Bluetooth controller) rather than
// hardcoding rfkill0 (confirmed on this hardware, but not guaranteed).
//
// This distinguishes adapter enabled/disabled, which is what rfkill
// actually exposes. It deliberately does NOT claim to report an active
// Bluetooth *connection* (a paired/connected peer) -- that requires BlueZ
// over D-Bus or raw HCI socket ioctls, neither of which this project links
// against, and fabricating that state would violate the same
// no-simulated-values rule this is meant to fix. If per-connection status
// is wanted later, it needs that dependency added deliberately, not guessed
// here.
BluetoothState readBluetoothState() {
    const char* overrideDir = std::getenv("CARDMESH_RFKILL_DIR");
    const std::string root = overrideDir != nullptr ? overrideDir : "/sys/class/rfkill";
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (readTrimmed(entry.path() / "name") != "hci0") {
            continue;
        }
        const bool blocked =
            readTrimmed(entry.path() / "soft") != "0" || readTrimmed(entry.path() / "hard") != "0";
        return blocked ? BluetoothState::Disabled : BluetoothState::Enabled;
    }
    return BluetoothState::Unavailable;
}

}  // namespace cardmesh::hardware
