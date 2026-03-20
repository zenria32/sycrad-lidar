#pragma once

#include "cuboid.h"
#include <calibration_store.h>

#include <QString>

#include <functional>
#include <vector>

using report_error = std::function<void(const QString &)>;

namespace annotation_importer {

    std::vector<cuboid> import_kitti(const QString &file_path,
        const calibration_store *calibration,
        const report_error &reporter = nullptr);

    std::vector<cuboid> import_nuscenes(const QString &file_path,
        const report_error &reporter = nullptr);

}
