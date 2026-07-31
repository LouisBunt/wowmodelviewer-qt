#ifndef TIMELINEPANEL_H
#define TIMELINEPANEL_H

#include <vector>

#include <QWidget>

class WoWModel;
class QComboBox;
class QLabel;
class QSlider;

// The animation strip along the bottom of the design: animation picker, transport,
// scrubber and speed.
//
// The model already animates by itself (GLHost calls update() every frame); this is
// the control surface for it, driven through WoWModel::animManager.
//
// Playback is opt-in: a freshly loaded model holds its bind pose until a clip is picked.
class TimelinePanel : public QWidget
{
  Q_OBJECT

public:
  explicit TimelinePanel(QWidget* parent = nullptr);

  void setModel(WoWModel* model);
  void tick();                  // called each frame to follow playback

  // Start the clip at this index into the model's anims[] array -- the same numbering
  // AnimManager::SetAnim uses. Returns false if the model has no such clip. Exists so the
  // animated path is testable from a headless run: since playback became opt-in there was
  // otherwise no way to reach an animated pose without clicking.
  bool playAnimation(int animIndex);

private:
  void rebuildAnimations();
  void applyAnimation(int index);

  WoWModel* model_ = nullptr;
  QComboBox* animList_ = nullptr;
  QLabel* playButton_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  QSlider* scrubber_ = nullptr;
  std::vector<QLabel*> speedChips_;
  bool updating_ = false;

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;
};

#endif
