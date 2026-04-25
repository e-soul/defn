#ifndef DEFN_HOSTED_TEST_RUNNER_H
#define DEFN_HOSTED_TEST_RUNNER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

class DefnHostedTestRunner : public RefCounted {
    GDCLASS(DefnHostedTestRunner, RefCounted);

  public:
    Dictionary run_registered_tests() const;

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif