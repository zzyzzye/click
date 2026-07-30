#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/MacroTypes.h"

class MacroRepository {
 public:
  explicit MacroRepository(QString rootPath = {});

  bool save(const MacroSequence& sequence, QString* error = nullptr) const;
  QVector<MacroSequence> loadAll(QStringList* warnings = nullptr) const;
  bool rename(const QString& id, const QString& newName,
              QString* error = nullptr) const;
  bool remove(const QString& id, QString* error = nullptr) const;
  QString rootPath() const;

 private:
  QString filePath(const QString& id) const;

  QString rootPath_;
};
