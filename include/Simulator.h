#pragma once

#include <string>

#include "AccountRepository.h"
#include "CustomerRepository.h"
#include "DatabaseManager.h"
#include "FraudEngine.h"
#include "TransactionService.h"

// Interactive demo. Generates scenarios and runs them through the real
// TransactionService (with the fraud engine attached), printing each result.
// Demo data stays in the database (emails end with @sim.local) until cleared.
class Simulator {
public:
    Simulator(DatabaseManager& db, CustomerRepository& customers,
              AccountRepository& accounts, TransactionService& service,
              const FraudEngine& engine)
        : db_(db), customers_(customers), accounts_(accounts),
          service_(service), engine_(engine) {}

    void run();  // shows the menu until the user exits

private:
    void normalTransaction();
    void highValueTransaction();
    void rapidTransactions();
    void failedAttempts();
    void suspiciousChain();
    void randomTransactions();
    void clearDemoData();

    // Creates a demo customer + savings account, optionally funded. Returns the account id.
    int createDemoAccount(const std::string& label, const char* initialDeposit);

    // Analyze (to show the report), then execute through the service.
    void transferWithReport(int from, int to, const char* amount);
    void showOutcome(const TransactionOutcome& outcome);

    DatabaseManager& db_;
    CustomerRepository& customers_;
    AccountRepository& accounts_;
    TransactionService& service_;
    const FraudEngine& engine_;
    int counter_ = 0;  // makes demo emails unique within a run
};