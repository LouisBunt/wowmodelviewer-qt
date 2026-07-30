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
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>

#include "GLHost.h"
#include "CharacterPanel.h"
#include "ExportController.h"
#include "InspectorTabs.h"
#include "TimelinePanel.h"
#include "MainWindow.h"

#include "Game.h"
#include "GameFile.h"
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

int main(int argc, char** argv)
{
  trace("main entered");
  QApplication app(argc, argv);
  trace("QApplication constructed");

  const QString dataFolder = resolveGameFolder(argc, argv);
  if (dataFolder.isEmpty()) {
    trace("no game folder chosen -- exiting");
    return 0;
  }
  const QStringList positional = positionalArgs(argc, argv);
  // 1000001 was the Phase 1 probe id. It no longer resolves in current clients, so a
  // plain double-click landed on "FileDataID not found" and stayed there. 917116 is
  // the male orc -- the model the readme uses as its example.
  const bool modelRequested = positional.size() > 1;
  const uint fileId = modelRequested ? positional.at(1).toUInt() : 917116u;

  auto* win = new MainWindow;
  GLHost* host = win->canvas();
  // Kept so the existing failure paths below still have somewhere to report to.
  auto* status = new QLabel;

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
    status->setText("Game::init failed -- is the data folder correct?");
    win->show();
    return app.exec();
  }
  // Game::init() only wires the objects together -- initDone() merely checks two
  // pointers. Nothing is opened until a config is chosen and handed to setConfig(),
  // which is what actually mounts the CASC storage. Skipping this was why every
  // getFile() came back null.
  trace("before configsFound");
  std::vector<core::GameConfig> configs = GAMEDIRECTORY.configsFound();
  trace(QString("configsFound returned %1 entries").arg(configs.size()));
  if (configs.empty()) {
    trace("FATAL: no config found in the data folder");
    status->setText("No WoW locale/config found in that folder");
    win->show();
    QTimer::singleShot(3000, qApp, &QApplication::quit);
    return app.exec();
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

  trace("before setConfig (mounts CASC)");
  if (!GAMEDIRECTORY.setConfig(config)) {
    trace(QString("FATAL: setConfig failed, lastError=%1").arg(GAMEDIRECTORY.lastError()));
    status->setText(QString("setConfig failed (error %1)").arg(GAMEDIRECTORY.lastError()));
    win->show();
    QTimer::singleShot(3000, qApp, &QApplication::quit);
    return app.exec();
  }
  trace("setConfig returned OK");

  // configFolder must match the client's major version; the listfile path below is
  // resolved relative to it.
  const QString cfgFolder = QString("games/wow/%1.0/").arg(config.version.section('.', 0, 0));
  core::Game::instance().setConfigFolder(cfgFolder);
  trace("configFolder = " + cfgFolder);

  trace("before initFromListfile");
  GAMEDIRECTORY.initFromListfile("../../../listfile.csv");
  trace("initFromListfile returned");

  // The database, texture regions and race registry. Without these WoWModel cannot
  // resolve a model file to a race, infos.raceID stays -1, and character models draw
  // as a handful of untextured geosets -- which is exactly what Phase 1 produced.
  trace("before GAMEDATABASE.initFromXML");
  if (!GAMEDATABASE.initFromXML("database.xml"))
    trace("WARNING: database.xml init failed -- character data will be empty");
  else
    trace("database.xml loaded");

  CharTexture::initRegions();
  RaceInfos::init();
  trace("RaceInfos::init done");

  // The item table. items.getById() is what maps an id to its slot, so without this
  // every equip attempt resolves to slot -1 and silently does nothing.
  {
    sqlResult r = GAMEDATABASE.sqlQuery(
      "SELECT Item.ID, ItemSparse.Display_Lang, Item.InventoryType, Item.ClassID, "
      "Item.SubclassID, Item.SheathType FROM Item "
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

  trace("before getFile");
  GameFile* file = GAMEDIRECTORY.getFile(fileId);
  trace("getFile returned");
  // An id the user asked for and did not get is worth stopping on. The default one
  // failing is not: the browser is right there, so come up with an empty viewport and
  // let them pick, instead of dead-ending on a message.
  if (!file && modelRequested) {
    status->setText(QString("FileDataID %1 not found in CASC").arg(fileId));
    win->show();
    return app.exec();
  }
  if (!file)
    trace(QString("default model %1 not in this client -- starting empty").arg(fileId));

  // The model must be built only AFTER the GL context exists: WoWModel uploads its
  // textures during construction, and without a current context those uploads fail
  // silently -- geometry survives (CPU side) but everything renders untextured.
  // GLHost creates the context in showEvent, so show first, pump the event queue,
  // then load.
  trace("before show (context comes up here)");
  win->show();
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

  // --category <0..3> selects a browser tab headlessly (0 all, 1 characters,
  // 2 creatures, 3 items).
  for (int i = 1; i < argc - 1; ++i) {
    if (QString(argv[i]) == "--category") {
      win->setCategory(QString::fromLocal8Bit(argv[i + 1]).toInt());
      trace(QString("category set to %1").arg(argv[i + 1]));
    }
  }

  QObject::connect(win, &MainWindow::fileIdActivated, [win](int id) {
    if (GameFile* f = GAMEDIRECTORY.getFile((uint)id))
      emit win->fileActivated(f);
  });

  QObject::connect(win, &MainWindow::fileActivated, [win, host](GameFile* picked) {
    if (!picked)
      return;
    auto* m = new WoWModel(picked, true);
    host->setModel(m);
    win->setPathLabel(picked->fullname());
    // Character models need their CharDetails set up before they render complete;
    // creatures and props resolve raceID == -1 and are left alone.
    win->characterPanel()->setModel(m->infos.raceID != -1 ? m : nullptr);
    win->timeline()->setModel(m);
    win->materialTab()->setModel(m);
  });

  if (win->characterPanel())
    win->characterPanel()->setModel(model && model->infos.raceID != -1 ? model : nullptr);
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
  for (const auto& f : exporters->formats())
    trace("  exporter: " + f.label);

  QObject::connect(win, &MainWindow::exportRequested, [win, host, exporters]() {
    if (exporters->formats().empty()) {
      QMessageBox::warning(win, "Export",
                           QString::fromUtf8("Keine Exporter gefunden. Liegt der Ordner "
                                             "\"plugins\" neben der Anwendung?"));
      return;
    }

    QStringList labels;
    for (const auto& f : exporters->formats())
      labels << f.label;

    bool ok = false;
    const QString chosen = QInputDialog::getItem(win, "Export", QString::fromUtf8("Format:"),
                                                 labels, 0, false, &ok);
    if (!ok)
      return;

    const QString err = exporters->exportModel(host->model(), labels.indexOf(chosen), win);
    if (!err.isEmpty())
      QMessageBox::warning(win, "Export", err);
  });

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

  // Keep the scrubber and frame counter in step with playback. The canvas advances
  // the animation itself; this only reads it back.
  auto* uiTick = new QTimer(win);
  QObject::connect(uiTick, &QTimer::timeout, [win]() { win->timeline()->tick(); });
  uiTick->start(60);

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

  win->show();

  // The GL context is set up in GLHost::showEvent, i.e. after show(). If it did
  // not come up, report why to a file and quit -- otherwise the window just sits
  // there doing nothing and the reason stays invisible.
  QTimer::singleShot(2500, [host]() {
    if (host->isReady())
      return;
    QFile f("userSettings/qt-frontend-error.txt");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&f);
      out << (host->lastError().isEmpty()
                ? QStringLiteral("video never initialised and no error was recorded "
                                 "-- showEvent/initVideo did not run at all")
                : host->lastError()) << "\n";
    }
    qApp->quit();
  });

  return app.exec();
}
