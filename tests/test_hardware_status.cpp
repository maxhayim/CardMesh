#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "TestRunner.h"
#include "hardware/HardwareStatus.h"

using cardmesh::hardware::BatteryStatus;
using cardmesh::hardware::BluetoothState;
using cardmesh::hardware::WifiState;
using cardmesh::hardware::readBatteryStatus;
using cardmesh::hardware::readBluetoothState;
using cardmesh::hardware::readWifiSignalPercent;
using cardmesh::hardware::readWifiState;

namespace {

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
}

void setEnv(const char* name, const std::filesystem::path& value) { setenv(name, value.string().c_str(), 1); }

void unsetEnvAll() {
    unsetenv("CARDMESH_WIFI_OPERSTATE_PATH");
    unsetenv("CARDMESH_WIRELESS_PROC_PATH");
    unsetenv("CARDMESH_RFKILL_DIR");
    unsetenv("CARDMESH_POWER_SUPPLY_DIR");
}

}  // namespace

CARDMESH_TEST_MAIN_BEGIN()

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cardmesh_test_hardware_status";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    // --- Wi-Fi ---

    {
        const auto path = root / "wifi_up";
        writeFile(path, "up\n");
        setEnv("CARDMESH_WIFI_OPERSTATE_PATH", path);
        CARDMESH_CHECK(readWifiState() == WifiState::Connected);
    }
    {
        const auto path = root / "wifi_down";
        writeFile(path, "down\n");
        setEnv("CARDMESH_WIFI_OPERSTATE_PATH", path);
        CARDMESH_CHECK(readWifiState() == WifiState::Disconnected);
    }
    {
        // Deliberately nonexistent -- no wlan0 interface at all.
        setEnv("CARDMESH_WIFI_OPERSTATE_PATH", root / "wifi_does_not_exist");
        CARDMESH_CHECK(readWifiState() == WifiState::Unavailable);
    }
    unsetenv("CARDMESH_WIFI_OPERSTATE_PATH");

    {
        const auto path = root / "wireless_good";
        writeFile(path,
                   "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
                   " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
                   "wlan0: 0000   54.  -52.  -256        0      0      0      0      0        0\n");
        setEnv("CARDMESH_WIRELESS_PROC_PATH", path);
        // 54/70 * 100 = 77.1... rounds to 77.
        CARDMESH_CHECK(readWifiSignalPercent() == 77);
    }
    {
        // No wlan0 line present.
        const auto path = root / "wireless_no_wlan0";
        writeFile(path,
                   "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
                   " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n");
        setEnv("CARDMESH_WIRELESS_PROC_PATH", path);
        CARDMESH_CHECK(readWifiSignalPercent() == -1);
    }
    {
        setEnv("CARDMESH_WIRELESS_PROC_PATH", root / "wireless_does_not_exist");
        CARDMESH_CHECK(readWifiSignalPercent() == -1);
    }
    unsetenv("CARDMESH_WIRELESS_PROC_PATH");

    // --- Bluetooth ---

    {
        const auto dir = root / "rfkill_enabled" / "rfkill0";
        writeFile(dir / "name", "hci0");
        writeFile(dir / "soft", "0");
        writeFile(dir / "hard", "0");
        setEnv("CARDMESH_RFKILL_DIR", root / "rfkill_enabled");
        CARDMESH_CHECK(readBluetoothState() == BluetoothState::Enabled);
    }
    {
        const auto dir = root / "rfkill_soft_blocked" / "rfkill0";
        writeFile(dir / "name", "hci0");
        writeFile(dir / "soft", "1");
        writeFile(dir / "hard", "0");
        setEnv("CARDMESH_RFKILL_DIR", root / "rfkill_soft_blocked");
        CARDMESH_CHECK(readBluetoothState() == BluetoothState::Disabled);
    }
    {
        const auto dir = root / "rfkill_hard_blocked" / "rfkill0";
        writeFile(dir / "name", "hci0");
        writeFile(dir / "soft", "0");
        writeFile(dir / "hard", "1");
        setEnv("CARDMESH_RFKILL_DIR", root / "rfkill_hard_blocked");
        CARDMESH_CHECK(readBluetoothState() == BluetoothState::Disabled);
    }
    {
        // Directory exists but has no hci0 entry -- e.g. only a wifi rfkill.
        const auto dir = root / "rfkill_no_hci0" / "rfkill0";
        writeFile(dir / "name", "phy0");
        writeFile(dir / "soft", "0");
        writeFile(dir / "hard", "0");
        setEnv("CARDMESH_RFKILL_DIR", root / "rfkill_no_hci0");
        CARDMESH_CHECK(readBluetoothState() == BluetoothState::Unavailable);
    }
    {
        setEnv("CARDMESH_RFKILL_DIR", root / "rfkill_does_not_exist");
        CARDMESH_CHECK(readBluetoothState() == BluetoothState::Unavailable);
    }
    unsetenv("CARDMESH_RFKILL_DIR");

    // --- Battery ---

    {
        const auto dir = root / "power_supply_charging" / "battery";
        writeFile(dir / "type", "Battery");
        writeFile(dir / "capacity", "62");
        writeFile(dir / "status", "Charging");
        setEnv("CARDMESH_POWER_SUPPLY_DIR", root / "power_supply_charging");
        const BatteryStatus status = readBatteryStatus();
        CARDMESH_CHECK(status.percent == 62);
        CARDMESH_CHECK(status.chargingKnown);
        CARDMESH_CHECK(status.charging);
    }
    {
        const auto dir = root / "power_supply_discharging" / "battery";
        writeFile(dir / "type", "Battery");
        writeFile(dir / "capacity", "35");
        writeFile(dir / "status", "Discharging");
        setEnv("CARDMESH_POWER_SUPPLY_DIR", root / "power_supply_discharging");
        const BatteryStatus status = readBatteryStatus();
        CARDMESH_CHECK(status.percent == 35);
        CARDMESH_CHECK(status.chargingKnown);
        CARDMESH_CHECK(!status.charging);
    }
    {
        // "Full" is plugged in but shouldn't show the charging glyph.
        const auto dir = root / "power_supply_full" / "battery";
        writeFile(dir / "type", "Battery");
        writeFile(dir / "capacity", "100");
        writeFile(dir / "status", "Full");
        setEnv("CARDMESH_POWER_SUPPLY_DIR", root / "power_supply_full");
        const BatteryStatus status = readBatteryStatus();
        CARDMESH_CHECK(status.percent == 100);
        CARDMESH_CHECK(status.chargingKnown);
        CARDMESH_CHECK(!status.charging);
    }
    {
        // No power_supply directory at all -- e.g. this host, or a device
        // without a standard Linux battery class.
        setEnv("CARDMESH_POWER_SUPPLY_DIR", root / "power_supply_does_not_exist");
        const BatteryStatus status = readBatteryStatus();
        CARDMESH_CHECK(status.percent == -1);
        CARDMESH_CHECK(!status.chargingKnown);
    }
    {
        // Directory exists but has no entry of type "Battery" (e.g. only a
        // USB/Mains supply node).
        const auto dir = root / "power_supply_no_battery" / "usb";
        writeFile(dir / "type", "USB");
        setEnv("CARDMESH_POWER_SUPPLY_DIR", root / "power_supply_no_battery");
        const BatteryStatus status = readBatteryStatus();
        CARDMESH_CHECK(status.percent == -1);
        CARDMESH_CHECK(!status.chargingKnown);
    }
    unsetEnvAll();

    std::filesystem::remove_all(root);

CARDMESH_TEST_MAIN_END()
