int puts(char *text);

int sum9(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    return a + b + c + d + e + f + g + h + i;
}

int main() {
    if (sum9(1, 2, 3, 4, 5, 6, 7, 8, 9) == 45) {
        puts("hosted macOS ABI ok");
        return 0;
    }
    return 1;
}
