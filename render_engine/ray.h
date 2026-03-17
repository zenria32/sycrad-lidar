#pragma once

#include <QQuaternion>
#include <QVector3D>

struct ray {
    QVector3D origin;
    QVector3D direction;

    QVector3D at(float distance) const { return origin + direction * distance; }

    bool intersect_aabb(const QVector3D &aabb_min, const QVector3D &aabb_max, float &entry_point, float &exit_point) const {
        const float inverse_x = (qFuzzyIsNull(direction.x())) ? 1e30f : 1.0f / direction.x();
        const float inverse_y = (qFuzzyIsNull(direction.y())) ? 1e30f : 1.0f / direction.y();
        const float inverse_z = (qFuzzyIsNull(direction.z())) ? 1e30f : 1.0f / direction.z();

        const float entry_x = (aabb_min.x() - origin.x()) * inverse_x;
        const float exit_x = (aabb_max.x() - origin.x()) * inverse_x;

        const float entry_y = (aabb_min.y() - origin.y()) * inverse_y;
        const float exit_y = (aabb_max.y() - origin.y()) * inverse_y;

        const float entry_z = (aabb_min.z() - origin.z()) * inverse_z;
        const float exit_z = (aabb_max.z() - origin.z()) * inverse_z;

        entry_point = std::max({std::min(entry_x, exit_x), std::min(entry_y, exit_y), std::min(entry_z, exit_z)});
        exit_point = std::min({std::max(entry_x, exit_x), std::max(entry_y, exit_y), std::max(entry_z, exit_z)});

        return exit_point >= entry_point && exit_point >= 0.0f;
    }

    bool intersect_oriented(const QVector3D &center, const QQuaternion &rotation, const QVector3D &half_extent, float &collision) const {
        const QQuaternion inverse_rotation = rotation.conjugated();
        const QVector3D local_origin = inverse_rotation.rotatedVector(origin - center);
        const QVector3D local_direction = inverse_rotation.rotatedVector(direction);

        const ray local_ray{local_origin, local_direction};
        float entry_point, exit_point;

        if (!local_ray.intersect_aabb(-half_extent, half_extent, entry_point, exit_point)) {
            return false;
        }

        collision = (entry_point >= 0.0f) ? entry_point : exit_point;
        return collision >= 0.0f;
    }

};