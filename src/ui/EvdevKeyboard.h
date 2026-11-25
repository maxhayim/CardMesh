#pragma once

#include <string>

namespace cardmesh::ui {

enum class GlobalKey {
    None,
    Refresh,
    Quit,
    ToggleMap,  // 'M' from the Dashboard: open the Map screen
    ToggleRelayMessages,  // 'T' from the Dashboard: open the MeshMonitor-relayed messages ("Text") screen
    ToggleMeshCore,  // 'K' from the Dashboard: open the direct-MeshCore screen (swaps the radio off Meshtastic)
    StartCompose,  // Enter: start typing a message (Messages screens), or open the highlighted tile (Home)
    Back,       // Esc/'B': return to the Dashboard/Home, depending on screen and context
    // This hardware has no dedicated arrow keys -- these fire on bare
    // F/Z/X/C (the device's documented no-fn arrow recommendation) as well
    // as real KEY_UP/DOWN/LEFT/RIGHT, should those ever arrive from some
    // other input source. PanLeft/PanRight also move the Home screen's tile
    // cursor. Direct-LoRa chat ("Chat", formerly plain 'C') is reachable via
    // the Home screen's Meshtastic tile instead, since plain 'C' is now
    // PanRight.
    PanUp,
    PanDown,
    PanLeft,
    PanRight,
    ZoomIn,
    ZoomOut,
    CheckForUpdate,   // 'U' from the Dashboard: check now, or install if one was already found
    ToggleAutoUpdate,  // 'A' from the Dashboard: toggle automatic startup update checks
    OpenSettings,  // 'S' from the Home screen or Dashboard: open the Settings screen
    GoHome,  // A real KEY_HOME event, should one ever arrive: jump to the icon-grid Home screen from anywhere
    // The physical ESC/HOME key (confirmed via a live /dev/input/event3
    // capture to send plain KEY_ESC, never KEY_HOME, on both a short tap
    // and a multi-second hold -- see EvdevKeyboard.cpp) matches the
    // platform's own convention, also used by the CardputerZero app store:
    // a short tap shows a reminder toast rather than acting immediately,
    // since the real "exit to system Home" gesture is a multi-second hold
    // handled by the compositor/launcher itself, outside any app's control.
    ShowHomeHint,
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
