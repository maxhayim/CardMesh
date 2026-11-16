#include "LinuxGpio.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace cardmesh::lora {

LinuxGpioLine::~LinuxGpioLine() { close(); }

LinuxGpioLine::LinuxGpioLine(LinuxGpioLine&& other) noexcept : lineFd_(other.lineFd_) {
    other.lineFd_ = -1;
}

LinuxGpioLine& LinuxGpioLine::operator=(LinuxGpioLine&& other) noexcept {
    if (this != &other) {
        close();
        lineFd_ = other.lineFd_;
        other.lineFd_ = -1;
    }
    return *this;
}

namespace {

bool requestLine(const std::string& chipPath, std::uint32_t offset, std::uint64_t flags,
                  bool initialHigh, const std::string& consumer, int* outLineFd) {
    const int chipFd = ::open(chipPath.c_str(), O_RDWR | O_CLOEXEC);
    if (chipFd < 0) {
        return false;
    }

    gpio_v2_line_request request{};
    request.offsets[0] = offset;
    request.num_lines = 1;
    std::snprintf(request.consumer, sizeof(request.consumer), "%s", consumer.c_str());
    request.config.flags = flags;
    if (flags & GPIO_V2_LINE_FLAG_OUTPUT) {
        request.config.num_attrs = 1;
        request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        request.config.attrs[0].attr.values = initialHigh ? 1ULL : 0ULL;
        request.config.attrs[0].mask = 1ULL;
    }

    const int ioctlResult = ::ioctl(chipFd, GPIO_V2_GET_LINE_IOCTL, &request);
    const int savedErrno = errno;
    ::close(chipFd);
    errno = savedErrno;
    if (ioctlResult < 0 || request.fd <= 0) {
        return false;
    }
    *outLineFd = request.fd;
    return true;
}

}  // namespace

bool LinuxGpioLine::requestOutput(const std::string& chipPath, std::uint32_t offset, bool initialHigh,
                                   const std::string& consumer) {
    close();
    return requestLine(chipPath, offset, GPIO_V2_LINE_FLAG_OUTPUT, initialHigh, consumer, &lineFd_);
}

bool LinuxGpioLine::requestInput(const std::string& chipPath, std::uint32_t offset,
                                  const std::string& consumer) {
    close();
    return requestLine(chipPath, offset, GPIO_V2_LINE_FLAG_INPUT, false, consumer, &lineFd_);
}

bool LinuxGpioLine::get(bool* high) const {
    if (lineFd_ < 0 || high == nullptr) {
        return false;
    }
    gpio_v2_line_values values{};
    values.mask = 1ULL;
    if (::ioctl(lineFd_, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
        return false;
    }
    *high = (values.bits & 1ULL) != 0;
    return true;
}

bool LinuxGpioLine::set(bool high) {
    if (lineFd_ < 0) {
        return false;
    }
    gpio_v2_line_values values{};
    values.mask = 1ULL;
    values.bits = high ? 1ULL : 0ULL;
    return ::ioctl(lineFd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) >= 0;
}

void LinuxGpioLine::close() {
    if (lineFd_ >= 0) {
        ::close(lineFd_);
        lineFd_ = -1;
    }
}

}  // namespace cardmesh::lora
