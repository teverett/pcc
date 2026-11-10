// EXPECT: 10
// A void function can recurse and call another void function; neither
// call's (nonexistent) result is ever used as a value, only as a
// statement.
int total = 0;

void record(int n) {
    total = total + n;
}

void countDown(int n) {
    if (n == 0) {
        return;
    }
    record(n);
    countDown(n - 1);
}

int main() {
    countDown(4);  // records 4, 3, 2, 1 -> total 10
    return total;
}
