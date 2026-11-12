// EXPECT: 9
struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[3];
    int i;
    int sum;
    for (i = 0; i < 3; i = i + 1) {
        pts[i].x = i;       // 0, 1, 2
        pts[i].y = i * 2;   // 0, 2, 4
    }
    sum = 0;
    for (i = 0; i < 3; i = i + 1) {
        sum = sum + pts[i].x + pts[i].y;
    }
    return sum;  // (0+0)+(1+2)+(2+4) = 9
}
