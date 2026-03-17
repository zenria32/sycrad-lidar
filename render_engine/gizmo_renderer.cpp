#include "gizmo_renderer.h"
#include "orbital_camera.h"

#include <cmath>
#include <cstring>

#ifndef PI
#define PI 3.14159265358979323846
#endif

struct alignas(16) gizmo_uniform {
	float mvp[16];
	float color[4];
	float world_scale;
	float pad[3];
};
static_assert(sizeof(gizmo_uniform) == gizmo_renderer::uniform_size);

gizmo_renderer::gizmo_renderer() = default;
gizmo_renderer::~gizmo_renderer() { release_resources(); }

void gizmo_renderer::release_resources() {
	for (int i = 0; i < 3; ++i) {
		axis_shader_resource[i].reset();
		axis_uniform[i].reset();
	}

	abstract_renderer::release_resources();
	vertex_buffer.reset();
	index_buffer.reset();
	is_geometry_built = false;
}

void gizmo_renderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) {
	if (!initialize_abstract(rhi, render_pass, viewport_size)) {
		return;
	}

	const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
	for (int i = 0; i < 3; ++i) {
		axis_uniform[i].reset(this->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniform_size));
		if (!axis_uniform[i]->create()) {
			report(QStringLiteral("Gizmo renderer: axis uniform buffer creation failed."));
			return;
		}
		axis_shader_resource[i].reset(this->rhi->newShaderResourceBindings());
		axis_shader_resource[i]->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, axis_uniform[i].get())});
		if (!axis_shader_resource[i]->create()) {
			report(QStringLiteral("Gizmo renderer: axis SRB creation failed."));
			return;
		}
	}

	build_pipeline();
	build_move_geometry();
	build_rotate_geometry();
	build_scale_geometry();
	upload_geometry();
}

void gizmo_renderer::build_pipeline() {
	if (!is_ready()) {
		return;
	}

	const QShader vert = load_shader(QStringLiteral(":/shaders/gizmo.vert.qsb"));
	const QShader frag = load_shader(QStringLiteral(":/shaders/gizmo.frag.qsb"));

	if (!vert.isValid() || !frag.isValid()) {
		report(QStringLiteral("Gizmo renderer: shader load failed."));
		return;
	}

	pipeline.reset(rhi->newGraphicsPipeline());
	pipeline->setShaderStages({{QRhiShaderStage::Vertex, vert}, {QRhiShaderStage::Fragment, frag}});

	constexpr quint32 stride = sizeof(gizmo_vertex);
	QRhiVertexInputLayout input_layout;
	input_layout.setBindings({{stride}});
	input_layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},});

	pipeline->setVertexInputLayout(input_layout);
	pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
	pipeline->setDepthTest(false);
	pipeline->setDepthWrite(false);
	pipeline->setCullMode(QRhiGraphicsPipeline::None);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = false;
	pipeline->setTargetBlends({blend});

	pipeline->setShaderResourceBindings(axis_shader_resource[0].get());
	pipeline->setRenderPassDescriptor(render_pass);

	if (!pipeline->create()) {
		report(QStringLiteral("Gizmo renderer: pipeline creation failed."));
		pipeline.reset();
	}
}

void gizmo_renderer::generate_cylinder(const QVector3D &start, const QVector3D &end, float radius,
	int segments, std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices) {

	const QVector3D axis = (end - start).normalized();
	QVector3D perpendicular;
	if (std::abs(axis.x()) < 0.9f) {
		perpendicular = QVector3D::crossProduct(axis, QVector3D(1, 0, 0)).normalized();
	} else {
		perpendicular = QVector3D::crossProduct(axis, QVector3D(0, 1, 0)).normalized();
	}
	const QVector3D binorm = QVector3D::crossProduct(axis, perpendicular).normalized();

	const auto base_index = static_cast<uint32_t>(vertices.size());

	for (int i = 0; i < segments; ++i) {
		const float angle = 2.0f * static_cast<float>(PI) * static_cast<float>(i) / static_cast<float>(segments);
		const float cos = std::cos(angle);
		const float sin = std::sin(angle);
		const QVector3D offset = (perpendicular * cos + binorm * sin) * radius;

		const QVector3D p0 = start + offset;
		const QVector3D p1 = end + offset;
		vertices.push_back({{p0.x(), p0.y(), p0.z()}});
		vertices.push_back({{p1.x(), p1.y(), p1.z()}});
	}

	for (int i = 0; i < segments; ++i) {
		const uint32_t i0 = base_index + static_cast<uint32_t>(i) * 2;
		const uint32_t i1 = i0 + 1;
		const uint32_t i2 = base_index + static_cast<uint32_t>((i + 1) % segments) * 2;
		const uint32_t i3 = i2 + 1;

		indices.push_back(i0);
		indices.push_back(i1);
		indices.push_back(i2);

		indices.push_back(i2);
		indices.push_back(i1);
		indices.push_back(i3);
	}
}

