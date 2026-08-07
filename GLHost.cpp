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

  // Lighting no longer needs the wx LightControl panel to exist (Phase 0). reset()
  // seeds the defaults and switches light 0 on; applyLights() is our own version of
  // SceneLighting::apply -- see there for why.
  SceneLighting::reset(lights_);
  videoReady_ = true;          // applyLights() checks this before touching GL
  applyLights();

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

// --- things the menu drives -------------------------------------------------

void GLHost::setBackgroundColour(const QColor& c)
{
  if (!c.isValid())
    return;
  bg_[0] = float(c.redF());
  bg_[1] = float(c.greenF());
  bg_[2] = float(c.blueF());
}

QColor GLHost::backgroundColour() const
{
  return QColor::fromRgbF(qreal(bg_[0]), qreal(bg_[1]), qreal(bg_[2]));
}

void GLHost::applyCameraPreset(int index)
{
  // yaw, pitch. Pitch 90 is level with the pivot; OrbitCamera clamps it to (0,180).
  static const struct { float yaw, pitch; } kPresets[] = {
    {   0.0f, 90.0f },   // front -- the same angle OrbitCamera::reset() uses
    {  45.0f, 80.0f },   // three-quarter
    {  90.0f, 90.0f },   // side
    {   0.0f, 12.0f }    // top-down
  };
  if (index < 0 || index >= int(sizeof(kPresets) / sizeof(kPresets[0])))
    return;
  camera_.setYawAndPitch(kPresets[index].yaw, kPresets[index].pitch);
}

bool GLHost::saveScreenshot(const QString& path)
{
  if (!videoReady_ || path.isEmpty())
    return false;

  // render() does the readback: the back buffer's contents are undefined once it has
  // swapped, so the grab has to happen inside the same draw.
  shotPath_ = path;
  shotOk_ = false;
  render();
  shotPath_.clear();
  return shotOk_;
}

void GLHost::setActiveLight(int i)
{
  if (i >= 0 && i < (int)MAX_LIGHTS)
    activeLight_ = i;
}

void GLHost::applyLights()
{
  if (!videoReady_)
    return;
  video.SetCurrent();

  // SceneLighting::apply() carries a documented upstream quirk: it writes the colour and
  // position of EVERY light into the ACTIVE light's GL slot, so editing light 2 changed
  // light 0 instead. Harmless while nothing edited them (the wx panel was never shown),
  // but with a real light panel it makes the controls look broken. Program each light
  // into its own slot here; the enable/attenuation semantics are otherwise the same.
  for (size_t i = 0; i < MAX_LIGHTS; i++) {
    const GLuint id = GL_LIGHT0 + (GLuint)i;
    const Light& l = lights_[i];

    if (l.enabled)
      glEnable(id);
    else
      glDisable(id);

    glLightfv(id, GL_DIFFUSE, glm::value_ptr(l.diffuse));
    glLightfv(id, GL_AMBIENT, glm::value_ptr(l.ambience));
    glLightfv(id, GL_SPECULAR, glm::value_ptr(l.specular));
    glLightfv(id, GL_POSITION, glm::value_ptr(l.pos));

    glLightf(id, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(id, GL_LINEAR_ATTENUATION, 0.0f);
    glLightf(id, GL_QUADRATIC_ATTENUATION, 0.0f);
    glLightf(id, GL_SPOT_CUTOFF, 180.0f);

    const float straightDown[3] = { 0.0f, -1.0f, 0.0f };
    glLightfv(id, GL_SPOT_DIRECTION, straightDown);

    if (l.type == LIGHT_POSITIONAL) {
      glLightf(id, GL_CONSTANT_ATTENUATION, l.constant_int);
      glLightf(id, GL_LINEAR_ATTENUATION, l.linear_int);
      glLightf(id, GL_QUADRATIC_ATTENUATION, l.quadradic_int);
    } else if (l.type == LIGHT_SPOT) {
      glLightf(id, GL_SPOT_CUTOFF, l.arc);
      glLightfv(id, GL_SPOT_DIRECTION, glm::value_ptr(l.target));
    }
  }
}

void GLHost::drawGrid()
{
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);

  // Scale the grid to the model so it stays a reference rather than a distraction:
  // one line per unit out to roughly the orbit distance.
  const float extent = qBound(5.0f, camera_.radius() * 1.5f, 200.0f);
  const float step = extent / 20.0f;

  glBegin(GL_LINES);
  for (int i = -20; i <= 20; ++i) {
    const float p = i * step;
    // The axes read slightly brighter, everything else stays near the background.
    if (i == 0)
      glColor3f(0.32f, 0.35f, 0.40f);
    else
      glColor3f(0.13f, 0.15f, 0.18f);
    glVertex3f(p, -extent, 0.0f);
    glVertex3f(p,  extent, 0.0f);
    glVertex3f(-extent, p, 0.0f);
    glVertex3f( extent, p, 0.0f);
  }
  glEnd();

  // video.InitGL() leaves GL_COLOR_MATERIAL enabled for AMBIENT/DIFFUSE/EMISSION/SPECULAR,
  // so the current glColor IS the material of whatever is drawn next. Leaving the grid's
  // dim grey set here tinted the model that follows -- it rendered noticeably darker with
  // the grid on than with it off.
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  glEnable(GL_LIGHTING);
  glEnable(GL_TEXTURE_2D);
}

