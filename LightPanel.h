#ifndef LIGHTPANEL_H
#define LIGHTPANEL_H

#include <vector>

#include <QWidget>

class GLHost;
class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

// The scene lighting controls.
//
// Under wx this was LightControl, a dock panel that was never actually shown -- it only
// existed to OWN the light array the renderer read every frame. Phase 0 moved the data
// into SceneLighting (widget-free), which left the four lights configurable in principle
// and unreachable in practice: light 0 on, white, from the front, forever.
//
// This is the missing half. It edits GLHost's Light array in place and pushes each change
// straight into GL, so the viewport reacts while you drag.
class LightPanel : public QWidget
{
  Q_OBJECT

public:
  explicit LightPanel(GLHost* host, QWidget* parent = nullptr);

private:
  void selectLight(int index);
  void readFromLight();      // light -> widgets, WITHOUT touching the light
  void writeToLight();       // widgets -> light, then into GL
  void updateSwatch();

  GLHost* host_ = nullptr;
  int current_ = 0;

  QComboBox* lightPicker_ = nullptr;
  QCheckBox* enabled_ = nullptr;
  QComboBox* type_ = nullptr;
  QSlider* intensity_ = nullptr;
  QSlider* colourR_ = nullptr;
  QSlider* colourG_ = nullptr;
  QSlider* colourB_ = nullptr;
  QSlider* posX_ = nullptr;
  QSlider* posY_ = nullptr;
  QSlider* posZ_ = nullptr;
  QLabel* swatch_ = nullptr;
  QLabel* hint_ = nullptr;

  bool updating_ = false;    // guards against reacting to our own writes
};

#endif
