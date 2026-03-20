#include "data_loader.h"
#include "pcd_reader.h"

#include <QFutureWatcher>
#include <QtConcurrent>

#include <laszip.hpp>
#include <laspoint.hpp>
#include <lasreader.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

inline float rgb_validation(const uint16_t r, const uint16_t g, const uint16_t b) {
	const uint8_t r8 = static_cast<uint8_t>(r >> 8);
	const uint8_t g8 = static_cast<uint8_t>(g >> 8);
	const uint8_t b8 = static_cast<uint8_t>(b >> 8);

	const uint32_t packed = (static_cast<uint32_t>(r8) << 16) | (static_cast<uint32_t>(g8) << 8) | (static_cast<uint32_t>(b8));

	float value;
	std::memcpy(&value, &packed, sizeof(uint32_t));
	return value;
}

struct bounds_calculator {
	float min_x = std::numeric_limits<float>::max();
	float max_x = std::numeric_limits<float>::lowest();
	float min_y = std::numeric_limits<float>::max();
	float max_y = std::numeric_limits<float>::lowest();
	float min_z = std::numeric_limits<float>::max();
	float max_z = std::numeric_limits<float>::lowest();

	float min_intensity = std::numeric_limits<float>::max();
	float max_intensity = std::numeric_limits<float>::lowest();

	float min_extra = std::numeric_limits<float>::max();
	float max_extra = std::numeric_limits<float>::lowest();

	void update_xyz(const float x, const float y, const float z) {
		min_x = std::min(min_x, x);
		max_x = std::max(max_x, x);
		min_y = std::min(min_y, y);
		max_y = std::max(max_y, y);
		min_z = std::min(min_z, z);
		max_z = std::max(max_z, z);
	}

	void merge_xyz(const bounds_calculator &other) {
		min_x = std::min(min_x, other.min_x);
		max_x = std::max(max_x, other.max_x);
		min_y = std::min(min_y, other.min_y);
		max_y = std::max(max_y, other.max_y);
		min_z = std::min(min_z, other.min_z);
		max_z = std::max(max_z, other.max_z);
	}

	template <typename T>
	void apply_xyz(T *result) const {
		result->bounds.min_x = min_x;
		result->bounds.max_x = max_x;
		result->bounds.min_y = min_y;
		result->bounds.max_y = max_y;
		result->bounds.min_z = min_z;
		result->bounds.max_z = max_z;
	}

	void update_xyzi(const float x, const float y, const float z, const float i) {
		min_x = std::min(min_x, x);
		max_x = std::max(max_x, x);
		min_y = std::min(min_y, y);
		max_y = std::max(max_y, y);
		min_z = std::min(min_z, z);
		max_z = std::max(max_z, z);

		min_intensity = std::min(min_intensity, i);
		max_intensity = std::max(max_intensity, i);
	}

	void merge_xyzi(const bounds_calculator &other) {
		min_x = std::min(min_x, other.min_x);
		max_x = std::max(max_x, other.max_x);
		min_y = std::min(min_y, other.min_y);
		max_y = std::max(max_y, other.max_y);
		min_z = std::min(min_z, other.min_z);
		max_z = std::max(max_z, other.max_z);

		min_intensity = std::min(min_intensity, other.min_intensity);
		max_intensity = std::max(max_intensity, other.max_intensity);
	}

	template <typename T>
	void apply_xyzi(T *result) const {
		result->bounds.min_x = min_x;
		result->bounds.max_x = max_x;
		result->bounds.min_y = min_y;
		result->bounds.max_y = max_y;
		result->bounds.min_z = min_z;
		result->bounds.max_z = max_z;

		result->min_intensity = min_intensity;
		result->max_intensity = max_intensity;
	}

	void update_xyzi_e(const float x, const float y, const float z, const float i, const float e) {
		update_xyzi(x, y, z, i);
		min_extra = std::min(min_extra, e);
		max_extra = std::max(max_extra, e);
	}

