// EXPECT: 2
// Unsigned `/=` and `%=` divide unsigned, the same as `/` and `%`:
// (UMAX-5) /= (UMAX-6) gives 1, and (UMAX-5) %= (UMAX-6) gives 1
// (signed division would give 0 and -6).
unsigned int a;
unsigned int b;

int main() {
    int result;

    result = 0;
    a = 0u - 6;
    b = 0u - 7;
    a /= b;
    result = result + (a == 1);
    a = 0u - 6;
    a %= b;
    result = result + (a == 1);
    return result;
}
