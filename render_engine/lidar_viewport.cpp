#include "lidar_viewport.h"
#include "grid_renderer.h"
#include "input_handler.h"
#include "orbital_camera.h"
#include "point_cloud_renderer.h"
#include "cuboid_renderer.h"
#include "gizmo_renderer.h"
#include "cuboid_manager.h"
#include "calibration_store.h"
#include "octree_builder.h"
#include "loading_bar.h"

#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

bool is_data_valid(const data_variants &data) {
	bool valid = false;
	std::visit(
		[&valid](const auto &pending_data) {
			using T = std::decay_t<decltype(pending_data)>;

			if constexpr (!std::is_same_v<T, std::monostate>) {
				valid = pending_data && pending_data->is_valid() &&
					pending_data->point_count > 0;
			}
		},
		data);

	return valid;
}

lidar_viewport::lidar_viewport(QWidget *parent) : QRhiWidget(parent),
		camera(std::make_unique<orbital_camera>()),
		input(std::make_unique<input_handler>()),
		render_grid(std::make_unique<grid_renderer>()),
		render_point_cloud(std::make_unique<point_cloud_renderer>()),
		render_cuboid(std::make_unique<cuboid_renderer>()),
		render_gizmo(std::make_unique<gizmo_renderer>()) {

#if defined(Q_OS_MACOS)
	setApi(Api::Metal);
#elif defined(Q_OS_WIN)
	setApi(Api::Direct3D12);
#endif

	loading_overlay = new loading_bar(this);
	loading_overlay->hide();

	setFocusPolicy(Qt::StrongFocus);

	input->set_camera(camera.get());

	connect(input.get(), &input_handler::render_requested, this,
		[this]() { update(); });

	frame_timer.start();
	idle_timer.start();
}

lidar_viewport::~lidar_viewport() = default;

void lidar_viewport::initialize(QRhiCommandBuffer *command_buffer) {
	if (rhi()) {
		graphics_api = QString::fromUtf8(rhi()->backendName());
	}

	if (renderers_initialized) {
		return;
	}

	setup_renderers();
	renderers_initialized = true;
}

void lidar_viewport::render(QRhiCommandBuffer *command_buffer) {
	const QSize pixel_size = renderTarget()->pixelSize();
	if (pixel_size.isEmpty()) {
		return;
	}

	if (render_resolution != pixel_size) {
		render_resolution = pixel_size;
	}

	const double elapsed_ms = frame_timer.nsecsElapsed() / 1'000'000.0;
	frame_timer.restart();
	idle_timer.restart();

	if (elapsed_ms > 0.0 && elapsed_ms < 1000.0) {
		frame_times[frame_time_index] = elapsed_ms;
		frame_time_index = (frame_time_index + 1) % frame_sample_count;
		if (frame_time_filled < frame_sample_count) {
			++frame_time_filled;
		}
	}

	if (render_point_cloud) {
		render_point_cloud->dispatch_cull_info(command_buffer, pixel_size, camera.get());
	}

	command_buffer->beginPass(renderTarget(),
		QColor::fromRgbF(0.13f, 0.13f, 0.13f, 1.0f),
		{1.0f, 0}, nullptr);

	if (render_grid) {
		render_grid->render(command_buffer, pixel_size, camera.get());
	}
	if (render_point_cloud) {
		render_point_cloud->render(command_buffer, pixel_size, camera.get());
	}
	if (render_cuboid && render_cuboid->is_visible()) {
		render_cuboid->render(command_buffer, pixel_size, camera.get());
	}
	if (render_gizmo && render_gizmo->is_visible()) {
		update_gizmo();
		render_gizmo->render(command_buffer, pixel_size, camera.get());
	}

	command_buffer->endPass();

	if (camera && camera->is_animation_playing()) {
		camera->advance_animation(static_cast<float>(elapsed_ms));
		update();
	}
}

void lidar_viewport::releaseResources() {
	render_grid.reset();
	render_point_cloud.reset();
	renderers_initialized = false;
}

void lidar_viewport::resizeEvent(QResizeEvent *event) {
	QRhiWidget::resizeEvent(event);

	if (event->size().height() > 0) {
		const float aspect = static_cast<float>(event->size().width()) /
			static_cast<float>(event->size().height());
		camera->set_aspect_ratio(aspect);
	}

	if (loading_overlay) {
		loading_overlay->setGeometry(rect());
	}
}

