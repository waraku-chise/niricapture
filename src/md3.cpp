#include "md3.hpp"

#include <QColor>
#include <QFileDialog>
#include <QStringList>
#include <QWidget>

namespace {

QColor mixColor(const QColor &a, const QColor &b, qreal t) {
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t);
}

qreal luminance(const QColor &color) {
  return (0.2126 * color.redF() + 0.7152 * color.greenF() +
          0.0722 * color.blueF());
}

QColor onColor(const QColor &color) {
  return luminance(color) > 0.55 ? QColor(QStringLiteral("#141922"))
                                 : QColor(Qt::white);
}

QString colorName(const QColor &color) {
  return color.name(QColor::HexRgb);
}

} // namespace

Md3Tokens md3Tokens(const EditorTheme &theme) {
  Md3Tokens tokens;
  tokens.surface = theme.background.isValid()
                       ? theme.background
                       : QColor(QStringLiteral("#0f1116"));
  const QColor seed = theme.accent.isValid()
                          ? theme.accent
                          : QColor(QStringLiteral("#2f6fed"));
  tokens.dark = luminance(tokens.surface) < 0.5;

  tokens.onSurface =
      tokens.dark ? QColor(QStringLiteral("#eef1f7"))
                  : mixColor(QColor(Qt::black), tokens.surface, 0.18);
  tokens.onSurfaceVariant =
      mixColor(tokens.onSurface, tokens.surface, tokens.dark ? 0.42 : 0.34);
  tokens.outline =
      mixColor(tokens.onSurface, tokens.surface, tokens.dark ? 0.70 : 0.62);
  tokens.outlineVariant =
      mixColor(tokens.onSurface, tokens.surface, tokens.dark ? 0.85 : 0.82);

  tokens.primary =
      tokens.dark ? mixColor(seed, QColor(Qt::white), 0.45)
                  : mixColor(seed, QColor(Qt::black), 0.15);
  tokens.onPrimary = onColor(tokens.primary);
  tokens.primaryHover =
      tokens.dark ? mixColor(tokens.primary, QColor(Qt::white), 0.08)
                  : mixColor(tokens.primary, QColor(Qt::black), 0.08);
  tokens.primaryContainer =
      tokens.dark ? mixColor(seed, tokens.surface, 0.55)
                  : mixColor(seed, QColor(Qt::white), 0.78);
  tokens.onPrimaryContainer =
      tokens.dark ? mixColor(seed, QColor(Qt::white), 0.72)
                  : mixColor(seed, QColor(Qt::black), 0.35);
  tokens.secondaryContainer =
      tokens.dark ? mixColor(seed, tokens.surface, 0.72)
                  : mixColor(seed, QColor(Qt::white), 0.86);
  tokens.onSecondaryContainer =
      tokens.dark ? mixColor(seed, QColor(Qt::white), 0.62)
                  : mixColor(seed, QColor(Qt::black), 0.42);

  tokens.surfaceContainerLow =
      tokens.dark ? mixColor(tokens.surface, QColor(Qt::white), 0.04)
                  : mixColor(tokens.surface, QColor(Qt::black), 0.02);
  tokens.surfaceContainer =
      tokens.dark ? mixColor(tokens.surface, QColor(Qt::white), 0.07)
                  : mixColor(tokens.surface, QColor(Qt::black), 0.035);
  tokens.surfaceContainerHigh =
      tokens.dark ? mixColor(tokens.surface, QColor(Qt::white), 0.10)
                  : mixColor(tokens.surface, QColor(Qt::black), 0.05);
  tokens.surfaceContainerHighest =
      tokens.dark ? mixColor(tokens.surface, QColor(Qt::white), 0.14)
                  : mixColor(tokens.surface, QColor(Qt::black), 0.07);

  tokens.error = tokens.dark ? QColor(QStringLiteral("#ffb4ab"))
                             : QColor(QStringLiteral("#ba1a1a"));
  tokens.onError = onColor(tokens.error);
  tokens.errorContainer = tokens.dark ? QColor(QStringLiteral("#93000a"))
                                      : QColor(QStringLiteral("#ffdad6"));
  tokens.onErrorContainer =
      tokens.dark ? QColor(QStringLiteral("#ffdad6"))
                  : QColor(QStringLiteral("#410002"));

  tokens.hover = mixColor(tokens.surface, tokens.onSurface, 0.08);
  tokens.pressed = mixColor(tokens.surface, tokens.onSurface, 0.12);
  return tokens;
}

