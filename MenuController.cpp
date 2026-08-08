#include "MenuController.h"

#include <algorithm>
#include <map>
#include <utility>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>

#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "CharacterPanel.h"
#include "ExportController.h"
#include "GLHost.h"
#include "MainWindow.h"
#include "MVLinkCode.h"
#include "WowheadDressingRoom.h"

#include "CharDetails.h"
#include "CharInfos.h"
#include "Game.h"
#include "GameDatabase.h"
#include "GameFile.h"
#include "ImporterPlugin.h"
#include "NPCInfos.h"
#include "PluginManager.h"
#include "RaceInfos.h"
#include "TabardDetails.h"
#include "WoWItem.h"
#include "WoWModel.h"
#include "database.h"
#include "wow_enums.h"

namespace {
// Same file main() uses -- one ini for the front-end's own settings.
const char* kSettingsFile = "userSettings/qt-frontend.ini";
const char* kFolderKey    = "game/installFolder";

// The importers reach into the game database and the plugin's network response, and
// when the result renders wrong there is nothing on screen that says WHY. Log what was
// actually applied, to the same file main() traces into.
void trace(const QString& line)
{
  static bool dirReady = false;
  if (!dirReady) {
    QDir().mkpath("userSettings");
    dirReady = true;
  }
  QFile f("userSettings/qt-frontend-trace.txt");
  if (f.open(QIODevice::Append | QIODevice::Text))
    QTextStream(&f) << QDateTime::currentDateTime().toString("HH:mm:ss.zzz")
                    << "  [import] " << line << "\n";
}

const char* kCharFilter = "Charakterdateien (*.chr);;Alle Dateien (*)";

// The character-file XML the wx front-end writes has the model under
// <SavedCharacter><model><file name="..."/>. That name is all we need to get the right
// M2 up before handing the file to WoWModel::load, which parses the rest itself.
QString modelNameFromCharFile(const QString& path, QString* error)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    *error = QObject::tr("Die Datei lässt sich nicht öffnen.");
    return QString();
  }

  QXmlStreamReader reader(&f);
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.hasError())
      break;
    if (reader.isStartElement() && reader.name() == "file") {
      const QString name = reader.attributes().value("name").toString();
      if (!name.isEmpty())
        return name;
    }
  }

  // Pre-2.0 files are plain text, not XML: the first line is the model name and the
  // customization is stored as raw pre-Shadowlands indices, which no longer map onto
  // anything. Say so rather than failing as "broken file".
  *error = QObject::tr("In der Datei steht kein Modellname. Charakterdateien von vor "
                       "WMV 2.0 (Textformat) kann dieses Frontend nicht lesen.");
  return QString();
}
}

MenuController::MenuController(MainWindow* win, GLHost* host, ExportController* exporters,
                               QObject* parent)
  : QObject(parent), win_(win), host_(host), exporters_(exporters)
{
}

WoWModel* MenuController::characterModel() const
{
  // The panel only holds a model that actually resolved to a race -- which is exactly
  // the condition for the character actions to make sense.
  CharacterPanel* panel = win_ ? win_->characterPanel() : nullptr;
  return panel ? panel->model() : nullptr;
}

QMenu* MenuController::addMenu(const QString& title)
{
  return win_->menuBar()->addMenu(title);
}

QAction* MenuController::add(QMenu* menu, const QString& text, const QString& shortcut,
                             void (MenuController::*slot)())
{
  QAction* a = menu->addAction(text);
  if (!shortcut.isEmpty())
    a->setShortcut(QKeySequence(shortcut));
  connect(a, &QAction::triggered, this, slot);
  return a;
}

void MenuController::build()
{
  if (!win_ || !win_->menuBar())
    return;

  // --- Datei ---------------------------------------------------------------
  QMenu* file = addMenu(tr("&Datei"));
  add(file, tr("Modell öffnen …"), "Ctrl+O", &MenuController::openByFileDataId);
  file->addSeparator();
  needsModel_.push_back(
    add(file, tr("Screenshot speichern …"), "F12", &MenuController::takeScreenshot));
  file->addSeparator();
  add(file, tr("WoW-Installationsordner wechseln …"), QString(),
      &MenuController::changeGameFolder);
  file->addSeparator();
  QAction* quit = file->addAction(tr("Beenden"));
  quit->setShortcut(QKeySequence("Ctrl+Q"));
  connect(quit, &QAction::triggered, win_, &MainWindow::close);

  // --- Ansicht -------------------------------------------------------------
  viewMenu_ = addMenu(tr("&Ansicht"));
  QAction* reframe = viewMenu_->addAction(tr("Auf Modell einpassen"));
  reframe->setShortcut(QKeySequence("Ctrl+R"));
  connect(reframe, &QAction::triggered, this, [this]() { host_->resetCamera(); });

  const char* kPresetNames[] = { "Vorn", "Dreiviertel", "Seite", "Oben" };
  for (int i = 0; i < 4; ++i) {
    QAction* a = viewMenu_->addAction(tr(kPresetNames[i]));
    a->setShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)));
    connect(a, &QAction::triggered, this, [this, i]() { applyPreset(i); });
  }
  viewMenu_->addSeparator();
  gridAction_ = viewMenu_->addAction(tr("Gitter"));
  gridAction_->setCheckable(true);
  gridAction_->setShortcut(QKeySequence("Ctrl+G"));
  connect(gridAction_, &QAction::triggered, this, &MenuController::toggleGrid);
  add(viewMenu_, tr("Hintergrundfarbe …"), QString(), &MenuController::chooseBackground);

  // --- Charakter -----------------------------------------------------------
  // Everything here needs a character model; without one the entries are visibly
  // greyed out instead of silently doing nothing.
  characterMenu_ = addMenu(tr("&Charakter"));
  needsCharacter_.push_back(
    add(characterMenu_, tr("Charakter laden …"), "F8", &MenuController::loadCharacter));
  needsCharacter_.push_back(
    add(characterMenu_, tr("Charakter speichern …"), "F7", &MenuController::saveCharacter));
  characterMenu_->addSeparator();
  add(characterMenu_, tr("Armory-Charakter importieren …"), QString(),
      &MenuController::importArmoryCharacter);
  add(characterMenu_, tr("NPC von URL importieren …"), QString(),
      &MenuController::importNpcFromUrl);
  add(characterMenu_, tr("Wowhead-Anprobe importieren …"), QString(),
      &MenuController::importWowheadDressingRoomDialog);
  add(characterMenu_, tr("Wowhead-Look importieren …"), QString(),
      &MenuController::importWowheadLook);
  characterMenu_->addSeparator();
  needsCharacter_.push_back(
    add(characterMenu_, tr("Ausrüstung speichern …"), "F5", &MenuController::saveEquipment));
  needsCharacter_.push_back(
    add(characterMenu_, tr("Ausrüstung laden …"), "F6", &MenuController::loadEquipment));
  needsCharacter_.push_back(
    add(characterMenu_, tr("Ausrüstung leeren"), "F9", &MenuController::clearEquipment));
  characterMenu_->addSeparator();
  needsCharacter_.push_back(
    add(characterMenu_, tr("Zufällige Anpassung"), "Ctrl+Shift+R",
        &MenuController::randomiseCharacter));
  characterMenu_->addSeparator();

  // The geoset toggles CharDetails exposes as plain bools, in the order the wx menu
  // had them. Each one needs a model refresh to become visible.
  const struct { const char* label; bool CharDetails::*field; } kToggles[] = {
    { "Unterwäsche anzeigen", &CharDetails::showUnderwear },
    { "Ohren anzeigen",       &CharDetails::showEars },
    { "Haare anzeigen",       &CharDetails::showHair },
    { "Gesichtshaar anzeigen",&CharDetails::showFacialHair },
    { "Füße anzeigen",        &CharDetails::showFeet },
    // Not cosmetic bookkeeping: this is what makes a helmet hide the hair, ears and
    // horns underneath it. With it on and a full helm equipped, a character looks
    // headless -- which is correct, but indistinguishable from a broken render unless
    // you can switch it off and look.
    { "Geosets unter Kopfitem ausblenden", &CharDetails::autoHideGeosetsForHeadItems }
  };
  for (const auto& t : kToggles) {
    QAction* a = characterMenu_->addAction(tr(t.label));
    a->setCheckable(true);
    auto field = t.field;
    connect(a, &QAction::toggled, this, [this, field](bool on) {
      WoWModel* m = characterModel();
      if (!m)
        return;
      (m->cd.*field) = on;
      m->refresh();
      win_->characterPanel()->refresh();
    });
    geosetToggles_.push_back(a);
    needsCharacter_.push_back(a);
  }

  // --- Export --------------------------------------------------------------
  // One entry per loaded exporter plugin, so the menu says which formats this
  // installation actually has rather than promising three and finding none.
  exportMenu_ = addMenu(tr("&Export"));
  const std::vector<ExportController::Format> formats =
    exporters_ ? exporters_->formats() : std::vector<ExportController::Format>();
  if (formats.empty()) {
    QAction* none = exportMenu_->addAction(tr("Keine Exporter gefunden"));
    none->setEnabled(false);
  } else {
    for (int i = 0; i < (int)formats.size(); ++i) {
      QAction* a = exportMenu_->addAction(tr("%1 exportieren …").arg(formats[i].label));
      connect(a, &QAction::triggered, this, [this, i]() {
        const QString err = exporters_->exportModel(host_->model(), i, win_);
        if (!err.isEmpty())
          QMessageBox::warning(win_, tr("Export"), err);
      });
      needsModel_.push_back(a);
    }
    exportMenu_->addSeparator();
    QAction* pick = exportMenu_->addAction(tr("Format wählen …"));
    pick->setShortcut(QKeySequence("Ctrl+E"));
    connect(pick, &QAction::triggered, this, &MenuController::exportWithDialog);
    needsModel_.push_back(pick);
  }

  // --- Hilfe ---------------------------------------------------------------
  QMenu* help = addMenu(tr("&Hilfe"));
  add(help, tr("Über …"), QString(), &MenuController::about);
  add(help, tr("Kontakt & Unterstützen …"), QString(), &MenuController::contact);

  // The viewport HUD, the toolbar and the rail all end up here rather than growing
  // their own copies of these actions.
  connect(win_, &MainWindow::screenshotRequested, this, &MenuController::takeScreenshot);
  connect(win_, &MainWindow::cameraPresetRequested, this,
          [this](int i) { host_->applyCameraPreset(i); });
  connect(win_, &MainWindow::exportRequested, this, &MenuController::exportWithDialog);
  connect(win_, &MainWindow::backgroundRequested, this, &MenuController::chooseBackground);
  connect(win_, &MainWindow::gridToggleRequested, this, &MenuController::toggleGrid);
  connect(win_, &MainWindow::fitCameraRequested, this, [this]() { host_->resetCamera(); });

  // "Kamera" in the toolbar opens the Ansicht menu where the camera entries live,
  // instead of duplicating them.
  connect(win_, &MainWindow::cameraMenuRequested, this, [this]() {
    if (viewMenu_)
      viewMenu_->popup(QCursor::pos());
  });

  win_->setGridIndicator(host_->gridVisible());

  modelChanged();
}

