// EXPECT: 150
// A global array literal initializer: each element is set at load
// time before main ever runs, so summing them needs no prior stores.
int g[6] = {5, 10, 15, 20, 25, 75};

int main() {
    int sum;
    int i;
    sum = 0;
    for (i = 0; i < 6; i = i + 1) {
        sum = sum + g[i];
    }
    return sum;  // 5+10+15+20+25+75 = 150
}
