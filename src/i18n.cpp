/** @fileoverview Runtime Chinese/English selection for native UI strings. */
#include "i18n.hpp"

#include <QLocale>
#include <QString>

bool chineseUi() {
  static const bool chinese = [] {
    const QString override =
        qEnvironmentVariable("NIRICAPTURE_LANG").trimmed().toLower();
    if (override == QStringLiteral("zh") ||
        override == QStringLiteral("zh_cn") ||
        override == QStringLiteral("zh-cn") ||
        override == QStringLiteral("chinese"))
      return true;
    if (override == QStringLiteral("en") ||
        override == QStringLiteral("english"))
      return false;
    return QLocale::system().language() == QLocale::Chinese;
  }();
  return chinese;
}

QString ui(const char *english, const char *chinese) {
  return QString::fromUtf8(chineseUi() ? chinese : english);
}
