// Entry point. Links core.dll/wow.dll unchanged, initialises CASC the same way the
// wx front-end does, then hands the mounted game data to MainWindow.
//
//   WoWModelViewer-Qt.exe [<wow-install-folder>] [<fileDataId>] [options]
//
// Example:
//   WoWModelViewer-Qt.exe "C:\Program Files (x86)\World of Warcraft" 1000001
//
// Both arguments are optional: the install folder is remembered between runs, and
// the user is asked for it when it cannot be found.

#include <QApplication>
#include <QDateTime>
#include <QPalette>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QPainter>
#include <QSettings>
#include <QSplashScreen>
#include <QTextStream>
#include <QTimer>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>

#include "GLHost.h"
#include "BlenderAddonInstaller.h"
#include "CharacterPanel.h"
#include "ExportController.h"
#include "InspectorTabs.h"
#include "ItemBrowser.h"
#include "MenuController.h"
#include "TimelinePanel.h"
#include "MainWindow.h"

#include "Game.h"
#include "GameFile.h"
#include "GlobalSettings.h"
#include "logger/LogOutputFile.h"
#include "logger/Logger.h"
#include "CharTexture.h"
#include "RaceInfos.h"
#include "WoWDatabase.h"
#include "WoWFolder.h"
#include "WoWModel.h"
#include "database.h"

// Append a stage marker to a file. The window never appears while the game data
// is loading, so this is the only way to see which call blocks.
static void trace(const QString& stage)
{
  // Was a hardcoded path under the development checkout, which meant the trace
  // silently went nowhere on any other machine -- exactly where it would be useful.
  static bool dirReady = false;
  if (!dirReady) {
    QDir().mkpath("userSettings");
    dirReady = true;
  }
  QFile f("userSettings/qt-frontend-trace.txt");
  if (f.open(QIODevice::Append | QIODevice::Text)) {
    QTextStream(&f) << QDateTime::currentDateTime().toString("HH:mm:ss.zzz")
                    << "  " << stage << "\n";
  }
}

// --- locating the WoW installation ------------------------------------------
//
// The wx front-end asks through ClientChoiceDialog. This is the smaller equivalent:
// remember the folder, and ask only when we cannot find one.

// A separate file from the wx front-end's Config.ini on purpose. That one is written
// by wx's own config machinery; rewriting it through QSettings would reformat keys
// this build does not own.
static const char* kSettingsFile = "userSettings/qt-frontend.ini";
static const char* kFolderKey    = "game/installFolder";

static const char* kDefaultFolder = "C:/Program Files (x86)/World of Warcraft";

// CASCFolder appends "Data" to whatever it is given and then reads
// "<install>/.build.info". Testing for that file is the cheapest way to tell a real
// installation from a wrong folder, and it has to happen BEFORE Game::init(): that
// call takes ownership of the WoWFolder, so there is no second attempt to be had.
static bool looksLikeWoWInstall(const QString& folder)
{
  return !folder.isEmpty() && QFile::exists(folder + "/.build.info");
}

// Empty return means the user gave up.
static QString askForWoWFolder(const QString& tried)
{
  QString start = tried;
  for (;;) {
    const QString picked = QFileDialog::getExistingDirectory(
      nullptr,
      QString::fromUtf8("WoW-Installationsordner wählen"),
      start,
      QFileDialog::ShowDirsOnly);

    if (picked.isEmpty())
      return QString();          // cancelled

    if (looksLikeWoWInstall(picked))
      return picked;

    // Naming the file we looked for beats "invalid folder": it tells the user both
    // what is wrong and that the Data subfolder is not what we want.
    if (QMessageBox::warning(
          nullptr,
          QString::fromUtf8("Keine WoW-Installation"),
          QString::fromUtf8("In\n\n%1\n\nliegt keine .build.info. Bitte den Ordner "
                            "wählen, in dem WoW installiert ist -- nicht den "
                            "Data-Unterordner.").arg(QDir::toNativeSeparators(picked)),
          QMessageBox::Retry | QMessageBox::Cancel,
          QMessageBox::Retry) == QMessageBox::Cancel)
      return QString();

    start = picked;
  }
}

// The two positional arguments -- install folder, then FileDataID -- are the ones
// before the first --flag. Both are optional, so "--shot out.png" must not be read
// as a folder and a model id.
static QStringList positionalArgs(int argc, char** argv)
{
  QStringList out;
  for (int i = 1; i < argc; ++i) {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a.startsWith("--"))
      break;
    out << a;
  }
  return out;
}

static QString resolveGameFolder(int argc, char** argv)
{
  // An explicit folder on the command line is taken as given and never second-guessed
  // with a dialog: --shot and --export are used from scripts, and a modal dialog there
  // would hang the run instead of failing it.
  const QStringList positional = positionalArgs(argc, argv);
  if (!positional.isEmpty())
    return positional.first();

  QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  const QString remembered = settings.value(QString::fromLatin1(kFolderKey)).toString();
  if (looksLikeWoWInstall(remembered)) {
    trace("game folder from settings: " + remembered);
    return remembered;
  }

  QString folder = QString::fromLatin1(kDefaultFolder);
  if (!looksLikeWoWInstall(folder)) {
    trace("no installation at the default path -- asking");
    folder = askForWoWFolder(remembered.isEmpty() ? QString() : remembered);
    if (folder.isEmpty())
      return QString();          // caller exits
  }

  QDir().mkpath("userSettings");
  settings.setValue(QString::fromLatin1(kFolderKey), folder);
  settings.sync();
  trace("game folder remembered: " + folder);
  return folder;
}

