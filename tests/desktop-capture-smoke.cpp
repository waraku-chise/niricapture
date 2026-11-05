/** @fileoverview Offscreen checks for multi-output composition and effects. */
#include "capture.hpp"
#include "desktop-capture.hpp"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QFontDatabase>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QFileInfo>

#include <cmath>
#include <cstdio>

namespace {
int fail(const char *message) {
  std::fprintf(stderr, "desktop smoke: %s\n", message);
  return 1;
}

QImage patternedImage(int width, int height, const QColor &base) {
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(base);
  QPainter painter(&image);
  painter.setPen(QPen(Qt::white, 2));
  for (int x = 0; x < width; x += 32)
    painter.drawLine(x, 0, x, height);
  for (int y = 0; y < height; y += 32)
    painter.drawLine(0, y, width, y);
  painter.end();
  return image;
}

bool testSpanningComposition() {
  DesktopFrames frames;
  OutputFrame left;
  left.monitor.name = QStringLiteral("LEFT");
  left.monitor.geometry = QRect(0, 0, 800, 600);
  left.monitor.scale = 1.0;
  left.monitor.pixelSize = QSize(800, 600);
  left.image = patternedImage(800, 600, QColor(QStringLiteral("#203040")));
  OutputFrame right;
  right.monitor.name = QStringLiteral("RIGHT");
  right.monitor.geometry = QRect(800, 0, 640, 480);
  right.monitor.scale = 1.25;
  right.monitor.pixelSize = QSize(800, 600);
  right.image = patternedImage(800, 600, QColor(QStringLiteral("#503020")));
  frames.outputs = {left, right};
  frames.logicalBounds = QRectF(0, 0, 1440, 600);

  const QRectF selection(700, 100, 400, 200);
  const qreal scale = scaleForSelection(frames, selection);
  if (!qFuzzyCompare(scale, 1.25))
    return false;
  QString error;
  const QImage selected = composeSelection(frames, selection, scale, error);
  if (selected.size() != QSize(500, 250))
    return false;
  if (selected.pixelColor(0, 0) != left.image.pixelColor(700, 100))
    return false;
  // At x=125 the output border (logical 800) is crossed; the right output's
  // warm background must replace the left output's cool background.
  const QColor leftPixel = selected.pixelColor(50, 10);
  const QColor rightPixel = selected.pixelColor(250, 10);
  if (rightPixel.alpha() != 255 || rightPixel.red() <= leftPixel.red())
    return false;
  // The gap below the shorter right monitor stays transparent.
  const QImage all = composeAllOutputs(frames, 1.25, error);
  if (all.size() != QSize(1800, 750) ||
      all.pixelColor(1600, 700).alpha() != 0)
    return false;
  return true;
}

bool testChineseOcr() {
  const QStringList families = QFontDatabase::families();
  const QString family =
      families.contains(QStringLiteral("Noto Sans CJK SC"))
          ? QStringLiteral("Noto Sans CJK SC")
          : (families.contains(QStringLiteral("Noto Sans CJK JP"))
                 ? QStringLiteral("Noto Sans CJK JP")
                 : QStringLiteral("Noto Sans"));
  QImage image(1000, 320, QImage::Format_RGB32);
  image.fill(Qt::white);
  QPainter painter(&image);
  painter.setPen(Qt::black);
  painter.setFont(QFont(family, 52, QFont::Bold));
  painter.drawText(image.rect().adjusted(30, 20, -30, -20),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("你好，世界\nHello screenshot 42"));
  painter.end();
  const QByteArray previous = qgetenv("NIRICAPTURE_OCR_LANGS");
  qputenv("NIRICAPTURE_OCR_LANGS", "eng+chi_sim");
  QString error;
  const QString text = recognizeText(image, error);
  if (previous.isEmpty())
    qunsetenv("NIRICAPTURE_OCR_LANGS");
  else
    qputenv("NIRICAPTURE_OCR_LANGS", previous);
  if (!error.isEmpty() || !text.contains(QStringLiteral("Hello")) ||
      !(text.contains(QStringLiteral("世界")) ||
        text.contains(QStringLiteral("你好")))) {
    std::fprintf(stderr, "ocr text=[%s] error=[%s]\n", text.toUtf8().constData(),
                 error.toUtf8().constData());
    return false;
  }
  return true;
}

bool testEllipseAndBlur() {
  CaptureData capture;
  capture.monitor.scale = 1.0;
  capture.monitor.pixelSize = QSize(640, 480);
  capture.monitor.geometry = QRect(0, 0, 640, 480);
  capture.source = patternedImage(640, 480, QColor(QStringLiteral("#123456")));
  capture.previewSize = capture.source.size();
  const QImage original = capture.source.copy();
  const QRectF full(QPointF(), capture.previewSize);

  Annotation outline;
  outline.kind = Annotation::Kind::Ellipse;
  outline.start = {80, 80};
  outline.end = {280, 220};
  outline.size = 4;
  outline.color = QColor(QStringLiteral("#ff375f"));
  const QImage hollow =
      renderCapture(capture, full, {outline}, BackgroundStyle::None);
  if (hollow.pixelColor(180, 80) == capture.source.pixelColor(180, 80) ||
      hollow.pixelColor(180, 150) != capture.source.pixelColor(180, 150))
    return false;

  Annotation filled = outline;
  filled.filled = true;
  const QImage solid =
      renderCapture(capture, full, {filled}, BackgroundStyle::None);
  if (solid.pixelColor(180, 150) != outline.color)
    return false;

  Annotation blur;
  blur.kind = Annotation::Kind::Blur;
  blur.start = {350, 100};
  blur.end = {550, 280};
  blur.size = 24;
  const QImage blurred =
      renderCapture(capture, full, {blur}, BackgroundStyle::None);
  if (blurred.pixelColor(300, 50) != capture.source.pixelColor(300, 50) ||
      blurred.pixelColor(450, 190) == capture.source.pixelColor(450, 190))
    return false;
  if (capture.source != original)
    return false;
  if (annotationLayer(Annotation::Kind::Blur) != AnnotationLayer::Redaction)
    return false;
  return true;
}
} // namespace

int main(int argc, char **argv) {
  QApplication application(argc, argv);
  if (!testSpanningComposition())
    return fail("spanning output composition is incorrect");
  if (!testEllipseAndBlur())
    return fail("ellipse/blur rendering is incorrect");
  const QStringList fontFamilies = QFontDatabase::families();
  const bool haveCjkFont =
      fontFamilies.contains(QStringLiteral("Noto Sans CJK SC")) ||
      fontFamilies.contains(QStringLiteral("Noto Sans CJK JP"));
  const bool haveChineseModel =
      QFileInfo::exists(QStringLiteral("/usr/share/tessdata/chi_sim.traineddata")) ||
      QFileInfo::exists(QStringLiteral(
          "/usr/share/tesseract-ocr/5/tessdata/chi_sim.traineddata"));
  if (haveCjkFont && haveChineseModel &&
      !QStandardPaths::findExecutable(QStringLiteral("tesseract")).isEmpty() &&
      !testChineseOcr())
    return fail("Chinese/English OCR failed");
  return 0;
}
