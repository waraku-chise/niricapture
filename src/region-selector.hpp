/** @fileoverview Frozen-frame region selection and in-place annotation. */
#pragma once

#include "capture.hpp"
#include "desktop-capture.hpp"

#include <QColor>
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVector>

#include <cstdint>

class QKeyEvent;
class QLineEdit;
class QPainter;
class QPlainTextEdit;
class QPushButton;
class RegionOverlay;

/**
 * One layer-shell overlay per output. Selection and annotations stay in the
 * frozen frames; the toolbar and annotations are painted directly on the
 * overlay. No second editor window is created for region/fullscreen capture.
 */
class RegionSelector final : public QObject {
  Q_OBJECT
public:
  explicit RegionSelector(DesktopFrames frames,
                          QuickOutputMode quickMode = QuickOutputMode::None,
                          QObject *parent = nullptr);
  ~RegionSelector() override;

  void start(const QRectF &initialSelection = {});
  void cancel();
  [[nodiscard]] bool hasSelection() const;
  [[nodiscard]] QRectF selection() const { return selection_; }
  [[nodiscard]] qreal selectedScale() const;
  [[nodiscard]] DesktopFrames &frames() { return frames_; }
  [[nodiscard]] const DesktopFrames &frames() const { return frames_; }

  enum class Tool : std::uint8_t {
    Select,
    Pen,
    Arrow,
    Line,
    Rectangle,
    Ellipse,
    Text,
    Highlighter,
    Mosaic,
    Blur,
    Marker
  };

signals:
  void finished(bool success, const QString &error);
  void cancelled();

private:
  friend class RegionOverlay;

  enum class Popup : std::uint8_t { None, Palette };
  enum class Drag : std::uint8_t {
    None,
    NewSelection,
    MoveSelection,
    ResizeSelection,
    MoveAnnotation,
    ResizeAnnotation,
    Draw,
    Ocr
  };
  struct Button {
    QRectF rect;
    QString action;
    QString tooltip;
    QColor color;
  };

  void selectionChanged();
  void setStatus(const QString &status);
  void beginSelection(const QPointF &global);
  void moveSelectionBy(const QPointF &delta);
  void resizeSelectionTo(const QString &handle, const QPointF &global);
  void finishSelection();
  void updateOwnerOutput();
  void beginEditing();
  void pushUndo();
  void undo();
  void redo();
  void deleteSelected();

  [[nodiscard]] int annotationAt(const QPointF &global) const;
  [[nodiscard]] QRectF annotationBounds(const Annotation &annotation) const;
  [[nodiscard]] QString annotationHandleAt(const QPointF &global) const;
  [[nodiscard]] Annotation previewAnnotation() const;
  void commitPreview();
  void paintEditor(QPainter &painter, RegionOverlay *overlay);
  void paintToolbar(QPainter &painter, RegionOverlay *overlay);
  void paintAnnotations(QPainter &painter, RegionOverlay *overlay,
                        const QVector<Annotation> &extra);
  void paintSelectionChrome(QPainter &painter, RegionOverlay *overlay);

  [[nodiscard]] QRectF toolbarRect(RegionOverlay *overlay) const;
  [[nodiscard]] QRectF popupRect(RegionOverlay *overlay) const;
  [[nodiscard]] QVector<Button> toolbarButtons(RegionOverlay *overlay) const;
  [[nodiscard]] bool toolbarHit(RegionOverlay *overlay, const QPointF &local,
                                QString *action);
  void handleAction(const QString &action);

  bool editorPress(RegionOverlay *overlay, const QPointF &global,
                   const QPointF &local, Qt::MouseButton button,
                   Qt::KeyboardModifiers modifiers);
  void editorMove(RegionOverlay *overlay, const QPointF &global);
  void editorRelease(RegionOverlay *overlay, const QPointF &global);
  bool editorKey(QKeyEvent *event, RegionOverlay *overlay);

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
  QPointF dragCurrent_;
  Annotation dragOriginalAnnotation_;
  int dragAnnotationIndex_ = -1;
  QString resizeHandle_;
  QString ownerOutput_;
  QString keyboardOutput_;
  QVector<RegionOverlay *> overlays_;

  Tool tool_ = Tool::Select;
  Popup popup_ = Popup::None;
  Drag drag_ = Drag::None;
  QColor color_ = QColor(QStringLiteral("#ff375f"));
  qreal width_ = 4.0;
  bool filled_ = false;
  int selected_ = -1;
  int nextMarker_ = 1;

  QVector<QPointF> freehandPoints_;
  QVector<Annotation> annotations_;
  QVector<QVector<Annotation>> undo_;
  QVector<QVector<Annotation>> redo_;

  QPointer<QLineEdit> textEditor_;
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