	void merge_xyzi_e(const bounds_calculator &other) {
		merge_xyzi(other);

		min_extra = std::min(min_extra, other.min_extra);
		max_extra = std::max(max_extra, other.max_extra);
	}

	void apply_xyzi_e(nuscenes_data *result) const {
		apply_xyzi(result);

		result->min_ring = min_extra;
		result->max_ring = max_extra;
	}

	void apply_xyzi_e(waymo_data *result) const {
		apply_xyzi(result);

		result->min_elongation = min_extra;
		result->max_elongation = max_extra;
	}
};

template <typename range, typename merge>
void multi_thread(size_t total_point_count, range &&process_range, merge &&local_merge, bounds_calculator &calculator) {
	const unsigned int hardware_threads = std::thread::hardware_concurrency();
	const unsigned int max_threads = (hardware_threads > 0) ? hardware_threads : 4;

	if (total_point_count < 1000000) {
		process_range(0, total_point_count, calculator);
		return;
	}

	const unsigned int worker_thread_count = max_threads;

	struct alignas(64) aligned_bounds {
		bounds_calculator calculator;
	};

	std::vector<aligned_bounds> per_thread(worker_thread_count);
	std::vector<std::thread> worker_threads;
	worker_threads.reserve(worker_thread_count);

	const size_t points_per_thread = total_point_count / worker_thread_count;
	const size_t remaining_points = total_point_count % worker_thread_count;

	size_t begin_index = 0;

	try {
		for (unsigned int thread_index = 0; thread_index < worker_thread_count; ++thread_index) {
			size_t count = points_per_thread + (thread_index < remaining_points ? 1 : 0);
			size_t end_index = begin_index + count;

			worker_threads.emplace_back([&, begin_index, end_index, thread_index]() {
				process_range(begin_index, end_index, per_thread[thread_index].calculator);
			});

			begin_index = end_index;
		}
	} catch (...) {
		for (auto &thread : worker_threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}
		throw;
	}

	for (auto &thread : worker_threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}

	for (const auto &local : per_thread) {
		local_merge(calculator, local.calculator);
	}
}

constexpr size_t z_range_separator_count = 1024;

struct alignas(64) bounds_z {
	uint32_t points_per_separator[z_range_separator_count] = {};
};

template <typename T>
size_t calculate_ground_peek(const T *histogram) {
	size_t absolute_peak = 0;
	for (size_t i = 1; i < z_range_separator_count; ++i) {
		if (histogram[i] > histogram[absolute_peak]) {
			absolute_peak = i;
		}
	}

	T threshold = histogram[absolute_peak] / 4;

	for (size_t i = 2; i < z_range_separator_count - 2; ++i) {
		if (histogram[i] > threshold) {
			if (histogram[i] >= histogram[i - 1] && histogram[i] >= histogram[i + 1] &&
				histogram[i] >= histogram[i - 2] && histogram[i] >= histogram[i + 2]) {
				return i;
			}
		}
	}

	return absolute_peak;
}

