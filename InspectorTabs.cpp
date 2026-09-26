#include "Theme.h"
#include "UiKit.h"
#include "InspectorTabs.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QUrl>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "BlenderAddonInstaller.h"
#include "ExportController.h"
#include "GLHost.h"
#include "MenuController.h"
#include "SourceDialog.h"
#include "WoWModel.h"
#include "modelheaders.h"

namespace {



}

// --- Charakter: Import und Export --------------------------------------------

namespace {
QLineEdit* urlField(const QString& placeholder)
{
  auto* e = new QLineEdit;
  e->setPlaceholderText(placeholder);
  return e;
}

QPushButton* accentButton(const QString& text)
{
  auto* b = new QPushButton(text);
  b->setProperty("variant", "primary");
  b->setCursor(Qt::PointingHandCursor);
  return b;
}

QPushButton* quietButton(const QString& text)
{
  auto* b = new QPushButton(text);
  b->setCursor(Qt::PointingHandCursor);
  return b;
}
}

CharacterIoTab::CharacterIoTab(MenuController* menus, ExportController* exporters,
                               GLHost* canvas, QWidget* parent)
  : QWidget(parent), menus_(menus), exporters_(exporters), canvas_(canvas)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setProperty("role", "panel");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(10);

  // --- The look library
  //
  // Named .chr files, listed right here. F7/F8 could always save and load a character,
  // but through a file dialog per use -- which in practice means nobody builds a
  // collection. A name field, one button and a list is what "easy abspeichern und
  // später drauf zugreifen" actually asks for.
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("MEINE LOOKS")));
  lookName_ = urlField(QString::fromUtf8("Name für diesen Look …"));
  col->addWidget(lookName_);
  auto* lookSaveBtn = accentButton(QString::fromUtf8("Aktuellen Look speichern"));
  connect(lookSaveBtn, &QPushButton::clicked, this, &CharacterIoTab::saveLook);
  connect(lookName_, &QLineEdit::returnPressed, this, &CharacterIoTab::saveLook);
  col->addWidget(lookSaveBtn);

  lookList_ = new QListWidget;
  // Tall enough for three thumbnail rows; the preview is the point of the list.
  lookList_->setFixedHeight(150);
  lookList_->setIconSize(QSize(44, 44));
  connect(lookList_, &QListWidget::itemDoubleClicked, this,
          &CharacterIoTab::loadSelectedLook);
  col->addWidget(lookList_);

  {
    auto* lookRow = new QHBoxLayout;
    lookRow->setSpacing(6);
    auto* lookLoadBtn = quietButton(QString::fromUtf8("Laden"));
    connect(lookLoadBtn, &QPushButton::clicked, this, &CharacterIoTab::loadSelectedLook);
    lookRow->addWidget(lookLoadBtn);
    auto* lookDelBtn = quietButton(QString::fromUtf8("Löschen"));
    connect(lookDelBtn, &QPushButton::clicked, this, &CharacterIoTab::deleteSelectedLook);
    lookRow->addWidget(lookDelBtn);
    col->addLayout(lookRow);
  }
  auto* lookHint = new QLabel(QString::fromUtf8(
    "Gespeichert wird alles: Rasse, Gesicht, Ausrüstung samt Farbvariante. "
    "Doppelklick lädt."));
  lookHint->setFont(typo::font(typo::Body));
  lookHint->setWordWrap(true);
  col->addWidget(lookHint);
  refreshLooks();

  col->addSpacing(4);

  // --- Aus dem Spiel (MVLink)
  //
  // This replaced the Wowhead dressing-room field. That route needed the look rebuilt on
  // a website and pasted back; the addon reads what the character is actually wearing,
  // which is the same job done in one step. The decoder and the --dressing-room flag stay
  // for scripts and for the 56-case test corpus -- only the control is gone.
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("AUS DEM SPIEL — MVLINK-ADDON")));

  // The order tells the truth about freshness. The clipboard is live, so it leads; the
  // paste field is the same data by hand; the file is a snapshot from the last /reload and
  // says so on its own button. The old layout led with the file and called both routes
  // equivalent -- which handed people yesterday's look with a clear conscience.
  auto* mvTop = new QLabel(QString::fromUtf8(
    "Im Spiel /mvlink öffnen und den Code kopieren — mehr nicht. ModelViewer erkennt "
    "ihn in der Zwischenablage und zieht den Look sofort an."));
  mvTop->setFont(typo::font(typo::Body));
  mvTop->setWordWrap(true);
  col->addWidget(mvTop);

  mvlinkCode_ = urlField(QString::fromUtf8("… oder Code von Hand einfügen: MVM1:R=…"));
  col->addWidget(mvlinkCode_);
  auto* mvBtn = quietButton(QString::fromUtf8("Code übernehmen"));
  connect(mvBtn, &QPushButton::clicked, this, &CharacterIoTab::importMVLink);
  connect(mvlinkCode_, &QLineEdit::returnPressed, this, &CharacterIoTab::importMVLink);
  col->addWidget(mvBtn);
  auto* mvHint = new QLabel(QString::fromUtf8(
    "Für Codes aus zweiter Hand — etwa von jemand anderem geschickt. Gesicht und "
    "Frisur kann ein Addon nicht auslesen; die bleiben, wie sie hier eingestellt sind."));
  mvHint->setFont(typo::font(typo::Body));
  mvHint->setWordWrap(true);
  col->addWidget(mvHint);

  col->addSpacing(2);
  mvFileBtn_ = quietButton(QString::fromUtf8("Ablage lesen (Stand: letztes /reload)"));
  connect(mvFileBtn_, &QPushButton::clicked, this, &CharacterIoTab::importMVLinkFromGame);
  col->addWidget(mvFileBtn_);

  // Neither route works until the addon is actually in the game folder, and the setup
  // cannot put it there -- at install time nobody knows where WoW lives. So it ships beside
  // the exe and lands here, the same way the Blender add-on does.
  col->addSpacing(2);
  mvInstallBtn_ = quietButton(QString());
  connect(mvInstallBtn_, &QPushButton::clicked, this, &CharacterIoTab::installMVLinkAddon);
  col->addWidget(mvInstallBtn_);
  mvInstallHint_ = new QLabel;
  mvInstallHint_->setFont(typo::font(typo::Body));
  mvInstallHint_->setWordWrap(true);
  col->addWidget(mvInstallHint_);
  refreshMVLinkState();

  // --- Armory
  col->addSpacing(4);
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("ARMORY")));
  armoryUrl_ = urlField(QString::fromUtf8("https://worldofwarcraft.blizzard.com/…"));
  col->addWidget(armoryUrl_);
  auto* amBtn = quietButton(QString::fromUtf8("Armory-Charakter importieren"));
  connect(amBtn, &QPushButton::clicked, this, &CharacterIoTab::importArmory);
  connect(armoryUrl_, &QLineEdit::returnPressed, this, &CharacterIoTab::importArmory);
  col->addWidget(amBtn);

  // --- Datei
  col->addSpacing(4);
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("CHARAKTERDATEI")));
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
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("NACH BLENDER")));
  auto* blHint = new QLabel(QString::fromUtf8(
    "Schreibt FBX mit Netz, Skelett und Gewichtung. OBJ kann kein Skelett — für "
    "Blender ist FBX der Weg. Animationen wählst du im Reiter „Export“."));
  blHint->setFont(typo::font(typo::Body));
  blHint->setWordWrap(true);
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
  status_->setFont(typo::font(typo::Body));
  status_->setWordWrap(true);
  col->addWidget(status_);

  col->addStretch(1);
}

