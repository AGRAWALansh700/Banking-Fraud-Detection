#include "Config.h"

#include <fstream>
#include <map>
#include <stdexcept>

namespace {

// Removes leading/trailing whitespace (including \r from Windows line endings)
std::string trim(const std::string& text) {
    const char* whitespace = " \t\r\n";
    std::size_t start = text.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

}  // namespace

DbConfig loadDbConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file '" + path +
                                 "'. Copy .env.example to .env and fill in your values.");
    }

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;  // skip blank lines and comments
        }
        std::size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            continue;  // not a KEY=VALUE line
        }
        values[trim(line.substr(0, equalsPos))] = trim(line.substr(equalsPos + 1));
    }

    auto require = [&](const std::string& key) -> std::string {
        auto it = values.find(key);
        if (it == values.end() || it->second.empty()) {
            throw std::runtime_error("Missing required setting '" + key + "' in " + path);
        }
        return it->second;
    };

    DbConfig config;
    config.user = require("DB_USER");
    config.password = require("DB_PASSWORD");
    config.database = require("DB_NAME");

    if (values.count("DB_HOST") != 0) {
        config.host = values["DB_HOST"];
    }
    if (values.count("DB_PORT") != 0) {
        try {
            config.port = static_cast<unsigned int>(std::stoul(values["DB_PORT"]));
        } catch (const std::exception&) {
            throw std::runtime_error("DB_PORT in " + path + " must be a number");
        }
    }
    return config;
}