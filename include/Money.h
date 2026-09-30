#pragma once

#include <string>

// Money is stored as a whole number of paise (1 rupee = 100 paise).
// Example: 95000.50 rupees -> 9500050
using Paise = long long;

namespace money {

// Converts text like "95000.50", "100" or "0.5" to paise.
// Accepts at most 2 decimal places. Throws std::invalid_argument if the text isn't valid money.
Paise parse(const std::string& text);

// Converts paise to database-style text: 9500050 -> "95000.50"
std::string format(Paise amount);

}  // namespace money