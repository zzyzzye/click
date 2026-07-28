#include <QApplication>

#include "app/AppIdentity.h"
#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  applyApplicationIdentity();

  MainWindow window;
  window.show();

  return app.exec();
}

