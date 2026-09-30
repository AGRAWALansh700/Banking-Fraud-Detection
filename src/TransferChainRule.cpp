#include "TransferChainRule.h"

#include <string>
#include <unordered_set>

namespace {

// Hard stop so a very long chain can never make the walk run forever
const int kMaxDepth = 20;

}  // namespace

RuleResult TransferChainRule::check(const TransactionRequest& request) const {
    RuleResult result;
    result.ruleName = "TRANSFER_CHAIN";

    // Only account-to-account transfers can form a chain.
    if (request.senderAccount == 0 || request.receiverAccount == 0) {
        result.message = "Not a transfer (nothing to check)";
        return result;
    }

    // Accounts already on the path. Starting with the receiver means
    // A -> B -> A ping-pong is not counted as a chain.
    std::unordered_set<int> visited;
    visited.insert(request.senderAccount);
    visited.insert(request.receiverAccount);

    int current = request.senderAccount;
    int stepsBack = 0;

    while (stepsBack < kMaxDepth) {
        // Who recently sent money INTO the current account? Most recent sender first.
        ResultSet rows = db_.query(
            "SELECT sender_account FROM Transactions "
            "WHERE receiver_account = ? AND transaction_type = 'TRANSFER' "
            "AND status IN ('COMPLETED', 'FLAGGED') "
            "AND timestamp >= NOW() - INTERVAL ? MINUTE "
            "GROUP BY sender_account ORDER BY MAX(transaction_id) DESC LIMIT 50",
            {std::to_string(current), std::to_string(config_.chainWindowMinutes)});

        int next = 0;
        for (const Row& row : rows) {
            int candidate = std::stoi(row[0]);
            if (visited.count(candidate) == 0) {
                next = candidate;
                break;
            }
        }
        if (next == 0) {
            break;  // nobody new fed this account: the chain starts here
        }

        visited.insert(next);
        current = next;
        ++stepsBack;
    }

    int hops = 1 + stepsBack;  // +1 is the transfer being checked now

    if (hops >= config_.chainHopsThreshold) {
        result.triggered = true;
        result.points = config_.chainPoints;
        result.message = "Funds passed through a chain of " + std::to_string(hops) +
                         " transfers within " + std::to_string(config_.chainWindowMinutes) +
                         " minutes (limit " + std::to_string(config_.chainHopsThreshold) + ")";
    } else {
        result.message = "Chain of " + std::to_string(hops) + " transfer(s)";
    }
    return result;
}