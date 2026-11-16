#include "HatPower.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace cardmesh::lora {

namespace {

bool readInt(const std::string& path, int* value) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char buffer[16] = {};
    ssize_t count;
    do {
        count = ::read(fd, buffer, sizeof(buffer) - 1);
    } while (count < 0 && errno == EINTR);
    ::close(fd);
    if (count <= 0) {
        return false;
    }
    *value = std::atoi(buffer);
    return true;
}

bool writeInt(const std::string& path, int value) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char buffer[16] = {};
    const int length = std::snprintf(buffer, sizeof(buffer), "%d", value);
    const ssize_t written = length > 0 ? ::write(fd, buffer, static_cast<std::size_t>(length)) : -1;
    ::close(fd);
    return written == length;
}

}  // namespace

bool HatPowerRails::enable() {
    if (active_) {
        return true;
    }
    for (auto& rail : rails_) {
        if (!readInt(rail.path, &rail.previousValue)) {
            restore();
            return false;
        }
        if (rail.previousValue > 0) {
            continue;
        }
        rail.restoreNeeded = true;
        if (!writeInt(rail.path, 1)) {
            restore();
            return false;
        }
    }
    active_ = true;
    ::usleep(150000);
    return true;
}

void HatPowerRails::restore() {
    for (auto& rail : rails_) {
        if (!rail.restoreNeeded) {
            continue;
        }
        writeInt(rail.path, rail.previousValue);
        rail.restoreNeeded = false;
    }
    active_ = false;
}

}  // namespace cardmesh::lora
