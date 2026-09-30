#include "Validation.h"

#include <cctype>
#include <stdexcept>

namespace validation {

namespace {

std::string trim(const std::string& text) {
    const char* whitespace = " \t\r\n";
    std::size_t start = text.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

}  // namespace

std::string validateName(const std::string& name) {
    std::string cleaned = trim(name);
    if (cleaned.empty()) {
        throw std::invalid_argument("Name cannot be empty");
    }
    if (cleaned.size() > 100) {
        throw std::invalid_argument("Name cannot be longer than 100 characters");
    }
    return cleaned;
}

// A simple sanity check, not a full email standard:
// one '@', something before it, and a dot inside the domain part.
std::string validateEmail(const std::string& email) {
    std::string cleaned = trim(email);
    if (cleaned.empty() || cleaned.size() > 150) {
        throw std::invalid_argument("Email must be between 1 and 150 characters");
    }
    for (char c : cleaned) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("Email cannot contain spaces");
        }
    }
    std::size_t at = cleaned.find('@');
    if (at == std::string::npos || at == 0 || cleaned.find('@', at + 1) != std::string::npos) {
        throw std::invalid_argument("Email must contain exactly one '@' with text before it");
    }
    std::size_t dot = cleaned.find('.', at + 1);
    if (dot == std::string::npos || dot == at + 1 || dot == cleaned.size() - 1) {
        throw std::invalid_argument("Email domain must look like 'example.com'");
    }
    return cleaned;
}

}  // namespace validation