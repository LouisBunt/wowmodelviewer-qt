#include "InspectorTabs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "ExportController.h"
#include "GLHost.h"
#include "WoWModel.h"
#include "modelheaders.h"

namespace {
const char* kText   = "#e8eaee";
const char* kSoft   = "#b6bdc8";
const char* kDim    = "#5f6874";
const char* kCard   = "#14181e";
const char* kBord   = "#23282f";
const char* kAccent = "#c8a15a";
const char* kOnAcc  = "#17130a";

QString uiFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (have.contains(f))
      return f;
  return QStringLiteral("sans-serif");
}

QString monoFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Mono"), QStringLiteral("Consolas")})
    if (have.contains(f))
      return f;
  return QStringLiteral("monospace");
}

QString checkboxStyle()
{
  return QString(
    "QCheckBox { color:%1; background:transparent; spacing:7px; }"
    "QCheckBox::indicator { width:13px; height:13px; border-radius:3px;"
    " border:1px solid %2; background:%3; }"
    "QCheckBox::indicator:checked { background:%4; border-color:%4; }")
    .arg(kSoft).arg(kBord).arg(kCard).arg(kAccent);
}

QLabel* sectionLabel(const QString& text)
{
  auto* l = new QLabel(text);
  QFont f(uiFamily(), 7);
  f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
  l->setFont(f);
  l->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  return l;
}
}

// --- Material ---------------------------------------------------------------

MaterialTab::MaterialTab(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(12);

  header_ = sectionLabel(QString::fromUtf8("GEOSETS"));
  col->addWidget(header_);

  rows_ = new QVBoxLayout;
  rows_->setSpacing(4);
  col->addLayout(rows_);

  col->addWidget(sectionLabel(QString::fromUtf8("TEXTUREN")));
  textureInfo_ = new QLabel;
  textureInfo_->setFont(QFont(monoFamily(), 8));
  textureInfo_->setStyleSheet(QString("color:%1; background:transparent;").arg(kSoft));
  textureInfo_->setWordWrap(true);
  col->addWidget(textureInfo_);

  col->addStretch(1);
  textureInfo_->setText(QString::fromUtf8("—"));
}

void MaterialTab::setModel(WoWModel* model)
{
  model_ = model;
  rebuild();
}

void MaterialTab::rebuild()
{
  while (QLayoutItem* item = rows_->takeAt(0)) {
    if (QWidget* w = item->widget())
      w->deleteLater();
    delete item;
  }

  if (!model_) {
    header_->setText(QString::fromUtf8("GEOSETS"));
    textureInfo_->setText(QString::fromUtf8("Kein Modell geladen."));
    return;
  }

  updating_ = true;
  int shown = 0;
  for (size_t i = 0; i < model_->geosets.size(); ++i) {
    ModelGeosetHD* g = model_->geosets[i];
    if (!g)
      continue;

    auto* cb = new QCheckBox(QString("%1  (%2 Dreiecke)")
                               .arg(g->id).arg(g->icount / 3));
    cb->setFont(QFont(uiFamily(), 8));
    cb->setStyleSheet(checkboxStyle());
    cb->setChecked(g->display);
    connect(cb, &QCheckBox::toggled, this, [this, g](bool on) {
      if (updating_ || !model_)
        return;
      g->display = on;
    });
    rows_->addWidget(cb);
    if (g->display)
      ++shown;
  }
  updating_ = false;

  header_->setText(QString::fromUtf8("GEOSETS · %1 / %2")
                     .arg(shown).arg(model_->geosets.size()));

  // textures is private on WoWModel; the render passes are the public view of the
  // same thing and are what actually matters for material inspection.
  textureInfo_->setText(QString::fromUtf8("%1 Renderdurchgänge")
                          .arg(model_->passes.size()));
}

// --- Export -----------------------------------------------------------------

