#pragma once

#include "data.h"

#include <QString>

#include <memory>
#include <vector>

class pcd_reader {
	public:
	struct header {
		QString field;
		uint32_t size;
		char type;
		uint32_t offset;
	};

	pcd_reader() = default;
	~pcd_reader() = default;

	std::shared_ptr<pcd_data> reader(const QString &file_path, QString *error_out = nullptr);

	private:
	int find_header_index(const QString &name) const;
	bool parse_header(const char *data, size_t size, QString *error_out);
	bool data_integrity_check(QString *error_out) const;

	bool parse_ascii_data(const char *data, size_t size, std::shared_ptr<pcd_data> result, QString *error_out);
	bool parse_binary_data(const char *data, size_t size, std::shared_ptr<pcd_data> result, QString *error_out);
	bool parse_binary_compressed_data(const char *data, size_t size, std::shared_ptr<pcd_data> &result, QString *error_out);

	coloration_format_for_las_pcd color_format = coloration_format_for_las_pcd::unknown;
	pcd_data_format pcd_format = pcd_data_format::unknown;

	std::vector<header> pcd_header;
	size_t header_size_in_bytes = 0;
	uint32_t point_size_in_bytes = 0;
	uint64_t point_count = 0;
};
