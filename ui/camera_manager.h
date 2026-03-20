#pragma once

#include <QLabel>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QVector3D>

#include <string>
#include <unordered_map>
#include <vector>

class orbital_camera;
class calibration_store;
struct project_config;

class camera_manager : public QObject {
    Q_OBJECT

    public:
    explicit camera_manager(QObject *parent = nullptr);
    ~camera_manager() override = default;

    void set_camera(orbital_camera *cam) { camera = cam; }
    void set_calibration_store(calibration_store *store) { cstore = store; }
    void set_media_display(QLabel *display) { media_display = display; }

    void load_frame(const project_config &config, const QString &frame_id);
    void clear();

    private:
    struct camera_channel {
        std::string name;
        QVector3D direction;
        QString path;
    };

    void resolve_channels(const project_config &config, const QString &frame_id);
    QString resolve_channel_direction();
    void show_image(const std::string &channel_name);
    void show_placeholder();

    orbital_camera *camera = nullptr;
    calibration_store *cstore = nullptr;
    QLabel *media_display = nullptr;

    std::vector<camera_channel> channels;
    std::string active_channel;
    std::unordered_map<std::string, QPixmap> cached_images;
    std::string dataset_format;

    QTimer directipn_change_timer;
};
