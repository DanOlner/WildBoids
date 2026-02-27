#include "io/boid_spec.h"
#include "brain/neat_genome.h"
#include "brain/neat_network.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

static constexpr float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;
static constexpr float RAD_TO_DEG = 180.0f / static_cast<float>(M_PI);

static EntityFilter parse_entity_filter(const std::string& s) {
    if (s == "prey") return EntityFilter::Prey;
    if (s == "predator") return EntityFilter::Predator;
    if (s == "food") return EntityFilter::Food;
    if (s == "speed") return EntityFilter::Speed;
    if (s == "angularVelocity") return EntityFilter::AngularVelocity;
    if (s == "noise") return EntityFilter::Noise;
    return EntityFilter::Any;
}

static std::string entity_filter_to_string(EntityFilter f) {
    switch (f) {
        case EntityFilter::Prey:     return "prey";
        case EntityFilter::Predator: return "predator";
        case EntityFilter::Any:      return "any";
        case EntityFilter::Food:     return "food";
        case EntityFilter::Speed:    return "speed";
        case EntityFilter::AngularVelocity: return "angularVelocity";
        case EntityFilter::Noise: return "noise";
    }
    return "any";
}

static SignalType parse_signal_type(const std::string& s) {
    if (s == "nearestDistance") return SignalType::NearestDistance;
    if (s == "sectorDensity") return SignalType::SectorDensity;
    return SignalType::NearestDistance;
}

static std::string signal_type_to_string(SignalType t) {
    switch (t) {
        case SignalType::NearestDistance: return "nearestDistance";
        case SignalType::SectorDensity:  return "sectorDensity";
    }
    return "nearestDistance";
}

static NodeType parse_node_type(const std::string& s) {
    if (s == "input")  return NodeType::Input;
    if (s == "output") return NodeType::Output;
    return NodeType::Hidden;
}

static std::string node_type_to_string(NodeType t) {
    switch (t) {
        case NodeType::Input:  return "input";
        case NodeType::Output: return "output";
        case NodeType::Hidden: return "hidden";
    }
    return "hidden";
}

static ActivationFn parse_activation_fn(const std::string& s) {
    if (s == "sigmoid") return ActivationFn::Sigmoid;
    if (s == "tanh")    return ActivationFn::Tanh;
    if (s == "relu")    return ActivationFn::ReLU;
    return ActivationFn::Linear;
}

static std::string activation_fn_to_string(ActivationFn fn) {
    switch (fn) {
        case ActivationFn::Sigmoid: return "sigmoid";
        case ActivationFn::Tanh:    return "tanh";
        case ActivationFn::ReLU:    return "relu";
        case ActivationFn::Linear:  return "linear";
    }
    return "linear";
}

static SensorChannel parse_sensor_channel(const std::string& s) {
    if (s == "food") return SensorChannel::Food;
    if (s == "same") return SensorChannel::Same;
    if (s == "opposite") return SensorChannel::Opposite;
    return SensorChannel::Food;
}

static std::string sensor_channel_to_string(SensorChannel ch) {
    switch (ch) {
        case SensorChannel::Food:     return "food";
        case SensorChannel::Same:     return "same";
        case SensorChannel::Opposite: return "opposite";
    }
    return "food";
}

