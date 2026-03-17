#include "pcd_reader.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

inline uint32_t lzf_decompress(const void *const input_data, uint32_t input_len, void *output_data, uint32_t output_len) {
	if (input_len == 0 || output_len == 0) {
		return 0;
	}

	const auto *input_ptr = static_cast<const uint8_t *>(input_data);
	const uint8_t *const input_ptr_end = input_ptr + input_len;
	auto *output_ptr = static_cast<uint8_t *>(output_data);
	uint8_t *const output_ptr_begin = output_ptr;
	uint8_t *const output_ptr_end = output_ptr + output_len;

	while (input_ptr < input_ptr_end) {
		uint32_t controller = *input_ptr++;

		if (controller < (1 << 5)) {
			controller++;
			if ((output_ptr + controller > output_ptr_end) || (input_ptr + controller > input_ptr_end)) {
				return 0;
			}

			std::memcpy(output_ptr, input_ptr, controller);
			output_ptr += controller;
			input_ptr += controller;
		} else {
			uint32_t len = controller >> 5;

			uint32_t reference_offset = ((controller & 0x1f) << 8) + 1;

			if (len == 7) {
				if (input_ptr >= input_ptr_end) {
					return 0;
				}
				len += *input_ptr++;
			}

			if (input_ptr >= input_ptr_end) {
				return 0;
			}
			reference_offset += *input_ptr++;

			if (static_cast<size_t>(output_ptr - output_ptr_begin) < reference_offset) {
				return 0;
			}

			const uint8_t *reference = output_ptr - reference_offset;

			if (output_ptr + len + 2 > output_ptr_end) {
				return 0;
			}

			*output_ptr++ = *reference++;
			*output_ptr++ = *reference++;
			do {
				*output_ptr++ = *reference++;
			} while (--len);
		}
	}
	return static_cast<uint32_t>(output_ptr - output_ptr_begin);
}

inline float int_to_float(const uint32_t rgb) {
	float value;
	std::memcpy(&value, &rgb, sizeof(uint32_t));
	return value;
}

inline float rgb_validation(const char *ptr, const char type) {
	if (type == 'F') {
		float value;
		std::memcpy(&value, ptr, sizeof(float));
		return value;
	}
	uint32_t rgb;
	std::memcpy(&rgb, ptr, sizeof(uint32_t));
	return int_to_float(rgb);
}

const char *find_data_keyword(const char *content, size_t file_size) {
	if (file_size < 6) {
		return nullptr;
	}
	if (std::strncmp(content, "DATA ", 5) == 0) {
		return content;
	}

	const char *end_ptr = content + file_size - 6;
	const char *ptr = content;

	while (ptr < end_ptr) {
		const char *next = static_cast<const char *>(std::memchr(ptr, '\n', end_ptr - ptr));
		if (!next) {
			break;
		}
		ptr = next + 1;
		if (std::strncmp(ptr, "DATA ", 5) == 0) {
			return ptr;
		}
	}
	return nullptr;
}

