#pragma once

#include "annotation_exporter.h"
#include "data_loader.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <string>

class cuboid_manager;
class calibration_store;
struct project_config;

class save_manager : public QObject {
    Q_OBJECT

    public:
    explicit save_manager(QObject *parent = nullptr);
    ~save_manager() override;

    void set_dependencies(cuboid_manager *cuboids, calibration_store *calibration, const project_config *config);

    void set_point_cloud(const data_variants *data);
    void set_frame_id(const QString &id);

    bool save_current();
    bool save_if_changed();
    bool is_changed() const { return changed; }

    signals:
    void save_succeed(const QString &message);
    void save_failed(const QString &message);

    private slots:
    void mark_as_changed();
    void on_auto_save();

private:
    cuboid_manager *cmngr = nullptr;
    calibration_store *cstore = nullptr;
    const project_config *project = nullptr;
    const data_variants *point_cloud = nullptr;

    QString current_frame_id;
    bool changed = false;

    QTimer auto_save_timer;
};
