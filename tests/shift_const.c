// EXPECT: 7
// Shifts by constant counts: left shifts fill with zeros, unsigned
// right shifts with zeros, signed right shifts with copies of the sign
// bit, and a count of 0 leaves the value unchanged.
unsigned int u;
int s;

int main() {
    int result;
    result = 0;
    u = 42;
    s = -20;
    result = result + ((u << 3) == 336);
    result = result + ((u >> 1) == 21);
    result = result + ((u << 0) == 42);
    result = result + ((s >> 2) == -5);
    result = result + ((s << 2) == -80);
    result = result + ((s >> 0) == -20);
    u = 0u - 1;
    result = result + ((u >> 4) == (0u - 1) / 16);
    return result;
}
