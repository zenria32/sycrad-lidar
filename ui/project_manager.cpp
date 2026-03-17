#include "project_manager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

project_manager::project_manager(QObject *parent) : QObject(parent) {
}

project_manager::~project_manager() = default;

bool project_manager::new_project(const std::string &name, const std::string &format, const std::string &data_path, const std::string &calibration_path, const std::string &media_path, const std::string &label_path) {
	QString docs_dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	if (docs_dir.isEmpty()) {
		return false;
	}

	root_dir = std::filesystem::path(docs_dir.toStdString()) / "Sycrad LiDAR" / name;

	std::error_code error_code;
	std::filesystem::create_directories(root_dir, error_code);
	if (error_code) {
		emit error(QString("Failed to create project directory: %1").arg(QString::fromStdString(error_code.message())));
		return false;
	}

	config.name = name;
	config.format = format;
	config.data_path = data_path;
	config.calibration_path = calibration_path;
	config.media_path = media_path;
	config.label_path = label_path;

	return write_json();
}

bool project_manager::open_project(const std::filesystem::path &project_dir) {
	auto json_path = project_dir / "project.json";
	if (!std::filesystem::exists(json_path)) {
		emit error(QString("Project configuration file (project.json) not found in: %1").arg(QString::fromStdString(project_dir.string())));
		return false;
	}

	root_dir = project_dir;
	return read_json(json_path);
}

bool project_manager::write_json() {
	QJsonObject object;
	object["name"] = QString::fromStdString(config.name);
	object["format"] = QString::fromStdString(config.format);
	object["data_path"] = QString::fromStdString(config.data_path);
	object["calibration_path"] = QString::fromStdString(config.calibration_path);
	object["media_path"] = QString::fromStdString(config.media_path);
	object["label_path"] = QString::fromStdString(config.label_path);

	QJsonDocument document(object);

	QFile file(QString::fromStdString((root_dir / "project.json").string()));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		emit error("Failed to save project configuration. Please check write permissions.");
		return false;
	}

	file.write(document.toJson(QJsonDocument::Indented));
	return file.error() == QFile::NoError;
}

bool project_manager::read_json(const std::filesystem::path &json_path) {
	QFile file(QString::fromStdString(json_path.string()));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		emit error("Failed to read project configuration. The file may be in use or inaccessible.");
		return false;
	}

	QJsonParseError parse_error;
	QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);

	if (parse_error.error != QJsonParseError::NoError) {
		emit error(QString("Project configuration is corrupted: %1").arg(parse_error.errorString()));
		return false;
	}

	if (!document.isObject()) {
		emit error("Invalid project configuration format. Expected a JSON object.");
		return false;
	}

	QJsonObject object = document.object();

	config.name = object["name"].toString().toStdString();
	config.format = object["format"].toString().toStdString();
	config.data_path = object["data_path"].toString().toStdString();
	config.calibration_path = object["calibration_path"].toString().toStdString();
	config.media_path = object["media_path"].toString().toStdString();
	config.label_path = object["label_path"].toString().toStdString();

	return !config.name.empty();
}