float calculate_ground(const float *vertices, size_t point_count, size_t floats_per_vertex, float min_z, float max_z) {
	const float range = max_z - min_z;
	if (point_count == 0 || range < 1e-6f) {
		return (min_z + max_z) * 0.5f;
	}

	const float z_to_separator_index = static_cast<float>(z_range_separator_count) / range;
	constexpr int max_index = z_range_separator_count - 1;

	if (point_count < 1000000) {
		bounds_z histogram{};
		for (size_t i = 0; i < point_count; ++i) {
			const float z = vertices[i * floats_per_vertex + 2];
			int separator_index = static_cast<int>((z - min_z) * z_to_separator_index);

			if (separator_index < 0) {
				separator_index = 0;
			} else if (separator_index > max_index) {
				separator_index = max_index;
			}

			++histogram.points_per_separator[separator_index];
		}

		size_t ground_peak = calculate_ground_peek(histogram.points_per_separator);
		return min_z + (static_cast<float>(ground_peak) + 0.5f) * range / static_cast<float>(z_range_separator_count);
	}

	const unsigned int hardware_threads = std::thread::hardware_concurrency();
	const unsigned int thread_count = (hardware_threads > 0) ? hardware_threads : 4;

	std::vector<bounds_z> thread_per_separator(thread_count);
	std::vector<std::thread> workers;
	workers.reserve(thread_count);

	const size_t points_per_thread = point_count / thread_count;
	const size_t remaining = point_count % thread_count;
	size_t begin = 0;

	try {
		for (unsigned int t = 0; t < thread_count; ++t) {
			const size_t count = points_per_thread + (t < remaining ? 1 : 0);
			const size_t end = begin + count;

			workers.emplace_back([&thread_per_separator, vertices, floats_per_vertex, min_z, z_to_separator_index, max_index, begin, end, t]() {
				auto &histogram = thread_per_separator[t].points_per_separator;
				for (size_t i = begin; i < end; ++i) {
					const float z = vertices[i * floats_per_vertex + 2];
					int separator_index = static_cast<int>((z - min_z) * z_to_separator_index);

					if (separator_index < 0) {
						separator_index = 0;
					} else if (separator_index > max_index) {
						separator_index = max_index;
					}

					++histogram[separator_index];
				}
			});

			begin = end;
		}
	} catch (...) {
		for (auto &w : workers) {
			if (w.joinable()) {
				w.join();
			}
		}
		throw;
	}

	for (auto &w : workers) {
		w.join();
	}

	uint64_t merged[z_range_separator_count] = {};
	for (const auto &local : thread_per_separator) {
		for (size_t i = 0; i < z_range_separator_count; ++i) {
			merged[i] += local.points_per_separator[i];
		}
	}

	size_t ground_peak = calculate_ground_peek(merged);
	return min_z + (static_cast<float>(ground_peak) + 0.5f) * range / static_cast<float>(z_range_separator_count);
}

data_loader::data_loader(QObject *parent) : QObject(parent) {
	connect(&watcher, &QFutureWatcher<data_variants>::finished, this,
		[this]() {
			is_data_loading = false;
			auto result = watcher.result();
			if (!std::holds_alternative<std::monostate>(result)) {
				emit loaded(result);
			}
		});
}

void data_loader::call_loader(const QString &file_path, const std::string &format) {
	if (is_data_loading) {
		emit error("Data loading is already in progress. Please wait for the current operation to complete.");
		return;
	}

	is_data_loading = true;

	QFuture<data_variants> future = QtConcurrent::run(
		[this, file_path, format]() {
			return loader(file_path, format);
		});

	watcher.setFuture(future);
}

bool data_loader::is_loading() const {
	return is_data_loading;
}

data_variants data_loader::loader(const QString &file_path, const std::string &format) {
	QFileInfo file_info(file_path);
	if (!file_info.exists()) {
		QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("The specified file could not be located: %1. "
																	 "Please verify the file path and try again.").arg(file_path)); }, Qt::QueuedConnection);
		return std::monostate{};
	}

	data_variants data = std::monostate{};

	if (format == "KITTI") {
		if (auto result = kitti_loader(file_path)) {
			data = result;
		}
	} else if (format == "LAS/LAZ") {
		if (auto result = las_loader(file_path)) {
			data = result;
		}
	} else if (format == "NuScenes") {
		if (auto result = nuscenes_loader(file_path)) {
			data = result;
		}
	} else if (format == "PCD") {
		if (auto result = pcd_loader(file_path)) {
			data = result;
		}
	} else if (format == "Waymo") {
		if (auto result = waymo_loader(file_path)) {
			data = result;
		}
	} else {
		QMetaObject::invokeMethod(this, [this, format]() { emit error(QString("Unsupported point cloud format: '%1'. "
																  "Supported formats: KITTI, LAS/LAZ, NuScenes, PCD, Waymo.").arg(QString::fromStdString(format))); }, Qt::QueuedConnection);
		return std::monostate{};
	}

	return data;
}

