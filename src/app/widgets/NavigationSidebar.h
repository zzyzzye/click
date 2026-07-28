#pragma once

#include <QFrame>

class QLabel;
class QListWidget;

enum class ShellPage {
  ClickSettings = 0,
  Hotkeys = 1,
  PresetsAbout = 2
};
Q_DECLARE_METATYPE(ShellPage)

class NavigationSidebar final : public QFrame {
  Q_OBJECT

 public:
  explicit NavigationSidebar(QWidget* parent = nullptr);

  int pageCount() const;
  ShellPage currentPage() const;
  void setCurrentPage(ShellPage page);
  QString productName() const;
  QString versionText() const;

 signals:
  void pageSelected(ShellPage page);

 private:
  QLabel* productLabel_ = nullptr;
  QLabel* versionLabel_ = nullptr;
  QListWidget* navigation_ = nullptr;
};
