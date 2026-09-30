#pragma once

#include <string>

#include "TransactionRequest.h"

// What a rule reports back to the engine.
struct RuleResult {
    std::string ruleName;  // stable identifier, stored later in FraudAlerts.rule_type
    bool triggered = false;
    int points = 0;        // 0 when the rule did not trigger
    std::string message;   // human-readable explanation
};

// Abstract base class: the engine only knows this interface.
class FraudRule {
public:
    virtual ~FraudRule() = default;  // virtual so deleting through FraudRule* is safe

    // Judges one transaction. Must not change any data.
    virtual RuleResult check(const TransactionRequest& request) const = 0;

    // Name shown in reports, e.g. "High Amount Rule"
    virtual std::string displayName() const = 0;
};