void CharacterIoTab::setStatus(const QString& text, bool error)
{
  if (!status_)
    return;
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

void CharacterIoTab::refreshMVLinkState()
{
  const bool haveWoW = menus_ && !menus_->wowInstallFolder().isEmpty();
  mvFileBtn_->setEnabled(haveWoW);
  mvFileBtn_->setToolTip(haveWoW ? QString()
                                 : QString::fromUtf8("Braucht eine WoW-Installation auf diesem "
                                                     "Rechner — die Ablage schreibt das Spiel."));
  // The ellipsis is the promise of a question: without a known folder the button asks first.
  mvInstallBtn_->setText(haveWoW ? QString::fromUtf8("MVLink-Addon in WoW installieren")
                                 : QString::fromUtf8("MVLink-Addon installieren …"));
  mvInstallHint_->setText(haveWoW
    ? QString::fromUtf8("Einmalig — danach im Spiel unter Addons aktivieren. Ein Update "
                        "überschreibt die Dateien; WoW sollte dabei geschlossen sein.")
    : QString::fromUtf8("Die Spieldaten kommen online; eine WoW-Installation ist hier nicht "
                        "bekannt. Das Addon läuft im Spiel und braucht eine — der Knopf fragt "
                        "nach dem Ordner. Ohne Installation bleibt der Code-Weg oben, etwa für "
                        "Codes, die dir jemand schickt."));
}

void CharacterIoTab::installMVLinkAddon()
{
  if (!menus_)
    return;
  // Online without a known installation: ask for one first. It is remembered for MVLink only
  // -- the game data keep coming from where they come from.
  if (menus_->wowInstallFolder().isEmpty()) {
    const QString picked = SourceDialog::askForWoWFolder(this, QString());
    if (picked.isEmpty())
      return;
    menus_->setWoWInstallFolder(picked);
    refreshMVLinkState();
  }
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

void CharacterIoTab::refreshLooks()
{
  if (!lookList_ || !menus_)
    return;
  lookList_->clear();
  for (const QString& name : menus_->savedLooks()) {
    auto* item = new QListWidgetItem(name);
    const QString thumb = menus_->lookThumbFor(name);
    if (!thumb.isEmpty()) {
      item->setIcon(QIcon(thumb));
      // Qt tooltips take rich text, which turns hovering into a real preview -- the
      // thumbnail is 96 px, big enough to recognise a look without opening it.
      item->setToolTip(QString("<img src='%1'>")
                         .arg(QUrl::fromLocalFile(QFileInfo(thumb).absoluteFilePath())
                                .toString()));
    } else {
      // Saved before previews existed. Said out loud, or the missing picture reads as
      // a bug rather than an old file.
      item->setToolTip(QString::fromUtf8("Noch ohne Vorschau — einmal laden und neu "
                                         "speichern, dann bekommt er eine."));
    }
    lookList_->addItem(item);
  }
}

void CharacterIoTab::saveLook()
{
  if (!menus_)
    return;
  const QString name = lookName_->text().trimmed();
  if (name.isEmpty()) {
    setStatus(QString::fromUtf8("Erst einen Namen eintippen, dann speichern."), true);
    return;
  }
  const QString err = menus_->saveLook(name);
  if (!err.isEmpty()) {
    setStatus(err, true);
    return;
  }
  lookName_->clear();
  refreshLooks();
  // Select what was just saved, so "Laden" right after does the expected thing.
  const auto hits = lookList_->findItems(name, Qt::MatchFixedString);
  if (!hits.isEmpty())
    lookList_->setCurrentItem(hits.first());
  setStatus(QString::fromUtf8("Look »%1« gespeichert.").arg(name), false);
}

void CharacterIoTab::loadSelectedLook()
{
  if (!menus_ || !lookList_ || !lookList_->currentItem())
    return;
  const QString name = lookList_->currentItem()->text();
  const QString err = menus_->loadLook(name);
  if (err.isEmpty())
    setStatus(QString::fromUtf8("Look »%1« geladen.").arg(name), false);
  else
    setStatus(err, true);
}

void CharacterIoTab::deleteSelectedLook()
{
  if (!menus_ || !lookList_ || !lookList_->currentItem())
    return;
  const QString name = lookList_->currentItem()->text();
  // One click less would be nicer; one look lost to a misclick would not.
  if (QMessageBox::question(this, QString::fromUtf8("Look löschen"),
        QString::fromUtf8("»%1« wirklich löschen?").arg(name))
      != QMessageBox::Yes)
    return;
  if (menus_->deleteLook(name)) {
    refreshLooks();
    setStatus(QString::fromUtf8("Look »%1« gelöscht.").arg(name), false);
  } else {
    setStatus(QString::fromUtf8("»%1« ließ sich nicht löschen.").arg(name), true);
  }
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
  setProperty("role", "panel");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(12);

  col->addWidget(uikit::sectionLabel(QString::fromUtf8("FORMAT")));

  format_ = new QComboBox;
  format_->setFont(typo::font(typo::Body));
  col->addWidget(format_);

  col->addWidget(uikit::sectionLabel(QString::fromUtf8("OPTIONEN")));

  optMesh_      = new QCheckBox(QString::fromUtf8("Geometrie"));
  optSkinning_  = new QCheckBox(QString::fromUtf8("Skinning"));
  optSkeleton_  = new QCheckBox(QString::fromUtf8("Skelett"));
  optAnimation_ = new QCheckBox(QString::fromUtf8("Animationen"));
  // Blender's default expectation: mesh with an armature and vertex weights.
  optMesh_->setChecked(true);
  optSkinning_->setChecked(true);
  optSkeleton_->setChecked(true);
  for (QCheckBox* c : { optMesh_, optSkinning_, optSkeleton_, optAnimation_ }) {
    c->setFont(typo::font(typo::Body));
    col->addWidget(c);
  }
  optSkinning_->setToolTip(QString::fromUtf8(
    "Braucht Geometrie und Skelett -- der Exporter schaltet beides bei Bedarf selbst zu."));

  // The clip list, shown only while "Animationen" is on. Multi-select, because the FBX
  // exporter takes a list of animation indices and writes one take per entry.
  clipList_ = new QListWidget;
  clipList_->setFont(typo::font(typo::Body));
  clipList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  clipList_->setFixedHeight(150);
  clipList_->setVisible(false);
  col->addWidget(clipList_);

  clipHint_ = new QLabel;
  clipHint_->setFont(typo::font(typo::Body));
  clipHint_->setWordWrap(true);
  clipHint_->setVisible(false);
  col->addWidget(clipHint_);

  connect(optAnimation_, &QCheckBox::toggled, this, [this](bool on) {
    clipList_->setVisible(on);
    clipHint_->setVisible(on);
    if (on)
      refreshClips();
  });

  auto* button = new QPushButton(QString::fromUtf8("Modell exportieren"));
  button->setFont(typo::font(typo::Strong));
  button->setCursor(Qt::PointingHandCursor);
  col->addWidget(button);

  status_ = new QLabel;
  status_->setFont(typo::font(typo::Body));
  status_->setWordWrap(true);
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
    if (!err.isEmpty())
      status_->setText(err);
    else if (!exporters_->lastReport().isEmpty())
      status_->setText(exporters_->lastReport());   // STL: the measured size and warnings
    else
      status_->setText(QString::fromUtf8("Export abgeschlossen."));
  });

  // --- 3D print ---------------------------------------------------------------
  // Its own section, not a fifth tab: --tab 0..3 indexes the four, and a "Druck" tab next
  // to an "Export" tab makes everyone guess. Its own button that sets format and options
  // itself, like "Als FBX exportieren" on the character tab. Exactly one number is asked of
  // the player: the printed height. Nozzle, walls, supports and cutting belong to the slicer.
  col->addSpacing(6);
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("FÜR DEN 3D-DRUCK")));

  auto* heightRow = new QHBoxLayout;
  heightRow->setSpacing(6);
  auto* heightLabel = new QLabel(QString::fromUtf8("Höhe"));
  heightLabel->setFont(typo::font(typo::Body));
  heightRow->addWidget(heightLabel);

  printHeight_ = new QSpinBox;
  printHeight_->setFont(typo::font(typo::Body));
  // 200 mm is the default because it is the size at which an ordinary FDM printer still
  // resolves the details and the figure still fits the bed (DRUCK-KONZEPT.md, Stufe 3).
  printHeight_->setRange(20, 600);
  printHeight_->setSingleStep(10);
  printHeight_->setValue(200);
  printHeight_->setSuffix(QString::fromUtf8(" mm"));
  heightRow->addWidget(printHeight_, 1);

  for (int preset : {100, 200, 300}) {
    auto* b = quietButton(QString::number(preset));
    b->setToolTip(QString::fromUtf8("%1 mm").arg(preset));
    connect(b, &QPushButton::clicked, this, [this, preset]() { printHeight_->setValue(preset); });
    heightRow->addWidget(b);
  }
  col->addLayout(heightRow);

  printHint_ = new QLabel;
  printHint_->setFont(typo::font(typo::Body));
  printHint_->setWordWrap(true);
  col->addWidget(printHint_);

  // The hint changes with the number: past the Z height of widespread printers the figure
  // has to be cut, and the place for that is the slicer -- so say so, and offer no cutting
  // control here.
  auto updatePrintHint = [this]() {
    const int mm = printHeight_->value();
    if (mm > 250)
      printHint_->setText(QString::fromUtf8("⚠ %1 mm passt auf die meisten Drucker nicht — "
                                            "im Slicer zerteilen (PrusaSlicer: Taste C).").arg(mm));
    else
      printHint_->setText(QString::fromUtf8("Schreibt die Figur so, wie sie hier steht: Pose, "
                                            "sichtbare Teile, ohne Effektflächen, in Millimetern "
                                            "auf der Platte. Bei einem einzelnen Teil (Waffe, Schild) "
                                            "gilt die Zahl für die längste Seite. Dünne Flächen "
                                            "verdickt erst Blender."));
  };
  connect(printHeight_, QOverload<int>::of(&QSpinBox::valueChanged), this, updatePrintHint);
  updatePrintHint();

  auto* printButton = accentButton(QString::fromUtf8("Für den Druck exportieren"));
  connect(printButton, &QPushButton::clicked, this, &ExportTab::exportForPrint);
  col->addWidget(printButton);

  printStatus_ = new QLabel;
  printStatus_->setFont(typo::font(typo::Body));
  printStatus_->setWordWrap(true);
  printStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  col->addWidget(printStatus_);

  col->addStretch(1);
}

