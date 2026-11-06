/** @fileoverview Standalone image editor window with a New/Open start page. */
#pragma once

#include "capture.hpp"
#include "config.hpp"

#include <QFutureWatcher>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QWidget>

class QLabel;
class QStackedWidget;

/**
 * The launcher-facing editor. It does not capture the screen; it opens an
 * existing image or creates a blank canvas, then edits it with the same
 * modular EditorCanvas/ToolBar used by the in-place screenshot editor.
 */
class EditorWindow final : public QWidget {
public:
  explicit EditorWindow(const AppConfig &config = AppConfig::defaults(),
                        QWidget *parent = nullptr);
  ~EditorWindow() override;

  void openImage(const QImage &image);
  void newCanvas(const QSize &size);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  class CanvasView;

  void buildHome();
  void buildEditor();
  void showHome();
  void showCanvas();
  void openFromDialog();
  void newFromDialog();
  void canvasFromDialog();
  void saveImage();
  void copyImage();
  void pinImage();
  void runOcr();
  void showOcrDialog(const QString &text);
  void setStatus(const QString &text);
  void updateZoomLabel(qreal zoom);

  struct OcrJob {
    QString text;
    QString error;
  };

  QStackedWidget *stack_ = nullptr;
  QWidget *home_ = nullptr;
  QWidget *editorPage_ = nullptr;
  CanvasView *view_ = nullptr;
  QLabel *status_ = nullptr;
  QLabel *zoomLabel_ = nullptr;
  QString lastImageDirectory_;
  QFutureWatcher<OcrJob> ocrWatcher_;
  bool ocrRunning_ = false;
  int pinCount_ = 0;
  AppConfig config_;
};
