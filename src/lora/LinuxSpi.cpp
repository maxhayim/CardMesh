#include "LinuxSpi.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace cardmesh::lora {

LinuxSpiDevice::~LinuxSpiDevice() { close(); }

bool LinuxSpiDevice::open(const std::string& devicePath, std::uint32_t speedHz) {
    close();
    const int fd = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    std::uint8_t mode = SPI_MODE_0;
    std::uint8_t bitsPerWord = 8;

    if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 || ::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord) < 0 ||
        ::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speedHz) < 0) {
        const int savedErrno = errno;
        ::close(fd);
        errno = savedErrno;
        return false;
    }

    fd_ = fd;
    speedHz_ = speedHz;
    return true;
}

bool LinuxSpiDevice::transfer(const std::uint8_t* tx, std::uint8_t* rx, std::size_t length) {
    if (fd_ < 0 || length == 0 || (tx == nullptr && rx == nullptr)) {
        return false;
    }

    spi_ioc_transfer message{};
    message.tx_buf = reinterpret_cast<std::uint64_t>(tx);
    message.rx_buf = reinterpret_cast<std::uint64_t>(rx);
    message.len = static_cast<std::uint32_t>(length);
    message.speed_hz = speedHz_;
    message.bits_per_word = 8;

    return ::ioctl(fd_, SPI_IOC_MESSAGE(1), &message) >= 0;
}

void LinuxSpiDevice::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

}  // namespace cardmesh::lora
