#pragma once

#include <QColor>
#include <QString>

// Fluent 浅色设计令牌：全部 UI 颜色、圆角、控件高度的唯一来源。
struct ThemeTokens {
  QColor accent;                    // #0067C0
  QColor accentHover;               // #1975C5
  QColor accentPressed;             // #1669B5
  QColor textPrimary;               // #1B1B1B
  QColor textSecondary;             // #616161
  QColor textDisabled;              // #9E9E9E
  QColor danger;                    // #C42B1C
  QColor dangerHover;               // #B7271C
  QColor dangerPressed;             // #A5231B
  QColor controlBackground;         // #FDFDFD
  QColor controlBackgroundHover;    // #F9F9F9
  QColor controlBackgroundPressed;  // #F5F5F5
  QColor navItemHover;              // rgba(255,255,255,0.50)
  QColor navItemSelected;           // rgba(255,255,255,0.70)
  int controlRadius = 4;
  int cardRadius = 8;
  int controlHeight = 32;
};

// 全局唯一浅色令牌实例。
const ThemeTokens& fluentLightTokens();

// translucent=true 时窗口与侧边栏背景透明（透出 Mica），否则回退不透明浅色。
QString clickFlowStyleSheet(bool translucent = false);
