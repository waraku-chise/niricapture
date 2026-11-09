#include "editor/font-picker.hpp"

#include "i18n.hpp"

#include <QFontComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

FontPicker::FontPicker(const QString &family, QWidget *parent,
                       std::function<void(const QString &)> accepted)
    : QFrame(parent), accepted_(std::move(accepted)) {
  setObjectName(QStringLiteral("fontPicker"));
  setStyleSheet(QStringLiteral(R"(
    QFrame#fontPicker { background: #151922; border: 1px solid #3a4352;
                        border-radius: 10px; }
    QLabel { color: #eef1f7; background: transparent; }
    QLabel#fontTitle { font-size: 14px; font-weight: 600; }
    QLabel#fontPreview { background: #0f1116; border: 1px solid #303849;
                         border-radius: 6px; padding: 12px; }
    QFontComboBox { background: #171c25; color: #eef1f7;
                    border: 1px solid #303849; border-radius: 6px;
                    padding: 6px; }
    QPushButton { background: #202633; color: #eef1f7;
                  border: 1px solid #303849; border-radius: 6px;
                  padding: 6px 16px; }
    QPushButton#fontAccept { background: #2f6fed; border-color: #4b83ff; }
    QPushButton:hover { background: #2a3343; }
    QPushButton#fontAccept:hover { background: #3f7df7; }
  )"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedSize(370, 205);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(10);
  auto *title = new QLabel(ui("Text font", "文字字体"), this);
  title->setObjectName(QStringLiteral("fontTitle"));
  layout->addWidget(title);

  fonts_ = new QFontComboBox(this);
  fonts_->setCurrentFont(QFont(family));
  layout->addWidget(fonts_);

  preview_ = new QLabel(this);
  preview_->setObjectName(QStringLiteral("fontPreview"));
  preview_->setAlignment(Qt::AlignCenter);
  preview_->setMinimumHeight(45);
  layout->addWidget(preview_);

  auto *buttons = new QHBoxLayout;
  buttons->addStretch(1);
  auto *cancelButton = new QPushButton(ui("Cancel", "取消"), this);
  auto *acceptButton = new QPushButton(ui("Use", "使用"), this);
  acceptButton->setObjectName(QStringLiteral("fontAccept"));
  buttons->addWidget(cancelButton);
  buttons->addWidget(acceptButton);
  layout->addLayout(buttons);

  connect(fonts_, &QFontComboBox::currentFontChanged, this,
          [this](const QFont &) { updatePreview(); });
  connect(cancelButton, &QPushButton::clicked, this, &FontPicker::cancel);
  connect(acceptButton, &QPushButton::clicked, this, [this] {
    finish(fonts_->currentFont().family());
  });
  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  escape->setContext(Qt::WidgetWithChildrenShortcut);
  connect(escape, &QShortcut::activated, this, &FontPicker::cancel);
  updatePreview();
}

void FontPicker::showCentered() {
  if (QWidget *parent = parentWidget()) {
    move((parent->width() - width()) / 2, (parent->height() - height()) / 2);
  }
  show();
  raise();
  fonts_->setFocus(Qt::ActiveWindowFocusReason);
}

void FontPicker::updatePreview() {
  if (!preview_ || !fonts_)
    return;
  QFont font = fonts_->currentFont();
  font.setPixelSize(22);
  preview_->setFont(font);
  preview_->setText(ui("AaBbYyZz  Hello", "AaBbYyZz  你好，世界"));
}

void FontPicker::finish(const QString &family) {
  if (accepted_)
    accepted_(family);
  deleteLater();
}

void FontPicker::cancel() { deleteLater(); }
