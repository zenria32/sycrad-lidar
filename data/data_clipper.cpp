#include "data_clipper.h"
#include "cuboid.h"

#include <algorithm>
#include <cmath>

clip_result clip_data(const data_variants &data, const QVector3D &aabb_min, const QVector3D &aabb_max) {

    clip_result result;

    std::visit([&](const auto &d) {
            using T = std::decay_t<decltype(d)>;

            if constexpr (!std::is_same_v<T, std::monostate>) {
                if (!d || !d->is_valid() || d->point_count == 0) {
                    return;
                }

                constexpr std::size_t fpv = T::element_type::floats_per_vertex();
                result.stride = static_cast<quint32>(fpv * sizeof(float));
                result.min_intensity = d->min_intensity;
                result.max_intensity = d->max_intensity;

                const auto *data_ptr = static_cast<const float *>(d->access());
                const std::size_t n = d->point_count;

                const float min_x = aabb_min.x();
                const float min_y = aabb_min.y();
                const float min_z = aabb_min.z();
                const float max_x = aabb_max.x();
                const float max_y = aabb_max.y();
                const float max_z = aabb_max.z();

                result.vertices.reserve(n / 10 * fpv);

                for (std::size_t i = 0; i < n; ++i) {
                    const float *ptr = data_ptr + i * fpv;
                    const float x = ptr[0];
                    const float y = ptr[1];
                    const float z = ptr[2];

                    if (x >= min_x && x <= max_x &&
                        y >= min_y && y <= max_y &&
                        z >= min_z && z <= max_z) {

                        result.vertices.insert(result.vertices.end(), ptr, ptr + fpv);
                        ++result.point_count;
                    }
                }
            }
        },
        data);

    return result;
}

QVector3D compute_clip_min(const cuboid &c, float margin_factor) {
    const QVector3D half = c.dimension * 0.5f * margin_factor;
    return c.position - half;
}

QVector3D compute_clip_max(const cuboid &c, float margin_factor) {
    const QVector3D half = c.dimension * 0.5f * margin_factor;
    return c.position + half;
}
