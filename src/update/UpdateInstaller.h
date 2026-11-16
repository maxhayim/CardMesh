#pragma once

#include <string>

namespace cardmesh::update {

struct InstallResult {
    bool ok = false;
    std::string message;
};

// Downloads `url` (an *_arm64.deb release asset) to a fixed temp path and
// installs it via `dpkg -i` -- a fixed binary invoked with a fixed argv
// (no shell, no string interpolation of untrusted input; see SECURITY.md's
// "avoid arbitrary shell execution").
//
// ASSUMPTION, not verified against real CardputerZero hardware: that the
// running process has permission to install packages (e.g. runs as root,
// as is common on single-user embedded Linux devices). If `dpkg -i` fails
// for lack of privilege, this returns ok=false with the downloaded path so
// the user can run `sudo dpkg -i <path>` themselves -- it does not attempt
// to escalate privileges itself.
InstallResult downloadAndInstall(const std::string& url,
                                  const std::string& destPath = "/tmp/cardmesh_update.deb");

}  // namespace cardmesh::update
