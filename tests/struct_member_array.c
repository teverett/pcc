// EXPECT: 25
struct Buf {
    int data[4];
    int len;
};

int main() {
    struct Buf buf;
    buf.data[0] = 1;
    buf.data[1] = 2;
    buf.data[2] = 3;
    buf.data[3] = 4;
    buf.len = 15;
    return buf.data[0] + buf.data[1] + buf.data[2] + buf.data[3] + buf.len;
}