void lidar_viewport::mousePressEvent(QMouseEvent *event) {
	input->handle_mouse_press(event->position(), event->button());
	event->accept();
}

void lidar_viewport::mouseReleaseEvent(QMouseEvent *event) {
	input->handle_mouse_release(event->position(), event->button());
	event->accept();
}

void lidar_viewport::mouseMoveEvent(QMouseEvent *event) {
	input->handle_mouse_move(event->position());
	event->accept();
}

void lidar_viewport::wheelEvent(QWheelEvent *event) {
	input->handle_mouse_wheel(event->angleDelta().y());
	event->accept();
}

void lidar_viewport::keyPressEvent(QKeyEvent *event) {
	input->handle_key_press(event->key());
	event->accept();
}

void lidar_viewport::keyReleaseEvent(QKeyEvent *event) {
	input->handle_key_release(event->key());
	event->accept();
}

void lidar_viewport::setup_renderers() {
	if (!render_grid) {
		render_grid = std::make_unique<grid_renderer>();
	}
	if (!render_point_cloud) {
		render_point_cloud = std::make_unique<point_cloud_renderer>();
	}

	QRhi *rhi_ptr = rhi();
	QRhiRenderPassDescriptor *render_pass = renderTarget()->renderPassDescriptor();

	auto reporter = [this](const QString &message) { emit error(message); };

	render_grid->set_reporter(reporter);
	render_point_cloud->set_reporter(reporter);
	camera->set_reporter(reporter);
	render_cuboid->set_reporter(reporter);
	render_gizmo->set_reporter(reporter);

	render_grid->initialize(rhi_ptr, render_pass, size());
	render_point_cloud->initialize(rhi_ptr, render_pass, size());
	render_cuboid->initialize(rhi_ptr, render_pass, size());
	render_gizmo->initialize(rhi_ptr, render_pass, size());

	render_cuboid->set_cuboid_manager(cmngr);

	render_grid->set_visible(true);
	render_point_cloud->set_visible(true);
	render_cuboid->set_visible(true);
	render_gizmo->set_visible(true);

	if (render_point_cloud->have_bounds()) {
		QVector3D bounds_min = render_point_cloud->bounds_min();
		QVector3D bounds_max = render_point_cloud->bounds_max();

		const float ground_level_bounds = render_point_cloud->get_ground_level();
		bounds_min.setZ(ground_level_bounds);
		bounds_max.setZ(ground_level_bounds);

		camera->get_bounds(bounds_min, bounds_max);
	} else {
		camera->get_bounds(render_grid->bounds_min(), render_grid->bounds_max());
	}
}

void lidar_viewport::load_point_cloud(data_variants data) {
	if (!render_point_cloud) {
		hide_loading_overlay();
		return;
	}

	const bool is_pending_data = is_data_valid(data);

	if (!is_pending_data) {
		render_point_cloud->upload_data(std::move(data));
		update();
		QTimer::singleShot(100, this, [this]() { hide_loading_overlay(); });
		return;
	}

	show_loading_overlay(QStringLiteral("Creating Octree Structure"));

	pending_data_for_upload = data;

	disconnect(&leaf_watcher, &QFutureWatcher<spatial::octree>::finished,
		this, nullptr);

	QFuture<spatial::octree> future = QtConcurrent::run([data]() -> spatial::octree {
			std::vector<float> x, y, z;

			std::visit(
				[&x, &y, &z](const auto &d) {
					using T = std::decay_t<decltype(d)>;
					if constexpr (!std::is_same_v<T, std::monostate>) {
						if (!d || !d->is_valid()) {
							return;
						}

						const auto *ptr = static_cast<const float *>(d->access());
						constexpr std::size_t fpv = T::element_type::floats_per_vertex();
						const std::size_t n = d->point_count;
						x.resize(n);
						y.resize(n);
						z.resize(n);
						for (std::size_t i = 0; i < n; ++i) {
							x[i] = ptr[i * fpv + 0];
							y[i] = ptr[i * fpv + 1];
							z[i] = ptr[i * fpv + 2];
						}
					}
				},
				data);

			if (x.empty()) {
				return {};
			}

			spatial::octree_builder builder;
			return builder.build(x.data(), y.data(), z.data(),
				static_cast<uint32_t>(x.size()));
		});

	connect(
		&leaf_watcher, &QFutureWatcher<spatial::octree>::finished, this,
		[this]() {
			auto result = leaf_watcher.result();
			if (!result.empty()) {
				render_point_cloud->upload_octree_data(std::move(result));
			}

			render_point_cloud->upload_data(std::move(pending_data_for_upload));

			if (render_grid) {
				render_grid.reset();
			}

			const auto data_type = render_point_cloud->get_data_type();
			if (camera && render_point_cloud->have_bounds()) {
				QVector3D bounds_min = render_point_cloud->bounds_min();
				QVector3D bounds_max = render_point_cloud->bounds_max();

				const float ground_level_bounds = render_point_cloud->get_ground_level();
				bounds_min.setZ(ground_level_bounds);
				bounds_max.setZ(ground_level_bounds);
				ground_z = ground_level_bounds;

				if (data_type == data_format::nuscenes && cstore && cstore->is_loaded()) {
					ground_z = cstore->get_nuscenes_ground_z();
				}

				camera->get_bounds(bounds_min, bounds_max);
			}

			update();
			QTimer::singleShot(100, this, [this]() { hide_loading_overlay(); });
		});

	leaf_watcher.setFuture(future);
}

