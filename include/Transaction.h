#pragma once

#include <string>

#include "Money.h"

enum class TransactionType { Deposit, Withdrawal, Transfer };
enum class TransactionStatus { Pending, Completed, Failed, Flagged, Blocked };

std::string transactionTypeToString(TransactionType type);
TransactionType transactionTypeFromString(const std::string& text);
std::string transactionStatusToString(TransactionStatus status);
TransactionStatus transactionStatusFromString(const std::string& text);

// A read-only record of one money movement (loaded from the database).
// senderAccount / receiverAccount are 0 when not applicable
// (deposits have no sender, withdrawals have no receiver).
class Transaction {
public:
    Transaction(int id, int senderAccount, int receiverAccount, Paise amount,
                TransactionType type, TransactionStatus status,
                const std::string& timestamp)
        : id_(id), senderAccount_(senderAccount), receiverAccount_(receiverAccount),
          amount_(amount), type_(type), status_(status), timestamp_(timestamp) {}

    int getId() const { return id_; }
    int getSenderAccount() const { return senderAccount_; }
    int getReceiverAccount() const { return receiverAccount_; }
    Paise getAmount() const { return amount_; }
    TransactionType getType() const { return type_; }
    TransactionStatus getStatus() const { return status_; }
    const std::string& getTimestamp() const { return timestamp_; }

    void display() const;

private:
    int id_;
    int senderAccount_;
    int receiverAccount_;
    Paise amount_;
    TransactionType type_;
    TransactionStatus status_;
    std::string timestamp_;
};