std::shared_ptr<kitti_data> data_loader::kitti_loader(const QString &file_path) {
	auto data = std::make_shared<kitti_data>();

	data->mapped_file = std::make_unique<QFile>(file_path);
	if (!data->mapped_file->open(QIODevice::ReadOnly)) {
		QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("Unable to read file: %1. "
																	 "The file may be corrupted, in use, or you may lack read permissions.").arg(file_path)); }, Qt::QueuedConnection);
		return nullptr;
	}

	const qint64 file_size = data->mapped_file->size();
	constexpr qint64 bytes_per_point = 4 * sizeof(float);

	if (file_size == 0 || file_size % bytes_per_point != 0) {
		QMetaObject::invokeMethod(this, [this]() { emit error("Invalid KITTI binary format. "
															  "File size must be a multiple of 16 bytes (4 floats per point)."); }, Qt::QueuedConnection);
		return nullptr;
	}

	data->mapped_ptr = data->mapped_file->map(0, file_size);
	if (!data->mapped_ptr) {
		return nullptr;
	}
	data->mapped_size = file_size;

	const auto point_count = static_cast<std::size_t>(file_size / bytes_per_point);
	data->point_count = point_count;

	const auto *ptr = reinterpret_cast<const float *>(data->mapped_ptr);
	bounds_calculator merged;

	multi_thread(point_count, [ptr](size_t b, size_t e, bounds_calculator &calculator) {
		for (size_t i = b; i < e; ++i) {
			const size_t index = i * 4;
			calculator.update_xyzi(ptr[index], ptr[index + 1], ptr[index + 2], ptr[index + 3]);
		} }, [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyzi(c); }, merged);

	merged.apply_xyzi(data.get());

	data->ground_level = calculate_ground(reinterpret_cast<const float *>(data->mapped_ptr), data->point_count, 4, data->bounds.min_z, data->bounds.max_z);

	return data;
}

