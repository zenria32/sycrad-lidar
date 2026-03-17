#pragma once

#include "data_loader.h"
#include "octree_node.h"
#include "cuboid.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QRhiWidget>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QVector3D>

#include <array>
#include <memory>

class grid_renderer;
class point_cloud_renderer;
class cuboid_renderer;
class gizmo_renderer;
class orbital_camera;
class input_handler;
class cuboid_manager;
class calibration_store;
class loading_bar;

struct ray;

enum class gizmo_axis;

class lidar_viewport : public QRhiWidget {
	Q_OBJECT

	public:
	explicit lidar_viewport(QWidget *parent = nullptr);
	~lidar_viewport() override;

	void set_cuboid_manager(cuboid_manager *manager);
	void set_calibration_store(calibration_store *store) { cstore = store; }

	orbital_camera *get_camera() const { return camera.get(); }
	input_handler *get_input() const { return input.get(); }
	QString get_graphics_api() const { return graphics_api; }

	double average_fps() const;
	double average_frame_time() const;
	bool is_rendering() const;
	QSize get_render_resolution() const { return render_resolution; }

	void load_point_cloud(data_variants data);
	void show_loading_overlay(const QString &message);
	void hide_loading_overlay();

	struct memory_info {
		std::size_t vram_data_size = 0;
		std::size_t ram_octree_size = 0;
	};
	memory_info get_memory_info() const;

	signals:
	void error(const QString &message);

	protected:
	void initialize(QRhiCommandBuffer *command_buffer) override;
	void render(QRhiCommandBuffer *command_buffer) override;
	void releaseResources() override;

	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

	void resizeEvent(QResizeEvent *event) override;

	private:
	void setup_renderers();

	void begin_gizmo_drag(const QPointF &position);
	void process_gizmo_drag(const QPointF &position);
	void end_gizmo_drag();
	void update_gizmo();

	QVector3D compute_drag_point(const ray &r, gizmo_axis axis) const;
	float compute_rotation_angle(const ray &r, gizmo_axis axis) const;

	std::unique_ptr<orbital_camera> camera;
	std::unique_ptr<input_handler> input;
	std::unique_ptr<grid_renderer> render_grid;
	std::unique_ptr<point_cloud_renderer> render_point_cloud;
	std::unique_ptr<cuboid_renderer> render_cuboid;
	std::unique_ptr<gizmo_renderer> render_gizmo;
	cuboid_manager *cmngr = nullptr;
	calibration_store *cstore = nullptr;

	float ground_z = 0.0f;
	data_format active_format = data_format::unknown;

	cuboid drag_snapshot;
	QVector3D drag_start_point;
	float drag_start_angle = 0.0f;
	bool is_dragging = false;

	QString graphics_api;
	bool renderers_initialized = false;

	static constexpr int frame_sample_count = 64;
	static constexpr double idle_threshold_ms = 500.0;

	std::array<double, frame_sample_count> frame_times{};
	int frame_time_index = 0;
	int frame_time_filled = 0;
	QElapsedTimer frame_timer;
	QElapsedTimer idle_timer;

	QSize render_resolution;

	loading_bar *loading_overlay = nullptr;
	QFutureWatcher<spatial::octree> leaf_watcher;
	data_variants pending_data_for_upload;
};
