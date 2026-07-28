#ifndef GLHOST_H
#define GLHOST_H

#include <QPoint>
#include <QWidget>

#include "OrbitCamera.h"
#include "SceneLighting.h"   // Light, MAX_LIGHTS -- widget-free since Phase 0

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

  // Grab the rendered frame off the GL back buffer after `frames` have been drawn,
  // write it to `path` and quit. Verification path -- there is no other way to prove
  // from outside that the pipeline actually produced pixels.
  void grabAfter(int frames, const QString& path) { grabAt_ = frames; grabPath_ = path; }

  // Drive the camera from outside, exercising the same calls the mouse handlers use.
  // Exists so the orbit controls are verifiable from a headless run.
  void setView(float yaw, float pitch) { camera_.setYawAndPitch(yaw, pitch); }

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

  WoWModel* model_ = nullptr;
  QTimer* timer_ = nullptr;
  bool videoReady_ = false;
  QString error_;

  OrbitCamera camera_;
  QPoint lastMousePos_;

  Light lights_[MAX_LIGHTS];

  int frame_ = 0;
  int grabAt_ = -1;
  QString grabPath_;
};

#endif
