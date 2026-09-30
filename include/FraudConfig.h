#pragma once

#include "Money.h"

// All fraud thresholds and point values in one place.
// Rules read from this instead of hardcoding numbers.
struct FraudConfig {
    // HighAmountRule
    Paise highAmountThreshold = 5000000;  // Rs 50,000.00
    int highAmountPoints = 30;

    // RapidTransactionRule
    int rapidCountThreshold = 5;
    int rapidWindowSeconds = 120;
    int rapidPoints = 25;

    // FailedAttemptRule
    int failedCountThreshold = 3;
    int failedWindowMinutes = 10;
    int failedPoints = 20;

    // UnusualTransactionRule
    int unusualMultiplier = 10;   // amount must be at least 10x the sender's average
    int unusualMinHistory = 3;    // need at least this many completed transactions
    int unusualPoints = 15;

    // TransferChainRule
    int chainHopsThreshold = 3;   // 3 transfers = A -> B -> C -> D
    int chainWindowMinutes = 10;
    int chainPoints = 30;

    // Risk levels: a score at or above a boundary belongs to that level
    int mediumFrom = 30;
    int highFrom = 60;
    int criticalFrom = 80;
};