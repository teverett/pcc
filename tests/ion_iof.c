// EXPECT: 7
int main() {
    int x;
    x = 5;
    _iof();
    x = x + 1;
    _ion();
    x = x + 1;
    return x;
}
