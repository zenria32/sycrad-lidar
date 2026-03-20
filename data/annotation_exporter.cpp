#include "annotation_exporter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUuid>

#include <array>
#include <cmath>

struct bounding_box_2d {
	float left = -1.0f;
	float top = -1.0f;
	float right = -1.0f;
	float bottom = -1.0f;
	bool valid = false;
};

std::array<QVector3D, 8> compute_corners(const cuboid &c) {
	const QVector3D h = c.dimension * 0.5f;
	const std::array<QVector3D, 8> local = {{
		{ -h.x(), -h.y(), -h.z() },
		{ h.x(), -h.y(), -h.z() },
		{ h.x(), h.y(), -h.z() },
		{ -h.x(), h.y(), -h.z() },
		{ -h.x(), -h.y(), h.z() },
		{ h.x(), -h.y(), h.z() },
		{ h.x(), h.y(), h.z() },
		{ -h.x(), h.y(), h.z() },
	}};

	std::array<QVector3D, 8> world{};
	for (int i = 0; i < 8; ++i) {
		world[i] = c.position + c.rotation.rotatedVector(local[i]);
	}
	return world;
}

bounding_box_2d project_to_2d(const std::array<QVector3D, 8> &corners_rect, const float *P_rect_02) {

	float min_u = std::numeric_limits<float>::max();
	float min_v = std::numeric_limits<float>::max();
	float max_u = std::numeric_limits<float>::lowest();
	float max_v = std::numeric_limits<float>::lowest();
	int valid_count = 0;

	for (const auto &corner : corners_rect) {
		if (corner.z() <= 0.0f) {
			continue;
		}

		const float u = P_rect_02[0] * corner.x() + P_rect_02[1] * corner.y() + P_rect_02[2] * corner.z() + P_rect_02[3];
		const float v = P_rect_02[4] * corner.x() + P_rect_02[5] * corner.y() + P_rect_02[6] * corner.z() + P_rect_02[7];
		const float w = P_rect_02[8] * corner.x() + P_rect_02[9] * corner.y() + P_rect_02[10] * corner.z() + P_rect_02[11];

		if (std::abs(w) < 1e-6f) {
			continue;
		}

		const float cx = u / w;
		const float cy = v / w;

		min_u = std::min(min_u, cx);
		max_u = std::max(max_u, cx);
		min_v = std::min(min_v, cy);
		max_v = std::max(max_v, cy);
		++valid_count;
	}

	if (valid_count < 2) {
		return {};
	}

	min_u = std::max(min_u, 0.0f);
	min_v = std::max(min_v, 0.0f);

	return { min_u, min_v, max_u, max_v, true };
}

bool atomic_write(const QString &temp, const QByteArray &content, const report_error &reporter) {
	const QString temp_file = temp + QStringLiteral(".tmp");

	QFile file(temp_file);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (reporter) {
			reporter(QStringLiteral("Unable to write temporary file: %1").arg(temp_file));
		}
		return false;
	}
	file.write(content);
	file.close();

	if (QFile::exists(temp)) {
		QFile::remove(temp);
	}
	if (!QFile::rename(temp_file, temp)) {
		if (reporter) {
			reporter(QStringLiteral("Failed to finalize file: %1").arg(temp));
		}
		QFile::remove(temp_file);
		return false;
	}
	return true;
}

std::size_t count_lidar_points(const cuboid &c, const data_variants *point_cloud) {
	if (!point_cloud) {
		return 0;
	}

	const QVector3D aabb_min = compute_clip_min(c, 1.0f);
	const QVector3D aabb_max = compute_clip_max(c, 1.0f);

	clip_result result = clip_data(*point_cloud, aabb_min, aabb_max);
	return result.point_count;
}

