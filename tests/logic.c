// EXPECT: 20
int counter = 0;

int bump() {
    counter = counter + 1;
    return counter;
}

int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 0; i < 5; i = i + 1) {
        sum = sum + i;       // 0+1+2+3+4 = 10
    }
    sum = sum + (10 % 3);    // +1 = 11
    sum = sum + !(0);        // +1 = 12
    sum = sum + (3 != 4);    // +1 = 13
    sum = sum + (5 >= 5);    // +1 = 14
    sum = sum + (5 <= 4);    // +0 = 14
    sum = sum + (0 || 7);    // +1 = 15
    sum = sum + bump();      // counter: 0->1, +1 = 16
    sum = sum + bump();      // counter: 1->2, +2 = 18
    sum = sum + counter;     // +2 = 20
    return sum;
}
