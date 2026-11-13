// EXPECT: 226
static int counter;
static int bias = 100;

int next() {
    static int calls = 0;
    calls = calls + 1;
    counter = counter + 1;
    return calls;
}

int main() {
    int a = next();
    int b = next();
    int c = next();
    return a * 100 + b * 10 + c + bias + counter;
}
