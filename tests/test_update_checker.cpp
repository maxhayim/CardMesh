#include "TestRunner.h"
#include "update/UpdateChecker.h"

using namespace cardmesh::update;

CARDMESH_TEST_MAIN_BEGIN()

    // isNewerVersion
    CARDMESH_CHECK(isNewerVersion("0.2.0", "0.1.0"));
    CARDMESH_CHECK(isNewerVersion("v0.2.0", "0.1.0"));
    CARDMESH_CHECK(isNewerVersion("1.0.0", "0.9.9"));
    CARDMESH_CHECK(!isNewerVersion("0.1.0", "0.1.0"));
    CARDMESH_CHECK(!isNewerVersion("0.1.0", "0.2.0"));
    CARDMESH_CHECK(!isNewerVersion("", ""));

    // parseLatestReleaseJson: happy path with an arm64 .deb asset.
    {
        const std::string body = R"({
            "tag_name": "v0.2.0",
            "assets": [
                {"name": "cardmesh_0.2.0_arm64.deb", "browser_download_url": "https://example.com/cardmesh_0.2.0_arm64.deb"},
                {"name": "source.tar.gz", "browser_download_url": "https://example.com/source.tar.gz"}
            ]
        })";
        const auto info = parseLatestReleaseJson(body);
        CARDMESH_CHECK(info.has_value());
        if (info.has_value()) {
            CARDMESH_CHECK(info->version == "0.2.0");
            CARDMESH_CHECK(info->downloadUrl == "https://example.com/cardmesh_0.2.0_arm64.deb");
        }
    }

    // No arm64 .deb asset attached -> nothing this device could install.
    {
        const std::string body = R"({
            "tag_name": "v0.2.0",
            "assets": [
                {"name": "source.tar.gz", "browser_download_url": "https://example.com/source.tar.gz"}
            ]
        })";
        CARDMESH_CHECK(!parseLatestReleaseJson(body).has_value());
    }

    // Malformed JSON must not throw.
    CARDMESH_CHECK(!parseLatestReleaseJson("not json").has_value());

    // Missing tag_name.
    CARDMESH_CHECK(!parseLatestReleaseJson(R"({"assets": []})").has_value());

CARDMESH_TEST_MAIN_END()
