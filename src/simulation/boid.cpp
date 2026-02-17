#include "simulation/boid.h"

void Boid::step(float dt, float linear_drag, float angular_drag) {
    for (const auto& t : thrusters) {
        body.apply_force(t.world_force(body.angle));
        body.apply_torque(t.torque(body.angle));
    }

    body.integrate(dt, linear_drag, angular_drag);
    body.clear_forces();
}
