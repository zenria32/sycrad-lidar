#include "octree_builder.h"

#include <QtConcurrent>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <numeric>

namespace spatial {

octree octree_builder::build(const float *x, const float *y, const float *z, uint32_t point_count) {
	if (point_count == 0) {
		return {};
	}

	std::vector<uint32_t> scratch(point_count);
	std::iota(scratch.begin(), scratch.end(), 0u);

	float root_min[3], root_max[3];
	compute_aabb(x, y, z, scratch.data(), scratch.data() + point_count, root_min, root_max);

	if (point_count >= MULTITHREAD_THRESHOLD) {
		return multithread_build(x, y, z, point_count, scratch, root_min, root_max);
	}

	octree tree;

	tree.nodes.reserve(point_count / (mcfg.max_leaf_points / 2));
	tree.point_indices.reserve(point_count);

	build_context context;
	context.x = x;
	context.y = y;
	context.z = z;
	context.tree = &tree;
	context.rng = std::mt19937(mcfg.rng_seed);

	context.scratch = std::move(scratch);

	build_node(context, context.scratch.data(), context.scratch.data() + point_count, root_min, root_max, 0);

	tree.nodes.shrink_to_fit();
	tree.point_indices.shrink_to_fit();
	return tree;
}

octree octree_builder::multithread_build(const float *x, const float *y, const float *z, uint32_t point_count, std::vector<uint32_t> &scratch,
	const float root_min[3], const float root_max[3]) {

	uint32_t *base = scratch.data();

	const float mid[3] = {
		(root_min[0] + root_max[0]) * 0.5f,
		(root_min[1] + root_max[1]) * 0.5f,
		(root_min[2] + root_max[2]) * 0.5f,
	};

	uint32_t counts[8] = {};
	for (uint32_t index = 0; index < point_count; ++index) {
		const uint32_t i = base[index];
		const int oct = ((x[i] >= mid[0]) ? 1 : 0) | ((y[i] >= mid[1]) ? 2 : 0) | ((z[i] >= mid[2]) ? 4 : 0);
		++counts[oct];
	}

	uint32_t offsets[9] = {};
	for (int i = 0; i < 8; ++i) {
		offsets[i + 1] = offsets[i] + counts[i];
	}

	std::vector<uint32_t> temp(point_count);
	uint32_t pos[8];
	for (int i = 0; i < 8; ++i) {
		pos[i] = offsets[i];
	}
	for (uint32_t index = 0; index < point_count; ++index) {
		const uint32_t i = base[index];
		const int oct = ((x[i] >= mid[0]) ? 1 : 0) | ((y[i] >= mid[1]) ? 2 : 0) | ((z[i] >= mid[2]) ? 4 : 0);
		temp[pos[oct]++] = i;
	}
	std::memcpy(base, temp.data(), point_count * sizeof(uint32_t));
	temp.clear();
	temp.shrink_to_fit();

	struct subtree_result {
		octree tree;
		int octant;
	};

	QThreadPool pool;
	std::vector<QFuture<subtree_result>> futures;
	futures.reserve(8);

	for (int i = 0; i < 8; ++i) {
		if (counts[i] == 0) {
			continue;
		}

		uint32_t *oct_begin = base + offsets[i];
		uint32_t *oct_end = base + offsets[i + 1];

		float oct_min[3], oct_max[3];
		compute_aabb(x, y, z, oct_begin, oct_end, oct_min, oct_max);

		const std::array<float, 3> mins = {oct_min[0], oct_min[1], oct_min[2]};
		const std::array<float, 3> maxs = {oct_max[0], oct_max[1], oct_max[2]};

		futures.push_back(QtConcurrent::run(&pool, [this, x, y, z, oct_begin, oct_end, mins, maxs, i]() -> subtree_result {
			const auto sub_tree_count = static_cast<uint32_t>(oct_end - oct_begin);
			octree sub_tree;
			sub_tree.nodes.reserve(sub_tree_count / (mcfg.max_leaf_points / 2));
			sub_tree.point_indices.reserve(sub_tree_count);

			build_context context;
			context.x = x;
			context.y = y;
			context.z = z;
			context.tree = &sub_tree;
			context.rng = std::mt19937(mcfg.rng_seed + static_cast<uint32_t>(i) + 1);

			float node_min[3] = {mins[0], mins[1], mins[2]};
			float node_max[3] = {maxs[0], maxs[1], maxs[2]};

			build_node(context, oct_begin, oct_end, node_min, node_max, 1);

			sub_tree.nodes.shrink_to_fit();
			sub_tree.point_indices.shrink_to_fit();
			return {std::move(sub_tree), i};
		}));
	}

	std::vector<subtree_result> results;
	results.reserve(futures.size());

	uint32_t total_nodes = 1;
	uint32_t total_indices = 0;
	for (auto &f : futures) {
		results.push_back(f.takeResult());
		total_nodes += static_cast<uint32_t>(results.back().tree.nodes.size());
		total_indices += static_cast<uint32_t>(results.back().tree.point_indices.size());
	}
	total_indices += std::min(point_count, mcfg.internal_samples);

	octree tree;
	tree.nodes.reserve(total_nodes);
	tree.point_indices.reserve(total_indices);

	tree.nodes.push_back(octree_node{});

	int32_t child_indices[8];
	for (int &ci : child_indices) {
		ci = -1;
	}

	for (auto &result : results) {
		const auto node_offset = static_cast<int32_t>(tree.nodes.size());
		const auto index_offset = static_cast<uint32_t>(tree.point_indices.size());

		for (auto &node : result.tree.nodes) {
			node.first_index += index_offset;
			for (int32_t &child : node.children) {
				if (child >= 0) {
					child += node_offset;
				}
			}
		}

		tree.nodes.insert(tree.nodes.end(), std::make_move_iterator(result.tree.nodes.begin()), std::make_move_iterator(result.tree.nodes.end()));
		tree.point_indices.insert(tree.point_indices.end(), result.tree.point_indices.begin(), result.tree.point_indices.end());

		child_indices[result.octant] = node_offset;
	}

	build_context root_context;
	root_context.x = x;
	root_context.y = y;
	root_context.z = z;
	root_context.tree = &tree;
	root_context.rng = std::mt19937(mcfg.rng_seed);

	const uint32_t root_first_index = append_sample(root_context, base, base + point_count, mcfg.internal_samples);
	const uint32_t root_point_count = static_cast<uint32_t>(tree.point_indices.size()) - root_first_index;

	octree_node &root = tree.nodes[0];
	root.aabb_min[0] = root_min[0];
	root.aabb_min[1] = root_min[1];
	root.aabb_min[2] = root_min[2];
	root.aabb_max[0] = root_max[0];
	root.aabb_max[1] = root_max[1];
	root.aabb_max[2] = root_max[2];
	root.depth = 0;
	root.is_leaf = false;
	root.first_index = root_first_index;
	root.point_count = root_point_count;
	for (int i = 0; i < 8; ++i) {
		root.children[i] = child_indices[i];
	}

	tree.nodes.shrink_to_fit();
	tree.point_indices.shrink_to_fit();
	return tree;
}

int32_t octree_builder::build_node(build_context &context, uint32_t *begin, uint32_t *end, float region_min[3], float region_max[3], uint8_t depth) {
	const auto count = static_cast<uint32_t>(end - begin);
	assert(count > 0);

	const auto node_index = static_cast<int32_t>(context.tree->nodes.size());
	context.tree->nodes.push_back(octree_node{});
	octree_node &node = context.tree->nodes[node_index];

	node.aabb_min[0] = region_min[0];
	node.aabb_min[1] = region_min[1];
	node.aabb_min[2] = region_min[2];
	node.aabb_max[0] = region_max[0];
	node.aabb_max[1] = region_max[1];
	node.aabb_max[2] = region_max[2];
	node.depth = depth;
	for (int &i : node.children) {
		i = -1;
	}

	const bool is_leaf = (count <= mcfg.max_leaf_points || depth >= mcfg.max_depth);

	if (is_leaf) {
		std::shuffle(begin, end, context.rng);
		const auto first_index = static_cast<uint32_t>(context.tree->point_indices.size());
		context.tree->point_indices.insert(context.tree->point_indices.end(), begin, end);
		octree_node &leaf = context.tree->nodes[node_index];
		leaf.first_index = first_index;
		leaf.point_count = count;
		leaf.is_leaf = true;
		return node_index;
	}

	const float mid[3] = {
		(region_min[0] + region_max[0]) * 0.5f,
		(region_min[1] + region_max[1]) * 0.5f,
		(region_min[2] + region_max[2]) * 0.5f,
	};

	uint32_t counts[8] = {};
	for (const uint32_t *ptr = begin; ptr != end; ++ptr) {
		const uint32_t i = *ptr;
		const int oct = ((context.x[i] >= mid[0]) ? 1 : 0) | ((context.y[i] >= mid[1]) ? 2 : 0) | ((context.z[i] >= mid[2]) ? 4 : 0);
		++counts[oct];
	}

	uint32_t offsets[9] = {};
	for (int i = 0; i < 8; ++i) {
		offsets[i + 1] = offsets[i] + counts[i];
	}

	std::vector<uint32_t> temp(count);
	uint32_t pos[8] = {};
	for (int i = 0; i < 8; ++i) {
		pos[i] = offsets[i];
	}

	for (const uint32_t *ptr = begin; ptr != end; ++ptr) {
		const uint32_t i = *ptr;
		const int oct = ((context.x[i] >= mid[0]) ? 1 : 0) | ((context.y[i] >= mid[1]) ? 2 : 0) | ((context.z[i] >= mid[2]) ? 4 : 0);
		temp[pos[oct]++] = i;
	}
	std::memcpy(begin, temp.data(), count * sizeof(uint32_t));

	int32_t child_indices[8];
	for (int &i : child_indices) {
		i = -1;
	}

	for (int i = 0; i < 8; ++i) {
		if (counts[i] == 0) {
			continue;
		}

		uint32_t *oct_begin = begin + offsets[i];
		uint32_t *oct_end = begin + offsets[i + 1];

		float oct_min[3] = {
			(i & 1) ? mid[0] : region_min[0],
			(i & 2) ? mid[1] : region_min[1],
			(i & 4) ? mid[2] : region_min[2],
		};
		float oct_max[3] = {
			(i & 1) ? region_max[0] : mid[0],
			(i & 2) ? region_max[1] : mid[1],
			(i & 4) ? region_max[2] : mid[2],
		};

		compute_aabb(context.x, context.y, context.z, oct_begin, oct_end, oct_min, oct_max);
		child_indices[i] = build_node(context, oct_begin, oct_end, oct_min, oct_max, depth + 1);
	}

	const uint32_t first_index = append_sample(context, begin, end, mcfg.internal_samples);
	const uint32_t written = static_cast<uint32_t>(context.tree->point_indices.size()) - first_index;

	octree_node &rnode = context.tree->nodes[node_index];
	rnode.first_index = first_index;
	rnode.point_count = written;
	rnode.is_leaf = false;
	for (int i = 0; i < 8; ++i) {
		rnode.children[i] = child_indices[i];
	}

	return node_index;
}

void octree_builder::compute_aabb(const float *x, const float *y, const float *z, const uint32_t *begin, const uint32_t *end, float aabb_min[3], float aabb_max[3]) {
	aabb_min[0] = aabb_min[1] = aabb_min[2] = std::numeric_limits<float>::max();
	aabb_max[0] = aabb_max[1] = aabb_max[2] = -std::numeric_limits<float>::max();

	for (const uint32_t *ptr = begin; ptr != end; ++ptr) {
		const uint32_t i = *ptr;
		if (x[i] < aabb_min[0]) {
			aabb_min[0] = x[i];
		}
		if (y[i] < aabb_min[1]) {
			aabb_min[1] = y[i];
		}
		if (z[i] < aabb_min[2]) {
			aabb_min[2] = z[i];
		}
		if (x[i] > aabb_max[0]) {
			aabb_max[0] = x[i];
		}
		if (y[i] > aabb_max[1]) {
			aabb_max[1] = y[i];
		}
		if (z[i] > aabb_max[2]) {
			aabb_max[2] = z[i];
		}
	}
}

uint32_t octree_builder::append_sample(build_context &context, const uint32_t *begin, const uint32_t *end, uint32_t max_samples) {
	const auto first_index = static_cast<uint32_t>(context.tree->point_indices.size());
	const auto count = static_cast<uint32_t>(end - begin);

	if (count <= max_samples) {
		std::vector<uint32_t> temp(begin, end);
		std::shuffle(temp.begin(), temp.end(), context.rng);
		context.tree->point_indices.insert(context.tree->point_indices.end(), temp.begin(), temp.end());
	} else {
		std::vector<uint32_t> reservoir(begin, begin + max_samples);
		std::uniform_int_distribution<uint32_t> dist;
		for (uint32_t i = max_samples; i < count; ++i) {
			dist.param(std::uniform_int_distribution<uint32_t>::param_type(0, i));
			const uint32_t j = dist(context.rng);
			if (j < max_samples) {
				reservoir[j] = begin[i];
			}
		}
		std::shuffle(reservoir.begin(), reservoir.end(), context.rng);
		context.tree->point_indices.insert(context.tree->point_indices.end(), reservoir.begin(), reservoir.end());
	}

	return first_index;
}

}
