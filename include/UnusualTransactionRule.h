#pragma once

#include "DatabaseManager.h"
#include "FraudConfig.h"
#include "FraudRule.h"

// Flags an amount far above the sender's own historical average.
class UnusualTransactionRule : public FraudRule {
public:
    // Holds a reference: the DatabaseManager must outlive this rule.
    UnusualTransactionRule(DatabaseManager& db, const FraudConfig& config)
        : db_(db), config_(config) {}

    RuleResult check(const TransactionRequest& request) const override;
    std::string displayName() const override { return "Unusual Transaction Rule"; }

private:
    DatabaseManager& db_;
    FraudConfig config_;
};