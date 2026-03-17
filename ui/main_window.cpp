#include "main_window.h"
#include "camera_frame.h"
#include "data_loader.h"
#include "import_widget.h"
#include "lidar_viewport.h"
#include "notification_widget.h"
#include "project_manager.h"
#include "statistics_widget.h"
#include "cuboid_manager.h"
#include "calibration_store.h"
#include "input_handler.h"
#include "orbital_camera.h"
#include "ray.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QScreen>
#include <QShortcut>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTimer>

struct tools {
	const char *name;
	const char *icon;
	const char *tooltip;
};

static constexpr tools tool_modes[] = {
	{"Add", ":/icons/add_cube.svg", "Add Cuboid (A)"},
	{"Select", ":/icons/select.svg", "Select Cuboid (Q)"},
	{"Move", ":/icons/move.svg", "Move Cuboid (W)"},
	{"Rotate", ":/icons/rotate.svg", "Rotate Cuboid (E)"},
	{"Scale", ":/icons/scale.svg", "Scale Cuboid (R)"},
};

static constexpr size_t tool_mode_count = std::size(tool_modes);

class property_delegate : public QStyledItemDelegate {
	public:
	using QStyledItemDelegate::QStyledItemDelegate;
	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override {
		if (index.column() != 1) {
			return nullptr;
		}
		if (!(index.flags() & Qt::ItemIsEditable)) {
			return nullptr;
		}
		return QStyledItemDelegate::createEditor(parent, option, index);
	}
};

main_window::main_window(QWidget *parent) : QMainWindow(parent) {
	setWindowTitle("Sycrad LiDAR");

	resize(1280, 720);
	setWindowState(Qt::WindowMaximized);

	load_qss();

	project_loader = std::make_unique<project_manager>(this);
	loader = std::make_unique<data_loader>(this);
	cmngr = std::make_unique<cuboid_manager>(this);
	cstore = std::make_unique<calibration_store>();

	cstore->set_reporter([this](const QString &message) { notify(message, 3); });

	connect(loader.get(), &data_loader::loaded, this, [this](data_variants d) {
		current_data = std::move(d);

		if (viewport) {
			viewport->load_point_cloud(current_data);
		}

		std::visit([this](const auto &data) {
			if constexpr (!std::is_same_v<std::decay_t<decltype(data)>, std::monostate>) {
				if (statistics) {
					statistics->get_point_count(static_cast<quint64>(data->point_count));
				}
			}
		},
			current_data);
	});

	connect(loader.get(), &data_loader::error, this, [this](const QString &message) {
		if (viewport) {
			viewport->hide_loading_overlay();
		}
		notify(message, 3);
	});

	connect(project_loader.get(), &project_manager::error, this, [this](const QString &message) {
		notify(message, 3);
	});

	process_window();
	if (cstore) {
		cstore->set_reporter([this](const QString &message) {
			notify(message, 3);
		});
	}
	process_menubar();

	if (viewport && statistics) {
		statistics->get_viewport(viewport);
		statistics->get_graphics_api(viewport->get_graphics_api());
	}
}

main_window::~main_window() = default;

void main_window::load_qss() {
	QFile style_file(":/style.qss");
	if (style_file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&style_file);
		qApp->setStyleSheet(stream.readAll());
	}
}

void main_window::process_window() {
	central_widget = new QWidget(this);
	central_widget->setObjectName("central_widget");
	setCentralWidget(central_widget);

	auto *root_layout = new QHBoxLayout(central_widget);
	root_layout->setContentsMargins(0, 0, 0, 0);
	root_layout->setSpacing(0);

	process_toolbar();
	root_layout->addWidget(toolbar_widget);

	auto *horizontal_container = new QWidget(central_widget);
	horizontal_container->setObjectName("horizontal_container");
	horizontal_layout = new QHBoxLayout(horizontal_container);
	horizontal_layout->setContentsMargins(0, 0, 0, 0);
	horizontal_layout->setSpacing(0);

	auto *vertical_container = new QWidget(horizontal_container);
	vertical_container->setObjectName("vertical_container");
	vertical_layout = new QVBoxLayout(vertical_container);
	vertical_layout->setContentsMargins(0, 0, 0, 0);
	vertical_layout->setSpacing(0);

	process_viewport();
	vertical_layout->addWidget(viewport_container, 3);

	process_camera_menu();
	vertical_layout->addWidget(camera_container, 1);
	horizontal_layout->addWidget(vertical_container, 7);

	process_object_menu();
	horizontal_layout->addWidget(object_container, 2);

	root_layout->addWidget(horizontal_container, 1);

	notification = new notification_widget(viewport_container);

	statistics = new statistics_widget(viewport_container);
	statistics->setAttribute(Qt::WA_TranslucentBackground);
	statistics->show();

	viewport_container->installEventFilter(this);

	QTimer::singleShot(0, this, [this]() { process_overlays(); });
}