void lidar_viewport::show_loading_overlay(const QString &message) {
	if (!loading_overlay) {
		return;
	}

	loading_overlay->setGeometry(rect());
	loading_overlay->show_loading(message);
}

void lidar_viewport::hide_loading_overlay() {
	if (loading_overlay) {
		loading_overlay->hide_loading();
	}
}

void lidar_viewport::set_cuboid_manager(cuboid_manager *manager) {
	cmngr = manager;
	if (render_cuboid && manager) {
		render_cuboid->set_cuboid_manager(manager);

		connect(manager, &cuboid_manager::cuboid_added, this, [this](uint32_t) {
			if (render_cuboid) { render_cuboid->set_geometry_changed(); }
			update();
		});
		connect(manager, &cuboid_manager::cuboid_removed, this, [this](uint32_t) {
			if (render_cuboid) { render_cuboid->set_geometry_changed(); }
			update();
		});
		connect(manager, &cuboid_manager::cuboid_updated, this, [this](uint32_t) {
			if (render_cuboid) { render_cuboid->set_geometry_changed(); }
			update();
		});
		connect(manager, &cuboid_manager::selected_cuboid_changed, this, [this](uint32_t, uint32_t) {
			if (render_cuboid) { render_cuboid->set_geometry_changed(); }
			update();
		});
	}

	if (input && manager) {
		connect(input.get(), &input_handler::cuboid_creation_requested, this, [this](const QPointF &position) {
			if (!camera) return;

			ray r = camera->raycast(position, size());

			if (std::abs(r.direction.z()) > 1e-6f) {
				const float t = (ground_z - r.origin.z()) / r.direction.z();
				if (t > 0.0f) {
					QVector3D collision = r.origin + r.direction * t;

					cuboid new_cuboid;
					new_cuboid.dimension = QVector3D(4.0f, 2.0f, 2.0f);
					new_cuboid.rotation = QQuaternion();
					new_cuboid.position = collision;

					collision.setZ(ground_z + new_cuboid.dimension.z() * 0.5f);

					cmngr->add_cuboid(new_cuboid);
				}
			}
		});

		connect(input.get(), &input_handler::cuboid_selection_requested, this, [this](const QPointF &position) {
			if (!camera || !cmngr) return;
			ray r = camera->raycast(position, size());

			const auto &cuboids = cmngr->get_cuboids();
			float min_t = std::numeric_limits<float>::max();
			uint32_t collision_id = 0;
			bool collision = false;

			for (const auto &c : cuboids) {
				float t;
				const QVector3D half_extents = c.dimension * 0.5f;
				if (r.intersect_oriented(c.position, c.rotation, half_extents, t)) {
					if (t < min_t) {
						min_t = t;
						collision_id = c.id;
						collision = true;
					}
				}
			}

			if (collision) {
				cmngr->select(collision_id);
			} else {
				cmngr->deselect();
			}
		});

		connect(input.get(), &input_handler::gizmo_drag_started, this,
			[this](const QPointF &position) { begin_gizmo_drag(position); });
		connect(input.get(), &input_handler::gizmo_drag_active, this,
			[this](const QPointF &position) { process_gizmo_drag(position); });
		connect(input.get(), &input_handler::gizmo_drag_ended, this,
			[this]() { end_gizmo_drag(); });
	}
}

