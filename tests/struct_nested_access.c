// EXPECT: 42
struct Inner {
    int v;
};
struct Outer {
    struct Inner in;
    int w;
};

int main() {
    struct Outer o;
    o.in.v = 30;
    o.w = 12;
    return o.in.v + o.w;
}
