// EXPECT: 7
// Exercises UnusedGlobalElimination.cpp's own two cases: a plain
// unused global/array get dropped, while a global reachable only
// through another live global's own array initializer (the
// dispatch-table idiom) stays - `kept_by_table` is never referenced
// from `main` directly, only through `table`'s own initializer.
int unused_global;
int unused_array[5];
int kept_by_table;
int table[2] = {kept_by_table, 0};
int main() {
    kept_by_table = 7;
    if (table[1] == 0) {
        return kept_by_table;
    }
    return 0;
}
