#include "Account.h"

#include <iostream>
#include <stdexcept>

std::string accountTypeToString(AccountType type) {
    switch (type) {
        case AccountType::Savings: return "SAVINGS";
        case AccountType::Current: return "CURRENT";
    }
    throw std::invalid_argument("Unknown account type");
}

AccountType accountTypeFromString(const std::string& text) {
    if (text == "SAVINGS") return AccountType::Savings;
    if (text == "CURRENT") return AccountType::Current;
    throw std::invalid_argument("Unknown account type: " + text);
}

Account::Account(int userId, AccountType type)
    : accountId_(0), userId_(userId), type_(type), balance_(0) {}

Account::Account(int accountId, int userId, AccountType type, Paise balance)
    : accountId_(accountId), userId_(userId), type_(type), balance_(balance) {
    if (balance < 0) {
        throw std::invalid_argument("Balance cannot be negative");
    }
}

void Account::deposit(Paise amount) {
    if (amount <= 0) {
        throw std::invalid_argument("Deposit amount must be greater than zero");
    }
    balance_ += amount;
}

void Account::withdraw(Paise amount) {
    if (amount <= 0) {
        throw std::invalid_argument("Withdrawal amount must be greater than zero");
    }
    if (amount > balance_) {
        throw std::runtime_error("Insufficient balance");
    }
    balance_ -= amount;
}

void Account::display() const {
    std::cout << "Account #" << accountId_ << " (" << accountTypeToString(type_) << ")\n"
              << "  Owner user id: " << userId_ << "\n"
              << "  Balance      : Rs " << money::format(balance_) << "\n";
}