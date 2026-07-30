#pragma once

#include <QStringList>
#include <QVector>

#include "core/MacroTypes.h"

namespace MacroCompressor {

QVector<MacroEvent> compress(const QVector<MacroEvent>& events);
QVector<MacroEvent> removeReservedTail(const QVector<MacroEvent>& events,
                                       const QStringList& reservedHotkeys);

}  // namespace MacroCompressor