void gizmo_renderer::generate_cone(const QVector3D &base_center, const QVector3D &tip, float radius,
	int segments, std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices) {

	const QVector3D axis = (tip - base_center).normalized();
	QVector3D perpendicular;
	if (std::abs(axis.x()) < 0.9f) {
		perpendicular = QVector3D::crossProduct(axis, QVector3D(1, 0, 0)).normalized();
	} else {
		perpendicular = QVector3D::crossProduct(axis, QVector3D(0, 1, 0)).normalized();
	}
	const QVector3D binorm = QVector3D::crossProduct(axis, perpendicular).normalized();

	const uint32_t base_index = static_cast<uint32_t>(vertices.size());

	vertices.push_back({{tip.x(), tip.y(), tip.z()}});

	for (int i = 0; i < segments; ++i) {
		const float angle = 2.0f * static_cast<float>(PI) * static_cast<float>(i) / static_cast<float>(segments);
		const float cos = std::cos(angle);
		const float sin = std::sin(angle);
		const QVector3D p = base_center + (perpendicular * cos + binorm * sin) * radius;
		vertices.push_back({{p.x(), p.y(), p.z()}});
	}

	for (int i = 0; i < segments; ++i) {
		const auto next = static_cast<uint32_t>((i + 1) % segments);
		indices.push_back(base_index);
		indices.push_back(base_index + 1 + static_cast<uint32_t>(i));
		indices.push_back(base_index + 1 + next);
	}

	const auto center_index = static_cast<uint32_t>(vertices.size());
	vertices.push_back({{base_center.x(), base_center.y(), base_center.z()}});

	for (int i = 0; i < segments; ++i) {
		const auto next = static_cast<uint32_t>((i + 1) % segments);
		indices.push_back(center_index);
		indices.push_back(base_index + 1 + next);
		indices.push_back(base_index + 1 + static_cast<uint32_t>(i));
	}
}

void gizmo_renderer::generate_cube(const QVector3D &center, float half_size,
	std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices) {

	const auto base_index = static_cast<uint32_t>(vertices.size());
	const float h = half_size;

	const QVector3D corners[8] = {
		center + QVector3D(-h, -h, -h), center + QVector3D(+h, -h, -h),
		center + QVector3D(+h, +h, -h), center + QVector3D(-h, +h, -h),
		center + QVector3D(-h, -h, +h), center + QVector3D(+h, -h, +h),
		center + QVector3D(+h, +h, +h), center + QVector3D(-h, +h, +h),
	};

	for (auto corner : corners) {
		vertices.push_back({{corner.x(), corner.y(), corner.z()}});
	}

	static constexpr uint32_t cube_indices[36] = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		0, 4, 7, 7, 3, 0,
		1, 5, 6, 6, 2, 1,
		3, 7, 6, 6, 2, 3,
		0, 4, 5, 5, 1, 0,
	};

	for (uint32_t index : cube_indices) {
		indices.push_back(base_index + index);
	}
}

void gizmo_renderer::generate_arc(const QVector3D &plane_u, const QVector3D &plane_v,
	float radius, float tube_radius, int arc_segments, int tube_segments,
	std::vector<gizmo_vertex> &vertices, std::vector<uint32_t> &indices) {

	const QVector3D plane_normal = QVector3D::crossProduct(plane_u, plane_v).normalized();
	const auto base_index = static_cast<uint32_t>(vertices.size());
	constexpr float sweep = static_cast<float>(PI) * 0.5f;

	for (int i = 0; i <= arc_segments; ++i) {
		const float theta = sweep * static_cast<float>(i) / static_cast<float>(arc_segments);
		const float cos = std::cos(theta);
		const float sin = std::sin(theta);
		const QVector3D center_on_arc = (plane_u * cos + plane_v * sin) * radius;
		const QVector3D radial = center_on_arc.normalized();

		for (int j = 0; j < tube_segments; ++j) {
			const float phi = 2.0f * static_cast<float>(PI) * static_cast<float>(j) / static_cast<float>(tube_segments);
			const QVector3D offset = (radial * std::cos(phi) + plane_normal * std::sin(phi)) * tube_radius;
			const QVector3D p = center_on_arc + offset;
			vertices.push_back({{p.x(), p.y(), p.z()}});
		}
	}

	for (int i = 0; i < arc_segments; ++i) {
		for (int j = 0; j < tube_segments; ++j) {
			const int next_j = (j + 1) % tube_segments;
			const uint32_t a = base_index + static_cast<uint32_t>(i * tube_segments + j);
			const uint32_t b = base_index + static_cast<uint32_t>((i + 1) * tube_segments + j);
			const uint32_t c = base_index + static_cast<uint32_t>((i + 1) * tube_segments + next_j);
			const uint32_t d = base_index + static_cast<uint32_t>(i * tube_segments + next_j);

			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);

			indices.push_back(a);
			indices.push_back(c);
			indices.push_back(d);
		}
	}
}

