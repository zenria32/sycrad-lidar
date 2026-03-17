#include "prepare_cmd_buffer.h"

#include <cmath>
#include <cstring>
#include <stack>

void prepare_cmd_buffer::traverse(const spatial::octree &tree, const QMatrix4x4 &vp_matrix, const QSize &viewport_size, float fov_y_degrees) {
	draw_commands.clear();
	visible_points = 0;
	visible_node_count = 0;

	if (tree.empty()) {
		return;
	}

	const float fov_rad = fov_y_degrees * 3.14159265f / 180.0f;
	const float sse_factor = static_cast<float>(viewport_size.height()) / (2.0f * std::tan(fov_rad * 0.5f));

	traversal_state ts{};
	ts.nodes = tree.nodes.data();
	ts.sse_factor = sse_factor;
	extract_frustum_planes(vp_matrix, ts.planes);

	uint32_t budget = (mcfg.max_points_budget > 0) ? mcfg.max_points_budget : 0xFFFF'FFFFu;
	ts.budget = &budget;

	recurse(ts, 0, vp_matrix);
}

void prepare_cmd_buffer::recurse(const traversal_state &ts, int32_t node_index, const QMatrix4x4 &vp) {
	if (node_index < 0 || *ts.budget == 0) {
		return;
	}

	const spatial::octree_node &node = ts.nodes[node_index];

	if (is_outside_frustum(ts.planes, node.aabb_min, node.aabb_max)) {
		return;
	}

	if (node.is_leaf) {
		if (node.point_count == 0) {
			return;
		}

		const uint32_t draw_count = std::min(node.point_count, *ts.budget);
		spatial::draw_indexed_command_buffer command_buffer;
		command_buffer.index_count = draw_count;
		command_buffer.instance_count = 1;
		command_buffer.first_index = node.first_index;
		command_buffer.vertex_offset = 0;
		command_buffer.first_instance = 0;
		draw_commands.push_back(command_buffer);

		*ts.budget -= draw_count;
		visible_points += draw_count;
		++visible_node_count;
		return;
	}

	if (node.point_count > 0) {
		const float pixel_size = get_screen_pixels(node.aabb_min, node.aabb_max, vp, ts.sse_factor);
		if (pixel_size < mcfg.sse_threshold_pixels) {
			const uint32_t draw_count = std::min(node.point_count, *ts.budget);
			spatial::draw_indexed_command_buffer command_buffer;
			command_buffer.index_count = draw_count;
			command_buffer.instance_count = 1;
			command_buffer.first_index = node.first_index;
			command_buffer.vertex_offset = 0;
			command_buffer.first_instance = 0;
			draw_commands.push_back(command_buffer);

			*ts.budget -= draw_count;
			visible_points += draw_count;
			++visible_node_count;
			return;
		}
	}

	for (int i : node.children) {
		if (i >= 0) {
			recurse(ts, i, vp);
		}
		if (*ts.budget == 0) {
			break;
		}
	}
}

void prepare_cmd_buffer::extract_frustum_planes(const QMatrix4x4 &vp, plane out[6]) {
	const float *mx = vp.constData();

	out[0] = {mx[3] + mx[0], mx[7] + mx[4], mx[11] + mx[8], mx[15] + mx[12]};
	out[1] = {mx[3] - mx[0], mx[7] - mx[4], mx[11] - mx[8], mx[15] - mx[12]};
	out[2] = {mx[3] + mx[1], mx[7] + mx[5], mx[11] + mx[9], mx[15] + mx[13]};
	out[3] = {mx[3] - mx[1], mx[7] - mx[5], mx[11] - mx[9], mx[15] - mx[13]};
	out[4] = {mx[3] + mx[2], mx[7] + mx[6], mx[11] + mx[10], mx[15] + mx[14]};
	out[5] = {mx[3] - mx[2], mx[7] - mx[6], mx[11] - mx[10], mx[15] - mx[14]};

	for (int i = 0; i < 6; ++i) {
		const float len = std::sqrt(out[i].nx * out[i].nx + out[i].ny * out[i].ny + out[i].nz * out[i].nz);
		if (len > 0.0f) {
			const float inverse = 1.0f / len;
			out[i].nx *= inverse;
			out[i].ny *= inverse;
			out[i].nz *= inverse;
			out[i].d *= inverse;
		}
	}
}

bool prepare_cmd_buffer::is_outside_frustum(const plane planes[6], const float min[3], const float max[3]) {
	for (int i = 0; i < 6; ++i) {
		const plane &p = planes[i];
		const float px = (p.nx >= 0.0f) ? max[0] : min[0];
		const float py = (p.ny >= 0.0f) ? max[1] : min[1];
		const float pz = (p.nz >= 0.0f) ? max[2] : min[2];
		if (p.nx * px + p.ny * py + p.nz * pz + p.d < 0.0f) {
			return true;
		}
	}
	return false;
}

float prepare_cmd_buffer::get_screen_pixels(const float min[3], const float max[3], const QMatrix4x4 &vp, float sse_factor) {
	const float cx = (min[0] + max[0]) * 0.5f;
	const float cy = (min[1] + max[1]) * 0.5f;
	const float cz = (min[2] + max[2]) * 0.5f;

	const float dx = max[0] - min[0];
	const float dy = max[1] - min[1];
	const float dz = max[2] - min[2];
	const float radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;

	const QVector4D clip = vp.map(QVector4D(cx, cy, cz, 1.0f));
	const float w = (clip.w() > 0.001f) ? clip.w() : 0.001f;

	return (radius / w) * sse_factor;
}
