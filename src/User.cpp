#include "User.h"

#include "Validation.h"

User::User(int userId, const std::string& name, const std::string& email)
    : userId_(userId),
      name_(validation::validateName(name)),
      email_(validation::validateEmail(email)) {}

void User::setName(const std::string& name) {
    name_ = validation::validateName(name);
}

void User::setEmail(const std::string& email) {
    email_ = validation::validateEmail(email);
}