void MenuController::modelChanged()
{
  WoWModel* character = characterModel();
  const bool haveModel = host_ && host_->model() != nullptr;

  for (QAction* a : needsModel_)
    a->setEnabled(haveModel);
  // The HUD's export button is not a QAction and needs telling separately.
  if (win_)
    win_->setModelActionsEnabled(haveModel);
  for (QAction* a : needsCharacter_)
    a->setEnabled(character != nullptr);

  // Import does not need a model -- it produces one.
  if (!character)
    return;

  // Reflect the model's actual flags rather than whatever was checked for the last one.
  const bool state[] = { character->cd.showUnderwear, character->cd.showEars,
                         character->cd.showHair, character->cd.showFacialHair,
                         character->cd.showFeet,
                         character->cd.autoHideGeosetsForHeadItems };
  const int nState = (int)(sizeof(state) / sizeof(state[0]));
  for (int i = 0; i < (int)geosetToggles_.size() && i < nState; ++i) {
    QSignalBlocker block(geosetToggles_[i]);
    geosetToggles_[i]->setChecked(state[i]);
  }
}

// --- Datei -------------------------------------------------------------------

void MenuController::openByFileDataId()
{
  bool ok = false;
  const QString entered = QInputDialog::getText(
    win_, tr("Modell öffnen"),
    tr("FileDataID oder Pfad im Spielarchiv:"), QLineEdit::Normal, QString(), &ok);
  if (!ok || entered.trimmed().isEmpty())
    return;

  const QString needle = entered.trimmed();
  bool numeric = false;
  const uint id = needle.toUInt(&numeric);

  GameFile* f = numeric ? GAMEDIRECTORY.getFile(id) : GAMEDIRECTORY.getFile(needle);
  if (!f) {
    QMessageBox::warning(win_, tr("Modell öffnen"),
                         tr("%1 ist in den geladenen Spieldaten nicht enthalten.").arg(needle));
    return;
  }

  emit loadFileRequested(f);
  modelChanged();
}

void MenuController::takeScreenshot()
{
  if (!host_->isReady()) {
    QMessageBox::warning(win_, tr("Screenshot"),
                         tr("Der OpenGL-Kontext ist nicht bereit."));
    return;
  }

  const QString base = host_->model() ? QFileInfo(host_->model()->name()).baseName()
                                      : QStringLiteral("modelviewer");
  const QString suggested = QDir(rememberedDir("dirs/screenshot")).filePath(base + ".png");

  const QString path = QFileDialog::getSaveFileName(
    win_, tr("Screenshot speichern"), suggested, tr("PNG-Bild (*.png);;Alle Dateien (*)"));
  if (path.isEmpty())
    return;

  if (!host_->saveScreenshot(path)) {
    QMessageBox::warning(win_, tr("Screenshot"),
                         tr("Das Bild konnte nicht geschrieben werden: %1").arg(path));
    return;
  }
  rememberDir("dirs/screenshot", path);
  win_->setPathLabel(tr("Screenshot gespeichert: %1").arg(QDir::toNativeSeparators(path)));
}

void MenuController::changeGameFolder()
{
  QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  const QString current = settings.value(QString::fromLatin1(kFolderKey)).toString();

  const QString picked = QFileDialog::getExistingDirectory(
    win_, tr("WoW-Installationsordner wählen"), current, QFileDialog::ShowDirsOnly);
  if (picked.isEmpty())
    return;

  // Same test main() uses before Game::init: CASCFolder reads <install>/.build.info.
  if (!QFile::exists(picked + "/.build.info")) {
    QMessageBox::warning(
      win_, tr("Keine WoW-Installation"),
      tr("In\n\n%1\n\nliegt keine .build.info. Bitte den Ordner wählen, in dem WoW "
         "installiert ist -- nicht den Data-Unterordner.")
        .arg(QDir::toNativeSeparators(picked)));
    return;
  }

  QDir().mkpath("userSettings");
  settings.setValue(QString::fromLatin1(kFolderKey), picked);
  settings.sync();

  // CASC is mounted once, before the window exists, and Game::init takes ownership of
  // the folder -- there is no second attempt in this process. Be honest about that
  // instead of pretending the switch took effect.
  QMessageBox::information(
    win_, tr("Ordner gespeichert"),
    tr("Der Ordner ist gespeichert. Er wird beim nächsten Start verwendet -- die "
       "Spieldaten werden nur einmal beim Programmstart geladen."));
}

// --- Ansicht -----------------------------------------------------------------

void MenuController::chooseBackground()
{
  const QColor picked = QColorDialog::getColor(host_->backgroundColour(), win_,
                                              tr("Hintergrundfarbe"));
  if (picked.isValid())
    host_->setBackgroundColour(picked);
}

void MenuController::applyPreset(int index)
{
  host_->applyCameraPreset(index);
  win_->setActiveCameraPreset(index);   // keep the HUD row in step
}

// --- item browser -------------------------------------------------------------

// The default display body. 917116 is the male orc the rest of the app already uses as
// its fallback model, so nothing new has to ship for this.
static const uint kMannequinFileId = 917116u;

WoWModel* MenuController::ensureMannequin()
{
  if (WoWModel* m = characterModel())
    return m;

  GameFile* f = GAMEDIRECTORY.getFile(kMannequinFileId);
  if (!f) {
    trace("mannequin: default character model not in this client");
    return nullptr;
  }
  emit loadFileRequested(f);
  return characterModel();
}

int MenuController::standaloneModelFor(int itemId, int* textureFileId, int* displayInfoId) const
{
  // Same resolution the wx front-end's LoadItem uses: item -> modified appearance ->
  // appearance -> display info -> model and texture file. An empty model file id is the
  // normal case for chest/legs/hands/... -- those are texture layers, not geometry.
  sqlResult r = GAMEDATABASE.sqlQuery(QString(
    "SELECT ModelFileData.FileDataID, TextureFileData.FileDataID, ItemDisplayInfo.ID "
    "FROM ItemDisplayInfo "
    "LEFT JOIN ModelFileData ON ItemDisplayInfo.ModelResourcesID1 = ModelFileData.ModelResourcesID "
    "LEFT JOIN TextureFileData ON ItemDisplayInfo.ModelMaterialResourcesID1 = TextureFileData.MaterialResourcesID "
    "WHERE ItemDisplayInfo.ID = (SELECT ItemDisplayInfoID FROM ItemAppearance WHERE ItemAppearance.ID = "
    "(SELECT ItemAppearanceID FROM ItemModifiedAppearance WHERE ItemID = %1))").arg(itemId));

  if (!r.valid || r.values.empty())
    return 0;

  if (textureFileId)
    *textureFileId = r.values[0][1].toInt();
  if (displayInfoId)
    *displayInfoId = r.values[0][2].toInt();
  return r.values[0][0].toInt();
}

