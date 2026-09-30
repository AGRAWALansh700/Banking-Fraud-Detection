#include "FailedAttemptRule.h"

#include <string>

RuleResult FailedAttemptRule::check(const TransactionRequest& request) const {
    RuleResult result;
    result.ruleName = "FAILED_ATTEMPTS";

    if (request.senderAccount == 0) {
        result.message = "No sender account (nothing to check)";
        return result;
    }

    ResultSet rows = db_.query(
        "SELECT COUNT(*) FROM Transactions "
        "WHERE sender_account = ? AND status = 'FAILED' "
        "AND timestamp >= NOW() - INTERVAL ? MINUTE",
        {std::to_string(request.senderAccount),
         std::to_string(config_.failedWindowMinutes)});

    int failures = std::stoi(rows[0][0]);

    if (failures >= config_.failedCountThreshold) {
        result.triggered = true;
        result.points = config_.failedPoints;
        result.message = std::to_string(failures) + " failed transactions in the last " +
                         std::to_string(config_.failedWindowMinutes) +
                         " minutes (limit " +
                         std::to_string(config_.failedCountThreshold) + ")";
    } else {
        result.message = std::to_string(failures) + " recent failed transaction(s)";
    }
    return result;
}