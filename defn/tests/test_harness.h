#ifndef DEFN_TEST_HARNESS_H
#define DEFN_TEST_HARNESS_H

#include <cmath>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace defn::tests {

struct TestCase {
    const char *name = "";
    void (*function)() = nullptr;
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char *name, void (*function)()) { registry().push_back({name, function}); }
};

[[noreturn]] inline void fail(const char *file, int line, const std::string &message) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + message);
}

template <typename Left, typename Right> inline void check_equal(const Left &left, const Right &right, const char *file, int line, const char *expr) {
    if (!(left == right)) {
        fail(file, line, std::string("expected equality for ") + expr);
    }
}

inline void check_close(double left, double right, double epsilon, const char *file, int line, const char *expr) {
    if (std::fabs(left - right) > epsilon) {
        fail(file, line, std::string("expected approximate equality for ") + expr);
    }
}

} // namespace defn::tests

#define DEFN_TEST(name)                                                                                                                                        \
    static void name();                                                                                                                                        \
    static ::defn::tests::Registrar registrar_##name(#name, &name);                                                                                            \
    static void name()

#define DEFN_REQUIRE(expr)                                                                                                                                     \
    do {                                                                                                                                                       \
        if (!(expr)) {                                                                                                                                         \
            ::defn::tests::fail(__FILE__, __LINE__, std::string("require failed: ") + #expr);                                                                  \
        }                                                                                                                                                      \
    } while (false)

#define DEFN_CHECK(expr) DEFN_REQUIRE(expr)

#define DEFN_CHECK_EQ(left, right) ::defn::tests::check_equal((left), (right), __FILE__, __LINE__, #left " == " #right)

#define DEFN_CHECK_CLOSE(left, right, epsilon) ::defn::tests::check_close((left), (right), (epsilon), __FILE__, __LINE__, #left " ~= " #right)

#endif