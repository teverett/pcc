// EXPECT: 3
// A global array's initializer accepts negated literals.
int g[4] = {5, -3, 0, -100};

int main() {
    int result;
    result = 0;
    if (g[1] == -3) {
        result = result + 1;
    }
    if (g[3] == -100) {
        result = result + 1;
    }
    if (g[0] + g[1] + g[2] + g[3] == -98) {
        result = result + 1;
    }
    return result;
}
