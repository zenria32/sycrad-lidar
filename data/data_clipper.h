#pragma once

#include "data_loader.h"

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <vector>

struct cuboid;

struct clip_result {
    std::vector<float> vertices;
    std::size_t point_count = 0;
    quint32 stride = 0;
    float min_intensity = 0.0f;
    float max_intensity = 1.0f;
};

clip_result clip_data(const data_variants &data, const QVector3D &aabb_min, const QVector3D &aabb_max);

QVector3D compute_clip_min(const cuboid &c, float margin_factor = 2.0f);
QVector3D compute_clip_max(const cuboid &c, float margin_factor = 2.0f);
