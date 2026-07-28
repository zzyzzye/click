#include "app/AppIdentity.h"

#include <QCoreApplication>
#include <QGuiApplication>

#include "ClickFlowVersion.h"

void applyApplicationIdentity() {
  QCoreApplication::setOrganizationName("OpenAI");
  QCoreApplication::setApplicationName("QtClicker");
  QCoreApplication::setApplicationVersion(ClickFlowVersion::string);
  QGuiApplication::setApplicationDisplayName("ClickFlow");
}
