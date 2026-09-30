#pragma once

#include <optional>
#include <vector>

#include "DatabaseManager.h"
#include "Transaction.h"

// All SQL for the Transactions table. Contains no business rules.
class TransactionRepository {
public:
    explicit TransactionRepository(DatabaseManager& db) : db_(db) {}

    // Inserts a row and returns its id. Use 0 for "no account" (stored as SQL NULL).
    int insert(int senderAccount, int receiverAccount, Paise amount,
               TransactionType type, TransactionStatus status);

    std::optional<Transaction> findById(int transactionId);

    // All transactions where the account is sender or receiver, newest first.
    std::vector<Transaction> findByAccount(int accountId);

private:
    DatabaseManager& db_;
};