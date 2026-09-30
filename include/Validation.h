#pragma once

#include <string>

namespace validation {

// Each function throws std::invalid_argument with a clear message if the input is bad.
// They return the cleaned (trimmed) value so callers can store it directly.
std::string validateName(const std::string& name);
std::string validateEmail(const std::string& email);

}  // namespace validation