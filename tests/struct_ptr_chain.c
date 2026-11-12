// EXPECT: 9
struct Point {
    int x;
    int y;
};

int main() {
    struct Point a;
    struct Point b;
    struct Point *pa;
    struct Point *pb;
    pa = &a;
    pb = &b;
    pa->x = 4;
    pa->y = 5;
    pb->x = pa->x;
    pb->y = pa->y;
    return pb->x + pb->y;
}
