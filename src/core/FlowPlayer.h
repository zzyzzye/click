#pragma once
#include "core/MacroPlayer.h"
#include "core/ClickBackend.h"
#include "core/FlowDocument.h"
#include <QElapsedTimer>

// 高级动作适配到现有输入服务，计时、循环和执行权仍由 MacroController 管理。
class FlowPlayer final : public MacroPlayer {
public:
  FlowPlayer(ClickBackend* backend, MacroPlayer* rawPlayer);
  void setDocument(const FlowDocument& document);
  bool prepare(const MacroSequence& sequence, QString* error = nullptr) override;
  bool inject(const MacroEvent& event, QString* error = nullptr) override;
  bool revalidate(QString* error = nullptr) override;
  void releaseAll() override;
  void cancel() override;
  int invalidStep() const {
    return invalidStep_;
  }

private:
  ClickBackend* backend_;
  MacroPlayer* raw_;
  FlowDocument document_;
  bool rawPrepared_ = false;
  bool hasPoints_ = false;
  int lastStep_ = -1;
  QElapsedTimer checkClock_;
  int invalidStep_ = -1;
};
