/** @fileoverview Runs the editor, capture, pin and lock smoke checks. */
#include "capture.hpp"
#include "clipboard-smoke.hpp"
#include "config-smoke.hpp"
#include "editor-window-smoke.hpp"
#include "instance-lock-smoke.hpp"
#include "pin-layout-smoke.hpp"
#include "pin-lifecycle-smoke.hpp"
#include "tool-smoke.hpp"
#include "transform-smoke.hpp"

#include <QApplication>
#include <QDebug>
#include <QString>

#include <cstdio>

int main(int argc, char **argv) {
  const QString heldLockPath =
      qEnvironmentVariable(kInstanceLockHolderVariable);
  if (!heldLockPath.isEmpty())
    return runInstanceLockHolder(heldLockPath);

  QApplication application(argc, argv);
  if (!loadCaptureFonts())
    return 17;

  QString error;
  if (!runEditorToolSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 140: %s\n", error.toUtf8().constData());
    return 140;
  }
  if (!runEditorWindowSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 141: %s\n", error.toUtf8().constData());
    return 141;
  }
  if (!runConfigSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 142: %s\n", error.toUtf8().constData());
    return 142;
  }
  if (!runClipboardSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 88: %s\n", error.toUtf8().constData());
    return 88;
  }
  if (!runInstanceLockSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 85: %s\n", error.toUtf8().constData());
    return 85;
  }
  if (!runPinLayoutSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 77: %s\n", error.toUtf8().constData());
    return 77;
  }
  if (!runPinLifecycleSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 78: %s\n", error.toUtf8().constData());
    return 78;
  }
  if (!runTransformSmoke(error)) {
    qWarning().noquote() << error;
    fprintf(stderr, "SMOKE 67: %s\n", error.toUtf8().constData());
    return 67;
  }
  return 0;
}