void main_window::process_menubar() {
	menuBar()->setNativeMenuBar(false);
	menuBar()->setObjectName("menu_bar");
	file_menu = menuBar()->addMenu("File");
	edit_menu = menuBar()->addMenu("Edit");
	view_menu = menuBar()->addMenu("View");
	tools_menu = menuBar()->addMenu("Tools");
	help_menu = menuBar()->addMenu("Help");

	QAction *new_project = file_menu->addAction("New Project");
	new_project->setShortcut(QKeySequence::New);

	QAction *open_project = file_menu->addAction("Open Project");
	open_project->setShortcut(QKeySequence::Open);

	file_menu->addSeparator();

	connect(new_project, &QAction::triggered, this, &main_window::new_project);
	connect(open_project, &QAction::triggered, this, &main_window::open_project);

	if (cmngr) {
		QAction *undo_action = cmngr->get_undo_stack()->createUndoAction(this, "Undo");
		undo_action->setShortcut(QKeySequence::Undo);
		edit_menu->addAction(undo_action);

		QAction *redo_action = cmngr->get_undo_stack()->createRedoAction(this, "Redo");
		redo_action->setShortcut(QKeySequence::Redo);
		edit_menu->addAction(redo_action);

		edit_menu->addSeparator();

		QAction *delete_action = edit_menu->addAction("Delete Selected");
		delete_action->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});
		connect(delete_action, &QAction::triggered, this, [this]() {
			if (cmngr && cmngr->has_anything_selected()) {
				cmngr->remove_cuboid(cmngr->get_selected_id());
				if (viewport) {
					viewport->update();
				}
			}
		});
	}
}

void main_window::process_toolbar() {
	toolbar_widget = new QWidget(central_widget);
	toolbar_widget->setObjectName("toolbar");
	toolbar_widget->setFixedWidth(46);

	toolbar_layout = new QVBoxLayout(toolbar_widget);
	toolbar_layout->setContentsMargins(5, 10, 5, 10);
	toolbar_layout->setSpacing(4);

	const qreal pixel_ratio = devicePixelRatioF();
	const int logical = 24;
	const int physical = qRound(logical * pixel_ratio);

	for (size_t i = 0; i < tool_mode_count; ++i) {
		auto *button = new QToolButton(toolbar_widget);
		button->setObjectName(QString("toolbar_button"));
		button->setCheckable(true);
		button->setCursor(QCursor(Qt::PointingHandCursor));
		button->setToolTip(tool_modes[i].tooltip);
		button->setFixedSize(36, 36);

		QIcon icon(tool_modes[i].icon);
		QPixmap pixmap = icon.pixmap(QSize(physical, physical));
		pixmap.setDevicePixelRatio(pixel_ratio);
		button->setIcon(QIcon(pixmap));
		button->setIconSize(QSize(logical, logical));

		connect(button, &QToolButton::clicked, this, [this, i]() {
			if (tool_mode == static_cast<int>(i)) {
				tool_mode = -1;
				toolbar_buttons[i]->setChecked(false);
				if (viewport && viewport->get_input()) {
					viewport->get_input()->set_tool_mode(static_cast<input_handler::tool_mode>(-1));
				}
				return;
			}
			tool_mode_changed(static_cast<int>(i));
		});

		static const QKeySequence keys[] = {
			Qt::Key_A,
			Qt::Key_Q,
			Qt::Key_W,
			Qt::Key_E,
			Qt::Key_R,
		};

		auto *shortcut = new QShortcut(keys[i], this);
		connect(shortcut, &QShortcut::activated, button, &QToolButton::click);

		toolbar_buttons.append(button);
		toolbar_layout->addWidget(button, 0, Qt::AlignHCenter);
	}

	toolbar_layout->addStretch();
}

