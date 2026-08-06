#include "TimelinePanel.h"

#include <map>

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

  // A loaded model rests in its bind pose until the user asks for an animation. The
  // neutral entry carries no item data, which is what applyAnimation() treats as "stop".
  // Auto-playing clip 0 on every load meant a model was never still -- you could not
  // look at a pose, and a screenshot caught whatever frame the clock happened to be on.
  animList_->addItem(QString::fromUtf8("keine — Ruhepose"));

  // Built from anims[] rather than from getAnimsMap(), because AnimManager::SetAnim uses
  // its id parameter as an INDEX into model.anims[] (see model.anims[id].length there),
  // while getAnimsMap() is keyed by AnimationData.ID -- a database id that runs well past
  // the end of that array. Enumerating anims[] keeps the stored index and the displayed
  // name attached to the same entry by construction.
  //
  // getAnimsMap() is still the source for the readable names; it collapses entries that
  // share an AnimationData.ID, which is why it cannot be the source for the indices.
  const std::map<int, std::wstring> names = model_->getAnimsMap();
  std::map<int, int> seen;                       // animID -> how often already listed
  for (size_t i = 0; i < model_->anims.size(); ++i) {
    const int animId = model_->anims[i].animID;
    const auto nameIt = names.find(animId);
    QString label = (nameIt != names.end()) ? QString::fromStdWString(nameIt->second)
                                            : QString("Animation %1").arg(animId);
    // Models carry several variations of the same animation; without this they all show
    // up under one identical name and there is no way to tell which row is which.
    const int n = ++seen[animId];
    if (n > 1)
      label += QString(" (%1)").arg(n);
    animList_->addItem(label, (int)i);
  }

  animList_->setCurrentIndex(0);
  updating_ = false;

  applyAnimation(0);        // parks the model in the rest pose
}

bool TimelinePanel::playAnimation(int animIndex)
{
  for (int i = 0; i < animList_->count(); ++i) {
    const QVariant id = animList_->itemData(i);
    if (id.isValid() && id.toInt() == animIndex) {
      animList_->setCurrentIndex(i);      // drives applyAnimation via the signal
      return true;
    }
  }
  return false;
}

void TimelinePanel::applyAnimation(int index)
{
  if (!model_ || !model_->animManager || index < 0)
    return;

  AnimManager* am = model_->animManager;

  // The neutral row (no item data): hold the bind pose.
  const QVariant id = animList_->itemData(index);
  if (!id.isValid()) {
    am->Pause(true);
    am->SetFrame(0);
    scrubber_->setRange(0, 0);
    scrubber_->setValue(0);
    if (playButton_)
      playButton_->setText(QString::fromUtf8("▶"));
    return;
  }

  // The anims[] index stored with the row. Passing the combo ROW (as this did originally)
  // drifted as soon as getAnimsMap() collapsed two variations into one name, so the list
  // named one clip and played another.
  const int animIndex = id.toInt();
  if (animIndex < 0 || animIndex >= (int)model_->anims.size())
    return;                                     // SetAnim would index anims[] out of range
  am->SetAnim(0, (unsigned int)animIndex, 0);
  am->Play();
  if (playButton_)
    playButton_->setText(QString::fromUtf8("⏸"));

  scrubber_->setRange(0, (int)am->GetFrameCount());
}

void TimelinePanel::tick()
{
  if (!model_ || !model_->animManager)
    return;

  // With no clip selected the AnimManager still reports whatever frame it was
  // constructed holding -- it read "2304 / 2467" next to a model that was standing
  // perfectly still. Report the rest pose as what it is instead.
  if (!animList_->itemData(animList_->currentIndex()).isValid()) {
    if (!scrubber_->isSliderDown())
      scrubber_->setValue(0);
    timeLabel_->setText(QString::fromUtf8("— / —"));
    return;
  }

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
        // Pressing play while the neutral row is selected has nothing to play, so take
        // it as "start the first real clip" rather than silently resuming whatever was
        // last loaded.
        if (!animList_->itemData(animList_->currentIndex()).isValid()) {
          if (animList_->count() > 1)
            animList_->setCurrentIndex(1);
          break;
        }
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
