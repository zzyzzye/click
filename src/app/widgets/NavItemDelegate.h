#pragma once

#include <QStyledItemDelegate>

// 侧边栏导航项绘制：Fluent 悬停/选中背景 + 左侧 3px accent 选中指示条。
class NavItemDelegate final : public QStyledItemDelegate {
  Q_OBJECT
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};
