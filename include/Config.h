#pragma once

#include <string>

// Connection settings for MySQL. Loaded from a .env file, never hardcoded.
struct DbConfig {
    std::string host = "127.0.0.1";
    unsigned int port = 3306;
    std::string user;
    std::string password;
    std::string database;
};

// Reads KEY=VALUE lines from the given file.
// Throws std::runtime_error if the file or a required key is missing.
DbConfig loadDbConfig(const std::string& path = ".env");