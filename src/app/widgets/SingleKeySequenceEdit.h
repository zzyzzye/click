#pragma once

#include <QKeySequenceEdit>

// 连点和流程步骤只接受一个按键组合，兼容 Linux 使用的 Qt 6.4。
class SingleKeySequenceEdit final : public QKeySequenceEdit {
public:
  explicit SingleKeySequenceEdit(QWidget* parent = nullptr) : QKeySequenceEdit(parent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    setMaximumSequenceLength(1);
#else
    connect(this, &QKeySequenceEdit::keySequenceChanged, this,
            [this](const QKeySequence& sequence) {
              if (sequence.count() > 1)
                setKeySequence(QKeySequence(sequence[0]));
            });
#endif
  }
};
