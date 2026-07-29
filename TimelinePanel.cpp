#include "TimelinePanel.h"

#include <QComboBox>
#include <QEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include "AnimManager.h"
#include "WoWModel.h"

namespace {
const char* kPanel  = "#0e1114";
const char* kCard   = "#14181e";
const char* kBord   = "#23282f";
const char* kBord2  = "#1c2128";
const char* kText   = "#e8eaee";
const char* kMuted  = "#8a93a0";
const char* kDim    = "#5f6874";
const char* kAccent = "#c8a15a";
const char* kOnAcc  = "#17130a";

QString pick(std::initializer_list<const char*> names, const char* fallback)
{
  const QStringList have = QFontDatabase().families();
  for (const char* n : names)
    if (have.contains(QString::fromLatin1(n)))
      return QString::fromLatin1(n);
  return QString::fromLatin1(fallback);
}
QString uiF()   { static QString f = pick({"IBM Plex Sans", "Segoe UI"}, "sans-serif"); return f; }
QString monoF() { static QString f = pick({"IBM Plex Mono", "Consolas"}, "monospace"); return f; }
QString iconF() { static QString f = pick({"Segoe UI Symbol", "Segoe UI"}, "sans-serif"); return f; }

const float kSpeeds[] = { 0.25f, 0.5f, 1.0f, 2.0f };
const char* kSpeedLabels[] = { "0.25x", "0.5x", "1x", "2x" };
}

TimelinePanel::TimelinePanel(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedHeight(104);
  setStyleSheet(QString("background:%1; border-top:1px solid %2;").arg(kPanel).arg(kBord2));

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(16, 12, 16, 14);
  col->setSpacing(11);

  auto* top = new QHBoxLayout;
  top->setSpacing(14);

  // Transport. The glyphs need the symbol font; Segoe UI has no coverage for them.
  auto* transport = new QWidget;
  transport->setStyleSheet("background:transparent;");
  auto* tr = new QHBoxLayout(transport);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(4);

  auto makeButton = [this](const QString& glyph, int size, const char* colour,
                           const char* background, int action) {
    auto* b = new QLabel(glyph);
    b->setFixedSize(size, size);
    b->setAlignment(Qt::AlignCenter);
    b->setFont(QFont(iconF(), size / 3 + 3));
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString("color:%1; background:%2; border:none; border-radius:%3px;")
                       .arg(colour).arg(background).arg(size > 30 ? 8 : 6));
    b->setProperty("transport", action);
    b->installEventFilter(this);
    return b;
  };

  tr->addWidget(makeButton(QString::fromUtf8("⏮"), 28, "#98a1ae", "transparent", 0));
  playButton_ = makeButton(QString::fromUtf8("⏸"), 34, kOnAcc, kAccent, 1);
  tr->addWidget(playButton_);
  tr->addWidget(makeButton(QString::fromUtf8("⏭"), 28, "#98a1ae", "transparent", 2));
  top->addWidget(transport);

  // Animation picker.
  auto* animWrap = new QFrame;
  animWrap->setFixedHeight(28);
  animWrap->setMinimumWidth(230);
  animWrap->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2;"
                                  " border-radius:6px; }").arg(kCard).arg(kBord));
  auto* aw = new QHBoxLayout(animWrap);
  aw->setContentsMargins(10, 0, 6, 0);
  aw->setSpacing(8);
  auto* animTag = new QLabel("ANIM");
  QFont tagFont(uiF(), 7);
  tagFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  animTag->setFont(tagFont);
  animTag->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(kDim));
  aw->addWidget(animTag);

  animList_ = new QComboBox;
  animList_->setFont(QFont(uiF(), 9));
  animList_->setStyleSheet(QString(
    "QComboBox { background:transparent; border:none; color:%1; }"
    "QComboBox::drop-down { border:none; width:16px; }"
    "QComboBox QAbstractItemView { background:%2; border:1px solid %3;"
    " selection-background-color:#181510; color:%1; }")
    .arg(kText).arg(kCard).arg(kBord));
  connect(animList_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int i) { if (!updating_) applyAnimation(i); });
  aw->addWidget(animList_, 1);
  top->addWidget(animWrap);

  timeLabel_ = new QLabel("0 / 0");
  timeLabel_->setFont(QFont(monoF(), 9));
  timeLabel_->setStyleSheet("color:#98a1ae; background:transparent; border:none;");
  top->addWidget(timeLabel_);
  top->addStretch(1);

  auto* speedTag = new QLabel("Tempo");
  speedTag->setFont(QFont(uiF(), 8));
  speedTag->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(kDim));
  top->addWidget(speedTag);

  for (int i = 0; i < 4; ++i) {
    auto* c = new QLabel(QString::fromLatin1(kSpeedLabels[i]));
    c->setFont(QFont(monoF(), 8));
    c->setAlignment(Qt::AlignCenter);
    c->setCursor(Qt::PointingHandCursor);
    c->setProperty("speedIndex", i);
    c->installEventFilter(this);
    speedChips_.push_back(c);
    top->addWidget(c);
  }
  col->addLayout(top);

  // Scrubber. A styled QSlider rather than a hand-drawn track: dragging it has to
  // seek, and QSlider already handles the interaction.
  scrubber_ = new QSlider(Qt::Horizontal);
  scrubber_->setRange(0, 0);
  scrubber_->setFixedHeight(26);
  scrubber_->setStyleSheet(QString(
    "QSlider::groove:horizontal { height:24px; border-radius:6px; background:#0a0d10;"
    " border:1px solid %1; }"
    "QSlider::sub-page:horizontal { border-radius:6px;"
    " background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
    " stop:0 rgba(200,161,90,120), stop:1 rgba(200,161,90,45)); }"
    "QSlider::handle:horizontal { width:3px; margin:-1px 0; border-radius:1px;"
    " background:%2; }").arg(kBord2).arg(kAccent));
  connect(scrubber_, &QSlider::sliderMoved, this, [this](int f) {
    if (!model_ || !model_->animManager)
      return;
    model_->animManager->Pause(true);
    model_->animManager->SetFrame((size_t)f);
    if (playButton_)
      playButton_->setText(QString::fromUtf8("▶"));
  });
  col->addWidget(scrubber_);

  // Reflect the default speed selection.
  for (int i = 0; i < (int)speedChips_.size(); ++i) {
    const bool active = (i == 2);
    speedChips_[i]->setStyleSheet(active
      ? QString("color:%1; background:#191509; border:1px solid #3a3222;"
                " border-radius:5px; padding:3px 8px;").arg(kAccent)
      : QString("color:%1; background:#12161b; border:1px solid %2;"
                " border-radius:5px; padding:3px 8px;").arg(kMuted).arg(kBord));
  }
}

