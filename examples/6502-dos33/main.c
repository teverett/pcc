#include "../../src/rt/apple2/apple2.h"

int main() {
    a2_home();
    a2_settxt();
    a2_vtab(10);
    a2_setinv();
    a2_puts("HELLO, WORLD!");
    a2_setnorm();
    a2_crout();
    return 0;
}
