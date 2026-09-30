#pragma once

#include <string>

// Abstract base class: holds what every kind of user shares.
// You cannot create a plain User, only a derived class such as Customer.
class User {
public:
    virtual ~User() = default;  // virtual, so deleting through a User* is safe

    int getUserId() const { return userId_; }
    const std::string& getName() const { return name_; }
    const std::string& getEmail() const { return email_; }

    // Validated setters (throw std::invalid_argument on bad input)
    void setName(const std::string& name);
    void setEmail(const std::string& email);

    // Assigned by the repository after the database generates the id
    void setUserId(int id) { userId_ = id; }

    // Pure virtual: every derived class must say how it displays itself
    virtual void displayProfile() const = 0;

protected:
    // protected: only derived classes can construct a User
    User(int userId, const std::string& name, const std::string& email);

private:
    int userId_;  // 0 means "not saved to the database yet"
    std::string name_;
    std::string email_;
};