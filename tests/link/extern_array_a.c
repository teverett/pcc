// EXPECT: 60
// An extern array: its real size still has to be stated directly
// (there's no initializer here to infer it from), and its elements
// live entirely in extern_array_b.c's own data, written there before
// this file ever reads them.
extern int shared_arr[3];

int main() {
    return shared_arr[0] + shared_arr[1] + shared_arr[2];  // 10+20+30 = 60
}
