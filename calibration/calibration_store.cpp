#include "calibration_store.h"
#include "calibration_parser.h"

#include <cmath>

bool calibration_store::load_kitti_metadata(const std::string &calibration_path) {
	if (calibration_path.empty()) {
		report(QStringLiteral("KITTI calibration path is empty. "
			"Specify a valid calibration directory in the project settings."));
		return false;
	}

	kitti = kitti_calibration{};
	if (!calibration_parser::parse_kitti_metadata(calibration_path, kitti)) {
		report(QString("Failed to load KITTI calibration data from '%1'. "
			"Check the calibration directory for missing or malformed files.")
			.arg(QString::fromStdString(calibration_path)));
		return false;
	}

	loaded = true;
	return true;
}

bool calibration_store::load_nuscenes_metadata(const std::string &calibration_path) {
	if (calibration_path.empty()) {
		report(QStringLiteral("NuScenes calibration path is empty. "
			"Specify a valid calibration directory in the project settings."));
		return false;
	}

	nuscenes = nuscenes_calibration{};
	if (!calibration_parser::parse_nuscenes_metadata(calibration_path, nuscenes)) {
		report(QString("Failed to load nuScenes calibration data from '%1'. "
			"Check the calibration directory for missing or malformed files.")
			.arg(QString::fromStdString(calibration_path)));
		return false;
	}

	loaded = true;
	return true;
}

float calibration_store::get_nuscenes_ground_z() const {
	auto iterator = nuscenes.by_channel.find("LIDAR_TOP");
	if (iterator != nuscenes.by_channel.end()) {
		return -iterator->second.translation.z();
	}
	return 0.0f;
}

const nuscenes_sensor_entry *calibration_store::get_sensor_entry(const std::string &channel) const {
	auto iterator = nuscenes.by_channel.find(channel);
	if (iterator != nuscenes.by_channel.end()) {
		return &iterator->second;
	}
	return nullptr;
}

QVector3D calibration_store::lidar_to_cam_position(const QVector3D &p) const {
	const float *T = kitti.translation_velo_to_cam;
	const float *R = kitti.rotation_velo_to_cam;

	float cx = R[0] * p.x() + R[1] * p.y() + R[2] * p.z() + T[0];
	float cy = R[3] * p.x() + R[4] * p.y() + R[5] * p.z() + T[1];
	float cz = R[6] * p.x() + R[7] * p.y() + R[8] * p.z() + T[2];

	return QVector3D(cx, cy, cz);
}

QVector3D calibration_store::lidar_to_rect_position(const QVector3D &p) const {
	QVector3D cam = lidar_to_cam_position(p);
	const float *R = kitti.rotation_rect_00;

	float rx = R[0] * cam.x() + R[1] * cam.y() + R[2] * cam.z();
	float ry = R[3] * cam.x() + R[4] * cam.y() + R[5] * cam.z();
	float rz = R[6] * cam.x() + R[7] * cam.y() + R[8] * cam.z();

	return QVector3D(rx, ry, rz);
}

float calibration_store::lidar_yaw_to_kitti_rotation_y(float yaw_degrees) const {
	const float *R = kitti.rotation_velo_to_cam;
	float frame_yaw_offset = std::atan2(R[1], R[0]);
	float yaw_radius = yaw_degrees * (static_cast<float>(M_PI) / 180.0f);
	return -(yaw_radius + frame_yaw_offset);
}

void calibration_store::report(const QString &message) const {
	if (reporter) {
		reporter(message);
	}
}
