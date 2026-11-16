#include "UpdateChecker.h"

#include <array>
#include <cstdlib>

#include <nlohmann/json.hpp>

#include "api/HttpClient.h"

namespace cardmesh::update {

namespace {

std::string stripLeadingV(const std::string& version) {
    return (!version.empty() && (version[0] == 'v' || version[0] == 'V')) ? version.substr(1) : version;
}

std::array<long, 3> parseSemver(const std::string& version) {
    std::array<long, 3> parts{0, 0, 0};
    size_t start = 0;
    for (int i = 0; i < 3 && start <= version.size(); ++i) {
        size_t dot = version.find('.', start);
        const std::string segment = version.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        parts[i] = std::strtol(segment.c_str(), nullptr, 10);
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    return parts;
}

}  // namespace

std::optional<ReleaseInfo> parseLatestReleaseJson(const std::string& body) {
    try {
        const auto json = nlohmann::json::parse(body);
        const std::string tag = json.value("tag_name", "");
        if (tag.empty()) {
            return std::nullopt;
        }

        const auto assets = json.find("assets");
        if (assets == json.end() || !assets->is_array()) {
            return std::nullopt;
        }

        for (const auto& asset : *assets) {
            const std::string name = asset.value("name", "");
            const std::string suffix = "_arm64.deb";
            if (name.size() >= suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                ReleaseInfo info;
                info.version = stripLeadingV(tag);
                info.downloadUrl = asset.value("browser_download_url", "");
                if (info.downloadUrl.empty()) {
                    return std::nullopt;
                }
                return info;
            }
        }
        return std::nullopt;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

bool isNewerVersion(const std::string& latest, const std::string& current) {
    const auto latestParts = parseSemver(stripLeadingV(latest));
    const auto currentParts = parseSemver(stripLeadingV(current));
    return latestParts > currentParts;
}

UpdateCheckResult checkForUpdate(const std::string& currentVersion, const std::string& repo) {
    UpdateCheckResult result;

    const std::string url = "https://api.github.com/repos/" + repo + "/releases/latest";
    const auto response = cardmesh::api::fetchUrl(
        url,
        {
            "User-Agent: CardMesh/" + currentVersion + " (+https://github.com/" + repo + ")",
            "Accept: application/vnd.github+json",
        });

    if (!response.ok()) {
        result.errorMessage = response.networkError ? response.errorMessage : response.body;
        return result;
    }

    const auto release = parseLatestReleaseJson(response.body);
    if (!release.has_value()) {
        result.errorMessage = "No arm64 .deb asset found on the latest release";
        return result;
    }

    result.ok = true;
    result.latestVersion = release->version;
    result.downloadUrl = release->downloadUrl;
    result.updateAvailable = isNewerVersion(release->version, currentVersion);
    return result;
}

}  // namespace cardmesh::update
