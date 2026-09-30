// Automated tests. Run from the project root (it reads .env).
// All test rows use emails ending in ".p14@example.com" and are removed at start and end.

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "AccountRepository.h"
#include "Config.h"
#include "CustomerRepository.h"
#include "DatabaseManager.h"
#include "FailedAttemptRule.h"
#include "FraudConfig.h"
#include "FraudEngine.h"
#include "HighAmountRule.h"
#include "RapidTransactionRule.h"
#include "TransactionRepository.h"
#include "TransactionService.h"
#include "TransferChainRule.h"
#include "UnusualTransactionRule.h"

namespace {

int passed = 0;
int failed = 0;

void expect(bool condition, const std::string& name) {
    if (condition) {
        ++passed;
        std::cout << "  [PASS] " << name << "\n";
    } else {
        ++failed;
        std::cout << "  [FAIL] " << name << "\n";
    }
}

template <typename Ex, typename Fn>
bool throwsType(Fn fn) {
    try {
        fn();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// Fixed strings (no user input), used only to clean up this test's rows.
const std::string kUsers = "SELECT user_id FROM Users WHERE email LIKE '%.p14@example.com'";
const std::string kAccounts = "SELECT account_id FROM Accounts WHERE user_id IN (" + kUsers + ")";
const std::string kTxns =
    "SELECT transaction_id FROM Transactions WHERE sender_account IN (" + kAccounts +
    ") OR receiver_account IN (" + kAccounts + ")";

void cleanup(DatabaseManager& db) {
    db.execute("DELETE FROM FraudAlerts WHERE transaction_id IN (" + kTxns + ")");
    db.execute("DELETE FROM Transactions WHERE sender_account IN (" + kAccounts +
               ") OR receiver_account IN (" + kAccounts + ")");
    db.execute("DELETE FROM Accounts WHERE user_id IN (" + kUsers + ")");
    db.execute("DELETE FROM Users WHERE email LIKE '%.p14@example.com'");
}

std::string snapshot(DatabaseManager& db) {
    return db.query("SELECT COUNT(*) FROM Users")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Accounts")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Transactions")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM FraudAlerts")[0][0];
}

struct Env {
    DatabaseManager& db;
    CustomerRepository& customers;
    AccountRepository& accounts;
    TransactionRepository& transactions;
    TransactionService& service;
    int counter = 0;

    // Fresh customer + savings account, optionally funded (no fraud engine attached yet).
    int newAccount(const char* funds) {
        std::string email = "t" + std::to_string(++counter) + ".p14@example.com";
        Customer c("Tester " + std::to_string(counter), email);
        customers.create(c);
        Account a(c.getUserId(), AccountType::Savings);
        int id = accounts.create(a);
        if (funds != nullptr) {
            service.deposit(id, money::parse(funds));
        }
        return id;
    }

    std::string balance(int id) { return money::format(accounts.findById(id)->getBalance()); }

    int countTxns(int account) {
        return std::stoi(db.query("SELECT COUNT(*) FROM Transactions "
                                  "WHERE sender_account = ? OR receiver_account = ?",
                                  {std::to_string(account), std::to_string(account)})[0][0]);
    }
};

bool triggered(const FraudReport& report, const std::string& ruleName) {
    for (const RuleResult& r : report.results) {
        if (r.ruleName == ruleName && r.triggered) {
            return true;
        }
    }
    return false;
}

TransactionRequest transferRequest(int from, int to, const char* amount) {
    TransactionRequest request;
    request.senderAccount = from;
    request.receiverAccount = to;
    request.amount = money::parse(amount);
    request.type = TransactionType::Transfer;
    return request;
}

}  // namespace

int main() {
    std::cout << "=== Automated tests ===\n";
    try {
        DbConfig cfg = loadDbConfig();
        DatabaseManager db(cfg);
        CustomerRepository customers(db);
        AccountRepository accounts(db);
        TransactionRepository transactions(db);
        TransactionService service(db, accounts, transactions);
        Env env{db, customers, accounts, transactions, service};

        cleanup(db);
        std::string before = snapshot(db);
        std::cout << "Counts before (users/accounts/transactions/alerts): " << before << "\n";

        try {
            // ---------------- Account ----------------
            std::cout << "\nAccount\n";
            std::string email = "acct.p14@example.com";
            Customer owner("Account Owner", email);
            customers.create(owner);
            Account first(owner.getUserId(), AccountType::Savings);
            int firstId = accounts.create(first);
            expect(firstId > 0 && env.balance(firstId) == "0.00", "valid account created with balance 0");
            Account dup(owner.getUserId(), AccountType::Savings);
            expect(throwsType<std::runtime_error>([&] { accounts.create(dup); }),
                   "duplicate account rejected");
            Account other(owner.getUserId(), AccountType::Current);
            expect(accounts.create(other) > 0, "second account type allowed");
            Account orphan(99999999, AccountType::Savings);
            expect(throwsType<std::runtime_error>([&] { accounts.create(orphan); }),
                   "account for unknown user rejected");
            expect(throwsType<std::invalid_argument>([&] { money::parse("abc"); }),
                   "invalid amount text rejected");
            expect(throwsType<std::invalid_argument>([&] { money::parse("10.999"); }),
                   "amount with 3 decimals rejected");
            expect(throwsType<std::invalid_argument>([&] { service.deposit(firstId, 0); }),
                   "zero amount rejected");
            expect(throwsType<std::invalid_argument>(
                       [&] { service.deposit(firstId, money::parse("-1")); }),
                   "negative amount rejected");

            // ---------------- Withdrawal ----------------
            std::cout << "\nWithdrawal\n";
            int w = env.newAccount("1000");
            service.withdraw(w, money::parse("400.50"));
            expect(env.balance(w) == "599.50", "valid withdrawal reduces balance");
            int failedBefore = std::stoi(db.query(
                "SELECT COUNT(*) FROM Transactions WHERE sender_account = ? AND status = 'FAILED'",
                {std::to_string(w)})[0][0]);
            expect(throwsType<std::runtime_error>(
                       [&] { service.withdraw(w, money::parse("5000")); }),
                   "insufficient balance rejected");
            expect(env.balance(w) == "599.50", "balance unchanged after failed withdrawal");
            int failedAfter = std::stoi(db.query(
                "SELECT COUNT(*) FROM Transactions WHERE sender_account = ? AND status = 'FAILED'",
                {std::to_string(w)})[0][0]);
            expect(failedAfter == failedBefore + 1, "failed withdrawal recorded as FAILED");
            expect(throwsType<std::runtime_error>(
                       [&] { service.withdraw(99999999, money::parse("10")); }),
                   "withdrawal from invalid account rejected");

            // ---------------- Transfer ----------------
            std::cout << "\nTransfer\n";
            int s = env.newAccount("1000");
            int r = env.newAccount(nullptr);
            service.transfer(s, r, money::parse("250.25"));
            expect(env.balance(s) == "749.75" && env.balance(r) == "250.25",
                   "valid transfer moves exactly the amount");
            expect(throwsType<std::runtime_error>(
                       [&] { service.transfer(99999999, r, money::parse("10")); }),
                   "invalid sender rejected");
            int txnsBefore = env.countTxns(s);
            expect(throwsType<std::runtime_error>(
                       [&] { service.transfer(s, 99999999, money::parse("10")); }),
                   "invalid receiver rejected");
            expect(env.balance(s) == "749.75", "sender unchanged after invalid receiver (rollback)");
            expect(env.countTxns(s) == txnsBefore, "no transaction recorded for invalid receiver");
            expect(throwsType<std::runtime_error>(
                       [&] { service.transfer(s, r, money::parse("99999")); }),
                   "insufficient balance rejected");
            expect(env.balance(s) == "749.75" && env.balance(r) == "250.25",
                   "both balances unchanged after failed transfer");

            // Database-level protection: even bypassing the C++ checks, MySQL refuses a negative balance.
            expect(throwsType<DatabaseException>([&] {
                       db.execute("UPDATE Accounts SET balance = -1 WHERE account_id = ?",
                                  {std::to_string(s)});
                   }),
                   "database CHECK refuses a negative balance");
            expect(env.balance(s) == "749.75", "balance intact after the refused update");

            // Money is conserved: the total of the two accounts never changed
            expect(money::parse(env.balance(s)) + money::parse(env.balance(r)) ==
                       money::parse("1000"),
                   "total money conserved across the transfer");

            // ---------------- Fraud ----------------
            std::cout << "\nFraud\n";
            FraudConfig config;
            FraudEngine engine(config);
            engine.addRule(std::make_unique<HighAmountRule>(config));
            engine.addRule(std::make_unique<RapidTransactionRule>(db, config));
            engine.addRule(std::make_unique<FailedAttemptRule>(db, config));
            engine.addRule(std::make_unique<UnusualTransactionRule>(db, config));
            engine.addRule(std::make_unique<TransferChainRule>(db, config));

            // Normal
            int n1 = env.newAccount("50000");
            int n2 = env.newAccount(nullptr);
            FraudReport normal = engine.analyze(transferRequest(n1, n2, "2500"));
            expect(normal.riskScore == 0 && normal.level == RiskLevel::Low &&
                       normal.action == RecommendedAction::Allow,
                   "normal transaction: score 0, LOW, ALLOW");

            // High amount
            FraudReport high = engine.analyze(transferRequest(n1, n2, "60000"));
            expect(triggered(high, "HIGH_AMOUNT"), "high amount rule triggers above the limit");
            FraudReport atLimit = engine.analyze(transferRequest(n1, n2, "50000"));
            expect(!triggered(atLimit, "HIGH_AMOUNT"), "high amount rule does not trigger at exactly the limit");

            // Rapid: 4 previous transfers + this one = 5
            int rp1 = env.newAccount("50000");
            int rp2 = env.newAccount(nullptr);
            for (int i = 0; i < 4; ++i) {
                service.transfer(rp1, rp2, money::parse("10"));
            }
            FraudReport rapid = engine.analyze(transferRequest(rp1, rp2, "10"));
            expect(triggered(rapid, "RAPID_TRANSACTIONS"), "rapid rule triggers on the 5th transfer");
            FraudReport notYet = engine.analyze(transferRequest(n1, n2, "10"));
            expect(!triggered(notYet, "RAPID_TRANSACTIONS"), "rapid rule quiet for a quiet sender");

            // Failed attempts
            int f1 = env.newAccount("100");
            int f2 = env.newAccount(nullptr);
            for (int i = 0; i < 3; ++i) {
                try {
                    service.transfer(f1, f2, money::parse("5000"));
                } catch (const std::runtime_error&) {
                }
            }
            FraudReport fails = engine.analyze(transferRequest(f1, f2, "10"));
            expect(triggered(fails, "FAILED_ATTEMPTS"), "failed attempt rule triggers after 3 failures");

            // Unusual: history 1000, 2000, 3000 (average 2000)
            int u1 = env.newAccount("300000");
            int u2 = env.newAccount(nullptr);
            for (const char* amt : {"1000", "2000", "3000"}) {
                service.transfer(u1, u2, money::parse(amt));
            }
            FraudReport unusual = engine.analyze(transferRequest(u1, u2, "20000"));
            expect(triggered(unusual, "UNUSUAL_AMOUNT"), "unusual rule triggers at 10x the average");
            FraudReport usual = engine.analyze(transferRequest(u1, u2, "5000"));
            expect(!triggered(usual, "UNUSUAL_AMOUNT"), "unusual rule quiet for a normal amount");

            // Score and level mapping
            FraudReport combo = engine.analyze(transferRequest(u1, u2, "95000"));
            expect(combo.riskScore == 45 && combo.level == RiskLevel::Medium &&
                       combo.action == RecommendedAction::Flag,
                   "95000 after small history: score 45, MEDIUM, FLAG");

            // Enforcement through the service: MEDIUM moves money but is FLAGGED
            service.setFraudEngine(&engine);
            TransactionOutcome flagged = service.transfer(u1, u2, money::parse("95000"));
            expect(flagged.status == TransactionStatus::Flagged && flagged.moneyMoved,
                   "MEDIUM transfer is FLAGGED and money moves");
            long long alertPoints = std::stoll(db.query(
                "SELECT COALESCE(SUM(rule_points), 0) FROM FraudAlerts WHERE transaction_id = ?",
                {std::to_string(flagged.transactionId)})[0][0]);
            expect(alertPoints == 45, "stored alert points match the risk score (45)");

            // CRITICAL is blocked and money does not move
            int c1 = env.newAccount("500000");
            int c2 = env.newAccount(nullptr);
            for (int i = 0; i < 3; ++i) {
                service.transfer(c1, c2, money::parse("1000"));
            }
            for (int i = 0; i < 3; ++i) {
                try {
                    service.transfer(c1, c2, money::parse("9999999"));
                } catch (const std::runtime_error&) {
                }
            }
            std::string s1 = env.balance(c1);
            std::string s2 = env.balance(c2);
            TransactionOutcome blocked = service.transfer(c1, c2, money::parse("95000"));
            expect(blocked.status == TransactionStatus::Blocked && !blocked.moneyMoved,
                   "CRITICAL transfer is BLOCKED");
            expect(env.balance(c1) == s1 && env.balance(c2) == s2,
                   "balances unchanged after a blocked transfer");
            service.setFraudEngine(nullptr);
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "  [FAIL] unexpected exception: " << e.what() << "\n";
        }

        cleanup(db);
        std::string after = snapshot(db);
        expect(after == before, "test data removed, counts back to " + before);
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}