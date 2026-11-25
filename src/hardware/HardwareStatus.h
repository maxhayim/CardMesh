#pragma once

// Real (never simulated) device hardware status reads for the Home screen's
// status row: battery, Wi-Fi, Bluetooth. Pure sysfs/procfs reads with no
// LVGL dependency, so this is unit-testable (see tests/test_hardware_status.cpp)
// without a graphics stack. Every function here supports an env var override
// (CARDMESH_POWER_SUPPLY_DIR, CARDMESH_WIFI_OPERSTATE_PATH,
// CARDMESH_WIRELESS_PROC_PATH, CARDMESH_RFKILL_DIR) for host-side testing;
// production defaults point at the real CardputerZero paths.
//
// NOT CONFIRMED ON PHYSICAL HARDWARE: every sysfs/procfs path assumed here
// is a best-effort guess (standard Linux power_supply class, standard wlan0
// naming, standard /proc/net/wireless format, standard rfkill "hci0"
// naming) -- reasonable for embedded Linux in general, but unverified for
// the real CardputerZero. See docs/DEVICE_BUILD.md. Every function here
// fails closed (returns an explicit "unavailable"/-1 state) rather than
// fabricating a value when its assumption doesn't hold.

namespace cardmesh::hardware {

// percent == -1 if no battery is present/readable. chargingKnown is false
// if the "status" sysfs attribute isn't present/readable; charging is only
// meaningful when chargingKnown is true, and is only true for the
// "Charging" status value specifically (not "Full", which is still
// plugged in but shouldn't show a charging-in-progress glyph).
struct BatteryStatus {
    int percent = -1;
    bool chargingKnown = false;
    bool charging = false;
};

BatteryStatus readBatteryStatus();

enum class WifiState { Unavailable, Disconnected, Connected };

// Unavailable: no wlan0 interface found at all (no wifi hardware/driver).
// Disconnected: the interface exists but isn't up.
// Connected: operstate == "up".
WifiState readWifiState();

// -1 if unavailable/unparseable. See HardwareStatus.cpp for why this is
// approximate (driver-dependent /proc/net/wireless scaling), not exact.
int readWifiSignalPercent();

enum class BluetoothState { Unavailable, Disabled, Enabled };

// Unavailable: no hci0 rfkill entry found at all.
// Disabled: hci0 found but soft- or hard-blocked.
// Enabled: hci0 found and not blocked.
//
// This is adapter state only (rfkill), not an active paired-connection
// indicator -- see HardwareStatus.cpp for why that's deliberately not
// implemented here rather than guessed.
BluetoothState readBluetoothState();

}  // namespace cardmesh::hardware
