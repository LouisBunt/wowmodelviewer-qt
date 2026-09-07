#include "Theme.h"
#include "UiKit.h"
#include "TimelinePanel.h"

#include <algorithm>
#include <map>

#include <QComboBox>
#include <QToolButton>
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
  // 88 px, and no longer the loudest thing in the window.
  //
  // The scrubber used to be a 24 px tall groove filled with an accent gradient across the
  // whole width -- the largest, most saturated object in the frame, louder than the model.
  // A transport control is not the subject of the page. It is a 4 px groove now, and the only
  // accent on it is the playhead, which is the one thing that carries information.
  setFixedHeight(ui::px(88));
  setProperty("role", "panel");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(met::sp(met::Edge), met::sp(met::Pad),
                          met::sp(met::Edge), met::sp(met::Pad));
  col->setSpacing(met::sp(met::Pad));

  auto* top = new QHBoxLayout;
  top->setSpacing(met::sp(met::Pad));

  // Transport. Real buttons with SVG icons: the glyphs used to come from "Segoe UI Symbol"
  // and rendered as empty boxes on any machine without it.
  auto* transport = new QWidget;
  transport->setAttribute(Qt::WA_StyledBackground, true);
  transport->setProperty("role", "panel");
  auto* tr = new QHBoxLayout(transport);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(met::sp(met::Snug));

  auto makeButton = [this](const QString& icon, const QString& tip, int action) {
    auto* b = uikit::iconButton(icon, tip);
    b->setProperty("transport", action);
    connect(b, &QToolButton::clicked, this, [this, action]() { transportAction(action); });
    return b;
  };

  tr->addWidget(makeButton("skip-back", QString::fromUtf8("An den Anfang"), 0));
  playButton_ = makeButton("play", QString::fromUtf8("Abspielen / Pause"), 1);
  playButton_->setIcon(Theme::icon("play", QColor(tok::accentText)));
  tr->addWidget(playButton_);
  tr->addWidget(makeButton("skip-forward", QString::fromUtf8("An das Ende"), 2));
  top->addWidget(transport);

  // Animation picker. A wide combo, because an animation name can be longer than the box:
  // the closed field and the drop-down list both used to elide, so a clip could not be
  // identified in either.
  auto* animTag = new QLabel("ANIM");
  animTag->setFont(typo::font(typo::Caption));
  animTag->setProperty("role", "section");
  top->addWidget(animTag);

  animList_ = uikit::wideCombo();
  animList_->setMinimumWidth(ui::px(230));
  animList_->setFont(typo::font(typo::Body));
  connect(animList_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int i) { if (!updating_) applyAnimation(i); });
  top->addWidget(animList_);

  timeLabel_ = new QLabel("0 / 0");
  timeLabel_->setFont(typo::font(typo::Mono));
  timeLabel_->setProperty("role", "mono");
  top->addWidget(timeLabel_);
  top->addStretch(1);

  auto* speedTag = new QLabel(QString::fromUtf8("Tempo"));
  speedTag->setFont(typo::font(typo::Caption));
  speedTag->setProperty("role", "section");
  top->addWidget(speedTag);

  speedBar_ = new SegmentedBar(SegmentedBar::Radio, this);
  for (const char* l : kSpeedLabels)
    speedBar_->addSegment(QString::fromLatin1(l));
  speedBar_->setCurrent(2);   // 1x
  connect(speedBar_, &SegmentedBar::activated, this, [this](int i) { applySpeed(i); });
  top->addWidget(speedBar_);
  col->addLayout(top);

  // The scrubber: a thin groove, an accent fill up to the playhead, and a grab handle big
  // enough to hit. The old handle was 3 px wide -- effectively impossible to grab.
  scrubber_ = new QSlider(Qt::Horizontal);
  scrubber_->setRange(0, 0);
  scrubber_->setFixedHeight(ui::px(20));
  scrubber_->setProperty("role", "scrub");
  connect(scrubber_, &QSlider::sliderMoved, this, [this](int f) {
    if (!model_ || !model_->animManager)
      return;
    model_->animManager->Pause(true);
    model_->animManager->SetFrame((size_t)f);
    if (playButton_)
      playButton_->setIcon(Theme::icon("play", QColor(tok::accentText)));
  });
  col->addWidget(scrubber_);
}

// One place for the three transport buttons, called from their clicked() signals.
void TimelinePanel::transportAction(int action)
{
  if (!model_ || !model_->animManager)
    return;
  AnimManager* am = model_->animManager;
  switch (action) {
    case 0:                                   // back to the start
      am->SetFrame(0);
      break;
    case 1:                                   // play / pause
      // Pressing play while the neutral row is selected has nothing to play, so take it as
      // "start the first real clip" rather than silently resuming whatever was last loaded.
      if (!animList_->itemData(animList_->currentIndex()).isValid()) {
        if (animList_->count() > 1)
          animList_->setCurrentIndex(1);
        break;
      }
      am->Pause();
      playButton_->setIcon(Theme::icon(am->IsPaused() ? "play" : "pause",
                                       QColor(tok::accentText)));
      break;
    case 2:                                   // next animation in the list
      if (animList_->currentIndex() + 1 < animList_->count())
        animList_->setCurrentIndex(animList_->currentIndex() + 1);
      break;
  }
}

void TimelinePanel::applySpeed(int index)
{
  if (index >= 0 && index < 4 && model_ && model_->animManager)
    model_->animManager->SetSpeed(kSpeeds[index]);
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
    // SetAnim before Pause, and reset currentAnim with it: animList[0].AnimID otherwise
    // still points at the clip chosen last, and GLHost's per-frame follow-up would drag
    // the pose straight back to it instead of holding the rest pose.
    am->SetAnim(0, 0, 0);
    am->Pause(true);
    model_->currentAnim = 0;
    scrubber_->setRange(0, 0);
    scrubber_->setValue(0);
    if (playButton_)
      playButton_->setIcon(Theme::icon("play", QColor(tok::accentText)));
    return;
  }

  // The anims[] index stored with the row. Passing the combo ROW (as this did originally)
  // drifted as soon as getAnimsMap() collapsed two variations into one name, so the list
  // named one clip and played another.
  const int animIndex = id.toInt();
  if (animIndex < 0 || animIndex >= (int)model_->anims.size())
    return;                                     // SetAnim would index anims[] out of range
  // Set alongside the AnimManager, so the very first frame after the choice is already the
  // right pose; GLHost keeps it in step from then on.
  model_->currentAnim = (size_t)animIndex;
  am->SetAnim(0, (unsigned int)animIndex, 0);
  am->Play();
  if (playButton_)
    playButton_->setIcon(Theme::icon("pause", QColor(tok::accentText)));

  // Last valid frame is length-1, and a clip reporting 0 frames would otherwise produce
  // setRange(0, -1).
  scrubber_->setRange(0, std::max(0, (int)am->GetFrameCount() - 1));
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

// eventFilter is gone: the speed chips and the transport glyphs were QLabels routed through
// property lookups, with no hover, no pressed state and no keyboard. They are real buttons and
// a segmented control now, each with its own signal.
