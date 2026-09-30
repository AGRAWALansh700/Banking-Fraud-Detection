#pragma once

#include <optional>
#include <vector>

#include "Account.h"
#include "DatabaseManager.h"

// All SQL for accounts lives here. Contains no business rules about
// transactions: the TransactionService decides when money moves.
class AccountRepository {
public:
    explicit AccountRepository(DatabaseManager& db) : db_(db) {}

    // Saves a new account for an existing user. Sets and returns its id.
    // Throws std::runtime_error if the user doesn't exist or already has this account type.
    int create(Account& account);

    std::optional<Account> findById(int accountId);
    std::vector<Account> findByUser(int userId);

    // Reads the account while locking its row (SELECT ... FOR UPDATE).
    // Only meaningful inside a database transaction; the lock lasts until COMMIT/ROLLBACK.
    std::optional<Account> findByIdForUpdate(int accountId);

    // Adds or subtracts money. Callers must already hold a transaction and have
    // validated the amount and balance. Both throw std::runtime_error if no row changed.
    void credit(int accountId, Paise amount);
    void debit(int accountId, Paise amount);

private:
    DatabaseManager& db_;
};