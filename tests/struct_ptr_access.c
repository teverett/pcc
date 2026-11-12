// EXPECT: 30
struct Point {
    int x;
    int y;
};

void move(struct Point *p, int dx, int dy) {
    p->x = p->x + dx;
    p->y = p->y + dy;
}

int main() {
    struct Point p;
    p.x = 1;
    p.y = 2;
    move(&p, 10, 17);
    return p.x + p.y;
}
