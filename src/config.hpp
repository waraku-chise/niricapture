/** @fileoverview User configuration for the editor and output paths. */
#pragma once

#include "capture.hpp"

#include <QByteArray>
#include <QColor>
#include <QJsonObject>
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
  QStringList colors{QStringLiteral("#000000"), QStringLiteral("#4a4a4a"), QStringLiteral("#7f1d1d"), QStringLiteral("#dc2626"), QStringLiteral("#ea580c"), QStringLiteral("#eab308"), QStringLiteral("#16a34a"), QStringLiteral("#0d9488"), QStringLiteral("#2563eb"), QStringLiteral("#6b7280"), QStringLiteral("#9ca3af"), QStringLiteral("#9f1239"), QStringLiteral("#f87171"), QStringLiteral("#fb923c"), QStringLiteral("#fde047"), QStringLiteral("#4ade80"), QStringLiteral("#2dd4bf"), QStringLiteral("#60a5fa"), QStringLiteral("#cbd5e1"), QStringLiteral("#ffffff"), QStringLiteral("#92400e"), QStringLiteral("#fca5a5"), QStringLiteral("#fdba74"), QStringLiteral("#fef08a"), QStringLiteral("#86efac"), QStringLiteral("#99f6e4"), QStringLiteral("#93c5fd"), QStringLiteral("#7c3aed"), QStringLiteral("#a855f7"), QStringLiteral("#db2777"), QStringLiteral("#ec4899"), QStringLiteral("#f472b6"), QStringLiteral("#f9a8d4"), QStringLiteral("#a16207"), QStringLiteral("#78350f"), QStringLiteral("#1e3a8a")};
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
  QString fontFamily = QStringLiteral("Neucha");
};

struct AppConfig {
  QStringList tools{QStringLiteral("select"),   QStringLiteral("pen"),
                    QStringLiteral("arrow"),    QStringLiteral("line"),
                    QStringLiteral("rectangle"), QStringLiteral("ellipse"),
                    QStringLiteral("text"),     QStringLiteral("highlighter"),
                    QStringLiteral("mosaic"),   QStringLiteral("blur"),
                    QStringLiteral("marker"),   QStringLiteral("eyedropper")};
  QStringList commands{QStringLiteral("palette"), QStringLiteral("font"),
                       QStringLiteral("undo"),    QStringLiteral("redo"),
                       QStringLiteral("ocr"),     QStringLiteral("copy"),
                       QStringLiteral("save"),    QStringLiteral("close")};
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
      {QStringLiteral("marker"), QStringLiteral("C")},
      {QStringLiteral("eyedropper"), QStringLiteral("I")}};
  PaletteConfig palette;
  CanvasConfig canvas;
  EditorTheme theme;
  QString ocrLanguages;
  QString screenshotDir;
  QString path;

  static constexpr int kCurrentSchemaVersion = 2;

  [[nodiscard]] static AppConfig defaults();
  /** Loads config, creating a documented default file when absent. */
  [[nodiscard]] static AppConfig load(QString *error = nullptr);
  [[nodiscard]] static QString defaultPath();
  [[nodiscard]] static bool writeDefault(const QString &path, QString *error);
  [[nodiscard]] BackgroundStyle backgroundStyle() const;
  [[nodiscard]] int schemaVersion() const { return schemaVersion_; }
  [[nodiscard]] QJsonObject toJson() const;
  [[nodiscard]] QByteArray toJsonBytes() const;
  /** Atomically writes the current configuration to `path`. */
  [[nodiscard]] bool writable() const { return writable_; }
  [[nodiscard]] bool save(QString *error = nullptr) const;
  [[nodiscard]] bool saveTo(const QString &path, QString *error = nullptr) const;

private:
  friend AppConfig configFromJson(const QByteArray &json, const AppConfig &base,
                                  QString *error);
  QJsonObject rawDocument_;
  int schemaVersion_ = kCurrentSchemaVersion;
  bool writable_ = true;
};

/** Testable JSON parser; missing keys inherit defaults. */
[[nodiscard]] AppConfig configFromJson(const QByteArray &json,
                                       const AppConfig &base, QString *error);
