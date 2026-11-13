/** @fileoverview Frozen-frame region selection and in-place annotation. */
#pragma once

#include "capture.hpp"
#include "config.hpp"
#include "desktop-capture.hpp"
#include "editor/editor-canvas.hpp"
#include "editor/tool-bar.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVector>

class FontPicker;

#include <cstdint>

class QKeyEvent;
class QLineEdit;
class QPainter;
class QWheelEvent;
class QPlainTextEdit;
class QPushButton;
class RegionOverlay;

/**
 * One layer-shell overlay per output. Selection and annotations stay in the
 * frozen frames; the toolbar and annotations are painted directly on the
 * overlay. Tool interactions live in EditorCanvas, not in this class.
 */
class RegionSelector final : public QObject {
  Q_OBJECT
public:
  explicit RegionSelector(DesktopFrames frames,
                          QuickOutputMode quickMode,
                          const AppConfig &config = AppConfig::defaults(),
                          QObject *parent = nullptr);
  ~RegionSelector() override;

  void start(const QRectF &initialSelection = {});
  void cancel();
  [[nodiscard]] bool hasSelection() const;
  [[nodiscard]] QRectF selection() const { return selection_; }
  [[nodiscard]] qreal selectedScale() const;
  [[nodiscard]] DesktopFrames &frames() { return frames_; }
  [[nodiscard]] const DesktopFrames &frames() const { return frames_; }

signals:
  void finished(bool success, const QString &error);
  void cancelled();

private:
  friend class RegionOverlay;

  enum class Drag : std::uint8_t {
    None,
    NewSelection,
    MoveSelection,
    ResizeSelection
  };

  void selectionChanged();
  void setStatus(const QString &status);
  void beginSelection(const QPointF &global);
  void moveSelectionBy(const QPointF &delta);
  void resizeSelectionTo(const QString &handle, const QPointF &global);
  void finishSelection();
  void updateOwnerOutput();
  [[nodiscard]] RegionOverlay *ownerOverlay() const;

  void paintEditor(QPainter &painter, RegionOverlay *overlay);
  void paintSourceEffects(QPainter &painter, RegionOverlay *overlay,
                          const QVector<Annotation> &extra);
  void paintSelectionChrome(QPainter &painter, RegionOverlay *overlay);

  [[nodiscard]] bool toolbarHit(RegionOverlay *overlay, const QPointF &local,
                                ToolBarHit *hit);
  [[nodiscard]] bool toolbarContains(RegionOverlay *overlay,
                                     const QPointF &local) const;
  void handleToolbarHit(const ToolBarHit &hit);
  void chooseFont();
  void setToolbarHover(RegionOverlay *overlay, const QPointF &local);
  void syncToolbar();

  bool editorPress(RegionOverlay *overlay, const QPointF &global,
                   const QPointF &local, Qt::MouseButton button,
                   Qt::KeyboardModifiers modifiers);
  void editorMove(RegionOverlay *overlay, const QPointF &global,
                  Qt::KeyboardModifiers modifiers);
  void editorRelease(RegionOverlay *overlay, const QPointF &global,
                     Qt::KeyboardModifiers modifiers);
  bool editorKey(QKeyEvent *event, RegionOverlay *overlay);
  bool editorWheel(QWheelEvent *event);

  void beginText(const QPointF &global, RegionOverlay *overlay);
  void commitText();
  void runOcr();
  void showOcrPanel(const QString &text, RegionOverlay *overlay);
  void closeOcrPanel();
  void editorRightClick();
  void setKeyboardOverlay(RegionOverlay *overlay);
  [[nodiscard]] QPointF clampPoint(const QPointF &point) const;
  [[nodiscard]] QRectF clampSelection(const QRectF &rect) const;

  void exportSelection(QuickOutputMode mode);
  void closeAll();

  DesktopFrames frames_;
  QuickOutputMode quickMode_ = QuickOutputMode::None;
  QRectF selection_;
  QRectF dragOriginalSelection_;
  QPointF dragStart_;
  Drag drag_ = Drag::None;
  QString resizeHandle_;
  QString ownerOutput_;
  QString keyboardOutput_;
  QVector<RegionOverlay *> overlays_;

  EditorCanvas canvas_;
  ToolBar toolBar_;

  QPointer<QLineEdit> textEditor_;
  FontPicker *fontPicker_ = nullptr;
  QPointer<RegionOverlay> textOverlay_;
  QPointF textPoint_;
  bool committingText_ = false;
  QPointer<QPlainTextEdit> ocrText_;
  QPointer<QPushButton> ocrCopy_;
  QPointer<QPushButton> ocrClose_;
  struct OcrJob {
    QString text;
    QString error;
  };
  QFutureWatcher<OcrJob> ocrWatcher_;
  QPointer<RegionOverlay> ocrOverlay_;

  QString status_;
  bool started_ = false;
  bool finished_ = false;
  bool editing_ = false;
};
