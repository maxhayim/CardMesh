#pragma once

#include <optional>
#include <string>

namespace cardmesh::update {

struct ReleaseInfo {
    std::string version;      // tag_name with any leading 'v' stripped
    std::string downloadUrl;  // browser_download_url of the *_arm64.deb asset
};

struct UpdateCheckResult {
    bool ok = false;
    bool updateAvailable = false;
    std::string latestVersion;
    std::string downloadUrl;
    std::string errorMessage;
};

// Parses a GitHub "get latest release" API response body. Pure/host-testable
// -- does not touch the network. Returns nullopt if no *_arm64.deb asset is
// attached to the release (nothing this device could install).
std::optional<ReleaseInfo> parseLatestReleaseJson(const std::string& body);

// True if `latest` is a newer version than `current`. Both are plain
// "MAJOR.MINOR.PATCH" strings (an optional leading 'v' is ignored). A
// malformed version string is treated as not-newer rather than throwing.
bool isNewerVersion(const std::string& latest, const std::string& current);

// Queries GitHub's "latest release" API for `repo` (e.g. "maxhayim/CardMesh")
// and compares it against `currentVersion`. Network-facing; not covered by
// host unit tests (see parseLatestReleaseJson/isNewerVersion for the
// testable logic this wraps).
UpdateCheckResult checkForUpdate(const std::string& currentVersion, const std::string& repo);

}  // namespace cardmesh::update
