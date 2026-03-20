#pragma once

#include "data_loader.h"

#include <QAction>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

class QTreeView;
class QFileSystemModel;

class project_manager;
class notification_widget;
class camera_frame;
class lidar_viewport;
class orthographic_viewport;
class statistics_widget;
class cuboid_manager;
class calibration_store;
class camera_manager;

class main_window : public QMainWindow {
	Q_OBJECT

	public:
	main_window(QWidget *parent = nullptr);
	~main_window();

	private slots:
	void new_project();
	void open_project();
	void tool_mode_changed(int tool_index);

	private:
	void process_window();
	void process_menubar();
	void process_toolbar();
	void process_viewport();
	void process_camera_menu();
	void process_overlays();

	bool eventFilter(QObject *object, QEvent *event) override;

	void process_object_menu();
	void process_explorer_tab();
	void process_object_tab();
	void expand_explorer();
	void refresh_object_tree() const;

	void update_orthographic_viewport();

	void notify(const QString &message, int type = 0);

	void load_file(const QString &path);

	void load_qss();

	QWidget *central_widget;

	QHBoxLayout *horizontal_layout;
	QVBoxLayout *vertical_layout;

	QWidget *viewport_container;
	lidar_viewport *viewport = nullptr;

	QWidget *toolbar_widget;
	QVBoxLayout *toolbar_layout;
	QVector<QToolButton *> toolbar_buttons;
	int tool_mode = -1;

	QWidget *camera_container;
	orthographic_viewport *orthographic_top = nullptr;
	orthographic_viewport *orthographic_front = nullptr;
	orthographic_viewport *orthographic_side = nullptr;
	camera_frame *media_camera;
	QLabel *media_placeholder = nullptr;

	QTabWidget *object_container;

	QWidget *explorer_tab;
	QTreeWidget *explorer_tree;

	QWidget *object_tab;
	QSplitter *object_splitter;

	QLabel *property_title;
	QWidget *property_container;
	QTreeWidget *property_tree;

	QLabel *object_list_title;
	QWidget *object_list_container;
	QTreeWidget *object_tree;

	notification_widget *notification = nullptr;
	statistics_widget *statistics = nullptr;

	QMenu *file_menu;
	QMenu *edit_menu;
	QMenu *view_menu;
	QMenu *tools_menu;
	QMenu *help_menu;

	std::unique_ptr<project_manager> project_loader;
	std::unique_ptr<data_loader> loader;
	std::unique_ptr<cuboid_manager> cmngr;
	std::unique_ptr<calibration_store> cstore;
	std::unique_ptr<camera_manager> media_manager;
	data_variants current_data;
	QString current_frame_id;
};