std::shared_ptr<las_data> data_loader::las_loader(const QString &file_path) {
	LASreadOpener las_read_opener;
	las_read_opener.set_file_name(file_path.toStdString().c_str());

	auto las_destructor = [](LASreader *las) { if (las) { las->close(); delete las; } };
	std::unique_ptr<LASreader, decltype(las_destructor)> las_reader(las_read_opener.open(), las_destructor);

	if (!las_reader) {
		QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("Unable to read LAS/LAZ file: %1. "
																	 "The file may be corrupted or you may lack read permissions.").arg(file_path)); }, Qt::QueuedConnection);
		return nullptr;
	}

	const std::size_t point_count = las_reader->npoints;

	if (point_count == 0) {
		QMetaObject::invokeMethod(this, [this]() { emit error("The LAS/LAZ file contains no point records."); }, Qt::QueuedConnection);
		return nullptr;
	}

	auto data = std::make_shared<las_data>();

	bool rgb_check = false;

	if (las_reader->point.have_rgb) {
		int checked = 0;
		while (las_reader->read_point() && checked < 1000) {
			const LASpoint &las_point = las_reader->point;
			if (las_point.get_R() != 0 || las_point.get_G() != 0 || las_point.get_B() != 0) {
				rgb_check = true;
				break;
			}
			++checked;
		}

		if (!las_reader->seek(0)) {
			las_reader->close();
			las_reader.reset(las_read_opener.open());

			if (!las_reader) {
				QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("Unable to re-open LAS/LAZ file after RGB check: %1. "
																			 "The file may be corrupted or not seekable.").arg(file_path)); }, Qt::QueuedConnection);
				return nullptr;
			}
		}
	}

	data->color_format = rgb_check ? coloration_format_for_las_pcd::xyzrgb : coloration_format_for_las_pcd::xyzi;

	data->point_count = point_count;

	try {
		data->vertices.resize(point_count * 4);
	} catch (const std::bad_alloc &) {
		QMetaObject::invokeMethod(this, [this]() { emit error("Insufficient memory to load point cloud data. "
															  "Consider closing other applications or using a smaller dataset."); }, Qt::QueuedConnection);
		return nullptr;
	}

	const double world_x = (las_reader->header.min_x + las_reader->header.max_x) * 0.5;
	const double world_y = (las_reader->header.min_y + las_reader->header.max_y) * 0.5;
	const double world_z = (las_reader->header.min_z + las_reader->header.max_z) * 0.5;

	data->bounds.min_x = static_cast<float>(las_reader->header.min_x - world_x);
	data->bounds.max_x = static_cast<float>(las_reader->header.max_x - world_x);
	data->bounds.min_y = static_cast<float>(las_reader->header.min_y - world_y);
	data->bounds.max_y = static_cast<float>(las_reader->header.max_y - world_y);
	data->bounds.min_z = static_cast<float>(las_reader->header.min_z - world_z);
	data->bounds.max_z = static_cast<float>(las_reader->header.max_z - world_z);

	bool is_compressed = (las_reader->header.point_data_format >= 128);

	if (!is_compressed && point_count >= 1000000) {
		las_reader->close();
		las_reader.reset();

		const unsigned int hardware_threads = std::thread::hardware_concurrency();
		const unsigned int thread_count = (hardware_threads > 0) ? hardware_threads : 4;

		struct alignas(64) aligned_intensity {
			float min_intensity = std::numeric_limits<float>::max();
			float max_intensity = std::numeric_limits<float>::lowest();
		};

		const bool has_rgb = (data->color_format == coloration_format_for_las_pcd::xyzrgb);

		std::vector<aligned_intensity> per_thread;
		if (!has_rgb) {
			per_thread.resize(thread_count);
		}

		std::vector<std::thread> worker_threads;
		worker_threads.reserve(thread_count);

		const size_t points_per_thread = point_count / thread_count;
		const size_t remaining = point_count % thread_count;

		float *output_ptr = data->vertices.data();
		const std::string path_str = file_path.toStdString();

		size_t begin = 0;

		try {
			for (unsigned int t = 0; t < thread_count; ++t) {
				const size_t count = points_per_thread + (t < remaining ? 1 : 0);
				const size_t end = begin + count;

				worker_threads.emplace_back([&per_thread, output_ptr, &path_str,
												world_x, world_y, world_z,
												has_rgb, begin, end, t]() {
					LASreadOpener las_thread_reader;
					las_thread_reader.set_file_name(path_str.c_str());

					auto thread_destructor = [](LASreader *las) { if (las) { las->close(); delete las; } };
					std::unique_ptr<LASreader, decltype(thread_destructor)>
						reader(las_thread_reader.open(), thread_destructor);

					if (!reader) {
						return;
					}

					reader->seek(static_cast<I64>(begin));

					if (has_rgb) {
						for (size_t i = begin; i < end; ++i) {
							if (!reader->read_point()) {
								break;
							}
							const LASpoint &las_point = reader->point;

							const auto x = static_cast<float>(las_point.get_x() - world_x);
							const auto y = static_cast<float>(las_point.get_y() - world_y);
							const auto z = static_cast<float>(las_point.get_z() - world_z);

							const size_t index = i * 4;
							output_ptr[index + 0] = x;
							output_ptr[index + 1] = y;
							output_ptr[index + 2] = z;
							output_ptr[index + 3] = rgb_validation(las_point.get_R(), las_point.get_G(), las_point.get_B());
						}
					} else {
						auto &intensity_bounds = per_thread[t];

						for (size_t i = begin; i < end; ++i) {
							if (!reader->read_point()) {
								break;
							}
							const LASpoint &las_point = reader->point;

							const auto x = static_cast<float>(las_point.get_x() - world_x);
							const auto y = static_cast<float>(las_point.get_y() - world_y);
							const auto z = static_cast<float>(las_point.get_z() - world_z);
							const auto intensity = static_cast<float>(las_point.get_intensity());

							const size_t index = i * 4;
							output_ptr[index + 0] = x;
							output_ptr[index + 1] = y;
							output_ptr[index + 2] = z;
							output_ptr[index + 3] = intensity;

							intensity_bounds.min_intensity = std::min(intensity_bounds.min_intensity, intensity);
							intensity_bounds.max_intensity = std::max(intensity_bounds.max_intensity, intensity);
						}
					}
				});

				begin = end;
			}
		} catch (...) {
			for (auto &thread : worker_threads) {
				if (thread.joinable()) {
					thread.join();
				}
			}
			throw;
		}

		for (auto &thread : worker_threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		if (!has_rgb) {
			float min_intensity = std::numeric_limits<float>::max();
			float max_intensity = std::numeric_limits<float>::lowest();

			for (const auto &local : per_thread) {
				min_intensity = std::min(min_intensity, local.min_intensity);
				max_intensity = std::max(max_intensity, local.max_intensity);
			}

			data->min_intensity = min_intensity;
			data->max_intensity = max_intensity;
		}
	} else {
		float *output_ptr = data->vertices.data();
		std::size_t index = 0;

		float min_intensity = std::numeric_limits<float>::max();
		float max_intensity = std::numeric_limits<float>::lowest();

		if (data->color_format == coloration_format_for_las_pcd::xyzrgb) {
			while (las_reader->read_point()) {
				const LASpoint &las_point = las_reader->point;

				const auto x = static_cast<float>(las_point.get_x() - world_x);
				const auto y = static_cast<float>(las_point.get_y() - world_y);
				const auto z = static_cast<float>(las_point.get_z() - world_z);

				output_ptr[index * 4 + 0] = x;
				output_ptr[index * 4 + 1] = y;
				output_ptr[index * 4 + 2] = z;
				output_ptr[index * 4 + 3] = rgb_validation(las_point.get_R(), las_point.get_G(), las_point.get_B());
				++index;
			}
		} else {
			while (las_reader->read_point()) {
				const LASpoint &las_point = las_reader->point;

				const auto x = static_cast<float>(las_point.get_x() - world_x);
				const auto y = static_cast<float>(las_point.get_y() - world_y);
				const auto z = static_cast<float>(las_point.get_z() - world_z);
				const auto intensity = static_cast<float>(las_point.get_intensity());

				output_ptr[index * 4 + 0] = x;
				output_ptr[index * 4 + 1] = y;
				output_ptr[index * 4 + 2] = z;
				output_ptr[index * 4 + 3] = intensity;

				min_intensity = std::min(min_intensity, intensity);
				max_intensity = std::max(max_intensity, intensity);
				++index;
			}
		}

		data->point_count = index;

		if (data->color_format == coloration_format_for_las_pcd::xyzi) {
			data->min_intensity = min_intensity;
			data->max_intensity = max_intensity;
		}
	}

	data->ground_level = calculate_ground(static_cast<const float *>(data->access()), data->point_count, 4, data->bounds.min_z, data->bounds.max_z);

	return data;
}

