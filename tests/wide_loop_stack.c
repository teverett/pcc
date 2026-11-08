// EXPECT: 3000
// A loop whose body does long arithmetic runs to completion: each
// iteration's temporaries are reclaimed.
long t;

int main() {
    int i;
    t = 0;
    for (i = 0; i < 3000; i = i + 1) {
        t = t + 1;
    }
    return (int)t;
}
