#include "app/widgets/CaptionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

#include "app/UiStyle.h"

namespace {

// 标题栏按钮：46x32，自绘 Fluent 图标（最小化/最大化/还原/关闭）。
class CaptionButton final : public QAbstractButton {
  Q_OBJECT
 public:
  enum class Icon { Minimize, Maximize, Restore, Close };

  explicit CaptionButton(Icon icon, QWidget* parent = nullptr)
      : QAbstractButton(parent), icon_(icon) {
    setFixedSize(46, 32);
    setMouseTracking(true);
  }

  void setIcon(Icon icon) {
    icon_ = icon;
    update();
  }

  void setForceHovered(bool hovered) {
    if (forceHovered_ == hovered) return;
    forceHovered_ = hovered;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = fluentLightTokens();
    const bool hovered = isDown() || forceHovered_ || underMouse();
    const bool close = icon_ == Icon::Close;

    if (hovered) {
      painter.fillRect(rect(), close ? tokens.danger : QColor(0, 0, 0, 15));
    }

    const QColor iconColor =
        (hovered && close) ? QColor(Qt::white) : tokens.textPrimary;
    painter.setPen(QPen(iconColor, 1.0));

    const QPointF c = rect().center();
    const QRectF r(c.x() - 5, c.y() - 5, 10, 10);
    switch (icon_) {
      case Icon::Minimize:
        painter.drawLine(QPointF(r.left(), c.y() + 0.5),
                         QPointF(r.right() + 1, c.y() + 0.5));
        break;
      case Icon::Maximize:
        painter.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        break;
      case Icon::Restore:
        // 后方方框只画露出的上边与右边，前方方框画完整轮廓。
        painter.drawPolyline(QVector<QPointF>{
            QPointF(r.left() + 2.5, r.top() + 2.5),
            QPointF(r.left() + 2.5, r.top() + 0.5),
            QPointF(r.right() + 0.5, r.top() + 0.5),
            QPointF(r.right() + 0.5, r.bottom() - 1.5)});
        painter.drawRect(QRectF(r.left() + 0.5, r.top() + 2.5, 7, 7));
        break;
      case Icon::Close:
        painter.drawLine(r.topLeft(), r.bottomRight() + QPointF(1, 1));
        painter.drawLine(r.topRight() + QPointF(1, 0),
                         r.bottomLeft() + QPointF(0, 1));
        break;
    }
  }

 private:
  Icon icon_;
  bool forceHovered_ = false;
};

}  // namespace

CaptionBar::CaptionBar(const QString& title, QWidget* parent)
    : QWidget(parent) {
  setFixedHeight(32);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 0, 0, 0);
  layout->setSpacing(0);

  titleLabel_ = new QLabel(title, this);
  titleLabel_->setObjectName(QStringLiteral("captionTitle"));
  layout->addWidget(titleLabel_);
  layout->addStretch(1);

  auto* minimize = new CaptionButton(CaptionButton::Icon::Minimize, this);
  minimize->setObjectName(QStringLiteral("captionMinimizeButton"));
  minimize->setToolTip(QStringLiteral("最小化"));
  auto* maximize = new CaptionButton(CaptionButton::Icon::Maximize, this);
  maximize->setObjectName(QStringLiteral("captionMaximizeButton"));
  maximize->setToolTip(QStringLiteral("最大化"));
  auto* close = new CaptionButton(CaptionButton::Icon::Close, this);
  close->setObjectName(QStringLiteral("captionCloseButton"));
  close->setToolTip(QStringLiteral("关闭"));

  minimizeButton_ = minimize;
  maximizeButton_ = maximize;
  closeButton_ = close;
  layout->addWidget(minimize);
  layout->addWidget(maximize);
  layout->addWidget(close);

  connect(minimize, &QAbstractButton::clicked, this,
          &CaptionBar::minimizeRequested);
  connect(maximize, &QAbstractButton::clicked, this,
          &CaptionBar::maximizeRestoreRequested);
  connect(close, &QAbstractButton::clicked, this, &CaptionBar::closeRequested);
}

void CaptionBar::setMaximized(bool maximized) {
  if (maximized_ == maximized) return;
  maximized_ = maximized;
  static_cast<CaptionButton*>(maximizeButton_)
      ->setIcon(maximized ? CaptionButton::Icon::Restore
                          : CaptionButton::Icon::Maximize);
  maximizeButton_->setToolTip(maximized ? QStringLiteral("还原")
                                        : QStringLiteral("最大化"));
}

void CaptionBar::setMaximizeButtonHovered(bool hovered) {
  static_cast<CaptionButton*>(maximizeButton_)->setForceHovered(hovered);
}

void CaptionBar::mousePressEvent(QMouseEvent* event) {
  // 空白区域拖动窗口；最大化时不拖动（避免误触，双击可还原）。
  if (event->button() == Qt::LeftButton && !maximized_ && window() &&
      window()->windowHandle()) {
    window()->windowHandle()->startSystemMove();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void CaptionBar::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit maximizeRestoreRequested();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

#include "CaptionBar.moc"
