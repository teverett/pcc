// EXPECT: 64
const int limit = 10;
const int table[3] = {1, 2, 3};

int clamp(const int x) {
    const int half = limit / 2;
    return x > half ? half : x;
}

int main() {
    static const int bias = 50;
    return clamp(3) + clamp(9) + table[0] + table[1] + table[2] + bias;
}
