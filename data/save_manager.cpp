#include "save_manager.h"
#include "cuboid_manager.h"
#include <project_manager.h>

save_manager::save_manager(QObject *parent) : QObject(parent) {
	auto_save_timer.setInterval(60000);
	connect(&auto_save_timer, &QTimer::timeout, this, &save_manager::on_auto_save);
}

save_manager::~save_manager() = default;

void save_manager::set_dependencies(cuboid_manager *cuboids, calibration_store *calibration, const project_config *config) {
	if (cmngr) {
		disconnect(cmngr, nullptr, this, nullptr);
	}

	cmngr = cuboids;
	cstore = calibration;
	project = config;

	if (cmngr) {
		connect(cmngr, &cuboid_manager::cuboid_added, this, &save_manager::mark_as_changed);
		connect(cmngr, &cuboid_manager::cuboid_removed, this, &save_manager::mark_as_changed);
		connect(cmngr, &cuboid_manager::cuboid_updated, this, &save_manager::mark_as_changed);
	}

	auto_save_timer.start();
}

void save_manager::set_point_cloud(const data_variants *data) {
	point_cloud = data;
}

void save_manager::set_frame_id(const QString &id) {
	current_frame_id = id;
	changed = false;
}

void save_manager::mark_as_changed() {
	changed = true;
}

void save_manager::on_auto_save() {
	if (!changed) {
		return;
	}

	if (save_current()) {
		emit save_succeed(QStringLiteral("Auto-save"));
	}
}

bool save_manager::save_current() {
	if (!cmngr || !project || current_frame_id.isEmpty()) {
		emit save_failed(QStringLiteral("No active frame to save."));
		return false;
	}

	if (project->label_path.empty()) {
		emit save_failed(QStringLiteral("Label path is not configured in project settings."));
		return false;
	}

	const QString label_dir = QString::fromStdString(project->label_path);
	const auto &cuboids = cmngr->get_cuboids();

	const auto reporter = [this](const QString &message) {
		emit save_failed(message);
	};

	bool success = false;

	if (project->format == "KITTI") {
		if (!cstore || !cstore->is_loaded()) {
			emit save_failed(QStringLiteral("KITTI calibration is not loaded. Cannot export labels."));
			return false;
		}

		kitti_export_config config;
		config.output_path = label_dir;
		config.frame_id = current_frame_id;
		config.cuboids = &cuboids;
		config.calibration = cstore;
		config.point_cloud = point_cloud;
		config.reporter = reporter;

		success = annotation_exporter::export_kitti(config);
	} else if (project->format == "NuScenes") {
		nuscenes_export_config config;
		config.output_path = label_dir;
		config.frame_id = current_frame_id;
		config.cuboids = &cuboids;
		config.point_cloud = point_cloud;
		config.reporter = reporter;

		success = annotation_exporter::export_nuscenes(config);
	} else {
		emit save_failed(QStringLiteral("Export is not supported for format: %1")
			.arg(QString::fromStdString(project->format)));
		return false;
	}

	if (success) {
		changed = false;
		emit save_succeed(QStringLiteral("Saved: %1").arg(current_frame_id));
	}

	return success;
}

bool save_manager::save_if_changed() {
	if (!changed) {
		return true;
	}
	return save_current();
}
