#pragma once

#include <QColor>
#include <QString>

// ClickFlow 浅色内容区与品牌侧栏令牌，配合 UiStyle.cpp 管理共享样式。
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
  int controlRadius = 8;
  int cardRadius = 14;
  int controlHeight = 32;
  QColor sidebarBackground = QColor("#15243B");
  QColor sidebarText = QColor("#CAD5E5");
  QColor sidebarAccent = QColor("#69D2FF");
};

// 全局唯一浅色令牌实例。
const ThemeTokens& fluentLightTokens();

// translucent=true 时内容区透出 Mica；品牌侧栏始终保持深蓝色。
QString clickFlowStyleSheet(bool translucent = false);
