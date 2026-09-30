#pragma once

#include "DatabaseManager.h"
#include "FraudConfig.h"
#include "FraudRule.h"

// Flags money that is passed quickly through a chain of accounts (A -> B -> C -> D).
// Walks backwards from the sender through recent incoming transfers.
class TransferChainRule : public FraudRule {
public:
    // Holds a reference: the DatabaseManager must outlive this rule.
    TransferChainRule(DatabaseManager& db, const FraudConfig& config)
        : db_(db), config_(config) {}

    RuleResult check(const TransactionRequest& request) const override;
    std::string displayName() const override { return "Transfer Chain Rule"; }

private:
    DatabaseManager& db_;
    FraudConfig config_;
};