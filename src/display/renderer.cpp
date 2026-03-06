#include "display/renderer.h"
#include "simulation/toroidal.h"
#include <stdexcept>
#include <cmath>
#include <array>
#include <vector>


Renderer::Renderer(int window_width, int window_height, const char* title)
    : window_width_(window_width), window_height_(window_height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(title, window_width, window_height, 0);
    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }
}

Renderer::~Renderer() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Renderer::should_close() const {
    return closed_;
}

float Renderer::scale_x(const WorldConfig& config) const {
    return static_cast<float>(window_width_) / config.width;
}

float Renderer::scale_y(const WorldConfig& config) const {
    return static_cast<float>(window_height_) / config.height;
}

float Renderer::world_to_screen_x(float wx, const WorldConfig& config) const {
    return wx * scale_x(config);
}

// Screen Y is flipped: screen (0,0) is top-left, world (0,0) is bottom-left
float Renderer::world_to_screen_y(float wy, const WorldConfig& config) const {
    return static_cast<float>(window_height_) - wy * scale_y(config);
}

float Renderer::screen_to_world_x(float sx, const WorldConfig& config) const {
    return sx / scale_x(config);
}

float Renderer::screen_to_world_y(float sy, const WorldConfig& config) const {
    return (static_cast<float>(window_height_) - sy) / scale_y(config);
}

void Renderer::draw(const World& world, int selected_boid) {
    // Black background
    SDL_SetRenderDrawColor(renderer_, 10, 10, 15, 255);
    SDL_RenderClear(renderer_);

    const auto& config = world.get_config();

    draw_food(world.get_food(), config);

    const auto& boids = world.get_boids();
    for (int i = 0; i < static_cast<int>(boids.size()); ++i) {
        const auto& boid = boids[i];
        if (!boid.alive) continue;
        if (show_sensors_) {
            draw_sensor_arcs(boid, config);
        }
        draw_boid(boid, config);
        if (show_thrusters_) {
            draw_thruster_indicators(boid, config);
        }
        if (i == selected_boid) {
            draw_selection_ring(boid, config);
        }
    }

    if (show_neighbours_) {
        draw_neighbour_lines(world);
    }

    draw_death_flashes(config);
}

void Renderer::present() {
    SDL_RenderPresent(renderer_);
}

void Renderer::draw_boid(const Boid& boid, const WorldConfig& config) {
    // Triangle vertices in body-local space (+Y = forward)
    // Tip (nose), bottom-left, bottom-right
    Vec2 local_verts[3] = {
        {0, BOID_LENGTH * 0.6f},              // tip
        {-BOID_HALF_WIDTH, -BOID_LENGTH * 0.4f}, // rear left
        {BOID_HALF_WIDTH, -BOID_LENGTH * 0.4f},  // rear right
    };

    // Rotate by heading angle, translate to world position, convert to screen
    SDL_Vertex sdl_verts[3];

    // Colour by type
    SDL_FColor color;
    if (boid.type == "predator") {
        color = {0.9f, 0.2f, 0.15f, 1.0f}; // red
    } else {
        color = {0.2f, 0.85f, 0.3f, 1.0f};  // green
    }

    for (int i = 0; i < 3; i++) {
        Vec2 rotated = local_verts[i].rotated(boid.body.angle);
        Vec2 world_pos = boid.body.position + rotated;

        sdl_verts[i].position.x = world_to_screen_x(world_pos.x, config);
        sdl_verts[i].position.y = world_to_screen_y(world_pos.y, config);
        sdl_verts[i].color = color;
    }

    SDL_RenderGeometry(renderer_, nullptr, sdl_verts, 3, nullptr, 0);
}

void Renderer::draw_thruster_indicators(const Boid& boid, const WorldConfig& config) {
    for (const auto& t : boid.thrusters) {
        if (t.power < 0.01f) continue; // skip inactive thrusters

        // Thruster position in world space
        Vec2 world_pos = boid.body.position + t.local_position.rotated(boid.body.angle);

        // Draw exhaust line opposite to thrust direction
        Vec2 exhaust_dir = (t.local_direction * -1.0f).rotated(boid.body.angle);
        float line_len = THRUSTER_LINE_LENGTH * t.power;
        Vec2 end_pos = world_pos + exhaust_dir * line_len;

        // Bright orange-yellow, intensity scales with power
        Uint8 r = static_cast<Uint8>(255 * (0.5f + 0.5f * t.power));
        Uint8 g = static_cast<Uint8>(180 * (0.5f + 0.5f * t.power));
        SDL_SetRenderDrawColor(renderer_, r, g, 20, 255);
        SDL_RenderLine(renderer_,
            world_to_screen_x(world_pos.x, config),
            world_to_screen_y(world_pos.y, config),
            world_to_screen_x(end_pos.x, config),
            world_to_screen_y(end_pos.y, config));
    }
}

