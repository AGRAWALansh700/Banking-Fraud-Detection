USE banking_fraud_db;

-- ============================================================
-- 1. Total transaction volume (aggregate)
--    Interview: "How much money moved through the system?"
-- ============================================================
SELECT COUNT(*) AS transactions, SUM(amount) AS total_volume
FROM Transactions
WHERE status IN ('COMPLETED', 'FLAGGED');

-- ============================================================
-- 2. Volume and counts by status (GROUP BY)
--    Interview: "How many transactions were blocked or failed?"
-- ============================================================
SELECT status, COUNT(*) AS n, SUM(amount) AS total_amount
FROM Transactions
GROUP BY status
ORDER BY n DESC;

-- ============================================================
-- 3. Top 5 largest transactions (ORDER BY + LIMIT)
--    Interview: "Show the highest-value transactions."
-- ============================================================
SELECT transaction_id, transaction_type, amount, status
FROM Transactions
ORDER BY amount DESC
LIMIT 5;

-- ============================================================
-- 4. Transactions per customer (3-table JOIN + GROUP BY)
--    Interview: "Which customers send the most transactions?"
-- ============================================================
SELECT u.user_id, u.name,
       COUNT(*) AS sent_count,
       SUM(t.amount) AS sent_total,
       ROUND(AVG(t.amount), 2) AS avg_amount
FROM Transactions t
JOIN Accounts a ON a.account_id = t.sender_account
JOIN Users u ON u.user_id = a.user_id
WHERE t.status IN ('COMPLETED', 'FLAGGED')
GROUP BY u.user_id, u.name
ORDER BY sent_total DESC;

-- ============================================================
-- 5. Most active accounts (sender or receiver, UNION ALL)
--    Interview: "Which accounts appear most often in transactions?"
-- ============================================================
SELECT account_id, COUNT(*) AS appearances
FROM (
    SELECT sender_account AS account_id FROM Transactions WHERE sender_account IS NOT NULL
    UNION ALL
    SELECT receiver_account FROM Transactions WHERE receiver_account IS NOT NULL
) AS all_accounts
GROUP BY account_id
ORDER BY appearances DESC
LIMIT 5;

-- ============================================================
-- 6. Fraud alerts by rule type (GROUP BY + aggregates)
--    Interview: "Which fraud rule fires most?"
-- ============================================================
SELECT rule_type, COUNT(*) AS alerts, SUM(rule_points) AS total_points
FROM FraudAlerts
GROUP BY rule_type
ORDER BY alerts DESC;

-- ============================================================
-- 7. Risk score per transaction (GROUP BY on the alerts table)
--    Interview: "How is a transaction's total risk score derived?"
-- ============================================================
SELECT transaction_id, SUM(rule_points) AS risk_score
FROM FraudAlerts
GROUP BY transaction_id
ORDER BY risk_score DESC;

-- ============================================================
-- 8. Transactions with a high risk score (HAVING)
--    Interview: "GROUP BY vs HAVING? WHERE vs HAVING?"
--    WHERE filters rows BEFORE grouping; HAVING filters groups AFTER.
-- ============================================================
SELECT transaction_id, SUM(rule_points) AS risk_score
FROM FraudAlerts
GROUP BY transaction_id
HAVING SUM(rule_points) >= 60
ORDER BY risk_score DESC;

-- ============================================================
-- 9. Customers with multiple fraud alerts (JOIN + HAVING)
--    Interview: "Which customers should the fraud team watch?"
-- ============================================================
SELECT u.user_id, u.name, COUNT(*) AS alerts
FROM FraudAlerts f
JOIN Transactions t ON t.transaction_id = f.transaction_id
JOIN Accounts a ON a.account_id = t.sender_account
JOIN Users u ON u.user_id = a.user_id
GROUP BY u.user_id, u.name
HAVING COUNT(*) >= 2
ORDER BY alerts DESC;

-- ============================================================
-- 10. Transactions above the overall average (subquery)
--     Interview: "What is a subquery? Scalar vs correlated?"
-- ============================================================
SELECT transaction_id, amount, status
FROM Transactions
WHERE amount > (SELECT AVG(amount) FROM Transactions)
ORDER BY amount DESC;

-- ============================================================
-- 11. Accounts richer than the average balance (subquery)
-- ============================================================
SELECT a.account_id, u.name, a.balance
FROM Accounts a
JOIN Users u ON u.user_id = a.user_id
WHERE a.balance > (SELECT AVG(balance) FROM Accounts)
ORDER BY a.balance DESC;

-- ============================================================
-- 12. Users with NO transactions (LEFT JOIN + IS NULL)
--     Interview: "INNER vs LEFT JOIN? Find rows with no match."
-- ============================================================
SELECT u.user_id, u.name
FROM Users u
JOIN Accounts a ON a.user_id = u.user_id
LEFT JOIN Transactions t
       ON t.sender_account = a.account_id OR t.receiver_account = a.account_id
WHERE t.transaction_id IS NULL;

-- ============================================================
-- 13. Daily transaction volume (date functions)
--     Interview: "Show volume per day / per month."
--     For monthly, replace DATE(timestamp) with DATE_FORMAT(timestamp, '%Y-%m').
-- ============================================================
SELECT DATE(timestamp) AS day, COUNT(*) AS n, SUM(amount) AS volume
FROM Transactions
GROUP BY DATE(timestamp)
ORDER BY day;

-- ============================================================
-- 14. VIEW: each transaction with its total risk and level
--     Interview: "What is a view and why use one?"
--     Thresholds mirror FraudConfig (30 / 60 / 80).
-- ============================================================
CREATE OR REPLACE VIEW v_transaction_risk AS
SELECT t.transaction_id,
       t.transaction_type,
       t.amount,
       t.status,
       COALESCE(SUM(f.rule_points), 0) AS risk_score,
       CASE
           WHEN COALESCE(SUM(f.rule_points), 0) >= 80 THEN 'CRITICAL'
           WHEN COALESCE(SUM(f.rule_points), 0) >= 60 THEN 'HIGH'
           WHEN COALESCE(SUM(f.rule_points), 0) >= 30 THEN 'MEDIUM'
           ELSE 'LOW'
       END AS risk_level
FROM Transactions t
LEFT JOIN FraudAlerts f ON f.transaction_id = t.transaction_id
GROUP BY t.transaction_id, t.transaction_type, t.amount, t.status;

-- Using the view
SELECT risk_level, COUNT(*) AS n
FROM v_transaction_risk
GROUP BY risk_level
ORDER BY FIELD(risk_level, 'CRITICAL', 'HIGH', 'MEDIUM', 'LOW');

SELECT * FROM v_transaction_risk
WHERE risk_level IN ('HIGH', 'CRITICAL')
ORDER BY risk_score DESC;

-- ============================================================
-- 15. Indexes: prove they are used (EXPLAIN)
--     Look at the "key" column: it should show the index name.
-- ============================================================
EXPLAIN SELECT COUNT(*) FROM Transactions
WHERE sender_account = 25 AND timestamp >= NOW() - INTERVAL 120 SECOND;
-- expect key = idx_txn_sender_time

EXPLAIN SELECT sender_account FROM Transactions
WHERE receiver_account = 25 AND timestamp >= NOW() - INTERVAL 10 MINUTE;
-- expect key = idx_txn_receiver_time

-- ============================================================
-- 16. Referential integrity check: alerts without a transaction (should be 0 rows)
-- ============================================================
SELECT f.alert_id
FROM FraudAlerts f
LEFT JOIN Transactions t ON t.transaction_id = f.transaction_id
WHERE t.transaction_id IS NULL;