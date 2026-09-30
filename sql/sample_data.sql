USE banking_fraud_db;

-- Order matters: parents first, children after (foreign keys)

INSERT INTO Users (name, email, kyc_status) VALUES
    ('Aarav Sharma', 'aarav.sharma@example.com', 'VERIFIED'),
    ('Priya Verma',  'priya.verma@example.com',  'VERIFIED'),
    ('Rohan Mehta',  'rohan.mehta@example.com',  'VERIFIED'),
    ('Sneha Iyer',   'sneha.iyer@example.com',   'PENDING'),
    ('Karan Singh',  'karan.singh@example.com',  'REJECTED');

-- Balances are seeded directly; they will not exactly match the history below.
-- Phase 7 will make balances change only through real transactions.
INSERT INTO Accounts (user_id, account_type, balance) VALUES
    (1, 'SAVINGS',  50000.00),   -- account 1
    (2, 'SAVINGS', 120000.00),   -- account 2
    (2, 'CURRENT',  30000.00),   -- account 3
    (3, 'SAVINGS',   8000.00),   -- account 4
    (4, 'SAVINGS',  15000.00),   -- account 5
    (5, 'CURRENT',    500.00);   -- account 6

INSERT INTO Transactions
    (sender_account, receiver_account, amount, transaction_type, status, timestamp)
VALUES
    (NULL, 1, 20000.00, 'DEPOSIT',    'COMPLETED', '2026-09-20 10:00:00'),
    (NULL, 2, 50000.00, 'DEPOSIT',    'COMPLETED', '2026-09-20 10:05:00'),
    (1, NULL,  5000.00, 'WITHDRAWAL', 'COMPLETED', '2026-09-21 11:15:00'),
    (1, 2,     2000.00, 'TRANSFER',   'COMPLETED', '2026-09-22 14:30:00'),
    (2, 3,    10000.00, 'TRANSFER',   'COMPLETED', '2026-09-23 09:45:00'),
    (2, 4,     3500.00, 'TRANSFER',   'COMPLETED', '2026-09-24 16:20:00'),
    (4, NULL,  1500.00, 'WITHDRAWAL', 'COMPLETED', '2026-09-25 12:00:00'),
    (4, 5,      800.00, 'TRANSFER',   'COMPLETED', '2026-09-26 18:10:00'),
    (6, NULL,  9000.00, 'WITHDRAWAL', 'FAILED',    '2026-09-27 10:30:00'),
    (2, 1,    95000.00, 'TRANSFER',   'FLAGGED',   '2026-09-28 22:47:00');

-- Transaction 10 (large transfer) triggered two rules: 30 + 15 = 45 (MEDIUM)
INSERT INTO FraudAlerts (transaction_id, rule_type, rule_points, message) VALUES
    (10, 'HIGH_AMOUNT',    30, 'Amount 95000.00 exceeds high-amount threshold'),
    (10, 'UNUSUAL_AMOUNT', 15, 'Amount far above sender historical average');

INSERT INTO LoginAttempts (user_id, success, timestamp) VALUES
    (1, TRUE,  '2026-09-28 09:00:00'),
    (2, TRUE,  '2026-09-28 09:30:00'),
    (3, TRUE,  '2026-09-28 10:00:00'),
    (5, FALSE, '2026-09-29 11:00:05'),
    (5, FALSE, '2026-09-29 11:00:40'),
    (5, FALSE, '2026-09-29 11:01:10');