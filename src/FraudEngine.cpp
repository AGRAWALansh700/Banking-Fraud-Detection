#include "FraudEngine.h"

#include <iostream>
#include <stdexcept>
#include <string>

std::string riskLevelToString(RiskLevel level) {
    switch (level) {
        case RiskLevel::Low:      return "LOW";
        case RiskLevel::Medium:   return "MEDIUM";
        case RiskLevel::High:     return "HIGH";
        case RiskLevel::Critical: return "CRITICAL";
    }
    throw std::invalid_argument("Unknown risk level");
}

std::string actionToString(RecommendedAction action) {
    switch (action) {
        case RecommendedAction::Allow:  return "ALLOW";
        case RecommendedAction::Flag:   return "FLAG TRANSACTION";
        case RecommendedAction::Review: return "HOLD FOR REVIEW";
        case RecommendedAction::Block:  return "BLOCK TRANSACTION";
    }
    throw std::invalid_argument("Unknown action");
}

void FraudEngine::addRule(std::unique_ptr<FraudRule> rule) {
    if (!rule) {
        throw std::invalid_argument("Cannot add a null fraud rule");
    }
    rules_.push_back(std::move(rule));
}

RiskLevel FraudEngine::levelForScore(int score) const {
    if (score >= config_.criticalFrom) return RiskLevel::Critical;
    if (score >= config_.highFrom)     return RiskLevel::High;
    if (score >= config_.mediumFrom)   return RiskLevel::Medium;
    return RiskLevel::Low;
}

RecommendedAction FraudEngine::actionForLevel(RiskLevel level) {
    switch (level) {
        case RiskLevel::Low:      return RecommendedAction::Allow;
        case RiskLevel::Medium:   return RecommendedAction::Flag;
        case RiskLevel::High:     return RecommendedAction::Review;
        case RiskLevel::Critical: return RecommendedAction::Block;
    }
    throw std::invalid_argument("Unknown risk level");
}

FraudReport FraudEngine::analyze(const TransactionRequest& request) const {
    FraudReport report;
    for (const auto& rule : rules_) {
        RuleResult result = rule->check(request);  // virtual call
        report.riskScore += result.points;         // points are 0 unless triggered
        report.ruleNames.push_back(rule->displayName());
        report.results.push_back(std::move(result));
    }
    report.level = levelForScore(report.riskScore);
    report.action = actionForLevel(report.level);
    return report;
}

void FraudEngine::saveAlerts(DatabaseManager& db, int transactionId,
                             const FraudReport& report) const {
    for (const RuleResult& result : report.results) {
        if (!result.triggered) {
            continue;  // only triggered rules become alerts
        }
        db.execute(
            "INSERT INTO FraudAlerts (transaction_id, rule_type, rule_points, message) "
            "VALUES (?, ?, ?, ?)",
            {std::to_string(transactionId), result.ruleName,
             std::to_string(result.points), result.message});
    }
}

void FraudEngine::printReport(const TransactionRequest& request, const FraudReport& report) {
    std::cout << "  Amount: Rs " << money::format(request.amount) << "\n"
              << "  Fraud Analysis\n"
              << "  ----------------------------\n";
    for (std::size_t i = 0; i < report.results.size(); ++i) {
        std::cout << "  " << report.ruleNames[i] << "  "
                  << (report.results[i].triggered ? "FAILED" : "PASSED") << "\n";
    }
    std::cout << "  Risk Score: " << report.riskScore << "\n"
              << "  Risk Level: " << riskLevelToString(report.level) << "\n"
              << "  Action: " << actionToString(report.action) << "\n";
}