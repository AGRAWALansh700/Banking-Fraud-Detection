#pragma once

#include "DatabaseManager.h"
#include "FraudConfig.h"
#include "FraudRule.h"

// Flags a sender who makes too many transactions within a short window.
// Reads the sender's recent history from the database.
class RapidTransactionRule : public FraudRule {
public:
    // Holds a reference: the DatabaseManager must outlive this rule.
    RapidTransactionRule(DatabaseManager& db, const FraudConfig& config)
        : db_(db), config_(config) {}

    RuleResult check(const TransactionRequest& request) const override;
    std::string displayName() const override { return "Rapid Transaction Rule"; }

private:
    DatabaseManager& db_;
    FraudConfig config_;
};