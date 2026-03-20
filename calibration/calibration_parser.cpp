#include "calibration_parser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <filesystem>
#include <sstream>

report_error calibration_parser::reporter{};

bool calibration_parser::read_kitti_matrix(const std::string &file_path, const std::string &key, float *output, int count) {
	QFile file(QString::fromStdString(file_path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		report(QStringLiteral("Unable to open KITTI calibration file '%1'. "
			"Verify the file exists and has appropriate read permissions.")
			.arg(QString::fromStdString(file_path)));
		return false;
	}

	QTextStream stream(&file);
	while (!stream.atEnd()) {
		QString line = stream.readLine().trimmed();
		if (!line.startsWith(QString::fromStdString(key + ":"))) {
			continue;
		}

		QString values = line.mid(static_cast<int>(key.size()) + 1).trimmed();
		QStringList parts = values.split(' ', Qt::SkipEmptyParts);
		if (parts.size() < count) {
			report(QStringLiteral("Key '%1' in '%2' contains %3 values, "
				"expected at least %4. The calibration file may be corrupted or incomplete.")
				.arg(QString::fromStdString(key), QString::fromStdString(file_path))
				.arg(parts.size())
				.arg(count));
			return false;
		}

		for (int i = 0; i < count; ++i) {
			bool ok = false;
			output[i] = parts[i].toFloat(&ok);
			if (!ok) {
				report(QStringLiteral("Non-numeric value at index %1 for key '%2' in '%3'. "
					"Expected a floating-point number but found '%4'.")
					.arg(i)
					.arg(QString::fromStdString(key), QString::fromStdString(file_path), parts[i]));
				return false;
			}
		}
		return true;
	}

	report(QStringLiteral("Key '%1' not found in '%2'. "
		"The calibration file may be missing required entries.")
		.arg(QString::fromStdString(key), QString::fromStdString(file_path)));
	return false;
}

bool calibration_parser::parse_kitti_metadata(const std::string &calibration_dir, kitti_calibration &output) {
	namespace fs = std::filesystem;

	std::string velo_to_cam = (fs::path(calibration_dir) / "calib_velo_to_cam.txt").string();
	std::string cam_to_cam = (fs::path(calibration_dir) / "calib_cam_to_cam.txt").string();

	if (!fs::exists(velo_to_cam) || !fs::exists(cam_to_cam)) {
		report(QStringLiteral("Required KITTI calibration files not found in '%1'. "
			"Expected 'calib_velo_to_cam.txt' and 'calib_cam_to_cam.txt' in the calibration directory.")
			.arg(QString::fromStdString(calibration_dir)));
		return false;
	}

	if (!read_kitti_matrix(velo_to_cam, "T", output.translation_velo_to_cam, 3)) {
		return false;
	}
	if (!read_kitti_matrix(velo_to_cam, "R", output.rotation_velo_to_cam, 9)) {
		return false;
	}
	if (!read_kitti_matrix(cam_to_cam, "R_rect_00", output.rotation_rect_00, 9)) {
		return false;
	}
	if (!read_kitti_matrix(cam_to_cam, "P_rect_00", output.projection_rect_00, 12)) {
		return false;
	}
	if (!read_kitti_matrix(cam_to_cam, "P_rect_02", output.projection_rect_02, 12)) {
		return false;
	}

	return true;
}

bool calibration_parser::parse_sensor_json(const std::string &file_path, std::map<std::string, std::string> &token_to_channel) {
	QFile file(QString::fromStdString(file_path));
	if (!file.open(QIODevice::ReadOnly)) {
		report(QStringLiteral("Unable to open nuScenes sensor file '%1'. "
			"Verify the file exists and has appropriate read permissions.")
			.arg(QString::fromStdString(file_path)));
		return false;
	}

	QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isArray()) {
		report(QStringLiteral("Sensor file '%1' does not contain a valid JSON array. "
			"The file may be corrupted or in an unsupported format.")
			.arg(QString::fromStdString(file_path)));
		return false;
	}

	for (const auto &value : document.array()) {
		QJsonObject obj = value.toObject();
		std::string token = obj["token"].toString().toStdString();
		std::string channel = obj["channel"].toString().toStdString();
		token_to_channel[token] = channel;
	}

	return true;
}

