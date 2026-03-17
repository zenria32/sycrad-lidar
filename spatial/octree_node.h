#pragma once

#include <cstdint>
#include <vector>

namespace spatial {

struct alignas(4) draw_indexed_command_buffer {
	uint32_t index_count = 0;
	uint32_t instance_count = 1;
	uint32_t first_index = 0;
	int32_t vertex_offset = 0;
	uint32_t first_instance = 0;
};
static_assert(sizeof(draw_indexed_command_buffer) == 20, "draw_indexed_command_buffer must be exactly 20 bytes for GPU buffer");

struct octree_node {
	float aabb_min[3];
	float aabb_max[3];

	uint32_t first_index;
	uint32_t point_count;

	int32_t children[8];

	uint8_t depth;
	bool is_leaf;
	uint8_t padding[2] = {}; // explicit 2 bytes zero padding
};
static_assert(sizeof(octree_node) == 68);

struct octree {
	std::vector<octree_node> nodes;
	std::vector<uint32_t> point_indices;

	bool empty() const { return nodes.empty(); }
	const octree_node &root() const { return nodes[0]; }
};

}