void MenuController::showItem(int itemId, bool standalone)
{
  const ItemRecord& rec = items.getById(itemId);
  const QString label = rec.name.isEmpty() ? QString::number(itemId) : rec.name;

  if (standalone) {
    int texId = 0, displayId = 0;
    const int modelId = standaloneModelFor(itemId, &texId, &displayId);

    if (modelId > 0) {
      if (GameFile* f = GAMEDIRECTORY.getFile((uint)modelId)) {
        emit loadFileRequested(f);
        // Bind the appearance's texture the way AnimControl::SetSkin does for an item:
        // the model's own skin slot, not the character skin.
        if (WoWModel* m = host_->model()) {
          if (texId > 0)
            if (GameFile* tex = GAMEDIRECTORY.getFile((uint)texId))
              m->updateTextureList(tex, TEXTURE_OBJECT_SKIN);
        }
        modelChanged();
        win_->setPathLabel(tr("%1 (eigenes Modell)").arg(label));
        return;
      }
    }

    // Not a failure, just how the data is: most armour is painted onto the body.
    win_->setPathLabel(tr("%1 hat kein eigenes Modell — auf der Figur gezeigt").arg(label));
  }

  WoWModel* m = ensureMannequin();
  if (!m) {
    QMessageBox::warning(win_, tr("Item anzeigen"),
                         tr("Es konnte keine Figur geladen werden, auf der das Teil "
                            "dargestellt werden kann."));
    return;
  }

  win_->characterPanel()->equip(itemId);
  modelChanged();
  if (!standalone)
    win_->setPathLabel(label);
}

std::vector<int> MenuController::fetchWowheadItemIds(const QString& url, QString* error) const
{
  std::vector<int> ids;

  // Qt loads OpenSSL lazily, so a build without libssl/libcrypto next to it fails every
  // HTTPS request with a generic transport error that says nothing about the cause. Name
  // it instead -- this is a packaging problem, not something the user did wrong.
  if (url.startsWith("https", Qt::CaseInsensitive)) {
    const QString why = httpsUnavailableReason();
    if (!why.isEmpty()) {
      *error = why;
      return ids;
    }
  }

  QNetworkAccessManager net;
  QNetworkRequest req{QUrl(url)};
  // Wowhead answers a bare Qt user agent with a challenge page rather than the item list.
  req.setHeader(QNetworkRequest::UserAgentHeader,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) betterModelViewer");
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

  QScopedPointer<QNetworkReply> reply(net.get(req));

  // Synchronous on purpose: this runs from a modal action and there is nothing useful to
  // do meanwhile. The timer means a silent server never hangs the window.
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  connect(reply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timeout.start(15000);
  loop.exec();

  if (!reply->isFinished()) {
    reply->abort();
    *error = tr("Zeitüberschreitung beim Abruf von Wowhead.");
    return ids;
  }
  if (reply->error() != QNetworkReply::NoError) {
    *error = tr("Abruf fehlgeschlagen: %1").arg(reply->errorString());
    return ids;
  }

  return parseWowheadItemIds(QString::fromUtf8(reply->readAll()), error);
}

// The scraping itself, shared by the synchronous (--wowhead, headless) and the
// asynchronous (menu) fetch path so the two can never drift apart.
std::vector<int> MenuController::parseWowheadItemIds(const QString& html, QString* error)
{
  std::vector<int> ids;

  // Outfit and transmog-set pages render each piece as an anchor whose element id carries
  // the item: _item-id-118279_0. That is the whole contract -- no JSON, no API.
  //
  // A transmog-set page, though, groups the page by slot and lists EVERY appearance
  // variant in each group: "Head" alone holds a handful of different hoods. Taking all of
  // them equips 29 pieces for an eight-slot set and lets whichever comes last win. Where
  // the page marks its groups, take the first entry of each -- that is the set's own piece.
  QRegularExpression rx("_item-id-(\\d+)_");
  const QString marker = "data-inventory-type=";

  if (html.contains(marker)) {
    const QStringList groups = html.split(marker);
    for (int g = 1; g < groups.size(); ++g) {          // [0] is everything before the first group
      auto m = rx.match(groups[g]);
      if (m.hasMatch())
        ids.push_back(m.captured(1).toInt());
    }
  }

  // An outfit is one concrete set of items with no such grouping -- take them all.
  if (ids.empty()) {
    auto it = rx.globalMatch(html);
    while (it.hasNext())
      ids.push_back(it.next().captured(1).toInt());
  }

  // Older layouts only link the items; take those if the icons gave nothing.
  //
  // This stage matches /item=<id> ANYWHERE in the page, and a present-day Wowhead page
  // carries that pattern in related items, comments, guide links and the sidebar. It only
  // ever fires when both structured stages came up empty -- which is exactly what a layout
  // change looks like -- so what it collects then is a page-wide scrape, not an outfit.
  // Since the caller clears the character before applying, an unchecked result replaces a
  // real outfit with whatever the page happened to mention.
  bool wasFallback = false;
  if (ids.empty()) {
    wasFallback = true;
    QRegularExpression alt("/item=(\\d+)");
    auto it2 = alt.globalMatch(html);
    while (it2.hasNext())
      ids.push_back(it2.next().captured(1).toInt());
  }

  // Preserve order, drop repeats -- a page mentions the same piece several times.
  std::vector<int> unique;
  for (int id : ids)
    if (id > 0 && std::find(unique.begin(), unique.end(), id) == unique.end())
      unique.push_back(id);

  // A character has 14 usable slots even counting both weapons and the tabard. Anything
  // far past that from the page-wide scrape is not an outfit, and equipping it would be
  // worse than doing nothing.
  const size_t kPlausibleMax = 20;
  if (wasFallback && unique.size() > kPlausibleMax) {
    trace(QString("wowhead: fallback scrape found %1 ids -- rejected as implausible")
            .arg(unique.size()));
    *error = tr("Diese Wowhead-Seite ist anders aufgebaut als erwartet — es wurden %1 "
                "Gegenstände gefunden, was für ein Outfit zu viele sind.\n\nDer Import "
                "wurde abgebrochen, damit nichts Falsches angelegt wird.").arg(unique.size());
    return {};
  }
  if (wasFallback)
    trace(QString("wowhead: structured parse failed, fallback scrape kept %1 ids")
            .arg(unique.size()));

  if (unique.empty())
    *error = tr("Auf dieser Seite standen keine Gegenstände. Ist es wirklich ein "
                "gespeichertes Outfit oder ein Transmog-Set?");
  return unique;
}

// Empty when HTTPS works, otherwise the reason. Every network entry point asks this first:
// the clear message used to exist only on the --wowhead path, so the routes a user actually
// clicks reported Qt's generic transport error and left them guessing at a missing DLL.
QString MenuController::httpsUnavailableReason() const
{
  if (QSslSocket::supportsSsl())
    return QString();
  return tr("Diese Installation kann kein HTTPS: die OpenSSL-Bibliotheken "
            "(libssl-1_1-x64.dll und libcrypto-1_1-x64.dll) liegen nicht neben der "
            "Anwendung. Ohne sie funktionieren Wowhead-, Armory- und NPC-Import nicht.");
}

void MenuController::startWowheadFetch(const QString& url, const QString& label)
{
  if (url.startsWith("https", Qt::CaseInsensitive)) {
    const QString why = httpsUnavailableReason();
    if (!why.isEmpty()) {
      // Same shape as the reply handler's failure path below: status line plus a dialog.
      trace("wowhead fetch aborted: no SSL support");
      win_->setPathLabel(tr("Kein HTTPS verfügbar."));
      QMessageBox::warning(win_, label, why);
      return;
    }
  }

  // A second request while one is in flight aborts the first -- its finished-handler
  // sees the abort error and stops; without this, two results race for the mannequin.
  if (activeReply_)
    activeReply_->abort();

  QNetworkRequest req{QUrl(url)};
  req.setHeader(QNetworkRequest::UserAgentHeader,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) betterModelViewer");
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

  QNetworkReply* reply = net_.get(req);
  activeReply_ = reply;
  win_->setPathLabel(tr("Wowhead wird abgefragt …"));
  trace("wowhead fetch started: " + url);

  // Parented to the reply: an answer before the timeout takes the timer with it.
  auto* timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
  timeout->start(15000);

  connect(reply, &QNetworkReply::finished, this, [this, reply, label]() {
    if (activeReply_ == reply)
      activeReply_ = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      const QString why = reply->error() == QNetworkReply::OperationCanceledError
        ? tr("Zeitüberschreitung beim Abruf von Wowhead.")
        : tr("Abruf fehlgeschlagen: %1").arg(reply->errorString());
      trace("wowhead fetch FAILED: " + why);
      win_->setPathLabel(why);
      QMessageBox::warning(win_, tr("Wowhead-Import"), why);
      return;
    }

    QString error;
    const std::vector<int> ids =
      parseWowheadItemIds(QString::fromUtf8(reply->readAll()), &error);
    if (ids.empty()) {
      win_->setPathLabel(tr("Wowhead-Import fehlgeschlagen"));
      QMessageBox::warning(win_, tr("Wowhead-Import"), error);
      return;
    }
    applyItemIds(ids, label);
  });
}

