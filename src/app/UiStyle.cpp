#include "app/UiStyle.h"

QString clickFlowStyleSheet() {
  return QStringLiteral(R"(
    QMainWindow, #contentSurface { background: #f4f5f7; color: #18202b; }
    #navigationSidebar { background: #e9ecf1; border-right: 1px solid #d4d9e1; }
    #productName { font-size: 22px; font-weight: 700; color: #14213d; }
    #productVersion { color: #6b7280; }
    #sidebarNavigation {
      background: transparent; border: none; outline: none;
    }
    #sidebarNavigation::item { border-radius: 8px; padding-left: 12px; }
    #sidebarNavigation::item:selected { background: #2563eb; color: white; }
    #settingsCard, #statusStrip, #actionBar {
      background: white; border: 1px solid #dfe3e8; border-radius: 10px;
    }
    #cardTitle { font-size: 16px; font-weight: 650; }
    QPushButton {
      min-height: 38px; max-height: 38px;
      background: white; color: #273244;
      border: 1px solid #cfd5dd; border-radius: 8px;
      padding: 0 16px; font-weight: 550;
    }
    QPushButton:hover {
      background: #f7f9fc; border-color: #9eabc0;
    }
    QPushButton:pressed {
      background: #edf1f7; border-color: #7f8da3;
    }
    QPushButton:focus { border-color: #2563eb; }
    QPushButton:disabled {
      color: #9aa3b2; background: #f5f6f8; border-color: #e1e5ea;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton {
      min-height: 44px; max-height: 44px;
      color: white; border: 0; border-radius: 9px;
      padding: 0 22px; font-weight: 650;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton { background: #2563eb; }
    QPushButton#macroPlayButton { background: #173b66; }
    QPushButton#startStopButton:hover,
    QPushButton#macroRecordButton:hover { background: #1d4ed8; }
    QPushButton#macroPlayButton:hover { background: #102f55; }
    QPushButton#startStopButton:pressed,
    QPushButton#macroRecordButton:pressed { background: #1e40af; }
    QPushButton#macroPlayButton:pressed { background: #0b2647; }
    QPushButton#startStopButton:disabled,
    QPushButton#macroRecordButton:disabled,
    QPushButton#macroPlayButton:disabled {
      color: #cbd5e1; background: #94a3b8;
    }
    QPushButton#startStopButton[running="true"] { background: #dc2626; }
    QPushButton#startStopButton[running="true"]:hover { background: #b91c1c; }
    QPushButton#startStopButton[running="true"]:pressed { background: #991b1b; }
    QComboBox, QSpinBox, QKeySequenceEdit {
      min-height: 38px; max-height: 38px;
      border: 1px solid #cfd5dd; border-radius: 8px;
      background: white; padding: 0 34px 0 10px;
    }
    QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover {
      border-color: #9eabc0;
    }
    QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus {
      border: 1px solid #2563eb;
    }
    QComboBox::drop-down {
      subcontrol-origin: padding; subcontrol-position: top right;
      width: 30px; margin: 3px; border: none; border-radius: 5px;
    }
    QComboBox::drop-down:hover { background: #edf3ff; }
    QComboBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 12px; height: 8px;
    }
    QSpinBox { padding-right: 32px; }
    QSpinBox::up-button, QSpinBox::down-button {
      subcontrol-origin: border; width: 28px;
      border: none; background: transparent;
    }
    QSpinBox::up-button {
      subcontrol-position: top right; margin: 3px 3px 0 0;
      border-top-left-radius: 5px; border-top-right-radius: 5px;
    }
    QSpinBox::down-button {
      subcontrol-position: bottom right; margin: 0 3px 3px 0;
      border-bottom-left-radius: 5px; border-bottom-right-radius: 5px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
      background: #edf3ff;
    }
    QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
      background: #dce8ff;
    }
    QSpinBox::up-arrow {
      image: url(:/clickflow/icons/chevron-up.svg);
      width: 10px; height: 6px;
    }
    QSpinBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 10px; height: 6px;
    }
    QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled {
      color: #8a94a3; background: #f5f6f8;
    }
  )");
}
