#include "GLHost.h"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

#include <windows.h>

#include "glm/gtc/type_ptr.hpp"

#include "Attachment.h"
#include "SceneLighting.h"
#include "video.h"
#include "WoWModel.h"

GLHost::GLHost(QWidget* parent) : QWidget(parent)
{
  // A real HWND is required before the context can be created, and Qt must not
  // double-buffer or paint into the same surface.
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setAutoFillBackground(false);
  setMinimumSize(320, 240);
  setFocusPolicy(Qt::StrongFocus);   // so the widget receives key events
  setMouseTracking(false);           // only interested in drags

  timer_ = new QTimer(this);
  connect(timer_, &QTimer::timeout, this, &GLHost::tick);
}

GLHost::~GLHost()
{
  delete model_;
}

void GLHost::setModel(WoWModel* model)
{
  if (!root_)
    root_ = new Attachment(nullptr, nullptr, -1, -1);

  root_->delChildren();
  root_->setModel(nullptr);
  delete model_;

  model_ = model;
  if (model_)
    root_->addChild(model_, 0, -1);

  camera_.reset(model_);   // frame the camera on the new model
  update();
}

bool GLHost::initVideo()
{
  if (videoReady_)
    return true;

  // VideoSettings does NOT create a context: Init() only runs glewInit(), which
  // already requires one to be current, and SetCurrent()/SwapBuffers() use hDC/hRC
  // that somebody else must fill in. Under wx that somebody is wxGLCanvas.
  HWND hwnd = reinterpret_cast<HWND>(winId());
  HDC dc = ::GetDC(hwnd);
  if (!dc) {
    error_ = "GetDC failed on the widget's HWND";
    return false;
  }

  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;

  const int pf = ::ChoosePixelFormat(dc, &pfd);
  if (!pf || !::SetPixelFormat(dc, pf, &pfd)) {
    error_ = "no usable pixel format for the widget";
    return false;
  }

  HGLRC rc = ::wglCreateContext(dc);
  if (!rc || !::wglMakeCurrent(dc, rc)) {
    error_ = "wglCreateContext/wglMakeCurrent failed";
    return false;
  }

  video.hWnd = hwnd;
  video.hDC = dc;
  video.hRC = rc;
  video.desktopBPP = 32;

  if (!video.Init()) {              // glewInit() -- valid now that a context is current
    error_ = "video.Init() failed (GLEW)";
    return false;
  }

  video.InitGL();

  // Lighting no longer needs the wx LightControl panel to exist (Phase 0).
  SceneLighting::reset(lights_);
  SceneLighting::apply(lights_, 0);

  video.ResizeGLScene(width(), height());
  videoReady_ = true;
  return true;
}

void GLHost::showEvent(QShowEvent* e)
{
  QWidget::showEvent(e);
  if (initVideo())
    timer_->start(16);            // ~60 Hz
}

void GLHost::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  if (videoReady_)
    video.ResizeGLScene(e->size().width(), e->size().height());
}

// --- orbit controls, ported from ModelCanvas::OnMouse ----------------------

void GLHost::mousePressEvent(QMouseEvent* e)
{
  lastMousePos_ = e->pos();
  QWidget::mousePressEvent(e);
}

void GLHost::mouseMoveEvent(QMouseEvent* e)
{
  // Shift = finer control, as in the wx canvas.
  const float mul = (e->modifiers() & Qt::ShiftModifier) ? 0.1f : 1.0f;
  const float MOUSE_SENSITIVITY = 0.25f;

  const float deltax = (e->x() - lastMousePos_.x()) * MOUSE_SENSITIVITY * mul;
  const float deltay = (e->y() - lastMousePos_.y()) * MOUSE_SENSITIVITY * mul;

  if (e->buttons() & Qt::LeftButton) {
    camera_.setYawAndPitch(camera_.yaw() - deltax, camera_.pitch() - deltay);
  }
  else if (e->buttons() & Qt::RightButton) {
    // Pan proportionally to the orbit distance, so it stays usable on huge models
    // while remaining fine on small ones.
    const float x = deltax * camera_.radius() * 0.0025f;
    const float y = deltay * camera_.radius() * 0.0025f;

    const glm::vec3 look = camera_.lookAt();
    const glm::vec3 right = camera_.right();
    camera_.setLookAt(glm::vec3(look.x + right.x * -x, look.y + right.y * -x, look.z + y));
  }
  else if (e->buttons() & Qt::MiddleButton) {
    camera_.setRadius(camera_.radius() * powf(1.01f, deltay));
  }

  lastMousePos_ = e->pos();
  QWidget::mouseMoveEvent(e);
}

void GLHost::wheelEvent(QWheelEvent* e)
{
  // Multiplicative zoom (dolly): each notch SCALES the orbit distance, so zooming
  // stays fast far out and precise up close regardless of model size.
  const float notches = e->angleDelta().y() / 120.0f;
  const float perNotch = (e->modifiers() & Qt::ShiftModifier) ? 0.95f : 0.82f;
  camera_.setRadius(camera_.radius() * powf(perNotch, notches));
  e->accept();
}

void GLHost::keyPressEvent(QKeyEvent* e)
{
  if (e->key() == Qt::Key_R) {      // reframe on the model
    camera_.reset(model_);
    e->accept();
    return;
  }
  QWidget::keyPressEvent(e);
}

// ---------------------------------------------------------------------------

void GLHost::tick()
{
  if (!videoReady_)
    return;
  if (model_)
    model_->update(16);
  ++frame_;
  render();
}

void GLHost::render()
{
  video.SetCurrent();

  glClearColor(0.043f, 0.051f, 0.063f, 1.0f);   // #0b0d10, the mock-up's app background
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  const glm::mat4 view = camera_.getViewMatrix();
  glMultMatrixf(glm::value_ptr(view));

  // Draw the attachment tree, not the model directly: equipped items, shoulders,
  // weapons and the like hang off it as children.
  if (root_)
    root_->draw();

  // Read back BEFORE swapping: after a buffer swap the back buffer's contents are
  // undefined.
  if (grabAt_ > 0 && frame_ >= grabAt_) {
    const int w = width(), h = height();
    QImage img(w, h, QImage::Format_RGBA8888);
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    img.mirrored(false, true).save(grabPath_);   // GL's origin is bottom-left
    grabAt_ = -1;
    qApp->quit();
  }

  video.SwapBuffers();
}
