#include "Theme.h"
#include "InspectorTabs.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "BlenderAddonInstaller.h"
#include "ExportController.h"
#include "GLHost.h"
#include "MenuController.h"
#include "WoWModel.h"
#include "modelheaders.h"

namespace {
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
    .arg(tok::kTextSoft).arg(tok::kBorder).arg(tok::kCard).arg(tok::kAccent);
}

QLabel* sectionLabel(const QString& text)
{
  auto* l = new QLabel(text);
  QFont f(uiFamily(), 7);
  f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
  l->setFont(f);
  l->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  return l;
}
}

// --- Charakter: Import und Export --------------------------------------------

namespace {
QLineEdit* urlField(const QString& placeholder)
{
  auto* e = new QLineEdit;
  e->setPlaceholderText(placeholder);
  e->setFont(QFont(uiFamily(), 8));
  e->setFixedHeight(28);
  e->setStyleSheet(QString(
    "QLineEdit { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:0 8px; color:%3; }"
    "QLineEdit:focus { border-color:#3a434f; }").arg(tok::kCard).arg(tok::kBorder).arg(tok::kText));
  return e;
}

QPushButton* accentButton(const QString& text)
{
  auto* b = new QPushButton(text);
  b->setFont(QFont(uiFamily(), 8));
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QString(
    "QPushButton { background:%1; border:none; border-radius:7px;"
    " color:%2; padding:8px 12px; }"
    "QPushButton:hover { background:#c084fc; }"
    "QPushButton:disabled { background:#252b34; color:#5f6874; }")
    .arg(tok::kAccent).arg(tok::kOnAccent));
  return b;
}

QPushButton* quietButton(const QString& text)
{
  auto* b = new QPushButton(text);
  b->setFont(QFont(uiFamily(), 8));
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QString(
    "QPushButton { background:%1; border:1px solid %2; border-radius:7px;"
    " color:%3; padding:8px 12px; }"
    "QPushButton:hover { border-color:#3a434f; }")
    .arg(tok::kCard).arg(tok::kBorder).arg(tok::kTextSoft));
  return b;
}
}

