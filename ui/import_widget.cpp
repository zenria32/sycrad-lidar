#include "import_widget.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>

const QVector<import_widget::dataset_info> &import_widget::dataset() {
	static const QVector<dataset_info> list = {
		{"KITTI", "Stable", "float32", ".bin", "XYZ + Intensity"},
		{"LAS/LAZ", "Experimental", "float64", ".las / .laz", "XYZ + Intensity/RGB"},
		{"NuScenes", "Stable", "float32", ".pcd.bin", "XYZ + Intensity + Ring"},
		{"PCD", "Experimental", "float32", ".pcd", "XYZ / XYZ+I / XYZ+RGB"},
		{"Waymo", "Experimental", "float32", ".bin", "XYZ + Intensity + Elongation"}};
	return list;
}

import_widget::import_widget(QWidget *parent) : QDialog(parent) {
	setWindowTitle("New Project");
	setObjectName("import_widget");
	setFixedSize(640, 560);
	setModal(true);

	root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(28, 24, 28, 24);
	root_layout->setSpacing(0);

	auto *header = new QLabel("Create New Project", this);
	header->setObjectName("import_widget_header");
	root_layout->addWidget(header);
	root_layout->addSpacing(4);

	step_bar = build_indicator();
	root_layout->addWidget(step_bar);
	root_layout->addSpacing(16);

	data_page = new QWidget(this);
	config_page = new QWidget(this);

	build_data_page();
	build_config_page();

	root_layout->addWidget(data_page, 1);
	root_layout->addWidget(config_page, 1);
	config_page->setVisible(false);
	root_layout->addSpacing(16);

	auto *button_layout = new QHBoxLayout();
	button_layout->setSpacing(8);

	cancel_button = new QPushButton("Cancel", this);
	back_button = new QPushButton("Back", this);
	next_button = new QPushButton("Next", this);
	create_button = new QPushButton("Create", this);

	create_button->setObjectName("create_button");
	back_button->setVisible(false);
	create_button->setVisible(false);

	button_layout->addWidget(cancel_button);
	button_layout->addStretch();
	button_layout->addWidget(back_button);
	button_layout->addWidget(next_button);
	button_layout->addWidget(create_button);

	root_layout->addLayout(button_layout);

	connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);

	connect(next_button, &QPushButton::clicked, this, [this]() {
		if (selected_dataset_index < 0) {
			return;
		}
		if (project_name_input->text().trimmed().isEmpty()) {
			return;
		}
		change_step(2);
	});

	connect(back_button, &QPushButton::clicked, this, [this]() {
		change_step(1);
	});

	connect(create_button, &QPushButton::clicked, this, [this]() {
		if (data_path_input->text().trimmed().isEmpty()) {
			return;
		}
		accept();
	});

	update_indicator(1);
}

QString import_widget::data_format() const {
	if (selected_dataset_index < 0 || selected_dataset_index >= dataset().size()) {
		return {};
	}
	return dataset()[selected_dataset_index].name;
}

QWidget *import_widget::build_indicator() {
	auto *indicator_bar = new QWidget(this);
	indicator_bar->setObjectName("indicator_bar");
	indicator_bar->setFixedHeight(32);

	auto *indicator_layout = new QHBoxLayout(indicator_bar);
	indicator_layout->setContentsMargins(0, 8, 0, 0);
	indicator_layout->setSpacing(0);

	data_step_logo = new QWidget(indicator_bar);
	data_step_logo->setFixedSize(8, 8);

	data_step_label = new QLabel("Data Struct", indicator_bar);

	connector_line = new QWidget(indicator_bar);
	connector_line->setFixedHeight(1);

	config_step_logo = new QWidget(indicator_bar);
	config_step_logo->setFixedSize(8, 8);

	config_step_label = new QLabel("Configuration", indicator_bar);

	indicator_layout->addStretch();
	indicator_layout->addWidget(data_step_logo, 0, Qt::AlignVCenter);
	indicator_layout->addSpacing(6);
	indicator_layout->addWidget(data_step_label, 0, Qt::AlignVCenter);
	indicator_layout->addSpacing(12);
	indicator_layout->addWidget(connector_line);
	connector_line->setMinimumWidth(40);
	indicator_layout->addSpacing(12);
	indicator_layout->addWidget(config_step_logo, 0, Qt::AlignVCenter);
	indicator_layout->addSpacing(6);
	indicator_layout->addWidget(config_step_label, 0, Qt::AlignVCenter);
	indicator_layout->addStretch();

	return indicator_bar;
}