void Renderer::draw_neighbour_lines(const World& world) {
    const auto& config = world.get_config();
    const auto& boids = world.get_boids();
    const auto& grid = world.grid();
    const float radius_sq = NEIGHBOUR_RADIUS * NEIGHBOUR_RADIUS;

    SDL_SetRenderDrawColor(renderer_, 60, 120, 200, 120);

    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(boids.size()); ++i) {
        Vec2 pos_a = boids[i].body.position;

        candidates.clear();
        grid.query(pos_a, NEIGHBOUR_RADIUS, candidates);

        for (int j : candidates) {
            if (j <= i) continue; // draw each pair once

            Vec2 delta = toroidal_delta(pos_a, boids[j].body.position,
                                        config.width, config.height);
            if (delta.length_squared() > radius_sq) continue;

            // Draw line from boid i to (boid i + delta) — correct for toroidal
            Vec2 end = pos_a + delta;

            SDL_RenderLine(renderer_,
                world_to_screen_x(pos_a.x, config),
                world_to_screen_y(pos_a.y, config),
                world_to_screen_x(end.x, config),
                world_to_screen_y(end.y, config));
        }
    }
}

void Renderer::draw_food(const std::vector<Food>& food, const WorldConfig& config) {
    // Draw each food item as a small filled rectangle (dot)
    SDL_SetRenderDrawColor(renderer_, 220, 200, 50, 255); // yellow-gold

    for (const auto& f : food) {
        float sx = world_to_screen_x(f.position.x, config);
        float sy = world_to_screen_y(f.position.y, config);
        SDL_FRect rect = {sx - 2.0f, sy - 2.0f, 4.0f, 4.0f};
        SDL_RenderFillRect(renderer_, &rect);
    }
}

// Draw a single sensor arc with given geometry and signal strength
// tint: 0 = green (short-range default), 1 = blue/cyan (long-range)
void Renderer::draw_one_arc(const Boid& boid, const WorldConfig& config,
                             float center_angle, float arc_width, float max_range, float signal,
                             int channel_tint, bool long_range) {
    // Apply sqrt curve so weak signals are still clearly visible
    float s = (signal > 0.0f) ? std::sqrt(signal) : 0.0f;

    Uint8 r, g, b;
    Uint8 base = long_range ? 40 : 60;  // dimmer baseline for long-range
    switch (channel_tint) {
        case 0:  // Food — yellow/orange
            r = static_cast<Uint8>(base + s * (220 - base));
            g = static_cast<Uint8>(base + s * (180 - base));
            b = static_cast<Uint8>(base * (1.0f - s));
            break;
        case 1:  // Same — blue/cyan
            r = static_cast<Uint8>(base * (1.0f - s));
            g = static_cast<Uint8>(base + s * (180 - base));
            b = static_cast<Uint8>(base + s * (255 - base));
            break;
        case 2:  // Opposite — red/magenta
            r = static_cast<Uint8>(base + s * (255 - base));
            g = static_cast<Uint8>(base * (1.0f - s));
            b = static_cast<Uint8>(base + s * (80 - base));
            break;
        default:  // Inactive — dim grey
            r = g = b = base;
            break;
    }
    SDL_SetRenderDrawColor(renderer_, r, g, b, 180);

    // Negate center_angle: sensor angles use atan2(x,y) convention where
    // positive = clockwise (right), but rotated() uses standard CCW convention.
    float world_angle = boid.body.angle;
    float arc_start = world_angle - center_angle - arc_width * 0.5f;
    float arc_end   = world_angle - center_angle + arc_width * 0.5f;
    Vec2 pos = boid.body.position;

    // Radial lines
    Vec2 start_dir = Vec2{0, max_range}.rotated(arc_start);
    SDL_RenderLine(renderer_,
        world_to_screen_x(pos.x, config), world_to_screen_y(pos.y, config),
        world_to_screen_x(pos.x + start_dir.x, config),
        world_to_screen_y(pos.y + start_dir.y, config));

    Vec2 end_dir = Vec2{0, max_range}.rotated(arc_end);
    SDL_RenderLine(renderer_,
        world_to_screen_x(pos.x, config), world_to_screen_y(pos.y, config),
        world_to_screen_x(pos.x + end_dir.x, config),
        world_to_screen_y(pos.y + end_dir.y, config));

    // Arc curve
    float step = (arc_end - arc_start) / SENSOR_ARC_SEGMENTS;
    for (int seg = 0; seg < SENSOR_ARC_SEGMENTS; ++seg) {
        float a0 = arc_start + step * seg;
        float a1 = arc_start + step * (seg + 1);
        Vec2 p0 = pos + Vec2{0, max_range}.rotated(a0);
        Vec2 p1 = pos + Vec2{0, max_range}.rotated(a1);
        SDL_RenderLine(renderer_,
            world_to_screen_x(p0.x, config), world_to_screen_y(p0.y, config),
            world_to_screen_x(p1.x, config), world_to_screen_y(p1.y, config));
    }
}

