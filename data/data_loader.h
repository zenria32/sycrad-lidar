#pragma once

#include "data.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <variant>

using data_variants = std::variant<
	std::monostate,
	std::shared_ptr<kitti_data>,
	std::shared_ptr<las_data>,
	std::shared_ptr<nuscenes_data>,
	std::shared_ptr<pcd_data>,
	std::shared_ptr<waymo_data>>;

class data_loader : public QObject {
	Q_OBJECT

	public:
	explicit data_loader(QObject *parent = nullptr);
	~data_loader() = default;

	void call_loader(const QString &file_path, const std::string &format);
	bool is_loading() const;

	signals:
	void error(const QString &errorMessage);
	void loaded(data_variants data);

	private:
	data_variants loader(const QString &file_path, const std::string &format);

	std::shared_ptr<kitti_data> kitti_loader(const QString &filePath);
	std::shared_ptr<las_data> las_loader(const QString &filePath);
	std::shared_ptr<nuscenes_data> nuscenes_loader(const QString &filePath);
	std::shared_ptr<pcd_data> pcd_loader(const QString &filePath);
	std::shared_ptr<waymo_data> waymo_loader(const QString &filePath);

	QFutureWatcher<data_variants> watcher;
	std::atomic<bool> is_data_loading{false};
};