void main_window::tool_mode_changed(int tool_index) {
	if (tool_index == tool_mode) {
		return;
	}
	tool_mode = tool_index;

	for (int i = 0; i < toolbar_buttons.size(); ++i) {
		toolbar_buttons[i]->setChecked(i == tool_index);
	}

	if (viewport && viewport->get_input()) {
		viewport->get_input()->set_tool_mode(static_cast<input_handler::tool_mode>(tool_index));
	}

	notify(QString("Tool: %1").arg(tool_modes[tool_index].tooltip));
}

void main_window::process_viewport() {
	viewport_container = new QWidget();
	viewport_container->setObjectName("viewport_container");
	viewport_container->setMinimumSize(400, 300);
	viewport_container->setFocusPolicy(Qt::StrongFocus);
	viewport_container->setAttribute(Qt::WA_StyledBackground, true);

	auto *container_layout = new QVBoxLayout(viewport_container);
	container_layout->setContentsMargins(0, 0, 0, 0);
	container_layout->setSpacing(0);

	viewport = new lidar_viewport(viewport_container);
	viewport->setFocusPolicy(Qt::StrongFocus);
	viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	container_layout->addWidget(viewport, 1);

	if (cmngr) {
		viewport->set_cuboid_manager(cmngr.get());
	}

	if (cstore) {
		viewport->set_calibration_store(cstore.get());
	}

	if (viewport->get_input() && viewport->get_camera() && cmngr) {
		connect(viewport->get_input(), &input_handler::focus_requested, this,
			[this]() {
				if (!cmngr->has_anything_selected()) {
					return;
				}
				const cuboid *c = cmngr->get_selected_cuboid();
				if (c) {
					viewport->get_camera()->animation_to_target(c->position, 300.0f);
					viewport->update();
				}
			});
	}

	connect(viewport, &lidar_viewport::error, this, [this](const QString &message) {
		notify(message, 3);
	});
}

void main_window::process_camera_menu() {
	camera_container = new QWidget();
	camera_container->setObjectName("camera_container");
	camera_container->setAttribute(Qt::WA_StyledBackground, true);
	camera_container->setMinimumHeight(140);

	auto *camera_container_layout = new QVBoxLayout(camera_container);
	camera_container_layout->setContentsMargins(0, 0, 0, 0);
	camera_container_layout->setSpacing(0);

	auto *camera_row = new QWidget(camera_container);
	camera_row->setObjectName("camera_row");
	auto *camera_layout = new QHBoxLayout(camera_row);
	camera_layout->setContentsMargins(2, 2, 2, 2);
	camera_layout->setSpacing(2);

	top_camera = new camera_frame("Top", camera_row);
	front_camera = new camera_frame("Front", camera_row);
	side_camera = new camera_frame("Side", camera_row);
	media_camera = new camera_frame("Camera", camera_row);

	camera_layout->addWidget(top_camera);
	camera_layout->addWidget(front_camera);
	camera_layout->addWidget(side_camera);
	camera_layout->addWidget(media_camera);

	camera_container_layout->addWidget(camera_row, 1);
}

void main_window::process_object_menu() {
	object_container = new QTabWidget(central_widget);
	object_container->setObjectName("object_container");
	object_container->setMinimumWidth(175);
	object_container->setTabPosition(QTabWidget::North);
	object_container->setAttribute(Qt::WA_StyledBackground, true);

	object_container->tabBar()->setExpanding(true);
	object_container->tabBar()->setDocumentMode(true);

	process_explorer_tab();
	process_object_tab();

	object_container->addTab(explorer_tab, "EXPLORER");
	object_container->addTab(object_tab, "OBJECT");
}

