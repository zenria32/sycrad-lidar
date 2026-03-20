#pragma once

#include "octree_node.h"

#include <QMatrix4x4>
#include <QSize>

#include <cstdint>
#include <vector>

class prepare_cmd_buffer {
	public:
	struct config {
		float sse_threshold_pixels = 90.0f;
		uint32_t max_points_budget = 12'000'000;
	};

	prepare_cmd_buffer() = default;
	explicit prepare_cmd_buffer(config cfg) : mcfg(cfg) {}

	void traverse(const spatial::octree &tree, const QMatrix4x4 &vp_matrix, const QSize &viewport_size, float fov_y_degrees);
	const std::vector<spatial::draw_indexed_command_buffer> &get_draw_commands() const { return draw_commands; }

	uint32_t get_visible_points() const { return visible_points; }
	uint32_t get_visible_node_count() const { return visible_node_count; }

	config &cfg() { return mcfg; }

	private:
	struct plane {
		float nx, ny, nz, d;
	};

	struct traversal_state {
		const spatial::octree_node *nodes;
		plane planes[6];
		float sse_factor;
		uint32_t *budget;
	};

	static void extract_frustum_planes(const QMatrix4x4 &vp, plane out[6]);
	static bool is_outside_frustum(const plane planes[6], const float min[3], const float max[3]);

	static float get_screen_pixels(const float min[3], const float max[3], const QMatrix4x4 &vp, float sse_factor);

	void recurse(const traversal_state &ts, int32_t node_index, const QMatrix4x4 &vp);

	config mcfg;
	std::vector<spatial::draw_indexed_command_buffer> draw_commands;
	uint32_t visible_points = 0;
	uint32_t visible_node_count = 0;
};