void MenuController::applyItemIds(const std::vector<int>& ids, const QString& label)
{
  WoWModel* m = ensureMannequin();
  if (!m) {
    QMessageBox::warning(win_, tr("Import"),
                         tr("Es konnte keine Figur geladen werden, auf der die Teile "
                            "dargestellt werden können."));
    return;
  }

  win_->characterPanel()->clearEquipment();

  int applied = 0, skipped = 0;
  for (int id : ids) {
    if (id <= 0)
      continue;                       // 0 is how an empty slot is written in a list
    // getById returns a default record for an unknown id, whose slot resolves to -1 and
    // equipping silently does nothing -- count those instead of pretending they landed.
    // Copied because ItemRecord::slot() is not const.
    ItemRecord rec = items.getById(id);
    if (rec.slot() < 0) {
      skipped++;
      continue;
    }
    win_->characterPanel()->equip(id);
    applied++;
  }

  modelChanged();
  trace(QString("item list applied: %1 of %2").arg(applied).arg(applied + skipped));
  win_->setPathLabel(skipped > 0
    ? tr("%1 — %2 Teile angelegt, %3 nicht zuordenbar").arg(label).arg(applied).arg(skipped)
    : tr("%1 — %2 Teile angelegt").arg(label).arg(applied));
}

void MenuController::importWowheadLook()
{
  bool ok = false;
  const QString entered = QInputDialog::getMultiLineText(
    win_, tr("Wowhead-Look importieren"),
    tr("Link zu einem gespeicherten Outfit oder Transmog-Set,\n"
       "oder einfach eine Liste von Item-IDs:\n\n"
       "  https://www.wowhead.com/outfit=12345\n"
       "  https://www.wowhead.com/transmog-set=1922\n"
       "  113861,186285,0,227493,229618"),
    QString(), &ok);
  if (!ok || entered.trimmed().isEmpty())
    return;

  const QString input = entered.trimmed();

  // A dressing-room link carries the whole look behind the '#', so there is nothing to
  // fetch -- it is decoded locally instead. It also carries race, gender and the
  // customizations, so it goes to the full character import rather than the item path.
  if (wowhead_is_dressing_room_url(input)) {
    const QString err = importWowheadDressingRoom(input, true);
    if (!err.isEmpty())
      QMessageBox::warning(win_, tr("Wowhead-Anprobe"), err);
    return;
  }

  std::vector<int> ids;
  QString label;

  if (input.startsWith("http", Qt::CaseInsensitive)) {
    // Asynchronous: the old path parked the whole window in a nested event loop for
    // up to 15 s. The continuation lives in startWowheadFetch.
    startWowheadFetch(input, tr("Wowhead-Import"));
    return;
  } else {
    // A plain list: commas, spaces or newlines, and 0 for an empty slot.
    for (const QString& part : input.split(QRegularExpression("[^0-9-]+"), QString::SkipEmptyParts)) {
      bool numeric = false;
      const int id = part.toInt(&numeric);
      if (numeric)
        ids.push_back(id);
    }
    if (ids.empty()) {
      QMessageBox::warning(win_, tr("Wowhead-Import"),
                           tr("Darin stand weder ein Link noch eine Item-ID."));
      return;
    }
    label = tr("Item-Liste");
  }

  applyItemIds(ids, label);
}

void MenuController::showSet(int setId, bool keepEquipment)
{
  // Named columns, not SELECT *: the old positional read ("everything from index 2
  // on is an item id") silently breaks the moment the table gains a column.
  QString itemCols;
  for (int i = 1; i <= 17; ++i)
    itemCols += QString(", ItemID%1").arg(i);
  sqlResult r = GAMEDATABASE.sqlQuery(
    QString("SELECT Name_Lang%1 FROM ItemSet WHERE ID = %2").arg(itemCols).arg(setId));
  if (!r.valid || r.values.empty()) {
    trace(QString("set %1 not found").arg(setId));
    return;
  }

  WoWModel* m = ensureMannequin();
  if (!m) {
    QMessageBox::warning(win_, tr("Set anzeigen"),
                         tr("Es konnte keine Figur geladen werden, auf der das Set "
                            "dargestellt werden kann."));
    return;
  }

  // By default a set replaces what is worn -- otherwise leftovers from the previous
  // one stay on whichever slots this set does not fill. keepEquipment is the browser
  // checkbox for deliberately mixing sets.
  if (!keepEquipment)
    win_->characterPanel()->clearEquipment();

  const auto& row = r.values[0];
  int applied = 0;
  for (int i = 1; i < (int)row.size(); ++i) {
    const int itemId = row[i].toInt();
    if (itemId <= 0)
      continue;
    win_->characterPanel()->equip(itemId);
    applied++;
  }

  modelChanged();
  win_->setPathLabel(keepEquipment
    ? tr("Set: %1 — %2 Teile hinzugefügt").arg(row[0]).arg(applied)
    : tr("Set: %1 — %2 Teile").arg(row[0]).arg(applied));
}

void MenuController::toggleGrid()
{
  const bool on = !host_->gridVisible();
  host_->setGridVisible(on);
  win_->setGridIndicator(on);
  if (gridAction_) {
    QSignalBlocker block(gridAction_);
    gridAction_->setChecked(on);        // the rail can toggle it too
  }
}

// --- Charakter ---------------------------------------------------------------

bool MenuController::writeCharacterTo(const QString& path, bool equipmentOnly)
{
  WoWModel* m = characterModel();
  if (!m)
    return false;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  // Byte-for-byte the upstream layout: the writing is done by wow.dll, so a file saved
  // here opens in the wx build and vice versa.
  QXmlStreamWriter stream(&file);
  stream.setAutoFormatting(true);
  stream.writeStartDocument();
  stream.writeStartElement("SavedCharacter");
  stream.writeAttribute("version", "2.0");

  if (!equipmentOnly)
    m->save(stream);

  stream.writeStartElement("equipment");
  for (WoWModel::iterator it = m->begin(); it != m->end(); ++it)
    (*it)->save(stream);
  stream.writeEndElement();   // equipment

  stream.writeEndElement();   // SavedCharacter
  stream.writeEndDocument();
  file.close();
  return true;
}

void MenuController::saveCharacterFile(bool equipmentOnly)
{
  WoWModel* m = characterModel();
  if (!m)
    return;

  const QString base = QFileInfo(m->name()).baseName();
  const QString suggested = QDir(rememberedDir("dirs/character")).filePath(base + ".chr");
  const QString path = QFileDialog::getSaveFileName(
    win_, equipmentOnly ? tr("Ausrüstung speichern") : tr("Charakter speichern"),
    suggested, tr(kCharFilter));
  if (path.isEmpty())
    return;

  if (!writeCharacterTo(path, equipmentOnly)) {
    QMessageBox::warning(win_, tr("Speichern"),
                         tr("Die Datei lässt sich nicht schreiben: %1").arg(path));
    return;
  }

  rememberDir("dirs/character", path);
  win_->setPathLabel(tr("Gespeichert: %1").arg(QDir::toNativeSeparators(path)));
}

void MenuController::swapModelPreservingState(const QString& modelPath)
{
  GameFile* target = GAMEDIRECTORY.getFile(modelPath);
  if (!target) {
    trace("posture swap: target not in the archive: " + modelPath);
    return;
  }

  // Round-trip through the character format rather than copying CharDetails by hand:
  // wow.dll already knows how to write and read every field, including the ones this
  // front-end never touches, and the file is thrown away immediately.
  QDir().mkpath("userSettings");
  const QString temp = "userSettings/posture-swap.chr";
  if (!writeCharacterTo(temp, false)) {
    trace("posture swap: could not save the current state");
    return;
  }

  emit loadFileRequested(target);

  WoWModel* m = characterModel();
  if (!m) {
    trace("posture swap: the new model is not a character model");
    QFile::remove(temp);
    return;
  }

  QString fn = temp;                            // load() takes a non-const reference
  m->load(fn);
  for (WoWModel::iterator it = m->begin(); it != m->end(); ++it)
    (*it)->load(fn);

  m->refresh();
  win_->characterPanel()->refresh();
  modelChanged();
  QFile::remove(temp);
  trace("posture swap -> " + modelPath);
}

