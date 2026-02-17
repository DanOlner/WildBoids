#include "io/boid_spec.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

BoidSpec load_boid_spec(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open boid spec file: " + path);
    }

    json j = json::parse(file);

    BoidSpec spec;
    spec.version = j.at("version").get<std::string>();
    spec.type = j.at("type").get<std::string>();
    spec.mass = j.at("mass").get<float>();
    spec.moment_of_inertia = j.at("momentOfInertia").get<float>();
    spec.initial_energy = j.at("initialEnergy").get<float>();

    for (const auto& jt : j.at("thrusters")) {
        ThrusterSpec ts;
        ts.id = jt.at("id").get<int>();
        ts.label = jt.at("label").get<std::string>();
        ts.position = {jt.at("positionX").get<float>(), jt.at("positionY").get<float>()};
        ts.direction = {jt.at("directionX").get<float>(), jt.at("directionY").get<float>()};
        ts.max_thrust = jt.at("maxThrust").get<float>();
        spec.thrusters.push_back(ts);
    }

    return spec;
}

Boid create_boid_from_spec(const BoidSpec& spec) {
    Boid boid;
    boid.type = spec.type;
    boid.body.mass = spec.mass;
    boid.body.moment_of_inertia = spec.moment_of_inertia;
    boid.energy = spec.initial_energy;

    for (const auto& ts : spec.thrusters) {
        Thruster t;
        t.local_position = ts.position;
        t.local_direction = ts.direction.normalized();
        t.max_thrust = ts.max_thrust;
        t.power = 0;
        boid.thrusters.push_back(t);
    }

    return boid;
}

void save_boid_spec(const BoidSpec& spec, const std::string& path) {
    json j;
    j["version"] = spec.version;
    j["type"] = spec.type;
    j["mass"] = spec.mass;
    j["momentOfInertia"] = spec.moment_of_inertia;
    j["initialEnergy"] = spec.initial_energy;

    j["thrusters"] = json::array();
    for (const auto& ts : spec.thrusters) {
        json jt;
        jt["id"] = ts.id;
        jt["label"] = ts.label;
        jt["positionX"] = ts.position.x;
        jt["positionY"] = ts.position.y;
        jt["directionX"] = ts.direction.x;
        jt["directionY"] = ts.direction.y;
        jt["maxThrust"] = ts.max_thrust;
        j["thrusters"].push_back(jt);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + path);
    }
    file << j.dump(4) << std::endl;
}
