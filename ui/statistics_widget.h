#pragma once

#include <QColor>
#include <QFont>
#include <QStaticText>
#include <QTimer>
#include <QWidget>

class lidar_viewport;

class statistics_widget : public QWidget {
	Q_OBJECT

	public:
	explicit statistics_widget(QWidget *parent = nullptr);

	void get_viewport(lidar_viewport *viewport);

	void get_graphics_api(const QString &api);
	void get_point_count(quint64 count);

	protected:
	void paintEvent(QPaintEvent *event) override;

	private slots:
	void update_stats();

	private:
	static QString format_bytes(quint64 bytes);

	static constexpr int widget_width = 210;
	static constexpr int widget_height = 168;
	static constexpr int padding_left = 13;
	static constexpr int padding_right = 13;
	static constexpr int padding_top = 10;
	static constexpr int row_height = 18;
	static constexpr int section_gap = 6;
	static constexpr int border_radius = 8;

	static constexpr int update_interval = 200;

	QFont variable_font;
	QFont value_font;

	QColor background_color{30, 30, 30, 160};
	QColor border_color{255, 255, 255, 18};
	QColor variable_color{160, 160, 170, 140};
	QColor value_color{232, 232, 232, 255};
	QColor section_color{100, 160, 255, 120};

	QStaticText api_variable{QStringLiteral("API")};
	QStaticText fps_variable{QStringLiteral("FPS")};
	QStaticText frametime_variable{QStringLiteral("FRAMETIME")};
	QStaticText resolution_variable{QStringLiteral("RESOLUTION")};
	QStaticText points_variable{QStringLiteral("POINTS")};

	QStaticText api_value{QStringLiteral("\u2014")};
	QStaticText fps_value{QStringLiteral("0")};
	QStaticText frametime_value{QStringLiteral("0.0 ms")};
	QStaticText resolution_value{QStringLiteral("\u2014")};
	QStaticText points_value{QStringLiteral("0")};

	QStaticText memory_label{QStringLiteral("MEMORY")};

	QStaticText data_size_variable{QStringLiteral("DATA")};
	QStaticText algorithm_size_variable{QStringLiteral("ALGORITHM")};

	QStaticText data_size_value{QStringLiteral("\u2014")};
	QStaticText algorithm_size_value{QStringLiteral("\u2014")};

	QString last_api_value;
	QString last_fps_value;
	QString last_frametime_value;
	QString last_resolution_value;
	QString last_points_value;
	QString last_data_size_value;
	QString last_algorithm_size_value;

	lidar_viewport *viewport_ptr = nullptr;
	QTimer timer;
};
