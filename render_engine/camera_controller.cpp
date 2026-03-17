#include "camera_controller.h"
#include "orbital_camera.h"

#include <QtMath>

camera_controller::camera_controller(QObject *parent) : QObject(parent) {}

void camera_controller::handle_mouse_press(const QPointF &position, Qt::MouseButton button) {
	last_pos = position;
	mouse_dragged = false;

	if (button == Qt::RightButton) {
		rmb_pressed = true;
	}
	if (button == Qt::MiddleButton) {
		mmb_pressed = true;
	}
}

void camera_controller::handle_mouse_release(Qt::MouseButton button) {
	if (button == Qt::RightButton) {
		rmb_pressed = false;
	}
	if (button == Qt::MiddleButton) {
		mmb_pressed = false;
	}
}

bool camera_controller::handle_mouse_move(const QPointF &position) {
	if (!camera_ptr) {
		return false;
	}

	const float dx = static_cast<float>(position.x() - last_pos.x());
	const float dy = static_cast<float>(position.y() - last_pos.y());
	last_pos = position;

	if (!mouse_dragged) {
		const float distance = qSqrt(dx * dx + dy * dy);
		if (distance >= drag_threshold_pixels) {
			mouse_dragged = true;
		}
	}

	bool changed = false;

	if (rmb_pressed) {
		camera_ptr->orbit(dx, dy);
		changed = true;
	} else if (mmb_pressed) {
		camera_ptr->pan(dx, dy);
		changed = true;
	}

	if (changed) {
		emit view_changed();
	}

	return changed;
}

bool camera_controller::handle_mouse_wheel(int angle_delta) {
	if (!camera_ptr) {
		return false;
	}

	const float ticks = static_cast<float>(angle_delta) / 120.0f;
	camera_ptr->zoom(ticks);

	emit view_changed();

	return true;
}

void camera_controller::reset_to_bounds() {
	if (!camera_ptr) {
		return;
	}

	camera_ptr->reset();

	emit view_changed();
}