CharacterIoTab::CharacterIoTab(MenuController* menus, ExportController* exporters,
                               GLHost* canvas, QWidget* parent)
  : QWidget(parent), menus_(menus), exporters_(exporters), canvas_(canvas)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(10);

  // --- Aus dem Spiel (MVLink)
  //
  // This replaced the Wowhead dressing-room field. That route needed the look rebuilt on
  // a website and pasted back; the addon reads what the character is actually wearing,
  // which is the same job done in one step. The decoder and the --dressing-room flag stay
  // for scripts and for the 56-case test corpus -- only the control is gone.
  col->addWidget(sectionLabel(QString::fromUtf8("AUS DEM SPIEL — MVLINK-ADDON")));
  auto* mvTop = new QLabel(QString::fromUtf8(
    "Zwei Wege — beide führen zum selben Ergebnis:"));
  mvTop->setFont(QFont(uiFamily(), 8));
  mvTop->setWordWrap(true);
  mvTop->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kTextSoft));
  col->addWidget(mvTop);

  auto* mvFileBtn2 = accentButton(QString::fromUtf8("Direkt aus WoW holen"));
  connect(mvFileBtn2, &QPushButton::clicked, this, &CharacterIoTab::importMVLinkFromGame);
  col->addWidget(mvFileBtn2);
  auto* mvFileHint = new QLabel(QString::fromUtf8(
    "Liest, was das Addon abgelegt hat — Stand des letzten /reload oder Ausloggens."));
  mvFileHint->setFont(QFont(uiFamily(), 8));
  mvFileHint->setWordWrap(true);
  mvFileHint->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(mvFileHint);

  mvlinkCode_ = urlField(QString::fromUtf8("… oder Code hier einfügen: MVM1:R=…"));
  col->addWidget(mvlinkCode_);
  auto* mvHint = new QLabel(QString::fromUtf8(
    "Im Spiel /mvlink öffnen, Code markieren (Strg+C), hier einfügen — wirkt sofort, "
    "ohne /reload. Gesicht und Frisur kann ein Addon nicht auslesen; die bleiben, wie "
    "sie hier eingestellt sind."));
  mvHint->setFont(QFont(uiFamily(), 8));
  mvHint->setWordWrap(true);
  mvHint->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(mvHint);
  auto* mvBtn = quietButton(QString::fromUtf8("Code übernehmen"));
  connect(mvBtn, &QPushButton::clicked, this, &CharacterIoTab::importMVLink);
  connect(mvlinkCode_, &QLineEdit::returnPressed, this, &CharacterIoTab::importMVLink);
  col->addWidget(mvBtn);

  // Neither route works until the addon is actually in the game folder, and the setup
  // cannot put it there -- at install time nobody knows where WoW lives. So it ships beside
  // the exe and lands here, the same way the Blender add-on does.
  col->addSpacing(2);
  auto* mvInstallBtn = quietButton(QString::fromUtf8("MVLink-Addon in WoW installieren"));
  connect(mvInstallBtn, &QPushButton::clicked, this, &CharacterIoTab::installMVLinkAddon);
  col->addWidget(mvInstallBtn);
  auto* mvInstallHint = new QLabel(QString::fromUtf8(
    "Einmalig — danach im Spiel unter Addons aktivieren. Ein Update überschreibt die "
    "Dateien; WoW sollte dabei geschlossen sein."));
  mvInstallHint->setFont(QFont(uiFamily(), 8));
  mvInstallHint->setWordWrap(true);
  mvInstallHint->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(mvInstallHint);

  // --- Armory
  col->addSpacing(4);
  col->addWidget(sectionLabel(QString::fromUtf8("ARMORY")));
  armoryUrl_ = urlField(QString::fromUtf8("https://worldofwarcraft.blizzard.com/…"));
  col->addWidget(armoryUrl_);
  auto* amBtn = quietButton(QString::fromUtf8("Armory-Charakter importieren"));
  connect(amBtn, &QPushButton::clicked, this, &CharacterIoTab::importArmory);
  connect(armoryUrl_, &QLineEdit::returnPressed, this, &CharacterIoTab::importArmory);
  col->addWidget(amBtn);

  // --- Datei
  col->addSpacing(4);
  col->addWidget(sectionLabel(QString::fromUtf8("CHARAKTERDATEI")));
  auto* fileRow = new QHBoxLayout;
  fileRow->setSpacing(6);
  auto* loadBtn = quietButton(QString::fromUtf8("Laden"));
  auto* saveBtn = quietButton(QString::fromUtf8("Speichern"));
  connect(loadBtn, &QPushButton::clicked, this, [this]() {
    if (menus_)
      menus_->loadCharacter();
  });
  connect(saveBtn, &QPushButton::clicked, this, [this]() {
    if (menus_)
      menus_->saveCharacter();
  });
  fileRow->addWidget(loadBtn, 1);
  fileRow->addWidget(saveBtn, 1);
  col->addLayout(fileRow);

  // --- Blender
  col->addSpacing(4);
  col->addWidget(sectionLabel(QString::fromUtf8("NACH BLENDER")));
  auto* blHint = new QLabel(QString::fromUtf8(
    "Schreibt FBX mit Netz, Skelett und Gewichtung. OBJ kann kein Skelett — für "
    "Blender ist FBX der Weg. Animationen wählst du im Reiter „Export“."));
  blHint->setFont(QFont(uiFamily(), 8));
  blHint->setWordWrap(true);
  blHint->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(blHint);
  auto* blBtn = accentButton(QString::fromUtf8("Als FBX exportieren"));
  connect(blBtn, &QPushButton::clicked, this, &CharacterIoTab::exportForBlender);
  col->addWidget(blBtn);

  // One button instead of "find your addons folder" instructions. Re-running it is the
  // update path too, so the label says both.
  auto* addonBtn = quietButton(QString::fromUtf8("Blender-Addon installieren/aktualisieren"));
  connect(addonBtn, &QPushButton::clicked, this, [this]() {
    const auto r = BlenderAddonInstaller::install();
    if (!r.error.isEmpty())
      setStatus(r.error, true);
    else
      setStatus(QString::fromUtf8("Addon in %1 Blender-Version(en) installiert. Einmalig "
                                  "in Blender aktivieren: Edit → Preferences → Add-ons → "
                                  "\"WoW Model Viewer FBX\".").arg(r.installedVersions),
                false);
  });
  col->addWidget(addonBtn);

  status_ = new QLabel;
  status_->setFont(QFont(uiFamily(), 8));
  status_->setWordWrap(true);
  status_->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(status_);

  col->addStretch(1);
}

void CharacterIoTab::setStatus(const QString& text, bool error)
{
  if (!status_)
    return;
  status_->setStyleSheet(QString("color:%1; background:transparent;")
                           .arg(error ? "#d98b6a" : tok::kTextSoft));
  status_->setText(text);
}

void CharacterIoTab::importMVLink()
{
  if (!menus_)
    return;
  const QString code = mvlinkCode_->text().trimmed();
  if (code.isEmpty()) {
    setStatus(QString::fromUtf8("Bitte zuerst den Code aus dem Addon einfügen."), true);
    return;
  }
  const QString err = menus_->importMVLinkCode(code, true);
  if (err.isEmpty())
    setStatus(QString::fromUtf8("Look aus dem Spiel übernommen."), false);
  else
    setStatus(err, true);
}

void CharacterIoTab::importMVLinkFromGame()
{
  if (!menus_)
    return;
  // No copying: read the addon's SavedVariables directly. Only as fresh as the last
  // /reload or logout, because that is when WoW writes the file -- the status line says
  // so rather than leaving the user wondering why nothing changed.
  const QString err = menus_->importMVLinkFromGame(QString(), true);
  if (err.isEmpty())
    setStatus(QString::fromUtf8("Look aus WoW übernommen (Stand: letztes /reload "
                                "oder Ausloggen)."), false);
  else
    setStatus(err, true);
}

