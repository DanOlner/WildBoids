#pragma once

#include "simulation/vec2.h"
#include <vector>

class SpatialGrid {
public:
    SpatialGrid(float world_w, float world_h, float cell_size, bool toroidal);

    void clear();
    void insert(int boid_index, Vec2 position);

    // Appends indices of boids in cells overlapping a circle at pos with given radius.
    // Callers must do fine-grained distance checks on the results.
    void query(Vec2 pos, float radius, std::vector<int>& out_indices) const;

    float cell_size() const { return cell_size_; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }

private:
    int cols_;
    int rows_;
    float cell_size_;
    float world_w_;
    float world_h_;
    bool toroidal_;
    std::vector<std::vector<int>> cells_;

    int cell_index(int col, int row) const;
    void col_row(Vec2 pos, int& col, int& row) const;
    int wrap_col(int c) const;
    int wrap_row(int r) const;
};
