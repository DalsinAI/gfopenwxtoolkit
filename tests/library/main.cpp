// The program side of scripts/test-library.sh's module. Prints PASS or FAIL
// per check, then "shapetest: N failed".
// Copyright (c) 2026 AmiCradle Team
#include "shapes.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

static int failed;
static void check(bool ok, const char *what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failed++;
}

struct Circle : Shape {                 // the program's class on the module's base
    long area() const override { return 3 * r * r; }
    long r = 2;
};

static long triple(long x)
{
    if (x < 0) throw std::invalid_argument("negative");
    return 3 * x;
}

int main()
{
    say("hello from the program");
    Shape *a = make_shape("rect"), *b = make_shape("square");
    Circle c;
    Shape *all[] = { a, b, &c };
    check(a->area() == 12 && b->area() == 25, "module objects' virtual calls");
    check(a->name() == "rect" && b->name() == "square" && c.name() == "shape", "std::string across");
    check(total_area(all, 3) == 49, "module calls the program's override");
    check(Shape::made == 2, "module data seen by the program");
    Shape::made = 7;
    check(Shape::made == 7 && std::strcmp(shape_names[1], "square") == 0, "module data written and constants read");
    check(dynamic_cast<Rect *>(a) != 0 && dynamic_cast<Rect *>(static_cast<Shape *>(&c)) == 0, "typeinfo across");
    bool caught = false;
    try { make_shape("hexagon"); }
    catch (const shape_error &e) { caught = std::strstr(e.what(), "hexagon") != 0; }
    check(caught, "module throws, program catches");
    check(call_back(triple, 4) == 12 && call_back(triple, -1) == -1, "program throws, module catches");
    long v[] = { 5, 1, 4, 2, 3 };
    sort_longs(v, 5);
    check(v[0] == 1 && v[4] == 5, "qsort callback in the module");
    check(twice(21) == 42, "template");
    delete a; delete b;
    std::printf("shapetest: %d failed\n", failed);
    return failed ? 10 : 0;
}
