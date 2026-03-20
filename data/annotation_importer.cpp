#include "annotation_importer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuaternion>
#include <QTextStream>
#include <QVector3D>

#include <cmath>

QVector3D rect_to_lidar_position(const QVector3D &rect_pos, const calibration_store *calibration) {
	const auto &kitti_calibration = calibration->get_kitti_calibration();
	const float *R_rect = kitti_calibration.rotation_rect_00;
	const float *R_velo = kitti_calibration.rotation_velo_to_cam;
	const float *T_velo = kitti_calibration.translation_velo_to_cam;

	const float cx = R_rect[0] * rect_pos.x() + R_rect[3] * rect_pos.y() + R_rect[6] * rect_pos.z();
	const float cy = R_rect[1] * rect_pos.x() + R_rect[4] * rect_pos.y() + R_rect[7] * rect_pos.z();
	const float cz = R_rect[2] * rect_pos.x() + R_rect[5] * rect_pos.y() + R_rect[8] * rect_pos.z();

	const float dx = cx - T_velo[0];
	const float dy = cy - T_velo[1];
	const float dz = cz - T_velo[2];

	const float lx = R_velo[0] * dx + R_velo[3] * dy + R_velo[6] * dz;
	const float ly = R_velo[1] * dx + R_velo[4] * dy + R_velo[7] * dz;
	const float lz = R_velo[2] * dx + R_velo[5] * dy + R_velo[8] * dz;

	return QVector3D(lx, ly, lz);
}

float kitti_rotation_y_to_lidar_yaw(float rotation_y, const calibration_store *calibration) {
	const float *R = calibration->get_kitti_calibration().rotation_velo_to_cam;
	const float frame_yaw_offset = std::atan2(R[1], R[0]);
	const float yaw_radius = -(rotation_y + frame_yaw_offset);
	return yaw_radius * (180.0f / static_cast<float>(M_PI));
}

std::vector<cuboid> annotation_importer::import_kitti(
	const QString &file_path,
	const calibration_store *calibration,
	const report_error &reporter) {

	std::vector<cuboid> results;

	if (!calibration || !calibration->is_loaded()) {
		if (reporter) {
			reporter(QStringLiteral("KITTI import requires loaded calibration data."));
		}
		return results;
	}

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return results;
	}

	QTextStream stream(&file);
	while (!stream.atEnd()) {
		const QString line = stream.readLine().trimmed();
		if (line.isEmpty()) {
			continue;
		}

		const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
		if (parts.size() < 15) {
			if (reporter) {
				reporter(QStringLiteral("Malformed KITTI label line (expected 15 fields, got %1).")
					.arg(parts.size()));
			}
			continue;
		}

		cuboid c;
		c.class_name = parts[0];
		c.truncation = parts[1].toFloat();
		c.occlusion = parts[2].toFloat();
		c.alpha = parts[3].toFloat();

		const float h = parts[8].toFloat();
		const float w = parts[9].toFloat();
		const float l = parts[10].toFloat();

		c.dimension = QVector3D(l, w, h);

		const float kitti_x = parts[11].toFloat();
		const float kitti_y = parts[12].toFloat();
		const float kitti_z = parts[13].toFloat();

		const QVector3D rect_pos(kitti_x, kitti_y - h * 0.5f, kitti_z);
		c.position = rect_to_lidar_position(rect_pos, calibration);

		const float rotation_y = parts[14].toFloat();
		const float yaw_degree = kitti_rotation_y_to_lidar_yaw(rotation_y, calibration);
		c.rotation = QQuaternion::fromEulerAngles(0.0f, 0.0f, yaw_degree);

		results.push_back(std::move(c));
	}

	return results;
}

std::vector<cuboid> annotation_importer::import_nuscenes(
	const QString &file_path,
	const report_error &reporter) {

	std::vector<cuboid> results;

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return results;
	}

	const QByteArray content = file.readAll();
	file.close();

	QJsonParseError parse_error;
	const QJsonDocument doc = QJsonDocument::fromJson(content, &parse_error);

	if (parse_error.error != QJsonParseError::NoError) {
		if (reporter) {
			reporter(QStringLiteral("Failed to parse nuScenes annotation file: %1")
				.arg(parse_error.errorString()));
		}
		return results;
	}

	if (!doc.isArray()) {
		if (reporter) {
			reporter(QStringLiteral("Expected JSON array in nuScenes annotation file."));
		}
		return results;
	}

	const QJsonArray annotations = doc.array();
	results.reserve(annotations.size());

	for (const auto &entry : annotations) {
		const QJsonObject object = entry.toObject();

		cuboid c;
		c.class_name = object[QStringLiteral("category_name")].toString();

		const QJsonArray translation = object[QStringLiteral("translation")].toArray();
		if (translation.size() == 3) {
			c.position = QVector3D(
				static_cast<float>(translation[0].toDouble()),
				static_cast<float>(translation[1].toDouble()),
				static_cast<float>(translation[2].toDouble()));
		}

		const QJsonArray size = object[QStringLiteral("size")].toArray();
		if (size.size() == 3) {
			c.dimension = QVector3D(
				static_cast<float>(size[0].toDouble()),
				static_cast<float>(size[1].toDouble()),
				static_cast<float>(size[2].toDouble()));
		}

		const QJsonArray rotation = object[QStringLiteral("rotation")].toArray();
		if (rotation.size() == 4) {
			c.rotation = QQuaternion(
				static_cast<float>(rotation[0].toDouble()),
				static_cast<float>(rotation[1].toDouble()),
				static_cast<float>(rotation[2].toDouble()),
				static_cast<float>(rotation[3].toDouble()));
		}

		results.push_back(std::move(c));
	}

	return results;
}
