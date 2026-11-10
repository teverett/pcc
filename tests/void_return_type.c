// EXPECT: 27
// A function declared with 'void' takes no return value - it runs
// purely for its side effect (here, accumulating into the global g)
// and a bare 'return;' with no expression ends it early.
int g = 0;

void addTwice(int n) {
    g = g + n;
    if (n < 0) {
        return;
    }
    g = g + n;
}

int main() {
    addTwice(5);
    addTwice(10);
    addTwice(-3);  // only adds once (the early return skips the second add)
    return g;
}
