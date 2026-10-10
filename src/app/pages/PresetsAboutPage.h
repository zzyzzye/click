#pragma once
#include <QWidget>
class QLabel; class QListWidget; class QPushButton;
class PresetsAboutPage final : public QWidget {
  Q_OBJECT
 public:
  explicit PresetsAboutPage(QWidget* parent = nullptr);
  void setPresetNames(const QStringList& names, const QString& selected = {});
  QString selectedPresetName() const;
  QString productName() const;
  QString versionText() const;
  QString platformText() const;
  void setMutationEnabled(bool enabled);
  void setUpdateStatus(const QString& status);
  void setUpdateAction(const QString& text, bool enabled);
 signals:
  void newRequested(); void saveRequested(); void renameRequested();
  void deleteRequested(); void loadRequested(); void selectionChanged();
  void updateRequested();
 private:
  QLabel* emptyHint_ = nullptr;
  void updateActions();
  QListWidget* list_ = nullptr;
  QPushButton* new_ = nullptr; QPushButton* save_ = nullptr; QPushButton* rename_ = nullptr;
  QPushButton* delete_ = nullptr; QPushButton* load_ = nullptr;
  QLabel* product_ = nullptr; QLabel* version_ = nullptr; QLabel* platform_ = nullptr;
  QLabel* updateStatus_ = nullptr;
  QPushButton* update_ = nullptr;
};
