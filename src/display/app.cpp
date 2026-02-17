#include "display/app.h"
#include <SDL3/SDL.h>
#include <algorithm>

App::App(World& world, Renderer& renderer, std::mt19937& rng)
    : world_(world), renderer_(renderer), rng_(rng) {}

void App::run() {
    const double dt = 1.0 / 120.0; // simulation at 120 Hz
    double accumulator = 0.0;
    Uint64 last_time = SDL_GetPerformanceCounter();
    double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running_ && !renderer_.should_close()) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frame_time = static_cast<double>(now - last_time) / freq;
        last_time = now;

        // Cap frame time to prevent spiral of death
        if (frame_time > 0.1) frame_time = 0.1;

        handle_events();

        if (!paused_) {
            accumulator += frame_time;
            while (accumulator >= dt) {
                apply_random_wander();
                world_.step(static_cast<float>(dt));
                accumulator -= dt;
            }
        } else {
            accumulator = 0.0; // don't build up time while paused
        }

        renderer_.draw(world_);
        renderer_.present();
    }
}

void App::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat) {
                    switch (event.key.scancode) {
                        case SDL_SCANCODE_ESCAPE:
                            running_ = false;
                            break;
                        case SDL_SCANCODE_SPACE:
                            paused_ = !paused_;
                            break;
                        case SDL_SCANCODE_T:
                            renderer_.set_show_thrusters(!renderer_.show_thrusters());
                            break;
                        case SDL_SCANCODE_D:
                            renderer_.set_show_neighbours(!renderer_.show_neighbours());
                            break;
                        case SDL_SCANCODE_S:
                            renderer_.set_show_sensors(!renderer_.show_sensors());
                            break;
                        default:
                            break;
                    }
                }
                break;

            default:
                break;
        }
    }
}

// Temporary pre-brain behaviour: constant rear thrust + random steering.
// Skips boids that have a brain — those are controlled by run_brains().
void App::apply_random_wander() {
    std::uniform_real_distribution<float> steer_dist(-0.4f, 0.4f);

    for (auto& boid : world_.get_boids_mut()) {
        if (boid.brain) continue;  // brain-driven boid
        if (boid.thrusters.size() < 3) continue;

        // Constant rear thrust
        boid.thrusters[0].power = 0.35f;

        // Random steering nudge
        float steer = steer_dist(rng_);
        boid.thrusters[1].power = std::max(0.0f, steer);   // left-rear
        boid.thrusters[2].power = std::max(0.0f, -steer);  // right-rear

        // Front brake off
        if (boid.thrusters.size() > 3) {
            boid.thrusters[3].power = 0.0f;
        }
    }
}
