#pragma once

#include <QQuaternion>
#include <QVector3D>
#include <QString>

#include <cstdint>

struct cuboid {
    uint32_t id = 0;
    QString class_name;

    QVector3D position {0.0f, 0.0f, 0.0f};
    QQuaternion rotation;
    QVector3D dimension {1.0f, 1.0f, 1.0f};

    float truncation = 0.0f;
    float occlusion = 0.0f;
    float alpha = 0.0f;

    QVector3D half_extent() const {
        return dimension * 0.5f;
    }

    QVector3D min_corner() const {
        return position - half_extent();
    }

    QVector3D max_corner() const {
        return position + half_extent();
    }

};