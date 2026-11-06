// Copyright (c) 2026 AmiCradle Team
#include "shapes.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

int Shape::made;
const char *const shape_names[] = { "rect", "square", 0 };
static std::string greeting("shapes ready");     // a constructor at start

Shape::~Shape() {}
std::string Shape::name() const { return "shape"; }
long Rect::area() const { return w * h; }
std::string Rect::name() const { return w == h ? "square" : "rect"; }

Shape *make_shape(const char *what)
{
    if (!std::strcmp(what, "rect")) { Shape::made++; return new Rect(3, 4); }
    if (!std::strcmp(what, "square")) { Shape::made++; return new Rect(5, 5); }
    throw shape_error(std::string("no shape called ") + what);
}

long total_area(Shape *const *s, int n)
{
    long t = 0;
    for (int i = 0; i < n; i++) t += twice(s[i]->area()) / 2;
    return t;
}

void say(const std::string &s) { std::cout << greeting << ": " << s << std::endl; }

static int cmp(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return x < y ? -1 : x > y;
}
void sort_longs(long *v, int n) { std::qsort(v, n, sizeof *v, cmp); }

long call_back(long (*fn)(long), long x)
{
    try { return fn(x); }
    catch (const std::exception &e) { say(std::string("caught from the program: ") + e.what()); return -1; }
}
