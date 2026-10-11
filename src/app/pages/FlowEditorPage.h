#pragma once
#include <QWidget>
#include "core/FlowDocument.h"
#include "core/MacroController.h"
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QKeySequenceEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QFormLayout;

class FlowEditorPage final : public QWidget {
  Q_OBJECT
public:
  explicit FlowEditorPage(QWidget* parent = nullptr);
  const FlowDocument& document() const {
    return document_;
  }
  bool isDirty() const;
  void setDocument(const FlowDocument& document, bool saved = true);
  void markSaved();
  void setLibrary(const QVector<FlowDocument>& documents);
  void addRecording(const MacroSequence& sequence);
  void capturePoint(const QPoint& point, const QVector<QRect>& screens);
  void confirmScreens(const QVector<QRect>& screens);
  void setRunning(MacroControllerState state, bool blocked = false);
  void setCapturing();
  void setPausePending();
  void setMessage(const QString& message, bool error = false);
  void setProgress(int step, int round, int completed);
  void setRunOptions(const MacroPlaybackSettings& options) {
    runOptions_ = options;
  }
  void selectStep(int index);
  void focusInvalidStep(int index);
  QWidget* actionPanel() const {
    return actions_;
  }
signals:
  void newRequested();
  void openRequested(const QString& id);
  void saveRequested();
  void runRequested(bool trial);
  void pauseRequested();
  void stopRequested();
  void captureRequested();
  void confirmScreensRequested();

private:
  void commit(const FlowDocument& next);
  void render(int selected = -2);
  void showProperties();
  void updateProperties();
  void addStep(FlowStepKind kind);
  void updateActions();
  void updateSavedLabel();
  FlowDocument document_;
  FlowDocument saved_;
  QVector<FlowDocument> history_;
  int historyIndex_ = 0;
  bool rendering_ = false;
  bool persisted_ = false;
  bool busy_ = false;
  bool blocked_ = false;
  MacroPlaybackSettings runOptions_;
  QWidget* editing_;
  QWidget* actions_;
  QListWidget* steps_;
  QComboBox* library_;
  QComboBox* kind_;
  QLineEdit* name_;
  QSpinBox *x_, *y_, *count_, *wait_, *rounds_, *loopDelay_;
  QComboBox* button_;
  QCheckBox* infinite_;
  QKeySequenceEdit* key_;
  QPlainTextEdit* text_;
  QFormLayout* properties_;
  QLabel *rawHint_, *propertyHint_, *message_, *progress_, *savedLabel_;
  QPushButton *open_, *add_, *copy_, *remove_, *up_, *down_;
  QPushButton *undo_, *redo_, *run_, *trial_, *pause_, *stop_, *save_;
};
