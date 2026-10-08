#include "app/AppIdentity.h"

#include <QCoreApplication>
#include <QGuiApplication>

#include "ClickFlowVersion.h"

void applyApplicationIdentity() {
  QCoreApplication::setOrganizationName("ClickFlow");
  QCoreApplication::setApplicationName("ClickFlow");
  QCoreApplication::setApplicationVersion(ClickFlowVersion::string);
  QGuiApplication::setApplicationDisplayName("ClickFlow");
}
