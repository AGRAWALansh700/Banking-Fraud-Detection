#include <iostream>
#include <memory>

#include "AccountRepository.h"
#include "Config.h"
#include "CustomerRepository.h"
#include "DatabaseManager.h"
#include "FailedAttemptRule.h"
#include "FraudConfig.h"
#include "FraudEngine.h"
#include "HighAmountRule.h"
#include "RapidTransactionRule.h"
#include "Simulator.h"
#include "TransactionRepository.h"
#include "TransactionService.h"
#include "TransferChainRule.h"
#include "UnusualTransactionRule.h"

int main() {
    std::cout << "=====================================\n";
    std::cout << "  Banking & Fraud Detection System\n";
    std::cout << "=====================================\n";

    try {
        DbConfig dbConfig = loadDbConfig();
                if (dbConfig.user == "root") {
            std::cerr << "[WARNING] Connected as MySQL 'root'. "
                         "Use a limited account such as 'bank_app' instead.\n";
        }
        DatabaseManager db(dbConfig);
        CustomerRepository customers(db);
        AccountRepository accounts(db);
        TransactionRepository transactions(db);
        TransactionService service(db, accounts, transactions);

        FraudConfig config;
        FraudEngine engine(config);
        engine.addRule(std::make_unique<HighAmountRule>(config));
        engine.addRule(std::make_unique<RapidTransactionRule>(db, config));
        engine.addRule(std::make_unique<FailedAttemptRule>(db, config));
        engine.addRule(std::make_unique<UnusualTransactionRule>(db, config));
        engine.addRule(std::make_unique<TransferChainRule>(db, config));
        service.setFraudEngine(&engine);

        Simulator simulator(db, customers, accounts, service, engine);
        simulator.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }
    return 0;
}