#pragma once

#include <optional>
#include <vector>

#include "Customer.h"
#include "DatabaseManager.h"

// All SQL for customers lives here. Customer itself knows nothing about MySQL.
class CustomerRepository {
public:
    // Holds a reference: the DatabaseManager must outlive this object.
    explicit CustomerRepository(DatabaseManager& db) : db_(db) {}

    // Saves a new customer, sets its id, and returns that id.
    // Throws std::runtime_error if the email is already registered.
    int create(Customer& customer);

    // Returns the customer, or std::nullopt if the id doesn't exist.
    std::optional<Customer> findById(int userId);

    std::vector<Customer> findAll();

    // Saves the customer's current name and email.
    void updateProfile(const Customer& customer);

    void updateKycStatus(int userId, KycStatus status);

private:
    DatabaseManager& db_;
};