// The window paints itself with stylesheets, but the standard dialogs (input, message,
// file, colour) are built by the style from the PALETTE. MainWindow's stylesheet cascades
// into any dialog parented to it and darkens the background, while the text colour stays
// whatever the platform default is -- which on Windows is black. That is how the armory
// import ended up as black text on a black field.
//
// Fusion plus an explicit dark palette fixes every dialog at once instead of patching them
// one at a time, and keeps the custom-drawn UI looking as it did.
static void applyDarkPalette(QApplication& app)
{
  app.setStyle(QStyleFactory::create("Fusion"));

  const QColor bg("#0f1216"), panel("#14181e"), text("#e8eaee"), dim("#5f6874");
  const QColor accent("#c8a15a"), onAccent("#17130a");

  QPalette p;
  p.setColor(QPalette::Window,          panel);
  p.setColor(QPalette::WindowText,      text);
  p.setColor(QPalette::Base,            bg);            // text entry backgrounds
  p.setColor(QPalette::AlternateBase,   panel);
  p.setColor(QPalette::Text,            text);          // and the text in them
  p.setColor(QPalette::Button,          QColor("#1c2229"));
  p.setColor(QPalette::ButtonText,      text);
  p.setColor(QPalette::BrightText,      QColor("#ff6b6b"));
  p.setColor(QPalette::ToolTipBase,     panel);
  p.setColor(QPalette::ToolTipText,     text);
  p.setColor(QPalette::Highlight,       accent);
  p.setColor(QPalette::HighlightedText, onAccent);
  p.setColor(QPalette::Link,            accent);
  p.setColor(QPalette::PlaceholderText, dim);

  p.setColor(QPalette::Disabled, QPalette::Text,       dim);
  p.setColor(QPalette::Disabled, QPalette::WindowText, dim);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, dim);

  app.setPalette(p);
}

