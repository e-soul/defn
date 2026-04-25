#include "defn_hosted_test_runner.h"

#include "test_harness.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <exception>

namespace defn {

void DefnHostedTestRunner::_bind_methods() {
    ClassDB::bind_static_method(get_class_static(), D_METHOD("run_registered_tests"), &DefnHostedTestRunner::run_registered_tests);
}

Dictionary DefnHostedTestRunner::run_registered_tests() {
    Array failures;
    int passed = 0;

    for (const tests::TestCase &test_case : tests::registry()) {
        try {
            test_case.function();
            UtilityFunctions::print(String("[PASS] ") + test_case.name);
            ++passed;
        } catch (const std::exception &error) {
            Dictionary failure;
            failure["name"] = test_case.name;
            failure["message"] = error.what();
            failures.push_back(failure);
            UtilityFunctions::printerr(String("[FAIL] ") + test_case.name + ": " + error.what());
        } catch (...) {
            Dictionary failure;
            failure["name"] = test_case.name;
            failure["message"] = "unknown exception";
            failures.push_back(failure);
            UtilityFunctions::printerr(String("[FAIL] ") + test_case.name + ": unknown exception");
        }
    }

    Dictionary result;
    result["success"] = failures.is_empty();
    result["passed"] = passed;
    result["failed"] = failures.size();
    result["total"] = passed + failures.size();
    result["failures"] = failures;
    return result;
}

} // namespace defn