void gizmo_renderer::build_move_geometry() {
	auto build_arrow = [this](const QVector3D &direction, axis_mesh &mesh) {
		mesh.vertex_offset = static_cast<quint32>(all_vertices.size());
		mesh.index_offset = static_cast<quint32>(all_indices.size());

		const QVector3D shaft_end = direction * shaft_length;
		const QVector3D cone_base = shaft_end;
		const QVector3D cone_tip = direction * (shaft_length + cone_length);

		generate_cylinder(QVector3D(0, 0, 0), shaft_end, shaft_radius, cylinder_segments, all_vertices, all_indices);
		generate_cone(cone_base, cone_tip, cone_radius, cylinder_segments, all_vertices, all_indices);

		mesh.vertex_count = static_cast<quint32>(all_vertices.size()) - mesh.vertex_offset;
		mesh.index_count = static_cast<quint32>(all_indices.size()) - mesh.index_offset;
	};

	build_arrow(QVector3D(1, 0, 0), move_x);
	build_arrow(QVector3D(0, 1, 0), move_y);
	build_arrow(QVector3D(0, 0, 1), move_z);
}

void gizmo_renderer::build_rotate_geometry() {
	auto build_arc = [this](const QVector3D &u, const QVector3D &v, axis_mesh &mesh) {
		mesh.vertex_offset = static_cast<quint32>(all_vertices.size());
		mesh.index_offset = static_cast<quint32>(all_indices.size());

		generate_arc(u, v, arc_radius, arc_tube_radius, arc_segments, tube_segments, all_vertices, all_indices);

		mesh.vertex_count = static_cast<quint32>(all_vertices.size()) - mesh.vertex_offset;
		mesh.index_count = static_cast<quint32>(all_indices.size()) - mesh.index_offset;
	};

	build_arc(QVector3D(0, 1, 0), QVector3D(0, 0, 1), rotate_x); //yz
	build_arc(QVector3D(1, 0, 0), QVector3D(0, 0, 1), rotate_y); //xz
	build_arc(QVector3D(1, 0, 0), QVector3D(0, 1, 0), rotate_z); //xy
}

void gizmo_renderer::build_scale_geometry() {
	auto build_scale_arrow = [this](const QVector3D &dir, axis_mesh &mesh) {
		mesh.vertex_offset = static_cast<quint32>(all_vertices.size());
		mesh.index_offset = static_cast<quint32>(all_indices.size());

		const QVector3D shaft_end = dir * shaft_length;
		const QVector3D cube_center = dir * (shaft_length + cube_half_size);

		generate_cylinder(QVector3D(0, 0, 0), shaft_end, shaft_radius, cylinder_segments, all_vertices, all_indices);
		generate_cube(cube_center, cube_half_size, all_vertices, all_indices);

		mesh.vertex_count = static_cast<quint32>(all_vertices.size()) - mesh.vertex_offset;
		mesh.index_count = static_cast<quint32>(all_indices.size()) - mesh.index_offset;
	};

	build_scale_arrow(QVector3D(1, 0, 0), scale_x);
	build_scale_arrow(QVector3D(0, 1, 0), scale_y);
	build_scale_arrow(QVector3D(0, 0, 1), scale_z);
}

