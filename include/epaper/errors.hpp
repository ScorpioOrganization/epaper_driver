#pragma once

#include <stdexcept>
#include <string>

namespace epaper {

/// Base class of every error thrown by the library.
class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/// A system call failed; carries the errno value.
class SystemError : public Error {
public:
  SystemError(const std::string& what, int error_number);

  int error_number() const noexcept;

private:
  int _error_number;
};

/// The panel did not release its BUSY line in time.
class TimeoutError : public Error {
public:
  using Error::Error;
};

}  // namespace epaper