void main_window::process_explorer_tab() {
	explorer_tab = new QWidget();
	explorer_tab->setObjectName("explorer_tab");
	auto *layout = new QVBoxLayout(explorer_tab);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	explorer_tree = new QTreeWidget(explorer_tab);
	explorer_tree->setObjectName("explorer_tree");
	explorer_tree->setHeaderHidden(true);
	explorer_tree->setIndentation(16);
	explorer_tree->setAnimated(true);
	explorer_tree->setExpandsOnDoubleClick(false);
	explorer_tree->setIconSize(QSize(18, 18));
	explorer_tree->setRootIsDecorated(true);
	explorer_tree->setSelectionMode(QAbstractItemView::NoSelection);
	explorer_tree->setFocusPolicy(Qt::NoFocus);

	connect(explorer_tree, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem *item, int) {
		if (item->childCount() > 0) {
			item->setExpanded(!item->isExpanded());
		}
	});

	connect(explorer_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
		if (!item || item->childCount() > 0) {
			return;
		}
		QString path = item->data(0, Qt::UserRole).toString();
		if (path.isEmpty()) {
			return;
		}
		load_file(path);
	});

	QIcon folder_icon(":/icons/folder.svg");

	auto *data_item = new QTreeWidgetItem(explorer_tree, {"Data"});
	data_item->setIcon(0, folder_icon);
	data_item->setFlags(data_item->flags() & ~Qt::ItemIsSelectable);
	data_item->setExpanded(false);

	layout->addWidget(explorer_tree, 1);
}

