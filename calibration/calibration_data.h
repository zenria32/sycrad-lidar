#pragma once

#include <QQuaternion>
#include <QVector3D>
#include <QString>

#include <array>
#include <map>
#include <string>

struct kitti_calibration {
    float translation_velo_to_cam[3] = {};
    float rotation_velo_to_cam[9] = {};
    float rotation_rect_00[9] = {};
    float projection_rect_00[12] = {};
    float projection_rect_02[12] = {};
};

struct nuscenes_sensor_entry {
    std::string token;
    std::string sensor_token;
    std::string channel;
    QVector3D translation;
    QQuaternion rotation;
    float intrinsic[9] = {};
    bool has_intrinsic = false;
};

struct nuscenes_calibration {
    std::map<std::string, nuscenes_sensor_entry> by_channel;
    std::map<std::string, nuscenes_sensor_entry> by_token;
};