#include "camera_manager.h"
#include "calibration_store.h"
#include "orbital_camera.h"
#include "project_manager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

camera_manager::camera_manager(QObject *parent) : QObject(parent) {
	directipn_change_timer.setInterval(400);

	connect(&directipn_change_timer, &QTimer::timeout, this, [this]() {
		if (!camera || !media_display || channels.empty()) {
			return;
		}

		const QString correct_channel = resolve_channel_direction();
		if (!correct_channel.isEmpty() && correct_channel.toStdString() != active_channel) {
			show_image(correct_channel.toStdString());
		}
	});
}

void camera_manager::load_frame(const project_config &config, const QString &frame_id) {
	clear();
	dataset_format = config.format;
	resolve_channels(config, frame_id);

	for (const auto &ch : channels) {
		if (!ch.path.isEmpty()) {
			QPixmap pixmap(ch.path);
			if (!pixmap.isNull()) {
				cached_images[ch.name] = std::move(pixmap);
			}
		}
	}

	if (!channels.empty()) {
		const QString format = QString::fromStdString(dataset_format);
		if (format == "KITTI") {
			show_image(channels.front().name);
		} else if (format == "NuScenes" && camera) {
			const QString correct_channel = resolve_channel_direction();
			if (!correct_channel.isEmpty()) {
				show_image(correct_channel.toStdString());
			} else {
				show_image(channels.front().name);
			}
			directipn_change_timer.start();
		} else {
			show_image(channels.front().name);
		}
	} else {
		show_placeholder();
	}
}

void camera_manager::clear() {
	channels.clear();
	cached_images.clear();
	active_channel.clear();
	directipn_change_timer.stop();
	show_placeholder();
}

void camera_manager::resolve_channels(const project_config &config, const QString &frame_id) {
	channels.clear();

	if (config.media_path.empty()) {
		return;
	}

	const QString media = QString::fromStdString(config.media_path);
	const QString format = QString::fromStdString(config.format);

	if (format == "KITTI") {
		static const QStringList extensions = {".png", ".jpg"};

		auto try_path = [&](const QString &path) -> bool {
			for (const auto &ext : extensions) {
				const QString img = path + "/" + frame_id + ext;
				if (QFileInfo::exists(img)) {
					camera_channel ch;
					ch.name = "image_02";
					ch.direction = QVector3D(0, 1, 0);
					ch.path = img;
					channels.push_back(std::move(ch));
					return true;
				}
			}
			return false;
		};

		// search directories can be expanded
		try_path(media);

	} else if (format == "NuScenes") {
		static const std::vector<std::pair<std::string, QVector3D>> cam_angles = {
			{"CAM_FRONT", QVector3D( 0,  1, 0)},
			{"CAM_FRONT_LEFT", QVector3D(-0.5f, 0.866f, 0)},
			{"CAM_FRONT_RIGHT", QVector3D( 0.5f, 0.866f, 0)},
			{"CAM_BACK", QVector3D( 0, -1, 0)},
			{"CAM_BACK_LEFT", QVector3D(-0.866f, -0.5f, 0)},
			{"CAM_BACK_RIGHT", QVector3D( 0.866f, -0.5f, 0)},
		};

		QString scene_token = frame_id;
		const int index_of_lidar_word = frame_id.indexOf("__LIDAR_TOP__");
		if (index_of_lidar_word > 0) {
			scene_token = frame_id.left(index_of_lidar_word);
		}

		for (const auto &[angle, direction] : cam_angles) {
			QVector3D dir = direction;

			if (cstore && cstore->is_loaded()) {
				const auto *entry = cstore->get_sensor_entry(angle);
				if (entry) {
					dir = entry->rotation.rotatedVector(QVector3D(1, 0, 0));
					dir.setZ(0);
					const float len = dir.length();
					if (len > 1e-6f) {
						dir /= len;
					} else {
						dir = direction;
					}
				}
			}

			const QString prefix = scene_token + "__" + QString::fromStdString(angle) + "__";

			// serach directories will be expanded
			const QStringList search_dirs = { media + "/" + QString::fromStdString(angle) };

			QString found_path;
			for (const auto &dir_path : search_dirs) {
				const QDir cam_dir(dir_path);
				if (!cam_dir.exists()) {
					continue;
				}
				const QStringList matches = cam_dir.entryList({prefix + "*"}, QDir::Files);
				if (!matches.isEmpty()) {
					found_path = cam_dir.absoluteFilePath(matches.first());
					break;
				}
			}

			if (!found_path.isEmpty()) {
				camera_channel ch;
				ch.name = angle;
				ch.direction = dir;
				ch.path = found_path;
				channels.push_back(std::move(ch));
			}
		}
	}
}

QString camera_manager::resolve_channel_direction() {
	if (channels.empty() || !camera) {
		return {};
	}

	if (dataset_format == "KITTI") {
		return QString::fromStdString(channels.front().name);
	}

	const QVector3D target = camera->get_target();
	QVector3D view_direction(target.x(), target.y(), 0.0f);
	float len = view_direction.length();

	if (len < 1.0f) {
		const QVector3D fwd = camera->forward();
		view_direction = QVector3D(fwd.x(), fwd.y(), 0.0f);
		len = view_direction.length();
		if (len < 1e-6f) {
			return QString::fromStdString(channels.front().name);
		}
	}
	view_direction /= len;

	float best_dot = -2.0f;
	std::string best_channel;

	for (const auto &ch : channels) {
		const float dot = QVector3D::dotProduct(view_direction, ch.direction);
		if (dot > best_dot) {
			best_dot = dot;
			best_channel = ch.name;
		}
	}

	return best_channel.empty() ? QString() : QString::fromStdString(best_channel);
}

void camera_manager::show_image(const std::string &channel_name) {
	if (!media_display) {
		return;
	}

	active_channel = channel_name;

	auto iterator = cached_images.find(channel_name);
	if (iterator != cached_images.end() && !iterator->second.isNull()) {
		const QSize size = media_display->size();
		if (size.width() > 0 && size.height() > 0) {
			media_display->setPixmap(iterator->second.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		} else {
			media_display->setPixmap(iterator->second);
		}
		media_display->setAlignment(Qt::AlignCenter);
	} else {
		for (const auto &ch : channels) {
			if (ch.name == channel_name && !ch.path.isEmpty()) {
				QPixmap pixmap(ch.path);
				if (!pixmap.isNull()) {
					cached_images[channel_name] = pixmap;
					const QSize size = media_display->size();
					if (size.width() > 0 && size.height() > 0) {
						media_display->setPixmap(pixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
					} else {
						media_display->setPixmap(pixmap);
					}
					media_display->setAlignment(Qt::AlignCenter);
					return;
				}
			}
		}
		show_placeholder();
	}
}

void camera_manager::show_placeholder() {
	if (!media_display) {
		return;
	}

	media_display->clear();
	media_display->setText(QStringLiteral("No Camera Data"));
	media_display->setAlignment(Qt::AlignCenter);
}
