#pragma once

#include "cuboid.h"
#include "data_clipper.h"

#include <calibration_data.h>
#include <calibration_store.h>

#include <QString>

#include <functional>
#include <vector>

using report_error = std::function<void(const QString &)>;

struct kitti_export_config {
    QString output_path;
    QString frame_id;
    const std::vector<cuboid> *cuboids = nullptr;
    const calibration_store *calibration = nullptr;
    const data_variants *point_cloud = nullptr;
    report_error reporter;
};

struct nuscenes_export_config {
    QString output_path;
    QString frame_id;
    const std::vector<cuboid> *cuboids = nullptr;
    const data_variants *point_cloud = nullptr;
    report_error reporter;
};

namespace annotation_exporter {

    bool export_kitti(const kitti_export_config &config);
    bool export_nuscenes(const nuscenes_export_config &config);

}
