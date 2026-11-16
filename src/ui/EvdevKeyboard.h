#pragma once

#include <string>

namespace cardmesh::ui {

enum class GlobalKey {
    None,
    Refresh,
    Quit,
    ToggleMap,  // 'M' from the Dashboard: open the Map screen
    ToggleMessages,  // 'C' from the Dashboard: open the direct-LoRa Messages ("Chat") screen
    Back,       // Esc/'B' from the Map/Messages screen: return to the Dashboard
    PanUp,
    PanDown,
    PanLeft,
    PanRight,
    ZoomIn,
    ZoomOut,
    CheckForUpdate,   // 'U' from the Dashboard: check now, or install if one was already found
    ToggleAutoUpdate,  // 'A' from the Dashboard: toggle automatic startup update checks
};

// Minimal, non-blocking reader for global keyboard shortcuts on a Linux
// evdev device (e.g. /dev/input/event0). Deliberately bypasses LVGL's
// focus/group indev system since the MVP dashboard has no focusable
// widgets — see lv_conf.h for that tradeoff.
//
// ASSUMPTION: the device path is not confirmed against real CardputerZero
// hardware; override via the CARDMESH_INPUT_DEVICE environment variable if
// /dev/input/event0 is wrong for the target.
class EvdevKeyboard {
public:
    explicit EvdevKeyboard(const std::string& devicePath);
    ~EvdevKeyboard();

    EvdevKeyboard(const EvdevKeyboard&) = delete;
    EvdevKeyboard& operator=(const EvdevKeyboard&) = delete;

    bool isOpen() const;

    // Non-blocking: returns GlobalKey::None if nothing new was pressed.
    GlobalKey poll();

private:
    int fd_ = -1;
};

}  // namespace cardmesh::ui
