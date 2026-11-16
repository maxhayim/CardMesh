#include "UpdateInstaller.h"

#include <sys/wait.h>
#include <unistd.h>

#include <fstream>

#include "Version.h"
#include "api/HttpClient.h"

namespace cardmesh::update {

InstallResult downloadAndInstall(const std::string& url, const std::string& destPath) {
    InstallResult result;

    const auto response = cardmesh::api::fetchUrl(
        url, {"User-Agent: CardMesh/" + std::string(cardmesh::kVersion) +
              " (+https://github.com/" + std::string(cardmesh::kGitHubRepo) + ")"},
        /*timeoutSeconds=*/60);
    if (!response.ok()) {
        result.message = "Download failed: " + (response.networkError ? response.errorMessage : response.body);
        return result;
    }

    {
        std::ofstream file(destPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            result.message = "Could not write " + destPath;
            return result;
        }
        file.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
    }

    const pid_t pid = fork();
    if (pid < 0) {
        result.message = "fork() failed; update downloaded to " + destPath + " -- install manually with: sudo dpkg -i " + destPath;
        return result;
    }
    if (pid == 0) {
        execl("/usr/bin/dpkg", "dpkg", "-i", destPath.c_str(), static_cast<char*>(nullptr));
        _exit(127);  // execl only returns on failure
    }

    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        result.message = "dpkg -i failed (likely needs root); update downloaded to " + destPath +
                          " -- install manually with: sudo dpkg -i " + destPath;
        return result;
    }

    result.ok = true;
    result.message = "Installed. Restart CardMesh to run the new version.";
    return result;
}

}  // namespace cardmesh::update
