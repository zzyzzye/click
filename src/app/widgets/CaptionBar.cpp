#include "app/widgets/CaptionBar.h"

#include <cmath>

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

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = fluentLightTokens();
    const bool hovered = isDown() || underMouse();
    const bool close = icon_ == Icon::Close;

    if (hovered) {
      painter.fillRect(rect(), close ? tokens.danger : QColor(0, 0, 0, 15));
    }

    const QColor iconColor =
        (hovered && close) ? QColor(Qt::white) : tokens.textPrimary;
    QPen pen(iconColor, 1.0);
    pen.setCosmetic(true);  // 始终 1 物理像素宽
    painter.setPen(pen);

    // 坐标吸附到物理像素中心，保证任何 DPI 下线条清晰不虚
    const qreal dpr = devicePixelRatioF();
    const auto px = [dpr](qreal v) { return (std::floor(v * dpr) + 0.5) / dpr; };

    const QPointF c = rect().center();
    const qreal l = px(c.x() - 5), t = px(c.y() - 5);
    const qreal rgt = px(c.x() + 5), btm = px(c.y() + 5);
    switch (icon_) {
      case Icon::Minimize:
        painter.drawLine(QPointF(l, px(c.y())), QPointF(rgt, px(c.y())));
        break;
      case Icon::Maximize:
        painter.drawRect(QRectF(QPointF(l, t), QPointF(rgt, btm)));
        break;
      case Icon::Restore:
        // 后方方框只画露出的上边与右边，前方方框画完整轮廓。
        painter.drawPolyline(QVector<QPointF>{
            QPointF(l + 2, t + 3), QPointF(l + 2, t), QPointF(rgt, t),
            QPointF(rgt, btm - 2)});
        painter.drawRect(QRectF(QPointF(l, t + 3), QPointF(rgt - 2, btm)));
        break;
      case Icon::Close:
        painter.drawLine(QPointF(l, t), QPointF(rgt, btm));
        painter.drawLine(QPointF(rgt, t), QPointF(l, btm));
        break;
    }
  }

 private:
  Icon icon_;
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