void MenuController::loadCharacterFile(bool equipmentOnly)
{
  if (equipmentOnly && !characterModel())
    return;

  const QString path = QFileDialog::getOpenFileName(
    win_, equipmentOnly ? tr("Ausrüstung laden") : tr("Charakter laden"),
    rememberedDir("dirs/character"), tr(kCharFilter));
  if (path.isEmpty())
    return;

  QString fn = path;   // WoWModel::load / WoWItem::load take a non-const reference

  if (!equipmentOnly) {
    QString error;
    const QString modelName = modelNameFromCharFile(path, &error);
    if (modelName.isEmpty()) {
      QMessageBox::warning(win_, tr("Charakter laden"), error);
      return;
    }

    GameFile* f = GAMEDIRECTORY.getFile(modelName);
    if (!f) {
      QMessageBox::warning(
        win_, tr("Charakter laden"),
        tr("Das Modell der Datei (%1) ist in den geladenen Spieldaten nicht enthalten.")
          .arg(modelName));
      return;
    }

    // The model has to exist -- and be set up as a character, which the load path does
    // -- before its customization can be applied to it.
    emit loadFileRequested(f);
  }

  WoWModel* m = characterModel();
  if (!m) {
    QMessageBox::warning(win_, tr("Charakter laden"),
                         tr("Das geladene Modell ist kein Charaktermodell."));
    return;
  }

  if (!equipmentOnly)
    m->load(fn);

  for (WoWModel::iterator it = m->begin(); it != m->end(); ++it)
    (*it)->load(fn);

  m->refresh();
  win_->characterPanel()->refresh();
  rememberDir("dirs/character", path);
  modelChanged();
  win_->setPathLabel(tr("Geladen: %1").arg(QDir::toNativeSeparators(path)));
}

void MenuController::clearEquipment()
{
  if (CharacterPanel* p = win_->characterPanel())
    p->clearEquipment();
}

void MenuController::randomiseCharacter()
{
  if (CharacterPanel* p = win_->characterPanel())
    p->randomise();
  modelChanged();
}

void MenuController::importArmoryCharacter()
{
  bool ok = false;
  const QString url = QInputDialog::getText(
    win_, tr("Armory-Import"),
    tr("Link zur Charakterseite:\n"
       "(z. B. https://worldofwarcraft.blizzard.com/de-de/character/eu/realm/name)"),
    QLineEdit::Normal, QString(), &ok);
  if (!ok || url.trimmed().isEmpty())
    return;

  const QString why = httpsUnavailableReason();
  if (!why.isEmpty()) {
    QMessageBox::warning(win_, tr("Armory-Import"), why);
    return;
  }

  const QString err = importArmory(url.trimmed(), true);
  if (!err.isEmpty())
    QMessageBox::warning(win_, tr("Armory-Import"), err);
}

