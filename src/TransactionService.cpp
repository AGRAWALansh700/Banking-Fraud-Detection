#include "TransactionService.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

// Largest single transaction the system accepts: Rs 100,000,000.00
const Paise kMaxTransactionAmount = 10000000000LL;

// Validates an amount: it must be positive and within the limit.
void requirePositive(Paise amount) {
    if (amount <= 0) {
        throw std::invalid_argument("Amount must be greater than zero");
    }
    if (amount > kMaxTransactionAmount) {
        throw std::invalid_argument("Amount cannot exceed Rs " +
                                    money::format(kMaxTransactionAmount));
    }
}

}  // namespace

std::optional<FraudReport> TransactionService::analyze(const TransactionRequest& request) const {
    if (engine_ == nullptr) {
        return std::nullopt;
    }
    return engine_->analyze(request);
}

TransactionStatus TransactionService::statusFor(const std::optional<FraudReport>& report) {
    if (report && (report->level == RiskLevel::Medium || report->level == RiskLevel::High)) {
        return TransactionStatus::Flagged;
    }
    return TransactionStatus::Completed;
}

bool TransactionService::isBlocked(const std::optional<FraudReport>& report) {
    return report && report->level == RiskLevel::Critical;
}

void TransactionService::saveAlertsIfAny(int transactionId,
                                         const std::optional<FraudReport>& report) {
    if (engine_ != nullptr && report) {
        engine_->saveAlerts(db_, transactionId, *report);
    }
}

TransactionOutcome TransactionService::recordBlocked(const TransactionRequest& request,
                                                     const FraudReport& report) {
    db_.beginTransaction();
    try {
        int id = transactions_.insert(request.senderAccount, request.receiverAccount,
                                      request.amount, request.type,
                                      TransactionStatus::Blocked);
        engine_->saveAlerts(db_, id, report);
        db_.commit();

        TransactionOutcome outcome;
        outcome.transactionId = id;
        outcome.status = TransactionStatus::Blocked;
        outcome.fraud = report;
        outcome.moneyMoved = false;
        return outcome;
    } catch (...) {
        db_.rollbackQuietly();
        throw;
    }
}

void TransactionService::recordFailure(int sender, int receiver, Paise amount,
                                       TransactionType type) {
    // Its own tiny transaction, after the failed one was rolled back,
    // so the FAILED row is kept even though the money work was undone.
    db_.beginTransaction();
    try {
        transactions_.insert(sender, receiver, amount, type, TransactionStatus::Failed);
        db_.commit();
    } catch (...) {
        db_.rollbackQuietly();
        throw;
    }
}

TransactionOutcome TransactionService::deposit(int accountId, Paise amount) {
    requirePositive(amount);

    TransactionRequest request;
    request.receiverAccount = accountId;
    request.amount = amount;
    request.type = TransactionType::Deposit;

    std::optional<FraudReport> report = analyze(request);
    if (isBlocked(report)) {
        return recordBlocked(request, *report);
    }

    db_.beginTransaction();
    try {
        if (!accounts_.findByIdForUpdate(accountId)) {
            throw std::runtime_error("Account not found: " + std::to_string(accountId));
        }
        accounts_.credit(accountId, amount);
        TransactionStatus status = statusFor(report);
        int id = transactions_.insert(0, accountId, amount, TransactionType::Deposit, status);
        saveAlertsIfAny(id, report);
        db_.commit();

        TransactionOutcome outcome;
        outcome.transactionId = id;
        outcome.status = status;
        outcome.fraud = report;
        return outcome;
    } catch (...) {
        db_.rollbackQuietly();
        throw;
    }
}

TransactionOutcome TransactionService::withdraw(int accountId, Paise amount) {
    requirePositive(amount);

    TransactionRequest request;
    request.senderAccount = accountId;
    request.amount = amount;
    request.type = TransactionType::Withdrawal;

    std::optional<FraudReport> report = analyze(request);
    if (isBlocked(report)) {
        return recordBlocked(request, *report);
    }

    bool insufficient = false;
    try {
        db_.beginTransaction();
        try {
            std::optional<Account> account = accounts_.findByIdForUpdate(accountId);
            if (!account) {
                throw std::runtime_error("Account not found: " + std::to_string(accountId));
            }
            if (account->getBalance() < amount) {
                insufficient = true;
                throw std::runtime_error("Insufficient balance");
            }
            accounts_.debit(accountId, amount);
            TransactionStatus status = statusFor(report);
            int id = transactions_.insert(accountId, 0, amount,
                                          TransactionType::Withdrawal, status);
            saveAlertsIfAny(id, report);
            db_.commit();

            TransactionOutcome outcome;
            outcome.transactionId = id;
            outcome.status = status;
            outcome.fraud = report;
            return outcome;
        } catch (...) {
            db_.rollbackQuietly();
            throw;
        }
    } catch (...) {
        if (insufficient) {
            recordFailure(accountId, 0, amount, TransactionType::Withdrawal);
        }
        throw;
    }
}

TransactionOutcome TransactionService::transfer(int fromAccountId, int toAccountId,
                                                Paise amount) {
    requirePositive(amount);
    if (fromAccountId == toAccountId) {
        throw std::invalid_argument("Cannot transfer to the same account");
    }

    TransactionRequest request;
    request.senderAccount = fromAccountId;
    request.receiverAccount = toAccountId;
    request.amount = amount;
    request.type = TransactionType::Transfer;

    std::optional<FraudReport> report = analyze(request);
    if (isBlocked(report)) {
        return recordBlocked(request, *report);
    }

    bool insufficient = false;
    try {
        db_.beginTransaction();
        try {
            // Lock in ascending id order so two opposite transfers can't deadlock
            int first = std::min(fromAccountId, toAccountId);
            int second = std::max(fromAccountId, toAccountId);
            std::optional<Account> a = accounts_.findByIdForUpdate(first);
            std::optional<Account> b = accounts_.findByIdForUpdate(second);
            if (!a) {
                throw std::runtime_error("Account not found: " + std::to_string(first));
            }
            if (!b) {
                throw std::runtime_error("Account not found: " + std::to_string(second));
            }
            const Account& sender = (fromAccountId == first) ? *a : *b;
            if (sender.getBalance() < amount) {
                insufficient = true;
                throw std::runtime_error("Insufficient balance");
            }
            accounts_.debit(fromAccountId, amount);
            accounts_.credit(toAccountId, amount);
            TransactionStatus status = statusFor(report);
            int id = transactions_.insert(fromAccountId, toAccountId, amount,
                                          TransactionType::Transfer, status);
            saveAlertsIfAny(id, report);
            db_.commit();

            TransactionOutcome outcome;
            outcome.transactionId = id;
            outcome.status = status;
            outcome.fraud = report;
            return outcome;
        } catch (...) {
            db_.rollbackQuietly();
            throw;
        }
    } catch (...) {
        if (insufficient) {
            recordFailure(fromAccountId, toAccountId, amount, TransactionType::Transfer);
        }
        throw;
    }
}