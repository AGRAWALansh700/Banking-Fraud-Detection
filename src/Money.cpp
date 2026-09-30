#include "Money.h"

#include <cctype>
#include <stdexcept>

namespace money {

Paise parse(const std::string& text) {
    if (text.empty()) {
        throw std::invalid_argument("Amount cannot be empty");
    }

    bool negative = false;
    std::size_t pos = 0;
    if (text[0] == '-') {
        negative = true;
        pos = 1;
    }

    Paise rupees = 0;
    Paise fraction = 0;
    int fractionDigits = 0;
    bool seenDot = false;
    bool seenDigit = false;

    for (; pos < text.size(); ++pos) {
        char c = text[pos];
        if (c == '.') {
            if (seenDot) {
                throw std::invalid_argument("Invalid amount: " + text);
            }
            seenDot = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            seenDigit = true;
            if (seenDot) {
                if (++fractionDigits > 2) {
                    throw std::invalid_argument("Amount can have at most 2 decimal places");
                }
                fraction = fraction * 10 + (c - '0');
            } else {
                rupees = rupees * 10 + (c - '0');
                if (rupees > 9000000000000LL) {  // stays inside DECIMAL(15,2) and avoids overflow
                    throw std::invalid_argument("Amount is too large");
                }
            }
        } else {
            throw std::invalid_argument("Invalid amount: " + text);
        }
    }
    if (!seenDigit) {
        throw std::invalid_argument("Invalid amount: " + text);
    }

    if (fractionDigits == 1) {
        fraction *= 10;  // "0.5" means 50 paise
    }
    Paise total = rupees * 100 + fraction;
    return negative ? -total : total;
}

std::string format(Paise amount) {
    bool negative = amount < 0;
    Paise absolute = negative ? -amount : amount;
    Paise rupees = absolute / 100;
    Paise paise = absolute % 100;

    std::string result = negative ? "-" : "";
    result += std::to_string(rupees) + ".";
    if (paise < 10) {
        result += "0";
    }
    result += std::to_string(paise);
    return result;
}

}  // namespace money