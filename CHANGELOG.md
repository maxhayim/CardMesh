# Changelog

All notable changes to CardMesh will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

## [0.3.5] - 2026-11-24

### Added (0.3.5)

- Worldwide LoRa region/frequency support for both the direct Meshtastic and
  MeshCore backends (`src/mesh/LoraRegions.h/.cpp`), selectable from
  Settings. Frequency tables sourced from the real Meshtastic firmware
  `RDEF()` region table; deliberately excludes ITU amateur-radio-only bands
  (require a verified ham license this app can't check), the deprecated
  `UA_868`, and `EU_N_868` (a frequency-duplicate of `EU_868` under this
  app's fixed-MediumFast-preset design). MeshCore has no published
  per-region frequency table of its own, so it reuses whichever region the
  user selects for the shared setting. Addresses maxhayim/CardMesh#6.
- v33 visual changes: enlarged Home cards, no Home footer, monochrome
  logos, live system indicators (`src/hardware/HardwareStatus.*`), and a
  reusable context-sensitive service footer (`src/ui/Footer.*`).

### Fixed (0.3.5)

- **Navigation crash on selecting a service from Home** (the reported bug):
  `MeshtasticWorker::start()`/`MeshCoreWorker::start()` move-assigned a new
  `std::thread` into `thread_` without checking whether the previous one was
  still joinable. If a worker's own `run()` had already exited on its own
  (radio `begin()` failing fast) without `stop()` ever being called to join
  it, the next `start()` call -- e.g. selecting that service's tile from
  Home -- overwrote a still-joinable `thread_` and called `std::terminate()`,
  aborting the whole process. Reproduced live and fixed by joining any
  joinable `thread_` before replacing it, in both workers.
- `MeshtasticWorker::stop()`/`MeshCoreWorker::stop()` could skip joining a
  finished-but-unjoined worker thread (gated the join on an atomic exchange
  that the worker itself could flip), leaving a joinable `std::thread` to be
  destroyed later and aborting the process the same way. Always join now if
  there's anything to join.
- RadioLib's `Module::SPItransferStream` called `memcpy` with a null source
  pointer and zero length for certain zero-payload commands (e.g. the
  SX126x standby wakeup NOP) -- defined behavior on glibc, but a hard fault
  under this build's libc. Guarded with a length check.
- Stale `model_parsing` test fixture used JSON field names
  (`conversationType`/`state`) that `Message::fromJson` had already been
  migrated away from (to `toNodeId`/`deliveryState`) to match the real
  MeshMonitor server response; unrelated to navigation, fixed separately.

### Changed

- Added timestamped `[nav]`/`[worker]` diagnostics (screen transitions,
  worker start/stop/join, `radio_.begin()` results) to aid future crash
  triage; always on, lightweight since they only fire on real transitions.
- Added an opt-in main-loop heartbeat (`CARDMESH_DEBUG_HEARTBEAT=1`) for
  diagnosing a hung UI thread; off by default to avoid continuous logging
  in normal operation.
- `scripts/build_arm64.py` gained `--enable-test-hooks`, which compiles in
  an `CARDMESH_FAKE_RADIO` test-only hook in `LoraRadio::begin()` (mock
  success/failure/slow-init, used to regression-test navigation and worker
  lifecycle without touching the physical SX1262). Gated behind a
  compile-time macro, never passed for a release build -- without it, the
  hook's code does not exist in the binary at all, so no production
  environment variable can disable real hardware access.

### Added

- Project scaffold: directory layout, CMake build, `.gitignore`.
- `HttpClient`: libcurl-based HTTPS client with bearer auth and timeouts.
- `MeshMonitorClient`: typed wrapper over the MeshMonitor REST API v1 (sources, nodes, channels, messages, telemetry, traceroutes).
- Data models: `Source`, `Node`, `Channel`, `Message`, `Telemetry`, `Traceroute`.
- `Settings`: config load/save at `~/.config/cardmesh/config.json` with `0600` permissions.
- `Database`: SQLite-backed local cache and unread/favorites state.
- Unit tests for URL construction, JSON parsing, and settings persistence.
