#pragma once

#include <QString>

/** Runs a detached pinned-image layer using the current NiriCapture process. */
[[nodiscard]] int runPinnedCapture(const QString &path);