void main_window::process_object_tab() {
	object_tab = new QWidget();
	object_tab->setObjectName("object_tab");
	auto *tab_layout = new QVBoxLayout(object_tab);
	tab_layout->setContentsMargins(0, 0, 0, 0);
	tab_layout->setSpacing(0);

	object_splitter = new QSplitter(Qt::Vertical, object_tab);
	object_splitter->setObjectName("object_splitter");
	object_splitter->setHandleWidth(1);
	object_splitter->setChildrenCollapsible(false);

	property_container = new QWidget();
	property_container->setObjectName("property_container");
	property_container->setAttribute(Qt::WA_StyledBackground, true);

	auto *property_layout = new QVBoxLayout(property_container);
	property_layout->setContentsMargins(0, 0, 0, 0);
	property_layout->setSpacing(0);

	property_title = new QLabel("PROPERTIES", property_container);
	property_title->setObjectName("property_title");
	property_layout->addWidget(property_title);

	property_tree = new QTreeWidget(property_container);
	property_tree->setObjectName("property_tree");
	property_tree->setColumnCount(2);
	property_tree->setHeaderLabels({"Property", "Value"});
	property_tree->header()->setStretchLastSection(true);
	property_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	property_tree->setIndentation(0);
	property_tree->setAnimated(false);
	property_tree->setAlternatingRowColors(false);
	property_tree->setIconSize(QSize(16, 16));
	property_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	property_tree->setItemDelegate(new property_delegate(property_tree));

	connect(property_tree, &QTreeWidget::itemClicked, this,
		[this](QTreeWidgetItem *item, int column) {
			if (!item || column != 1) {
				return;
			}
			if (!(item->flags() & Qt::ItemIsEditable)) {
				return;
			}
			property_tree->editItem(item, column);
		});

	auto make_item = [](const QString &label, const QString &value, bool editable) -> QTreeWidgetItem * {
		auto *item = new QTreeWidgetItem({label, value});
		Qt::ItemFlags flags = item->flags() & ~Qt::ItemIsEditable;
		if (editable) {
			flags |= Qt::ItemIsEditable;
		}
		item->setFlags(flags);
		return item;
	};

	property_tree->addTopLevelItem(make_item("id", "none", false));
	property_tree->addTopLevelItem(make_item("class", "none", true));

	auto *position = new QTreeWidgetItem({"Position", ""});
	position->setFlags(position->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
	position->addChild(make_item("X", "0.000", true));
	position->addChild(make_item("Y", "0.000", true));
	position->addChild(make_item("Z", "0.000", true));
	property_tree->addTopLevelItem(position);
	position->setExpanded(true);

	auto *rotation = new QTreeWidgetItem({"Rotation", ""});
	rotation->setFlags(rotation->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
	rotation->addChild(make_item("X", "0.000", true));
	rotation->addChild(make_item("Y", "0.000", true));
	rotation->addChild(make_item("Z", "0.000", true));
	property_tree->addTopLevelItem(rotation);
	rotation->setExpanded(true);

	auto *scale = new QTreeWidgetItem({"Scale", ""});
	scale->setFlags(scale->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
	scale->addChild(make_item("X", "1.000", true));
	scale->addChild(make_item("Y", "1.000", true));
	scale->addChild(make_item("Z", "1.000", true));
	property_tree->addTopLevelItem(scale);
	scale->setExpanded(true);

	connect(property_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
		if (!cmngr || !cmngr->has_anything_selected() || column != 1)
			return;

		uint32_t id = cmngr->get_selected_id();
		cuboid c = *cmngr->get_selected_cuboid();
		bool changed = false;

		if (item->parent() == nullptr) {
			if (item->text(0) == "class") {
				c.class_name = item->text(1);
				changed = true;
			}
		} else {
			QString parent_text = item->parent()->text(0);
			QString key = item->text(0);
			float value = item->text(1).toFloat();

			if (parent_text == "Position") {
				if (key == "X") {
					c.position.setX(value);
					changed = true;
				}
				if (key == "Y") {
					c.position.setY(value);
					changed = true;
				}
				if (key == "Z") {
					c.position.setZ(value);
					changed = true;
				}
			} else if (parent_text == "Rotation") {
				QVector3D euler = c.rotation.toEulerAngles();
				if (key == "X") {
					euler.setX(value);
					changed = true;
				}
				if (key == "Y") {
					euler.setY(value);
					changed = true;
				}
				if (key == "Z") {
					euler.setZ(value);
					changed = true;
				}
				c.rotation = QQuaternion::fromEulerAngles(euler);
			} else if (parent_text == "Scale") {
				if (key == "X") {
					c.dimension.setX(value);
					changed = true;
				}
				if (key == "Y") {
					c.dimension.setY(value);
					changed = true;
				}
				if (key == "Z") {
					c.dimension.setZ(value);
					changed = true;
				}
			}
		}

		if (changed) {
			cmngr->update_cuboid(id, c);
			if (viewport)
				viewport->update();
		}
	});

	if (cmngr) {
		auto update_tree_from_cuboid = [this](uint32_t id) {
			property_tree->blockSignals(true);
			if (id == 0 || !cmngr->has_anything_selected()) {

				property_tree->topLevelItem(0)->setText(1, "none");
				property_tree->topLevelItem(1)->setText(1, "none");

				auto *position = property_tree->topLevelItem(2);
				position->child(0)->setText(1, "0.000");
				position->child(1)->setText(1, "0.000");
				position->child(2)->setText(1, "0.000");

				auto *rotation = property_tree->topLevelItem(3);
				rotation->child(0)->setText(1, "0.000");
				rotation->child(1)->setText(1, "0.000");
				rotation->child(2)->setText(1, "0.000");

				auto *scale = property_tree->topLevelItem(4);
				scale->child(0)->setText(1, "1.000");
				scale->child(1)->setText(1, "1.000");
				scale->child(2)->setText(1, "1.000");
			} else {
				const cuboid *c = cmngr->get_selected_cuboid();
				property_tree->topLevelItem(0)->setText(1, QString::number(c->id));
				property_tree->topLevelItem(1)->setText(1, c->class_name.isEmpty() ? "Unknown" : c->class_name);

				auto *position = property_tree->topLevelItem(2);
				position->child(0)->setText(1, QString::number(c->position.x(), 'f', 3));
				position->child(1)->setText(1, QString::number(c->position.y(), 'f', 3));
				position->child(2)->setText(1, QString::number(c->position.z(), 'f', 3));

				auto *rotation = property_tree->topLevelItem(3);
				QVector3D euler = c->rotation.toEulerAngles();
				rotation->child(0)->setText(1, QString::number(euler.x(), 'f', 3));
				rotation->child(1)->setText(1, QString::number(euler.y(), 'f', 3));
				rotation->child(2)->setText(1, QString::number(euler.z(), 'f', 3));

				auto *scale = property_tree->topLevelItem(4);
				scale->child(0)->setText(1, QString::number(c->dimension.x(), 'f', 3));
				scale->child(1)->setText(1, QString::number(c->dimension.y(), 'f', 3));
				scale->child(2)->setText(1, QString::number(c->dimension.z(), 'f', 3));
			}
			property_tree->blockSignals(false);
		};

		connect(cmngr.get(), &cuboid_manager::selected_cuboid_changed, this,
		[update_tree_from_cuboid](uint32_t, uint32_t new_id) {
			update_tree_from_cuboid(new_id);
		});

		connect(cmngr.get(), &cuboid_manager::cuboid_updated, this,
		[this, update_tree_from_cuboid](uint32_t id) {
			if (cmngr->get_selected_id() == id) {
				update_tree_from_cuboid(id);
			}
		});

		connect(cmngr.get(), &cuboid_manager::cuboid_added, this,
			[this](uint32_t) { refresh_object_tree(); });
		connect(cmngr.get(), &cuboid_manager::cuboid_removed, this,
			[this](uint32_t) { refresh_object_tree(); });
		connect(cmngr.get(), &cuboid_manager::cuboid_updated, this,
			[this](uint32_t) { refresh_object_tree(); });
		connect(cmngr.get(), &cuboid_manager::selected_cuboid_changed, this,
			[this](uint32_t, uint32_t) { refresh_object_tree(); });
		connect(cmngr.get(), &cuboid_manager::cleared, this,
			[this]() { refresh_object_tree(); });
	}

	property_layout->addWidget(property_tree, 1);
	object_splitter->addWidget(property_container);

	object_list_container = new QWidget();
	object_list_container->setObjectName("object_list_container");
	object_list_container->setAttribute(Qt::WA_StyledBackground, true);

	auto *list_layout = new QVBoxLayout(object_list_container);
	list_layout->setContentsMargins(0, 0, 0, 0);
	list_layout->setSpacing(0);

	object_list_title = new QLabel("OBJECT LIST", object_list_container);
	object_list_title->setObjectName("object_list_title");
	list_layout->addWidget(object_list_title);

	object_tree = new QTreeWidget(object_list_container);
	object_tree->setObjectName("object_tree");
	object_tree->setColumnCount(2);
	object_tree->setHeaderLabels({"ID", "Class"});
	object_tree->header()->setStretchLastSection(true);
	object_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	object_tree->setIndentation(0);
	object_tree->setRootIsDecorated(false);
	object_tree->setAnimated(false);
	object_tree->setIconSize(QSize(16, 16));

	connect(object_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
		if (!item || !cmngr) {
			return;
		}
		uint32_t id = item->data(0, Qt::UserRole).value<uint32_t>();
		cmngr->select(id);
		const cuboid *c = cmngr->get_selected_cuboid();
		if (c && viewport && viewport->get_camera()) {
			viewport->get_camera()->animation_to_target(c->position, 300.0f);
			viewport->update();
		}
	});

	list_layout->addWidget(object_tree, 1);
	object_splitter->addWidget(object_list_container);

	object_splitter->setStretchFactor(0, 0);
	object_splitter->setStretchFactor(1, 1);
	object_splitter->setSizes({490, 100});

	tab_layout->addWidget(object_splitter, 1);
}

void main_window::new_project() {
	import_widget widget(this);
	if (widget.exec() == QDialog::Accepted) {
		bool control = project_loader->new_project(
			widget.project_name().toStdString(),
			widget.data_format().toStdString(),
			widget.data_path().toStdString(),
			widget.calibration_path().toStdString(),
			widget.media_path().toStdString(),
			widget.label_path().toStdString());
		if (control) {
			notify(QString("Project '%1' created successfully.").arg(widget.project_name()), 1);

			const auto &cfg = project_loader->read_config();
			if (cfg.format == "KITTI" && !cfg.calibration_path.empty()) {
				cstore->load_kitti_metadata(cfg.calibration_path);
			} else if (cfg.format == "NuScenes" && !cfg.calibration_path.empty()) {
				cstore->load_nuscenes_metadata(cfg.calibration_path);
			}
			expand_explorer();
		} else {
			notify("Failed to create project.", 3);
		}
	}
}

void main_window::open_project() {
	QString root_path = QDir::homePath() + "/Documents/Sycrad LiDAR";
	QString dir = QFileDialog::getExistingDirectory(this, "Open Project", root_path);
	if (dir.isEmpty()) {
		return;
	}

	if (project_loader->open_project(std::filesystem::path(dir.toStdString()))) {
		notify(QString("Project opened: %1").arg(QString::fromStdString(project_loader->current_project_name())), 1);

		const auto &cfg = project_loader->read_config();
		if (cfg.format == "KITTI" && !cfg.calibration_path.empty()) {
			cstore->load_kitti_metadata(cfg.calibration_path);
		} else if (cfg.format == "NuScenes" && !cfg.calibration_path.empty()) {
			cstore->load_nuscenes_metadata(cfg.calibration_path);
		}

		expand_explorer();
	} else {
		notify("Failed to open project: project.json not found.", 3);
	}
}

void main_window::expand_explorer() {
	if (!project_loader->is_project_name_valid()) {
		return;
	}

	explorer_tree->clear();
	const auto &cfg = project_loader->read_config();

	QIcon folder_icon(":/icons/folder.svg");
	QIcon file_icon(":/icons/file.svg");

	auto expand_folder = [this, &folder_icon, &file_icon](const std::string &path, const QString &label) -> QTreeWidgetItem * {
		auto *root = new QTreeWidgetItem(explorer_tree, {label});
		root->setIcon(0, folder_icon);
		root->setFlags(root->flags() & ~Qt::ItemIsSelectable);

		if (path.empty()) {
			return root;
		}

		std::filesystem::path dir(path);
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec)) {
			return root;
		}

		for (auto &entry : std::filesystem::directory_iterator(dir, ec)) {
			QString name = QString::fromStdString(entry.path().filename().string());
			if (entry.is_directory(ec)) {
				auto *sub = new QTreeWidgetItem(root, {name});
				sub->setIcon(0, folder_icon);
				for (auto &sub_entry : std::filesystem::directory_iterator(entry.path(), ec)) {
					auto *file_item = new QTreeWidgetItem(sub, {QString::fromStdString(sub_entry.path().filename().string())});
					if (sub_entry.is_directory(ec)) {
						file_item->setIcon(0, folder_icon);
					} else {
						file_item->setIcon(0, file_icon);
						file_item->setData(0, Qt::UserRole,
							QString::fromStdString(sub_entry.path().string()));
					}
				}
			} else {
				auto *file_item = new QTreeWidgetItem(root, {name});
				file_item->setIcon(0, file_icon);
				file_item->setData(0, Qt::UserRole,
					QString::fromStdString(entry.path().string()));
			}
		}
		return root;
	};

	expand_folder(cfg.data_path, "Data");

	explorer_tree->expandAll();
}

