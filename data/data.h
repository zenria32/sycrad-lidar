#pragma once

#include <QFile>

#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

enum class data_format {
	unknown,
	kitti,
	las,
	nuscenes,
	pcd,
	waymo
};

enum class coloration_format_for_las_pcd {
	unknown,
	xyz,
	xyzi,
	xyzrgb
};

enum class pcd_data_format {
	unknown,
	ascii,
	binary,
	binary_compressed
};

template <std::size_t floats_per_vertex_init>
struct data {
	std::vector<float> vertices;

	std::unique_ptr<QFile> mapped_file;
	uchar *mapped_ptr = nullptr;
	qint64 mapped_size = 0;

	std::size_t point_count = 0;

	struct data_bounds {
		float min_x = std::numeric_limits<float>::max();
		float max_x = std::numeric_limits<float>::lowest();
		float min_y = std::numeric_limits<float>::max();
		float max_y = std::numeric_limits<float>::lowest();
		float min_z = std::numeric_limits<float>::max();
		float max_z = std::numeric_limits<float>::lowest();
	} bounds;

	float min_intensity = std::numeric_limits<float>::max();
	float max_intensity = std::numeric_limits<float>::lowest();

	data_format format = data_format::unknown;

	~data() {
		if (mapped_ptr && mapped_file) {
			mapped_file->unmap(mapped_ptr);
		}
	}

	static constexpr std::size_t floats_per_vertex() noexcept {
		return floats_per_vertex_init;
	}

	static constexpr std::size_t stride() noexcept {
		return floats_per_vertex_init * sizeof(float);
	}

	std::size_t size_in_bytes() const noexcept {
		if (mapped_ptr) {
			return mapped_size;
		}
		return vertices.size() * sizeof(float);
	}

	bool is_valid() const noexcept {
		if (mapped_ptr) {
			return mapped_size > 0 && point_count > 0;
		}
		return vertices.size() / floats_per_vertex_init == point_count;
	}

	const void *access() const noexcept {
		if (mapped_ptr) {
			return mapped_ptr;
		}
		return vertices.data();
	}

	bool is_mapped() const noexcept { return mapped_ptr != nullptr; }

	void release_sources() {
		if (mapped_ptr && mapped_file) {
			mapped_file->unmap(mapped_ptr);
			mapped_ptr = nullptr;
			mapped_size = 0;
			mapped_file->close();
			mapped_file.reset();
		}
		vertices.clear();
		vertices.shrink_to_fit();
	}
};

struct kitti_data : data<4> {
	kitti_data() { format = data_format::kitti; }
};

struct las_data : data<4> {
	coloration_format_for_las_pcd color_format = coloration_format_for_las_pcd::unknown;
	float ground_level = 0.0f;

	las_data() { format = data_format::las; }
};

struct nuscenes_data : data<5> {
	float min_ring = std::numeric_limits<float>::max();
	float max_ring = std::numeric_limits<float>::lowest();

	nuscenes_data() { format = data_format::nuscenes; }
};

struct pcd_data : data<4> {
	coloration_format_for_las_pcd color_format = coloration_format_for_las_pcd::unknown;
	pcd_data_format pcd_format = pcd_data_format::unknown;
	float ground_level = 0.0f;

	pcd_data() { format = data_format::pcd; }
};

struct waymo_data : data<5> {
	float min_elongation = std::numeric_limits<float>::max();
	float max_elongation = std::numeric_limits<float>::lowest();

	waymo_data() { format = data_format::waymo; }
};
