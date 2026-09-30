#pragma once

#include "DatabaseManager.h"
#include "FraudConfig.h"
#include "FraudRule.h"

// Flags a sender who has had several recent FAILED transactions.
class FailedAttemptRule : public FraudRule {
public:
    // Holds a reference: the DatabaseManager must outlive this rule.
    FailedAttemptRule(DatabaseManager& db, const FraudConfig& config)
        : db_(db), config_(config) {}

    RuleResult check(const TransactionRequest& request) const override;
    std::string displayName() const override { return "Failed Attempt Rule"; }

private:
    DatabaseManager& db_;
    FraudConfig config_;
};
