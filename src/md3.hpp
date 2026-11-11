/** @fileoverview Material 3 tokens for the Qt Widgets surfaces. */
#pragma once

#include "config.hpp"

#include <QColor>
#include <QString>

class QWidget;

struct Md3Tokens {
  bool dark = true;
  QColor surface;
  QColor onSurface;
  QColor onSurfaceVariant;
  QColor outline;
  QColor outlineVariant;
  QColor primary;
  QColor onPrimary;
  QColor primaryHover;
  QColor primaryContainer;
  QColor onPrimaryContainer;
  QColor secondaryContainer;
  QColor onSecondaryContainer;
  QColor surfaceContainerLow;
  QColor surfaceContainer;
  QColor surfaceContainerHigh;
  QColor surfaceContainerHighest;
  QColor error;
  QColor onError;
  QColor errorContainer;
  QColor onErrorContainer;
  QColor hover;
  QColor pressed;
};

[[nodiscard]] Md3Tokens md3Tokens(const EditorTheme &theme);
[[nodiscard]] QString md3WidgetStyleSheet(const Md3Tokens &tokens);

[[nodiscard]] QString md3OpenImage(QWidget *parent, const QString &title,
                                   const QString &directory,
                                   const QString &filter);
[[nodiscard]] QString md3SaveImage(QWidget *parent, const QString &title,
                                   const QString &directory,
                                   const QString &filter,
                                   const QString &suggestedName);
[[nodiscard]] QString md3ChooseDirectory(QWidget *parent, const QString &title,
                                         const QString &directory);
