#pragma once

namespace cardmesh {

// Keep in sync with app-builder.json's top-level "version" -- there is no
// build-time codegen linking the two, so both must be bumped by hand on
// every release. This had drifted to a stale "0.1.0" for several real
// releases (0.1.1 through 0.3.1), which made the update checker always
// think v0.3.x was newer than itself and never report "up to date".
inline constexpr const char* kVersion = "0.3.5";
inline constexpr const char* kGitHubRepo = "maxhayim/CardMesh";

}  // namespace cardmesh
