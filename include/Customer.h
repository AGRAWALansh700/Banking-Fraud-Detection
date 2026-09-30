#pragma once

#include <string>

#include "User.h"

enum class KycStatus { Pending, Verified, Rejected };

// Conversion helpers between the C++ enum and the database text
std::string kycToString(KycStatus status);
KycStatus kycFromString(const std::string& text);  // throws std::invalid_argument

class Customer : public User {
public:
    // New customer, not yet saved (id 0), KYC starts as Pending
    Customer(const std::string& name, const std::string& email);

    // Customer loaded from the database
    Customer(int userId, const std::string& name, const std::string& email,
             KycStatus kycStatus);

    KycStatus getKycStatus() const { return kycStatus_; }
    void setKycStatus(KycStatus status) { kycStatus_ = status; }

    void displayProfile() const override;

private:
    KycStatus kycStatus_;
};