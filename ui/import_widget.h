#pragma once

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

class import_widget : public QDialog {
	Q_OBJECT

	public:
	explicit import_widget(QWidget *parent = nullptr);

	QString project_name() const { return project_name_input->text().trimmed(); }
	QString data_format() const;
	QString data_path() const { return data_path_input->text(); }
	QString calibration_path() const { return calibration_path_input->text(); }
	QString media_path() const { return media_path_input->text(); }
	QString label_path() const { return label_path_input->text(); }

	protected:
	bool event(QEvent *event) override;

	private:
	struct dataset_info {
		QString name;
		QString stability;
		QString precision;
		QString extension;
		QString variables;
	};

	void build_data_page();
	void build_config_page();

	QWidget *build_indicator();
	void update_indicator(int current_step);

	void select_dataset(int index);
	void browse_folder(QLineEdit *target, const QString &dialog_title);

	void change_step(int step);

	QWidget *step_bar;
	QLabel *data_step_label;
	QLabel *config_step_label;
	QWidget *data_step_logo;
	QWidget *config_step_logo;
	QWidget *connector_line;

	QWidget *data_page;
	QVector<QWidget *> dataset_cards;
	QWidget *project_info_panel;
	QLineEdit *project_name_input;
	int selected_dataset_index = -1;

	QWidget *config_page;
	QLineEdit *data_path_input;
	QLineEdit *calibration_path_input;
	QLineEdit *media_path_input;
	QLineEdit *label_path_input;

	QPushButton *cancel_button;
	QPushButton *back_button;
	QPushButton *next_button;
	QPushButton *create_button;

	QVBoxLayout *root_layout;

	static const QVector<dataset_info> &dataset();
};
