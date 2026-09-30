-- ============================================================
-- Banking & Fraud Detection System - schema
-- WARNING: this script DROPS and recreates the database.
-- Use it only in development.
-- ============================================================

DROP DATABASE IF EXISTS banking_fraud_db;
CREATE DATABASE banking_fraud_db
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE banking_fraud_db;

-- ------------------------------------------------------------
-- Users: one row per customer
-- ------------------------------------------------------------
CREATE TABLE Users (
    user_id     INT AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(100) NOT NULL,
    email       VARCHAR(150) NOT NULL,
    kyc_status  ENUM('PENDING', 'VERIFIED', 'REJECTED') NOT NULL DEFAULT 'PENDING',
    created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT uq_users_email UNIQUE (email)
) ENGINE=InnoDB;

-- ------------------------------------------------------------
-- Accounts: a user can hold at most one account of each type
-- ------------------------------------------------------------
CREATE TABLE Accounts (
    account_id    INT AUTO_INCREMENT PRIMARY KEY,
    user_id       INT NOT NULL,
    account_type  ENUM('SAVINGS', 'CURRENT') NOT NULL,
    balance       DECIMAL(15,2) NOT NULL DEFAULT 0.00,
    created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_accounts_user
        FOREIGN KEY (user_id) REFERENCES Users(user_id),
    CONSTRAINT uq_accounts_user_type UNIQUE (user_id, account_type),
    CONSTRAINT chk_accounts_balance CHECK (balance >= 0)
) ENGINE=InnoDB;

-- ------------------------------------------------------------
-- Transactions: every money movement
--   DEPOSIT    -> receiver only
--   WITHDRAWAL -> sender only
--   TRANSFER   -> both, and they must be different accounts
-- ------------------------------------------------------------
CREATE TABLE Transactions (
    transaction_id    INT AUTO_INCREMENT PRIMARY KEY,
    sender_account    INT NULL,
    receiver_account  INT NULL,
    amount            DECIMAL(15,2) NOT NULL,
    transaction_type  ENUM('DEPOSIT', 'WITHDRAWAL', 'TRANSFER') NOT NULL,
    status            ENUM('PENDING', 'COMPLETED', 'FAILED', 'FLAGGED', 'BLOCKED')
                      NOT NULL DEFAULT 'PENDING',
    timestamp         DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_txn_sender
        FOREIGN KEY (sender_account) REFERENCES Accounts(account_id),
    CONSTRAINT fk_txn_receiver
        FOREIGN KEY (receiver_account) REFERENCES Accounts(account_id),
    CONSTRAINT chk_txn_amount CHECK (amount > 0),
    CONSTRAINT chk_txn_shape CHECK (
        (transaction_type = 'DEPOSIT'
            AND sender_account IS NULL AND receiver_account IS NOT NULL)
        OR
        (transaction_type = 'WITHDRAWAL'
            AND sender_account IS NOT NULL AND receiver_account IS NULL)
        OR
        (transaction_type = 'TRANSFER'
            AND sender_account IS NOT NULL AND receiver_account IS NOT NULL
            AND sender_account <> receiver_account)
    )
) ENGINE=InnoDB;

CREATE INDEX idx_txn_sender_time   ON Transactions (sender_account, timestamp);
CREATE INDEX idx_txn_receiver_time ON Transactions (receiver_account, timestamp);

-- ------------------------------------------------------------
-- FraudAlerts: one row per triggered rule for a transaction
-- Total risk score = SUM(rule_points) grouped by transaction_id
-- ------------------------------------------------------------
CREATE TABLE FraudAlerts (
    alert_id        INT AUTO_INCREMENT PRIMARY KEY,
    transaction_id  INT NOT NULL,
    rule_type       VARCHAR(50) NOT NULL,
    rule_points     INT NOT NULL,
    message         VARCHAR(255) NULL,
    status          ENUM('OPEN', 'REVIEWED', 'DISMISSED') NOT NULL DEFAULT 'OPEN',
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_alerts_txn
        FOREIGN KEY (transaction_id) REFERENCES Transactions(transaction_id),
    CONSTRAINT chk_alerts_points CHECK (rule_points > 0)
) ENGINE=InnoDB;

CREATE INDEX idx_alerts_rule ON FraudAlerts (rule_type);

-- ------------------------------------------------------------
-- LoginAttempts: feeds the failed-attempt rule
-- ------------------------------------------------------------
CREATE TABLE LoginAttempts (
    attempt_id  INT AUTO_INCREMENT PRIMARY KEY,
    user_id     INT NOT NULL,
    success     BOOLEAN NOT NULL,
    timestamp   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_login_user
        FOREIGN KEY (user_id) REFERENCES Users(user_id)
) ENGINE=InnoDB;

CREATE INDEX idx_login_user_time ON LoginAttempts (user_id, timestamp);