void gizmo_renderer::upload_geometry() {
	if (!rhi || all_vertices.empty()) {
		return;
	}

	const auto vertex_size = static_cast<quint32>(all_vertices.size() * sizeof(gizmo_vertex));
	const auto index_size = static_cast<quint32>(all_indices.size() * sizeof(uint32_t));

	vertex_buffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, vertex_size));
	if (!vertex_buffer->create()) {
		report(QStringLiteral("Gizmo renderer: vertex buffer allocation failed."));
		return;
	}

	index_buffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, index_size));
	if (!index_buffer->create()) {
		report(QStringLiteral("Gizmo renderer: index buffer allocation failed."));
		return;
	}

	if (!pending_updates) {
		pending_updates = rhi->nextResourceUpdateBatch();
	}

	pending_updates->uploadStaticBuffer(vertex_buffer.get(), 0, vertex_size, all_vertices.data());
	pending_updates->uploadStaticBuffer(index_buffer.get(), 0, index_size, all_indices.data());

	is_geometry_built = true;
}

void gizmo_renderer::set_target(const QVector3D &position, const QQuaternion &rotation) {
	target_position = position;
	target_rotation = rotation;
}

QVector4D gizmo_renderer::get_axis_color(gizmo_axis axis) const {
	if (axis == dragging_axis && dragging_axis != gizmo_axis::none) {
		return {1.0f, 1.0f, 1.0f, 1.0f};
	}
	switch (axis) {
	case gizmo_axis::x: return {1.0f, 0.2f, 0.2f, 1.0f};
	case gizmo_axis::y: return {0.2f, 1.0f, 0.2f, 1.0f};
	case gizmo_axis::z: return {0.3f, 0.5f, 1.0f, 1.0f};
	default: return {0.5f, 0.5f, 0.5f, 1.0f};
	}
}

QVector3D gizmo_renderer::get_axis_direction(gizmo_axis axis) const {
	switch (axis) {
	case gizmo_axis::x: return {1, 0, 0};
	case gizmo_axis::y: return {0, 1, 0};
	case gizmo_axis::z: return {0, 0, 1};
	default: return {0, 0, 0};
	}
}

void gizmo_renderer::render_axis(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera, gizmo_axis axis, const axis_mesh &mesh) {
	if (mesh.index_count == 0) {
		return;
	}

	int index;
	switch (axis) {
	case gizmo_axis::x: index = 0; break;
	case gizmo_axis::y: index = 1; break;
	case gizmo_axis::z: index = 2; break;
	default: return;
	}

	if (!axis_uniform[index] || !axis_shader_resource[index]) {
		return;
	}

	QMatrix4x4 model;
	model.translate(target_position);

	const QMatrix4x4 vp = camera->vp_matrix();
	const QMatrix4x4 mvp = vp * model;

	const QVector4D color = get_axis_color(axis);

	gizmo_uniform uniform_data{};
	std::memcpy(uniform_data.mvp, mvp.constData(), sizeof(uniform_data.mvp));
	uniform_data.color[0] = color.x();
	uniform_data.color[1] = color.y();
	uniform_data.color[2] = color.z();
	uniform_data.color[3] = color.w();
	uniform_data.world_scale = world_scale;

	auto *updates = rhi->nextResourceUpdateBatch();
	updates->updateDynamicBuffer(axis_uniform[index].get(), 0, uniform_size, &uniform_data);
	command_buffer->resourceUpdate(updates);

	command_buffer->setGraphicsPipeline(pipeline.get());
	command_buffer->setViewport({0.0f, 0.0f, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height())});
	command_buffer->setShaderResources(axis_shader_resource[index].get());

	const QRhiCommandBuffer::VertexInput vb(vertex_buffer.get(), 0);
	command_buffer->setVertexInput(0, 1, &vb, index_buffer.get(),
	mesh.index_offset * static_cast<quint32>(sizeof(uint32_t)),
		QRhiCommandBuffer::IndexUInt32);
	command_buffer->drawIndexed(mesh.index_count);
}

void gizmo_renderer::render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) {
	if (!is_ready() || !is_visible() || !is_active || !pipeline || !is_geometry_built) {
		return;
	}

	if (pending_updates) {
		command_buffer->resourceUpdate(pending_updates);
		pending_updates = nullptr;
	}

	const axis_mesh *x_mesh = nullptr;
	const axis_mesh *y_mesh = nullptr;
	const axis_mesh *z_mesh = nullptr;

	switch (current_mode) {
	case gizmo_mode::move:
		x_mesh = &move_x;
		y_mesh = &move_y;
		z_mesh = &move_z;
		break;
	case gizmo_mode::rotate:
		x_mesh = &rotate_x;
		y_mesh = &rotate_y;
		z_mesh = &rotate_z;
		break;
	case gizmo_mode::scale:
		x_mesh = &scale_x;
		y_mesh = &scale_y;
		z_mesh = &scale_z;
		break;
	}

	if (x_mesh) render_axis(command_buffer, viewport_size, camera, gizmo_axis::x, *x_mesh);
	if (y_mesh) render_axis(command_buffer, viewport_size, camera, gizmo_axis::y, *y_mesh);
	if (z_mesh) render_axis(command_buffer, viewport_size, camera, gizmo_axis::z, *z_mesh);
}

