# Banking & Fraud Detection System

A C++17 banking backend with a rule-based fraud detection engine, backed by MySQL.
Customers open accounts, deposit, withdraw and transfer money. Every transaction is
scored by a set of fraud rules before any money moves.

## Problem statement

Banking software must move money safely (never lose or duplicate it) and spot suspicious
activity early. This project models both: ACID-safe transfers, and a pluggable fraud
engine that turns rule results into a risk score, a risk level and an action.

## Features

- Customers with KYC status, accounts (savings/current), deposits, withdrawals, transfers
- Transaction history with statuses: COMPLETED, FAILED, FLAGGED, BLOCKED
- Atomic transfers using MySQL transactions and row locking
- Five fraud rules, each a separate class behind one interface
- Risk score, risk level, recommended action, alerts stored in the database
- Interactive transaction simulator with six scenarios
- SQL analytics (joins, GROUP BY / HAVING, subqueries, a view, EXPLAIN)
- Automated tests (56 checks), security checks and a performance check

## Tech stack

C++17, OOP, STL, MySQL 8, SQL, CMake + Ninja, MariaDB Connector/C (MySQL-compatible C API).

## Architecture

```text
main.cpp / Simulator
        |
TransactionService ---- FraudEngine ---- FraudRule (abstract)
        |                                   |-- HighAmountRule
Customer/Account/Transaction                |-- RapidTransactionRule
   Repositories                             |-- FailedAttemptRule
        |                                   |-- UnusualTransactionRule
DatabaseManager (RAII, prepared statements) '-- TransferChainRule
        |
      MySQL
```

Upper layers call lower layers only. Only `DatabaseManager` talks to MySQL.
Repositories hold SQL, the service holds business flow, rules hold detection logic.

## OOP concepts used

- **Abstraction and polymorphism:** `FraudRule` is abstract; the engine calls
  `rule->check()` through base pointers stored in `vector<unique_ptr<FraudRule>>`.
- **Inheritance:** `User` (abstract) to `Customer`; `FraudRule` to five rules.
- **Encapsulation:** private fields, validated setters, no invalid objects.
- **RAII:** `DatabaseManager` opens/closes the connection; statements use `unique_ptr`.
- **Open/Closed:** adding a rule means a new class plus one `addRule` line.
- **Separation of concerns:** repository, service and engine layers.

## Database schema

Tables: `Users`, `Accounts`, `Transactions`, `FraudAlerts`, `LoginAttempts`
(`LoginAttempts` exists but is not used yet).

```text
Users 1--N Accounts 1--N Transactions 1--N FraudAlerts
```

Constraints: primary and foreign keys, UNIQUE email, UNIQUE (user, account type),
`CHECK balance >= 0`, `CHECK amount > 0`, a CHECK on transaction shape.
Money is `DECIMAL(15,2)` in MySQL and integer paise (`long long`) in C++.
Indexes: `(sender_account, timestamp)`, `(receiver_account, timestamp)`.

## Fraud detection logic

| Rule | Detects | Points |
|---|---|---|
| High Amount | amount above Rs 50,000 | +30 |
| Rapid Transactions | 5+ transactions from a sender within 120 s | +25 |
| Failed Attempts | 3+ FAILED transactions within 10 min | +20 |
| Unusual Amount | amount at least 10x the sender's average (needs 3 completed) | +15 |
| Transfer Chain | A to B to C to D style chain within 10 min (3+ transfers) | +30 |

All thresholds live in `FraudConfig`.

## Risk scoring

| Score | Level | Action | Money |
|---|---|---|---|
| 0-29 | LOW | ALLOW | moves, COMPLETED |
| 30-59 | MEDIUM | FLAG | moves, FLAGGED |
| 60-79 | HIGH | HOLD FOR REVIEW | moves, FLAGGED |
| 80+ | CRITICAL | BLOCK | does not move, BLOCKED |

These thresholds are project-defined, not claims about real banks.

## Installation (Windows)

1. Install MSYS2, then in the **UCRT64** shell:
   `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-libmariadbclient`
   (optionally `mingw-w64-ucrt-x86_64-lld` for clearer link errors)
2. Add `C:\msys64\ucrt64\bin` to PATH. If MSYS2 lives elsewhere, pass
   `-DMSYS2_PREFIX=<path>/ucrt64` to CMake.
3. Install MySQL Server 8.

## Database setup

```text
mysql -u root -p -e "source sql/schema.sql"
mysql -u root -p -e "source sql/sample_data.sql"
```

Create a limited application user (choose your own password):

```sql
CREATE USER 'bank_app'@'localhost' IDENTIFIED WITH mysql_native_password BY '<your-password>';
GRANT SELECT, INSERT, UPDATE, DELETE ON banking_fraud_db.* TO 'bank_app'@'localhost';
```

`schema.sql` drops and recreates the database: development use only.
Copy `.env.example` to `.env` and fill in your values. Never commit `.env`.

## How to run

```text
cmake -S . -B build -G Ninja
cmake --build build
.\build\bank_fraud.exe        # simulator
.\build\bank_tests.exe        # automated tests
.\build\security_check.exe    # security checks
.\build\perf_check.exe        # performance check
```

Run from the project root (the programs read `.env` from the current folder).

## Sample output

```text
Transfer Rs 95000 from account 31 to account 32
  High Amount Rule  FAILED
  Rapid Transaction Rule  PASSED
  Failed Attempt Rule  PASSED
  Unusual Transaction Rule  FAILED
  Transfer Chain Rule  PASSED
  Risk Score: 45
  Risk Level: MEDIUM
  Action: FLAG TRANSACTION
  Result: txn #58 FLAGGED, money moved: yes
```

## SQL analytics

`sql/analytics.sql` has 16 queries: totals, GROUP BY / HAVING, three-table joins,
UNION ALL, scalar subqueries, LEFT JOIN with IS NULL, a `v_transaction_risk` view,
and `EXPLAIN` checks that the indexes are used.

## Testing

`bank_tests` (37 checks), `security_check` (19 checks: SQL injection, secrets,
validation, rollback safety), and `perf_check`. Test data is removed afterwards and
table counts are verified.

## Known limitations

- No authentication (next step: salted, slow hashing such as Argon2id or bcrypt,
  plus logging failed logins to `LoginAttempts`)
- HIGH risk still moves money; a real system would hold it pending review
- The chain walk is greedy (follows the latest sender), not a full graph search
- The Unusual rule's average shifts after a large transfer and scans a sender's whole
  history (about 51 ms at 20,000 transactions in a test)
- Fraud analysis runs just before the money transaction, so state can change in between
- No TLS to MySQL; console output uses "Rs" instead of the rupee symbol

## Future improvements

Authentication, a windowed or running-average Unusual rule with a composite index,
pending-review workflow, graph-based chain detection, a REST API or GUI.