bool GLHost::grabTo(const QString& path)
{
  const int w = width(), h = height();
  if (w <= 0 || h <= 0)
    return false;

  QImage img(w, h, QImage::Format_RGBA8888);
  glFinish();
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
  return img.mirrored(false, true).save(path);   // GL's origin is bottom-left
}

// ---------------------------------------------------------------------------

void GLHost::frameVisible()
{
  glm::vec3 mn, mx;
  if (!model_ || !model_->visibleBounds(mn, mx)) {
    camera_.reset(model_);
    return;
  }
  const glm::vec3 center = (mn + mx) * 0.5f;
  const glm::vec3 d = mx - center;

  // Longest half-AXIS, not the half-diagonal. frameBounds pulls back far enough to fit a
  // sphere of the given radius, and the sphere around a two-handed sword is mostly empty
  // air -- the blade came out a sliver in the middle of the viewport. Fitting the longest
  // axis makes that axis fill the frame instead; a compact piece barely notices (for a
  // cube-ish shape the two differ by the usual sqrt(3)), and nothing can be cropped,
  // because no other axis is longer than the one being fitted.
  const float radius = std::max(d.x, std::max(d.y, d.z));
  camera_.frameBounds(center, radius > 0.0001f ? radius : 0.1f);
}

void GLHost::tick()
{
  if (!videoReady_)
    return;
  if (model_)
    model_->update(16);
  ++frame_;
  render();

  // Sample over a window rather than per frame: 1/frametime jitters too much to read.
  if (!fpsClock_.isValid())
    fpsClock_.start();
  ++fpsFrames_;
  const qint64 elapsed = fpsClock_.elapsed();
  if (elapsed >= 500) {
    fps_ = float(fpsFrames_) * 1000.0f / float(elapsed);
    fpsFrames_ = 0;
    fpsClock_.restart();
  }
}

void GLHost::render()
{
  video.SetCurrent();

  glClearColor(bg_[0], bg_[1], bg_[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Viewport and projection every frame, as ModelCanvas::Render does. resizeEvent sets
  // them too, but a render pass is free to leave the matrix stack somewhere else, and
  // re-establishing them costs nothing.
  //
  // Deliberately the engine's own ResizeGLScene rather than a hand-rolled
  // glm::perspective: video.fov is in DEGREES (see OrbitCamera::frameBounds, which feeds
  // it through glm::radians), and gluPerspective takes degrees. Building the matrix with
  // glm::perspective would need radians and silently produce a different field of view.
  video.ResizeGLScene(width(), height());

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  const glm::mat4 view = camera_.getViewMatrix();
  glMultMatrixf(glm::value_ptr(view));

  // The state the render passes ASSUME somebody else has established. Under wx that
  // somebody is ModelCanvas::RenderObjects(); it was never ported, and video.InitGL()
  // only sets glDepthFunc without ever enabling the test. Without this the model draws
  // in submission order with no occlusion at all: interior geometry gets painted over
  // the skin (an orc seen from behind showed its open mouth and teeth through the back
  // of its head) and the whole figure reads as semi-transparent.
  //
  // ModelRenderPass::init/deinit owns GL_BLEND, GL_ALPHA_TEST, GL_CULL_FACE and
  // glDepthMask per pass -- none of those belong here.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_LIGHTING);
  glEnable(GL_TEXTURE_2D);

  if (showGrid_)
    drawGrid();

  // Draw the attachment tree, not the model directly: equipped items, shoulders,
  // weapons and the like hang off it as children.
  if (root_)
    root_->draw();

  // Particles last, with depth writes off so they blend against the solid geometry
  // instead of clipping one another -- weapon glows, enchant effects, spell visuals.
  // This pass was missing entirely, so none of them were drawn.
  if (root_) {
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    root_->drawParticles();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
  }

  // Read back BEFORE swapping: after a buffer swap the back buffer's contents are
  // undefined.
  if (!shotPath_.isEmpty())
    shotOk_ = grabTo(shotPath_);

  if (grabAt_ > 0 && frame_ >= grabAt_) {
    grabTo(grabPath_);
    grabAt_ = -1;
    qApp->quit();
  }

  video.SwapBuffers();
}
