#include "HighAmountRule.h"

RuleResult HighAmountRule::check(const TransactionRequest& request) const {
    RuleResult result;
    result.ruleName = "HIGH_AMOUNT";

    if (request.amount > config_.highAmountThreshold) {
        result.triggered = true;
        result.points = config_.highAmountPoints;
        result.message = "Amount Rs " + money::format(request.amount) +
                         " exceeds threshold Rs " +
                         money::format(config_.highAmountThreshold);
    } else {
        result.message = "Amount within limit";
    }
    return result;
}