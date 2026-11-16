#include "LinuxI2c.h"

#include <cerrno>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace cardmesh::lora {

LinuxI2cBus::~LinuxI2cBus() { close(); }

bool LinuxI2cBus::open(const std::string& devicePath) {
    close();
    const int fd = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    fd_ = fd;
    return true;
}

bool LinuxI2cBus::write(std::uint8_t address7, const std::uint8_t* data, std::size_t length) {
    if (fd_ < 0) {
        return false;
    }
    i2c_msg msg{};
    msg.addr = address7;
    msg.flags = 0;
    msg.len = static_cast<std::uint16_t>(length);
    msg.buf = const_cast<std::uint8_t*>(data);

    i2c_rdwr_ioctl_data payload{};
    payload.msgs = &msg;
    payload.nmsgs = 1;
    return ::ioctl(fd_, I2C_RDWR, &payload) >= 0;
}

bool LinuxI2cBus::writeThenRead(std::uint8_t address7, const std::uint8_t* writeData, std::size_t writeLength,
                                 std::uint8_t* readData, std::size_t readLength) {
    if (fd_ < 0) {
        return false;
    }
    i2c_msg msgs[2]{};
    msgs[0].addr = address7;
    msgs[0].flags = 0;
    msgs[0].len = static_cast<std::uint16_t>(writeLength);
    msgs[0].buf = const_cast<std::uint8_t*>(writeData);
    msgs[1].addr = address7;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = static_cast<std::uint16_t>(readLength);
    msgs[1].buf = readData;

    i2c_rdwr_ioctl_data payload{};
    payload.msgs = msgs;
    payload.nmsgs = 2;
    return ::ioctl(fd_, I2C_RDWR, &payload) >= 0;
}

void LinuxI2cBus::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

}  // namespace cardmesh::lora
