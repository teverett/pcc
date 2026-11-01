// EXPECT: 34
int square(int n) {
    return n * n;
}

int fact(int n) {
    int result;
    result = 1;
    while (n > 1) {
        result = result * n;
        n = n - 1;
    }
    return result;
}

int main() {
    int a;
    int total;
    a = 6 / 2;         // 3
    total = square(a); // 9
    if (a == 3) {
        total = total + fact(4);  // 9 + 24 = 33
    } else {
        total = 0;
    }
    total = total + (a < 10 && a != 0); // +1 = 34
    return total;
}