QString md3WidgetStyleSheet(const Md3Tokens &tokens) {
  QString style = QStringLiteral(R"(
    QWidget#editorRoot, QWidget#home, QWidget#editorPage, QStackedWidget {
      background: @SURFACE@;
    }
    QWidget#topbar { background: @SURFACE@; }
    QLabel { background: transparent; color: @ON@; }
    QLabel#status { color: @MUTED@; padding: 6px 14px; }
    QLabel#zoom { color: @MUTED@; min-width: 52px; }
    QLabel#topbarTitle { color: @ON@; font-size: 15px; font-weight: 500; }
    QLabel#homeTitle { color: @ON@; font-size: 26px; font-weight: 500; }
    QLabel#homeSubtitle { color: @MUTED@; font-size: 14px; }

    QPushButton {
      background: transparent; color: @PRIMARY@; border: none;
      border-radius: 18px; padding: 8px 14px;
    }
    QPushButton:hover { background: @HOVER@; }
    QPushButton:pressed { background: @PRESSED@; }
    QPushButton:disabled { color: @MUTED@; background: transparent; }
    QPushButton#primary {
      background: @PRIMARY@; color: @ONPRIMARY@; border-radius: 20px;
      padding: 10px 22px; font-weight: 600;
    }
    QPushButton#primary:hover { background: @PRIMARYHOVER@; }
    QPushButton#tonal {
      background: @SECONDARY@; color: @ONSECONDARY@; border-radius: 20px;
      padding: 10px 22px; font-weight: 500;
    }
    QPushButton#fontAccept { background: @PRIMARY@; color: @ONPRIMARY@; }

    QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
      background: @CONTAINERLOW@; color: @ON@;
      border: 1px solid @OUTLINEVARIANT@; border-radius: 8px;
      padding: 6px 10px;
      selection-background-color: @PRIMARY@; selection-color: @ONPRIMARY@;
    }
    QLineEdit:focus, QSpinBox:focus, QComboBox:focus,
    QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid @PRIMARY@; }
    QComboBox QAbstractItemView {
      background: @CONTAINER@; color: @ON@;
      border: 1px solid @OUTLINEVARIANT@;
      selection-background-color: @PRIMARYCONTAINER@;
      selection-color: @ONPRIMARYCONTAINER@;
      outline: 0;
    }

    QMenu {
      background: @CONTAINER@; color: @ON@;
      border: 1px solid @OUTLINEVARIANT@; border-radius: 12px;
      padding: 6px;
    }
    QMenu::item { padding: 8px 28px 8px 14px; border-radius: 8px; }
    QMenu::item:selected { background: @CONTAINERHIGH@; }
    QMenu::item:disabled { color: @MUTED@; }
    QMenu::separator { height: 1px; background: @OUTLINEVARIANT@; margin: 6px 8px; }

    QDialog, QFileDialog, QMessageBox { background: @CONTAINERHIGH@; color: @ON@; }
    QDialogButtonBox QPushButton, QMessageBox QPushButton {
      border-radius: 18px; padding: 8px 16px; min-width: 72px;
    }
    QPushButton:default { background: @PRIMARY@; color: @ONPRIMARY@; }
    QPushButton:default:hover { background: @PRIMARYHOVER@; }
    QMessageBox QLabel { color: @ON@; }

    QFileDialog QLabel { color: @ON@; }
    QFileDialog QLabel:disabled { color: @MUTED@; }
    QFileDialog QListView, QFileDialog QTreeView {
      background: @CONTAINERLOW@; color: @ON@; border: none;
      selection-background-color: @PRIMARYCONTAINER@;
      selection-color: @ONPRIMARYCONTAINER@;
    }
    QFileDialog QHeaderView::section {
      background: @CONTAINER@; color: @MUTED@; border: none; padding: 6px;
    }
    QFileDialog QToolButton {
      background: transparent; border: none; border-radius: 8px; padding: 4px;
    }
    QFileDialog QToolButton:hover { background: @HOVER@; }
    QFileDialog QComboBox { min-width: 140px; }

    QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
    QScrollBar::handle:vertical { background: @OUTLINEVARIANT@; border-radius: 4px; min-height: 28px; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

    QToolTip {
      background: @CONTAINERHIGH@; color: @ON@;
      border: 1px solid @OUTLINEVARIANT@; padding: 4px 8px;
    }

    QFrame#fontPicker {
      background: @CONTAINERHIGH@; border: 1px solid @OUTLINEVARIANT@;
      border-radius: 24px;
    }
    QLabel#fontTitle { color: @ON@; font-size: 16px; font-weight: 500; }
    QLabel#fontPreview {
      background: @CONTAINERLOW@; border: 1px solid @OUTLINEVARIANT@;
      border-radius: 8px; padding: 12px; color: @ON@;
    }
  )");

  const struct {
    const char *token;
    QString value;
  } replacements[] = {
      {"@PRIMARYHOVER@", colorName(tokens.primaryHover)},
      {"@PRIMARYCONTAINER@", colorName(tokens.primaryContainer)},
      {"@ONPRIMARYCONTAINER@", colorName(tokens.onPrimaryContainer)},
      {"@SECONDARY@", colorName(tokens.secondaryContainer)},
      {"@ONSECONDARY@", colorName(tokens.onSecondaryContainer)},
      {"@CONTAINERHIGH@", colorName(tokens.surfaceContainerHigh)},
      {"@CONTAINERLOW@", colorName(tokens.surfaceContainerLow)},
      {"@CONTAINER@", colorName(tokens.surfaceContainer)},
      {"@OUTLINEVARIANT@", colorName(tokens.outlineVariant)},
      {"@ONPRIMARY@", colorName(tokens.onPrimary)},
      {"@ON@", colorName(tokens.onSurface)},
      {"@MUTED@", colorName(tokens.onSurfaceVariant)},
      {"@PRESSED@", colorName(tokens.pressed)},
      {"@PRIMARY@", colorName(tokens.primary)},
      {"@HOVER@", colorName(tokens.hover)},
      {"@SURFACE@", colorName(tokens.surface)},
  };
  for (const auto &entry : replacements)
    style.replace(QString::fromLatin1(entry.token), entry.value);
  return style;
}

QString md3OpenImage(QWidget *parent, const QString &title,
                     const QString &directory, const QString &filter) {
  QFileDialog dialog(parent, title, directory, filter);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  if (dialog.exec() != QDialog::Accepted)
    return {};
  const QStringList selected = dialog.selectedFiles();
  return selected.isEmpty() ? QString() : selected.first();
}

QString md3SaveImage(QWidget *parent, const QString &title,
                     const QString &directory, const QString &filter,
                     const QString &suggestedName) {
  QFileDialog dialog(parent, title, directory, filter);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  if (!suggestedName.isEmpty())
    dialog.selectFile(suggestedName);
  if (dialog.exec() != QDialog::Accepted)
    return {};
  const QStringList selected = dialog.selectedFiles();
  return selected.isEmpty() ? QString() : selected.first();
}

QString md3ChooseDirectory(QWidget *parent, const QString &title,
                           const QString &directory) {
  QFileDialog dialog(parent, title, directory);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setOption(QFileDialog::ShowDirsOnly, true);
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  if (dialog.exec() != QDialog::Accepted)
    return {};
  const QStringList selected = dialog.selectedFiles();
  return selected.isEmpty() ? QString() : selected.first();
}