double lidar_viewport::average_fps() const {
	if (!is_rendering()) {
		return 0.0;
	}
	const double average_ms = average_frame_time();
	return (average_ms > 0.0) ? 1000.0 / average_ms : 0.0;
}

double lidar_viewport::average_frame_time() const {
	if (frame_time_filled == 0) {
		return 0.0;
	}
	double sum = 0.0;
	for (int i = 0; i < frame_time_filled; ++i) {
		sum += frame_times[i];
	}
	return sum / frame_time_filled;
}

bool lidar_viewport::is_rendering() const {
	return frame_time_filled > 0 && idle_timer.elapsed() < idle_threshold_ms;
}

lidar_viewport::memory_info lidar_viewport::get_memory_info() const {
	memory_info info;
	if (render_point_cloud) {
		auto stats = render_point_cloud->get_memory_stats();
		info.vram_size = stats.vram_size;
		info.ram_size = stats.ram_size;
	}
	return info;
}

void lidar_viewport::update_gizmo() {
	if (!render_gizmo || !cmngr || !input) {
		return;
	}

	const auto tool = input->get_tool_mode();
	const bool is_gizmo_tool = (tool == input_handler::tool_mode::move ||
		tool == input_handler::tool_mode::rotate ||
		tool == input_handler::tool_mode::scale);

	const bool has_selection = cmngr->has_anything_selected();

	render_gizmo->set_active(is_gizmo_tool && has_selection);

	if (is_gizmo_tool && has_selection) {
		const cuboid *c = cmngr->get_selected_cuboid();
		if (c) {
			render_gizmo->set_target(c->position, c->rotation);
		}

		switch (tool) {
		case input_handler::tool_mode::move:
			render_gizmo->set_gizmo_mode(gizmo_mode::move);
			break;
		case input_handler::tool_mode::rotate:
			render_gizmo->set_gizmo_mode(gizmo_mode::rotate);
			break;
		case input_handler::tool_mode::scale:
			render_gizmo->set_gizmo_mode(gizmo_mode::scale);
			break;
		default:
			break;
		}
	}
}

void lidar_viewport::begin_gizmo_drag(const QPointF &position) {
	if (!camera || !cmngr || !render_gizmo || !cmngr->has_anything_selected()) {
		return;
	}

	ray r = camera->raycast(position, size());
	gizmo_axis hit = render_gizmo->hit_test(r);

	if (hit == gizmo_axis::none) {
		input->set_active_mode_idle();
		return;
	}

	const cuboid *c = cmngr->get_selected_cuboid();
	if (!c) {
		return;
	}

	drag_snapshot = *c;
	is_dragging = true;
	render_gizmo->set_dragging_axis(hit);

	if (render_gizmo->get_gizmo_mode() == gizmo_mode::rotate) {
		drag_start_angle = compute_rotation_angle(r, hit);
	} else {
		drag_start_point = compute_drag_point(r, hit);
	}

	input->set_active_mode_gizmo();
	update();
}

void lidar_viewport::process_gizmo_drag(const QPointF &position) {
	if (!is_dragging || !camera || !cmngr || !render_gizmo) {
		return;
	}

	const uint32_t id = cmngr->get_selected_id();
	if (id == 0) {
		return;
	}

	ray r = camera->raycast(position, size());
	const gizmo_axis axis = render_gizmo->get_dragging_axis();

	if (render_gizmo->get_gizmo_mode() == gizmo_mode::move) {
		const QVector3D current_point = compute_drag_point(r, axis);
		const QVector3D delta = current_point - drag_start_point;

		cuboid updated = drag_snapshot;
		updated.position = drag_snapshot.position + delta;
		cmngr->update_cuboid_avoid_undo_stack(id, updated);

	} else if (render_gizmo->get_gizmo_mode() == gizmo_mode::rotate) {
		const float current_angle = compute_rotation_angle(r, axis);
		const float delta_angle = current_angle - drag_start_angle;

		QVector3D axis_vec;
		switch (axis) {
		case gizmo_axis::x: axis_vec = QVector3D(1, 0, 0); break;
		case gizmo_axis::y: axis_vec = QVector3D(0, 1, 0); break;
		case gizmo_axis::z: axis_vec = QVector3D(0, 0, 1); break;
		default: return;
		}

		const float degrees = delta_angle * 180.0f / static_cast<float>(M_PI);
		cuboid updated = drag_snapshot;
		updated.rotation = QQuaternion::fromAxisAndAngle(axis_vec, degrees) * drag_snapshot.rotation;
		cmngr->update_cuboid_avoid_undo_stack(id, updated);

	} else if (render_gizmo->get_gizmo_mode() == gizmo_mode::scale) {
		const QVector3D current_point = compute_drag_point(r, axis);
		const QVector3D delta = current_point - drag_start_point;

		cuboid updated = drag_snapshot;
		switch (axis) {
		case gizmo_axis::x:
			updated.dimension.setX(std::max(0.1f, drag_snapshot.dimension.x() + delta.x()));
			break;
		case gizmo_axis::y:
			updated.dimension.setY(std::max(0.1f, drag_snapshot.dimension.y() + delta.y()));
			break;
		case gizmo_axis::z:
			updated.dimension.setZ(std::max(0.1f, drag_snapshot.dimension.z() + delta.z()));
			break;
		default: break;
		}
		cmngr->update_cuboid_avoid_undo_stack(id, updated);
	}

	update();
}