int ExportTab::stlFormatIndex() const
{
  if (!exporters_)
    return -1;
  const auto& formats = exporters_->formats();
  for (size_t i = 0; i < formats.size(); ++i)
    if (formats[i].label.compare("stl", Qt::CaseInsensitive) == 0 ||
        formats[i].filter.contains("*.stl", Qt::CaseInsensitive))
      return (int)i;
  return -1;
}

void ExportTab::exportForPrint()
{
  if (!exporters_ || !canvas_)
    return;
  if (!canvas_->model()) {
    printStatus_->setText(QString::fromUtf8("Kein Modell geladen."));
    return;
  }

  const int stl = stlFormatIndex();
  if (stl < 0) {
    printStatus_->setText(QString::fromUtf8("Der STL-Exporter fehlt. Liegt stlexporter.dll im "
                                            "Ordner \"plugins\" neben der Anwendung?"));
    return;
  }

  // Geometry only: an STL carries no skeleton, no weights and no animation. The exporter
  // reads the pose off the model itself, so nothing else needs switching on.
  ExportController::Options o;
  o.mesh = true;
  o.skeleton = o.skinning = o.animation = false;
  o.printHeightMm = printHeight_->value();
  exporters_->setOptions(o);

  const QString err = exporters_->exportModel(canvas_->model(), stl, this);
  if (!err.isEmpty()) {
    printStatus_->setText(err);
    return;
  }
  // Empty error AND empty report is a cancelled dialog: nothing happened, say nothing.
  const QString report = exporters_->lastReport();
  if (!report.isEmpty())
    printStatus_->setText(report);
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
