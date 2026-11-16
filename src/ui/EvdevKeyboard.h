#pragma once

#include <string>

namespace cardmesh::ui {

enum class GlobalKey {
    None,
    Refresh,
    Quit,
    ToggleMap,  // 'M' from the Dashboard: open the Map screen
    ToggleMessages,  // 'C' from the Dashboard: open the direct-LoRa Messages ("Chat") screen
    ToggleRelayMessages,  // 'T' from the Dashboard: open the MeshMonitor-relayed messages ("Text") screen
    StartCompose,  // Enter from a Messages screen (not yet composing): start typing a message
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

// Result of a pollText() call: the printable characters typed since the
// last call (usually 0 or 1, but a fast typist can outrun one tick), plus
// edit/submit signals.
struct TextInputEvent {
    std::string chars;
    bool backspace = false;
    bool enterPressed = false;
    bool cancelPressed = false;

    bool isEmpty() const { return chars.empty() && !backspace && !enterPressed && !cancelPressed; }
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
    // Only call this OR pollText() per tick, never both -- each drains the
    // same underlying evdev read loop. Call poll() while not composing a
    // message; switch to pollText() for the duration of composing it.
    GlobalKey poll();

    // Non-blocking: captures raw text input instead of global shortcuts
    // (e.g. typing 'm' while composing a message must not open the Map).
    // Letters are case-shifted via Shift's held state; digits, space, and
    // basic punctuation are supported; the shifted symbol row (!@#...) is
    // not yet. Esc cancels, Enter submits, Backspace deletes.
    TextInputEvent pollText();

private:
    int fd_ = -1;
    bool shiftHeld_ = false;
};

}  // namespace cardmesh::ui
