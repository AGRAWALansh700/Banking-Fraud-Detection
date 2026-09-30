#include "UnusualTransactionRule.h"

#include <string>

RuleResult UnusualTransactionRule::check(const TransactionRequest& request) const {
    RuleResult result;
    result.ruleName = "UNUSUAL_AMOUNT";

    if (request.senderAccount == 0) {
        result.message = "No sender account (nothing to check)";
        return result;
    }

    // COUNT and the rounded AVERAGE in one query.
    // CAST(... AS DECIMAL(15,2)) keeps the average to 2 decimal places,
    // which is the only format money::parse accepts.
    ResultSet rows = db_.query(
        "SELECT COUNT(*), CAST(AVG(amount) AS DECIMAL(15,2)) FROM Transactions "
        "WHERE sender_account = ? AND status = 'COMPLETED'",
        {std::to_string(request.senderAccount)});

    int history = std::stoi(rows[0][0]);
    if (history < config_.unusualMinHistory) {
        result.message = "Only " + std::to_string(history) +
                         " completed transaction(s); need " +
                         std::to_string(config_.unusualMinHistory) +
                         " to judge what is unusual";
        return result;
    }

    Paise average = money::parse(rows[0][1]);
    Paise limit = average * config_.unusualMultiplier;

    if (request.amount >= limit) {
        result.triggered = true;
        result.points = config_.unusualPoints;
        result.message = "Amount Rs " + money::format(request.amount) + " is at least " +
                         std::to_string(config_.unusualMultiplier) +
                         "x the sender's average Rs " + money::format(average);
    } else {
        result.message = "Amount Rs " + money::format(request.amount) +
                         " is normal for average Rs " + money::format(average);
    }
    return result;
}