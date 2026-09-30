#include "Simulator.h"

#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

// Reads a whole line so bad input can never put std::cin into a failed state.
int readMenuChoice() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return 8;  // input closed: exit
    }
    try {
        return std::stoi(line);
    } catch (const std::exception&) {
        return -1;
    }
}

void printMenu() {
    std::cout << "\n========== TRANSACTION SIMULATOR ==========\n\n"
              << "1. Normal Transaction\n"
              << "2. High Value Transaction\n"
              << "3. Rapid Transactions\n"
              << "4. Multiple Failed Attempts\n"
              << "5. Suspicious Transfer Pattern\n"
              << "6. Random Transaction Generation\n"
              << "7. Clear demo data\n"
              << "8. Exit\n\n"
              << "Enter choice: ";
}

}  // namespace

void Simulator::run() {
    while (true) {
        printMenu();
        int choice = readMenuChoice();
        try {
            switch (choice) {
                case 1: normalTransaction(); break;
                case 2: highValueTransaction(); break;
                case 3: rapidTransactions(); break;
                case 4: failedAttempts(); break;
                case 5: suspiciousChain(); break;
                case 6: randomTransactions(); break;
                case 7: clearDemoData(); break;
                case 8: std::cout << "Goodbye.\n"; return;
                default: std::cout << "Please enter a number from 1 to 8.\n";
            }
        } catch (const std::exception& e) {
            // A failed scenario must not close the whole program
            std::cout << "[ERROR] " << e.what() << "\n";
        }
    }
}

int Simulator::createDemoAccount(const std::string& label, const char* initialDeposit) {
    // Time in ms + counter keeps emails unique across runs
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string email = "demo." + std::to_string(ms) + "." + std::to_string(++counter_) +
                        "@sim.local";

    Customer customer("Demo " + label, email);
    customers_.create(customer);
    Account account(customer.getUserId(), AccountType::Savings);
    int accountId = accounts_.create(account);

    if (initialDeposit != nullptr) {
        // Funding is not a "scenario" transaction, but it goes through the same service.
        service_.deposit(accountId, money::parse(initialDeposit));
    }
    return accountId;
}

void Simulator::showOutcome(const TransactionOutcome& outcome) {
    std::cout << "  Result: txn #" << outcome.transactionId << " "
              << transactionStatusToString(outcome.status)
              << ", money moved: " << (outcome.moneyMoved ? "yes" : "NO") << "\n";
}

void Simulator::transferWithReport(int from, int to, const char* amount) {
    TransactionRequest request;
    request.senderAccount = from;
    request.receiverAccount = to;
    request.amount = money::parse(amount);
    request.type = TransactionType::Transfer;

    std::cout << "\nTransfer Rs " << amount << " from account " << from
              << " to account " << to << "\n";
    FraudReport report = engine_.analyze(request);
    FraudEngine::printReport(request, report);
    showOutcome(service_.transfer(from, to, request.amount));
}

void Simulator::normalTransaction() {
    std::cout << "\n--- Normal transaction ---\n";
    int sender = createDemoAccount("Normal Sender", "50000");
    int receiver = createDemoAccount("Normal Receiver", nullptr);
    transferWithReport(sender, receiver, "2500");
}

void Simulator::highValueTransaction() {
    std::cout << "\n--- High value transaction ---\n";
    int sender = createDemoAccount("HighValue Sender", "300000");
    int receiver = createDemoAccount("HighValue Receiver", nullptr);
    std::cout << "Building history: 1000, 2000, 3000 (average 2000)\n";
    for (const char* amount : {"1000", "2000", "3000"}) {
        service_.transfer(sender, receiver, money::parse(amount));
    }
    transferWithReport(sender, receiver, "95000");
}

void Simulator::rapidTransactions() {
    std::cout << "\n--- Rapid transactions: 6 transfers of Rs 100 ---\n";
    int sender = createDemoAccount("Rapid Sender", "50000");
    int receiver = createDemoAccount("Rapid Receiver", nullptr);
    for (int i = 1; i <= 6; ++i) {
        std::cout << "\n#" << i;
        transferWithReport(sender, receiver, "100");
    }
}