ExportTab::ExportTab(ExportController* exporters, GLHost* canvas, QWidget* parent)
  : QWidget(parent), exporters_(exporters), canvas_(canvas)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(12);

  col->addWidget(sectionLabel(QString::fromUtf8("FORMAT")));

  format_ = new QComboBox;
  format_->setFont(QFont(uiFamily(), 8));
  format_->setStyleSheet(QString(
    "QComboBox { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:4px 8px; color:%3; }"
    "QComboBox::drop-down { border:none; width:18px; }"
    "QComboBox QAbstractItemView { background:%1; border:1px solid %2;"
    " selection-background-color:#181510; color:%3; }")
    .arg(kCard).arg(kBord).arg(kText));
  col->addWidget(format_);

  col->addWidget(sectionLabel(QString::fromUtf8("OPTIONEN")));

  optMesh_      = new QCheckBox(QString::fromUtf8("Geometrie"));
  optSkinning_  = new QCheckBox(QString::fromUtf8("Skinning"));
  optSkeleton_  = new QCheckBox(QString::fromUtf8("Skelett"));
  optAnimation_ = new QCheckBox(QString::fromUtf8("Animationen"));
  // Blender's default expectation: mesh with an armature and vertex weights.
  optMesh_->setChecked(true);
  optSkinning_->setChecked(true);
  optSkeleton_->setChecked(true);
  for (QCheckBox* c : { optMesh_, optSkinning_, optSkeleton_, optAnimation_ }) {
    c->setFont(QFont(uiFamily(), 8));
    c->setStyleSheet(checkboxStyle());
    col->addWidget(c);
  }
  optSkinning_->setToolTip(QString::fromUtf8(
    "Braucht Geometrie und Skelett -- der Exporter schaltet beides bei Bedarf selbst zu."));

  // The clip list, shown only while "Animationen" is on. Multi-select, because the FBX
  // exporter takes a list of animation indices and writes one take per entry.
  clipList_ = new QListWidget;
  clipList_->setFont(QFont(uiFamily(), 8));
  clipList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  clipList_->setFixedHeight(150);
  clipList_->setStyleSheet(QString(
    "QListWidget { background:%1; border:1px solid %2; border-radius:6px; color:%3; }"
    "QListWidget::item { padding:3px 5px; }"
    "QListWidget::item:selected { background:#181510; color:%4; }"
    "QScrollBar:vertical { background:transparent; width:9px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:4px; min-height:24px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }")
    .arg(kCard).arg(kBord).arg(kText).arg(kAccent));
  clipList_->setVisible(false);
  col->addWidget(clipList_);

  clipHint_ = new QLabel;
  clipHint_->setFont(QFont(uiFamily(), 8));
  clipHint_->setWordWrap(true);
  clipHint_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  clipHint_->setVisible(false);
  col->addWidget(clipHint_);

  connect(optAnimation_, &QCheckBox::toggled, this, [this](bool on) {
    clipList_->setVisible(on);
    clipHint_->setVisible(on);
    if (on)
      refreshClips();
  });

  auto* button = new QPushButton(QString::fromUtf8("Modell exportieren"));
  button->setFont(QFont(uiFamily(), 9, QFont::DemiBold));
  button->setCursor(Qt::PointingHandCursor);
  button->setStyleSheet(QString(
    "QPushButton { background:%1; border:1px solid #d9b678; border-radius:8px;"
    " color:%2; padding:9px 14px; }"
    "QPushButton:hover { background:#d9b678; }"
    "QPushButton:disabled { background:#252b34; border-color:%3; color:#5f6874; }")
    .arg(kAccent).arg(kOnAcc).arg(kBord));
  col->addWidget(button);

  status_ = new QLabel;
  status_->setFont(QFont(uiFamily(), 8));
  status_->setWordWrap(true);
  status_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  col->addWidget(status_);

  connect(button, &QPushButton::clicked, this, [this]() {
    if (!exporters_ || !canvas_)
      return;

    ExportController::Options o;
    o.mesh      = optMesh_->isChecked();
    o.skeleton  = optSkeleton_->isChecked();
    o.skinning  = optSkinning_->isChecked();
    o.animation = optAnimation_->isChecked();
    if (o.animation)
      for (QListWidgetItem* item : clipList_->selectedItems())
        o.clips.push_back(item->data(Qt::UserRole).toInt());

    if (o.animation && o.clips.empty()) {
      status_->setText(QString::fromUtf8("Keine Animation ausgewählt -- bitte mindestens "
                                        "einen Clip markieren."));
      return;
    }

    exporters_->setOptions(o);
    const QString err = exporters_->exportModel(canvas_->model(), format_->currentIndex(), this);
    if (err.isEmpty())
      status_->setText(QString::fromUtf8("Export abgeschlossen."));
    else
      status_->setText(err);
  });

  col->addStretch(1);
}

void ExportTab::refreshFormats()
{
  format_->clear();
  if (!exporters_)
    return;
  for (const auto& f : exporters_->formats())
    format_->addItem(f.label);
  if (format_->count() == 0)
    status_->setText(QString::fromUtf8("Keine Exporter gefunden."));
}

void ExportTab::refreshClips()
{
  clipList_->clear();
  WoWModel* m = canvas_ ? canvas_->model() : nullptr;
  if (!m) {
    clipHint_->setText(QString::fromUtf8("Kein Modell geladen."));
    return;
  }

  // Same source the timeline uses: keyed by the model's animation index, which is what
  // setAnimationsToExport() expects.
  for (const auto& a : m->getAnimsMap()) {
    auto* item = new QListWidgetItem(QString::fromStdWString(a.second));
    item->setData(Qt::UserRole, (int)a.first);
    clipList_->addItem(item);
  }

  if (clipList_->count() == 0) {
    clipHint_->setText(QString::fromUtf8("Dieses Modell hat keine Animationen."));
    return;
  }

  clipList_->setCurrentRow(0);
  clipHint_->setText(QString::fromUtf8("%1 Clips -- Mehrfachauswahl mit Strg/Shift. "
                                      "Jeder Clip wird als eigener Take geschrieben.")
                       .arg(clipList_->count()));
}
