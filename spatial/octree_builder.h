#pragma once

#include "octree_node.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace spatial {

class octree_builder {
	public:
	static constexpr uint32_t DEFAULT_MAX_LEAF_POINTS = 4096;
	static constexpr uint8_t DEFAULT_MAX_DEPTH = 14;
	static constexpr uint32_t DEFAULT_INTERNAL_SAMPLES = 2048;

	struct config {
		uint32_t max_leaf_points = DEFAULT_MAX_LEAF_POINTS;
		uint8_t max_depth = DEFAULT_MAX_DEPTH;
		uint32_t internal_samples = DEFAULT_INTERNAL_SAMPLES;
		uint32_t rng_seed = 42;
	};

	octree_builder() = default;
	explicit octree_builder(const config cfg) : mcfg(cfg) {}

	octree build(const float *x, const float *y, const float *z, uint32_t point_count);

	private:
	struct build_context {
		const float *x;
		const float *y;
		const float *z;
		octree *tree;
		std::mt19937 rng;

		std::vector<uint32_t> scratch;
	};

	int32_t build_node(build_context &context, uint32_t *begin, uint32_t *end, float region_min[3], float region_max[3], uint8_t depth);
	static void compute_aabb(const float *x, const float *y, const float *z, const uint32_t *begin, const uint32_t *end, float aabb_min[3], float aabb_max[3]);
	static uint32_t append_sample(build_context &context, const uint32_t *begin, const uint32_t *end, uint32_t max_samples);

	config mcfg;
};

}
