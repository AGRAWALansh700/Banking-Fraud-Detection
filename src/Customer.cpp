#include "Customer.h"

#include <iostream>
#include <stdexcept>

std::string kycToString(KycStatus status) {
    switch (status) {
        case KycStatus::Pending:  return "PENDING";
        case KycStatus::Verified: return "VERIFIED";
        case KycStatus::Rejected: return "REJECTED";
    }
    throw std::invalid_argument("Unknown KYC status");
}

KycStatus kycFromString(const std::string& text) {
    if (text == "PENDING")  return KycStatus::Pending;
    if (text == "VERIFIED") return KycStatus::Verified;
    if (text == "REJECTED") return KycStatus::Rejected;
    throw std::invalid_argument("Unknown KYC status: " + text);
}

Customer::Customer(const std::string& name, const std::string& email)
    : User(0, name, email), kycStatus_(KycStatus::Pending) {}

Customer::Customer(int userId, const std::string& name, const std::string& email,
                   KycStatus kycStatus)
    : User(userId, name, email), kycStatus_(kycStatus) {}

void Customer::displayProfile() const {
    std::cout << "Customer #" << getUserId() << "\n"
              << "  Name : " << getName() << "\n"
              << "  Email: " << getEmail() << "\n"
              << "  KYC  : " << kycToString(kycStatus_) << "\n";
}