std::shared_ptr<nuscenes_data> data_loader::nuscenes_loader(const QString &file_path) {
	auto data = std::make_shared<nuscenes_data>();

	data->mapped_file = std::make_unique<QFile>(file_path);
	if (!data->mapped_file->open(QIODevice::ReadOnly)) {
		QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("Unable to read file: %1. "
																	 "The file may be corrupted, in use, or you may lack read permissions.").arg(file_path)); }, Qt::QueuedConnection);
		return nullptr;
	}

	const qint64 file_size = data->mapped_file->size();
	constexpr qint64 bytes_per_point = 5 * sizeof(float);

	if (file_size == 0 || file_size % bytes_per_point != 0) {
		QMetaObject::invokeMethod(this, [this]() { emit error("Invalid NuScenes binary format. "
															  "File size must be a multiple of 20 bytes (5 floats per point)."); }, Qt::QueuedConnection);
		return nullptr;
	}

	data->mapped_ptr = data->mapped_file->map(0, file_size);
	if (!data->mapped_ptr) {
		return nullptr;
	}
	data->mapped_size = file_size;

	const auto point_count = static_cast<std::size_t>(file_size / bytes_per_point);
	data->point_count = point_count;

	const auto *ptr = reinterpret_cast<const float *>(data->mapped_ptr);
	bounds_calculator merged;

	multi_thread(point_count, [ptr](size_t b, size_t e, bounds_calculator &calculator) {
		for (size_t i = b; i < e; ++i) {
			const size_t index = i * 5;
			calculator.update_xyzi_e(ptr[index], ptr[index + 1], ptr[index + 2], ptr[index + 3], ptr[index + 4]);
		} }, [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyzi_e(c); }, merged);

	merged.apply_xyzi_e(data.get());

	data->ground_level = calculate_ground(reinterpret_cast<const float *>(data->mapped_ptr), data->point_count, 5, data->bounds.min_z, data->bounds.max_z);

	return data;
}

