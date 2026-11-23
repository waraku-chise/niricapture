/** @fileoverview Loads the user's Xcursor theme for custom editor cursors. */
#pragma once

#include <QCursor>
#include <QString>
#include <QStringList>

/** Returns a cursor from XCURSOR_THEME, falling back to a Qt standard cursor. */
[[nodiscard]] QCursor themedCursor(const QStringList &names,
                                   Qt::CursorShape fallback);
[[nodiscard]] QCursor themedCursor(const QString &name,
                                   Qt::CursorShape fallback);
