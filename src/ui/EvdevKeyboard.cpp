#include "EvdevKeyboard.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace cardmesh::ui {

namespace {

// Translates a subset of evdev keycodes to a printable character, or '\0'
// if this code isn't one we handle for text entry. Letters are case-shifted
// by `shift`; the shifted symbol row (digits -> !@#...) is not supported.
//
// evdev's KEY_A..KEY_Z values follow physical QWERTY scan order (KEY_Q=16,
// KEY_W=17, ... KEY_A=30, ... KEY_Z=44, ...), NOT alphabetical order, so
// each letter needs its own case rather than arithmetic on KEY_A.
char charForKeyCode(unsigned int code, bool shift) {
    switch (code) {
        case KEY_Q: return shift ? 'Q' : 'q';
        case KEY_W: return shift ? 'W' : 'w';
        case KEY_E: return shift ? 'E' : 'e';
        case KEY_R: return shift ? 'R' : 'r';
        case KEY_T: return shift ? 'T' : 't';
        case KEY_Y: return shift ? 'Y' : 'y';
        case KEY_U: return shift ? 'U' : 'u';
        case KEY_I: return shift ? 'I' : 'i';
        case KEY_O: return shift ? 'O' : 'o';
        case KEY_P: return shift ? 'P' : 'p';
        case KEY_A: return shift ? 'A' : 'a';
        case KEY_S: return shift ? 'S' : 's';
        case KEY_D: return shift ? 'D' : 'd';
        case KEY_F: return shift ? 'F' : 'f';
        case KEY_G: return shift ? 'G' : 'g';
        case KEY_H: return shift ? 'H' : 'h';
        case KEY_J: return shift ? 'J' : 'j';
        case KEY_K: return shift ? 'K' : 'k';
        case KEY_L: return shift ? 'L' : 'l';
        case KEY_Z: return shift ? 'Z' : 'z';
        case KEY_X: return shift ? 'X' : 'x';
        case KEY_C: return shift ? 'C' : 'c';
        case KEY_V: return shift ? 'V' : 'v';
        case KEY_B: return shift ? 'B' : 'b';
        case KEY_N: return shift ? 'N' : 'n';
        case KEY_M: return shift ? 'M' : 'm';
        case KEY_1: return '1';
        case KEY_2: return '2';
        case KEY_3: return '3';
        case KEY_4: return '4';
        case KEY_5: return '5';
        case KEY_6: return '6';
        case KEY_7: return '7';
        case KEY_8: return '8';
        case KEY_9: return '9';
        case KEY_0: return '0';
        case KEY_SPACE: return ' ';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_SLASH: return shift ? '?' : '/';
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        default: return '\0';
    }
}

}  // namespace

EvdevKeyboard::EvdevKeyboard(const std::string& devicePath) {
    fd_ = ::open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
}

EvdevKeyboard::~EvdevKeyboard() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool EvdevKeyboard::isOpen() const { return fd_ >= 0; }

GlobalKey EvdevKeyboard::poll() {
    if (fd_ < 0) {
        return GlobalKey::None;
    }

    struct input_event event {};
    GlobalKey result = GlobalKey::None;

    while (true) {
        const ssize_t bytesRead = ::read(fd_, &event, sizeof(event));
        if (bytesRead != static_cast<ssize_t>(sizeof(event))) {
            break;
        }
        if (event.type != EV_KEY || event.value != 1) {
            continue;
        }
        switch (event.code) {
            case KEY_R:
                result = GlobalKey::Refresh;
                break;
            case KEY_Q:
                result = GlobalKey::Quit;
                break;
            case KEY_M:
                result = GlobalKey::ToggleMap;
                break;
            case KEY_C:
                result = GlobalKey::ToggleMessages;
                break;
            case KEY_T:
                result = GlobalKey::ToggleRelayMessages;
                break;
            case KEY_ENTER:
            case KEY_KPENTER:
                result = GlobalKey::StartCompose;
                break;
            case KEY_ESC:
            case KEY_B:
                // KEY_B as a fallback: CardputerZero's chiclet keyboard may
                // not have a reliable dedicated Esc key -- unconfirmed on
                // real hardware, see docs/DEVICE_BUILD.md.
                result = GlobalKey::Back;
                break;
            case KEY_UP:
                result = GlobalKey::PanUp;
                break;
            case KEY_DOWN:
                result = GlobalKey::PanDown;
                break;
            case KEY_LEFT:
                result = GlobalKey::PanLeft;
                break;
            case KEY_RIGHT:
                result = GlobalKey::PanRight;
                break;
            case KEY_EQUAL:
            case KEY_KPPLUS:
                result = GlobalKey::ZoomIn;
                break;
            case KEY_MINUS:
            case KEY_KPMINUS:
                result = GlobalKey::ZoomOut;
                break;
            case KEY_U:
                result = GlobalKey::CheckForUpdate;
                break;
            case KEY_A:
                result = GlobalKey::ToggleAutoUpdate;
                break;
            default:
                break;
        }
    }

    return result;
}

TextInputEvent EvdevKeyboard::pollText() {
    TextInputEvent result;
    if (fd_ < 0) {
        return result;
    }

    struct input_event event {};
    while (true) {
        const ssize_t bytesRead = ::read(fd_, &event, sizeof(event));
        if (bytesRead != static_cast<ssize_t>(sizeof(event))) {
            break;
        }
        if (event.type != EV_KEY) {
            continue;
        }
        if (event.code == KEY_LEFTSHIFT || event.code == KEY_RIGHTSHIFT) {
            // value: 1 = down, 0 = up, 2 = autorepeat (held; state unchanged).
            if (event.value == 0 || event.value == 1) {
                shiftHeld_ = (event.value == 1);
            }
            continue;
        }
        if (event.value != 1) {
            continue;  // only act on key-down for everything else
        }
        switch (event.code) {
            case KEY_ESC:
                result.cancelPressed = true;
                break;
            case KEY_ENTER:
            case KEY_KPENTER:
                result.enterPressed = true;
                break;
            case KEY_BACKSPACE:
                result.backspace = true;
                break;
            default: {
                const char c = charForKeyCode(event.code, shiftHeld_);
                if (c != '\0') {
                    result.chars.push_back(c);
                }
                break;
            }
        }
    }

    return result;
}

}  // namespace cardmesh::ui
