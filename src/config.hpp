/** @fileoverview User configuration for the editor and output paths. */
#pragma once

#include "capture.hpp"

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct CanvasConfig {
  int width = 1600;
  int height = 900;
  int padding = 0;
  /** transparent, white, dark, aurora, sunset, lagoon, violet */
  QString background = QStringLiteral("transparent");
  QColor backgroundColor = Qt::transparent;
  bool fitOnOpen = true;
};

struct PaletteConfig {
  QStringList colors{QStringLiteral("#ff375f"), QStringLiteral("#ff9f0a"),
                     QStringLiteral("#ffd60a"), QStringLiteral("#30d158"),
                     QStringLiteral("#0a84ff"), QStringLiteral("#bf5af2"),
                     QStringLiteral("#ffffff"), QStringLiteral("#000000")};
  QVector<int> widths{2, 4, 8, 12};
};

struct EditorTheme {
  QColor accent = QColor(QStringLiteral("#2f6fed"));
  QColor background = QColor(QStringLiteral("#0f1116"));
  QColor panel = QColor(QStringLiteral("#151922"));
  QColor checkerLight = QColor(QStringLiteral("#242a35"));
  QColor checkerDark = QColor(QStringLiteral("#1b1f27"));
  QColor defaultColor = QColor(QStringLiteral("#ff375f"));
  qreal defaultWidth = 4.0;
};

struct AppConfig {
  QStringList tools{QStringLiteral("select"),   QStringLiteral("pen"),
                    QStringLiteral("arrow"),    QStringLiteral("line"),
                    QStringLiteral("rectangle"), QStringLiteral("ellipse"),
                    QStringLiteral("text"),     QStringLiteral("highlighter"),
                    QStringLiteral("mosaic"),   QStringLiteral("blur"),
                    QStringLiteral("marker")};
  QStringList commands{QStringLiteral("palette"), QStringLiteral("undo"),
                       QStringLiteral("redo"),    QStringLiteral("ocr"),
                       QStringLiteral("copy"),    QStringLiteral("save"),
                       QStringLiteral("close")};
  QHash<QString, QString> shortcuts{
      {QStringLiteral("select"), QStringLiteral("V")},
      {QStringLiteral("pen"), QStringLiteral("F")},
      {QStringLiteral("arrow"), QStringLiteral("A")},
      {QStringLiteral("line"), QStringLiteral("L")},
      {QStringLiteral("rectangle"), QStringLiteral("R")},
      {QStringLiteral("ellipse"), QStringLiteral("E")},
      {QStringLiteral("text"), QStringLiteral("T")},
      {QStringLiteral("highlighter"), QStringLiteral("H")},
      {QStringLiteral("mosaic"), QStringLiteral("D")},
      {QStringLiteral("blur"), QStringLiteral("B")},
      {QStringLiteral("marker"), QStringLiteral("C")}};
  PaletteConfig palette;
  CanvasConfig canvas;
  EditorTheme theme;
  QString ocrLanguages;
  QString screenshotDir;
  QString path;

  [[nodiscard]] static AppConfig defaults();
  /** Loads config, creating a documented default file when absent. */
  [[nodiscard]] static AppConfig load(QString *error = nullptr);
  [[nodiscard]] static QString defaultPath();
  [[nodiscard]] static bool writeDefault(const QString &path, QString *error);
  [[nodiscard]] BackgroundStyle backgroundStyle() const;
};

/** Testable JSON parser; missing keys inherit defaults. */
[[nodiscard]] AppConfig configFromJson(const QByteArray &json,
                                       const AppConfig &base, QString *error);
