#pragma once

#include <array>
#include <string>

namespace cardmesh::lora {

// CardputerZero exposes the hat connector's 5V rails as LED-class sysfs
// brightness files (ext_5v_out, grove_5v_out) rather than a dedicated power
// control node. enable() turns on whichever of these were off, and
// restore() puts back only the ones this call actually changed -- mirrors
// Cap-LoRa-1262's cp0_lora_ext_power_controller exactly, since other
// peripherals may share these rails.
class HatPowerRails {
public:
    bool enable();
    void restore();

private:
    struct RailState {
        std::string path;
        int previousValue = 0;
        bool restoreNeeded = false;
    };

    std::array<RailState, 2> rails_{{
        {"/sys/class/leds/ext_5v_out/brightness", 0, false},
        {"/sys/class/leds/grove_5v_out/brightness", 0, false},
    }};
    bool active_ = false;
};

}  // namespace cardmesh::lora