void TimelinePanel::setModel(WoWModel* model)
{
  model_ = model;
  rebuildAnimations();
}

void TimelinePanel::rebuildAnimations()
{
  updating_ = true;
  animList_->clear();

  if (!model_) {
    animList_->addItem(QString::fromUtf8("—"));
    scrubber_->setRange(0, 0);
    updating_ = false;
    return;
  }

  // getAnimsMap() is keyed by the model's animation index and carries the readable
  // name from AnimationData.
  for (const auto& a : model_->getAnimsMap())
    animList_->addItem(QString::fromStdWString(a.second), a.first);

  if (animList_->count() == 0)
    animList_->addItem(QString::fromUtf8("keine Animationen"));

  updating_ = false;

  if (animList_->count() > 0)
    applyAnimation(0);
}

void TimelinePanel::applyAnimation(int index)
{
  if (!model_ || !model_->animManager || index < 0)
    return;

  const QVariant id = animList_->itemData(index);
  if (!id.isValid())
    return;

  model_->animManager->SetAnim(0, (unsigned int)index, 0);
  model_->animManager->Play();
  if (playButton_)
    playButton_->setText(QString::fromUtf8("⏸"));

  scrubber_->setRange(0, (int)model_->animManager->GetFrameCount());
}

void TimelinePanel::tick()
{
  if (!model_ || !model_->animManager)
    return;

  const size_t frame = model_->animManager->GetFrame();
  const size_t total = model_->animManager->GetFrameCount();

  if (!scrubber_->isSliderDown())
    scrubber_->setValue((int)frame);
  timeLabel_->setText(QString("%1 / %2").arg(frame).arg(total));
}

bool TimelinePanel::eventFilter(QObject* obj, QEvent* e)
{
  if (e->type() != QEvent::MouseButtonRelease)
    return QWidget::eventFilter(obj, e);

  const QVariant speed = obj->property("speedIndex");
  if (speed.isValid()) {
    const int idx = speed.toInt();
    if (model_ && model_->animManager)
      model_->animManager->SetSpeed(kSpeeds[idx]);
    for (int i = 0; i < (int)speedChips_.size(); ++i) {
      const bool active = (i == idx);
      speedChips_[i]->setStyleSheet(active
        ? QString("color:%1; background:#191509; border:1px solid #3a3222;"
                  " border-radius:5px; padding:3px 8px;").arg(kAccent)
        : QString("color:%1; background:#12161b; border:1px solid %2;"
                  " border-radius:5px; padding:3px 8px;").arg(kMuted).arg(kBord));
    }
    return true;
  }

  const QVariant transport = obj->property("transport");
  if (transport.isValid() && model_ && model_->animManager) {
    AnimManager* am = model_->animManager;
    switch (transport.toInt()) {
      case 0:                                   // back to the start
        am->SetFrame(0);
        break;
      case 1:                                   // play / pause
        am->Pause();
        playButton_->setText(am->IsPaused() ? QString::fromUtf8("▶")
                                            : QString::fromUtf8("⏸"));
        break;
      case 2:                                   // next animation in the list
        if (animList_->currentIndex() + 1 < animList_->count())
          animList_->setCurrentIndex(animList_->currentIndex() + 1);
        break;
    }
    return true;
  }

  return QWidget::eventFilter(obj, e);
}
