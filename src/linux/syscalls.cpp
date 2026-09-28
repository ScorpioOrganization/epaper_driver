#include "epaper/linux/syscalls.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "epaper/errors.hpp"

namespace epaper::os {

LinuxSyscalls::LinuxSyscalls(std::string device_directory)
: _device_directory(std::move(device_directory)) { }

int LinuxSyscalls::open(const std::string& path, int flags) {
  return ::open(path.c_str(), flags | O_CLOEXEC);
}

int LinuxSyscalls::close(int fd) {
  return ::close(fd);
}

int LinuxSyscalls::ioctl(int fd, IoctlRequest request, void* argument) {
  return ::ioctl(fd, request, argument);
}

std::vector<std::string> LinuxSyscalls::list_gpiochips() {
  std::vector<std::string> chips;
  std::error_code error;
  const std::filesystem::directory_iterator entries(_device_directory, error);
  for (const auto& entry : entries) {
    if (entry.path().filename().string().rfind("gpiochip", 0) == 0) {
      chips.push_back(entry.path().string());
    }
  }
  std::sort(chips.begin(), chips.end());
  return chips;
}

FileDescriptor::FileDescriptor(std::shared_ptr<Syscalls> syscalls, int fd) noexcept
: _syscalls(std::move(syscalls)), _fd(fd) { }

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
: _syscalls(std::move(other._syscalls)), _fd(std::exchange(other._fd, -1)) { }

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
  if (this != &other) {
    reset();
    _syscalls = std::move(other._syscalls);
    _fd = std::exchange(other._fd, -1);
  }
  return *this;
}

FileDescriptor::~FileDescriptor() {
  reset();
}

int FileDescriptor::get() const noexcept {
  return _fd;
}

bool FileDescriptor::valid() const noexcept {
  return _fd >= 0;
}

void FileDescriptor::reset() noexcept {
  if (_fd >= 0 && _syscalls) {
    _syscalls->close(_fd);
  }
  _fd = -1;
}

FileDescriptor open_or_throw(const std::shared_ptr<Syscalls>& syscalls, const std::string& path, int flags) {
  const int fd = syscalls->open(path, flags);
  if (fd < 0) {
    throw SystemError("cannot open " + path, errno);
  }
  return FileDescriptor(syscalls, fd);
}

int ioctl_or_throw(
  Syscalls& syscalls, int fd, IoctlRequest request, void* argument, const char* what,
  const std::string& subject) {
  const int result = syscalls.ioctl(fd, request, argument);
  if (result < 0) {
    throw SystemError(std::string(what) + " " + subject, errno);
  }
  return result;
}

}  // namespace epaper::os
