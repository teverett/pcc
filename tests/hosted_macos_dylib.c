int hosted_custom_value();
int puts(char *text);

int main() {
    if (hosted_custom_value() != 42) return 7;
    puts("custom dylib import ok");
    return 0;
}