int sensor_input_count(const BoidSpec& spec) {
    if (spec.compound_eyes.has_value()) return spec.compound_eyes->total_inputs();
    return static_cast<int>(spec.sensors.size());
}

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

    // Compound eyes take precedence over legacy sensors
    if (j.contains("compoundEyes")) {
        CompoundEyeConfig cfg;
        const auto& jce = j.at("compoundEyes");

        for (const auto& je : jce.at("eyes")) {
            EyeSpec eye;
            eye.id = je.at("id").get<int>();
            eye.center_angle = je.at("centerAngleDeg").get<float>() * DEG_TO_RAD;
            eye.arc_width = je.at("arcWidthDeg").get<float>() * DEG_TO_RAD;
            eye.max_range = je.at("maxRange").get<float>();
            cfg.eyes.push_back(eye);
        }

        for (const auto& jch : jce.at("channels")) {
            cfg.channels.push_back(parse_sensor_channel(jch.get<std::string>()));
        }

        cfg.has_speed_sensor = jce.value("speedSensor", true);
        cfg.has_angular_velocity_sensor = jce.value("angularVelocitySensor", false);
        cfg.has_noise_sensor = jce.value("noiseSensor", false);
        spec.compound_eyes = std::move(cfg);
    }
    // Legacy sensors — old specs without compound eyes still load fine
    else if (j.contains("sensors")) {
        for (const auto& js : j.at("sensors")) {
            SensorSpec ss;
            ss.id = js.at("id").get<int>();
            ss.center_angle = js.at("centerAngleDeg").get<float>() * DEG_TO_RAD;
            ss.arc_width = js.at("arcWidthDeg").get<float>() * DEG_TO_RAD;
            ss.max_range = js.at("maxRange").get<float>();
            ss.filter = parse_entity_filter(js.value("filter", "any"));
            ss.signal_type = parse_signal_type(js.value("signalType", "nearestDistance"));
            spec.sensors.push_back(ss);
        }
    }

    // Genome is optional — boids without a brain don't have one
    if (j.contains("genome")) {
        NeatGenome genome;
        const auto& jg = j.at("genome");

        for (const auto& jn : jg.at("nodes")) {
            NodeGene ng;
            ng.id = jn.at("id").get<int>();
            ng.type = parse_node_type(jn.at("type").get<std::string>());
            ng.activation = parse_activation_fn(jn.value("activation", "sigmoid"));
            ng.bias = jn.value("bias", 0.0f);
            genome.nodes.push_back(ng);
        }

        for (const auto& jc : jg.at("connections")) {
            ConnectionGene cg;
            cg.innovation = jc.at("innovation").get<int>();
            cg.source = jc.at("source").get<int>();
            cg.target = jc.at("target").get<int>();
            cg.weight = jc.at("weight").get<float>();
            cg.enabled = jc.value("enabled", true);
            cg.recurrent = jc.value("recurrent", false);
            genome.connections.push_back(cg);
        }

        spec.genome = std::move(genome);
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

    if (spec.compound_eyes.has_value()) {
        boid.sensors.emplace(*spec.compound_eyes);
        boid.sensor_outputs.resize(boid.sensors->input_count(), 0.0f);
    } else if (!spec.sensors.empty()) {
        boid.sensors.emplace(spec.sensors);
        boid.sensor_outputs.resize(boid.sensors->input_count(), 0.0f);
    }

    if (spec.genome.has_value()) {
        boid.brain = std::make_unique<NeatNetwork>(*spec.genome);
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

    if (spec.compound_eyes.has_value()) {
        json jce;
        jce["eyes"] = json::array();
        for (const auto& eye : spec.compound_eyes->eyes) {
            json je;
            je["id"] = eye.id;
            je["centerAngleDeg"] = eye.center_angle * RAD_TO_DEG;
            je["arcWidthDeg"] = eye.arc_width * RAD_TO_DEG;
            je["maxRange"] = eye.max_range;
            jce["eyes"].push_back(je);
        }

        jce["channels"] = json::array();
        for (auto ch : spec.compound_eyes->channels) {
            jce["channels"].push_back(sensor_channel_to_string(ch));
        }

        jce["speedSensor"] = spec.compound_eyes->has_speed_sensor;
        jce["angularVelocitySensor"] = spec.compound_eyes->has_angular_velocity_sensor;
        jce["noiseSensor"] = spec.compound_eyes->has_noise_sensor;
        j["compoundEyes"] = jce;
    } else if (!spec.sensors.empty()) {
        j["sensors"] = json::array();
        for (const auto& ss : spec.sensors) {
            json js;
            js["id"] = ss.id;
            js["centerAngleDeg"] = ss.center_angle * RAD_TO_DEG;
            js["arcWidthDeg"] = ss.arc_width * RAD_TO_DEG;
            js["maxRange"] = ss.max_range;
            js["filter"] = entity_filter_to_string(ss.filter);
            js["signalType"] = signal_type_to_string(ss.signal_type);
            j["sensors"].push_back(js);
        }
    }

    if (spec.genome.has_value()) {
        json jg;
        jg["nodes"] = json::array();
        for (const auto& ng : spec.genome->nodes) {
            json jn;
            jn["id"] = ng.id;
            jn["type"] = node_type_to_string(ng.type);
            jn["activation"] = activation_fn_to_string(ng.activation);
            jn["bias"] = ng.bias;
            jg["nodes"].push_back(jn);
        }

        jg["connections"] = json::array();
        for (const auto& cg : spec.genome->connections) {
            json jc;
            jc["innovation"] = cg.innovation;
            jc["source"] = cg.source;
            jc["target"] = cg.target;
            jc["weight"] = cg.weight;
            jc["enabled"] = cg.enabled;
            if (cg.recurrent) jc["recurrent"] = true;
            jg["connections"].push_back(jc);
        }

        j["genome"] = jg;
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + path);
    }
    file << j.dump(4) << std::endl;
}
