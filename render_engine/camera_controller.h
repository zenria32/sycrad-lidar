#pragma once

#include <QObject>
#include <QPointF>

class orbital_camera;

class camera_controller : public QObject {
	Q_OBJECT

	public:
	explicit camera_controller(QObject *parent = nullptr);
	~camera_controller() override = default;

	void set_camera(orbital_camera *camera) { camera_ptr = camera; }
	orbital_camera *get_camera() const { return camera_ptr; }

	void handle_mouse_press(const QPointF &position, Qt::MouseButton button);
	void handle_mouse_release(Qt::MouseButton button);
	bool handle_mouse_move(const QPointF &position);
	bool handle_mouse_wheel(int angle_delta);
	bool was_drag() const { return mouse_dragged; }

	void reset_to_bounds();

	signals:
	void view_changed();

	private:
	static constexpr float drag_threshold_pixels = 3.0f;

	orbital_camera *camera_ptr = nullptr;

	QPointF last_pos;
	bool rmb_pressed = false;
	bool mmb_pressed = false;
	bool mouse_dragged = false;
};
