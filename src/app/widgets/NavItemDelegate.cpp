#include "app/widgets/NavItemDelegate.h"

#include <QPainter>
#include <QPainterPath>
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
    painter->setBrush(tokens.sidebarAccent);
    painter->drawRoundedRect(indicator, 1.5, 1.5);
  } else if (hovered) {
    painter->setBrush(tokens.navItemHover);
    painter->drawRoundedRect(rowRect, tokens.controlRadius, tokens.controlRadius);
  }

  QTextOption textOption;
  textOption.setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  const QColor ink = selected ? QColor(Qt::white) : tokens.sidebarText;
  painter->setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter->setBrush(Qt::NoBrush);
  painter->save();
  painter->translate(rowRect.left() + 18, rowRect.center().y() - 8);
  switch (index.row()) {
    case 0: {
      QPainterPath cursor;
      cursor.moveTo(1, 0); cursor.lineTo(13, 9); cursor.lineTo(8, 10);
      cursor.lineTo(5, 15); cursor.closeSubpath();
      painter->drawPath(cursor);
      break;
    }
    case 1:
      painter->drawRoundedRect(QRectF(0, 2, 16, 12), 3, 3);
      painter->drawEllipse(QRectF(5, 5, 6, 6));
      break;
    case 2:
      painter->drawRoundedRect(QRectF(0, 2, 16, 12), 2, 2);
      for (int x = 3; x < 15; x += 4) painter->drawPoint(QPointF(x, 6));
      painter->drawLine(QPointF(4, 10), QPointF(12, 10));
      break;
    default:
      for (int y = 2; y < 16; y += 6) {
        painter->drawLine(QPointF(0, y), QPointF(16, y));
        painter->drawEllipse(QPointF(y == 8 ? 11 : 5, y), 2, 2);
      }
      break;
  }
  painter->restore();
  painter->setPen(ink);
  QFont font = option.font;
  font.setWeight(selected ? QFont::DemiBold : QFont::Normal);
  painter->setFont(font);
  const QRectF textRect = rowRect.adjusted(44, 0, -8, 0);
  painter->drawText(textRect, index.data(Qt::DisplayRole).toString(), textOption);
  painter->restore();
}

QSize NavItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  return QSize(QStyledItemDelegate::sizeHint(option, index).width() + 32, 44);
}