void import_widget::update_indicator(int current_step) {
	const bool data_page_active = (current_step >= 1);
	const bool config_page_active = (current_step >= 2);

	data_step_logo->setObjectName(data_page_active ? "step_logo_active" : "step_logo_inactive");
	data_step_label->setObjectName(data_page_active ? "step_label_active" : "step_label_inactive");

	connector_line->setObjectName(config_page_active ? "connector_active" : "connector_inactive");

	config_step_logo->setObjectName(config_page_active ? "step_logo_active" : "step_logo_inactive");
	config_step_label->setObjectName(config_page_active ? "step_label_active" : "step_label_inactive");

	for (auto *widget : {data_step_logo, config_step_logo, connector_line}) {
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
		widget->setAttribute(Qt::WA_StyledBackground, true);
	}
	for (auto *label : {data_step_label, config_step_label}) {
		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

void import_widget::build_data_page() {
	auto *page_layout = new QVBoxLayout(data_page);
	page_layout->setContentsMargins(0, 0, 0, 0);
	page_layout->setSpacing(12);

	auto *subtitle = new QLabel("Select Dataset", data_page);
	subtitle->setObjectName("data_step_subtitle");
	page_layout->addWidget(subtitle);

	auto *card_grid = new QWidget(data_page);
	auto *grid_layout = new QGridLayout(card_grid);
	grid_layout->setContentsMargins(10, 0, 10, 0);
	grid_layout->setSpacing(8);
	grid_layout->setAlignment(Qt::AlignTop | Qt::AlignCenter);

	constexpr int max_cards_per_row = 4;
	for (int column = 0; column < max_cards_per_row; ++column) {
		grid_layout->setColumnStretch(column, 1);
	}

	const auto &data_info = dataset();
	for (int index = 0; index < data_info.size(); ++index) {
		const auto &i = data_info[index];
		auto *card = new QWidget(card_grid);
		card->setObjectName("dataset_card");
		card->setCursor(Qt::PointingHandCursor);
		card->setAttribute(Qt::WA_StyledBackground, true);
		card->setProperty("selected", false);
		card->setFixedHeight(128);

		auto *card_layout = new QVBoxLayout(card);
		card_layout->setContentsMargins(12, 10, 12, 10);
		card_layout->setSpacing(3);

		auto *name_label = new QLabel(i.name, card);
		name_label->setObjectName("dataset_card_name");
		card_layout->addWidget(name_label);

		auto *stability_label = new QLabel(i.stability, card);
		stability_label->setObjectName(i.stability == "Stable" ? "dataset_card_stable" : "dataset_card_experimental");
		card_layout->addWidget(stability_label);

		card_layout->addSpacing(2);

		auto *precision_label = new QLabel(i.precision + "  " + i.extension, card);
		precision_label->setObjectName("dataset_card_precision");
		precision_label->setWordWrap(true);
		card_layout->addWidget(precision_label);

		auto *variables_label = new QLabel(i.variables, card);
		variables_label->setObjectName("dataset_card_variables");
		variables_label->setWordWrap(true);
		card_layout->addWidget(variables_label);

		card_layout->addStretch();

		const int row = index / max_cards_per_row;
		const int column = index % max_cards_per_row;
		grid_layout->addWidget(card, row, column);
		dataset_cards.append(card);
	}

	page_layout->addWidget(card_grid);

	project_info_panel = new QWidget(data_page);
	project_info_panel->setObjectName("dataset_info_panel");
	project_info_panel->setAttribute(Qt::WA_StyledBackground, true);
	project_info_panel->setVisible(false);

	auto *info_layout = new QVBoxLayout(project_info_panel);
	info_layout->setContentsMargins(16, 12, 16, 12);
	info_layout->setSpacing(8);

	auto *project_name_label = new QLabel("Project Name", project_info_panel);
	project_name_label->setObjectName("project_name_label");
	info_layout->addWidget(project_name_label);

	project_name_input = new QLineEdit(project_info_panel);
	project_name_input->setPlaceholderText("Enter project name...");
	info_layout->addWidget(project_name_input);

	page_layout->addWidget(project_info_panel);

	page_layout->addStretch();
}

void import_widget::build_config_page() {
	auto *page_layout = new QVBoxLayout(config_page);
	page_layout->setContentsMargins(0, 0, 0, 0);
	page_layout->setSpacing(10);

	auto *subtitle = new QLabel("Configure Paths", config_page);
	subtitle->setObjectName("config_step_subtitle");
	page_layout->addWidget(subtitle);

	auto build_card = [this](QWidget *parent, const QString &title, bool is_required, QLineEdit *&path_output) -> QWidget * {
		auto *card = new QWidget(parent);
		card->setObjectName("config_card");
		card->setAttribute(Qt::WA_StyledBackground, true);

		auto *card_layout = new QVBoxLayout(card);
		card_layout->setContentsMargins(16, 12, 16, 12);
		card_layout->setSpacing(8);

		auto *title_layout = new QHBoxLayout();
		title_layout->setSpacing(8);

		auto *title_label = new QLabel(title, card);
		title_label->setObjectName("config_card_title");
		title_layout->addWidget(title_label);

		auto *req_label = new QLabel(is_required ? "Required" : "Optional", card);
		req_label->setObjectName(is_required ? "required_label" : "optional_label");
		title_layout->addWidget(req_label);

		title_layout->addStretch();
		card_layout->addLayout(title_layout);

		auto *path_layout = new QHBoxLayout();
		path_layout->setSpacing(8);

		path_output = new QLineEdit(card);
		path_output->setPlaceholderText("No folder selected");
		path_output->setReadOnly(true);
		path_layout->addWidget(path_output, 1);

		auto *browse_button = new QPushButton("Browse", card);
		browse_button->setObjectName("browse_button");
		browse_button->setFixedWidth(88);
		browse_button->setCursor(Qt::PointingHandCursor);

		connect(browse_button, &QPushButton::clicked, this, [this, path_output, title]() {
			browse_folder(path_output, title);
		});

		path_layout->addWidget(browse_button);
		card_layout->addLayout(path_layout);

		auto *path_display = new QLabel("", card);
		path_display->setObjectName("path_display");
		path_display->setWordWrap(true);
		card_layout->addWidget(path_display);

		connect(path_output, &QLineEdit::textChanged, path_display, [path_display](const QString &text) {
			path_display->setText(text.isEmpty() ? "" : text);
			path_display->setVisible(!text.isEmpty());
		});
		path_display->setVisible(false);

		return card;
	};

	auto *data_card = build_card(config_page, "Select Data Folder", true, data_path_input);
	auto *calibration_card = build_card(config_page, "Select Calibration Folder", false, calibration_path_input);
	auto *media_card = build_card(config_page, "Select Media Folder", false, media_path_input);
	auto *label_card = build_card(config_page, "Select Label Folder", false, label_path_input);

	page_layout->addWidget(data_card);
	page_layout->addWidget(calibration_card);
	page_layout->addWidget(media_card);
	page_layout->addWidget(label_card);
	page_layout->addStretch();
}

void import_widget::select_dataset(const int index) {
	if (index < 0 || index >= dataset_cards.size()) {
		return;
	}

	if (index == selected_dataset_index) {
		selected_dataset_index = -1;
		for (auto *card : dataset_cards) {
			card->setProperty("selected", false);
			card->style()->unpolish(card);
			card->style()->polish(card);
		}
		project_info_panel->setVisible(false);
		return;
	}

	selected_dataset_index = index;

	for (int i = 0; i < dataset_cards.size(); ++i) {
		bool is_selected = (i == index);
		dataset_cards[i]->setProperty("selected", is_selected);
		dataset_cards[i]->style()->unpolish(dataset_cards[i]);
		dataset_cards[i]->style()->polish(dataset_cards[i]);
	}

	project_info_panel->setVisible(true);
}

void import_widget::browse_folder(QLineEdit *target, const QString &dialog_title) {
	QString dir = QFileDialog::getExistingDirectory(this, dialog_title, QDir::homePath());
	if (!dir.isEmpty()) {
		target->setText(dir);
	}
}

void import_widget::change_step(int step) {
	const bool show_data_step = (step == 1);

	data_page->setVisible(show_data_step);
	config_page->setVisible(!show_data_step);

	next_button->setVisible(show_data_step);
	back_button->setVisible(!show_data_step);
	create_button->setVisible(!show_data_step);

	update_indicator(step);
}

bool import_widget::event(QEvent *event) {
	if (event->type() == QEvent::MouseButtonRelease) {
		if (!data_page->isVisible()) {
			return QDialog::event(event);
		}
		auto *mouse_event = static_cast<QMouseEvent *>(event);
		for (int i = 0; i < dataset_cards.size(); ++i) {
			QWidget *card = dataset_cards[i];
			QPoint card_position = card->mapFrom(this, mouse_event->pos());
			if (card->rect().contains(card_position)) {
				select_dataset(i);
				return true;
			}
		}
	}
	return QDialog::event(event);
}