float gizmo_renderer::closest_point_on_axis(const ray &r, const QVector3D &axis_direction) const {
	const QVector3D w = r.origin - target_position;
	const float a = QVector3D::dotProduct(axis_direction, axis_direction);
	const float b = QVector3D::dotProduct(axis_direction, r.direction);
	const float c = QVector3D::dotProduct(r.direction, r.direction);
	const float d = QVector3D::dotProduct(axis_direction, w);
	const float e = QVector3D::dotProduct(r.direction, w);

	const float determinant = a * c - b * b;
	if (std::abs(determinant) < 1e-10f) {
		return -1.0f;
	}

	const float t_axis = (c * d - b * e) / determinant;
	const float t_ray = (b * d - a * e) / determinant;

	if (t_ray < 0.0f) {
		return -1.0f;
	}

	const float max_t = shaft_length + cone_length;
	if (t_axis < -0.05f || t_axis > max_t + 0.05f) {
		return -1.0f;
	}

	const QVector3D closest_on_axis = target_position + axis_direction * t_axis;
	const QVector3D closest_on_ray = r.origin + r.direction * t_ray;
	const float distance = (closest_on_axis - closest_on_ray).length();

	return distance;
}

float gizmo_renderer::closest_point_on_arc(const ray &r, const QVector3D &plane_u, const QVector3D &plane_v, float radius) const {
	const QVector3D normal = QVector3D::crossProduct(plane_u, plane_v).normalized();
	const float determinant = QVector3D::dotProduct(r.direction, normal);
	if (std::abs(determinant) < 1e-6f) {
		return -1.0f;
	}

	const float t = QVector3D::dotProduct(target_position - r.origin, normal) / determinant;
	if (t < 0.0f) {
		return -1.0f;
	}

	const QVector3D hit = r.origin + r.direction * t;
	const QVector3D local = hit - target_position;
	const float distance_from_center = local.length();
	const float radius_error = std::abs(distance_from_center - radius);

	const float u_comp = QVector3D::dotProduct(local, plane_u);
	const float v_comp = QVector3D::dotProduct(local, plane_v);
	const float tolerance = radius * 0.15f;
	if (u_comp < -tolerance || v_comp < -tolerance) {
		return -1.0f;
	}

	return radius_error;
}

gizmo_axis gizmo_renderer::hit_test(const ray &r) const {
	const float ws = world_scale;
	const float threshold = ws * axis_hit_threshold;

	float best_distance = threshold;
	gizmo_axis best_axis = gizmo_axis::none;

	if (current_mode == gizmo_mode::move || current_mode == gizmo_mode::scale) {
		const gizmo_axis axes[] = {gizmo_axis::x, gizmo_axis::y, gizmo_axis::z};
		for (auto axis : axes) {
			const QVector3D direction = get_axis_direction(axis);
			const float distance = closest_point_on_axis(r, direction * ws);
			if (distance >= 0.0f && distance < best_distance) {
				best_distance = distance;
				best_axis = axis;
			}
		}
	} else if (current_mode == gizmo_mode::rotate) {
		const float scaled_radius = arc_radius * ws;

		float distance = closest_point_on_arc(r, QVector3D(0, 1, 0), QVector3D(0, 0, 1), scaled_radius); //yz
		if (distance >= 0.0f && distance < best_distance) { best_distance = distance; best_axis = gizmo_axis::x; }

		distance = closest_point_on_arc(r, QVector3D(1, 0, 0), QVector3D(0, 0, 1), scaled_radius); //xz
		if (distance >= 0.0f && distance < best_distance) { best_distance = distance; best_axis = gizmo_axis::y; }

		distance = closest_point_on_arc(r, QVector3D(1, 0, 0), QVector3D(0, 1, 0), scaled_radius); //xy
		if (distance >= 0.0f && distance < best_distance) { best_distance = distance; best_axis = gizmo_axis::z; }
	}

	return best_axis;
}
