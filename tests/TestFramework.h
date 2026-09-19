// Tiny dependency-free test framework (the project must build with no external
// packages; tests are part of the deliverable, per the brief).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace sh101test {

struct TestCase {
    const char* name;
    // Plain function pointer, not std::function: registration happens in static
    // initialisation in every test translation unit, and std::function's
    // template instantiation there is both unnecessary and (with the zig/libc++
    // toolchain used on the development machine) enough to corrupt the heap
    // before main() runs.
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& checkCount() { static int c = 0; return c; }
inline int& failureCount() { static int f = 0; return f; }
inline const char*& currentTest() { static const char* n = ""; return n; }

struct Registrar {
    Registrar(const char* name, void (*fn)()) {
        registry().push_back(TestCase{ name, fn });
    }
};

inline void fail(const char* expr, const char* file, int line, const std::string& detail) {
    ++failureCount();
    std::printf("  FAIL [%s] %s:%d: %s%s%s\n", currentTest(), file, line, expr,
                detail.empty() ? "" : " -> ", detail.c_str());
}

inline bool check(bool cond, const char* expr, const char* file, int line) {
    ++checkCount();
    if (!cond) {
        fail(expr, file, line, "");
        return false;
    }
    return true;
}

inline bool checkNear(double a, double b, double tol, const char* expr, const char* file, int line) {
    ++checkCount();
    const bool ok = std::fabs(a - b) <= tol;
    if (!ok) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "got %.9g, expected %.9g +/- %.3g (delta %.3g)", a, b, tol,
                      a - b);
        fail(expr, file, line, buf);
    }
    return ok;
}

inline bool checkLess(double a, double b, const char* expr, const char* file, int line) {
    ++checkCount();
    if (!(a < b)) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "got %.9g, expected < %.9g", a, b);
        fail(expr, file, line, buf);
        return false;
    }
    return true;
}

inline bool checkGreater(double a, double b, const char* expr, const char* file, int line) {
    ++checkCount();
    if (!(a > b)) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "got %.9g, expected > %.9g", a, b);
        fail(expr, file, line, buf);
        return false;
    }
    return true;
}

} // namespace sh101test

#define SH101_TEST(name)                                                      \
    static void name();                                                       \
    static ::sh101test::Registrar sh101_reg_##name(#name, name);              \
    static void name()

#define CHECK(cond) ::sh101test::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol) ::sh101test::checkNear((double)(a), (double)(b), (double)(tol), #a " ~= " #b, __FILE__, __LINE__)
#define CHECK_LT(a, b) ::sh101test::checkLess((double)(a), (double)(b), #a " < " #b, __FILE__, __LINE__)
#define CHECK_GT(a, b) ::sh101test::checkGreater((double)(a), (double)(b), #a " > " #b, __FILE__, __LINE__)

// Constant expected by the model that is worth restating in test notes: all
// tolerances in this suite are acceptance thresholds for the *modelled*
// behaviour; where a measurement against real hardware is still outstanding the
// test comment says so explicitly.
