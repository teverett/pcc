// EXPECT: 2
// A long long subtraction borrows through every word: going below
// zero, and subtracting 1 from 2^32.
long long v;

int main() {
    int result;
    result = 0;
    v = 3;
    if (v - 10 == -7) {
        result = result + 1;
    }
    v = 4294967296;
    if (v - 1 == 4294967295) {
        result = result + 1;
    }
    return result;
}
