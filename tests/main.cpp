// Test runner entry point.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "TestFramework.h"

int main(int argc, char** argv) {
    // Unbuffered output: if a test crashes, the log must still show how far it
    // got (a redirected, block-buffered stdout would lose everything).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* filter = (argc > 1) ? argv[1] : nullptr;
    std::printf("SH-101 model validation tests\n");
    std::printf("=============================\n");

    int run = 0;
    for (const auto& tc : sh101test::registry()) {
        if (filter && std::strstr(tc.name, filter) == nullptr) continue;
        ++run;
        sh101test::currentTest() = tc.name;
        const int before = sh101test::failureCount();
        tc.fn();
        const char* status = (sh101test::failureCount() == before) ? "ok" : "FAILED";
        std::printf("[%s] %s\n", status, tc.name);
    }

    std::printf("=============================\n");
    std::printf("%d test(s) run, %d check(s), %d failure(s)\n", run, sh101test::checkCount(),
                sh101test::failureCount());
    return sh101test::failureCount() == 0 ? 0 : 1;
}
