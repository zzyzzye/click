#pragma once

#include <QObject>

#include "core/MacroTypes.h"

class MacroPlayer : public QObject {
  Q_OBJECT

 public:
  explicit MacroPlayer(QObject* parent = nullptr) : QObject(parent) {}
  ~MacroPlayer() override = default;

  virtual bool prepare(const MacroSequence& sequence, QString* error = nullptr) = 0;
  virtual bool inject(const MacroEvent& event, QString* error = nullptr) = 0;
  virtual void releaseAll() = 0;
  virtual void cancel() = 0;
};
