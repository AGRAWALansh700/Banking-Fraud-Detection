#pragma once

#include <optional>

#include "AccountRepository.h"
#include "DatabaseManager.h"
#include "FraudEngine.h"
#include "TransactionRepository.h"

// What a money operation returns.
struct TransactionOutcome {
    int transactionId = 0;
    TransactionStatus status = TransactionStatus::Completed;
    // Present only when a fraud engine is attached.
    std::optional<FraudReport> fraud;
    bool moneyMoved = true;  // false when the transaction was blocked
};

// Business flow for moving money. Owns the database transaction boundaries.
//
// Throws std::invalid_argument for bad input (amount <= 0, same account, ...)
// and std::runtime_error for banking failures (unknown account, insufficient balance).
// Insufficient balance also records a FAILED transaction row before throwing.
//
// If a FraudEngine is attached, every request is analyzed first:
//   LOW -> COMPLETED, MEDIUM/HIGH -> FLAGGED (money moves), CRITICAL -> BLOCKED (money stays).
class TransactionService {
public:
    TransactionService(DatabaseManager& db, AccountRepository& accounts,
                       TransactionRepository& transactions)
        : db_(db), accounts_(accounts), transactions_(transactions) {}

    // The engine is not owned. It must outlive the service. Pass nullptr to detach.
    void setFraudEngine(const FraudEngine* engine) { engine_ = engine; }

    TransactionOutcome deposit(int accountId, Paise amount);
    TransactionOutcome withdraw(int accountId, Paise amount);
    TransactionOutcome transfer(int fromAccountId, int toAccountId, Paise amount);

private:
    // Runs the engine, or returns an empty optional when none is attached.
    std::optional<FraudReport> analyze(const TransactionRequest& request) const;

    // Status for money that will move, based on the report (COMPLETED or FLAGGED).
    static TransactionStatus statusFor(const std::optional<FraudReport>& report);

    static bool isBlocked(const std::optional<FraudReport>& report);

    // Records a BLOCKED row plus its alerts in one small transaction.
    TransactionOutcome recordBlocked(const TransactionRequest& request,
                                     const FraudReport& report);

    // Records a FAILED row. Called after the money work has been rolled back.
    void recordFailure(int sender, int receiver, Paise amount, TransactionType type);

    // Saves alerts for a report if an engine is attached and rules triggered.
    void saveAlertsIfAny(int transactionId, const std::optional<FraudReport>& report);

    DatabaseManager& db_;
    AccountRepository& accounts_;
    TransactionRepository& transactions_;
    const FraudEngine* engine_ = nullptr;
};