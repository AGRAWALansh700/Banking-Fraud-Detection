#include "RapidTransactionRule.h"

#include <string>

RuleResult RapidTransactionRule::check(const TransactionRequest& request) const {
    RuleResult result;
    result.ruleName = "RAPID_TRANSACTIONS";

    // Deposits have no sender, so there is no sender history to inspect.
    if (request.senderAccount == 0) {
        result.message = "No sender account (nothing to check)";
        return result;
    }

    ResultSet rows = db_.query(
        "SELECT COUNT(*) FROM Transactions "
        "WHERE sender_account = ? AND timestamp >= NOW() - INTERVAL ? SECOND",
        {std::to_string(request.senderAccount),
         std::to_string(config_.rapidWindowSeconds)});

    int previous = std::stoi(rows[0][0]);
    int total = previous + 1;  // +1 counts the transaction being checked now

    if (total >= config_.rapidCountThreshold) {
        result.triggered = true;
        result.points = config_.rapidPoints;
        result.message = std::to_string(total) + " transactions within " +
                         std::to_string(config_.rapidWindowSeconds) +
                         " seconds (limit " +
                         std::to_string(config_.rapidCountThreshold) + ")";
    } else {
        result.message = std::to_string(total) + " transaction(s) in the last " +
                         std::to_string(config_.rapidWindowSeconds) + " seconds";
    }
    return result;
}