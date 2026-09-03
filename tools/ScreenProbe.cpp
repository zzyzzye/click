// 临时调试工具：截取 ClickFlow 真实窗口，把标题栏三个按钮区域输出为字符画。
// 用法：先启动 ClickFlow，再运行本程序（Debug/ClickFlowScreenProbe.exe）。
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QString>

#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static void dumpButton(const QImage& image, const QRect& rect,
                       const char* name) {
  std::printf("=== %s (%d,%d %dx%d) ===\n", name, rect.x(), rect.y(),
              rect.width(), rect.height());
  for (int y = rect.top(); y <= rect.bottom(); ++y) {
    QString line;
    for (int x = rect.left(); x <= rect.right(); ++x) {
      const QColor c = image.pixelColor(x, y);
      const int lum = c.red() + c.green() + c.blue();
      line += lum < 400 ? QStringLiteral("#")
                        : (lum < 650 ? QStringLiteral("+") : QStringLiteral("."));
    }
    std::printf("%s\n", qPrintable(line));
  }
}

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);

  HWND hwnd = FindWindowW(nullptr, L"ClickFlow");
  if (!hwnd) {
    std::printf("NO_WINDOW\n");
    return 1;
  }
  if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
  // 反复尝试置前，直到 ClickFlow 成为前台窗口（防止被遮挡截取错误）
  for (int attempt = 0; attempt < 10; ++attempt) {
    SetForegroundWindow(hwnd);
    Sleep(300);
    if (GetForegroundWindow() == hwnd) break;
  }
  std::printf("FOREGROUND=%d\n", GetForegroundWindow() == hwnd ? 1 : 0);

  RECT rc{};
  GetWindowRect(hwnd, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  std::printf("WINDOW_RECT left=%ld top=%ld w=%d h=%d\n", rc.left, rc.top, w, h);
  std::printf("IsZoomed=%d IsIconic=%d\n", IsZoomed(hwnd) ? 1 : 0,
              IsIconic(hwnd) ? 1 : 0);
  WINDOWPLACEMENT wp{};
  wp.length = sizeof(wp);
  GetWindowPlacement(hwnd, &wp);
  std::printf("showCmd=%u flags=%u\n", wp.showCmd, wp.flags);

  QScreen* screen = QGuiApplication::primaryScreen();
  const QPixmap full = screen->grabWindow(0);
  const qreal dpr = full.devicePixelRatio();
  std::printf("SCREEN_DPR=%f\n", dpr);

  // GetWindowRect 返回物理像素坐标，grabWindow 的像素图也是物理像素，直接使用。
  const QImage image = full.copy(rc.left, rc.top, w, h).toImage();
  image.save(QStringLiteral("D:/ZW/click/build/clickflow_window.png"));

  // 标题栏按钮：逻辑 46x32，换算物理像素
  const int btnW = static_cast<int>(46 * dpr + 0.5);
  const int capH = static_cast<int>(32 * dpr + 0.5);
  const int imgW = image.width();
  dumpButton(image, QRect(imgW - btnW * 3, 0, btnW, capH), "minimize");
  dumpButton(image, QRect(imgW - btnW * 2, 0, btnW, capH), "maximize");
  dumpButton(image, QRect(imgW - btnW, 0, btnW, capH), "close");

  // 整个窗口的粗粒度亮度图（每 16x16 物理像素一个字符）
  std::printf("=== overview (%d x %d) ===\n", imgW, image.height());
  for (int y = 0; y < image.height(); y += 16) {
    QString line;
    for (int x = 0; x < imgW; x += 16) {
      const QColor c = image.pixelColor(x, y);
      const int lum = c.red() + c.green() + c.blue();
      line += lum < 200 ? QStringLiteral("#")
                        : (lum < 500 ? QStringLiteral("+")
                                     : (lum < 700 ? QStringLiteral("-")
                                                  : QStringLiteral(".")));
    }
    std::printf("%s\n", qPrintable(line));
  }

  // 最大化按钮区域暗像素坐标列表（精确分析字形）
  std::printf("=== maxbutton dark pixels ===\n");
  const QRect maxRect(imgW - btnW * 2, 0, btnW, capH);
  for (int y = maxRect.top(); y <= maxRect.bottom(); ++y) {
    QString row;
    bool any = false;
    for (int x = maxRect.left(); x <= maxRect.right(); ++x) {
      const QColor c = image.pixelColor(x, y);
      const int lum = c.red() + c.green() + c.blue();
      if (lum < 650) {
        row += QStringLiteral(" %1").arg(x - maxRect.left(), 3);
        any = true;
      }
    }
    if (any) std::printf("y=%3d:%s\n", y, qPrintable(row));
  }
  // 用 PrintWindow 强制窗口全新重绘到内存 DC（绕过屏幕合成器），
  // 用来区分「应用画了两遍」与「合成器残留重影」。
  const int pw = w, ph = h;
  HDC screenDc = GetDC(nullptr);
  HDC memDc = CreateCompatibleDC(screenDc);
  HBITMAP memBmp = CreateCompatibleBitmap(screenDc, pw, ph);
  HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDc, memBmp));
  const BOOL printed = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
  QImage printedImage(pw, ph, QImage::Format_ARGB32);
  printedImage.fill(Qt::magenta);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = pw;
  bmi.bmiHeader.biHeight = -ph;  // 顶向下
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  GetDIBits(memDc, memBmp, 0, ph, printedImage.bits(), &bmi,
            DIB_RGB_COLORS);
  SelectObject(memDc, oldBmp);
  DeleteObject(memBmp);
  DeleteDC(memDc);
  ReleaseDC(nullptr, screenDc);
  std::printf("PRINTWINDOW=%d\n", printed ? 1 : 0);
  // GDI 位图为 BGR，转成 RGB
  printedImage = printedImage.rgbSwapped();
  printedImage.save(QStringLiteral("D:/ZW/click/build/clickflow_printwindow.png"));
  std::printf("=== printwindow maxbutton dark pixels ===\n");
  for (int y = 0; y < capH; ++y) {
    QString row;
    bool any = false;
    for (int x = pw - btnW * 2; x < pw - btnW; ++x) {
      const QColor c = printedImage.pixelColor(x, y);
      const int lum = c.red() + c.green() + c.blue();
      if (lum < 650) {
        row += QStringLiteral(" %1").arg(x - (pw - btnW * 2), 3);
        any = true;
      }
    }
    if (any) std::printf("y=%3d:%s\n", y, qPrintable(row));
  }

  return 0;
}
