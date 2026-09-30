#pragma once

#include "FraudConfig.h"
#include "FraudRule.h"

// Flags transactions above a fixed amount. Needs no database access.
class HighAmountRule : public FraudRule {
public:
    explicit HighAmountRule(const FraudConfig& config) : config_(config) {}

    RuleResult check(const TransactionRequest& request) const override;
    std::string displayName() const override { return "High Amount Rule"; }

private:
    FraudConfig config_;
};