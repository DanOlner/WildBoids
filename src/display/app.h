#pragma once

#include "simulation/world.h"
#include "display/renderer.h"
#include <random>

class App {
public:
    App(World& world, Renderer& renderer, std::mt19937& rng);
    void run();

private:
    World& world_;
    Renderer& renderer_;
    std::mt19937& rng_;
    bool running_ = true;
    bool paused_ = false;
    int speed_multiplier_ = 1;

    void handle_events();
    void apply_random_wander();
};
