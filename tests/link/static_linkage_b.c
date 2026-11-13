static int shared = 1;

static int helper() {
    return shared + 1;  // 2
}

int otherHelper() {
    return helper();  // 2
}
