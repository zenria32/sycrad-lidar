#pragma once

#include "data.h"
#include "calibration_data.h"

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <string>

using report_error = std::function<void(const QString &message)>;

class calibration_store {
    public:
    calibration_store() = default;

    void set_reporter(report_error error_handler) { reporter = std::move(error_handler); }

    bool load_kitti_metadata(const std::string &calibration_path);
    bool load_nuscenes_metadata(const std::string &calibration_path);

    bool is_loaded() const { return loaded; }

    float get_lidar_ground_z(int data_format) const;
    void set_kitti_ground_z(float z) { kitti_ground_z = z; }

    QVector3D lidar_to_cam_position(const QVector3D &p) const;
    QVector3D lidar_to_rect_position(const QVector3D &p) const;
    float lidar_yaw_to_kitti_rotation_y(float yaw_degrees) const;

    const kitti_calibration &get_kitti_calibration() const { return kitti; }

    const nuscenes_sensor_entry *get_sensor_entry(const std::string &channel) const;
    const nuscenes_calibration &get_nuscenes_calibration() const { return nuscenes; }

    private:
    void report(const QString &message) const;

    bool loaded = false;

    kitti_calibration kitti;
    nuscenes_calibration nuscenes;

    float kitti_ground_z = 0.0f;
    report_error reporter;
};
