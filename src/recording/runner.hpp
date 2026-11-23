/** @fileoverview CLI recording workflow (selection, countdown, wf-recorder). */
#pragma once

#include "config.hpp"

class QScreen;

enum class RecordingMode { None, Region, Fullscreen };

[[nodiscard]] int runRecording(const AppConfig &config, RecordingMode mode);
