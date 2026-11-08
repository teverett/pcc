// EXPECT: 2
// A long long addition carries through every word: adding a small
// constant to a negative value, and adding 1 to 2^32 - 1.
long long v;

int main() {
    int result;
    result = 0;
    v = -7;
    if (v + 10 == 3) {
        result = result + 1;
    }
    v = 4294967295;
    if (v + 1 == 4294967296) {
        result = result + 1;
    }
    return result;
}
