#include "brain/population.h"
#include "brain/mutation.h"
#include "brain/crossover.h"
#include <algorithm>
#include <numeric>
#include <cassert>

Population::Population(const NeatGenome& seed, const PopulationParams& params,
                       std::mt19937& rng)
    : params_(params), rng_(rng), tracker_(1) {
    // Find the max innovation in the seed genome so the tracker starts after it
    int max_innov = 0;
    for (const auto& c : seed.connections) {
        max_innov = std::max(max_innov, c.innovation);
    }
    tracker_ = InnovationTracker(max_innov + 1);

    // Create initial population by cloning seed and mutating weights
    genomes_.reserve(params_.population_size);
    for (int i = 0; i < params_.population_size; ++i) {
        NeatGenome g = seed;
        if (i > 0) {
            mutate_weights(g, rng_, params_.weight_perturb_prob,
                           params_.weight_sigma, params_.weight_replace_prob);
        }
        genomes_.push_back(std::move(g));
    }

    fitness_.resize(params_.population_size, 0.0f);

    // Initial speciation
    assign_species(species_, genomes_, params_.compat, params_.compat_threshold,
                   next_species_id_);
}

void Population::enable_morphology(const MorphologyEvolutionConfig& config) {
    morphology_config_ = config;
    auto default_morpho = create_default_morphology(config);

    morphologies_.clear();
    morphologies_.reserve(params_.population_size);
    for (int i = 0; i < params_.population_size; ++i) {
        MorphologyGenome m = default_morpho;
        if (i > 0) {
            mutate_morphology(m, config, rng_);
        }
        morphologies_.push_back(std::move(m));
    }
}

void Population::enable_morphology(const MorphologyEvolutionConfig& config,
                                   const MorphologyGenome& seed) {
    morphology_config_ = config;

    morphologies_.clear();
    morphologies_.reserve(params_.population_size);
    for (int i = 0; i < params_.population_size; ++i) {
        MorphologyGenome m = seed;
        if (i > 0) {
            mutate_morphology(m, config, rng_);
        }
        morphologies_.push_back(std::move(m));
    }
}

void Population::evaluate(
    std::function<float(int genome_index, const NeatGenome& genome)> fitness_fn) {
    for (int i = 0; i < static_cast<int>(genomes_.size()); ++i) {
        fitness_[i] = fitness_fn(i, genomes_[i]);
    }

    // Update species best fitness and stagnation
    for (auto& s : species_) {
        float species_best = 0.0f;
        for (int idx : s.members) {
            species_best = std::max(species_best, fitness_[idx]);
        }
        if (species_best > s.best_fitness) {
            s.best_fitness = species_best;
            s.stagnation_count = 0;
        } else {
            ++s.stagnation_count;
        }
    }
}

int Population::tournament_select(const Species& species, int k) {
    assert(!species.members.empty());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(species.members.size()) - 1);

    int best = species.members[dist(rng_)];
    for (int i = 1; i < k; ++i) {
        int candidate = species.members[dist(rng_)];
        if (fitness_[candidate] > fitness_[best]) {
            best = candidate;
        }
    }
    return best;
}

NeatGenome Population::reproduce_from_species(const Species& species) {
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    NeatGenome child;
    int p1_idx = -1, p2_idx = -1;

    if (species.members.size() == 1 || coin(rng_) >= params_.crossover_prob) {
        // Asexual: clone and mutate
        p1_idx = tournament_select(species);
        child = genomes_[p1_idx];
    } else {
        // Sexual: crossover two parents
        p1_idx = tournament_select(species);
        p2_idx = tournament_select(species);
        // Ensure different parents when possible
        if (species.members.size() > 1) {
            int attempts = 0;
            while (p2_idx == p1_idx && attempts < 5) {
                p2_idx = tournament_select(species);
                ++attempts;
            }
        }
        // Fitter parent goes first
        if (fitness_[p1_idx] >= fitness_[p2_idx]) {
            child = crossover(genomes_[p1_idx], genomes_[p2_idx], rng_);
        } else {
            child = crossover(genomes_[p2_idx], genomes_[p1_idx], rng_);
            std::swap(p1_idx, p2_idx);  // p1 is now the fitter parent
        }
    }

    mutate(child);

    // Morphology: use same parent selection for body genome
    if (morphology_config_.has_value()) {
        MorphologyGenome child_morpho;
        if (p2_idx >= 0) {
            // Sexual: crossover morphology with same parent order
            child_morpho = crossover_morphology(
                morphologies_[p1_idx], morphologies_[p2_idx], rng_);
        } else {
            // Asexual: clone
            child_morpho = morphologies_[p1_idx];
        }
        mutate_morphology(child_morpho, *morphology_config_, rng_);
        pending_morphology_ = std::move(child_morpho);
    }

    return child;
}

void Population::mutate(NeatGenome& genome) {
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    if (coin(rng_) < params_.weight_mutate_prob) {
        mutate_weights(genome, rng_, params_.weight_perturb_prob,
                       params_.weight_sigma, params_.weight_replace_prob);
    }
    if (coin(rng_) < params_.add_connection_prob) {
        mutate_add_connection(genome, rng_, tracker_, 20, params_.allow_recurrent);
    }
    if (coin(rng_) < params_.add_node_prob) {
        mutate_add_node(genome, rng_, tracker_);
    }
    if (coin(rng_) < params_.toggle_connection_prob) {
        mutate_toggle_connection(genome, rng_);
    }
    if (coin(rng_) < params_.delete_connection_prob) {
        mutate_delete_connection(genome, rng_);
    }
}

