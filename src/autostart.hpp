/** @fileoverview XDG autostart helper for the tray process. */
#pragma once

#include <QString>

[[nodiscard]] QString autostartFilePath();
[[nodiscard]] bool isAutostartEnabled();
[[nodiscard]] bool setAutostartEnabled(bool enabled, QString *error = nullptr);
