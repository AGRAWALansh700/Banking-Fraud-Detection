#include "Transaction.h"

#include <iostream>
#include <stdexcept>

std::string transactionTypeToString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:    return "DEPOSIT";
        case TransactionType::Withdrawal: return "WITHDRAWAL";
        case TransactionType::Transfer:   return "TRANSFER";
    }
    throw std::invalid_argument("Unknown transaction type");
}

TransactionType transactionTypeFromString(const std::string& text) {
    if (text == "DEPOSIT")    return TransactionType::Deposit;
    if (text == "WITHDRAWAL") return TransactionType::Withdrawal;
    if (text == "TRANSFER")   return TransactionType::Transfer;
    throw std::invalid_argument("Unknown transaction type: " + text);
}

std::string transactionStatusToString(TransactionStatus status) {
    switch (status) {
        case TransactionStatus::Pending:   return "PENDING";
        case TransactionStatus::Completed: return "COMPLETED";
        case TransactionStatus::Failed:    return "FAILED";
        case TransactionStatus::Flagged:   return "FLAGGED";
        case TransactionStatus::Blocked:   return "BLOCKED";
    }
    throw std::invalid_argument("Unknown transaction status");
}

TransactionStatus transactionStatusFromString(const std::string& text) {
    if (text == "PENDING")   return TransactionStatus::Pending;
    if (text == "COMPLETED") return TransactionStatus::Completed;
    if (text == "FAILED")    return TransactionStatus::Failed;
    if (text == "FLAGGED")   return TransactionStatus::Flagged;
    if (text == "BLOCKED")   return TransactionStatus::Blocked;
    throw std::invalid_argument("Unknown transaction status: " + text);
}

void Transaction::display() const {
    std::cout << "Transaction #" << id_ << " " << transactionTypeToString(type_)
              << " Rs " << money::format(amount_)
              << " [" << transactionStatusToString(status_) << "] " << timestamp_;
    if (senderAccount_ != 0) {
        std::cout << " from acct " << senderAccount_;
    }
    if (receiverAccount_ != 0) {
        std::cout << " to acct " << receiverAccount_;
    }
    std::cout << "\n";
}