void CharacterIoTab::installMVLinkAddon()
{
  if (!menus_)
    return;
  QString dest;
  const QString err = menus_->installMVLinkAddon(&dest);
  if (!err.isEmpty()) {
    setStatus(err, true);
    return;
  }
  // The path is part of the message on purpose: if the game is installed twice, this is the
  // only way to see which copy just got the addon.
  setStatus(QString::fromUtf8("Addon installiert: %1 — in WoW /reload, dann /mvlink.")
              .arg(dest), false);
}

void CharacterIoTab::importArmory()
{
  if (!menus_)
    return;
  const QString url = armoryUrl_->text().trimmed();
  if (url.isEmpty()) {
    setStatus(QString::fromUtf8("Bitte zuerst eine Armory-Adresse einsetzen."), true);
    return;
  }
  setStatus(QString::fromUtf8("Frage die Armory ab …"), false);
  // Same reason as above, and it matters more here: this one waits on the network.
  QApplication::processEvents();
  const QString err = menus_->importArmory(url, false);
  setStatus(err.isEmpty() ? QString::fromUtf8("Charakter übernommen.") : err, !err.isEmpty());
}

int CharacterIoTab::fbxFormatIndex() const
{
  if (!exporters_)
    return -1;
  const auto& formats = exporters_->formats();
  for (size_t i = 0; i < formats.size(); ++i)
    if (formats[i].label.contains("fbx", Qt::CaseInsensitive) ||
        formats[i].filter.contains("fbx", Qt::CaseInsensitive))
      return (int)i;
  return -1;
}

void CharacterIoTab::exportForBlender()
{
  if (!exporters_ || !canvas_)
    return;
  if (!canvas_->model()) {
    setStatus(QString::fromUtf8("Kein Modell geladen."), true);
    return;
  }

  const int fbx = fbxFormatIndex();
  if (fbx < 0) {
    setStatus(QString::fromUtf8("Der FBX-Exporter fehlt. Liegt der Ordner \"plugins\" "
                                "neben der Anwendung?"), true);
    return;
  }

  // Mesh, skeleton and skinning, no animation: that is what makes a character usable in
  // Blender straight away. Animation stays out on purpose -- it needs a clip selection,
  // and that is what the Export tab is for.
  ExportController::Options o;
  o.mesh = o.skeleton = o.skinning = true;
  o.animation = false;
  exporters_->setOptions(o);

  const QString err = exporters_->exportModel(canvas_->model(), fbx, this);
  setStatus(err.isEmpty()
              ? QString::fromUtf8("FBX geschrieben. In Blender: Seitenleiste (N) → "
                                  "Reiter \"WMV\" → \"Letzten WMV-Export importieren\".")
              : err,
            !err.isEmpty());
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
    " selection-background-color:#1a1226; color:%3; }")
    .arg(tok::kCard).arg(tok::kBorder).arg(tok::kText));
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
    "QListWidget::item:selected { background:#1a1226; color:%4; }"
    "QScrollBar:vertical { background:transparent; width:9px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:4px; min-height:24px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }")
    .arg(tok::kCard).arg(tok::kBorder).arg(tok::kText).arg(tok::kAccent));
  clipList_->setVisible(false);
  col->addWidget(clipList_);

  clipHint_ = new QLabel;
  clipHint_->setFont(QFont(uiFamily(), 8));
  clipHint_->setWordWrap(true);
  clipHint_->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
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
    "QPushButton { background:%1; border:1px solid #c084fc; border-radius:8px;"
    " color:%2; padding:9px 14px; }"
    "QPushButton:hover { background:#c084fc; }"
    "QPushButton:disabled { background:#252b34; border-color:%3; color:#5f6874; }")
    .arg(tok::kAccent).arg(tok::kOnAccent).arg(tok::kBorder));
  col->addWidget(button);

  status_ = new QLabel;
  status_->setFont(QFont(uiFamily(), 8));
  status_->setWordWrap(true);
  status_->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
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

  // Built from anims[], NOT from getAnimsMap(). The comment here used to claim that map was
  // "keyed by the model's animation index" -- it is not. Its key is AnimationData.ID, a
  // database id running well past the end of anims[], while setAnimationsToExport() feeds
  // these values straight into model->anims[] as indices. Ticking one clip could therefore
  // export a different one, or none at all. TimelinePanel::rebuildAnimations() already gets
  // this right; this is the same construction.
  //
  // The map stays the source for the readable NAME, looked up by each entry's own animID.
  const std::map<int, std::wstring> names = m->getAnimsMap();
  std::map<int, int> seen;                        // animID -> how often already listed
  for (size_t i = 0; i < m->anims.size(); ++i) {
    const int animId = m->anims[i].animID;
    const auto nameIt = names.find(animId);
    QString label = (nameIt != names.end()) ? QString::fromStdWString(nameIt->second)
                                            : QString("Animation %1").arg(animId);
    // Several variations share one name; without this they are indistinguishable rows.
    const int n = ++seen[animId];
    if (n > 1)
      label += QString(" (%1)").arg(n);

    auto* item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, (int)i);
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
