#pragma once

#include <memory>
#include <string>
#include <vector>

namespace epaper::os {

using IoctlRequest = unsigned long;  // NOLINT(runtime/int) - matches ioctl(2)

/**
 * @brief The handful of system calls the Linux backends need.
 *
 * Every hardware access goes through this interface, so SPI and GPIO code can be tested against a fake
 * kernel. Implementations follow the POSIX contract: -1 and errno on failure.
 */
class Syscalls {
public:
  virtual int open(const std::string& path, int flags) = 0;
  virtual int close(int fd) = 0;
  virtual int ioctl(int fd, IoctlRequest request, void* argument) = 0;
  /// Absolute paths of all GPIO character devices (gpiochipN), sorted.
  virtual std::vector<std::string> list_gpiochips() = 0;

protected:
  /// Owned through std::shared_ptr, see epaper/hal.hpp.
  ~Syscalls() = default;
};

/// The real thing.
class LinuxSyscalls final : public Syscalls {
public:
  /// @param device_directory where list_gpiochips() looks for gpiochipN nodes.
  explicit LinuxSyscalls(std::string device_directory = "/dev");

  int open(const std::string& path, int flags) override;
  int close(int fd) override;
  int ioctl(int fd, IoctlRequest request, void* argument) override;
  std::vector<std::string> list_gpiochips() override;

private:
  std::string _device_directory;
};

/// Owning file descriptor, closed through the Syscalls instance it came from.
class FileDescriptor {
public:
  FileDescriptor() noexcept = default;
  FileDescriptor(std::shared_ptr<Syscalls> syscalls, int fd) noexcept;
  FileDescriptor(FileDescriptor&& other) noexcept;
  FileDescriptor& operator=(FileDescriptor&& other) noexcept;
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  ~FileDescriptor();

  int get() const noexcept;
  bool valid() const noexcept;
  void reset() noexcept;

private:
  std::shared_ptr<Syscalls> _syscalls;
  int _fd = -1;
};

/// open() that throws epaper::SystemError instead of returning -1.
FileDescriptor open_or_throw(const std::shared_ptr<Syscalls>& syscalls, const std::string& path, int flags);

/// ioctl() that throws epaper::SystemError("<what> <subject>: <errno text>") instead of returning -1.
int ioctl_or_throw(
  Syscalls& syscalls, int fd, IoctlRequest request, void* argument, const char* what,
  const std::string& subject);

}  // namespace epaper::os