bool annotation_exporter::export_kitti(const kitti_export_config &config) {
	if (!config.cuboids || !config.calibration || !config.calibration->is_loaded()) {
		if (config.reporter) {
			config.reporter(QStringLiteral("KITTI export requires loaded calibration data."));
		}
		return false;
	}

	QDir().mkpath(config.output_path);
	const QString file_path = config.output_path + QStringLiteral("/") + config.frame_id + QStringLiteral(".txt");

	QByteArray buffer;
	QTextStream stream(&buffer, QIODevice::WriteOnly);

	const auto &calibration_path = *config.calibration;
	const auto &kitti_calibration = calibration_path.get_kitti_calibration();

	for (const auto &c : *config.cuboids) {
		const QVector3D rect_pos = calibration_path.lidar_to_rect_position(c.position);

		const float kitti_x = rect_pos.x();
		const float kitti_y = rect_pos.y() + c.dimension.z() * 0.5f;
		const float kitti_z = rect_pos.z();

		const float h = c.dimension.z();
		const float w = c.dimension.y();
		const float l = c.dimension.x();

		const QVector3D euler = c.rotation.toEulerAngles();
		const float rotation_y = calibration_path.lidar_yaw_to_kitti_rotation_y(euler.z());

		const float alpha = std::atan2(rect_pos.x(), rect_pos.z()) - rotation_y;

		bounding_box_2d bounding_box;
		const std::array<QVector3D, 8> corners = compute_corners(c);
		std::array<QVector3D, 8> corners_rect{};
		for (int i = 0; i < 8; ++i) {
			corners_rect[i] = calibration_path.lidar_to_rect_position(corners[i]);
		}
		bounding_box = project_to_2d(corners_rect, kitti_calibration.projection_rect_02);

		const QString type = c.class_name.isEmpty() ? QStringLiteral("unknown") : c.class_name;

		stream << type << ' '
			   << QString::number(c.truncation, 'f', 2) << ' '
			   << QString::number(static_cast<int>(c.occlusion)) << ' '
			   << QString::number(alpha, 'f', 6) << ' ';

		if (bounding_box.valid) {
			stream << QString::number(bounding_box.left, 'f', 2) << ' '
				   << QString::number(bounding_box.top, 'f', 2) << ' '
				   << QString::number(bounding_box.right, 'f', 2) << ' '
				   << QString::number(bounding_box.bottom, 'f', 2) << ' ';
		} else {
			stream << "-1 -1 -1 -1 ";
		}

		stream << QString::number(h, 'f', 6) << ' '
			   << QString::number(w, 'f', 6) << ' '
			   << QString::number(l, 'f', 6) << ' '
			   << QString::number(kitti_x, 'f', 6) << ' '
			   << QString::number(kitti_y, 'f', 6) << ' '
			   << QString::number(kitti_z, 'f', 6) << ' '
			   << QString::number(rotation_y, 'f', 6) << '\n';
	}

	stream.flush();
	return atomic_write(file_path, buffer, config.reporter);
}

bool annotation_exporter::export_nuscenes(const nuscenes_export_config &config) {
	if (!config.cuboids) {
		if (config.reporter) {
			config.reporter(QStringLiteral("No cuboid data available for nuScenes export."));
		}
		return false;
	}

	QDir().mkpath(config.output_path);
	const QString file_path = config.output_path + QStringLiteral("/") + config.frame_id + QStringLiteral(".json");

	QJsonArray annotations;

	for (const auto &c : *config.cuboids) {
		QJsonObject object;

		object[QStringLiteral("token")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
		object[QStringLiteral("sample_token")] = config.frame_id;
		object[QStringLiteral("instance_token")] = QUuid::createUuid().toString(QUuid::WithoutBraces);

		const QString category = c.class_name.isEmpty() ? QStringLiteral("unknown") : c.class_name;
		object[QStringLiteral("category_name")] = category;

		QJsonArray translation;
		translation.append(static_cast<double>(c.position.x()));
		translation.append(static_cast<double>(c.position.y()));
		translation.append(static_cast<double>(c.position.z()));
		object[QStringLiteral("translation")] = translation;

		QJsonArray size;
		size.append(static_cast<double>(c.dimension.x()));
		size.append(static_cast<double>(c.dimension.y()));
		size.append(static_cast<double>(c.dimension.z()));
		object[QStringLiteral("size")] = size;

		QJsonArray rotation;
		rotation.append(static_cast<double>(c.rotation.scalar()));
		rotation.append(static_cast<double>(c.rotation.x()));
		rotation.append(static_cast<double>(c.rotation.y()));
		rotation.append(static_cast<double>(c.rotation.z()));
		object[QStringLiteral("rotation")] = rotation;

		const std::size_t points = count_lidar_points(c, config.point_cloud);
		object[QStringLiteral("num_lidar_pts")] = static_cast<qint64>(points);
		object[QStringLiteral("num_radar_pts")] = 0;

		annotations.append(object);
	}

	const QJsonDocument doc(annotations);
	return atomic_write(file_path, doc.toJson(QJsonDocument::Indented), config.reporter);
}
