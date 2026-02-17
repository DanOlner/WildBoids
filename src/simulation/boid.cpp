#include "simulation/boid.h"

void Boid::step(float dt, float linear_drag, float angular_drag) {
    if (!alive) return;

    for (const auto& t : thrusters) {
        body.apply_force(t.world_force(body.angle));
        body.apply_torque(t.torque(body.angle));
    }

    body.integrate(dt, linear_drag, angular_drag);
    body.clear_forces();
}

float Boid::total_thrust() const {
    float total = 0.0f;
    for (const auto& t : thrusters) {
        total += t.power * t.max_thrust;
    }
    return total;
}