void Renderer::draw_sensor_arcs(const Boid& boid, const WorldConfig& config) {
    if (!boid.sensors) return;

    if (boid.sensors->is_compound()) {
        // Compound eyes: one arc per eye, colour by max signal across channels
        const auto& cfg = boid.sensors->compound_config();
        int n_channels = static_cast<int>(cfg.channels.size());
        int n_short = cfg.short_range_eye_count();

        // Helper: find dominant channel and its signal for an eye
        auto find_dominant = [&](int out_offset, int eye_idx) -> std::pair<float, int> {
            float max_signal = 0.0f;
            int dominant_ch = 3; // inactive
            for (int c = 0; c < n_channels; ++c) {
                int idx = out_offset + eye_idx * n_channels + c;
                if (idx < static_cast<int>(boid.sensor_outputs.size())) {
                    float v = boid.sensor_outputs[idx];
                    if (v > max_signal) {
                        max_signal = v;
                        // Map channel enum to tint: Food=0, Same=1, Opposite=2
                        dominant_ch = static_cast<int>(cfg.channels[c]);
                    }
                }
            }
            return {max_signal, dominant_ch};
        };

        // Short-range eyes
        for (int e = 0; e < n_short; ++e) {
            const auto& eye = cfg.eyes[e];
            auto [signal, tint] = find_dominant(0, e);
            draw_one_arc(boid, config, eye.center_angle, eye.arc_width, eye.max_range, signal, tint, false);
        }

        // Long-range eyes
        int long_offset = n_short * n_channels;
        for (int e = 0; e < cfg.long_range_eye_count(); ++e) {
            const auto& eye = cfg.long_range_eyes[e];
            auto [signal, tint] = find_dominant(long_offset, e);
            draw_one_arc(boid, config, eye.center_angle, eye.arc_width, eye.max_range, signal, tint, true);
        }
    } else {
        // Legacy sensor arcs
        const auto& specs = boid.sensors->specs();
        for (int si = 0; si < static_cast<int>(specs.size()); ++si) {
            const auto& spec = specs[si];
            float signal = (si < static_cast<int>(boid.sensor_outputs.size()))
                           ? boid.sensor_outputs[si] : 0.0f;
            draw_one_arc(boid, config, spec.center_angle, spec.arc_width, spec.max_range, signal);
        }
    }
}

void Renderer::draw_selection_ring(const Boid& boid, const WorldConfig& config) {
    constexpr float RING_RADIUS = 20.0f;
    constexpr int SEGMENTS = 24;
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);

    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = 2.0f * 3.14159265f * i / SEGMENTS;
        float a1 = 2.0f * 3.14159265f * (i + 1) / SEGMENTS;
        float x0 = world_to_screen_x(boid.body.position.x + RING_RADIUS * std::cos(a0), config);
        float y0 = world_to_screen_y(boid.body.position.y + RING_RADIUS * std::sin(a0), config);
        float x1 = world_to_screen_x(boid.body.position.x + RING_RADIUS * std::cos(a1), config);
        float y1 = world_to_screen_y(boid.body.position.y + RING_RADIUS * std::sin(a1), config);
        SDL_RenderLine(renderer_, x0, y0, x1, y1);
    }
}

void Renderer::add_death_flash(float x, float y, bool is_predator) {
    death_flashes_.push_back({x, y, is_predator, DeathFlash::MAX_FRAMES});
}

void Renderer::draw_death_flashes(const WorldConfig& config) {
    if (death_flashes_.empty()) return;

    float w = static_cast<float>(window_width_);
    float h = static_cast<float>(window_height_);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    for (auto& flash : death_flashes_) {
        float alpha = static_cast<float>(flash.frames_remaining) / DeathFlash::MAX_FRAMES;
        Uint8 a = static_cast<Uint8>(255 * alpha);

        if (flash.is_predator) {
            SDL_SetRenderDrawColor(renderer_, 255, 60, 40, a);
        } else {
            SDL_SetRenderDrawColor(renderer_, 40, 255, 60, a);
        }

        float sx = world_to_screen_x(flash.x, config);
        float sy = world_to_screen_y(flash.y, config);

        // Full-width horizontal line and full-height vertical line
        for (float offset = -1.0f; offset <= 1.0f; offset += 1.0f) {
            SDL_RenderLine(renderer_, 0, sy + offset, w, sy + offset);
            SDL_RenderLine(renderer_, sx + offset, 0, sx + offset, h);
        }

        --flash.frames_remaining;
    }

    // Remove expired flashes
    std::erase_if(death_flashes_, [](const DeathFlash& f) {
        return f.frames_remaining <= 0;
    });
}
