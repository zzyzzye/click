#include "app/UiStyle.h"

namespace {

QString hex(const QColor& color) { return color.name().toUpper(); }

}  // namespace

const ThemeTokens& fluentLightTokens() {
  static const ThemeTokens tokens{
      QColor("#0067C0"),            // accent
      QColor("#1975C5"),            // accentHover
      QColor("#1669B5"),            // accentPressed
      QColor("#1B1B1B"),            // textPrimary
      QColor("#616161"),            // textSecondary
      QColor("#9E9E9E"),            // textDisabled
      QColor("#C42B1C"),            // danger
      QColor("#B7271C"),            // dangerHover
      QColor("#A5231B"),            // dangerPressed
      QColor("#FDFDFD"),            // controlBackground
      QColor("#F9F9F9"),            // controlBackgroundHover
      QColor("#F5F5F5"),            // controlBackgroundPressed
      QColor(255, 255, 255, 16),    // navItemHover
      QColor(255, 255, 255, 30),    // navItemSelected
      8,                            // controlRadius
      14,                           // cardRadius
      32,                           // controlHeight
  };
  return tokens;
}

QString clickFlowStyleSheet(bool translucent) {
  const ThemeTokens& t = fluentLightTokens();
  const char* surfaceBackground = translucent ? "transparent" : "#F5F7FB";
  const char* contentBackground = translucent ? "transparent" : "#F5F7FB";
  const QString sidebarBackground = hex(t.sidebarBackground);
  return QStringLiteral(R"(
    QWidget {
      font-family: "Segoe UI Variable Text", "Segoe UI", "PingFang SC", "Microsoft YaHei UI", "Noto Sans CJK SC";
      font-size: 13px;
      color: %4;
    }
    QMainWindow, #contentSurface { background: %1; color: %4; }
    #contentPages { background: %2; }
    QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
    #navigationSidebar { background: %3; }
    #productName { font-size: 19px; font-weight: 600; color: white; }
    #productVersion, #sidebarFooter { color: #CAD5E5; font-size: 11px; }
    #navigationSidebar QCheckBox { color: #CAD5E5; font-size: 11px; }
    #sidebarSection { color: #9CADC5; font-size: 11px; padding: 0 10px; }
    #pageTitle { font-size: 26px; font-weight: 600; color: #15243B; }
    #pageDescription { color: %5; padding-bottom: 4px; }
    #actionHint, #progressLabel { color: %5; font-size: 12px; }
    #statusStrip[permissionAvailable="false"] #permissionLabel { color: %13; }
    #statusStrip[permissionAvailable="true"] #permissionLabel { color: %10; }
    #sidebarNavigation { background: transparent; border: none; outline: none; }
    #sidebarNavigation::item { border-radius: 4px; padding-left: 12px; }
    #sidebarNavigation::item:hover { background: rgba(255,255,255,0.06); }
    #sidebarNavigation::item:selected { background: rgba(255,255,255,0.12); color: white; }
    #settingsCard, #statusStrip, #actionBar {
      background: white; border: 1px solid rgba(21,36,59,0.09); border-radius: 14px;
    }
    #statusStrip { border: none; background: transparent; }
    #actionBar { border-color: rgba(0,103,192,0.18); background: #F0F6FD; }
    #cardTitle { font-size: 15px; font-weight: 600; padding-bottom: 4px; }
    QPushButton {
      min-height: 32px; max-height: 32px;
      background: %6; color: %4;
      border: 1px solid rgba(0,0,0,0.08);
      border-bottom: 1px solid rgba(0,0,0,0.16);
      border-radius: 8px;
      padding: 0 12px;
    }
    QPushButton:hover { background: %7; }
    QPushButton:pressed { background: %8; color: %5; }
    QPushButton:focus { border: 2px solid %10; }
    QPushButton:disabled {
      color: %9; background: rgba(0,0,0,0.04); border-color: rgba(0,0,0,0.04);
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton {
      min-height: 36px; max-height: 36px;
      color: white;
      border: 1px solid rgba(255,255,255,0.08);
      border-bottom: 1px solid rgba(0,0,0,0.40);
      border-radius: 8px;
      padding: 0 16px; font-weight: 600;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton { background: %10; }
    QPushButton#startStopButton:hover,
    QPushButton#macroRecordButton:hover,
    QPushButton#macroPlayButton:hover { background: %11; }
    QPushButton#startStopButton:pressed,
    QPushButton#macroRecordButton:pressed,
    QPushButton#macroPlayButton:pressed { background: %12; }
    QPushButton#startStopButton:disabled,
    QPushButton#macroRecordButton:disabled,
    QPushButton#macroPlayButton:disabled {
      color: %9; background: rgba(0,0,0,0.04);
    }
    QPushButton#startStopButton[running="true"] { background: %13; }
    QPushButton#startStopButton[running="true"]:hover { background: %14; }
    QPushButton#startStopButton[running="true"]:pressed { background: %15; }
    QComboBox, QSpinBox, QKeySequenceEdit, QLineEdit {
      min-height: 32px; max-height: 32px;
      border: 1px solid rgba(21,36,59,0.16);
      border-radius: 8px;
      background: #F8FAFD; padding: 0 34px 0 10px;
      selection-color: white; selection-background-color: %10;
    }
    QLineEdit { padding: 0 10px; }
    QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover, QLineEdit:hover {
      background: white; border-color: rgba(0,103,192,0.45);
    }
    QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus, QLineEdit:focus {
      background: white; border: 1px solid %10; border-bottom: 2px solid %10;
    }
    QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled, QLineEdit:disabled {
      color: %9; background: rgba(0,0,0,0.04);
      border-bottom-color: rgba(0,0,0,0.08);
    }
    QComboBox::drop-down {
      subcontrol-origin: padding; subcontrol-position: top right;
      width: 30px; margin: 3px; border: none; border-radius: 4px;
    }
    QComboBox::drop-down:hover { background: rgba(0,0,0,0.06); }
    QComboBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.png);
      width: 12px; height: 8px;
    }
    QComboBox QAbstractItemView {
      background: white; border: 1px solid rgba(0,0,0,0.08);
      border-radius: 8px; padding: 4px; outline: none;
    }
    QComboBox QAbstractItemView::item {
      min-height: 28px; border-radius: 4px; padding-left: 10px;
    }
    QComboBox QAbstractItemView::item:hover { background: rgba(0,0,0,0.04); }
    QComboBox QAbstractItemView::item:selected {
      background: rgba(0,0,0,0.06); color: %4;
    }
    QSpinBox { padding-right: 32px; }
    QSpinBox::up-button, QSpinBox::down-button {
      subcontrol-origin: border; width: 28px;
      border: none; background: transparent;
    }
    QSpinBox::up-button {
      subcontrol-position: top right; margin: 3px 3px 0 0;
      border-top-left-radius: 4px; border-top-right-radius: 4px;
    }
    QSpinBox::down-button {
      subcontrol-position: bottom right; margin: 0 3px 3px 0;
      border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
      background: rgba(0,0,0,0.06);
    }
    QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
      background: rgba(0,0,0,0.10);
    }
    QSpinBox::up-arrow {
      image: url(:/clickflow/icons/chevron-up.png);
      width: 10px; height: 6px;
    }
    QSpinBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.png);
      width: 10px; height: 6px;
    }
    QCheckBox { spacing: 8px; }
    QCheckBox::indicator {
      width: 18px; height: 18px;
      border: 1px solid rgba(0,0,0,0.45); border-radius: 4px;
      background: white;
    }
    QCheckBox::indicator:hover { border-color: rgba(0,0,0,0.60); }
    QCheckBox::indicator:checked {
      background: %10; border-color: %10;
      image: url(:/clickflow/icons/check.png);
    }
    QCheckBox::indicator:checked:hover {
      background: %11; border-color: %11;
    }
    QCheckBox::indicator:disabled {
      background: rgba(0,0,0,0.04); border-color: rgba(0,0,0,0.20);
    }
    QListWidget { outline: none; }
    #presetList { border: 1px solid rgba(21,36,59,0.12); border-radius: 8px; background: #F8FAFD; padding: 4px; }
    #presetEmptyHint { color: %5; padding: 16px 0; }
    QListWidget:focus { border: 1px solid %10; }
    QListWidget::item { border-radius: 4px; min-height: 32px; padding: 2px 8px; }
    QListWidget::item:hover { background: rgba(0,0,0,0.04); }
    QListWidget::item:selected { background: rgba(0,0,0,0.06); color: %4; }
    QScrollBar:vertical {
      background: transparent; width: 12px; margin: 2px 2px 2px 0;
    }
    QScrollBar::handle:vertical {
      background: rgba(0,0,0,0.35); min-height: 32px;
      border-radius: 3px; margin: 0 3px;
    }
    QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.55); }
    QScrollBar::handle:vertical:pressed { background: rgba(0,0,0,0.65); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0; border: none; background: transparent;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
      background: transparent;
    }
    QScrollBar:horizontal {
      background: transparent; height: 12px; margin: 0 2px 2px 2px;
    }
    QScrollBar::handle:horizontal {
      background: rgba(0,0,0,0.35); min-width: 32px;
      border-radius: 3px; margin: 3px 0;
    }
    QScrollBar::handle:horizontal:hover { background: rgba(0,0,0,0.55); }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
      width: 0; border: none; background: transparent;
    }
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
      background: transparent;
    }
    QToolTip {
      background: white; color: %4;
      border: 1px solid rgba(0,0,0,0.10); border-radius: 4px;
      padding: 6px 10px;
    }
    QMenu {
      background: white; border: 1px solid rgba(0,0,0,0.08);
      border-radius: 8px; padding: 4px;
    }
    QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
    QMenu::item:selected { background: rgba(0,0,0,0.06); }
  )")
      .arg(QString::fromLatin1(surfaceBackground),
           QString::fromLatin1(contentBackground),
           sidebarBackground,
           hex(t.textPrimary),                // %4
           hex(t.textSecondary),              // %5
           hex(t.controlBackground),          // %6
           hex(t.controlBackgroundHover),     // %7
           hex(t.controlBackgroundPressed),   // %8
           hex(t.textDisabled),               // %9
           hex(t.accent),                     // %10
           hex(t.accentHover),                // %11
           hex(t.accentPressed),              // %12
           hex(t.danger),                     // %13
           hex(t.dangerHover),                // %14
           hex(t.dangerPressed));             // %15
}
