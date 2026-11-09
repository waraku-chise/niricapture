/** @fileoverview Runtime Chinese/English selection for native UI strings. */
#include "i18n.hpp"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
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

void installQtTranslator(QCoreApplication &application) {
  if (!chineseUi())
    return;
  const QString directory =
      QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  auto *translator = new QTranslator(&application);
  if (translator->load(QStringLiteral("qtbase_zh_CN"), directory) ||
      translator->load(QStringLiteral("qt_zh_CN"), directory)) {
    application.installTranslator(translator);
  } else {
    delete translator;
  }
}
