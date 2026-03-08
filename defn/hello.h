#ifndef HELLO_H
#define HELLO_H

#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

class Hello : public Sprite2D {
    GDCLASS(Hello, Sprite2D)

private:
    double speed;

protected:
    static void _bind_methods();

public:
    Hello();

    void _ready() override;
    void _process(double delta) override;

    void set_speed(const double p_speed);
    double get_speed() const;
};

#endif