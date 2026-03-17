#pragma once

#include "calibration_data.h"

#include <QString>

#include <functional>
#include <string>

using report_error = std::function<void(const QString &message)>;

class calibration_parser {
    public:
    static void set_reporter(report_error error_handler) { reporter = std::move(error_handler); }

    static bool parse_kitti_metadata(const std::string &calibration_dir, kitti_calibration &output);
    static bool parse_nuscenes_metadata(const std::string &calibration_dir, nuscenes_calibration &output);

    private:
    static void report(const QString &message);

    static bool read_kitti_matrix(const std::string &file_path, const std::string &key, float *output, int count);
    static bool parse_sensor_json(const std::string &file_path, std::map<std::string, std::string> &token_to_channel);
    static bool parse_calibrated_sensor_json(const std::string &file_path, const std::map<std::string, std::string> &token_to_channel, nuscenes_calibration &output);

    static report_error reporter;
};