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

  void set_show_neighbours(bool show) { show_neighbours_ = show; }
  bool show_neighbours() const { return show_neighbours_; }

  void set_show_sensors(bool show) { show_sensors_ = show; }
  bool show_sensors() const { return show_sensors_; }

private:
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  int window_width_;
  int window_height_;
  bool closed_ = false;
  bool show_thrusters_ = true;
  bool show_neighbours_ = false;
  bool show_sensors_ = false;

  // Boid triangle size in world units
  static constexpr float BOID_LENGTH = 12.0f;
  static constexpr float BOID_HALF_WIDTH = 4.0f;
  // Thruster indicator length in world units
  static constexpr float THRUSTER_LINE_LENGTH = 40.0f;

  float scale_x(const WorldConfig &config) const;
  float scale_y(const WorldConfig &config) const;
  float world_to_screen_x(float wx, const WorldConfig &config) const;
  float world_to_screen_y(float wy, const WorldConfig &config) const;

  static constexpr float NEIGHBOUR_RADIUS = 100.0f;

  static constexpr int SENSOR_ARC_SEGMENTS = 12;

  void draw_boid(const Boid &boid, const WorldConfig &config);
  void draw_thruster_indicators(const Boid &boid, const WorldConfig &config);
  void draw_neighbour_lines(const World &world);
  void draw_sensor_arcs(const Boid &boid, const WorldConfig &config);
  void draw_food(const std::vector<Food> &food, const WorldConfig &config);
};
