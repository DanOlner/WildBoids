#include "display/app.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>

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
            accumulator += frame_time * speed_multiplier_;
            while (accumulator >= dt) {
                apply_random_wander();
                world_.step(static_cast<float>(dt), &rng_);
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
                        case SDL_SCANCODE_P: {
                            // Debug: dump first predator's sensor outputs
                            const auto& boids = world_.get_boids();
                            for (const auto& b : boids) {
                                if (b.type != "predator" || !b.alive || !b.sensors) continue;
                                const auto& cfg = b.sensors->compound_config();
                                int n_ch = static_cast<int>(cfg.channels.size());
                                int n_short = cfg.short_range_eye_count();
                                int n_long = cfg.long_range_eye_count();
                                std::cerr << "=== Predator sensor dump ===\n";
                                std::cerr << "sensor_outputs.size()=" << b.sensor_outputs.size()
                                          << " expected=" << cfg.total_inputs() << "\n";
                                std::cerr << "n_short=" << n_short << " n_long=" << n_long
                                          << " n_channels=" << n_ch << "\n";
                                std::cerr << "Short-range eyes:\n";
                                for (int e = 0; e < n_short; ++e) {
                                    float max_sig = 0;
                                    for (int c = 0; c < n_ch; ++c) {
                                        int idx = e * n_ch + c;
                                        float v = b.sensor_outputs[idx];
                                        if (v > 0.01f)
                                            std::cerr << "  eye[" << e << "] ch=" << c
                                                      << " angle=" << (cfg.eyes[e].center_angle * 180/3.14159f)
                                                      << "deg val=" << v << "\n";
                                        max_sig = std::max(max_sig, v);
                                    }
                                }
                                int long_off = n_short * n_ch;
                                std::cerr << "Long-range eyes (offset=" << long_off << "):\n";
                                for (int e = 0; e < n_long; ++e) {
                                    for (int c = 0; c < n_ch; ++c) {
                                        int idx = long_off + e * n_ch + c;
                                        float v = b.sensor_outputs[idx];
                                        if (v > 0.01f)
                                            std::cerr << "  long_eye[" << e << "] ch=" << c
                                                      << " angle=" << (cfg.long_range_eyes[e].center_angle * 180/3.14159f)
                                                      << "deg val=" << v << "\n";
                                    }
                                }
                                int pi = (n_short + n_long) * n_ch;
                                std::cerr << "Proprioceptive (from idx " << pi << "):";
                                for (int i = pi; i < static_cast<int>(b.sensor_outputs.size()); ++i)
                                    std::cerr << " " << b.sensor_outputs[i];
                                std::cerr << "\n";
                                break; // just first predator
                            }
                            break;
                        }
                        case SDL_SCANCODE_F:
                            if (speed_multiplier_ == 1) speed_multiplier_ = 2;
                            else if (speed_multiplier_ == 2) speed_multiplier_ = 4;
                            else if (speed_multiplier_ == 4) speed_multiplier_ = 8;
                            else if (speed_multiplier_ == 8) speed_multiplier_ = 16;
                            else speed_multiplier_ = 1;
                            std::cerr << "Speed: " << speed_multiplier_ << "x\n";
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
        if (!boid.alive) continue;  // dead boid
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
