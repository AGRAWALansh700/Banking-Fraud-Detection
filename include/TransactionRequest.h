#pragma once

#include "Money.h"
#include "Transaction.h"

// A transaction that is about to happen and hasn't been saved yet.
// Fraud rules judge this before any money moves.
// senderAccount / receiverAccount are 0 when not applicable.
struct TransactionRequest {
    int senderAccount = 0;
    int receiverAccount = 0;
    Paise amount = 0;
    TransactionType type = TransactionType::Transfer;
};