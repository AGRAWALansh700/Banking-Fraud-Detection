#pragma once

#include <memory>
#include <string>
#include <vector>

#include "DatabaseManager.h"
#include "FraudConfig.h"
#include "FraudRule.h"

enum class RiskLevel { Low, Medium, High, Critical };
enum class RecommendedAction { Allow, Flag, Review, Block };

std::string riskLevelToString(RiskLevel level);
std::string actionToString(RecommendedAction action);

// Everything the engine concluded about one request.
struct FraudReport {
    std::vector<RuleResult> results;  // one per rule, in the order they were added
    std::vector<std::string> ruleNames;  // display names, parallel to results
    int riskScore = 0;
    RiskLevel level = RiskLevel::Low;
    RecommendedAction action = RecommendedAction::Allow;
};

class FraudEngine {
public:
    explicit FraudEngine(const FraudConfig& config) : config_(config) {}

    // Takes ownership of the rule.
    void addRule(std::unique_ptr<FraudRule> rule);

    // Runs every rule and computes score, level and action. Changes no data.
    FraudReport analyze(const TransactionRequest& request) const;

    // Stores one FraudAlerts row per triggered rule, for an already-saved transaction.
    void saveAlerts(DatabaseManager& db, int transactionId, const FraudReport& report) const;

    // Prints the report in the format from the project spec.
    static void printReport(const TransactionRequest& request, const FraudReport& report);

private:
    RiskLevel levelForScore(int score) const;
    static RecommendedAction actionForLevel(RiskLevel level);

    FraudConfig config_;
    std::vector<std::unique_ptr<FraudRule>> rules_;
};