const char *find_newline(const char *start, const char *end) {
	if (start >= end) {
		return nullptr;
	}
	return static_cast<const char *>(std::memchr(start, '\n', end - start));
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

	void apply_xyz(pcd_data *result) const {
		result->bounds.min_x = min_x;
		result->bounds.max_x = max_x;
		result->bounds.min_y = min_y;
		result->bounds.max_y = max_y;
		result->bounds.min_z = min_z;
		result->bounds.max_z = max_z;
	}

	void update_xyzi(const float x, const float y, const float z, const float intensity) {
		min_x = std::min(min_x, x);
		max_x = std::max(max_x, x);
		min_y = std::min(min_y, y);
		max_y = std::max(max_y, y);
		min_z = std::min(min_z, z);
		max_z = std::max(max_z, z);

		min_intensity = std::min(min_intensity, intensity);
		max_intensity = std::max(max_intensity, intensity);
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

	void apply_xyzi(pcd_data *result) const {
		result->bounds.min_x = min_x;
		result->bounds.max_x = max_x;
		result->bounds.min_y = min_y;
		result->bounds.max_y = max_y;
		result->bounds.min_z = min_z;
		result->bounds.max_z = max_z;

		result->min_intensity = min_intensity;
		result->max_intensity = max_intensity;
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

template <typename TX, typename TY, typename TZ>
void binary_xyz(const char *data, size_t stride, size_t offset_x, size_t offset_y, size_t offset_z, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	for (size_t i = thread_begin; i < thread_end; ++i) {
		const char *ptr = data + (i * stride);
		float *gpu_buffer = output_ptr + (i * 4);

		TX x;
		TY y;
		TZ z;

		std::memcpy(&x, ptr + offset_x, sizeof(TX));
		std::memcpy(&y, ptr + offset_y, sizeof(TY));
		std::memcpy(&z, ptr + offset_z, sizeof(TZ));

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = 1.0f;

		calculator.update_xyz(x, y, z);
	}
}

template <typename TX, typename TY, typename TZ>
void binary_xyzi(const char *data, size_t stride, size_t offset_x, size_t offset_y, size_t offset_z, size_t offset_intensity, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	for (size_t i = thread_begin; i < thread_end; ++i) {
		const char *ptr = data + (i * stride);
		float *gpu_buffer = output_ptr + (i * 4);

		TX x;
		TY y;
		TZ z;
		float intensity;

		std::memcpy(&x, ptr + offset_x, sizeof(TX));
		std::memcpy(&y, ptr + offset_y, sizeof(TY));
		std::memcpy(&z, ptr + offset_z, sizeof(TZ));
		std::memcpy(&intensity, ptr + offset_intensity, sizeof(float));

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = intensity;

		calculator.update_xyzi(x, y, z, intensity);
	}
}

template <typename TX, typename TY, typename TZ>
void binary_xyzrgb(const char *data, size_t stride, size_t offset_x, size_t offset_y, size_t offset_z, size_t offset_rgb, char rgb_type, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	for (size_t i = thread_begin; i < thread_end; ++i) {
		const char *ptr = data + (i * stride);
		float *gpu_buffer = output_ptr + (i * 4);

		TX x;
		TY y;
		TZ z;

		std::memcpy(&x, ptr + offset_x, sizeof(TX));
		std::memcpy(&y, ptr + offset_y, sizeof(TY));
		std::memcpy(&z, ptr + offset_z, sizeof(TZ));

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = rgb_validation(ptr + offset_rgb, rgb_type);

		calculator.update_xyz(x, y, z);
	}
}

template <typename TX, typename TY, typename TZ>
void binary_compressed_xyz(const char *x_array_ptr, const char *y_array_ptr, const char *z_array_ptr, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	const TX *x_data = reinterpret_cast<const TX *>(x_array_ptr);
	const TY *y_data = reinterpret_cast<const TY *>(y_array_ptr);
	const TZ *z_data = reinterpret_cast<const TZ *>(z_array_ptr);

	for (size_t i = thread_begin; i < thread_end; ++i) {
		float *gpu_buffer = output_ptr + (i * 4);

		const auto x = static_cast<float>(x_data[i]);
		const auto y = static_cast<float>(y_data[i]);
		const auto z = static_cast<float>(z_data[i]);

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = 1.0f;

		calculator.update_xyz(x, y, z);
	}
}

template <typename TX, typename TY, typename TZ>
void binary_compressed_xyzi(const char *x_array_ptr, const char *y_array_ptr, const char *z_array_ptr, const char *intensity_array_ptr, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	const TX *x_data = reinterpret_cast<const TX *>(x_array_ptr);
	const TY *y_data = reinterpret_cast<const TY *>(y_array_ptr);
	const TZ *z_data = reinterpret_cast<const TZ *>(z_array_ptr);
	const auto *i_data = reinterpret_cast<const float *>(intensity_array_ptr);

	for (size_t i = thread_begin; i < thread_end; ++i) {
		float *gpu_buffer = output_ptr + (i * 4);

		const auto x = static_cast<float>(x_data[i]);
		const auto y = static_cast<float>(y_data[i]);
		const auto z = static_cast<float>(z_data[i]);
		const float intensity = i_data[i];

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = intensity;

		calculator.update_xyzi(x, y, z, intensity);
	}
}

template <typename TX, typename TY, typename TZ>
void binary_compressed_xyzrgb(const char *x_array_ptr, const char *y_array_ptr, const char *z_array_ptr, const char *rgb_array_ptr, char rgb_type, size_t thread_begin, size_t thread_end, float *output_ptr, bounds_calculator &calculator) {
	const TX *x_data = reinterpret_cast<const TX *>(x_array_ptr);
	const TY *y_data = reinterpret_cast<const TY *>(y_array_ptr);
	const TZ *z_data = reinterpret_cast<const TZ *>(z_array_ptr);

	for (size_t i = thread_begin; i < thread_end; ++i) {
		float *gpu_buffer = output_ptr + (i * 4);

		const auto x = static_cast<float>(x_data[i]);
		const auto y = static_cast<float>(y_data[i]);
		const auto z = static_cast<float>(z_data[i]);

		gpu_buffer[0] = x;
		gpu_buffer[1] = y;
		gpu_buffer[2] = z;
		gpu_buffer[3] = rgb_validation(rgb_array_ptr + (i * 4), rgb_type);

		calculator.update_xyz(x, y, z);
	}
}

std::shared_ptr<pcd_data> pcd_reader::reader(const QString &file_path, QString *error_out) {
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		if (error_out) {
			*error_out = QString("Error: cannot open file: %1").arg(file_path);
		}
		return nullptr;
	}

	const qint64 file_size = file.size();
	if (file_size <= 0) {
		return nullptr;
	}

	uchar *mapped = file.map(0, file_size);
	if (!mapped) {
		return nullptr;
	}

	struct map_guard {
		QFile &f;
		uchar *m;
		~map_guard() {
			if (m) {
				f.unmap(m);
			}
		}
	} guard{file, mapped};

	const char *file_content = reinterpret_cast<const char *>(mapped);
	const auto total_size = static_cast<size_t>(file_size);

	const char *data = find_data_keyword(file_content, total_size);
	if (!data) {
		if (error_out) {
			*error_out = "Error: missing data section.";
		}
		return nullptr;
	}

	const char *data_end = find_newline(data, file_content + total_size);
	if (!data_end) {
		return nullptr;
	}

	header_size_in_bytes = static_cast<size_t>((data_end - file_content) + 1);

	if (!parse_header(file_content, header_size_in_bytes, error_out)) {
		return nullptr;
	}

	if (!data_integrity_check(error_out)) {
		return nullptr;
	}

	auto result = std::make_shared<pcd_data>();
	result->point_count = point_count;
	result->pcd_format = pcd_format;

	const int intensity_index = find_header_index("intensity");
	const int rgb_index = find_header_index("rgb");

	if (intensity_index >= 0) {
		color_format = coloration_format_for_las_pcd::xyzi;
	} else if (rgb_index >= 0) {
		color_format = coloration_format_for_las_pcd::xyzrgb;
	} else {
		color_format = coloration_format_for_las_pcd::xyz;
	}

	result->color_format = color_format;

	try {
		result->vertices.resize(point_count * pcd_data::floats_per_vertex());
	} catch (const std::bad_alloc &) {
		if (error_out) {
			*error_out = QString("Error: out of memory failed to allocate vertex buffer.");
		}
		return nullptr;
	}

	const char *data_ptr = file_content + header_size_in_bytes;

	const size_t data_section_size = total_size - header_size_in_bytes;
	bool success = false;

	switch (result->pcd_format) {
	case pcd_data_format::ascii: {
		success = parse_ascii_data(
			data_ptr, data_section_size, result, error_out);
		break;
	}
	case pcd_data_format::binary: {
		success = parse_binary_data(
			data_ptr, data_section_size, result, error_out);
		break;
	}
	case pcd_data_format::binary_compressed: {
		success = parse_binary_compressed_data(
			data_ptr, data_section_size, result, error_out);
		break;
	}
	default: {
		return nullptr;
	}
	}

	file.close();
	return success ? result : nullptr;
}

int pcd_reader::find_header_index(const QString &name) const {
	const QString lower_name = name.toLower();
	for (int i = 0; i < static_cast<int>(pcd_header.size()); ++i) {
		if (pcd_header[i].field == lower_name) {
			return i;
		}
	}
	return -1;
}

bool pcd_reader::parse_header(const char *data, size_t size, QString *error_out) {
	QByteArray header_info = QByteArray::fromRawData(data, static_cast<int>(size));
	QTextStream stream(header_info);

	QStringList field_name, field_size, field_type;

	pcd_header.clear();
	point_count = 0;
	point_size_in_bytes = 0;
	pcd_format = pcd_data_format::unknown;

	static const QRegularExpression qre("\\s+");

	while (!stream.atEnd()) {
		const QString line = stream.readLine().trimmed();
		if (line.isEmpty() || line.startsWith('#')) {
			continue;
		}

		const auto part = line.split(qre, Qt::SkipEmptyParts);
		if (part.isEmpty()) {
			continue;
		}

		const QString &keyword = part[0];

		if (keyword.compare("FIELDS", Qt::CaseInsensitive) == 0) {
			field_name = part.mid(1);
		} else if (keyword.compare("SIZE", Qt::CaseInsensitive) == 0) {
			field_size = part.mid(1);
		} else if (keyword.compare("TYPE", Qt::CaseInsensitive) == 0) {
			field_type = part.mid(1);
		} else if (keyword.compare("POINTS", Qt::CaseInsensitive) == 0) {
			if (part.size() > 1) {
				point_count = part[1].toULongLong();
			}
		} else if (keyword.compare("DATA", Qt::CaseInsensitive) == 0) {
			if (part.size() > 1) {
				const QString f = part[1].toLower();
				if (f == "ascii") {
					pcd_format = pcd_data_format::ascii;
				} else if (f == "binary") {
					pcd_format = pcd_data_format::binary;
				} else if (f == "binary_compressed") {
					pcd_format = pcd_data_format::binary_compressed;
				}
			}
		}
	}

	uint32_t offset = 0;
	pcd_header.reserve(field_name.size());

	for (int i = 0; i < field_name.size(); ++i) {
		header header_data;
		header_data.field = field_name[i].toLower();
		header_data.size = (i < field_size.size()) ? field_size[i].toUInt() : 4;
		header_data.type = (i < field_type.size() && !field_type[i].isEmpty()) ? field_type[i][0].toLatin1() : 'F';
		header_data.offset = offset;
		offset += header_data.size;
		pcd_header.push_back(header_data);
	}

	point_size_in_bytes = offset;

	if (point_count == 0 || pcd_format == pcd_data_format::unknown || pcd_header.empty()) {
		if (error_out) {
			*error_out = "Error: unsupported data format.";
		}
		return false;
	}

	return true;
}

bool pcd_reader::data_integrity_check(QString *error_out) const {
	const int x_index = find_header_index("x");
	const int y_index = find_header_index("y");
	const int z_index = find_header_index("z");
	const int intensity_index = find_header_index("intensity");
	const int rgb_index = find_header_index("rgb");

	if (x_index < 0 || y_index < 0 || z_index < 0) {
		if (error_out) {
			*error_out = "Error: Missing required x, y, or z fields.";
		}
		return false;
	}

	auto check_float32 = [&](int index, const char *field_name) -> bool {
		const header &header_data = pcd_header[index];
		if (header_data.type != 'F' || header_data.size != 4) {
			if (error_out) {
				*error_out = QString("Error: '%1' must be float32.").arg(field_name);
			}
			return false;
		}
		return true;
	};

	if (!check_float32(x_index, "x")) {
		return false;
	}
	if (!check_float32(y_index, "y")) {
		return false;
	}
	if (!check_float32(z_index, "z")) {
		return false;
	}

	if (intensity_index >= 0) {
		if (!check_float32(intensity_index, "intensity")) {
			return false;
		}
	}

	if (rgb_index >= 0) {
		if (pcd_header[rgb_index].size != 4 || (pcd_header[rgb_index].type != 'F' && pcd_header[rgb_index].type != 'U' && pcd_header[rgb_index].type != 'I')) {
			if (error_out) {
				*error_out = "Error: rgb must be float32, uint32 or int32.";
			}
			return false;
		}
	}

	return true;
}

bool pcd_reader::parse_ascii_data(const char *data, size_t size, std::shared_ptr<pcd_data> result, QString *error_out) {
	float *output_ptr = result->vertices.data();

	const int x_index = find_header_index("x");
	const int y_index = find_header_index("y");
	const int z_index = find_header_index("z");
	const int intensity_index = find_header_index("intensity");
	const int rgb_index = find_header_index("rgb");
	const size_t field_count = pcd_header.size();

	const char rgb_type = (color_format == coloration_format_for_las_pcd::xyzrgb) ? pcd_header[rgb_index].type : 'F';

	constexpr size_t MAX_FIELDS = 64;
	std::array<float, MAX_FIELDS> row_values{};
	if (field_count > MAX_FIELDS) {
		return false;
	}

	bounds_calculator calculator;
	const char *ptr_begin = data;
	const char *ptr_end = data + size;
	size_t points_read = 0;

	while (ptr_begin < ptr_end && points_read < point_count) {
		while (ptr_begin < ptr_end && static_cast<unsigned char>(*ptr_begin) <= 32) {
			++ptr_begin;
		}
		if (ptr_begin >= ptr_end) {
			break;
		}

		for (size_t i = 0; i < field_count; ++i) {
			float value = 0.0f;
			const auto res = std::from_chars(ptr_begin, ptr_end, value);
			if (res.ec == std::errc()) {
				row_values[i] = value;
				ptr_begin = res.ptr;
			} else {
				row_values[i] = 0.0;
				while (ptr_begin < ptr_end && static_cast<unsigned char>(*ptr_begin) > 32) {
					++ptr_begin;
				}
			}
			while (ptr_begin < ptr_end && static_cast<unsigned char>(*ptr_begin) <= 32) {
				++ptr_begin;
			}
		}

		const auto x = static_cast<float>(row_values[x_index]);
		const auto y = static_cast<float>(row_values[y_index]);
		const auto z = static_cast<float>(row_values[z_index]);

		output_ptr[0] = x;
		output_ptr[1] = y;
		output_ptr[2] = z;

		if (color_format == coloration_format_for_las_pcd::xyzi) {
			const auto intensity = static_cast<float>(row_values[intensity_index]);
			output_ptr[3] = intensity;
			calculator.update_xyzi(x, y, z, intensity);
		} else if (color_format == coloration_format_for_las_pcd::xyzrgb) {
			if (rgb_type == 'F') {
				output_ptr[3] = static_cast<float>(row_values[rgb_index]);
			} else {
				const auto raw_rgb = static_cast<uint32_t>(row_values[rgb_index]);
				output_ptr[3] = int_to_float(raw_rgb);
			}
			calculator.update_xyz(x, y, z);
		} else {
			output_ptr[3] = 1.0f;
			calculator.update_xyz(x, y, z);
		}

		output_ptr += 4;
		++points_read;
	}

	result->point_count = points_read;
	if (color_format == coloration_format_for_las_pcd::xyzi) {
		calculator.apply_xyzi(result.get());
	} else {
		calculator.apply_xyz(result.get());
	}

	return true;
}

bool pcd_reader::parse_binary_data(const char *data, size_t size, std::shared_ptr<pcd_data> result, QString *error_out) {
	if (point_count > (size / point_size_in_bytes)) {
		return false;
	}

	const int x = find_header_index("x");
	const int y = find_header_index("y");
	const int z = find_header_index("z");
	const int intensity = find_header_index("intensity");
	const int rgb = find_header_index("rgb");

	const size_t offset_x = pcd_header[x].offset;
	const size_t offset_y = pcd_header[y].offset;
	const size_t offset_z = pcd_header[z].offset;
	const size_t offset_intensity = (intensity >= 0) ? pcd_header[intensity].offset : 0;
	const size_t offset_rgb = (rgb >= 0) ? pcd_header[rgb].offset : 0;

	const char rgb_type = color_format == coloration_format_for_las_pcd::xyzrgb ? pcd_header[rgb].type : 'F';

	bounds_calculator merged;
	float *output_ptr = result->vertices.data();
	const size_t stride = point_size_in_bytes;

	const auto merge_xyz = [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyz(c); };
	const auto merge_xyzi = [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyzi(c); };

	if (color_format == coloration_format_for_las_pcd::xyzi) {
		multi_thread(
			point_count,
			[&](size_t b, size_t e, bounds_calculator &calculator) {
				binary_xyzi<float, float, float>(
					data, stride, offset_x, offset_y, offset_z, offset_intensity, b, e, output_ptr, calculator);
			},
			merge_xyzi,
			merged);

		merged.apply_xyzi(result.get());
		return true;
	}

	if (color_format == coloration_format_for_las_pcd::xyzrgb) {
		multi_thread(
			point_count,
			[&](size_t b, size_t e, bounds_calculator &calculator) {
				binary_xyzrgb<float, float, float>(
					data, stride, offset_x, offset_y, offset_z, offset_rgb, rgb_type, b, e, output_ptr, calculator);
			},
			merge_xyz,
			merged);

		merged.apply_xyz(result.get());
		return true;
	}

	multi_thread(
		point_count,
		[&](size_t b, size_t e, bounds_calculator &calculator) {
			binary_xyz<float, float, float>(
				data, stride, offset_x, offset_y, offset_z, b, e, output_ptr, calculator);
		},
		merge_xyz,
		merged);

	merged.apply_xyz(result.get());
	return true;
}

bool pcd_reader::parse_binary_compressed_data(const char *data, size_t size, std::shared_ptr<pcd_data> &result, QString *error_out) {
	uint32_t compressed_size = 0;
	uint32_t uncompressed_size = 0;
	std::memcpy(&compressed_size, data, 4);
	std::memcpy(&uncompressed_size, data + 4, 4);

	const uint64_t expected_size = static_cast<uint64_t>(point_size_in_bytes) * point_count;
	if (uncompressed_size < expected_size) {
		return false;
	}

	std::unique_ptr<char[]> buffer;

	try {
		buffer.reset(new char[uncompressed_size]);
	} catch (const std::bad_alloc &) {
		if (error_out) {
			*error_out = "Error: out of memory during decompression.";
		}
		return false;
	}

	const uint32_t decompressed_size = lzf_decompress(data + 8, compressed_size, buffer.get(), uncompressed_size);
	if (decompressed_size != uncompressed_size) {
		return false;
	}

	const int x = find_header_index("x");
	const int y = find_header_index("y");
	const int z = find_header_index("z");
	const int intensity = find_header_index("intensity");
	const int rgb = find_header_index("rgb");

	auto offset_calculator = [&](int index) -> size_t {
		if (index < 0) {
			return 0;
		}
		size_t t = 0;
		for (int i = 0; i < index; ++i) {
			t += static_cast<size_t>(pcd_header[i].size) * point_count;
		}
		return t;
	};

	const size_t offset_x = offset_calculator(x);
	const size_t offset_y = offset_calculator(y);
	const size_t offset_z = offset_calculator(z);
	const size_t offset_int = offset_calculator(intensity);
	const size_t offset_rgb = offset_calculator(rgb);

	const char *base_ptr = buffer.get();
	const char *ptr_x = base_ptr + offset_x;
	const char *ptr_y = base_ptr + offset_y;
	const char *ptr_z = base_ptr + offset_z;
	const char *ptr_intensity = base_ptr + offset_int;
	const char *ptr_rgb = base_ptr + offset_rgb;

	auto rgb_type = color_format == coloration_format_for_las_pcd::xyzrgb ? pcd_header[rgb].type : 'F';

	bounds_calculator merged;
	float *output_ptr = result->vertices.data();

	const auto merge_xyz = [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyz(c); };
	const auto merge_xyzi = [](bounds_calculator &b, const bounds_calculator &c) { b.merge_xyzi(c); };

	if (color_format == coloration_format_for_las_pcd::xyzi) {
		multi_thread(
			point_count,
			[&](size_t b, size_t e, bounds_calculator &calculator) {
				binary_compressed_xyzi<float, float, float>(
					ptr_x, ptr_y, ptr_z, ptr_intensity, b, e, output_ptr, calculator);
			},
			merge_xyzi,
			merged);

		merged.apply_xyzi(result.get());
		return true;
	}

	if (color_format == coloration_format_for_las_pcd::xyzrgb) {
		multi_thread(
			point_count,
			[&](size_t b, size_t e, bounds_calculator &calculator) {
				binary_compressed_xyzrgb<float, float, float>(
					ptr_x, ptr_y, ptr_z, ptr_rgb, rgb_type, b, e, output_ptr, calculator);
			},
			merge_xyz,
			merged);

		merged.apply_xyz(result.get());
		return true;
	}

	multi_thread(
		point_count,
		[&](size_t b, size_t e, bounds_calculator &calculator) {
			binary_compressed_xyz<float, float, float>(
				ptr_x, ptr_y, ptr_z, b, e, output_ptr, calculator);
		},
		merge_xyz,
		merged);

	merged.apply_xyz(result.get());
	return true;
}