bool calibration_parser::parse_calibrated_sensor_json(const std::string &file_path, const std::map<std::string, std::string> &token_to_channel, nuscenes_calibration &output) {
	QFile file(QString::fromStdString(file_path));
	if (!file.open(QIODevice::ReadOnly)) {
		report(QStringLiteral("Unable to open nuScenes calibrated sensor file '%1'. "
			"Verify the file exists and has appropriate read permissions.")
			.arg(QString::fromStdString(file_path)));
		return false;
	}

	QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isArray()) {
		report(QStringLiteral("Calibrated sensor file '%1' does not contain a valid JSON array. "
			"The file may be corrupted or in an unsupported format.")
			.arg(QString::fromStdString(file_path)));
		return false;
	}

	for (const auto &value : document.array()) {
		QJsonObject obj = value.toObject();
		nuscenes_sensor_entry entry;

		entry.token = obj["token"].toString().toStdString();
		entry.sensor_token = obj["sensor_token"].toString().toStdString();

		auto iterator = token_to_channel.find(entry.sensor_token);
		if (iterator != token_to_channel.end()) {
			entry.channel = iterator->second;
		}

		QJsonArray translation = obj["translation"].toArray();
		if (translation.size() == 3) {
			entry.translation = QVector3D(
				static_cast<float>(translation[0].toDouble()),
				static_cast<float>(translation[1].toDouble()),
				static_cast<float>(translation[2].toDouble()));
		}

		QJsonArray rotation = obj["rotation"].toArray();
		if (rotation.size() == 4) {
			entry.rotation = QQuaternion(
				static_cast<float>(rotation[0].toDouble()),
				static_cast<float>(rotation[1].toDouble()),
				static_cast<float>(rotation[2].toDouble()),
				static_cast<float>(rotation[3].toDouble()));
		}

		QJsonArray intrinsic = obj["camera_intrinsic"].toArray();
		if (intrinsic.size() == 3) {
			int index = 0;
			for (int row = 0; row < 3; ++row) {
				QJsonArray row_arr = intrinsic[row].toArray();
				if (row_arr.size() == 3) {
					for (int col = 0; col < 3; ++col) {
						entry.intrinsic[index++] = static_cast<float>(row_arr[col].toDouble());
					}
					entry.has_intrinsic = true;
				}
			}
		}

		if (!entry.channel.empty()) {
			output.by_channel[entry.channel] = entry;
		}
		output.by_token[entry.token] = entry;
	}

	return true;
}

bool calibration_parser::parse_nuscenes_metadata(const std::string &calibration_dir, nuscenes_calibration &output) {
	namespace fs = std::filesystem;

	std::string sensor_path = (fs::path(calibration_dir) / "sensor.json").string();
	std::string calibrated_sensor_path = (fs::path(calibration_dir) / "calibrated_sensor.json").string();

	if (!fs::exists(sensor_path) || !fs::exists(calibrated_sensor_path)) {
		report(QStringLiteral("Required nuScenes calibration files not found in '%1'. "
			"Expected 'sensor.json' and 'calibrated_sensor.json' in the calibration directory.")
			.arg(QString::fromStdString(calibration_dir)));
		return false;
	}

	std::map<std::string, std::string> token_to_channel;
	if (!parse_sensor_json(sensor_path, token_to_channel)) {
		return false;
	}

	if (!parse_calibrated_sensor_json(calibrated_sensor_path, token_to_channel, output)) {
		return false;
	}

	return true;
}

void calibration_parser::report(const QString &message) {
	if (reporter) {
		reporter(message);
	}
}
