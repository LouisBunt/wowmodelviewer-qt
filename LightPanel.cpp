#include "Theme.h"
#include "LightPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include "GLHost.h"
#include "SceneLighting.h"

namespace {
QString uiFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (have.contains(f))
      return f;
  return QStringLiteral("sans-serif");
}



QLabel* caption(const QString& text, int pt, const char* colour, qreal spacing = 0)
{
  auto* l = new QLabel(text);
  QFont f(uiFamily(), pt);
  if (spacing > 0)
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
  l->setFont(f);
  return l;
}
}

LightPanel::LightPanel(GLHost* host, QWidget* parent) : QWidget(parent), host_(host)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setProperty("role", "panel");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(12);

  col->addWidget(caption(QString::fromUtf8("LICHTQUELLE"), 7, tok::fgMuted, 1.4));

  lightPicker_ = new QComboBox;
  lightPicker_->setFont(typo::font(typo::Body));
  for (size_t i = 0; i < MAX_LIGHTS; ++i)
    lightPicker_->addItem(QString::fromUtf8("Licht %1").arg(i + 1));
  connect(lightPicker_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int i) { if (!updating_) selectLight(i); });
  col->addWidget(lightPicker_);

  enabled_ = new QCheckBox(QString::fromUtf8("Eingeschaltet"));
  enabled_->setFont(typo::font(typo::Body));
  connect(enabled_, &QCheckBox::toggled, this, [this]() { if (!updating_) writeToLight(); });
  col->addWidget(enabled_);

  type_ = new QComboBox;
  type_->setFont(typo::font(typo::Body));
  type_->addItem(QString::fromUtf8("Punktlicht"), LIGHT_POSITIONAL);
  type_->addItem(QString::fromUtf8("Spot"), LIGHT_SPOT);
  type_->addItem(QString::fromUtf8("Gerichtet"), LIGHT_DIRECTIONAL);
  connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() { if (!updating_) writeToLight(); });
  col->addWidget(type_);

  // A row of "label + slider", used for colour and position alike.
  auto addSlider = [this, col](const QString& label, int lo, int hi, QSlider** out) {
    auto* row = new QWidget;
    auto* rr = new QHBoxLayout(row);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(8);
    auto* name = caption(label, 8, tok::fgSoft);
    name->setFixedWidth(74);
    rr->addWidget(name);
    auto* s = new QSlider(Qt::Horizontal);
    s->setRange(lo, hi);
    connect(s, &QSlider::valueChanged, this, [this]() { if (!updating_) writeToLight(); });
    rr->addWidget(s, 1);
    col->addWidget(row);
    *out = s;
  };

  col->addWidget(caption(QString::fromUtf8("FARBE"), 7, tok::fgMuted, 1.4));

  swatch_ = new QLabel;
  swatch_->setFixedHeight(16);
  col->addWidget(swatch_);

  addSlider(QString::fromUtf8("Rot"), 0, 100, &colourR_);
  addSlider(QString::fromUtf8("Grün"), 0, 100, &colourG_);
  addSlider(QString::fromUtf8("Blau"), 0, 100, &colourB_);
  addSlider(QString::fromUtf8("Helligkeit"), 0, 200, &intensity_);

  col->addWidget(caption(QString::fromUtf8("POSITION"), 7, tok::fgMuted, 1.4));
  addSlider(QString::fromUtf8("X"), -100, 100, &posX_);
  addSlider(QString::fromUtf8("Y"), -100, 100, &posY_);
  addSlider(QString::fromUtf8("Z"), -100, 100, &posZ_);

  hint_ = caption(QString::fromUtf8("Bei „Gerichtet“ wirkt die Position als Richtung, "
                                    "nicht als Ort."), 8, tok::fgMuted);
  hint_->setWordWrap(true);
  col->addWidget(hint_);

  col->addStretch(1);

  selectLight(0);
}

void LightPanel::selectLight(int index)
{
  if (!host_ || index < 0 || index >= (int)MAX_LIGHTS)
    return;
  current_ = index;
  host_->setActiveLight(index);
  readFromLight();
}

void LightPanel::readFromLight()
{
  if (!host_)
    return;

  const Light& l = host_->lights()[current_];

  updating_ = true;
  lightPicker_->setCurrentIndex(current_);
  enabled_->setChecked(l.enabled);

  const int typeRow = type_->findData((int)l.type);
  type_->setCurrentIndex(typeRow >= 0 ? typeRow : 2);

  // The diffuse colour is stored 0..1; the sliders are whole percent. Brightness is the
  // largest component, so dragging it keeps the hue and scales the whole colour.
  const float peak = qMax(qMax(l.diffuse.r, l.diffuse.g), qMax(l.diffuse.b, 0.0001f));
  colourR_->setValue(int(l.diffuse.r / peak * 100.0f));
  colourG_->setValue(int(l.diffuse.g / peak * 100.0f));
  colourB_->setValue(int(l.diffuse.b / peak * 100.0f));
  intensity_->setValue(int(peak * 100.0f));

  posX_->setValue(int(l.pos.x * 10.0f));
  posY_->setValue(int(l.pos.y * 10.0f));
  posZ_->setValue(int(l.pos.z * 10.0f));
  updating_ = false;

  // Deliberately NOT writeToLight(). Reading has to stay read-only: this panel is built
  // during MainWindow's constructor, and writing back there re-derived the light from the
  // widgets and changed it -- ambience dropped to a third of what SceneLighting::reset()
  // set, and pos.w flipped to 0, turning the scene light directional. The whole viewport
  // came out brighter and yellower before anyone had touched a slider.
  updateSwatch();
}

void LightPanel::updateSwatch()
{
  if (!host_)
    return;
  const glm::vec4& d = host_->lights()[current_].diffuse;
}

void LightPanel::writeToLight()
{
  if (!host_)
    return;

  Light& l = host_->lights()[current_];

  const float scale = intensity_->value() / 100.0f;
  const float r = colourR_->value() / 100.0f * scale;
  const float g = colourG_->value() / 100.0f * scale;
  const float b = colourB_->value() / 100.0f * scale;

  l.enabled = enabled_->isChecked();
  l.type = (unsigned short)type_->currentData().toInt();
  l.diffuse = glm::vec4(r, g, b, 1.0f);
  l.ambience = glm::vec4(r * 0.35f, g * 0.35f, b * 0.35f, 1.0f);
  l.specular = glm::vec4(r, g, b, 1.0f);

  // w = 0 marks a directional light for the fixed-function pipeline; positional and spot
  // lights need w > 0 or GL treats their position as a direction.
  const float w = (l.type == LIGHT_DIRECTIONAL) ? 0.0f : 1.0f;
  l.pos = glm::vec4(posX_->value() / 10.0f, posY_->value() / 10.0f,
                    posZ_->value() / 10.0f, w);

  host_->applyLights();
  updateSwatch();
}
