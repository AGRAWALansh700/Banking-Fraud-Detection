// Performance check. Run from the project root (it reads .env).
// Loads 20,000 synthetic transfers for one throwaway sender, times each fraud rule,
// prints EXPLAIN plans, then deletes everything it created (emails end in .p15@example.com).

#include <chrono>
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
#include "HighAmountRule.h"
#include "RapidTransactionRule.h"
#include "TransferChainRule.h"
#include "UnusualTransactionRule.h"

namespace {

const int kRows = 20000;
const int kBatch = 1000;
const int kRepeats = 20;

// Fixed strings (no user input), used only to clean up this test's rows.
const std::string kUsers = "SELECT user_id FROM Users WHERE email LIKE '%.p15@example.com'";
const std::string kAccounts = "SELECT account_id FROM Accounts WHERE user_id IN (" + kUsers + ")";

void cleanup(DatabaseManager& db) {
    db.execute("DELETE FROM Transactions WHERE sender_account IN (" + kAccounts +
               ") OR receiver_account IN (" + kAccounts + ")");
    db.execute("DELETE FROM Accounts WHERE user_id IN (" + kUsers + ")");
    db.execute("DELETE FROM Users WHERE email LIKE '%.p15@example.com'");
}

std::string snapshot(DatabaseManager& db) {
    return db.query("SELECT COUNT(*) FROM Users")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Accounts")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Transactions")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM FraudAlerts")[0][0];
}

// Average milliseconds per call over kRepeats runs.
template <typename Fn>
double averageMs(Fn fn) {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRepeats; ++i) {
        fn();
    }
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / kRepeats;
}

void printPlan(DatabaseManager& db, const std::string& title, const std::string& sql,
               const std::vector<std::string>& params) {
    std::cout << "\nEXPLAIN: " << title << "\n";
    ResultSet rows = db.query("EXPLAIN " + sql, params);
    for (const Row& r : rows) {
        // columns: 2 table, 4 type, 6 key, 9 rows, 11 Extra
        std::cout << "  table=" << r[2] << " type=" << r[4] << " key=" << r[6]
                  << " rows=" << r[9] << " extra=" << r[11] << "\n";
    }
}

}  // namespace

int main() {
    std::cout << "=== Performance check (" << kRows << " transactions) ===\n";
    try {
        DbConfig cfg = loadDbConfig();
        DatabaseManager db(cfg);
        CustomerRepository customers(db);
        AccountRepository accounts(db);

        cleanup(db);
        std::string before = snapshot(db);
        std::cout << "Counts before (users/accounts/transactions/alerts): " << before << "\n";

        try {
            Customer cs("Perf Sender", "sender.p15@example.com");
            Customer cr("Perf Receiver", "receiver.p15@example.com");
            customers.create(cs);
            customers.create(cr);
            Account as(cs.getUserId(), AccountType::Savings);
            Account ar(cr.getUserId(), AccountType::Savings);
            int sender = accounts.create(as);
            int receiver = accounts.create(ar);

            std::cout << "Loading rows...\n";
            auto loadStart = std::chrono::steady_clock::now();
            db.beginTransaction();
            try {
                for (int b = 0; b < kRows / kBatch; ++b) {
                    std::string sql =
                        "INSERT INTO Transactions (sender_account, receiver_account, amount, "
                        "transaction_type, status, timestamp) VALUES ";
                    for (int j = 0; j < kBatch; ++j) {
                        int i = b * kBatch + j;
                        int rupees = 100 + (i % 50) * 100;
                        int minutesAgo = (i + 1) * 10;
                        if (j > 0) sql += ", ";
                        sql += "(" + std::to_string(sender) + ", " + std::to_string(receiver) +
                               ", " + std::to_string(rupees) +
                               ".00, 'TRANSFER', 'COMPLETED', NOW() - INTERVAL " +
                               std::to_string(minutesAgo) + " MINUTE)";
                    }
                    db.execute(sql);
                }
                db.commit();
            } catch (...) {
                db.rollbackQuietly();
                throw;
            }
            double loadMs = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - loadStart).count();
            std::cout << "Loaded " << kRows << " rows in " << loadMs << " ms\n";

            FraudConfig config;
            HighAmountRule high(config);
            RapidTransactionRule rapid(db, config);
            FailedAttemptRule failedRule(db, config);
            UnusualTransactionRule unusual(db, config);
            TransferChainRule chain(db, config);

            TransactionRequest request;
            request.senderAccount = sender;
            request.receiverAccount = receiver;
            request.amount = money::parse("100");
            request.type = TransactionType::Transfer;

            std::cout << "\nAverage time per check over " << kRepeats << " runs (ms)\n";
            std::cout << "  High Amount        : " << averageMs([&] { high.check(request); }) << "\n";
            std::cout << "  Rapid Transaction  : " << averageMs([&] { rapid.check(request); }) << "\n";
            std::cout << "  Failed Attempt     : " << averageMs([&] { failedRule.check(request); }) << "\n";
            std::cout << "  Unusual (all time) : " << averageMs([&] { unusual.check(request); }) << "\n";
            std::cout << "  Transfer Chain     : " << averageMs([&] { chain.check(request); }) << "\n";

            const std::string windowedSql =
                "SELECT COUNT(*), CAST(AVG(amount) AS DECIMAL(15,2)) FROM Transactions "
                "WHERE sender_account = ? AND status = 'COMPLETED' "
                "AND timestamp >= NOW() - INTERVAL 90 DAY";
            std::vector<std::string> senderParam{std::to_string(sender)};
            std::cout << "  Unusual (90 days)  : "
                      << averageMs([&] { db.query(windowedSql, senderParam); })
                      << "   <- candidate replacement query\n";

            printPlan(db, "Unusual rule, all time",
                      "SELECT COUNT(*), CAST(AVG(amount) AS DECIMAL(15,2)) FROM Transactions "
                      "WHERE sender_account = ? AND status = 'COMPLETED'",
                      senderParam);
            printPlan(db, "Unusual rule, 90 days", windowedSql, senderParam);
            printPlan(db, "Chain rule step",
                      "SELECT sender_account FROM Transactions "
                      "WHERE receiver_account = ? AND transaction_type = 'TRANSFER' "
                      "AND status IN ('COMPLETED', 'FLAGGED') "
                      "AND timestamp >= NOW() - INTERVAL 10 MINUTE "
                      "GROUP BY sender_account ORDER BY MAX(transaction_id) DESC LIMIT 50",
                      {std::to_string(receiver)});
        } catch (const std::exception& e) {
            std::cout << "[ERROR] " << e.what() << "\n";
        }

        cleanup(db);
        std::string after = snapshot(db);
        std::cout << "\nCounts after: " << after
                  << (after == before ? "  (match, test data removed)" : "  (MISMATCH!)") << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }
    return 0;
}