#include "TransactionRepository.h"

#include <string>

namespace {

// Column order: transaction_id, sender_account, receiver_account, amount,
//               transaction_type, status, timestamp
// SQL NULL arrives as "" (see DatabaseManager), which we map to 0.
Transaction rowToTransaction(const Row& row) {
    int sender = row[1].empty() ? 0 : std::stoi(row[1]);
    int receiver = row[2].empty() ? 0 : std::stoi(row[2]);
    return Transaction(std::stoi(row[0]), sender, receiver, money::parse(row[3]),
                       transactionTypeFromString(row[4]),
                       transactionStatusFromString(row[5]), row[6]);
}

const char* kSelectColumns =
    "SELECT transaction_id, sender_account, receiver_account, amount, "
    "transaction_type, status, timestamp FROM Transactions ";

}  // namespace

int TransactionRepository::insert(int senderAccount, int receiverAccount, Paise amount,
                                  TransactionType type, TransactionStatus status) {
    // NULLIF turns the text "0" into SQL NULL, so "no account" is stored as NULL
    db_.execute(
        "INSERT INTO Transactions "
        "(sender_account, receiver_account, amount, transaction_type, status) "
        "VALUES (NULLIF(?, 0), NULLIF(?, 0), ?, ?, ?)",
        {std::to_string(senderAccount), std::to_string(receiverAccount),
         money::format(amount), transactionTypeToString(type),
         transactionStatusToString(status)});
    return static_cast<int>(db_.lastInsertId());
}

std::optional<Transaction> TransactionRepository::findById(int transactionId) {
    ResultSet rows = db_.query(
        std::string(kSelectColumns) + "WHERE transaction_id = ?",
        {std::to_string(transactionId)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return rowToTransaction(rows[0]);
}

std::vector<Transaction> TransactionRepository::findByAccount(int accountId) {
    ResultSet rows = db_.query(
        std::string(kSelectColumns) +
            "WHERE sender_account = ? OR receiver_account = ? "
            "ORDER BY transaction_id DESC",
        {std::to_string(accountId), std::to_string(accountId)});
    std::vector<Transaction> transactions;
    transactions.reserve(rows.size());
    for (const Row& row : rows) {
        transactions.push_back(rowToTransaction(row));
    }
    return transactions;
}