#include "AccountRepository.h"

#include <stdexcept>
#include <string>

namespace {

// Column order used by every SELECT below: account_id, user_id, account_type, balance
Account rowToAccount(const Row& row) {
    return Account(std::stoi(row[0]), std::stoi(row[1]),
                   accountTypeFromString(row[2]), money::parse(row[3]));
}

}  // namespace

int AccountRepository::create(Account& account) {
    if (db_.query("SELECT user_id FROM Users WHERE user_id = ?",
                  {std::to_string(account.getUserId())}).empty()) {
        throw std::runtime_error("User not found: " + std::to_string(account.getUserId()));
    }
    std::string typeText = accountTypeToString(account.getType());
    if (!db_.query("SELECT account_id FROM Accounts WHERE user_id = ? AND account_type = ?",
                   {std::to_string(account.getUserId()), typeText}).empty()) {
        throw std::runtime_error("User " + std::to_string(account.getUserId()) +
                                 " already has a " + typeText + " account");
    }
    try {
        db_.execute("INSERT INTO Accounts (user_id, account_type, balance) VALUES (?, ?, ?)",
                    {std::to_string(account.getUserId()), typeText,
                     money::format(account.getBalance())});
    } catch (const DatabaseException& e) {
        // Backstop for the race between our check and the insert
        if (std::string(e.what()).find("1062") != std::string::npos) {
            throw std::runtime_error("User already has a " + typeText + " account");
        }
        throw;
    }
    int newId = static_cast<int>(db_.lastInsertId());
    account.setAccountId(newId);
    return newId;
}

std::optional<Account> AccountRepository::findById(int accountId) {
    ResultSet rows = db_.query(
        "SELECT account_id, user_id, account_type, balance FROM Accounts WHERE account_id = ?",
        {std::to_string(accountId)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return rowToAccount(rows[0]);
}

std::vector<Account> AccountRepository::findByUser(int userId) {
    ResultSet rows = db_.query(
        "SELECT account_id, user_id, account_type, balance FROM Accounts "
        "WHERE user_id = ? ORDER BY account_id",
        {std::to_string(userId)});
    std::vector<Account> accounts;
    accounts.reserve(rows.size());
    for (const Row& row : rows) {
        accounts.push_back(rowToAccount(row));
    }
    return accounts;
}

std::optional<Account> AccountRepository::findByIdForUpdate(int accountId) {
    ResultSet rows = db_.query(
        "SELECT account_id, user_id, account_type, balance FROM Accounts "
        "WHERE account_id = ? FOR UPDATE",
        {std::to_string(accountId)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return rowToAccount(rows[0]);
}

void AccountRepository::credit(int accountId, Paise amount) {
    unsigned long long changed = db_.execute(
        "UPDATE Accounts SET balance = balance + ? WHERE account_id = ?",
        {money::format(amount), std::to_string(accountId)});
    if (changed == 0) {
        throw std::runtime_error("Account not found: " + std::to_string(accountId));
    }
}

void AccountRepository::debit(int accountId, Paise amount) {
    // The balance check stays in the SQL as a second safety net
    // (the service also checks first, to give a friendly message).
    unsigned long long changed = db_.execute(
        "UPDATE Accounts SET balance = balance - ? WHERE account_id = ? AND balance >= ?",
        {money::format(amount), std::to_string(accountId), money::format(amount)});
    if (changed == 0) {
        throw std::runtime_error("Insufficient balance or account not found: " +
                                 std::to_string(accountId));
    }
}