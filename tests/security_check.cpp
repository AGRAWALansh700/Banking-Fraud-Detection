// Security and validation checks. Run from the project root (it reads .env).
// It creates temporary rows with emails ending in ".p13@example.com",
// then deletes them and verifies the table counts are back to what they were.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "AccountRepository.h"
#include "Config.h"
#include "CustomerRepository.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "TransactionService.h"

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

// True if fn() throws exactly the given exception type (or a subclass).
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
const std::string kTestUsers =
    "SELECT user_id FROM Users WHERE email LIKE '%.p13@example.com'";
const std::string kTestAccounts =
    "SELECT account_id FROM Accounts WHERE user_id IN (" + kTestUsers + ")";

void cleanup(DatabaseManager& db) {
    db.execute("DELETE FROM Transactions WHERE sender_account IN (" + kTestAccounts +
               ") OR receiver_account IN (" + kTestAccounts + ")");
    db.execute("DELETE FROM Accounts WHERE user_id IN (" + kTestUsers + ")");
    db.execute("DELETE FROM Users WHERE email LIKE '%.p13@example.com'");
}

std::string snapshot(DatabaseManager& db) {
    return db.query("SELECT COUNT(*) FROM Users")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Accounts")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM Transactions")[0][0] + "/" +
           db.query("SELECT COUNT(*) FROM FraudAlerts")[0][0];
}

}  // namespace

int main() {
    std::cout << "=== Security and validation checks ===\n";
    try {
        DbConfig cfg = loadDbConfig();
        DatabaseManager db(cfg);
        CustomerRepository customers(db);
        AccountRepository accounts(db);
        TransactionRepository transactions(db);
        TransactionService service(db, accounts, transactions);  // no fraud engine

        cleanup(db);  // in case an earlier run stopped halfway
        std::string before = snapshot(db);
        std::cout << "Counts before (users/accounts/transactions/alerts): " << before << "\n";

        try {
            std::cout << "\nSQL injection\n";
            const std::string hostileName = "Robert'); DROP TABLE Users;--";
            Customer hostile(hostileName, "robert.p13@example.com");
            int hostileId = customers.create(hostile);
            auto loaded = customers.findById(hostileId);
            expect(loaded.has_value() && loaded->getName() == hostileName,
                   "hostile name is stored as plain text");
            expect(!db.query("SELECT COUNT(*) FROM Users").empty(),
                   "Users table still exists");

            Customer apostrophe("O Brien", "o'brien.p13@example.com");
            customers.create(apostrophe);
            expect(db.query("SELECT user_id FROM Users WHERE email = ?",
                            {"o'brien.p13@example.com"}).size() == 1,
                   "apostrophe in email is stored and found literally");
            expect(db.query("SELECT user_id FROM Users WHERE email = ?",
                            {"' OR '1'='1"}).empty(),
                   "' OR '1'='1 matches nothing");
            expect(throwsType<DatabaseException>([&] {
                       db.query("SELECT ?, ?", {"only one value"});
                   }),
                   "wrong parameter count is rejected");
            if (cfg.user != "root") {
                expect(throwsType<DatabaseException>([&] {
                           db.query("SELECT 1; DROP TABLE Users");
                       }),
                       "stacked statement is rejected");
                expect(!db.query("SELECT COUNT(*) FROM Users").empty(),
                       "Users table survived the stacked statement");
            }

            std::cout << "\nAmount validation\n";
            Customer holder("Amount Tester", "amount.p13@example.com");
            customers.create(holder);
            Account account(holder.getUserId(), AccountType::Savings);
            int acct = accounts.create(account);

            expect(throwsType<std::invalid_argument>([&] { service.deposit(acct, 0); }),
                   "zero deposit rejected");
            expect(throwsType<std::invalid_argument>(
                       [&] { service.deposit(acct, money::parse("-5")); }),
                   "negative deposit rejected");
            expect(throwsType<std::invalid_argument>(
                       [&] { service.deposit(acct, money::parse("100000000.01")); }),
                   "amount above the limit rejected");
            expect(throwsType<std::invalid_argument>(
                       [&] { service.transfer(acct, acct, money::parse("10")); }),
                   "transfer to the same account rejected");
            service.deposit(acct, money::parse("100"));
            expect(money::format(accounts.findById(acct)->getBalance()) == "100.00",
                   "valid deposit works and rejected attempts changed nothing");

            std::cout << "\nConfiguration and secrets\n";
            expect(throwsType<std::runtime_error>([&] { loadDbConfig("no_such_file.env"); }),
                   "missing .env file gives a clear error");
            {
                std::ofstream file("p13_test.env");
                file << "DB_USER=someone\nDB_NAME=somedb\n";  // no DB_PASSWORD on purpose
            }
            std::string message;
            try {
                loadDbConfig("p13_test.env");
            } catch (const std::runtime_error& e) {
                message = e.what();
            }
            std::remove("p13_test.env");
            expect(message.find("DB_PASSWORD") != std::string::npos,
                   "missing DB_PASSWORD is named in the error");

            DbConfig bad = cfg;
            bad.password = "definitely-wrong-pass-123";
            bool rejected = false;
            bool leaked = false;
            try {
                DatabaseManager wrong(bad);
            } catch (const DatabaseException& e) {
                rejected = true;
                leaked = std::string(e.what()).find(bad.password) != std::string::npos;
            }
            expect(rejected, "wrong password is rejected");
            expect(!leaked, "error message does not contain the password");

            std::cout << "\nTransaction safety\n";
            bool quietOk = true;
            try {
                db.rollbackQuietly();  // no transaction open: must not throw
            } catch (...) {
                quietOk = false;
            }
            expect(quietOk, "rollbackQuietly is safe with no open transaction");

            bool resetOk = true;
            try {
                db.beginTransaction();
                db.rollbackQuietly();
                db.beginTransaction();  // would throw if the state were stuck
                db.commit();
            } catch (...) {
                resetOk = false;
                db.rollbackQuietly();
            }
            expect(resetOk, "rollbackQuietly resets the transaction state");
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