std::shared_ptr<pcd_data> data_loader::pcd_loader(const QString &file_path) {
	pcd_reader pcd_reader;
	QString error_message;

	auto data = pcd_reader.reader(file_path, &error_message);

	if (!data) {
		QMetaObject::invokeMethod(this, [this, error_message]() { emit error(error_message.isEmpty()
																		  ? "Failed to parse the PCD file. Please verify the file is not corrupted."
																		  : error_message); }, Qt::QueuedConnection);
		return nullptr;
	}

	if (data->is_valid()) {
		data->ground_level = calculate_ground(static_cast<const float *>(data->access()), data->point_count, 4, data->bounds.min_z, data->bounds.max_z);
	}

	return data;
}

std::shared_ptr<waymo_data> data_loader::waymo_loader(const QString &file_path) {
	auto data = std::make_shared<waymo_data>();

	data->mapped_file = std::make_unique<QFile>(file_path);
	if (!data->mapped_file->open(QIODevice::ReadOnly)) {
		QMetaObject::invokeMethod(this, [this, file_path]() { emit error(QString("Unable to read file: %1. "
																	 "The file may be corrupted, in use, or you may lack read permissions.").arg(file_path)); }, Qt::QueuedConnection);
		return nullptr;
	}

	const qint64 file_size = data->mapped_file->size();
	constexpr qint64 bytes_per_point = 5 * sizeof(float);

	if (file_size == 0 || file_size % bytes_per_point != 0) {
		QMetaObject::invokeMethod(this, [this]() { emit error("Invalid Waymo binary format. "
															  "File size must be a multiple of 20 bytes (5 floats per point)."); }, Qt::QueuedConnection);
		return nullptr;
	}

	data->mapped_ptr = data->mapped_file->map(0, file_size);
	if (!data->mapped_ptr) {
		return nullptr;
	}
	data->mapped_size = file_size;

	const auto point_count = static_cast<std::size_t>(file_size / bytes_per_point);
	data->point_count = point_count;

	const auto *ptr = reinterpret_cast<const float *>(data->mapped_ptr);
	bounds_calculator merged;

	multi_thread(point_count, [ptr](size_t b, size_t e, bounds_calculator &calculator) {
		for (size_t i = b; i < e; ++i) {
			const size_t index = i * 5;
			calculator.update_xyzi_e(ptr[index], ptr[index + 1], ptr[index + 2], ptr[index + 3], ptr[index + 4]);
		} }, [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyzi_e(c); }, merged);

	merged.apply_xyzi_e(data.get());

	data->ground_level = calculate_ground(reinterpret_cast<const float *>(data->mapped_ptr), data->point_count, 5, data->bounds.min_z, data->bounds.max_z);

	return data;
}
