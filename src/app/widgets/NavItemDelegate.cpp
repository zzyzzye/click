#include "app/widgets/NavItemDelegate.h"

#include <QPainter>
#include <QTextOption>

#include "app/UiStyle.h"

void NavItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  const ThemeTokens& tokens = fluentLightTokens();
  const bool selected = option.state & QStyle::State_Selected;
  const bool hovered = option.state & QStyle::State_MouseOver;

  const QRectF rowRect = option.rect;
  painter->setPen(Qt::NoPen);
  if (selected) {
    painter->setBrush(tokens.navItemSelected);
    painter->drawRoundedRect(rowRect, tokens.controlRadius, tokens.controlRadius);
    // 左侧 3px accent 指示条，高 16px，垂直居中。
    const QRectF indicator(rowRect.left() + 4, rowRect.center().y() - 8, 3, 16);
    painter->setBrush(tokens.accent);
    painter->drawRoundedRect(indicator, 1.5, 1.5);
  } else if (hovered) {
    painter->setBrush(tokens.navItemHover);
    painter->drawRoundedRect(rowRect, tokens.controlRadius, tokens.controlRadius);
  }

  QTextOption textOption;
  textOption.setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  painter->setPen(tokens.textPrimary);
  const QRectF textRect = rowRect.adjusted(12, 0, -8, 0);
  painter->drawText(textRect, index.data(Qt::DisplayRole).toString(), textOption);
  painter->restore();
}

QSize NavItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 36);
}
