// EXPECT: 300
// short storage and arithmetic near its own signed 16-bit boundary,
// kept so the true mathematical result (unlike either operand alone)
// stays well away from that boundary.
short a;
short b;
int main() {
    a = 32000;
    b = -31700;
    return a + b;  // 32000 - 31700 = 300
}
