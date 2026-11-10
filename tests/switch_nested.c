// EXPECT: 23
// A switch nested inside another case's body: the inner switch's own
// `break` must stop only the inner switch, letting the outer case keep
// running afterward and add its own contribution.
int main() {
    int x = 1;
    int y = 2;
    int result = 0;
    switch (x) {
        case 1:
            switch (y) {
                case 1:
                    result += 1;
                    break;
                case 2:
                    result += 20;
                    break;
            }
            result += 3;
            break;
        default:
            result = 99;
    }
    return result;
}
