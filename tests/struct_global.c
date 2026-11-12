// EXPECT: 11
struct Point {
    int x;
    int y;
};

struct Point g;

int main() {
    g.x = 5;
    g.y = 6;
    return g.x + g.y;
}
