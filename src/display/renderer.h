#pragma once

#include "simulation/world.h"
#include <SDL3/SDL.h>

class Renderer {
public:
  Renderer(int window_width, int window_height, const char *title);
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  void draw(const World &world);
  void present();
  bool should_close() const;

  void set_show_thrusters(bool show) { show_thrusters_ = show; }
  bool show_thrusters() const { return show_thrusters_; }

private:
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  int window_width_;
  int window_height_;
  bool closed_ = false;
  bool show_thrusters_ = true;

  // Boid triangle size in world units
  static constexpr float BOID_LENGTH = 12.0f;
  static constexpr float BOID_HALF_WIDTH = 4.0f;
  // Thruster indicator length in world units
  static constexpr float THRUSTER_LINE_LENGTH = 40.0f;

  float scale_x(const WorldConfig &config) const;
  float scale_y(const WorldConfig &config) const;
  float world_to_screen_x(float wx, const WorldConfig &config) const;
  float world_to_screen_y(float wy, const WorldConfig &config) const;

  void draw_boid(const Boid &boid, const WorldConfig &config);
  void draw_thruster_indicators(const Boid &boid, const WorldConfig &config);
};
