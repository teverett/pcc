// EXPECT: 10
// A hand-rolled loop using goto/labels instead of while - sums 0..4.
int main() {
    int i = 0;
    int total = 0;
loop:
    if (i >= 5) goto done;
    total += i;
    i++;
    goto loop;
done:
    return total;
}
