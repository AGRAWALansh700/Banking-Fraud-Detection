#include "CustomerRepository.h"

#include <stdexcept>
#include <string>

namespace {

// Column order used by every SELECT below: user_id, name, email, kyc_status
Customer rowToCustomer(const Row& row) {
    return Customer(std::stoi(row[0]), row[1], row[2], kycFromString(row[3]));
}

bool emailTaken(DatabaseManager& db, const std::string& email, int exceptUserId) {
    ResultSet rows = db.query(
        "SELECT user_id FROM Users WHERE email = ? AND user_id <> ?",
        {email, std::to_string(exceptUserId)});
    return !rows.empty();
}

}  // namespace

int CustomerRepository::create(Customer& customer) {
    if (emailTaken(db_, customer.getEmail(), 0)) {
        throw std::runtime_error("Email already registered: " + customer.getEmail());
    }
    try {
        db_.execute("INSERT INTO Users (name, email, kyc_status) VALUES (?, ?, ?)",
                    {customer.getName(), customer.getEmail(),
                     kycToString(customer.getKycStatus())});
    } catch (const DatabaseException& e) {
        // Backstop: another process could insert the same email between
        // our check and our insert. The UNIQUE constraint still catches it.
        if (std::string(e.what()).find("1062") != std::string::npos) {
            throw std::runtime_error("Email already registered: " + customer.getEmail());
        }
        throw;
    }
    int newId = static_cast<int>(db_.lastInsertId());
    customer.setUserId(newId);
    return newId;
}

std::optional<Customer> CustomerRepository::findById(int userId) {
    ResultSet rows = db_.query(
        "SELECT user_id, name, email, kyc_status FROM Users WHERE user_id = ?",
        {std::to_string(userId)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return rowToCustomer(rows[0]);
}

std::vector<Customer> CustomerRepository::findAll() {
    ResultSet rows = db_.query(
        "SELECT user_id, name, email, kyc_status FROM Users ORDER BY user_id");
    std::vector<Customer> customers;
    customers.reserve(rows.size());
    for (const Row& row : rows) {
        customers.push_back(rowToCustomer(row));
    }
    return customers;
}

void CustomerRepository::updateProfile(const Customer& customer) {
    if (customer.getUserId() == 0) {
        throw std::runtime_error("Cannot update a customer that was never saved");
    }
    if (emailTaken(db_, customer.getEmail(), customer.getUserId())) {
        throw std::runtime_error("Email already used by another customer: " +
                                 customer.getEmail());
    }
    // Note: affected rows can be 0 when nothing actually changed,
    // so we don't treat 0 as an error here.
    db_.execute("UPDATE Users SET name = ?, email = ? WHERE user_id = ?",
                {customer.getName(), customer.getEmail(),
                 std::to_string(customer.getUserId())});
}

void CustomerRepository::updateKycStatus(int userId, KycStatus status) {
    if (!findById(userId).has_value()) {
        throw std::runtime_error("Customer not found: " + std::to_string(userId));
    }
    db_.execute("UPDATE Users SET kyc_status = ? WHERE user_id = ?",
                {kycToString(status), std::to_string(userId)});
}