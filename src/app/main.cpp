#include <QApplication>
#include <QIcon>

#include "app/AppIdentity.h"
#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  applyApplicationIdentity();
  QApplication::setWindowIcon(QIcon(":/clickflow/icons/ClickFlow.png"));

  MainWindow window;
  window.show();

  return app.exec();
}
