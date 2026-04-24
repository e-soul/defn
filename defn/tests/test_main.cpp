#include "test_harness.h"

#include <cstdio>

int main() {
    int passed = 0;

    for (const defn::tests::TestCase &test_case : defn::tests::registry()) {
        try {
            test_case.function();
            std::printf("[PASS] %s\n", test_case.name);
            ++passed;
        } catch (const std::exception &error) {
            std::fprintf(stderr, "[FAIL] %s: %s\n", test_case.name, error.what());
            return 1;
        }
    }

    std::printf("%d test(s) passed\n", passed);
    return 0;
}