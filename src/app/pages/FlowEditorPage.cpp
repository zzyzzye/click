#include "app/pages/FlowEditorPage.h"
#include "app/widgets/SingleKeySequenceEdit.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QScrollArea>
#include <QVBoxLayout>

FlowEditorPage::FlowEditorPage(QWidget* parent) : QWidget(parent) {
  setObjectName("flowEditorPage");
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(12);
  root->setSizeConstraint(QLayout::SetMinimumSize);
  editing_ = new QWidget(this);
  auto* content = new QVBoxLayout(editing_);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(12);
  auto card = [&](const QString& title) {
    auto* frame = new QFrame(editing_);
    frame->setObjectName("settingsCard");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);
    auto* label = new QLabel(title, frame);
    label->setObjectName("cardTitle");
    layout->addWidget(label);
    content->addWidget(frame);
    return layout;
  };
  auto* header = card("本机流程");
  header->setContentsMargins(18, 12, 18, 12);
  header->setSpacing(8);
  auto* row = new QHBoxLayout;
  library_ = new QComboBox(this);
  library_->setObjectName("flowLibrary");
  library_->setMinimumContentsLength(8);
  library_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  library_->setAccessibleName("已保存的本机流程");
  row->addWidget(library_, 1);
  auto button = [&](const QString& title, const QString& id, QHBoxLayout* layout) {
    auto* b = new QPushButton(title, this);
    b->setObjectName(id);
    layout->addWidget(b);
    return b;
  };
  open_ = button("打开", "flowOpen", row);
  auto* fresh = button("新建", "flowNew", row);
  header->addLayout(row);
  name_ = new QLineEdit(this);
  name_->setObjectName("flowName");
  name_->setAccessibleName("流程名称");
  name_->setPlaceholderText("流程名称");
  name_->setToolTip("当前流程的名称，点击“保存流程”后写入本机。");
  name_->setMinimumWidth(0);
  row->insertWidget(0, name_, 1);
  savedLabel_ = new QLabel(this);
  savedLabel_->setWordWrap(true);
  header->addWidget(savedLabel_);
  auto* sequence = card("按顺序执行 · 一轮包含全部步骤");
  auto* addRow = new QHBoxLayout;
  kind_ = new QComboBox(this);
  kind_->setObjectName("flowStepKind");
  kind_->setAccessibleName("要添加的步骤类型");
  kind_->addItems({"点击 / 多点编排", "按键组合", "输入文字", "等待"});
  addRow->addWidget(kind_, 1);
  add_ = button("添加步骤", "flowAdd", addRow);
  sequence->addLayout(addRow);
  steps_ = new QListWidget(this);
  steps_->setObjectName("flowSteps");
  steps_->setAccessibleName("流程步骤，按执行顺序排列");
  steps_->setWordWrap(true);
  steps_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  steps_->setMinimumHeight(140);
  steps_->setMaximumHeight(200);
  sequence->addWidget(steps_);
  auto* tools = new QHBoxLayout;
  copy_ = button("复制", "flowCopy", tools);
  remove_ = button("删除", "flowRemove", tools);
  remove_->setToolTip("删除所选步骤；可使用“撤销”恢复。");
  up_ = button("上移", "flowUp", tools);
  down_ = button("下移", "flowDown", tools);
  undo_ = button("撤销", "flowUndo", tools);
  redo_ = button("重做", "flowRedo", tools);
  sequence->addLayout(tools);
  auto* propertyCard = card("所选步骤");
  propertyHint_ = new QLabel("先添加步骤，再在这里设置操作内容。", this);
  propertyHint_->setObjectName("flowPropertyHint");
  propertyHint_->setWordWrap(true);
  propertyCard->addWidget(propertyHint_);
  properties_ = new QFormLayout;
  properties_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  propertyCard->addLayout(properties_);
  auto spin = [&](const QString& label, const QString& id, int min, int max) {
    auto* s = new QSpinBox(this);
    s->setObjectName(id);
    s->setRange(min, max);
    s->setAccessibleName(label);
    properties_->addRow(label, s);
    return s;
  };
  x_ = spin("屏幕 X", "flowX", -100000, 100000);
  y_ = spin("屏幕 Y", "flowY", -100000, 100000);
  button_ = new QComboBox(this);
  button_->addItems({"左键", "右键"});
  button_->setObjectName("flowMouseButton");
  properties_->addRow("鼠标按键", button_);
  count_ = spin("点击次数", "flowCount", 1, 10000);
  wait_ = spin("等待（毫秒）", "flowWait", 0, 3600000);
  key_ = new SingleKeySequenceEdit(this);
  key_->setObjectName("flowKey");
  key_->setAccessibleName("步骤按键或组合键");
  properties_->addRow("按键组合", key_);
  text_ = new QPlainTextEdit(this);
  text_->setObjectName("flowText");
  text_->setMaximumHeight(100);
  text_->setAccessibleName("要输入的文字");
  properties_->addRow("文字（保存时明文存储）", text_);
  rawHint_ = new QLabel(
      "原始录制完整保留移动、滚轮、拖拽和按下/释放。可移动、复制或删除整个块；仅在块结束后暂停。",
      this);
  rawHint_->setWordWrap(true);
  properties_->addRow(rawHint_);
  auto* capture = new QPushButton("3 秒后捕获当前位置", this);
  capture->setObjectName("flowCapture");
  properties_->addRow(capture);
  connect(capture, &QPushButton::clicked, this, &FlowEditorPage::captureRequested);
  auto* screenHint = new QLabel(
      "多个点位：重复添加点击步骤。使用屏幕坐标，切换显示器或缩放后须检查全部位置。", this);
  screenHint->setWordWrap(true);
  propertyCard->addWidget(screenHint);
  auto* confirm = new QPushButton("确认当前屏幕布局", this);
  confirm->setObjectName("flowConfirmScreens");
  propertyCard->addWidget(confirm);
  connect(confirm, &QPushButton::clicked, this, &FlowEditorPage::confirmScreensRequested);
  auto* options = card("整体循环");
  auto* form = new QFormLayout;
  options->addLayout(form);
  infinite_ = new QCheckBox("无限循环（不显示百分比）", this);
  infinite_->setObjectName("flowInfinite");
  form->addRow(infinite_);
  rounds_ = new QSpinBox(this);
  rounds_->setObjectName("flowRounds");
  rounds_->setAccessibleName("流程执行轮数");
  rounds_->setRange(1, 1000000);
  form->addRow("执行轮数", rounds_);
  loopDelay_ = new QSpinBox(this);
  loopDelay_->setObjectName("flowLoopDelay");
  loopDelay_->setAccessibleName("每轮等待毫秒数");
  loopDelay_->setRange(0, 3600000);
  form->addRow("每轮等待（毫秒）", loopDelay_);
  root->addWidget(editing_);
  actions_ = new QFrame(this);
  actions_->setObjectName("settingsCard");
  auto* actionLayout = new QVBoxLayout(actions_);
  actionLayout->setContentsMargins(16, 12, 16, 12);
  progress_ = new QLabel("添加步骤后可先试运行一轮。运行会操作真实桌面。", actions_);
  progress_->setWordWrap(true);
  progress_->setObjectName("flowProgress");
  actionLayout->addWidget(progress_);
  message_ = new QLabel(actions_);
  message_->setObjectName("flowMessage");
  message_->setWordWrap(true);
  actionLayout->addWidget(message_);
  auto* actionRow = new QHBoxLayout;
  save_ = button("保存流程", "flowSave", actionRow);
  trial_ = button("试运行一次", "flowTrial", actionRow);
  run_ = button("开始", "flowRun", actionRow);
  run_->setProperty("primary", true);
  pause_ = button("暂停", "flowPause", actionRow);
  stop_ = button("停止", "flowStop", actionRow);
  actionLayout->addLayout(actionRow);
  root->addWidget(actions_);
  root->addStretch();
  connect(fresh, &QPushButton::clicked, this, &FlowEditorPage::newRequested);
  connect(open_, &QPushButton::clicked, this,
          [this] { emit openRequested(library_->currentData().toString()); });
  connect(library_, &QComboBox::currentIndexChanged, this, [this] { updateActions(); });
  connect(save_, &QPushButton::clicked, this, &FlowEditorPage::saveRequested);
  connect(run_, &QPushButton::clicked, this, [this] { emit runRequested(false); });
  connect(trial_, &QPushButton::clicked, this, [this] { emit runRequested(true); });
  connect(pause_, &QPushButton::clicked, this, &FlowEditorPage::pauseRequested);
  connect(stop_, &QPushButton::clicked, this, &FlowEditorPage::stopRequested);
  connect(add_, &QPushButton::clicked, this,
          [this] { addStep(FlowStepKind(kind_->currentIndex())); });
  connect(steps_, &QListWidget::currentRowChanged, this, [this] {
    if (!rendering_)
      showProperties();
  });
  connect(copy_, &QPushButton::clicked, this, [this] {
    int i = steps_->currentRow();
    if (i < 0 || i >= document_.steps.size() || document_.steps.size() >= 1000)
      return;
    auto next = document_;
    auto step = next.steps[i];
    step.id = FlowTypes::createStep(step.kind).id;
    next.steps.insert(i + 1, step);
    commit(next);
    render(i + 1);
  });
  connect(remove_, &QPushButton::clicked, this, [this] {
    int i = steps_->currentRow();
    if (i < 0 || i >= document_.steps.size())
      return;
    auto next = document_;
    next.steps.removeAt(i);
    commit(next);
    render(qMin(i, int(next.steps.size()) - 1));
  });
  auto move = [this](int offset) {
    int i = steps_->currentRow(), j = i + offset;
    if (i < 0 || j < 0 || j >= document_.steps.size())
      return;
    auto next = document_;
    next.steps.move(i, j);
    commit(next);
    render(j);
  };
  connect(up_, &QPushButton::clicked, this, [move] { move(-1); });
  connect(down_, &QPushButton::clicked, this, [move] { move(1); });
  connect(undo_, &QPushButton::clicked, this, [this] {
    if (historyIndex_ > 0) {
      document_ = history_[--historyIndex_];
      render();
    }
  });
  connect(redo_, &QPushButton::clicked, this, [this] {
    if (historyIndex_ + 1 < history_.size()) {
      document_ = history_[++historyIndex_];
      render();
    }
  });
  for (auto* s : {x_, y_, count_, wait_, rounds_, loopDelay_})
    connect(s, &QSpinBox::valueChanged, this, [this] { updateProperties(); });
  connect(button_, &QComboBox::currentIndexChanged, this, [this] { updateProperties(); });
  connect(infinite_, &QCheckBox::toggled, this, [this] { updateProperties(); });
  connect(key_, &QKeySequenceEdit::editingFinished, this, [this] { updateProperties(); });
  connect(text_, &QPlainTextEdit::textChanged, this, [this] { updateProperties(); });
  connect(name_, &QLineEdit::textEdited, this, [this] { updateProperties(); });
  setDocument(FlowTypes::create(), false);
  setRunning(MacroControllerState::Idle);
}