void lidar_viewport::end_gizmo_drag() {
	if (!is_dragging || !cmngr) {
		return;
	}

	const uint32_t id = cmngr->get_selected_id();
	if (id != 0) {
		const cuboid *current = cmngr->find(id);
		if (current) {
			cuboid final_state = *current;
			cmngr->update_cuboid_avoid_undo_stack(id, drag_snapshot);
			cmngr->update_cuboid(id, final_state);
		}
	}

	is_dragging = false;
	render_gizmo->set_dragging_axis(gizmo_axis::none);
	update();
}

QVector3D lidar_viewport::compute_drag_point(const ray &r, gizmo_axis axis) const {
	if (!render_gizmo) {
		return {};
	}

	QVector3D axis_direction;
	switch (axis) {
	case gizmo_axis::x: axis_direction = QVector3D(1, 0, 0); break;
	case gizmo_axis::y: axis_direction = QVector3D(0, 1, 0); break;
	case gizmo_axis::z: axis_direction = QVector3D(0, 0, 1); break;
	default: return {};
	}

	const QVector3D origin = drag_snapshot.position;
	const QVector3D w = r.origin - origin;

	const float a = QVector3D::dotProduct(axis_direction, axis_direction);
	const float b = QVector3D::dotProduct(axis_direction, r.direction);
	const float c = QVector3D::dotProduct(r.direction, r.direction);
	const float d = QVector3D::dotProduct(axis_direction, w);
	const float e = QVector3D::dotProduct(r.direction, w);

	const float determinant = a * c - b * b;
	if (std::abs(determinant) < 1e-10f) {
		return origin;
	}

	const float t_axis = (c * d - b * e) / determinant;
	return origin + axis_direction * t_axis;
}

float lidar_viewport::compute_rotation_angle(const ray &r, gizmo_axis axis) const {
	QVector3D normal;
	switch (axis) {
	case gizmo_axis::x: normal = QVector3D(1, 0, 0); break;
	case gizmo_axis::y: normal = QVector3D(0, 1, 0); break;
	case gizmo_axis::z: normal = QVector3D(0, 0, 1); break;
	default: return 0.0f;
	}

	const QVector3D origin = drag_snapshot.position;
	const float determinant = QVector3D::dotProduct(r.direction, normal);
	if (std::abs(determinant) < 1e-6f) {
		return 0.0f;
	}

	const float t = QVector3D::dotProduct(origin - r.origin, normal) / determinant;
	const QVector3D hit = r.origin + r.direction * t;
	const QVector3D direction = (hit - origin).normalized();

	QVector3D ref_x, ref_y;
	switch (axis) {
	case gizmo_axis::x:
		ref_x = QVector3D(0, 1, 0);
		ref_y = QVector3D(0, 0, 1);
		break;
	case gizmo_axis::y:
		ref_x = QVector3D(0, 0, 1);
		ref_y = QVector3D(1, 0, 0);
		break;
	case gizmo_axis::z:
		ref_x = QVector3D(1, 0, 0);
		ref_y = QVector3D(0, 1, 0);
		break;
	default: return 0.0f;
	}

	return std::atan2(QVector3D::dotProduct(direction, ref_y), QVector3D::dotProduct(direction, ref_x));
}
