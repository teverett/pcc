// EXPECT: 5040
// Direct recursion (factorial) and mutual/indirect recursion
// (isEven/isOdd) both need every call to get its own, independent
// copy of its parameters and locals - this only works once a
// function's storage lives in a fresh per-call stack frame rather
// than one shared, permanent cell reused by every call (see
// src/ir/IR.h's Slot::frameOffset/Function::frameSize).
int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int isEven(int n);
int isOdd(int n);

int isEven(int n) {
    if (n == 0) {
        return 1;
    }
    return isOdd(n - 1);
}

int isOdd(int n) {
    if (n == 0) {
        return 0;
    }
    return isEven(n - 1);
}

int main() {
    int ok;
    ok = 1;
    if (isEven(10) != 1) {
        ok = 0;
    }
    if (isOdd(10) != 0) {
        ok = 0;
    }
    if (isEven(7) != 0) {
        ok = 0;
    }
    if (ok == 0) {
        return 0;
    }
    return fact(7);  // 5040
}