bool FlowEditorPage::isDirty() const {
  return document_ != saved_;
}
void FlowEditorPage::setDocument(const FlowDocument& d, bool saved) {
  document_ = d;
  persisted_ = saved;
  history_ = {d};
  historyIndex_ = 0;
  saved_ = d;
  progress_->setText("添加步骤后可先试运行一轮。运行会操作真实桌面。");
  setMessage({});
  render(0);
}
void FlowEditorPage::markSaved() {
  saved_ = document_;
  persisted_ = true;
  render();
}
void FlowEditorPage::commit(const FlowDocument& next) {
  if (next == document_)
    return;
  history_.resize(historyIndex_ + 1);
  history_.append(next);
  if (history_.size() > 50)
    history_.removeFirst();
  historyIndex_ = history_.size() - 1;
  document_ = next;
  updateSavedLabel();
  updateActions();
}
void FlowEditorPage::render(int selected) {
  if (selected == -2)
    selected = steps_->currentRow();
  rendering_ = true;
  name_->setText(document_.name);
  infinite_->setChecked(document_.playback.infinite);
  rounds_->setValue(document_.playback.repeatCount);
  rounds_->setEnabled(!document_.playback.infinite);
  loopDelay_->setValue(document_.playback.loopDelayMs);
  steps_->clear();
  for (int i = 0; i < document_.steps.size(); ++i)
    steps_->addItem(QString("%1   %2").arg(i + 1, 2).arg(FlowTypes::summary(document_.steps[i])));
  if (document_.steps.isEmpty()) {
    steps_->addItem("还没有步骤。选择类型并点击“添加步骤”。");
    steps_->item(0)->setFlags(Qt::NoItemFlags);
  } else
    steps_->setCurrentRow(qBound(0, selected, int(document_.steps.size()) - 1));
  updateSavedLabel();
  rendering_ = false;
  showProperties();
}
void FlowEditorPage::showProperties() {
  rendering_ = true;
  int i = steps_->currentRow();
  bool valid = i >= 0 && i < document_.steps.size();
  propertyHint_->setVisible(!valid);
  FlowStep s = valid ? document_.steps[i] : FlowStep{};
  bool click = valid && s.kind == FlowStepKind::Click;
  for (auto* w : QVector<QWidget*>{x_, y_, count_, button_})
    properties_->setRowVisible(w, click);
  properties_->setRowVisible(findChild<QPushButton*>("flowCapture"), click);
  properties_->setRowVisible(wait_, valid && (click || s.kind == FlowStepKind::Wait));
  properties_->setRowVisible(key_, valid && s.kind == FlowStepKind::Key);
  properties_->setRowVisible(text_, valid && s.kind == FlowStepKind::Text);
  properties_->setRowVisible(rawHint_, valid && s.kind == FlowStepKind::Recording);
  x_->setValue(s.point.x());
  y_->setValue(s.point.y());
  count_->setValue(s.count);
  wait_->setValue(s.waitMs);
  button_->setCurrentIndex(s.button == MacroMouseButton::Right ? 1 : 0);
  key_->setKeySequence(QKeySequence::fromString(s.value, QKeySequence::PortableText));
  text_->setPlainText(s.value);
  rendering_ = false;
  updateActions();
}
void FlowEditorPage::updateSavedLabel() {
  savedLabel_->setText(isDirty()    ? "流程尚未保存 · 仅在本次会话保留"
                       : persisted_ ? "流程已保存到本机"
                                    : "新流程 · 尚未保存到本机");
}
void FlowEditorPage::updateActions() {
  const bool editable = !busy_ && !blocked_;
  const int index = steps_->currentRow();
  const bool selected = index >= 0 && index < document_.steps.size();
  const bool hasSteps = !document_.steps.isEmpty();
  const bool canAdd = document_.steps.size() < 1000;
  open_->setEnabled(editable && !library_->currentData().toString().isEmpty());
  add_->setEnabled(editable && canAdd);
  add_->setToolTip(canAdd ? "添加到所选步骤后面" : "已达到 1000 个步骤上限，请先删除步骤。");
  copy_->setEnabled(editable && selected && canAdd);
  remove_->setEnabled(editable && selected);
  up_->setEnabled(editable && selected && index > 0);
  down_->setEnabled(editable && selected && index + 1 < document_.steps.size());
  undo_->setEnabled(editable && historyIndex_ > 0);
  redo_->setEnabled(editable && historyIndex_ + 1 < history_.size());
  save_->setEnabled(editable && hasSteps);
  run_->setEnabled(editable && hasSteps);
  trial_->setEnabled(editable && hasSteps);
}
void FlowEditorPage::updateProperties() {
  if (rendering_)
    return;
  auto next = document_;
  next.name = name_->text();
  next.playback.infinite = infinite_->isChecked();
  next.playback.repeatCount = rounds_->value();
  next.playback.loopDelayMs = loopDelay_->value();
  rounds_->setEnabled(!next.playback.infinite);
  int i = steps_->currentRow();
  if (i >= 0 && i < next.steps.size()) {
    auto& s = next.steps[i];
    if (s.kind == FlowStepKind::Click) {
      s.point = {x_->value(), y_->value()};
      s.count = count_->value();
      s.button = button_->currentIndex() ? MacroMouseButton::Right : MacroMouseButton::Left;
    }
    if (s.kind == FlowStepKind::Click || s.kind == FlowStepKind::Wait)
      s.waitMs = wait_->value();
    if (s.kind == FlowStepKind::Key)
      s.value = key_->keySequence().toString(QKeySequence::PortableText);
    if (s.kind == FlowStepKind::Text)
      s.value = text_->toPlainText();
  }
  commit(next);
  if (i >= 0 && i < document_.steps.size())
    steps_->item(i)->setText(
        QString("%1   %2").arg(i + 1, 2).arg(FlowTypes::summary(document_.steps[i])));
}
void FlowEditorPage::addStep(FlowStepKind kind) {
  if (document_.steps.size() >= 1000) {
    setMessage("最多添加 1000 个步骤。", true);
    return;
  }
  auto next = document_;
  int index = qBound(0, steps_->currentRow() + 1, int(next.steps.size()));
  next.steps.insert(index, FlowTypes::createStep(kind));
  commit(next);
  render(index);
}
void FlowEditorPage::addRecording(const MacroSequence& recording) {
  auto next = document_;
  auto step = FlowTypes::createStep(FlowStepKind::Recording);
  step.recording = recording;
  next.steps.append(step);
  commit(next);
  render(next.steps.size() - 1);
}
void FlowEditorPage::capturePoint(const QPoint& p, const QVector<QRect>& screens) {
  int i = steps_->currentRow();
  if (i < 0 || i >= document_.steps.size() || document_.steps[i].kind != FlowStepKind::Click)
    return;
  auto next = document_;
  next.steps[i].point = p;
  if (next.screens.isEmpty())
    next.screens = screens;
  commit(next);
  render();
}
void FlowEditorPage::confirmScreens(const QVector<QRect>& screens) {
  auto next = document_;
  next.screens = screens;
  commit(next);
  render();
}
void FlowEditorPage::setLibrary(const QVector<FlowDocument>& docs) {
  const QString selected = library_->currentData().toString();
  library_->clear();
  library_->addItem("选择已保存的流程", "");
  for (const auto& d : docs)
    library_->addItem(QString("%1 · %2 步 · %3").arg(d.name).arg(d.steps.size()).arg(d.id.left(8)),
                      d.id);
  const int index = library_->findData(persisted_ ? document_.id : selected);
  if (index >= 0)
    library_->setCurrentIndex(index);
  updateActions();
}
void FlowEditorPage::setCapturing() {
  setRunning(MacroControllerState::Idle, true);
  stop_->setEnabled(true);
}
void FlowEditorPage::setPausePending() {
  pause_->setEnabled(false);
  pause_->setText("等待暂停");
}
void FlowEditorPage::setRunning(MacroControllerState state, bool blocked) {
  bool busy = state != MacroControllerState::Idle;
  busy_ = busy;
  blocked_ = blocked;
  editing_->setEnabled(!busy && !blocked);
  updateActions();
  pause_->setEnabled(busy && !blocked);
  pause_->setText(state == MacroControllerState::Paused ? "继续" : "暂停");
  stop_->setEnabled(busy && !blocked);
}
void FlowEditorPage::setMessage(const QString& m, bool error) {
  message_->setText(m);
  message_->setProperty("error", error);
  message_->style()->unpolish(message_);
  message_->style()->polish(message_);
}
void FlowEditorPage::setProgress(int step, int round, int completed) {
  progress_->setText(
      QString("第 %1 轮%2 · %3 · 已完成 %4 轮")
          .arg(round)
          .arg(runOptions_.infinite ? "（无限）" : QString(" / %1").arg(runOptions_.repeatCount))
          .arg(step >= 0 ? QString("步骤 %1 / %2").arg(step + 1).arg(document_.steps.size())
                         : "本轮结束")
          .arg(completed));
  if (step >= 0)
    selectStep(step);
}
void FlowEditorPage::selectStep(int index) {
  if (index >= 0 && index < document_.steps.size())
    steps_->setCurrentRow(index);
}
void FlowEditorPage::focusInvalidStep(int index) {
  selectStep(index);
  QWidget* field = name_;
  if (index >= 0 && index < document_.steps.size()) {
    switch (document_.steps[index].kind) {
    case FlowStepKind::Click:
      field = x_;
      break;
    case FlowStepKind::Key:
      field = key_;
      break;
    case FlowStepKind::Text:
      field = text_;
      break;
    case FlowStepKind::Wait:
      field = wait_;
      break;
    case FlowStepKind::Recording:
      field = steps_;
      break;
    }
  }
  field->setFocus(Qt::OtherFocusReason);
  for (auto* ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
    if (auto* scroll = qobject_cast<QScrollArea*>(ancestor)) {
      scroll->ensureWidgetVisible(field, 0, 24);
      break;
    }
  }
}
