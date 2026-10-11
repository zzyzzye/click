#pragma once

#include "core/MacroTypes.h"
#include <QRect>

enum class FlowStepKind { Click, Key, Text, Wait, Recording };

struct FlowStep {
  bool operator==(const FlowStep&) const = default;
  QString id;
  FlowStepKind kind = FlowStepKind::Click;
  QPoint point;
  MacroMouseButton button = MacroMouseButton::Left;
  int count = 1;
  int waitMs = 100;
  QString value;
  MacroSequence recording;
};

struct FlowDocument {
  bool operator==(const FlowDocument&) const = default;
  QString id;
  QString name = "新流程";
  QVector<FlowStep> steps;
  MacroPlaybackSettings playback;
  QVector<QRect> screens;
};

namespace FlowTypes {
FlowDocument create();
FlowStep createStep(FlowStepKind kind);
bool validate(const FlowDocument& document, QString* error = nullptr, int* invalidStep = nullptr);
QString summary(const FlowStep& step);
QJsonObject toJson(const FlowDocument& document);
std::optional<FlowDocument> fromJson(const QJsonObject& object, QString* error = nullptr);
std::optional<MacroSequence> compile(const FlowDocument& document, QString* error = nullptr);
} // namespace FlowTypes
