#include "brain/speciation.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

float compatibility_distance(const NeatGenome& a, const NeatGenome& b,
                             const CompatibilityParams& params) {
    if (a.connections.empty() && b.connections.empty()) return 0.0f;

    // Index connections by innovation number
    std::unordered_map<int, const ConnectionGene*> a_conns, b_conns;
    int a_max_innov = 0, b_max_innov = 0;

    for (const auto& c : a.connections) {
        a_conns[c.innovation] = &c;
        a_max_innov = std::max(a_max_innov, c.innovation);
    }
    for (const auto& c : b.connections) {
        b_conns[c.innovation] = &c;
        b_max_innov = std::max(b_max_innov, c.innovation);
    }

    int excess = 0;
    int disjoint = 0;
    float weight_diff_sum = 0.0f;
    int matching = 0;

    // Scan all innovation numbers present in either genome
    int max_innov = std::max(a_max_innov, b_max_innov);
    int min_max_innov = std::min(a_max_innov, b_max_innov);

    for (const auto& [innov, ac] : a_conns) {
        auto it = b_conns.find(innov);
        if (it != b_conns.end()) {
            // Matching gene
            weight_diff_sum += std::abs(ac->weight - it->second->weight);
            ++matching;
        } else if (innov > min_max_innov) {
            ++excess;
        } else {
            ++disjoint;
        }
    }

    // Count b's genes not in a
    for (const auto& [innov, bc] : b_conns) {
        if (a_conns.find(innov) == a_conns.end()) {
            if (innov > min_max_innov) {
                ++excess;
            } else {
                ++disjoint;
            }
        }
    }

    float mean_weight_diff = (matching > 0) ? (weight_diff_sum / matching) : 0.0f;
    int N = std::max(static_cast<int>(std::max(a.connections.size(), b.connections.size())),
                     params.normalise_threshold);

    return (params.c1 * excess / N) + (params.c2 * disjoint / N) + params.c3 * mean_weight_diff;
}

void assign_species(std::vector<Species>& species,
                    const std::vector<NeatGenome>& genomes,
                    const CompatibilityParams& params,
                    float threshold,
                    int& next_species_id) {
    // Clear member lists
    for (auto& s : species) {
        s.members.clear();
    }

    // Assign each genome to the first compatible species
    for (int i = 0; i < static_cast<int>(genomes.size()); ++i) {
        bool placed = false;
        for (auto& s : species) {
            float dist = compatibility_distance(genomes[i], s.representative, params);
            if (dist < threshold) {
                s.members.push_back(i);
                placed = true;
                break;
            }
        }

        if (!placed) {
            // Create new species with this genome as representative
            Species new_species;
            new_species.id = next_species_id++;
            new_species.representative = genomes[i];
            new_species.members.push_back(i);
            species.push_back(std::move(new_species));
        }
    }

    // Remove empty species
    species.erase(
        std::remove_if(species.begin(), species.end(),
                       [](const Species& s) { return s.members.empty(); }),
        species.end());

    // Update representatives: pick a random member from each species
    // (for simplicity, pick the first member)
    for (auto& s : species) {
        if (!s.members.empty()) {
            s.representative = genomes[s.members[0]];
        }
    }
}
