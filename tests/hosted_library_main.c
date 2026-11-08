extern int library_value();

int main() {
    if (library_value() != 42) return 1;
    return library_value();
}
