#pragma once
#include "core/FlowDocument.h"
#include <QStringList>

class FlowRepository {
public:
  explicit FlowRepository(QString root = {});
  bool save(const FlowDocument& document, QString* error = nullptr) const;
  QVector<FlowDocument> loadAll(QStringList* warnings = nullptr) const;

private:
  QString root_;
};
