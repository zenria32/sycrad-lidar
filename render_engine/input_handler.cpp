#include "input_handler.h"
#include "camera_controller.h"

input_handler::input_handler(QObject *parent) : QObject(parent), controller_ptr(new camera_controller(this)) {
	connect(controller_ptr, &camera_controller::view_changed, this, &input_handler::render_requested);
}

void input_handler::set_camera(orbital_camera *camera) {
	controller_ptr->set_camera(camera);
}

void input_handler::handle_mouse_press(const QPointF &position, Qt::MouseButton button) {
	cursor_position = position;

	if (button == Qt::RightButton) {
		active_mode = interaction_mode::camera_orbit;
		controller_ptr->handle_mouse_press(position, button);
	} else if (button == Qt::MiddleButton) {
		active_mode = interaction_mode::camera_pan;
		controller_ptr->handle_mouse_press(position, button);
	} else if (button == Qt::LeftButton) {
		if (active_tool == tool_mode::add) {
			active_mode = interaction_mode::cuboid_interaction;
		} else if (active_tool == tool_mode::move || active_tool == tool_mode::rotate || active_tool == tool_mode::scale) {
			emit gizmo_drag_started(position);
		} else {
			active_mode = interaction_mode::idle;
		}
	}


}

void input_handler::handle_mouse_release(const QPointF &position, Qt::MouseButton button) {
	if (button == Qt::LeftButton) {
		if (active_mode == interaction_mode::gizmo_interaction) {
			emit gizmo_drag_ended();
			active_mode = interaction_mode::idle;
		} else if (active_mode == interaction_mode::cuboid_interaction) {
			emit cuboid_creation_requested(position);
			emit render_requested();
		} else if (active_mode == interaction_mode::idle) {
			const float dx = static_cast<float>(position.x() - cursor_position.x());
			const float dy = static_cast<float>(position.y() - cursor_position.y());
			const float distance = std::sqrt(dx * dx + dy * dy);

			if (distance < 5.0f) {
				if (active_tool == tool_mode::select || active_tool == tool_mode::move ||
					active_tool == tool_mode::rotate || active_tool == tool_mode::scale) {
					emit cuboid_selection_requested(position);
					emit render_requested();
					}
			}
		}
	}

	if (button == Qt::RightButton || button == Qt::MiddleButton) {
		controller_ptr->handle_mouse_release(button);
	}

	if (active_mode != interaction_mode::gizmo_interaction) {
		active_mode = interaction_mode::idle;
	}
}

void input_handler::handle_mouse_move(const QPointF &position) {
	if (active_mode == interaction_mode::camera_orbit || active_mode == interaction_mode::camera_pan) {
		controller_ptr->handle_mouse_move(position);
	} else if (active_mode == interaction_mode::gizmo_interaction) {
		emit gizmo_drag_active(position);
	}
}

void input_handler::handle_mouse_wheel(int angle_delta) {
	controller_ptr->handle_mouse_wheel(angle_delta);
}

void input_handler::handle_key_press(int key) {
	switch (key) {
	case Qt::Key_Z:
		controller_ptr->reset_to_bounds();
		break;
	case Qt::Key_F:
		emit focus_requested();
		break;
	default:
		break;
	}
}

void input_handler::handle_key_release(int key) {
	Q_UNUSED(key);
	// No actions for holding keys right now.
}