// The first start builds the database from DB2 (~20 s) and even a warm start reads a
// ~148 MB listfile -- all of it before the window exists. Without this the user
// launches the app and nothing at all appears. A splash with stage messages is the
// honest fix here; threading is not: GAMEDIRECTORY/GAMEDATABASE are unlocked globals
// and the GL context belongs to the UI thread.
static QSplashScreen* makeSplash()
{
  // The branded poster, compiled in via the Qt resource system so the exe stays
  // self-contained. The drawn fallback only exists so a build without resources
  // still shows SOMETHING rather than nothing.
  QPixmap pm(":/splash.png");
  if (pm.isNull()) {
    pm = QPixmap(420, 160);
    pm.fill(QColor("#14181e"));
    QPainter p(&pm);
    p.setPen(QColor("#23282f"));
    p.drawRect(0, 0, pm.width() - 1, pm.height() - 1);
    p.setPen(QColor("#c8a15a"));
    QFont f("Segoe UI", 14, QFont::DemiBold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(f);
    p.drawText(QRect(0, 40, pm.width(), 30), Qt::AlignCenter, "MODEL VIEWER");
    p.end();
  }
  return new QSplashScreen(pm);
}

static void splashStage(QSplashScreen* splash, QApplication& app, const QString& text)
{
  if (!splash)
    return;
  splash->showMessage(text, Qt::AlignBottom | Qt::AlignHCenter, QColor("#b6bdc8"));
  app.processEvents();
}

// True when the run is driven by a script (--shot / --export and friends). A modal dialog
// there would block forever instead of failing, so those runs report to the trace and exit.
static bool g_scripted = false;

// Startup failures the user has to be TOLD about. They all mean the same thing in practice
// -- the folder is not a usable WoW installation -- and picking the wrong folder is the
// single most likely thing to go wrong on a first run.
//
// This used to write into a QLabel that was never parented, laid out or shown, so the app
// put up an empty window for three seconds and quit without a word. A message box costs one
// dialog and turns a dead end into an instruction.
static int fatalStart(QSplashScreen* splash, QWidget* win, const QString& what,
                      const QString& detail)
{
  if (splash)
    splash->close();
  trace("FATAL: " + what + (detail.isEmpty() ? QString() : " -- " + detail));

  if (g_scripted) {
    if (win)
      win->close();
    return 1;
  }

  QMessageBox box(QMessageBox::Critical, QString(WMV_APP_NAME), what, QMessageBox::Ok);
  box.setInformativeText(
      // NOT "_retail_": that folder holds no .build.info, which is exactly the file
      // looksLikeWoWInstall() tests for -- naming it here would have sent the user
      // straight back into the rejection they just got.
      QObject::tr("Erwartet wird der Ordner der WoW-Installation, in dem die Datei "
                  ".build.info liegt — das ist \"World of Warcraft\" selbst, nicht der "
                  "Unterordner \"_retail_\" und nicht \"Data\".\n\n"
                  "Der zuletzt gewählte Ordner ist in userSettings\\qt-frontend.ini "
                  "gespeichert; nach dem Löschen dieser Zeile fragt das Programm beim "
                  "nächsten Start erneut."));
  if (!detail.isEmpty())
    box.setDetailedText(detail);
  box.exec();

  if (win)
    win->close();
  return 1;
}

int main(int argc, char** argv)
{
  trace(QString("main entered -- %1 %2").arg(WMV_APP_NAME).arg(WMV_QT_VERSION));
  QApplication app(argc, argv);

  // Every data path -- listfile.csv, games/wow/<ver>/database.xml, ./plugins, wowdb.sqlite,
  // userSettings -- is opened RELATIVE. Started from the Start menu that is the install
  // folder and all is well, but the documented command line ("WoWModelViewer-Qt.exe <folder>
  // <id>" from a shell) left the working directory wherever the user happened to be: no file
  // tree, no item database, no exporters, and a fresh ~80 MB cache written into that folder.
  // Pinning it here fixes all of them at once.
  //
  // Consequence for scripted runs: a RELATIVE --shot/--export path now lands next to the
  // executable, not in the caller's directory. Pass absolute paths there.
  QDir::setCurrent(QCoreApplication::applicationDirPath());

  // Nothing in the Qt front-end ever registered a log sink, so every LOG_INFO/LOG_ERROR from
  // core.dll, wow.dll and the exporter plugins went nowhere -- while dialogs told the user to
  // "see the log for details". Registered before anything else so early failures are caught.
  QDir().mkpath("userSettings");
  LOGGER.addChild(new WMVLog::LogOutputFile("userSettings/log.txt"));

  // The armory importer talks to a proxy that queries Blizzard's API server-side. Its
  // address is compiled into the plugin, and the runtime override existed only in the wx
  // front-end -- so when that proxy was down, a user here had no way to point anywhere
  // else and the plugin's own error text referred to a settings dialog that does not
  // exist. Same key and same file the wx build uses, so the two agree.
  {
    QSettings settings(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
    const QString proxy = settings.value("Armory/ProxyURL", "").toString();
    if (!proxy.isEmpty()) {
      GLOBALSETTINGS.setArmoryProxyURL(proxy.toStdString());
      trace("armory proxy from settings: " + proxy);
    }
  }

  applyDarkPalette(app);
  // Window/taskbar icon for every top-level widget; the exe's Explorer icon comes
  // from resources/appicon.rc.
  app.setWindowIcon(QIcon(":/appicon.png"));
  trace("QApplication constructed");

  // Scripted runs must never stop on a modal dialog; decided here, before anything can fail.
  for (int i = 1; i < argc; ++i) {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == "--shot" || a == "--export" || a == "--install-blender-addon") {
      g_scripted = true;
      break;
    }
  }

  QSplashScreen* splash = makeSplash();
  splash->show();
  splashStage(splash, app, QString::fromUtf8("Spieldaten werden geöffnet …"));

  const QString dataFolder = resolveGameFolder(argc, argv);
  if (dataFolder.isEmpty()) {
    trace("no game folder chosen -- exiting");
    return 0;
  }
  const QStringList positional = positionalArgs(argc, argv);
  // Nothing is loaded unless an id was asked for. Starting on a fixed model meant every
  // launch began by throwing away someone else's orc: the first act was always to find
  // the browser and replace it. An empty viewport with a pointer to the tree is the
  // honest starting point.
  //
  // This is NOT the same id as MenuController's kMannequinFileId. That one is the figure
  // an item gets put on when the browser is used without a character loaded, and it stays
  // -- a piece of armour needs someone to wear it.
  const bool modelRequested = positional.size() > 1;
  const uint fileId = modelRequested ? positional.at(1).toUInt() : 0u;

  auto* win = new MainWindow;
  GLHost* host = win->canvas();

  // --- game init: identical to what modelviewer.cpp does, minus the wx wrapping
  // CASCFolder builds the build-info path as "<folder>\..\.build.info", so the
  // folder handed to WoWFolder has to be the client's Data directory, not the
  // install root. Passing the root silently yields zero configs.
  QString cascFolder = QDir::fromNativeSeparators(dataFolder);
  while (cascFolder.endsWith('/'))
    cascFolder.chop(1);
  if (!cascFolder.endsWith("/Data", Qt::CaseInsensitive))
    cascFolder += "/Data";
  trace("CASC folder = " + cascFolder);

  trace("before WoWFolder construction");
  auto* folder = new wow::WoWFolder(QDir::toNativeSeparators(cascFolder));
  trace("WoWFolder constructed; before Game::init");
  core::Game::instance().init(folder, new wow::WoWDatabase());
  trace("Game::init returned");
  if (!core::Game::instance().initDone()) {
    return fatalStart(splash, win,
                      QObject::tr("Die Spieldaten konnten nicht geöffnet werden."),
                      cascFolder);
  }
  // Game::init() only wires the objects together -- initDone() merely checks two
  // pointers. Nothing is opened until a config is chosen and handed to setConfig(),
  // which is what actually mounts the CASC storage. Skipping this was why every
  // getFile() came back null.
  trace("before configsFound");
  std::vector<core::GameConfig> configs = GAMEDIRECTORY.configsFound();
  trace(QString("configsFound returned %1 entries").arg(configs.size()));
  if (configs.empty()) {
    return fatalStart(splash, win,
                      QObject::tr("In diesem Ordner wurde keine WoW-Installation gefunden."),
                      cascFolder);
  }

  // Same rule the wx front-end uses: prefer the retail "wow" product, newest build.
  auto isNewer = [](const QString& a, const QString& b) {
    const QStringList va = a.split('.'), vb = b.split('.');
    const int n = qMax(va.size(), vb.size());
    for (int i = 0; i < n; ++i) {
      const long long na = i < va.size() ? va[i].toLongLong() : 0;
      const long long nb = i < vb.size() ? vb[i].toLongLong() : 0;
      if (na != nb) return na > nb;
    }
    return false;
  };
  size_t best = 0;
  for (size_t i = 1; i < configs.size(); ++i) {
    const bool bestRetail = configs[best].product == "wow";
    const bool iRetail    = configs[i].product == "wow";
    if (iRetail != bestRetail) { if (iRetail) best = i; }
    else if (isNewer(configs[i].version, configs[best].version)) best = i;
  }
  const core::GameConfig config = configs[best];
  trace(QString("chosen config: %1 / %2 / %3")
          .arg(config.locale).arg(config.product).arg(config.version));

  splashStage(splash, app, QString::fromUtf8("Spielarchiv wird eingebunden …"));
  trace("before setConfig (mounts CASC)");
  if (!GAMEDIRECTORY.setConfig(config)) {
    return fatalStart(splash, win,
                      QObject::tr("Das Spielarchiv konnte nicht eingebunden werden. "
                                  "Läuft WoW oder der Battle.net-Updater gerade?"),
                      QString("setConfig error %1\n%2").arg(GAMEDIRECTORY.lastError()).arg(cascFolder));
  }
  trace("setConfig returned OK");

  // configFolder must match the client's major version; the listfile path below is
  // resolved relative to it.
  //
  // Only some major versions ship data definitions, so an exact match cannot be assumed:
  // a client we have no folder for (11.x, or any Classic product) used to point the whole
  // database at a directory that does not exist. Everything then "worked" -- empty item
  // list, no race data, untextured characters, imports reporting zero pieces -- with the
  // reason sitting in a trace file nobody reads. Fall back to the newest bundled definition
  // at or below the client's version, and say so when it is not an exact match.
  const int clientMajor = config.version.section('.', 0, 0).toInt();
  QString cfgFolder = QString("games/wow/%1.0/").arg(clientMajor);
  if (!QFile::exists(cfgFolder + "database.xml")) {
    int best = -1;
    const QFileInfoList dirs = QDir("games/wow").entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& d : dirs) {
      const int major = d.fileName().section('.', 0, 0).toInt();
      if (major <= clientMajor && major > best &&
          QFile::exists(d.absoluteFilePath() + "/database.xml"))
        best = major;
    }
    if (best < 0) {
      return fatalStart(splash, win,
                        QObject::tr("Für die WoW-Version %1 liegen dieser Installation keine "
                                    "Datendefinitionen bei.").arg(config.version),
                        QString("kein games/wow/<=%1.0/database.xml gefunden in %2")
                          .arg(clientMajor).arg(QDir::currentPath()));
    }
    trace(QString("no data for client major %1 -- falling back to %2.0").arg(clientMajor).arg(best));
    cfgFolder = QString("games/wow/%1.0/").arg(best);
  }
  core::Game::instance().setConfigFolder(cfgFolder);
  trace("configFolder = " + cfgFolder);

  splashStage(splash, app, QString::fromUtf8("Dateiliste wird geladen … (~148 MB)"));
  trace("before initFromListfile");
  GAMEDIRECTORY.initFromListfile("../../../listfile.csv");
  trace("initFromListfile returned");

  // The database, texture regions and race registry. Without these WoWModel cannot
  // resolve a model file to a race, infos.raceID stays -1, and character models draw
  // as a handful of untextured geosets -- which is exactly what Phase 1 produced.
  // The expensive stage on a first start or schema bump; a warm start only opens the
  // cached sqlite. One text covers both honestly.
  splashStage(splash, app, QString::fromUtf8("Datenbank wird geladen — beim ersten Start dauert das eine Weile …"));
  trace("before GAMEDATABASE.initFromXML");
  // Without the database every character draws as untextured geosets and every equip is a
  // silent no-op -- an application that LOOKS like it started but can do nothing it promises.
  // Stopping here beats letting the user discover that one broken feature at a time.
  if (!GAMEDATABASE.initFromXML("database.xml")) {
    return fatalStart(splash, win,
                      QObject::tr("Die Datendefinitionen konnten nicht geladen werden."),
                      cfgFolder + "database.xml (" + QDir::currentPath() + ")");
  }
  trace("database.xml loaded");

  CharTexture::initRegions();
  RaceInfos::init();
  trace("RaceInfos::init done");

  splashStage(splash, app, QString::fromUtf8("Gegenstände werden gelesen …"));
  // The item table. items.getById() is what maps an id to its slot, so without this
  // every equip attempt resolves to slot -1 and silently does nothing.
  {
    // OverallQualityID is the seventh column on purpose: ItemRecord reads it from index 6
    // and falls back to 0 for queries that omit it. Without it every item was "poor" and
    // the equipment list rendered in a single colour.
    sqlResult r = GAMEDATABASE.sqlQuery(
      "SELECT Item.ID, ItemSparse.Display_Lang, Item.InventoryType, Item.ClassID, "
      "Item.SubclassID, Item.SheathType, ItemSparse.OverallQualityID FROM Item "
      "LEFT JOIN ItemSparse ON Item.ID = ItemSparse.ID "
      "WHERE Item.InventoryType != 0 AND ItemSparse.Display_Lang != \"\"");
    if (r.valid && !r.empty()) {
      for (int i = 0, imax = r.values.size(); i < imax; i++)
        items.items.push_back(ItemRecord(r.values[i]));
      trace(QString("item database: %1 entries").arg(items.items.size()));
    } else {
      trace("WARNING: item query returned nothing -- equipping will not work");
    }
  }

  // Only look anything up when an id was actually given -- the normal start has none.
  GameFile* file = nullptr;
  if (modelRequested) {
    trace("before getFile");
    file = GAMEDIRECTORY.getFile(fileId);
    trace("getFile returned");
  }
  if (!file && modelRequested) {
    // Not a fatal folder problem: the client is fine, only this one id is not in it.
    // Say so and carry on with an empty viewport -- the browser is right there.
    trace(QString("requested FileDataID %1 not in this client").arg(fileId));
    if (!g_scripted) {
      splash->close();
      QMessageBox::warning(nullptr, QString(WMV_APP_NAME),
                           QObject::tr("Das Modell mit der ID %1 gibt es in dieser "
                                       "WoW-Version nicht.\n\nDer Modellbrowser links "
                                       "zeigt, was vorhanden ist.").arg(fileId));
    }
  }
  if (!file)
    trace("starting with an empty viewport");

  // The model must be built only AFTER the GL context exists: WoWModel uploads its
  // textures during construction, and without a current context those uploads fail
  // silently -- geometry survives (CPU side) but everything renders untextured.
  // GLHost creates the context in showEvent, so show first, pump the event queue,
  // then load.
  splashStage(splash, app, file ? QString::fromUtf8("Modell wird geladen …")
                                : QString::fromUtf8("Fertig"));
  trace("before show (context comes up here)");
  win->show();
  splash->finish(win);
  splash->deleteLater();
  app.processEvents();
  trace(QString("context ready = %1").arg(host->isReady() ? "yes" : "NO"));
  if (!host->isReady())
    trace("GL init error: " + host->lastError());

  trace("before WoWModel construction");
  WoWModel* model = file ? new WoWModel(file, true) : nullptr;
  trace(model ? "WoWModel constructed" : "starting without a model");
  if (model)
    host->setModel(model);

  for (int i = 1; i < argc - 1; ++i) {
    const QString arg(argv[i]);
    if (arg == "--shot")
      host->grabAfter(90, QString::fromLocal8Bit(argv[i + 1]));  // ~1.5 s of frames
    else if (arg == "--view") {                                   // "<yaw>,<pitch>"
      const QStringList yp = QString::fromLocal8Bit(argv[i + 1]).split(',');
      if (yp.size() == 2) {
        host->setView(yp[0].toFloat(), yp[1].toFloat());
        trace(QString("view set to yaw=%1 pitch=%2").arg(yp[0]).arg(yp[1]));
      }
    }
  }
  win->setBuildLabel(QString("CASC · %1").arg(config.version));
  win->setPathLabel(file ? file->fullname()
                         : QString::fromUtf8("Kein Modell geladen — links im Baum eines wählen"));

  // Fill the browser and make picking a row load that model.
  trace("before populateTree");
  win->populateTree();
  trace("populateTree returned");

  // --grid turns the reference grid on, so that draw path is verifiable from a shot.
  for (int i = 1; i < argc; ++i)
    if (QString(argv[i]) == "--grid") {
      host->setGridVisible(true);
      win->setGridIndicator(true);
      trace("grid on");
    }

  // --category <0..3> selects a browser tab headlessly (0 all, 1 characters,
  // 2 creatures, 3 items).
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) == "--category") {
      win->setCategory(QString::fromLocal8Bit(argv[i + 1]).toInt());
      trace(QString("category set to %1").arg(argv[i + 1]));
    }
  }

  // --install-blender-addon copies the bundled add-on into every Blender profile, the
  // same as the button in the character tab -- scriptable, so the install path can be
  // verified against a scratch APPDATA without clicking.
  for (int i = 1; i < argc; ++i) {
    if (QString(argv[i]) == "--install-blender-addon") {
      const auto r = BlenderAddonInstaller::install();
      trace(r.error.isEmpty()
              ? QString("blender addon installed into %1 version(s)").arg(r.installedVersions)
              : QString("blender addon install FAILED: %1").arg(r.error));
      // Documented as "install and quit", and it never did the quitting: the window came
      // up and the process sat there until something killed it, which for a script means
      // a timeout instead of a result. The exit code now carries the outcome.
      return r.error.isEmpty() ? 0 : 1;
    }
  }

  // --tab <0..3> opens an inspector tab (0 Anpassen, 1 Charakter, 2 Licht, 3 Export).
  // Same purpose as --category: a tab that can only be reached by clicking cannot be
  // checked from a script. Applied late, so it wins over the default tab.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) == "--tab") {
      const int tab = QString::fromLocal8Bit(argv[i + 1]).toInt();
      QTimer::singleShot(0, win, [win, tab]() { win->setInspectorTab(tab); });
      trace(QString("inspector tab set to %1").arg(tab));
    }
  }

  QObject::connect(win, &MainWindow::fileIdActivated, [win](int id) {
    if (GameFile* f = GAMEDIRECTORY.getFile((uint)id))
      emit win->fileActivated(f);
  });

  // The one place a model becomes THE model. Everything that wants to show something
  // -- the browser tree, the menu's character/NPC/armory imports -- routes through
  // here, so the inspector panels can never be left pointing at the previous model.
  auto showModel = [win, host](GameFile* picked) {
    if (!picked)
      return;
    auto* m = new WoWModel(picked, true);

    // ModelViewer::LoadModel stamps both of these on a character model and neither was
    // being carried over. charModelDetails.isChar is the one that matters at render
    // time: WoWModel::calcBones takes a DIFFERENT path for characters (it animates the
    // root/key bones first, then lets the rest inherit), so with it left false a
    // character's mesh is skinned by the generic path -- which is what made an
    // equipped character look contorted, with the body collapsing while the attached
    // armour pieces stayed at their bone positions.
    //
    // raceID != -1 is the criterion, not upstream's "does the path start with char":
    // it is the same test the character panel and the item slots already use, so the
    // three cannot drift apart.
    const bool isCharacter = (m->infos.raceID != -1);
    m->modelType = isCharacter ? MT_CHAR : MT_NORMAL;
    m->charModelDetails.isChar = isCharacter;

    host->setModel(m);
    win->setPathLabel(picked->fullname());
    // Character models need their CharDetails set up before they render complete;
    // creatures and props resolve raceID == -1 and are left alone.
    win->characterPanel()->setModel(isCharacter ? m : nullptr);
    win->timeline()->setModel(m);
    // Geosets are not character-only, so they get every model -- including the
    // creature that just made characterPanel()->setModel() a null above.
    win->characterPanel()->setGeosetModel(m);
  };
  QObject::connect(win, &MainWindow::fileActivated, win, showModel);

  if (win->characterPanel())
    QObject::connect(win->characterPanel(), &CharacterPanel::itemFocusChanged,
                     host, [host](int) { host->frameVisible(); });

  if (win->characterPanel()) {
    win->characterPanel()->setModel(model && model->infos.raceID != -1 ? model : nullptr);
    // The model loaded at startup went through this path, not showModel(), so it never
    // reached the geoset list -- the section came up empty until the user opened a
    // second model. Same omission the old Material tab had.
    win->characterPanel()->setGeosetModel(model);
  }
  if (win->timeline())
    win->timeline()->setModel(model);

  // Exporters. The plugin API in core is already Qt, so this is just loading the
  // directory and hooking the button up.
  auto* exporters = new ExportController(win);
  const int nExporters = exporters->loadPlugins();
  trace(QString("exporters loaded: %1").arg(nExporters));

  // The Export tab lives in the inspector; main owns the controller, so it is
  // installed here rather than in MainWindow's constructor.
  auto* exportTab = new ExportTab(exporters, host);
  exportTab->refreshFormats();
  win->exportHost()->layout()->addWidget(exportTab);

  // The status bar used to claim "FBX · OBJ · glTF" regardless of what loaded, and there
  // is no glTF exporter at all. Say what is actually there.
  QStringList formatLabels;
  for (const auto& f : exporters->formats()) {
    formatLabels << f.label;
    trace("  exporter: " + f.label);
  }
  win->setExportFormats(formatLabels);

  // The menus in the title bar. Built here rather than in MainWindow because nearly
  // every entry needs something that only exists at this point: the mounted game data,
  // the item database, the loaded plugins. It also takes over the viewport's
  // "Exportieren" and "Screenshot" buttons, so each action has exactly one
  // implementation.
  auto* menus = new MenuController(win, host, exporters, win);
  QObject::connect(menus, &MenuController::loadFileRequested, menus, showModel);
  menus->build();

  // The character tab delegates every button to the menu controller, so it can only be
  // built once that exists.
  win->characterIoHost()->layout()->addWidget(new CharacterIoTab(menus, exporters, host));
  // Connected after showModel, so by the time the menu re-reads the state the panels
  // already hold the new model.
  QObject::connect(win, &MainWindow::fileActivated, menus,
                   [menus](GameFile*) { menus->modelChanged(); });

  // The orc's upright posture is a different MODEL, not a different setting -- the panel
  // detects that and names the file, the menu controller performs the swap and carries
  // the customization and equipment across.
  QObject::connect(win->characterPanel(), &CharacterPanel::postureModelRequested,
                   menus, &MenuController::swapModelPreservingState);
  menus->modelChanged();

  // The item browser queries the item database directly, so it can only be filled once
  // that database exists.
  QObject::connect(win->itemBrowser(), &ItemBrowser::itemActivated,
                   menus, &MenuController::showItem);
  QObject::connect(win->itemBrowser(), &ItemBrowser::setActivated,
                   menus, &MenuController::showSet);
  win->itemBrowser()->initialise();
  trace("menus and item browser built");

  // --armory <url> runs the armory import without the dialog, so it is verifiable
  // headlessly (combined with --shot) instead of only by clicking through the menu.
  // Runs before app.exec(), so a --shot grab afterwards catches the imported character.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--armory")
      continue;
    const QString url = QString::fromLocal8Bit(argv[i + 1]);
    const QString err = menus->importArmory(url, false);
    trace(err.isEmpty() ? QString("armory import OK: %1").arg(url)
                        : QString("armory import FAILED: %1").arg(err));
  }

  // --dressing-room <url> does the same for a Wowhead anprobe link. Quote it on the
  // command line: the '#' that carries the whole look also starts a comment in most
  // shells, and an unquoted link arrives here truncated to "…/dressing-room".
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--dressing-room")
      continue;
    const QString url = QString::fromLocal8Bit(argv[i + 1]);
    const QString err = menus->importWowheadDressingRoom(url, false);
    trace(err.isEmpty() ? QString("dressing room import OK: %1").arg(url)
                        : QString("dressing room import FAILED: %1").arg(err));
  }

  // --clips <id>[,<id>...] selects animation clips for the export below, by the same
  // model animation index the timeline and the Export tab use. Parsed before --export so
  // the animated path is provable headlessly too, not only by clicking the checkbox.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--clips")
      continue;
    ExportController::Options o = exporters->options();
    o.clips.clear();
    for (const QString& s : QString::fromLocal8Bit(argv[i + 1]).split(',')) {
      bool ok = false;
      const int id = s.trimmed().toInt(&ok);
      if (ok)
        o.clips.push_back(id);
    }
    o.animation = !o.clips.empty();
    exporters->setOptions(o);
    trace(QString("export clips: %1").arg((int)o.clips.size()));
  }


  // Keep the scrubber and frame counter in step with playback. The canvas advances
  // the animation itself; this only reads it back.
  auto* uiTick = new QTimer(win);
  QObject::connect(uiTick, &QTimer::timeout, [win]() {
    win->timeline()->tick();
    win->updateStats();     // the measured frame rate in the viewport overlay
  });
  uiTick->start(60);

  // --customize <optionId>,<choiceId> applies one customization through the same path
  // the panel's pickers use, including the conditional-model check. That is what makes
  // the orc posture verifiable without clicking (and without the armory, which needs a
  // network round trip through a flaky proxy). Like --anim, this has to run after the
  // panel has been given the model.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--customize")
      continue;
    const QStringList a = QString::fromLocal8Bit(argv[i + 1]).split(',');
    if (a.size() != 2)
      continue;
    WoWModel* cm = win->characterPanel() ? win->characterPanel()->model() : nullptr;
    if (!cm) {
      trace("--customize: no character model loaded");
      continue;
    }
    cm->cd.set(a[0].toUInt(), a[1].toUInt());
    cm->refresh();
    trace(QString("customize option %1 -> choice %2").arg(a[0]).arg(a[1]));
    // Last: on a conditional-model match this replaces the model and frees cm.
    win->characterPanel()->checkPostureVariant(a[1].toUInt());
  }

  // --item / --item-solo / --set drive the item browser's three actions without the list,
  // so the mannequin path, the standalone-model path and set loading are all verifiable
  // from a shot.
  for (int i = 1; i < argc - 1; ++i) {
    const QString a(argv[i]);
    const int id = QString::fromLocal8Bit(argv[i + 1]).toInt();
    if (a == "--item")
      menus->showItem(id, false);
    else if (a == "--item-solo")
      menus->showItem(id, true);
    else if (a == "--set")
      menus->showSet(id, false);
  }

  // --wowhead <url|liste> runs the Wowhead look import without the dialog: either an
  // outfit/transmog-set address to fetch, or a plain list of item ids.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--wowhead")
      continue;
    const QString arg = QString::fromLocal8Bit(argv[i + 1]);
    std::vector<int> ids;
    if (arg.startsWith("http", Qt::CaseInsensitive)) {
      QString err;
      ids = menus->fetchWowheadItemIds(arg, &err);
      trace(ids.empty() ? QString("wowhead fetch FAILED: %1").arg(err)
                        : QString("wowhead fetch: %1 items").arg((int)ids.size()));
    } else {
      for (const QString& p : arg.split(QRegularExpression("[^0-9-]+"), QString::SkipEmptyParts))
        ids.push_back(p.toInt());
      trace(QString("wowhead list: %1 entries").arg((int)ids.size()));
    }
    if (!ids.empty())
      menus->applyItemIds(ids, "headless");
  }

  // --anim <index> starts a clip by its index into the model's anims[] array. Needed
  // because playback is opt-in now: without it there is no way to reach an animated pose
  // headlessly. Must run AFTER the timeline has been given the model, or there is no
  // animation list to pick from yet.
  for (int i = 1; i < argc - 1; ++i)
    if (QString(argv[i]) == "--anim") {
      const int idx = QString::fromLocal8Bit(argv[i + 1]).toInt();
      const bool ok = win->timeline()->playAnimation(idx);
      trace(QString("anim %1 -> %2").arg(idx).arg(ok ? "playing" : "NOT on this model"));
    }

  // --equip <id>[,<id>...] drives the same path as the panel's id field, so the
  // equipment grid is verifiable without typing into it.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--equip")
      continue;
    for (const QString& s : QString::fromLocal8Bit(argv[i + 1]).split(',')) {
      bool ok = false;
      const int id = s.trimmed().toInt(&ok);
      if (ok && id > 0 && win->characterPanel()) {
        win->characterPanel()->equip(id);
        trace(QString("equipped item %1").arg(id));
      }
    }
  }

  // --unequip <slot>[,<slot>...] takes single pieces off by CharSlots index -- the
  // headless twin of the equipment rows' "x", same as --equip mirrors the id field.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--unequip")
      continue;
    for (const QString& p : QString::fromLocal8Bit(argv[i + 1]).split(',', QString::SkipEmptyParts)) {
      win->characterPanel()->unequipSlot(p.toInt());
      trace(QString("unequip slot %1").arg(p.toInt()));
    }
  }

  // --focus <slot> turns on the item view for one CharSlots index (-1 = off), the
  // headless twin of the equipment row's eye.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--focus")
      continue;
    // toInt() turns anything unparseable into 0, which is CS_HEAD -- a typo would silently
    // focus the helmet slot instead of reporting itself.
    bool ok = false;
    const QString raw = QString::fromLocal8Bit(argv[i + 1]);
    const int slot = raw.toInt(&ok);
    if (!ok) {
      trace(QString("--focus: '%1' ist keine Slot-Nummer -- ignoriert").arg(raw));
      continue;
    }
    win->characterPanel()->setItemFocus(slot);
    trace(QString("item focus %1").arg(slot));
  }

  // --export <Format>,<Pfad> runs an export without the dialog, so it is verifiable
  // headlessly rather than merely assumed to work.
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) != "--export")
      continue;
    const QStringList a = QString::fromLocal8Bit(argv[i + 1]).split(',');
    if (a.size() != 2)
      continue;
    int idx = -1;
    for (int k = 0; k < (int)exporters->formats().size(); ++k)
      if (exporters->formats()[k].label.compare(a[0], Qt::CaseInsensitive) == 0)
        idx = k;
    const QString err = exporters->exportTo(host->model(), idx, a[1]);
    trace(err.isEmpty() ? QString("export OK -> %1").arg(a[1])
                        : QString("export FAILED: %1").arg(err));
  }

  win->show();

  // The GL context is set up in GLHost::showEvent, i.e. after show(). If it did
  // not come up, report why to a file and quit -- otherwise the window just sits
  // there doing nothing and the reason stays invisible.
  QTimer::singleShot(2500, [host]() {
    if (host->isReady())
      return;
    const QString why = host->lastError().isEmpty()
                          ? QStringLiteral("video never initialised and no error was recorded "
                                           "-- showEvent/initVideo did not run at all")
                          : host->lastError();
    QFile f("userSettings/qt-frontend-error.txt");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
      QTextStream(&f) << why << "\n";
    trace("FATAL: OpenGL init failed -- " + why);

    // This is the likeliest failure on hardware we have never seen -- remote desktop, a VM
    // without GPU passthrough, an ancient display driver. Writing it only to a file the user
    // does not know about made the application appear to vanish two seconds after launch.
    if (!g_scripted) {
      QMessageBox box(QMessageBox::Critical, QString(WMV_APP_NAME),
                      QObject::tr("Die 3D-Ansicht konnte nicht gestartet werden."),
                      QMessageBox::Ok);
      box.setInformativeText(
          QObject::tr("Das Programm braucht OpenGL. Häufige Ursachen:\n\n"
                      "• Start über Remotedesktop — dort steht meist kein OpenGL bereit\n"
                      "• Grafiktreiber veraltet oder nur der Windows-Standardtreiber aktiv\n"
                      "• virtuelle Maschine ohne Grafikdurchreichung"));
      box.setDetailedText(why);
      box.exec();
    }
    qApp->quit();
  });

  return app.exec();
}
