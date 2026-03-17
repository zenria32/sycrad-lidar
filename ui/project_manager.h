#pragma once

#include <QObject>
#include <QString>

#include <filesystem>
#include <string>

struct project_config {
	std::string name;
	std::string format;
	std::string data_path;
	std::string calibration_path;
	std::string media_path;
	std::string label_path;
};

class project_manager : public QObject {
	Q_OBJECT

	public:
	explicit project_manager(QObject *parent = nullptr);
	~project_manager();

	bool new_project(const std::string &name,
		const std::string &format,
		const std::string &data_path,
		const std::string &calibration_path,
		const std::string &media_path,
		const std::string &label_path);

	bool open_project(const std::filesystem::path &project_dir);

	std::filesystem::path project_root() const { return root_dir; }
	const project_config &read_config() const { return config; }

	bool is_project_name_valid() const { return !config.name.empty(); }
	std::string current_project_name() const { return config.name; }

	signals:
	void error(const QString &message);

	private:
	bool write_json();
	bool read_json(const std::filesystem::path &json_path);

	std::filesystem::path root_dir;
	project_config config;
};
