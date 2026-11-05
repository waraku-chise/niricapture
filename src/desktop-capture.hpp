/** @fileoverview DMS-backed frozen desktop frames and coordinate composition. */
#pragma once

#include "capture.hpp"

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

/** One output as reported by Niri, plus the pixels captured for it. */
struct OutputFrame {
  MonitorInfo monitor;
  QImage image;
};

/** A frozen snapshot of one or more outputs in Niri's logical layout space. */
struct DesktopFrames {
  QVector<OutputFrame> outputs;
  QRectF logicalBounds;

  [[nodiscard]] bool isEmpty() const { return outputs.isEmpty(); }
  [[nodiscard]] const OutputFrame *find(const QString &name) const;
};

/** Queries every connected output with its logical geometry, scale and mode. */
[[nodiscard]] bool probeOutputs(QVector<MonitorInfo> &outputs, QString &error);
/** Queries the name of Niri's focused output. */
[[nodiscard]] bool probeFocusedOutputName(QString &name, QString &error);

/**
 * Grabs one output through DMS's screenshot backend.
 *
 * `dms screenshot output -o NAME --stdout --no-clipboard --no-file` is the
 * same freeze path DMS Screenshot+ uses, but without a temporary PNG or a
 * clipboard side effect. `grim -o NAME -` is a compatibility fallback for
 * sessions where the DMS CLI is unavailable; it still produces a still frame
 * before any overlay is mapped.
 */
[[nodiscard]] bool captureOutputFrame(const MonitorInfo &monitor,
                                      OutputFrame &frame, QString &error,
                                      bool allowGrimFallback = true);

/** Captures every probed output and builds the logical union. */
[[nodiscard]] bool captureDesktopFrames(DesktopFrames &frames, QString &error);
/** Captures only the named output (or the focused output when empty). */
[[nodiscard]] bool captureSingleOutputFrame(DesktopFrames &frames,
                                            const QString &outputName,
                                            QString &error);
/** Captures all outputs and composes them at the largest output scale. */
[[nodiscard]] bool captureAllOutputFrame(DesktopFrames &frames, QString &error);

/** The scale of the output covering the largest part of `selection`. */
[[nodiscard]] qreal scaleForSelection(const DesktopFrames &frames,
                                      const QRectF &selection);
/**
 * Composites a global logical selection into one image.
 *
 * Every output is sampled from its own frozen frame at its native scale and
 * resampled into `targetScale` (normally scaleForSelection()). Areas with no
 * output (gaps between monitors) stay transparent. The returned image is
 * exactly round(selection.width * targetScale) pixels wide.
 */
[[nodiscard]] QImage composeSelection(const DesktopFrames &frames,
                                      const QRectF &selection,
                                      qreal targetScale, QString &error);
/** Composites the whole logical desktop, e.g. for "all monitors". */
[[nodiscard]] QImage composeAllOutputs(const DesktopFrames &frames,
                                       qreal targetScale, QString &error);
/** Wraps a composed image as CaptureData for the editor. */
[[nodiscard]] CaptureData editorCaptureFromImage(QImage image,
                                                 const QRectF &logicalRect,
                                                 qreal scale,
                                                 const QString &outputName);
