#include "epaper/errors.hpp"

#include <string>
#include <system_error>

namespace epaper {

SystemError::SystemError(const std::string& what, int error_number)
: Error(what + ": " + std::generic_category().message(error_number)), _error_number(error_number) { }

int SystemError::error_number() const noexcept {
  return _error_number;
}

}  // namespace epaper
