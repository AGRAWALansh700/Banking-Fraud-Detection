#pragma once

#include <string>

#include "Money.h"

enum class AccountType { Savings, Current };

std::string accountTypeToString(AccountType type);
AccountType accountTypeFromString(const std::string& text);  // throws std::invalid_argument

class Account {
public:
    // New account, not yet saved (id 0), balance starts at 0
    Account(int userId, AccountType type);

    // Account loaded from the database
    Account(int accountId, int userId, AccountType type, Paise balance);

    int getAccountId() const { return accountId_; }
    int getUserId() const { return userId_; }
    AccountType getType() const { return type_; }
    Paise getBalance() const { return balance_; }

    void setAccountId(int id) { accountId_ = id; }

    // Both throw std::invalid_argument if amount <= 0,
    // and withdraw throws std::runtime_error on insufficient balance.
    void deposit(Paise amount);
    void withdraw(Paise amount);

    void display() const;

private:
    int accountId_;  // 0 means "not saved yet"
    int userId_;
    AccountType type_;
    Paise balance_;
};