QString MenuController::importArmory(const QString& rawUrl, bool interactive)
{
  // The plugin's link parser takes the character name from the last path segment and
  // does not tolerate a trailing slash -- it then reads an EMPTY name and reports
  // "Could not read the character link". Browsers and copied links routinely carry
  // that slash, so normalise it here instead of blaming the user's link.
  QString url = rawUrl.trimmed();
  while (url.endsWith('/'))
    url.chop(1);

  trace("=== armory import: " + url + (url == rawUrl.trimmed() ? "" : "  (trailing / removed)"));

  // The importers are plugins, loaded already by ExportController::loadPlugins --
  // whichever one recognises the URL does the fetching and parsing. The fetch happens
  // INSIDE the plugin DLL, synchronously; making it truly async means changing the
  // plugin API. Until then: say what is happening and show a wait cursor -- the paint
  // pass before the call is what actually gets both onto the screen.
  win_->setPathLabel(tr("Armory wird abgefragt …"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QApplication::processEvents();

  CharInfos* result = nullptr;
  for (auto it = PLUGINMANAGER.begin(); it != PLUGINMANAGER.end(); ++it) {
    const auto* plugin = dynamic_cast<ImporterPlugin*>(*it);
    if (plugin && plugin->acceptURL(url))
      result = plugin->importChar(url);
  }
  QApplication::restoreOverrideCursor();

  if (!result) {
    trace("FAILED: no importer accepted the URL / page unreachable");
    return tr("Kein Importer hat diesen Link angenommen, oder die Seite war nicht "
              "erreichbar. Liegt der Ordner \"plugins\" neben der Anwendung?");
  }

  return applyCharInfos(result, interactive, tr("Armory-Import"));
}

QString MenuController::applyCharInfos(CharInfos* result, bool interactive,
                                       const QString& label)
{
  if (!result->valid) {
    const QString msg = result->errorMessage.empty()
      ? tr("Der Link konnte nicht ausgewertet werden. Er muss auf eine Charakterseite "
           "zeigen.")
      : QString::fromStdString(result->errorMessage);
    trace("FAILED: " + msg);
    delete result;
    return msg;
  }

  // getFileIDForRaceSex takes const int& -- raceId is unsigned, so it needs a named
  // int to bind to.
  const int sex = (result->gender == "Male") ? 0 : 1;
  const int race = (int)result->raceId;
  const int raceModelFileID = RaceInfos::getFileIDForRaceSex(race, sex);
  if (raceModelFileID <= 0) {
    const int badRace = (int)result->raceId;
    delete result;
    trace(QString("FAILED: no model registered for race %1 sex %2").arg(badRace).arg(sex));
    return tr("Die Rasse dieses Charakters (ID %1) ist dem Model Viewer nicht bekannt.")
             .arg(badRace);
  }

  GameFile* f = GAMEDIRECTORY.getFile((uint)raceModelFileID);
  if (f)
    emit loadFileRequested(f);

  WoWModel* m = characterModel();
  if (!m) {
    delete result;
    trace("FAILED: model did not load / is not a character model");
    return tr("Das Modell des Charakters konnte nicht geladen werden.");
  }

  // A model that shares its M2 with another race resolves to that OTHER race on load,
  // because the by-file lookup cannot tell them apart (Mag'har vs Orc, Gilnean vs
  // Human, ...). Stamp the race we actually imported, or the character keeps the host
  // race's texture layout and none of the imported choices resolve.
  trace(QString("import race=%1 sex=%2 -> modelFileID=%3; model resolved raceID=%4 "
                "ChrModelID=%5 isChar=%6 modelType=%7")
          .arg(race).arg(sex).arg(raceModelFileID).arg(m->infos.raceID)
          .arg(m->infos.ChrModelID.empty() ? -1 : m->infos.ChrModelID[0])
          .arg(m->charModelDetails.isChar ? 1 : 0).arg((int)m->modelType));

  RaceInfos importedRace;
  if (RaceInfos::getRaceInfosForRaceSex(race, sex, importedRace) &&
      m->infos.raceID != race) {
    trace(QString("race correction: %1 -> %2").arg(m->infos.raceID).arg(race));
    m->infos = importedRace;
    m->cd.rebuildCustomizationMap(m);
    m->cd.reset(m);
    m->cd.showFeet = importedRace.barefeet;   // reset() clears it
  } else {
    trace("no race correction needed");
  }

  if (result->hasTransmogGear && interactive)
    QMessageBox::information(
      win_, tr("Transmog"),
      tr("Der Charakter trägt transmogrifizierte Ausrüstung. Angezeigt werden die "
         "Gegenstände, nach denen die Ausrüstung aussieht."));

  // The appearance API returns EVERY customization on the account's character record,
  // which today includes the character's dragonriding drakes. Those options belong to
  // the drake ChrModels; a drake "Skin Color" targets the universal base-skin layer and
  // would paint scales over the body. Keep only this model's own options.
  std::vector<std::pair<unsigned int, unsigned int>> own;
  QStringList skippedIds;
  for (const auto& c : result->customizations) {
    if (m->cd.hasOption(c.first))
      own.push_back(c);
    else
      skippedIds << QString::number(c.first);
  }
  trace(QString("customizations: %1 returned, %2 belong to this model")
          .arg(result->customizations.size()).arg(own.size()));
  trace("skipped optionIds: " + skippedIds.join(','));

  // Two passes on purpose: skin/hair COLOUR options are parent/child-linked and their
  // textures are related-gated, so a colour only resolves correctly once its partner
  // holds its final value -- and the API returns the choices in arbitrary order.
  for (const auto& c : own)
    m->cd.set(c.first, c.second);
  for (const auto& c : own)
    m->cd.set(c.first, c.second);

  // Whether a choice actually took is the interesting part: a choice id that does not
  // belong to the option is silently ignored, and the option keeps its default.
  for (const auto& c : own)
    trace(QString("  option %1: wanted choice %2, model now holds %3%4")
            .arg(c.first).arg(c.second).arg(m->cd.get(c.first))
            .arg(m->cd.get(c.first) == c.second ? "" : "   <-- NOT APPLIED"));

  m->cd.eyeGlowType = static_cast<EyeGlowTypes>(result->eyeGlowType);
  trace(QString("eyeGlowType=%1 customTabard=%2")
          .arg(result->eyeGlowType).arg(result->customTabard ? 1 : 0));

  if (result->customTabard) {
    m->td.showCustom = true;
    m->td.setIconId(result->tabardIcon);
    m->td.setIconColor(result->iconColor);
    m->td.setBorderId(result->tabardBorder);
    m->td.setBorderColor(result->borderColor);
    m->td.setBackgroundId(result->background);
    if ((int)result->equipment.size() > CS_TABARD)
      m->td.setTabardId(result->equipment[CS_TABARD]);
  }

  for (int i = 0; i < NUM_CHAR_SLOTS; ++i) {
    WoWItem* item = m->getItem((CharSlots)i);
    if (i < (int)result->equipment.size() && result->equipment[i] != 0)
      trace(QString("  slot %1: item %2 modifier %3 -> %4")
              .arg(i).arg(result->equipment[i])
              .arg(i < (int)result->itemModifierIds.size() ? result->itemModifierIds[i] : -1)
              .arg(item ? "slot exists" : "NO SLOT -- item dropped"));
    if (!item || i >= (int)result->equipment.size())
      continue;
    item->setId(result->equipment[i]);
    if (i < (int)result->itemModifierIds.size())
      item->setModifierId(result->itemModifierIds[i]);
  }

  const int applied = (int)own.size();
  const int skipped = (int)result->customizations.size() - applied;
  delete result;

  m->refresh();

  // After the composite skin and the attachments have been rebuilt: did each item
  // resolve to a real record, and is anything hidden?
  for (int i = 0; i < NUM_CHAR_SLOTS; ++i) {
    WoWItem* item = m->getItem((CharSlots)i);
    if (item && item->id() != 0)
      trace(QString("  after refresh slot %1: id=%2 name=\"%3\" quality=%4")
              .arg(i).arg(item->id()).arg(item->name()).arg(item->quality()));
  }
  trace(QString("geoset flags: underwear=%1 ears=%2 hair=%3 facialHair=%4 feet=%5 "
                "autoHideForHead=%6")
          .arg(m->cd.showUnderwear).arg(m->cd.showEars).arg(m->cd.showHair)
          .arg(m->cd.showFacialHair).arg(m->cd.showFeet)
          .arg(m->cd.autoHideGeosetsForHeadItems));

  win_->characterPanel()->refresh();
  modelChanged();
  win_->setPathLabel(tr("%1: %2 Anpassungen übernommen, %3 übersprungen")
                       .arg(label).arg(applied).arg(skipped));
  trace(QString("done: %1 applied, %2 skipped").arg(applied).arg(skipped));

  // Last, because a match replaces the model and deletes `m`. An imported character whose
  // posture is "upright" needs the variant MODEL, which cd.set() alone cannot deliver --
  // that is what made an imported upright orc come out hunched.
  std::vector<uint> choiceIds;
  choiceIds.reserve(own.size());
  for (const auto& c : own)
    choiceIds.push_back(c.second);
  win_->characterPanel()->applyPostureFor(choiceIds);

  return QString();
}

QString MenuController::importMVLinkCode(const QString& code, bool interactive)
{
  MVLinkLook look;
  QString error;
  if (!mvlink_parse_code(code, &look, &error)) {
    trace("=== mvlink FAILED: " + error);
    return error;
  }

  trace(QString("=== mvlink: v%1 race=%2 gender=%3, %4 pieces")
          .arg(look.version).arg(look.race).arg(look.gender).arg((int)look.pieces.size()));

  QScopedPointer<CharInfos> info(new CharInfos());
  info->valid = true;
  info->raceId = (unsigned int)look.race;
  info->gender = (look.gender == 0) ? "Male" : "Female";
  // What the addon read IS the appearance -- there is no "real" gear behind it to warn
  // about, the same reasoning as for a dressing-room look.
  info->hasTransmogGear = false;
  info->eyeGlowType = EGT_DEFAULT;      // class is not in the code; no death-knight glow

  info->equipment.assign(NUM_CHAR_SLOTS, 0);
  info->itemModifierIds.assign(NUM_CHAR_SLOTS, 0);

  int applied = 0;
  for (const auto& p : look.pieces) {
    if (p.slot < 0 || p.slot >= NUM_CHAR_SLOTS) {
      trace(QString("  slot %1 out of range -- skipped").arg(p.slot));
      continue;
    }
    // The slot comes from WoW's own inventory slot, so unlike Wowhead's positional
    // counting it can be trusted. The item database is still asked, purely so a
    // disagreement shows up in the log instead of as a helmet on the feet.
    ItemRecord rec = items.getById(p.itemId);   // by value: slot() is not const
    if (rec.id != 0 && rec.slot() != p.slot) {
      trace(QString("  WARNING item %1 says slot %2, code says %3 -- following the code")
              .arg(p.itemId).arg(rec.slot()).arg(p.slot));
    }
    info->equipment[p.slot] = p.itemId;
    info->itemModifierIds[p.slot] = p.modifier;
    ++applied;
    trace(QString("  slot %1: item %2 modifier %3").arg(p.slot).arg(p.itemId).arg(p.modifier));
  }

  if (applied == 0)
    return tr("Im Code stand kein Teil, das sich einem Slot zuordnen ließ.");

  // take(), not data(): applyCharInfos deletes the CharInfos on every path. Handing it
  // data() left the scoped pointer holding a freed object and deleting it a second time
  // on return -- the import reported success and the application then went down with an
  // access violation.
  const QString err = applyCharInfos(info.take(), interactive, tr("MVLink"));
  if (err.isEmpty())
    trace("mvlink import OK");
  return err;
}

QString MenuController::importMVLinkFromGame(const QString& outfitName, bool interactive)
{
  // The install folder the application already knows -- asking again would be a second
  // place to get it wrong.
  QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  const QString wow = settings.value("game/installFolder").toString();

  const std::vector<QString> paths = mvlink_saved_variable_paths(wow);
  if (paths.empty()) {
    return tr("Keine MVLink-Daten gefunden.\n\nIst das Addon installiert und war es seit "
              "dem letzten /reload einmal aktiv? Gesucht wurde unter\n%1")
             .arg(QDir::toNativeSeparators(wow + "/WTF/Account/<Account>/SavedVariables/"));
  }

  // Newest first. With several accounts the one just played is the intended one; the
  // path is traced so a wrong guess is visible rather than mysterious.
  trace("mvlink savedvariables: " + paths.front());
  QString code, error;
  if (!mvlink_read_saved_variables(paths.front(), outfitName, &code, &error))
    return error;

  return importMVLinkCode(code, interactive);
}

QString MenuController::importWowheadDressingRoom(const QString& rawUrl, bool interactive)
{
  WowheadCharacter chr;
  QString error;
  if (!wowhead_parse_dressing_room(rawUrl, &chr, &error)) {
    trace("=== wowhead dressing room FAILED: " + error);
    return error;
  }

  trace(QString("=== wowhead dressing room: v%1 race=%2 gender=%3 class=%4 level=%5, "
                "%6 customizations, %7 items")
          .arg(chr.version).arg(chr.race).arg(chr.gender).arg(chr.classId).arg(chr.level)
          .arg((int)chr.customizationChoices.size())
          .arg((int)chr.equipment.size()));

  // A look with two DIFFERENT shoulder pieces cannot be shown: WMV has one shoulder
  // slot, and quietly letting the second item through used to make it overwrite the
  // first. Say so once instead; the left piece is what gets displayed.
  int leftShoulderItem = 0;
  for (const auto& e : chr.equipment)
    if (e.wowheadSlot == 2)
      leftShoulderItem = e.itemId;
  if (chr.separateShoulders && chr.shoulder2ItemId > 0 &&
      chr.shoulder2ItemId != leftShoulderItem) {
    trace(QString("  separate shoulders: right item %1 NOT displayed (viewer has one "
                  "shoulder slot)").arg(chr.shoulder2ItemId));
    if (interactive)
      QMessageBox::information(
        win_, tr("Getrennte Schulterstücke"),
        tr("Dieser Look nutzt getrennte Schulterstücke. Angezeigt wird beidseitig das "
           "linke Teil — getrennte Schultern kann der Model Viewer noch nicht "
           "darstellen."));
  }

  // From here on this is the same shape the armory importer produces, so it can go
  // through the same apply path -- model lookup, race correction, customizations,
  // equipment, posture variant.
  QScopedPointer<CharInfos> info(new CharInfos());
  info->valid = true;
  info->raceId = (unsigned int)chr.race;
  info->gender = (chr.gender == 0) ? "Male" : "Female";
  // A dressing-room look IS the appearance -- there is no "what it really is" behind
  // it, so the transmog notice would be wrong here.
  info->hasTransmogGear = false;
  info->eyeGlowType = (chr.classId == 6) ? EGT_DEATHKNIGHT : EGT_DEFAULT;

  info->equipment.assign(NUM_CHAR_SLOTS, 0);
  info->itemModifierIds.assign(NUM_CHAR_SLOTS, 0);

  // Where each item goes is decided by the ITEM, not by Wowhead's slot counting. The
  // counting depends on marker bytes whose meaning is not fully known, and it has been
  // observed placing gloves in the tabard slot; the game database always knows that
  // 202462 is a pair of gloves. Wowhead's position is used only for an item this
  // client cannot identify -- then it is the only hint there is.
  for (const auto& entry : chr.equipment) {
    ItemRecord rec = items.getById(entry.itemId);   // copied -- slot() is not const
    int slot = rec.slot();
    const bool fromDatabase = (slot >= 0 && slot < NUM_CHAR_SLOTS);
    if (!fromDatabase)
      slot = entry.positionalSlot;

    // Two weapons both name the right hand; the second one is the off-hand. Anything
    // else already occupied means the link listed a slot twice -- last one wins, which
    // is what the dressing room shows too.
    if (slot == CS_HAND_RIGHT && info->equipment[CS_HAND_RIGHT] != 0)
      slot = CS_HAND_LEFT;

    if (slot < 0 || slot >= NUM_CHAR_SLOTS) {
      trace(QString("  item %1: no slot (wowhead said %2) -- dropped")
              .arg(entry.itemId).arg(entry.positionalSlot));
      continue;
    }

    info->equipment[slot] = entry.itemId;

    // The colour. A tier piece's Raid Finder/Heroic/Mythic tints share ONE item id;
    // the hash's bonus-list id picks the tint via its type-7 entry
    // (ItemAppearanceModifierID), which is exactly what WoWItem::setModifierId eats.
    // No bonus, or a bonus without a type-7 entry (ilvl-only bonuses), means the
    // Normal look -- modifier 0, which the slot already holds.
    if (entry.bonusId > 0) {
      sqlResult bonus = GAMEDATABASE.sqlQuery(
        QString("SELECT Value1 FROM ItemBonus WHERE ParentItemBonusListID = %1 AND Type = 7")
          .arg(entry.bonusId));
      if (bonus.valid && !bonus.values.empty() && !bonus.values[0].empty())
        info->itemModifierIds[slot] = bonus.values[0][0].toInt();
      trace(QString("  item %1: bonus %2 -> appearance modifier %3")
              .arg(entry.itemId).arg(entry.bonusId).arg(info->itemModifierIds[slot]));
    }

    trace(QString("  item %1 -> CharSlots %2 (%3; wowhead said %4)")
            .arg(entry.itemId).arg(slot)
            .arg(fromDatabase ? "from the item database" : "wowhead position, item unknown")
            .arg(entry.positionalSlot));
  }

  // Wowhead stores only the choice id; CharDetails is keyed by option. The link
  // between the two is in the game database, so an option the current client does not
  // know simply drops out here rather than being applied to the wrong option.
  int unresolved = 0;
  for (unsigned int choiceId : chr.customizationChoices) {
    sqlResult r = GAMEDATABASE.sqlQuery(
      QString("SELECT ChrCustomizationOptionID FROM ChrCustomizationChoice WHERE ID = %1")
        .arg(choiceId));
    if (!r.valid || r.values.empty() || r.values[0].empty()) {
      unresolved++;
      trace(QString("  choice %1: no option in the database").arg(choiceId));
      continue;
    }
    info->customizations.push_back(
      std::make_pair(r.values[0][0].toUInt(), choiceId));
  }
  if (unresolved > 0)
    trace(QString("  %1 of %2 choices could not be resolved to an option")
            .arg(unresolved).arg((int)chr.customizationChoices.size()));

  return applyCharInfos(info.take(), interactive, tr("Wowhead-Anprobe"));
}

void MenuController::importWowheadDressingRoomDialog()
{
  bool ok = false;
  const QString url = QInputDialog::getText(
    win_, tr("Wowhead-Anprobe importieren"),
    tr("Link aus der Wowhead-Anprobe:\n"
       "(in der Anprobe auf \"Teilen\"/die Adresszeile — er enthält ein '#')\n\n"
       "  https://www.wowhead.com/dressing-room#…"),
    QLineEdit::Normal, QString(), &ok);
  if (!ok || url.trimmed().isEmpty())
    return;

  const QString err = importWowheadDressingRoom(url.trimmed(), true);
  if (!err.isEmpty())
    QMessageBox::warning(win_, tr("Wowhead-Anprobe"), err);
}

void MenuController::importNpcFromUrl()
{
  bool ok = false;
  const QString url = QInputDialog::getText(
    win_, tr("NPC-Import"),
    tr("Wowhead-Link zum NPC:"), QLineEdit::Normal, QString(), &ok);
  if (!ok || url.trimmed().isEmpty())
    return;

  NPCInfos* result = nullptr;
  for (auto it = PLUGINMANAGER.begin(); it != PLUGINMANAGER.end(); ++it) {
    const auto* plugin = dynamic_cast<ImporterPlugin*>(*it);
    if (plugin && plugin->acceptURL(url.trimmed()))
      result = plugin->importNPC(url.trimmed());
  }

  if (!result) {
    QMessageBox::warning(
      win_, tr("NPC-Import"),
      tr("Kein Importer hat diesen Link angenommen, oder die Seite war nicht "
         "erreichbar."));
    return;
  }

  const int npcId = result->id;
  const int displayId = result->displayId;
  const int type = result->type;
  const QString name = QString::fromStdWString(result->name);
  delete result;

  if (npcId <= 0) {
    QMessageBox::warning(win_, tr("NPC-Import"),
                         tr("Der Link enthält keine NPC-ID."));
    return;
  }

  // An NPC from a newer patch than the loaded data is not in the local Creature table.
  // Registering the imported row lets the display/model lookup below resolve it, the
  // same way the old NPC browser did.
  sqlResult existing =
    GAMEDATABASE.sqlQuery(QString("SELECT ID FROM Creature WHERE ID = %1").arg(npcId));
  if ((!existing.valid || existing.empty()) && displayId > 0)
    GAMEDATABASE.sqlQuery(
      QString("INSERT INTO Creature(ID,CreatureType,DisplayID1,Name_Lang) "
              "VALUES (%1,%2,%3,\"%4\")").arg(npcId).arg(type).arg(displayId).arg(name));

  QString error;
  if (!loadNpc(npcId, &error)) {
    QMessageBox::warning(win_, tr("NPC-Import"), error);
    return;
  }

  modelChanged();
  win_->setPathLabel(name.isEmpty() ? tr("NPC %1 geladen").arg(npcId)
                                    : tr("NPC geladen: %1").arg(name));
}

bool MenuController::loadNpc(int creatureId, QString* error)
{
  sqlResult r = GAMEDATABASE.sqlQuery(
    QString("SELECT CreatureModelData.FileDataID, "
            "CreatureDisplayInfo.TextureVariationFileDataID1, "
            "CreatureDisplayInfo.TextureVariationFileDataID2, "
            "CreatureDisplayInfo.TextureVariationFileDataID3, "
            "CreatureDisplayInfo.ExtendedDisplayInfoID, CreatureDisplayInfo.ID "
            "FROM Creature "
            "LEFT JOIN CreatureDisplayInfo ON Creature.DisplayID1 = CreatureDisplayInfo.ID "
            "LEFT JOIN CreatureModelData ON CreatureDisplayInfo.modelID = CreatureModelData.ID "
            "WHERE Creature.ID = %1;").arg(creatureId));

  if (!r.valid || r.empty()) {
    *error = tr("Zu NPC %1 steht in den geladenen Spieldaten kein Modell.").arg(creatureId);
    return false;
  }

  const int modelFileId = r.values[0][0].toInt();
  const int extraId = r.values[0][4].toInt();

  // The unavailable case is common enough to name: a Wowhead page can hand back a
  // display id from a newer (or PTR) build, which the local data has no model for.
  const QString unavailable =
    tr("Dieser NPC lässt sich nicht anzeigen. Sein Modell steckt nicht in den geladenen "
       "Spieldaten -- meist heißt das, der NPC stammt aus einem neueren Patch.");

  if (extraId == 0) {
    // A plain creature: its own M2, skinned by the display info's texture variations.
    GameFile* f = GAMEDIRECTORY.getFile((uint)modelFileId);
    if (!f) {
      *error = unavailable;
      return false;
    }
    emit loadFileRequested(f);

    WoWModel* m = host_->model();
    if (!m) {
      *error = unavailable;
      return false;
    }
    m->modelType = MT_NORMAL;

    // The wx front-end goes through its skin list (AnimControl::SetSkinByDisplayID) to
    // get here; the three variation files ARE that skin, so bind them directly.
    for (int i = 0; i < 3; ++i) {
      const int texId = r.values[0][1 + i].toInt();
      if (texId > 0)
        if (GameFile* tex = GAMEDIRECTORY.getFile((uint)texId))
          m->updateTextureList(tex, TEXTURE_GAMEOBJECT1 + i);
    }
    return true;
  }

  // An "extended" NPC is a dressed-up character model: race customization plus item
  // display ids per slot.
  GameFile* f = GAMEDIRECTORY.getFile((uint)RaceInfos::getHDModelForFileID(modelFileId));
  if (!f) {
    *error = unavailable;
    return false;
  }
  emit loadFileRequested(f);

  WoWModel* m = characterModel();
  if (!m) {
    *error = tr("Das Modell dieses NPCs ist kein Charaktermodell -- Anpassung und "
                "Ausrüstung lassen sich nicht anwenden.");
    return false;
  }

  sqlResult extra = GAMEDATABASE.sqlQuery(
    QString("SELECT Skin, Face, HairStyle, HairColor, FacialHair "
            "FROM CreatureDisplayInfoExtra WHERE ID = %1").arg(extraId));
  if (extra.valid && !extra.empty()) {
    m->cd.set(CharDetails::SKIN_COLOR, extra.values[0][0].toInt());
    m->cd.set(CharDetails::FACE, extra.values[0][1].toInt());
    m->cd.set(CharDetails::FACIAL_CUSTOMIZATION_STYLE, extra.values[0][2].toInt());
    m->cd.set(CharDetails::FACIAL_CUSTOMIZATION_COLOR, extra.values[0][3].toInt());
    m->cd.set(CharDetails::ADDITIONAL_FACIAL_CUSTOMIZATION, extra.values[0][4].toInt());
  }

  sqlResult gear = GAMEDATABASE.sqlQuery(
    QString("SELECT ItemDisplayInfoID, ItemSlot FROM NpcModelItemSlotDisplayInfo "
            "WHERE NpcModelID = %1").arg(extraId));
  if (gear.valid && !gear.empty()) {
    // NpcModelItemSlotDisplayInfo.ItemSlot is its own numbering, not CharSlots.
    static const std::map<int, CharSlots> kSlotMap = {
      { 0, CS_HEAD }, { 1, CS_SHOULDER }, { 2, CS_SHIRT }, { 3, CS_CHEST },
      { 4, CS_BELT }, { 5, CS_PANTS },    { 6, CS_BOOTS }, { 7, CS_BRACERS },
      { 8, CS_GLOVES }, { 9, CS_TABARD }, { 10, CS_CAPE }
    };
    for (size_t i = 0; i < gear.values.size(); ++i) {
      const auto slot = kSlotMap.find(gear.values[i][1].toInt());
      if (slot == kSlotMap.end())
        continue;
      if (WoWItem* item = m->getItem(slot->second))
        item->setDisplayId(gear.values[i][0].toInt());
    }
  }

  m->cd.isNPC = true;
  m->refresh();
  win_->characterPanel()->refresh();
  return true;
}

// --- Export / Hilfe ----------------------------------------------------------

void MenuController::exportWithDialog()
{
  if (!exporters_ || exporters_->formats().empty()) {
    QMessageBox::warning(win_, tr("Export"),
                         tr("Keine Exporter gefunden. Liegt der Ordner \"plugins\" "
                            "neben der Anwendung?"));
    return;
  }
  if (!host_->model()) {
    QMessageBox::warning(win_, tr("Export"), tr("Kein Modell geladen."));
    return;
  }

  QStringList labels;
  for (const auto& f : exporters_->formats())
    labels << f.label;

  bool ok = false;
  const QString chosen = QInputDialog::getItem(win_, tr("Export"), tr("Format:"),
                                               labels, 0, false, &ok);
  if (!ok)
    return;

  const QString err = exporters_->exportModel(host_->model(), labels.indexOf(chosen), win_);
  if (!err.isEmpty())
    QMessageBox::warning(win_, tr("Export"), err);
}

// --- contact and support details, in ONE place -------------------------------------------
//
// Deliberately empty where the real value is not known yet: buildContactRows() skips every
// entry whose text AND url are both empty, so an unfilled field is simply absent instead of
// shipping as a placeholder somebody would mistake for a real handle. Where a row has both a
// label and a url, clear BOTH to remove it -- a label on its own would render as dead text.
namespace {

const char* kDiscordHandle = "peppawutz69";
const char* kDiscordInvite = "";                  // no server yet -- handle only
const char* kBattleTag     = "peppawutz131#2465";
const char* kRedditLabel   = "u/PeppaWutZ21";
const char* kRedditUrl     = "https://www.reddit.com/user/PeppaWutZ21/";
const char* kSupportLabel  = "Patreon";
const char* kSupportUrl    = "https://www.patreon.com/c/LouisBunt";
const char* kMusicLabel    = "ルイス・ブント";
const char* kMusicUrl      = "https://on.soundcloud.com/cF8PsOmBw2fhkbSlbq";

// One table row per filled-in entry. Handles are plain selectable text (a Discord name is
// not a link), everything with a URL becomes a clickable anchor.
QString buildContactRows()
{
  const struct { const char* label; const char* text; const char* url; } rows[] = {
    { QT_TR_NOOP("Discord"),      kDiscordHandle, kDiscordInvite },
    { QT_TR_NOOP("Battle.net"),   kBattleTag,     "" },
    { QT_TR_NOOP("Reddit"),       kRedditLabel,   kRedditUrl },
    { QT_TR_NOOP("Unterstützen"), kSupportLabel,  kSupportUrl },
    { QT_TR_NOOP("Musik"),        kMusicLabel,    kMusicUrl },
  };

  QString html;
  for (const auto& r : rows) {
    const QString text(QString::fromUtf8(r.text));
    const QString url(QString::fromUtf8(r.url));
    if (text.isEmpty() && url.isEmpty())
      continue;
    const QString shown = text.isEmpty() ? url : text;
    const QString cell = url.isEmpty()
        ? shown.toHtmlEscaped()
        : QString("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), shown.toHtmlEscaped());
    html += QString("<tr><td style='padding-right:14px'><b>%1</b></td><td>%2</td></tr>")
              .arg(QObject::tr(r.label), cell);
  }
  return html;
}

} // namespace

void MenuController::contact()
{
  const QString rows = buildContactRows();

  QMessageBox box(win_);
  box.setWindowTitle(tr("Kontakt & Unterstützen"));
  box.setTextFormat(Qt::RichText);
  // Without this the anchors render but do nothing when clicked.
  box.setTextInteractionFlags(Qt::TextBrowserInteraction);
  box.setIconPixmap(QPixmap(":/appicon.png").scaled(64, 64, Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));

  if (rows.isEmpty()) {
    box.setText(tr("<b>%1</b><br><br>Für diese Fassung sind noch keine Kontaktdaten "
                   "hinterlegt.").arg(WMV_APP_NAME));
  } else {
    box.setText(tr("<b>%1</b><br>"
                   "Entwickelt von Skogdesign.<br><br>"
                   "<table>%2</table><br>"
                   "Fehler und Wünsche gerne über GitHub:<br>"
                   "<a href=\"%3/issues\">%3/issues</a>")
                  .arg(WMV_APP_NAME, rows, QStringLiteral(WMV_PROJECT_URL)));
  }
  box.setStandardButtons(QMessageBox::Ok);
  box.exec();
}

void MenuController::about()
{
  QStringList formats;
  if (exporters_)
    for (const auto& f : exporters_->formats())
      formats << f.label;

  QMessageBox::about(
    win_, tr("Über %1").arg(WMV_APP_NAME),
    tr("<b>%1 %2</b><br>"
       "Qt-Frontend für WoW Model Viewer.<br><br>"
       "Modell-, Render- und Datenschicht (core.dll / wow.dll) stammen aus dem "
       "Ursprungsprojekt und sind bis auf wenige gezielte Eingriffe unverändert; "
       "die Oberfläche ist neu.<br><br>"
       "Exportformate: %2<br><br>"
       "Lizenz: GPLv3, geerbt von WoW Model Viewer. Hinweise zu mitgelieferten "
       "Fremdkomponenten stehen in THIRD-PARTY-NOTICES.txt neben dem Programm.<br>"
       "World of Warcraft ist eine eingetragene Marke von Blizzard Entertainment; "
       "dieses Projekt steht in keiner Verbindung zu Blizzard.")
      .arg(WMV_QT_VERSION)
      .arg(formats.isEmpty() ? tr("keine geladen") : formats.join(", ")));
}

// --- helpers -----------------------------------------------------------------

QString MenuController::rememberedDir(const char* key) const
{
  QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  return settings.value(QString::fromLatin1(key)).toString();
}

void MenuController::rememberDir(const char* key, const QString& path)
{
  QDir().mkpath("userSettings");
  QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  settings.setValue(QString::fromLatin1(key), QFileInfo(path).absolutePath());
  settings.sync();
}
