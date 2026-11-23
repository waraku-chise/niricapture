#include "cursor-theme.hpp"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QStandardPaths>

#include <algorithm>
#include <cstring>

namespace {
constexpr quint32 kXcursorImageType = 0xfffd0002U;

quint32 readU32(const QByteArray &data, qsizetype offset) {
  if (offset < 0 || offset + 4 > data.size())
    return 0;
  return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
         (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1)))
          << 8) |
         (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2)))
          << 16) |
         (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3)))
          << 24);
}

QCursor loadXcursorFile(const QString &path, Qt::CursorShape fallback) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return QCursor(fallback);
  const QByteArray data = file.readAll();
  if (data.size() < 16 || data.left(4) != QByteArrayLiteral("Xcur"))
    return QCursor(fallback);
  const quint32 headerSize = readU32(data, 4);
  const quint32 tableCount = readU32(data, 12);
  if (headerSize < 16 || tableCount == 0 || tableCount > 4096)
    return QCursor(fallback);

  const int configuredSize = qEnvironmentVariableIntValue("XCURSOR_SIZE");
  const qint64 requestedSize =
      configuredSize > 0 ? configuredSize : 32;
  qint64 bestDistance = std::numeric_limits<qint64>::max();
  QImage bestImage;
  int bestXHot = 0;
  int bestYHot = 0;

  for (quint32 index = 0; index < tableCount; ++index) {
    const qsizetype toc = 16 + static_cast<qsizetype>(index) * 12;
    if (toc + 12 > data.size())
      break;
    const quint32 type = readU32(data, toc);
    const quint32 subtype = readU32(data, toc + 4);
    const quint32 position = readU32(data, toc + 8);
    if (type != kXcursorImageType || position + 36 > data.size())
      continue;
    const quint32 imageHeaderSize = readU32(data, position);
    const quint32 width = readU32(data, position + 16);
    const quint32 height = readU32(data, position + 20);
    const quint32 xhot = readU32(data, position + 24);
    const quint32 yhot = readU32(data, position + 28);
    if (width == 0 || height == 0 || width > 1024 || height > 1024 ||
        imageHeaderSize < 36)
      continue;
    const qsizetype pixels =
        static_cast<qsizetype>(width) * static_cast<qsizetype>(height) * 4;
    const qsizetype pixelStart =
        static_cast<qsizetype>(position) + imageHeaderSize;
    if (pixelStart + pixels > data.size())
      continue;
    const qint64 distance =
        std::abs(static_cast<qint64>(subtype) - requestedSize);
    if (distance >= bestDistance)
      continue;
    QImage image(static_cast<int>(width), static_cast<int>(height),
                 QImage::Format_ARGB32_Premultiplied);
    std::memcpy(image.bits(), data.constData() + pixelStart,
                static_cast<size_t>(pixels));
    bestImage = std::move(image);
    bestXHot = static_cast<int>(xhot);
    bestYHot = static_cast<int>(yhot);
    bestDistance = distance;
  }
  if (bestImage.isNull())
    return QCursor(fallback);
  return QCursor(QPixmap::fromImage(bestImage), bestXHot, bestYHot);
}

QString cursorThemeName() {
  const QString theme = qEnvironmentVariable("XCURSOR_THEME").trimmed();
  return theme.isEmpty() ? QStringLiteral("default") : theme;
}
} // namespace

QCursor themedCursor(const QString &name, Qt::CursorShape fallback) {
  static QHash<QString, QCursor> cache;
  const QString key = cursorThemeName() + QLatin1Char('/') + name;
  if (const auto it = cache.constFind(key); it != cache.constEnd())
    return it.value();

  QCursor result(fallback);
  const QStringList roots =
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (const QString &root : roots) {
    const QString path =
        root + QStringLiteral("/icons/") + cursorThemeName() +
        QStringLiteral("/cursors/") + name;
    if (!QFileInfo::exists(path))
      continue;
    result = loadXcursorFile(path, fallback);
    break;
  }
  cache.insert(key, result);
  return result;
}

QCursor themedCursor(const QStringList &names, Qt::CursorShape fallback) {
  for (const QString &name : names) {
    const QCursor cursor = themedCursor(name, fallback);
    if (cursor.shape() != fallback)
      return cursor;
    if (QFileInfo::exists(
            QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("icons/") +
                                       cursorThemeName() +
                                       QStringLiteral("/cursors/") + name)))
      return cursor;
  }
  return QCursor(fallback);
}
