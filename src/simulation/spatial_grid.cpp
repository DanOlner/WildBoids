#include "simulation/spatial_grid.h"
#include <cmath>
#include <algorithm>

SpatialGrid::SpatialGrid(float world_w, float world_h, float cell_size, bool toroidal)
    : cell_size_(cell_size)
    , world_w_(world_w)
    , world_h_(world_h)
    , toroidal_(toroidal)
{
    cols_ = std::max(1, static_cast<int>(std::ceil(world_w / cell_size)));
    rows_ = std::max(1, static_cast<int>(std::ceil(world_h / cell_size)));
    cells_.resize(cols_ * rows_);
}

void SpatialGrid::clear() {
    for (auto& cell : cells_) {
        cell.clear();
    }
}

void SpatialGrid::insert(int boid_index, Vec2 position) {
    int col, row;
    col_row(position, col, row);
    cells_[cell_index(col, row)].push_back(boid_index);
}

void SpatialGrid::query(Vec2 pos, float radius, std::vector<int>& out_indices) const {
    // How many cells in each direction we need to check
    int cell_span = static_cast<int>(std::ceil(radius / cell_size_));

    int center_col, center_row;
    col_row(pos, center_col, center_row);

    for (int dr = -cell_span; dr <= cell_span; ++dr) {
        for (int dc = -cell_span; dc <= cell_span; ++dc) {
            int c = center_col + dc;
            int r = center_row + dr;

            if (toroidal_) {
                c = wrap_col(c);
                r = wrap_row(r);
            } else {
                // Non-toroidal: skip out-of-bounds cells
                if (c < 0 || c >= cols_ || r < 0 || r >= rows_) continue;
            }

            const auto& cell = cells_[cell_index(c, r)];
            for (int idx : cell) {
                out_indices.push_back(idx);
            }
        }
    }
}

int SpatialGrid::cell_index(int col, int row) const {
    return row * cols_ + col;
}

void SpatialGrid::col_row(Vec2 pos, int& col, int& row) const {
    col = std::clamp(static_cast<int>(pos.x / cell_size_), 0, cols_ - 1);
    row = std::clamp(static_cast<int>(pos.y / cell_size_), 0, rows_ - 1);
}

int SpatialGrid::wrap_col(int c) const {
    return ((c % cols_) + cols_) % cols_;
}

int SpatialGrid::wrap_row(int r) const {
    return ((r % rows_) + rows_) % rows_;
}
