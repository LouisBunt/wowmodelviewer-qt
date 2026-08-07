#ifndef GLHOST_H
#define GLHOST_H

#include <QColor>
#include <QElapsedTimer>
#include <QPoint>
#include <QWidget>

#include "OrbitCamera.h"
#include "SceneLighting.h"   // Light, MAX_LIGHTS -- widget-free since Phase 0

class Attachment;
class WoWModel;
class QTimer;

// Phase 1/2 canvas.
//
// Deliberately NOT a QOpenGLWidget: wow.dll's VideoSettings does not create a GL
// context -- Init() only runs glewInit(), and SetCurrent()/SwapBuffers() operate on
// hDC/hRC that the caller must supply. Under wx that caller is wxGLCanvas; here we
// create the context ourselves from this widget's native HWND and hand it over.
class GLHost : public QWidget
{
  Q_OBJECT

public:
  explicit GLHost(QWidget* parent = nullptr);
  ~GLHost() override;

  // Takes ownership of the model and frames the camera on it.
  void setModel(WoWModel* model);

  QString lastError() const { return error_; }
  bool isReady() const { return videoReady_; }
  WoWModel* model() const { return model_; }

  // Grab the rendered frame off the GL back buffer after `frames` have been drawn,
  // write it to `path` and quit. Verification path -- there is no other way to prove
  // from outside that the pipeline actually produced pixels.
  void grabAfter(int frames, const QString& path) { grabAt_ = frames; grabPath_ = path; }

  // Drive the camera from outside, exercising the same calls the mouse handlers use.
  // Exists so the orbit controls are verifiable from a headless run.
  void setView(float yaw, float pitch) { camera_.setYawAndPitch(yaw, pitch); }

  // Write the current view to `path`. Synchronous on purpose: it draws one frame and
  // reads it back before the buffer swap, so the caller can report success or failure
  // instead of hoping a queued grab happened.
  bool saveScreenshot(const QString& path);

  void setBackgroundColour(const QColor& c);
  QColor backgroundColour() const;

  // 0 front, 1 three-quarter, 2 side, 3 top. Distance and pivot are kept, so a preset
  // reorients the current framing rather than throwing it away.
  void applyCameraPreset(int index);
  void resetCamera() { camera_.reset(model_); }

  // Frame the camera on what is currently DRAWN rather than on the whole mesh --
  // the item view hides the body, and framing the invisible figure leaves the piece
  // as a speck in the middle. Falls back to resetCamera() when nothing is visible.
  void frameVisible();

  // The scene's four fixed-function lights. Handed out so a panel can edit them in
  // place; call applyLights() afterwards to push the change into GL.
  Light* lights() { return lights_; }
  void applyLights();
  int activeLight() const { return activeLight_; }
  void setActiveLight(int i);

  void setGridVisible(bool on) { showGrid_ = on; }
  bool gridVisible() const { return showGrid_; }

  // Frames actually drawn per second, averaged over the last sampling window. The
  // status overlay used to show a hardcoded "60 FPS".
  float fps() const { return fps_; }

protected:
  // Qt must not paint over the area wow.dll renders into.
  QPaintEngine* paintEngine() const override { return nullptr; }
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;

  // Orbit controls, ported from ModelCanvas::OnMouse.
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void keyPressEvent(QKeyEvent*) override;

private slots:
  void tick();

private:
  bool initVideo();
  void render();

  // Read the back buffer into `path`. Only valid between drawing and the swap.
  bool grabTo(const QString& path);

  // A subtle line grid on the z=0 plane. Deliberately NOT the upstream one: that is a
  // 40x40 white/black checkerboard (ModelCanvas::RenderGrid), which in a dark viewport
  // reads as a lit floor and drowns the model.
  void drawGrid();

  // Equipped items become CHILD attachments of the model's attachment, so drawing
  // model_->draw() alone would never show them. The wx canvas keeps the same root.
  Attachment* root_ = nullptr;
  WoWModel* model_ = nullptr;
  QTimer* timer_ = nullptr;
  bool videoReady_ = false;
  QString error_;

  OrbitCamera camera_;
  QPoint lastMousePos_;

  Light lights_[MAX_LIGHTS];
  int activeLight_ = 0;
  bool showGrid_ = false;

  int frame_ = 0;
  int grabAt_ = -1;
  QString grabPath_;

  // #0b0d10, the mock-up's app background, until the user picks another one.
  float bg_[3] = { 0.043f, 0.051f, 0.063f };

  // Set by saveScreenshot() for the duration of the one frame it draws.
  QString shotPath_;
  bool shotOk_ = false;

  // FPS sampling. Counting frames over a window beats reciprocating one frame time,
  // which jitters far too much to read.
  QElapsedTimer fpsClock_;
  int fpsFrames_ = 0;
  float fps_ = 0.0f;
};

#endif
