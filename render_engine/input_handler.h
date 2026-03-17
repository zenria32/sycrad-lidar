#pragma once

#include <QObject>
#include <QPointF>
#include <QSize>
#include <Qt>

#include <memory>
#include <cstdint>

class orbital_camera;
class camera_controller;
class cuboid_manager;

class input_handler : public QObject {
	Q_OBJECT

	public:
	enum class interaction_mode {
		idle,
		camera_pan,
		camera_orbit,
		gizmo_interaction,
		cuboid_interaction
	};

	enum class tool_mode {
		none = -1,
		add = 0,
		select = 1,
		move = 2,
		rotate = 3,
		scale = 4
	};

	explicit input_handler(QObject *parent = nullptr);
	~input_handler() override = default;

	void set_camera(orbital_camera *camera);
	void set_cuboid_manager(cuboid_manager *manager) { cmngr = manager; }
	void set_viewport_size(const QSize &size) { viewport_size = size; }

	camera_controller *controller() const { return controller_ptr; }

	void set_tool_mode(tool_mode mode) { active_tool = mode; emit render_requested(); }
	tool_mode get_tool_mode() const { return active_tool; }

	void set_active_mode_gizmo() { active_mode = interaction_mode::gizmo_interaction; }
	void set_active_mode_idle() { active_mode = interaction_mode::idle; }

	void handle_mouse_press(const QPointF &position, Qt::MouseButton button);
	void handle_mouse_release(const QPointF &position, Qt::MouseButton button);
	void handle_mouse_move(const QPointF &position);
	void handle_mouse_wheel(int angle_delta);

	void handle_key_press(int key);
	void handle_key_release(int key);

	signals:
	void render_requested();

	void focus_requested();
	void cuboid_creation_requested(const QPointF &position);
	void cuboid_selection_requested(const QPointF &position);

	void gizmo_drag_started(const QPointF &position);
	void gizmo_drag_active(const QPointF &position);
	void gizmo_drag_ended();

	private:
	camera_controller *controller_ptr = nullptr;
	cuboid_manager *cmngr = nullptr;

	interaction_mode active_mode = interaction_mode::idle;
	tool_mode active_tool = tool_mode::select;

	QPointF cursor_position;
	QSize viewport_size;
};
