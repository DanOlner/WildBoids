#include "display/app.h"
#include "simulation/toroidal.h"
#include "simulation/sensor.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <fstream>
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

        renderer_.draw(world_, paused_ ? selected_boid_index_ : -1);
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
                            // Dump full sensor diagnostic CSV for first predator
                            const auto& boids = world_.get_boids();
                            const auto& wconfig = world_.get_config();
                            const auto& food = world_.get_food();
                            const auto& grid = world_.grid();
                            constexpr float RAD2DEG = 180.0f / 3.14159265f;

                            for (const auto& self : boids) {
                                if (self.type != "predator" || !self.alive || !self.sensors) continue;
                                if (!self.sensors->is_compound()) break;
                                const auto& cfg = self.sensors->compound_config();
                                int n_ch = static_cast<int>(cfg.channels.size());
                                int n_short = cfg.short_range_eye_count();
                                int n_long = cfg.long_range_eye_count();

                                // Find max range across all eyes for entity query
                                float max_range = 0;
                                for (const auto& eye : cfg.eyes) max_range = std::max(max_range, eye.max_range);
                                for (const auto& eye : cfg.long_range_eyes) max_range = std::max(max_range, eye.max_range);

                                std::ofstream csv("sensor_debug.csv");

                                // Section 1: Self
                                csv << "# Self\n";
                                csv << "self_x,self_y,self_angle_deg,self_type\n";
                                csv << self.body.position.x << "," << self.body.position.y << ","
                                    << self.body.angle * RAD2DEG << "," << self.type << "\n\n";

                                // Section 2: Nearby entities
                                csv << "# Nearby entities\n";
                                csv << "entity_type,world_x,world_y,delta_x,delta_y,body_angle_deg,distance,channel\n";

                                // Boids
                                std::vector<int> candidates;
                                grid.query(self.body.position, max_range, candidates);
                                for (int j : candidates) {
                                    if (&boids[j] == &self) continue;
                                    if (!boids[j].alive) continue;
                                    Vec2 delta = toroidal_delta(self.body.position, boids[j].body.position,
                                                                 wconfig.width, wconfig.height);
                                    float dist = std::sqrt(delta.length_squared());
                                    if (dist > max_range) continue;
                                    Vec2 body_delta = delta.rotated(-self.body.angle);
                                    float angle = std::atan2(body_delta.x, body_delta.y);
                                    std::string ch = (boids[j].type == self.type) ? "same" : "opposite";
                                    csv << boids[j].type << "," << boids[j].body.position.x << ","
                                        << boids[j].body.position.y << "," << delta.x << "," << delta.y << ","
                                        << angle * RAD2DEG << "," << dist << "," << ch << "\n";
                                }

                                // Food
                                for (const auto& f : food) {
                                    Vec2 delta = toroidal_delta(self.body.position, f.position,
                                                                 wconfig.width, wconfig.height);
                                    float dist = std::sqrt(delta.length_squared());
                                    if (dist > max_range) continue;
                                    Vec2 body_delta = delta.rotated(-self.body.angle);
                                    float angle = std::atan2(body_delta.x, body_delta.y);
                                    csv << "food," << f.position.x << "," << f.position.y << ","
                                        << delta.x << "," << delta.y << ","
                                        << angle * RAD2DEG << "," << dist << ",food\n";
                                }

                                // Section 3: Eye activations
                                csv << "\n# Eye activations\n";
                                csv << "tier,eye_id,center_angle_deg,arc_width_deg,max_range";
                                for (int c = 0; c < n_ch; ++c) {
                                    if (cfg.channels[c] == SensorChannel::Food) csv << ",food_signal";
                                    else if (cfg.channels[c] == SensorChannel::Same) csv << ",same_signal";
                                    else if (cfg.channels[c] == SensorChannel::Opposite) csv << ",opposite_signal";
                                }
                                csv << "\n";

                                for (int e = 0; e < n_short; ++e) {
                                    const auto& eye = cfg.eyes[e];
                                    csv << "short," << e << "," << eye.center_angle * RAD2DEG << ","
                                        << eye.arc_width * RAD2DEG << "," << eye.max_range;
                                    for (int c = 0; c < n_ch; ++c) {
                                        int idx = e * n_ch + c;
                                        csv << "," << self.sensor_outputs[idx];
                                    }
                                    csv << "\n";
                                }

                                int long_off = n_short * n_ch;
                                for (int e = 0; e < n_long; ++e) {
                                    const auto& eye = cfg.long_range_eyes[e];
                                    csv << "long," << e << "," << eye.center_angle * RAD2DEG << ","
                                        << eye.arc_width * RAD2DEG << "," << eye.max_range;
                                    for (int c = 0; c < n_ch; ++c) {
                                        int idx = long_off + e * n_ch + c;
                                        csv << "," << self.sensor_outputs[idx];
                                    }
                                    csv << "\n";
                                }

                                csv.close();
                                std::cerr << "Wrote sensor_debug.csv for predator at ("
                                          << self.body.position.x << ", " << self.body.position.y << ")\n";
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
                        case SDL_SCANCODE_K:
                        case SDL_SCANCODE_L:
                            if (paused_) {
                                const auto& boids = world_.get_boids();
                                int n = static_cast<int>(boids.size());
                                if (n == 0) break;
                                int dir = (event.key.scancode == SDL_SCANCODE_L) ? 1 : -1;
                                int start = selected_boid_index_;
                                if (start < 0) start = (dir == 1) ? -1 : n;
                                for (int i = 0; i < n; ++i) {
                                    start = (start + dir + n) % n;
                                    if (boids[start].alive) {
                                        selected_boid_index_ = start;
                                        std::cerr << "Selected boid " << start
                                                  << " (" << boids[start].type << ")\n";
                                        break;
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }

                // Paused-mode controls (allow key repeat for smooth nudging)
                if (paused_ && selected_boid_index_ >= 0) {
                    auto& boids = world_.get_boids_mut();
                    if (selected_boid_index_ < static_cast<int>(boids.size()) &&
                        boids[selected_boid_index_].alive) {
                        auto& body = boids[selected_boid_index_].body;
                        constexpr float NUDGE = 1.0f;
                        constexpr float ROTATE_STEP = 3.14159265f / 180.0f; // 1 degree
                        bool moved = false;
                        switch (event.key.scancode) {
                            case SDL_SCANCODE_UP:    body.position.y -= NUDGE; moved = true; break;
                            case SDL_SCANCODE_DOWN:  body.position.y += NUDGE; moved = true; break;
                            case SDL_SCANCODE_LEFT:  body.position.x -= NUDGE; moved = true; break;
                            case SDL_SCANCODE_RIGHT: body.position.x += NUDGE; moved = true; break;
                            case SDL_SCANCODE_COMMA:  body.angle += ROTATE_STEP; moved = true; break;
                            case SDL_SCANCODE_PERIOD: body.angle -= ROTATE_STEP; moved = true; break;
                            default: break;
                        }
                        if (moved) {
                            world_.refresh_sensors(selected_boid_index_, &rng_);
                        }
                    }
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (paused_ && selected_boid_index_ >= 0 &&
                    event.button.button == SDL_BUTTON_LEFT) {
                    auto& boids = world_.get_boids_mut();
                    if (selected_boid_index_ < static_cast<int>(boids.size()) &&
                        boids[selected_boid_index_].alive) {
                        const auto& config = world_.get_config();
                        float wx = renderer_.screen_to_world_x(event.button.x, config);
                        float wy = renderer_.screen_to_world_y(event.button.y, config);
                        boids[selected_boid_index_].body.position = {wx, wy};
                        world_.refresh_sensors(selected_boid_index_, &rng_);
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