void Population::advance_generation() {
    // Remove stagnant species (but keep at least one)
    if (species_.size() > 1) {
        species_.erase(
            std::remove_if(species_.begin(), species_.end(),
                           [&](const Species& s) {
                               return s.stagnation_count >= params_.max_stagnation;
                           }),
            species_.end());
        // Ensure we still have at least one species
        if (species_.empty()) {
            // This shouldn't happen, but safety net
            Species fallback;
            fallback.id = next_species_id_++;
            fallback.representative = genomes_[0];
            fallback.members = {0};
            species_.push_back(std::move(fallback));
        }
    }

    // Compute adjusted fitness (fitness sharing within species)
    std::vector<float> adjusted_fitness(genomes_.size(), 0.0f);
    float total_adjusted = 0.0f;

    for (const auto& s : species_) {
        float species_size = static_cast<float>(s.members.size());
        for (int idx : s.members) {
            adjusted_fitness[idx] = fitness_[idx] / species_size;
            total_adjusted += adjusted_fitness[idx];
        }
    }

    // Compute offspring count per species (proportional to total adjusted fitness)
    std::vector<int> offspring_counts(species_.size(), 0);
    int total_offspring = 0;

    if (total_adjusted > 0.0f) {
        for (size_t si = 0; si < species_.size(); ++si) {
            float species_adj_sum = 0.0f;
            for (int idx : species_[si].members) {
                species_adj_sum += adjusted_fitness[idx];
            }
            offspring_counts[si] = std::max(1,
                static_cast<int>(std::round(
                    species_adj_sum / total_adjusted * params_.population_size)));
            total_offspring += offspring_counts[si];
        }
    } else {
        // All fitness is zero — distribute evenly
        int per_species = params_.population_size / static_cast<int>(species_.size());
        for (size_t si = 0; si < species_.size(); ++si) {
            offspring_counts[si] = per_species;
            total_offspring += per_species;
        }
    }

    // Adjust to hit exact population size
    while (total_offspring > params_.population_size) {
        // Remove from largest species
        auto max_it = std::max_element(offspring_counts.begin(), offspring_counts.end());
        if (*max_it > 1) {
            --(*max_it);
            --total_offspring;
        } else {
            break;
        }
    }
    while (total_offspring < params_.population_size) {
        // Add to largest species
        auto max_it = std::max_element(offspring_counts.begin(), offspring_counts.end());
        ++(*max_it);
        ++total_offspring;
    }

    // Sort each species' members by fitness (descending) for elitism and selection
    for (auto& s : species_) {
        std::sort(s.members.begin(), s.members.end(),
                  [&](int a, int b) { return fitness_[a] > fitness_[b]; });
    }

    // Produce next generation
    std::vector<NeatGenome> new_genomes;
    new_genomes.reserve(params_.population_size);
    std::vector<MorphologyGenome> new_morphologies;
    if (has_morphology()) {
        new_morphologies.reserve(params_.population_size);
    }

    for (size_t si = 0; si < species_.size(); ++si) {
        auto& s = species_[si];
        int count = offspring_counts[si];

        // Elitism: copy top genomes unchanged
        int elites = std::min(params_.elitism, static_cast<int>(s.members.size()));
        elites = std::min(elites, count);
        for (int e = 0; e < elites; ++e) {
            new_genomes.push_back(genomes_[s.members[e]]);
            if (has_morphology()) {
                new_morphologies.push_back(morphologies_[s.members[e]]);
            }
        }

        // Trim species to survival_rate for breeding pool
        int survivors = std::max(1, static_cast<int>(
            std::ceil(s.members.size() * params_.survival_rate)));
        std::vector<int> original_members = s.members;
        s.members.resize(std::min(survivors, static_cast<int>(s.members.size())));

        // Fill remaining slots with offspring
        for (int i = elites; i < count; ++i) {
            new_genomes.push_back(reproduce_from_species(s));
            if (has_morphology()) {
                new_morphologies.push_back(std::move(pending_morphology_));
            }
        }

        // Restore full member list (needed for species representative selection)
        s.members = original_members;
    }

    // Replace population
    genomes_ = std::move(new_genomes);
    fitness_.assign(genomes_.size(), 0.0f);
    if (has_morphology()) {
        morphologies_ = std::move(new_morphologies);
    }

    // New generation for innovation tracker
    tracker_.new_generation();
    ++generation_;

    // Re-speciate
    assign_species(species_, genomes_, params_.compat, params_.compat_threshold,
                   next_species_id_);
}

int Population::best_index() const {
    auto it = std::max_element(fitness_.begin(), fitness_.end());
    return static_cast<int>(std::distance(fitness_.begin(), it));
}

const NeatGenome& Population::best_genome() const {
    return genomes_[best_index()];
}

float Population::best_fitness() const {
    return *std::max_element(fitness_.begin(), fitness_.end());
}
