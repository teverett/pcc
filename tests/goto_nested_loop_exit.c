// EXPECT: 13
// The classic idiom goto exists for in C: breaking out of two nested
// loops at once, which a bare `break` can't do (it only exits the
// innermost loop).
int main() {
    int i;
    int j;
    int result = 0;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            if (i == 2 && j == 3) goto done;
            result++;
        }
    }
done:
    return result;
}
