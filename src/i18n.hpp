/** @fileoverview Runtime Chinese/English selection for native UI strings. */
#pragma once

#include <QString>

class QCoreApplication;

/** Loads Qt's own Chinese catalog for standard dialogs (font/colour/file). */
void installQtTranslator(QCoreApplication &application);

/** True when the user-facing UI should use Simplified Chinese. */
[[nodiscard]] bool chineseUi();
/** Picks one of two strings for the current UI language. */
[[nodiscard]] QString ui(const char *english, const char *chinese);