void main_window::refresh_object_tree() const {
	if (!object_tree || !cmngr) {
		return;
	}

	object_tree->blockSignals(true);
	object_tree->clear();

	const auto &cuboids = cmngr->get_cuboids();
	for (const auto &c : cuboids) {
		auto *item = new QTreeWidgetItem();
		item->setText(0, "#" + QString::number(c.id));
		item->setText(1, c.class_name.isEmpty() ? "Unknown" : c.class_name);
		item->setData(0, Qt::UserRole, c.id);
		object_tree->addTopLevelItem(item);
		if (c.id == cmngr->get_selected_id()) {
			object_tree->setCurrentItem(item);
		}
	}

	object_tree->blockSignals(false);
}

void main_window::notify(const QString &message, int type) {
	if (notification) {
		notification->notify(message, static_cast<notification_widget::Type>(type));
	}
}

void main_window::load_file(const QString &path) {
	if (!project_loader || !project_loader->is_project_name_valid()) {
		notify("No project is currently loaded. Please create or open a project first.", 2);
		return;
	}

	if (cmngr) {
		cmngr->clear();
		refresh_object_tree();
	}

	current_data = std::monostate{};
	current_frame_id = QFileInfo(path).baseName();

	const auto &format = project_loader->read_config().format;
	if (viewport) {
		viewport->show_loading_overlay(QStringLiteral("Loading Point Cloud Data"));
	}
	loader->call_loader(path, format);
}

void main_window::process_overlays() {
	if (!viewport_container || !notification || !statistics) {
		return;
	}

	const int viewport_width = viewport_container->width();

	notification->move(16, 16);
	notification->raise();

	const int sx = viewport_width - statistics->width() - 16;
	statistics->move(sx, 16);
	statistics->raise();
}

bool main_window::eventFilter(QObject *object, QEvent *event) {
	if (object == viewport_container && event->type() == QEvent::Resize) {
		process_overlays();
	}
	return QMainWindow::eventFilter(object, event);
}
