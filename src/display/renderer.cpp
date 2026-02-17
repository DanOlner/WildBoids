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

void Renderer::draw(const World& world) {
    // Black background
    SDL_SetRenderDrawColor(renderer_, 10, 10, 15, 255);
    SDL_RenderClear(renderer_);

    const auto& config = world.get_config();

    for (const auto& boid : world.get_boids()) {
        if (show_sensors_) {
            draw_sensor_arcs(boid, config);
        }
        draw_boid(boid, config);
        if (show_thrusters_) {
            draw_thruster_indicators(boid, config);
        }
    }

    if (show_neighbours_) {
        draw_neighbour_lines(world);
    }
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

void Renderer::draw_sensor_arcs(const Boid& boid, const WorldConfig& config) {
    if (!boid.sensors) return;

    const auto& specs = boid.sensors->specs();
    for (int si = 0; si < static_cast<int>(specs.size()); ++si) {
        const auto& spec = specs[si];
        float signal = (si < static_cast<int>(boid.sensor_outputs.size()))
                       ? boid.sensor_outputs[si] : 0.0f;

        // Colour: dim cyan when no signal, bright yellow-green when detecting
        Uint8 r = static_cast<Uint8>(signal * 200);
        Uint8 g = static_cast<Uint8>(80 + signal * 175);
        Uint8 b = static_cast<Uint8>(120 * (1.0f - signal));
        SDL_SetRenderDrawColor(renderer_, r, g, b, 180);

        float world_angle = boid.body.angle; // boid heading
        float arc_start = world_angle + spec.center_angle - spec.arc_width * 0.5f;
        float arc_end   = world_angle + spec.center_angle + spec.arc_width * 0.5f;
        float range = spec.max_range;

        Vec2 pos = boid.body.position;

        // Draw arc outline: two radial lines + arc segments
        // Start radial line
        Vec2 start_dir = Vec2{0, range}.rotated(arc_start);
        SDL_RenderLine(renderer_,
            world_to_screen_x(pos.x, config),
            world_to_screen_y(pos.y, config),
            world_to_screen_x(pos.x + start_dir.x, config),
            world_to_screen_y(pos.y + start_dir.y, config));

        // End radial line
        Vec2 end_dir = Vec2{0, range}.rotated(arc_end);
        SDL_RenderLine(renderer_,
            world_to_screen_x(pos.x, config),
            world_to_screen_y(pos.y, config),
            world_to_screen_x(pos.x + end_dir.x, config),
            world_to_screen_y(pos.y + end_dir.y, config));

        // Arc curve segments
        float step = (arc_end - arc_start) / SENSOR_ARC_SEGMENTS;
        for (int seg = 0; seg < SENSOR_ARC_SEGMENTS; ++seg) {
            float a0 = arc_start + step * seg;
            float a1 = arc_start + step * (seg + 1);
            Vec2 p0 = pos + Vec2{0, range}.rotated(a0);
            Vec2 p1 = pos + Vec2{0, range}.rotated(a1);
            SDL_RenderLine(renderer_,
                world_to_screen_x(p0.x, config),
                world_to_screen_y(p0.y, config),
                world_to_screen_x(p1.x, config),
                world_to_screen_y(p1.y, config));
        }
    }
}
