#!/usr/bin/env python3
"""Cross-compiles CardMesh for CardputerZero (aarch64-linux-gnu) using zig cc,
against a local sysroot extracted from Debian bookworm arm64 packages.

Verified running on real CardputerZero hardware; see docs/DEVICE_BUILD.md.

Usage: python3 scripts/build_arm64.py <path-to-sysroot-root>
  <path-to-sysroot-root> is the directory containing usr/include, usr/lib/...
  extracted from the .deb packages (see docs/DEVICE_BUILD.md for the list).
"""
import glob
import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Pinned to Debian bookworm's glibc (2.36) since the bundled runtime .so
# files in the sysroot were built against it -- see docs/DEVICE_BUILD.md.
TARGET = "aarch64-linux-gnu.2.36"
OUT_DIR = os.path.join(REPO_ROOT, "build-arm64")
OBJ_DIR = os.path.join(OUT_DIR, "obj")


def zig_cc(is_cpp):
    return ["zig", "c++" if is_cpp else "cc"]


def compile_unit(src, sysroot, is_cpp):
    rel = os.path.relpath(src, REPO_ROOT)
    obj = os.path.join(OBJ_DIR, rel + ".o")
    os.makedirs(os.path.dirname(obj), exist_ok=True)

    cmd = zig_cc(is_cpp) + [
        "-target", TARGET,
        "-c", src,
        "-o", obj,
        "-O2",
        "-Wno-deprecated-declarations",
        # RadioLib's BuildOpt.h embeds __DATE__/__TIME__ in a version string
        # (RADIOLIB_INFO), which zig cc treats as an error by default since
        # it breaks reproducible builds -- harmless here, just a build
        # timestamp string never read at runtime.
        "-Wno-date-time",
        "-I", os.path.join(REPO_ROOT, "src"),
        "-I", os.path.join(REPO_ROOT, "src", "ui"),
        "-I", os.path.join(REPO_ROOT, "third_party"),
        "-I", os.path.join(REPO_ROOT, "third_party", "lvgl"),
        "-I", os.path.join(REPO_ROOT, "third_party", "RadioLib", "src"),
        "-isystem", os.path.join(sysroot, "usr", "include"),
        "-isystem", os.path.join(sysroot, "usr", "include", "aarch64-linux-gnu"),
    ]
    if is_cpp:
        cmd += ["-std=gnu++17"]
    else:
        cmd += ["-std=gnu11"]

    print("CC " if not is_cpp else "CXX", rel)
    subprocess.run(cmd, check=True)
    return obj


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    sysroot = os.path.abspath(sys.argv[1])

    os.makedirs(OUT_DIR, exist_ok=True)

    cpp_sources = [
        "src/api/HttpClient.cpp",
        "src/api/MeshMonitorClient.cpp",
        "src/storage/Settings.cpp",
        "src/storage/Database.cpp",
        "src/map/TileCache.cpp",
        "src/map/TileFetcher.cpp",
        "src/ui/NetworkWorker.cpp",
        "src/ui/MeshtasticWorker.cpp",
        "src/ui/MeshCoreWorker.cpp",
        "src/ui/BootScreen.cpp",
        "src/ui/HomeScreen.cpp",
        "src/ui/SettingsScreen.cpp",
        "src/ui/DashboardScreen.cpp",
        "src/ui/MapScreen.cpp",
        "src/ui/LoraMessagesScreen.cpp",
        "src/ui/MeshMonitorMessagesScreen.cpp",
        "src/ui/MeshCoreMessagesScreen.cpp",
        "src/ui/EmojiFont.cpp",
        "src/ui/EvdevKeyboard.cpp",
        "src/update/UpdateChecker.cpp",
        "src/update/UpdateInstaller.cpp",
        "src/update/UpdateWorker.cpp",
        "src/mesh/Aes.cpp",
        "src/mesh/Sha256.cpp",
        "src/mesh/MeshCoreCrypto.cpp",
        "src/mesh/MeshCorePacketCodec.cpp",
        "src/mesh/MeshtasticCrypto.cpp",
        "src/mesh/Protobuf.cpp",
        "src/mesh/ChannelKey.cpp",
        "src/mesh/MeshPacketCodec.cpp",
        "src/lora/HatPower.cpp",
        "src/lora/LinuxGpio.cpp",
        "src/lora/LinuxI2c.cpp",
        "src/lora/LinuxSpi.cpp",
        "src/lora/LoraRadio.cpp",
        "src/lora/Pi4ioExpander.cpp",
        "src/lora/SX1262Hal.cpp",
        "src/ui_main.cpp",
    ]
    cpp_sources = [os.path.join(REPO_ROOT, p) for p in cpp_sources]

    lvgl_sources = sorted(
        glob.glob(os.path.join(REPO_ROOT, "third_party", "lvgl", "src", "**", "*.c"), recursive=True)
    )
    if not lvgl_sources:
        print("No LVGL sources found under third_party/lvgl/src -- did you vendor LVGL?")
        sys.exit(1)

    radiolib_sources = sorted(
        glob.glob(os.path.join(REPO_ROOT, "third_party", "RadioLib", "src", "**", "*.cpp"), recursive=True)
    )
    if not radiolib_sources:
        print("No RadioLib sources found under third_party/RadioLib/src -- did you vendor RadioLib?")
        sys.exit(1)

    objects = []
    for src in cpp_sources:
        objects.append(compile_unit(src, sysroot, is_cpp=True))
    for src in radiolib_sources:
        objects.append(compile_unit(src, sysroot, is_cpp=True))
    for src in lvgl_sources:
        objects.append(compile_unit(src, sysroot, is_cpp=False))

    binary = os.path.join(OUT_DIR, "cardmesh")
    link_cmd = zig_cc(is_cpp=True) + [
        "-target", TARGET,
        "-O2",
        "-o", binary,
        *objects,
        "-L", os.path.join(sysroot, "usr", "lib", "aarch64-linux-gnu"),
        "-Wl,-rpath,/usr/lib/cardmesh",
        "-lcurl",
        "-lsqlite3",
        "-lpthread",
        "-lm",
    ]
    print("LINK", os.path.relpath(binary, REPO_ROOT))
    subprocess.run(link_cmd, check=True)

    print(f"\nBuilt: {binary}")


if __name__ == "__main__":
    main()
