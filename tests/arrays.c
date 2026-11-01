// EXPECT: 1155
int garr[5] = {10, 20, 30, 40, 50};

int main() {
    int larr[3];
    int i;
    int sum;

    larr[0] = 1;
    larr[1] = 2;
    larr[2] = 3;

    sum = 0;
    for (i = 0; i < 5; i = i + 1) {
        sum = sum + garr[i];     // 10+20+30+40+50 = 150
    }
    for (i = 0; i < 3; i = i + 1) {
        sum = sum + larr[i];     // +1+2+3 = 156
    }

    garr[2] = 999;
    sum = sum + garr[2];        // +999 = 1155
    return sum;
}
