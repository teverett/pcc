// EXPECT: 2
// A call to a function declared with an `extern` prototype has the
// prototype's return type, here long.
extern long big();
extern long add(long a, long b);

int main() {
    int result;
    result = 0;
    if (big() == 300000) {
        result = result + 1;
    }
    if (add(200000, 100000) == 300000) {
        result = result + 1;
    }
    return result;
}
