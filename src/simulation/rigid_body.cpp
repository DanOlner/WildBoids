#include "simulation/rigid_body.h"

void RigidBody::apply_force(Vec2 force) {
    net_force += force;
}

void RigidBody::apply_torque(float torque) {
    net_torque += torque;
}

void RigidBody::integrate(float dt, float linear_drag, float angular_drag) {
    // Semi-implicit Euler: update velocity first, then position
    velocity += (net_force / mass) * dt;
    velocity *= (1.0f - linear_drag * dt);
    position += velocity * dt;

    angular_velocity += (net_torque / moment_of_inertia) * dt;
    angular_velocity *= (1.0f - angular_drag * dt);
    angle += angular_velocity * dt;
}

void RigidBody::clear_forces() {
    net_force = {0, 0};
    net_torque = 0;
}
