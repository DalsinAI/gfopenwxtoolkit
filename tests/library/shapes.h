// A small C++ library for scripts/test-library.sh: what a program on
// openwxtoolkit.library does with wx, in miniature (classes with vtables,
// exceptions both ways, std::string, static data, iostream, a template).
// Copyright (c) 2026 AmiCradle Team
#ifndef SHAPES_H
#define SHAPES_H
#include <string>
#include <stdexcept>

struct Shape {
    virtual ~Shape();
    virtual long area() const = 0;
    virtual std::string name() const;
    static int made;                    // module data a program reads
};
struct Rect : Shape {
    Rect(long w, long h) : w(w), h(h) {}
    long area() const override;
    std::string name() const override;
    long w, h;
};
struct shape_error : std::runtime_error {
    explicit shape_error(const std::string &s) : std::runtime_error(s) {}
};
Shape *make_shape(const char *what);    // throws shape_error
long total_area(Shape *const *s, int n);
void say(const std::string &s);         // std::cout
void sort_longs(long *v, int n);        // qsort with a callback
long call_back(long (*fn)(long), long x);  // the module calls the program
extern const char *const shape_names[];
int count_made();                       // Shape::made, as the module sees it

template <class T> T twice(T x) { return x + x; }
#endif