void Simulator::failedAttempts() {
    std::cout << "\n--- Multiple failed attempts ---\n";
    int sender = createDemoAccount("Failing Sender", "100");
    int receiver = createDemoAccount("Failing Receiver", nullptr);
    for (int i = 1; i <= 3; ++i) {
        std::cout << "\nAttempt " << i << " (overdraw Rs 5000 with a balance of Rs 100)";
        try {
            transferWithReport(sender, receiver, "5000");
        } catch (const std::runtime_error& e) {
            std::cout << "  Result: FAILED - " << e.what() << "\n";
        }
    }
    std::cout << "\nAttempt 4 (a small valid transfer, now judged with 3 failures on record)";
    transferWithReport(sender, receiver, "10");
}

void Simulator::suspiciousChain() {
    std::cout << "\n--- Suspicious transfer pattern: A -> B -> C -> D ---\n";
    int a = createDemoAccount("Chain A", "100000");
    int b = createDemoAccount("Chain B", nullptr);
    int c = createDemoAccount("Chain C", nullptr);
    int d = createDemoAccount("Chain D", nullptr);
    std::cout << "Accounts: A=" << a << " B=" << b << " C=" << c << " D=" << d << "\n";
    transferWithReport(a, b, "10000");
    transferWithReport(b, c, "9000");
    transferWithReport(c, d, "8000");
}

void Simulator::randomTransactions() {
    std::cout << "\n--- Random transaction generation: 15 transfers ---\n";
    std::vector<int> pool;
    for (int i = 1; i <= 3; ++i) {
        pool.push_back(createDemoAccount("Random " + std::to_string(i), "200000"));
    }

    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> pickAccount(0, static_cast<int>(pool.size()) - 1);
    std::uniform_int_distribution<int> roll(1, 100);
    std::uniform_int_distribution<int> smallRupees(100, 5000);
    std::uniform_int_distribution<int> midRupees(10000, 45000);
    std::uniform_int_distribution<int> largeRupees(60000, 150000);

    for (int i = 1; i <= 15; ++i) {
        int from = pickAccount(rng);
        int to = pickAccount(rng);
        if (from == to) {
            to = (to + 1) % static_cast<int>(pool.size());
        }
        int r = roll(rng);
        int rupees = (r <= 70) ? smallRupees(rng) : (r <= 90) ? midRupees(rng) : largeRupees(rng);
        std::string amount = std::to_string(rupees);

        std::cout << "\n#" << i;
        try {
            transferWithReport(pool[from], pool[to], amount.c_str());
        } catch (const std::runtime_error& e) {
            std::cout << "  Result: FAILED - " << e.what() << "\n";
        }
    }
}

void Simulator::clearDemoData() {
    // Only rows belonging to @sim.local customers. Children first (foreign keys).
    const std::string demoAccounts =
        "SELECT a.account_id FROM Accounts a JOIN Users u ON u.user_id = a.user_id "
        "WHERE u.email LIKE '%@sim.local'";

    db_.beginTransaction();
    try {
        db_.execute("DELETE FROM FraudAlerts WHERE transaction_id IN "
                    "(SELECT transaction_id FROM Transactions WHERE sender_account IN (" +
                    demoAccounts + ") OR receiver_account IN (" + demoAccounts + "))");
        db_.execute("DELETE FROM Transactions WHERE sender_account IN (" + demoAccounts +
                    ") OR receiver_account IN (" + demoAccounts + ")");
        db_.execute("DELETE FROM Accounts WHERE user_id IN "
                    "(SELECT user_id FROM Users WHERE email LIKE '%@sim.local')");
        unsigned long long users =
            db_.execute("DELETE FROM Users WHERE email LIKE '%@sim.local'");
        db_.commit();
        std::cout << "Cleared demo data (" << users << " demo customers removed).\n";
    } catch (...) {
        db_.rollbackQuietly();
        throw;
    }
}